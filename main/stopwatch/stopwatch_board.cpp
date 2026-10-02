// M5Stack StopWatch hardware bridge. Power and panel sequence were validated
// on the user's device with ESP-IDF 5.5.4. Based on M5StopWatch-UserDemo (MIT).
#include "stopwatch_board.h"
#include "cst820.h"
#include "charge_led_policy.h"

#include <M5GFX.h>
#include <M5IOE1.h>
#include <M5PM1.h>
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>
#include <esp_rom_sys.h>
#include <bmi270.h>
#include <math.h>
extern "C" {
#include "bsp_obd_dsp/nvs_storage.h"
}

namespace {
constexpr char TAG[] = "stopwatch";
bool rear_power_ready = false;

class PanelCO5300 final : public lgfx::Panel_AMOLED {
public:
    PanelCO5300() {
        _cfg.memory_width = _cfg.panel_width = 480;
        _cfg.memory_height = _cfg.panel_height = 480;
        _write_depth = lgfx::color_depth_t::rgb565_2Byte;
        _read_depth = lgfx::color_depth_t::rgb565_2Byte;
    }

    const uint8_t* getInitCommands(uint8_t listno) const override {
        static constexpr uint8_t commands[] = {
            0x11, 0 + CMD_INIT_DELAY, 150,
            0xC4, 1, 0x80, 0x35, 1, 0x80,
            0x44, 2, 0x01, 0xD2, 0x53, 1, 0x20,
            0x20, 0, 0x36, 1, 0, 0x51, 1, 0xA0,
            0x29, 0, 0xff, 0xff,
        };
        return listno == 0 ? commands : nullptr;
    }
};

class StopWatchDisplay final : public M5GFX {
    lgfx::Bus_SPI bus_;
    PanelCO5300 panel_;
public:
    void setPanelBrightness(uint8_t brightness) { panel_.setBrightness(brightness); }
    void waitForPanelTransfer() { panel_.waitDisplay(); }
    bool init_impl(bool use_reset, bool use_clear) override {
        auto bus_cfg = bus_.config();
        bus_cfg.freq_write = 80000000;
        bus_cfg.freq_read = 10000000;
        bus_cfg.pin_sclk = GPIO_NUM_40;
        bus_cfg.pin_io0 = GPIO_NUM_41;
        bus_cfg.pin_io1 = GPIO_NUM_42;
        bus_cfg.pin_io2 = GPIO_NUM_46;
        bus_cfg.pin_io3 = GPIO_NUM_45;
        bus_cfg.spi_host = SPI2_HOST;
        bus_cfg.spi_mode = 0;
        bus_cfg.spi_3wire = true;
        bus_cfg.dma_channel = SPI_DMA_CH_AUTO;
        bus_.config(bus_cfg);
        panel_.setBus(&bus_);

        auto panel_cfg = panel_.config();
        panel_cfg.pin_rst = GPIO_NUM_NC;
        panel_cfg.pin_cs = GPIO_NUM_39;
        panel_cfg.panel_width = 468;
        panel_cfg.panel_height = 466;
        panel_cfg.offset_x = 6;
        panel_cfg.offset_y = 0;
        panel_cfg.readable = false;
        panel_.config(panel_cfg);
        setPanel(&panel_);
        lgfx::pinMode(GPIO_NUM_38, lgfx::pin_mode_t::input_pullup);
        if (!LGFX_Device::init_impl(use_reset, use_clear)) return false;
        if (!panel_.initPanelFb()) return false;
        auto *fb_panel = panel_.getPanelFb();
        if (!fb_panel) return false;
        fb_panel->setBus(&bus_);
        fb_panel->setAutoDisplay(false);
        setPanel(fb_panel);
        panel_.setBrightness(170);
        return true;
    }
};

StopWatchDisplay display;
M5PM1 pmic;
bool charge_input_ready = false;
bool charge_led_state_known = false;
bool charge_led_on = false;
charge_led_policy_t charge_led_policy = {};

bool configure_charge_input() {
    // LGS4056 CHRG is active-low on PMIC GPIO2, pulled up by the board.
    // GPIO3 selects charge current; do not change it for LED control.
    if (pmic.gpioSetFunc(M5PM1_GPIO_NUM_2, M5PM1_GPIO_FUNC_GPIO) != M5PM1_OK ||
        pmic.gpioSetMode(M5PM1_GPIO_NUM_2, M5PM1_GPIO_MODE_INPUT) != M5PM1_OK ||
        pmic.gpioSetPull(M5PM1_GPIO_NUM_2, M5PM1_GPIO_PULL_NONE) != M5PM1_OK)
        return false;
    m5pm1_pin_status_t pin = {};
    return pmic.getPinStatus(M5PM1_GPIO_NUM_2, &pin) == M5PM1_OK &&
           pin.func == M5PM1_GPIO_FUNC_GPIO && pin.mode == M5PM1_GPIO_MODE_INPUT &&
           pin.pull == M5PM1_GPIO_PULL_NONE;
}

bool set_charge_led(bool enabled) {
    uint8_t power_cfg = 0;
    if (pmic.getPowerConfig(&power_cfg) != M5PM1_OK) return false;
    if (((power_cfg & M5PM1_PWR_CFG_LED_CTRL) != 0) != enabled) {
        // The official masked API preserves charging and all power-rail bits.
        if (pmic.setLedEnLevel(enabled) != M5PM1_OK ||
            pmic.getPowerConfig(&power_cfg) != M5PM1_OK ||
            (((power_cfg & M5PM1_PWR_CFG_LED_CTRL) != 0) != enabled))
            return false;
    }
    if (!charge_led_state_known || charge_led_on != enabled) {
        charge_led_state_known = true;
        charge_led_on = enabled;
        ESP_LOGI(TAG, "[POWER] charge LED %s", enabled ? "ON (charging)" : "OFF");
    }
    return true;
}

bool configure_rear_power_wake() {
    // StopWatch v1.0: rear pin 14 is Int_5V, not VBAT. Q2 makes
    // PMG4_PORT_INT active-low when that input is powered. Its U27 diode
    // joins Internal_5V after the USB VIN ADC, so readVin() cannot see it.
    // GPIO3 is CHG_PROG; only disable its conflicting wake function.
    // Use standard GPIO plus WAKE_EN: FUNC=0b10 is reserved in the datasheet.
    if (pmic.gpioSetWakeEnable(M5PM1_GPIO_NUM_3, false) != M5PM1_OK ||
        pmic.gpioSetFunc(M5PM1_GPIO_NUM_4, M5PM1_GPIO_FUNC_GPIO) != M5PM1_OK ||
        pmic.gpioSetMode(M5PM1_GPIO_NUM_4, M5PM1_GPIO_MODE_INPUT) != M5PM1_OK ||
        pmic.gpioSetPull(M5PM1_GPIO_NUM_4, M5PM1_GPIO_PULL_NONE) != M5PM1_OK ||
        pmic.gpioSetWakeEdge(M5PM1_GPIO_NUM_4, M5PM1_GPIO_WAKE_FALLING) != M5PM1_OK ||
        pmic.gpioSetWakeEnable(M5PM1_GPIO_NUM_4, true) != M5PM1_OK ||
        pmic.verifyPinConfig() != M5PM1_OK) return false;
    m5pm1_pin_status_t pin = {};
    return pmic.getPinStatus(M5PM1_GPIO_NUM_4, &pin) == M5PM1_OK &&
           pin.func == M5PM1_GPIO_FUNC_GPIO && pin.mode == M5PM1_GPIO_MODE_INPUT &&
           pin.pull == M5PM1_GPIO_PULL_NONE && pin.wake_en &&
           pin.wake_edge == M5PM1_GPIO_WAKE_FALLING;
}

M5IOE1 ioe;
Cst820 touch;
i2c_master_bus_handle_t i2c_bus = nullptr;
i2c_master_dev_handle_t imu_device = nullptr;
struct bmi2_dev imu = {};
bool imu_ready = false;
TaskHandle_t feedback_task_handle = nullptr;
esp_codec_dev_handle_t sound_codec = nullptr;
i2s_chan_handle_t sound_tx = nullptr;
const audio_codec_data_if_t *sound_data_if = nullptr;
const audio_codec_ctrl_if_t *sound_ctrl_if = nullptr;
const audio_codec_gpio_if_t *sound_gpio_if = nullptr;
const audio_codec_if_t *sound_codec_if = nullptr;
uint64_t sound_retry_after_us = 0;
bool sound_ready = false;
constexpr uint32_t SOUND_SAMPLE_RATE = 44100;
constexpr uint32_t SOUND_CHANNELS = 2;
constexpr uint32_t SOUND_TONE_SAMPLES = SOUND_SAMPLE_RATE * 80 / 1000;
constexpr uint32_t SOUND_DMA_COUNT = 4;
constexpr uint32_t SOUND_DMA_FRAMES = 256;
constexpr uint32_t SOUND_DRAIN_SAMPLES = (SOUND_DMA_COUNT + 1) * SOUND_DMA_FRAMES;
constexpr int SOUND_VOLUME = 80;
volatile uint32_t sound_sent_buffers = 0;
volatile uint32_t sound_sent_nonzero_buffers = 0;
portMUX_TYPE feedback_lock = portMUX_INITIALIZER_UNLOCKED;
uint8_t pending_feedback = 0;
bool explicit_feedback_for_press = false;
uint64_t last_touch_feedback_us = 0;
uint64_t last_option_feedback_us = 0;
uint64_t last_page_feedback_us = 0;
bool touch_ready = false;
bool touch_filtered = false;
bool touch_candidate = false;
uint32_t touch_candidate_since = 0;
uint16_t touch_x = 0;
uint16_t touch_y = 0;

BMI2_INTF_RETURN_TYPE bmi_i2c_read(uint8_t reg, uint8_t *data, uint32_t len, void *ptr) {
    auto dev = *static_cast<i2c_master_dev_handle_t *>(ptr);
    return i2c_master_transmit_receive(dev, &reg, 1, data, len, 100) == ESP_OK ? 0 : -1;
}

BMI2_INTF_RETURN_TYPE bmi_i2c_write(uint8_t reg, const uint8_t *data, uint32_t len, void *ptr) {
    if (len > 64) return -1;
    auto dev = *static_cast<i2c_master_dev_handle_t *>(ptr);
    uint8_t tx[65];
    tx[0] = reg;
    for (uint32_t i = 0; i < len; ++i) tx[i + 1] = data[i];
    return i2c_master_transmit(dev, tx, len + 1, 100) == ESP_OK ? 0 : -1;
}

void bmi_delay_us(uint32_t us, void *) {
    if (us >= 2000) vTaskDelay(pdMS_TO_TICKS((us + 999) / 1000));
    else esp_rom_delay_us(us);
}

bool set_speaker_amplifier(bool enabled) {
    m5ioe1_err_t err = M5IOE1_OK;
    // AW8737A is gated by both the IO expander and ESP GPIO14.
    const esp_err_t gpio_err = gpio_set_level(GPIO_NUM_14, enabled ? 1 : 0);
    ioe.digitalWriteWithRes(M5IOE1_PIN_10, enabled ? 1 : 0, &err);
    if (err == M5IOE1_OK) {
        const int level = ioe.digitalReadWithRes(M5IOE1_PIN_10, &err);
        if (err == M5IOE1_OK && gpio_err == ESP_OK &&
            level == (enabled ? 1 : 0) && gpio_get_level(GPIO_NUM_14) == (enabled ? 1 : 0)) return true;
    }
    gpio_set_level(GPIO_NUM_14, 0);
    ioe.digitalWrite(M5IOE1_PIN_10, 0);
    ESP_LOGE(TAG, "[AUDIO] amplifier %s failed: IOE=%d GPIO=%d", enabled ? "ON" : "OFF", (int)err, (int)gpio_err);
    return false;
}

bool audio_sent(i2s_chan_handle_t, i2s_event_data_t *event, void *) {
    // Observe completed DMA data before auto_clear_after_cb zeros it. No
    // logging or allocation in the interrupt; the feedback task reports it.
    ++sound_sent_buffers;
    const int16_t *samples = static_cast<const int16_t *>(event->dma_buf);
    for (size_t i=0; samples && i<event->size/sizeof(int16_t); ++i) {
        if (samples[i]) { ++sound_sent_nonzero_buffers; break; }
    }
    return false;
}

void release_sound(void) {
    sound_ready = false;
    if (sound_codec) { esp_codec_dev_delete(sound_codec); sound_codec = nullptr; }
    if (sound_codec_if) { audio_codec_delete_codec_if(sound_codec_if); sound_codec_if = nullptr; }
    if (sound_ctrl_if) { audio_codec_delete_ctrl_if(sound_ctrl_if); sound_ctrl_if = nullptr; }
    if (sound_gpio_if) { audio_codec_delete_gpio_if(sound_gpio_if); sound_gpio_if = nullptr; }
    if (sound_data_if) { audio_codec_delete_data_if(sound_data_if); sound_data_if = nullptr; }
    if (sound_tx) {
        i2s_channel_disable(sound_tx);
        i2s_del_channel(sound_tx);
        sound_tx = nullptr;
    }
}

bool sound_failure(const char *stage, int status) {
    ESP_LOGE(TAG, "[AUDIO] %s failed: %d; retry allowed after 5s", stage, status);
    set_speaker_amplifier(false);
    release_sound();
    sound_retry_after_us = esp_timer_get_time() + 5000000ULL;
    return false;
}

bool init_sound(void) {
    if (sound_ready) return true;
    if ((uint64_t)esp_timer_get_time() < sound_retry_after_us) return false;
    m5ioe1_err_t power_err=M5IOE1_OK;
    const int l3b=ioe.digitalReadWithRes(M5IOE1_PIN_8,&power_err);
    if (power_err != M5IOE1_OK || l3b != 1) return sound_failure("L3B power readback",power_err);
    const int audio_power=ioe.digitalReadWithRes(M5IOE1_PIN_3,&power_err);
    if (power_err != M5IOE1_OK || audio_power != 1) return sound_failure("audio power readback",power_err);
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = SOUND_DMA_COUNT;
    chan_cfg.dma_frame_num = SOUND_DMA_FRAMES;
    // Silence after underrun, instead of replaying an old cue from the DMA ring.
    chan_cfg.auto_clear_after_cb = true;
    int rc = i2s_new_channel(&chan_cfg,&sound_tx,nullptr);
    if (rc != ESP_OK) return sound_failure("I2S allocation",rc);
    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SOUND_SAMPLE_RATE);
    std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = GPIO_NUM_18;
    std_cfg.gpio_cfg.bclk = GPIO_NUM_17;
    std_cfg.gpio_cfg.ws = GPIO_NUM_15;
    std_cfg.gpio_cfg.dout = GPIO_NUM_21;
    std_cfg.gpio_cfg.din = GPIO_NUM_NC;
    rc = i2s_channel_init_std_mode(sound_tx,&std_cfg);
    if (rc != ESP_OK) return sound_failure("I2S setup",rc);
    i2s_event_callbacks_t callbacks = {};
    callbacks.on_sent=audio_sent;
    rc = i2s_channel_register_event_callback(sound_tx,&callbacks,nullptr);
    if (rc != ESP_OK) return sound_failure("I2S TX observation",rc);
    rc = i2s_channel_enable(sound_tx);
    if (rc != ESP_OK) return sound_failure("I2S start",rc);
    // Keep standard left/right slots and send the same PCM to both. This
    // avoids depending on the codec's mono slot selection for sound output.
    audio_codec_i2s_cfg_t data_cfg = {};
    data_cfg.tx_handle = sound_tx;
    sound_data_if = audio_codec_new_i2s_data(&data_cfg);
    if (!sound_data_if) return sound_failure("I2S data interface",ESP_ERR_NO_MEM);
    audio_codec_i2c_cfg_t i2c_cfg = {};
    i2c_cfg.addr = ES8311_CODEC_DEFAULT_ADDR;
    i2c_cfg.bus_handle = i2c_bus;
    sound_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (!sound_ctrl_if) return sound_failure("ES8311 I2C interface",ESP_ERR_NO_MEM);
    sound_gpio_if = audio_codec_new_gpio();
    if (!sound_gpio_if) return sound_failure("codec GPIO interface",ESP_ERR_NO_MEM);
    es8311_codec_cfg_t codec_cfg = {};
    codec_cfg.ctrl_if=sound_ctrl_if;
    codec_cfg.gpio_if=sound_gpio_if;
    codec_cfg.codec_mode=ESP_CODEC_DEV_WORK_MODE_DAC;
    codec_cfg.pa_pin=GPIO_NUM_NC;
    codec_cfg.use_mclk=true;
    sound_codec_if=es8311_codec_new(&codec_cfg);
    if (!sound_codec_if) return sound_failure("ES8311 initialization",ESP_CODEC_DEV_DRV_ERR);
    esp_codec_dev_cfg_t dev_cfg = {};
    dev_cfg.dev_type=ESP_CODEC_DEV_TYPE_OUT;
    dev_cfg.codec_if=sound_codec_if;
    dev_cfg.data_if=sound_data_if;
    sound_codec=esp_codec_dev_new(&dev_cfg);
    if (!sound_codec) return sound_failure("codec device",ESP_ERR_NO_MEM);
    esp_codec_dev_sample_info_t format = {};
    format.bits_per_sample=16;
    format.channel=SOUND_CHANNELS;
    format.sample_rate=SOUND_SAMPLE_RATE;
    rc = esp_codec_dev_open(sound_codec,&format);
    if (rc != ESP_CODEC_DEV_OK) return sound_failure("codec open",rc);
    // The default codec curve maps 18 to -41 dB, making a short cue inaudible.
    rc = esp_codec_dev_set_out_vol(sound_codec,SOUND_VOLUME);
    if (rc != ESP_CODEC_DEV_OK) return sound_failure("codec volume",rc);
    rc = esp_codec_dev_set_out_mute(sound_codec,false);
    if (rc != ESP_CODEC_DEV_OK) return sound_failure("codec unmute",rc);
    int mute_reg=0,volume_reg=0;
    rc = esp_codec_dev_read_reg(sound_codec,0x31,&mute_reg);
    if (rc != ESP_CODEC_DEV_OK || (mute_reg & 0x60))
        return sound_failure("DAC unmute readback",rc == ESP_CODEC_DEV_OK ? ESP_FAIL : rc);
    rc = esp_codec_dev_read_reg(sound_codec,0x32,&volume_reg);
    if (rc != ESP_CODEC_DEV_OK) return sound_failure("DAC volume readback",rc);
    const uint8_t regs[]={0x00,0x01,0x02,0x06,0x09,0x0D,0x0E,0x12,0x14,0x37,0x44};
    for (uint8_t reg:regs) {
        int value=0;
        rc=esp_codec_dev_read_reg(sound_codec,reg,&value);
        if (rc != ESP_CODEC_DEV_OK) return sound_failure("codec register readback",rc);
        ESP_LOGI(TAG,"[AUDIO] ES8311 reg%02X=%02X",reg,value);
    }
    sound_ready=true;
    ESP_LOGI(TAG, "[AUDIO] ES8311 ready: %luHz stereo vol=%d DMA=%lux%lu; cue=80ms DAC31=%02X DAC32=%02X power=%d/%d",
             (unsigned long)SOUND_SAMPLE_RATE,SOUND_VOLUME,
             (unsigned long)SOUND_DMA_COUNT,(unsigned long)SOUND_DMA_FRAMES,
             mute_reg,volume_reg,l3b,audio_power);
    return true;
}

void play_feedback_sound(uint8_t kind, int16_t *pcm) {
    if (!init_sound() || !set_speaker_amplifier(true)) return;
    const int64_t amplifier_start = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(50));
    const uint32_t sent_before=sound_sent_buffers;
    const uint32_t nonzero_before=sound_sent_nonzero_buffers;
    const int64_t write_start = esp_timer_get_time();
    int rc = esp_codec_dev_write(sound_codec,pcm,SOUND_TONE_SAMPLES*SOUND_CHANNELS*sizeof(int16_t));
    const int64_t write_end = esp_timer_get_time();
    // write() only copies into DMA. Keep the PA on while more than one full
    // DMA ring of silence passes through; every tone frame has then left TX.
    static int16_t silence[SOUND_DRAIN_SAMPLES*SOUND_CHANNELS] = {};
    if (rc == ESP_CODEC_DEV_OK)
        rc = esp_codec_dev_write(sound_codec,silence,sizeof(silence));
    vTaskDelay(pdMS_TO_TICKS(5)); // codec/filter settling before power gating
    const bool amplifier_off = set_speaker_amplifier(false);
    if (rc != ESP_CODEC_DEV_OK) {
        sound_failure("cue write/drain",rc);
        return;
    }
    ESP_LOGI(TAG, "[AUDIO] cue=%u OK: write=%lldus PA=%lldms drain=%lu samples off=%s TX=%lu nonzero=%lu",
             kind,(long long)(write_end-write_start),
             (long long)((esp_timer_get_time()-amplifier_start)/1000),
             (unsigned long)SOUND_DRAIN_SAMPLES,amplifier_off ? "verified" : "FAILED",
             (unsigned long)(sound_sent_buffers-sent_before),
             (unsigned long)(sound_sent_nonzero_buffers-nonzero_before));
}

void feedback_task(void *) {
    static int16_t pcm[SOUND_TONE_SAMPLES*SOUND_CHANNELS];
    constexpr uint32_t fade_samples = SOUND_SAMPLE_RATE * 3 / 1000;
    for (uint32_t i=0;i<SOUND_TONE_SAMPLES;++i) {
        float envelope=1.f;
        if (i<fade_samples) envelope=(float)i/fade_samples;
        else if (i>=SOUND_TONE_SAMPLES-fade_samples)
            envelope=(float)(SOUND_TONE_SAMPLES-1-i)/fade_samples;
        const int16_t sample=(int16_t)(5500.f*envelope*sinf(6.2831853f*660.f*i/SOUND_SAMPLE_RATE));
        pcm[2*i]=pcm[2*i+1]=sample;
    }
    for (;;) {
        ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
        // Touch release reaches the driver just before LVGL emits CLICKED,
        // VALUE_CHANGED or GESTURE. Coalesce those events into one cue.
        vTaskDelay(pdMS_TO_TICKS(35));
        portENTER_CRITICAL(&feedback_lock);
        uint8_t kind=pending_feedback;
        pending_feedback=0;
        portEXIT_CRITICAL(&feedback_lock);
        if (!kind) continue;
        const nvs_user_cfg_t *cfg=nvs_cfg_get();
        bool haptic=cfg->touch_haptic_enabled;
        bool sound=cfg->touch_sound_enabled;
        const uint8_t duty=kind==STOPWATCH_FEEDBACK_PAGE ? 80 : 55;
        const uint32_t pulse_ms=kind==STOPWATCH_FEEDBACK_PAGE ? 35 : 20;
        if (haptic) {
            ioe.setPwmDuty(0,duty,false,true);
            vTaskDelay(pdMS_TO_TICKS(pulse_ms));
            ioe.setPwmDuty(0,0,false,true);
        }
        // Finish the vibration independently; codec setup/draining must not
        // extend a 20/35ms motor pulse. Both run outside the LVGL task.
        if (sound) play_feedback_sound(kind,pcm);
    }
}
}

extern "C" bool stopwatch_board_init(void) {
    i2c_master_bus_config_t i2c_cfg = {};
    i2c_cfg.i2c_port = I2C_NUM_0;
    i2c_cfg.sda_io_num = GPIO_NUM_47;
    i2c_cfg.scl_io_num = GPIO_NUM_48;
    i2c_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_cfg.glitch_ignore_cnt = 7;
    i2c_cfg.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&i2c_cfg, &i2c_bus) != ESP_OK) return false;

    if (pmic.begin(i2c_bus) != M5PM1_OK) return false;
    pmic.setI2cSleepTime(0);
    pmic.wdtSet(0);
    // Native reset/download indications precede app control. Default LED_EN
    // is high; turn it off until actual charging has been confirmed.
    if (!set_charge_led(false))
        ESP_LOGW(TAG, "[POWER] initial charge LED off could not be verified");
    uint8_t wake_source = 0;
    if (pmic.getWakeSource(&wake_source, M5PM1_CLEAN_ALL) == M5PM1_OK)
        ESP_LOGI(TAG, "[POWER] cold boot wake source=0x%02X (USB=02 button=04 rear5V=20)", wake_source);
    pmic.ldoSetPowerHold(true);
    rear_power_ready = configure_rear_power_wake();
    if (rear_power_ready)
        ESP_LOGI(TAG, "[POWER] v1.0 rear 5V detection / falling-edge wake verified");
    else
        ESP_LOGE(TAG, "[POWER] rear 5V setup failed; automatic shutdown blocked");
    stopwatch_board_charge_led_update();

    auto ioe_result = ioe.begin(i2c_bus, 0x4F, M5IOE1_I2C_FREQ_400K);
    if (ioe_result != M5IOE1_OK)
        ioe_result = ioe.begin(i2c_bus, 0x6F, M5IOE1_I2C_FREQ_400K);
    if (ioe_result != M5IOE1_OK) return false;
    ioe.setI2cSleepTime(0);
    ioe.pinMode(M5IOE1_PIN_8, OUTPUT);
    ioe.pinMode(M5IOE1_PIN_5, OUTPUT);
    ioe.pinMode(M5IOE1_PIN_4, OUTPUT);
    ioe.pinMode(M5IOE1_PIN_9, OUTPUT);
    ioe.pinMode(M5IOE1_PIN_10, OUTPUT);
    ioe.pinMode(M5IOE1_PIN_3, OUTPUT);
    ioe.digitalWrite(M5IOE1_PIN_8, 1);
    ioe.digitalWrite(M5IOE1_PIN_4, 1);
    ioe.digitalWrite(M5IOE1_PIN_5, 1);
    ioe.digitalWrite(M5IOE1_PIN_10, 0); // speaker amplifier stays off until enabled by user
    ioe.digitalWrite(M5IOE1_PIN_3, 1);
    ioe.setPwmFrequency(5000);
    ioe.setPwmDuty(0,0,false,true);
    gpio_reset_pin(GPIO_NUM_14);
    gpio_set_direction(GPIO_NUM_14,GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(GPIO_NUM_14,0);
    gpio_config_t button_cfg = {};
    button_cfg.pin_bit_mask = (1ULL << GPIO_NUM_2) | (1ULL << GPIO_NUM_1);
    button_cfg.mode = GPIO_MODE_INPUT;
    button_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    button_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    if (gpio_config(&button_cfg) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(50));

    if (!display.init()) return false;
    ESP_LOGI(TAG, "CO5300 ready: %d x %d", display.width(), display.height());
    display.fillScreen(TFT_BLACK);
    display.display();

    // The official StopWatch HAL pulses touch reset before probing CST820B.
    ioe.digitalWrite(M5IOE1_PIN_4, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    ioe.digitalWrite(M5IOE1_PIN_4, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    touch_ready = touch.begin(i2c_bus, 0x15);
    ESP_LOGI(TAG, "CST820 touch %s", touch_ready ? "ready" : "unavailable");
    xTaskCreate(feedback_task,"touch-feedback",8192,nullptr,4,&feedback_task_handle);
    return true;
}

extern "C" void stopwatch_board_flush(int x, int y, int width, int height,
                                         const uint16_t *pixels, bool last) {
    // The CO5300 reports 468 pixels horizontally. LVGL uses the 466-pixel
    // centered circle, leaving one panel pixel on each side.
    display.startWrite();
    display.setAddrWindow(x + 1, y, width, height);
    uint32_t count = static_cast<uint32_t>(width) * height;
    uint32_t offset = 0;
#if CONFIG_LV_COLOR_16_SWAP
    // The Waveshare panel expects LVGL's byte-swapped RGB565 stream. M5GFX's
    // frame buffer expects native RGB565 words, so convert each strip here.
    static uint16_t native_pixels[1024];
#endif
    while (count) {
        uint32_t chunk = count > 1024 ? 1024 : count;
#if CONFIG_LV_COLOR_16_SWAP
        for (uint32_t i = 0; i < chunk; ++i)
            native_pixels[i] = __builtin_bswap16(pixels[offset + i]);
        display.writePixels(reinterpret_cast<const lgfx::rgb565_t *>(native_pixels), chunk);
#else
        display.writePixels(reinterpret_cast<const lgfx::rgb565_t *>(pixels + offset), chunk);
#endif
        offset += chunk;
        count -= chunk;
    }
    display.endWrite();
    // Panel_AMOLED_Framebuffer uses two DMA row buffers sized to the transfer
    // width. Partial-width updates repeatedly shrink and grow those buffers;
    // after BLE starts, a grow can fail and switch individual rows to the
    // much slower register path. Keep the row width fixed at the 468-pixel
    // panel width while still transferring only the dirty vertical span.
    static int dirty_top = 466;
    static int dirty_bottom = -1;
    if (y < dirty_top) dirty_top = y;
    if (y + height - 1 > dirty_bottom) dirty_bottom = y + height - 1;
    if (last) {
        if (dirty_bottom >= dirty_top) {
            display.display(0, dirty_top, 468, dirty_bottom - dirty_top + 1);
            // display.waitDisplay() targets the framebuffer panel (a no-op).
            // Wait on the underlying AMOLED bus before LVGL reuses its strips.
            display.waitForPanelTransfer();
        }
        dirty_top = 466;
        dirty_bottom = -1;
    }
}

extern "C" bool stopwatch_board_touch(uint16_t *x, uint16_t *y) {
    if (!touch_ready) return false;
    bool raw = touch.read() && touch.isPressed();
    if (raw) {
        uint16_t raw_x = touch.getX();
        touch_x = raw_x > 0 ? raw_x - 1 : 0;
        touch_y = touch.getY();
        if (touch_x > 465) touch_x = 465;
        if (touch_y > 465) touch_y = 465;
    }
    uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    if (raw != touch_candidate) {
        touch_candidate = raw;
        touch_candidate_since = now;
    }
    if (touch_filtered != touch_candidate && now - touch_candidate_since >= 60)
        touch_filtered = touch_candidate;
    static bool was_pressed = false;
    if (!was_pressed && touch_filtered) {
        portENTER_CRITICAL(&feedback_lock);
        explicit_feedback_for_press=false;
        portEXIT_CRITICAL(&feedback_lock);
    }
    if (was_pressed && !touch_filtered)
        stopwatch_board_feedback(STOPWATCH_FEEDBACK_TOUCH);
    was_pressed=touch_filtered;
    if (!touch_filtered) return false;
    *x = touch_x;
    *y = touch_y;
    return true;
}

extern "C" void stopwatch_board_buttons_read(bool *left_pressed, bool *right_pressed) {
    if (left_pressed) *left_pressed = gpio_get_level(GPIO_NUM_2) == 0;
    if (right_pressed) *right_pressed = gpio_get_level(GPIO_NUM_1) == 0;
}

extern "C" void stopwatch_board_feedback(stopwatch_feedback_t kind) {
    if (!feedback_task_handle || kind<STOPWATCH_FEEDBACK_TOUCH ||
        kind>STOPWATCH_FEEDBACK_PAGE) return;
    uint64_t now_us=esp_timer_get_time();
    bool queue=false;
    portENTER_CRITICAL(&feedback_lock);
    if (kind!=STOPWATCH_FEEDBACK_TOUCH) explicit_feedback_for_press=true;
    uint64_t *last=kind==STOPWATCH_FEEDBACK_PAGE ? &last_page_feedback_us :
                   kind==STOPWATCH_FEEDBACK_OPTION ? &last_option_feedback_us :
                   &last_touch_feedback_us;
    if ((kind!=STOPWATCH_FEEDBACK_TOUCH || !explicit_feedback_for_press) &&
        now_us-*last>=120000) {
        *last=now_us;
        if (pending_feedback<kind) pending_feedback=kind;
        queue=true;
    }
    portEXIT_CRITICAL(&feedback_lock);
    if (queue) xTaskNotifyGive(feedback_task_handle);
}

extern "C" void stopwatch_board_charge_led_update(void) {
    // Called by a global LVGL timer, including boot and all gauge/settings pages.
    static int64_t next_config_retry_us = 0;
    static bool was_failed = false;
    const int64_t now_us = esp_timer_get_time();
    if (!charge_input_ready && now_us >= next_config_retry_us) {
        charge_input_ready = configure_charge_input();
        next_config_retry_us = now_us + 5000000;
    }
    uint16_t usb_mv = 0;
    uint8_t rear_level = 1, charge_level = 1;
    const bool status_known = charge_input_ready && rear_power_ready &&
        pmic.readVin(&usb_mv) == M5PM1_OK &&
        pmic.gpioGetInput(M5PM1_GPIO_NUM_4, &rear_level) == M5PM1_OK &&
        pmic.gpioGetInput(M5PM1_GPIO_NUM_2, &charge_level) == M5PM1_OK;
    // External supply alone is not charging: CHRG releases high at full charge.
    // Rear 5V bypasses the USB VIN ADC, so both input paths must be considered.
    const bool external_power = usb_mv > 4000 || rear_level == 0;
    const bool raw_charging = status_known && external_power && charge_level == 0;
    const bool confirmed_charging = charge_led_policy_update(
        &charge_led_policy, status_known, external_power, raw_charging,
        static_cast<uint64_t>(now_us) / 1000U);
    // Short CHRG assertions from touch/vibration/load transients do not relight
    // the lamp. Every new assertion must pass the full confirmation window.
    static bool sample_logged = false;
    static bool last_known = false, last_external = false, last_raw = false;
    if (!sample_logged || status_known != last_known || external_power != last_external ||
        raw_charging != last_raw) {
        uint16_t battery_mv = 0;
        const bool battery_known = pmic.readVbat(&battery_mv) == M5PM1_OK;
        ESP_LOGI(TAG, "[POWER] charge sample: known=%u external=%u CHRG=%u VBAT=%umV%s confirm=%ums",
                 status_known, external_power, raw_charging, battery_mv,
                 battery_known ? "" : " (unknown)", CHARGE_LED_CONFIRM_MS);
        sample_logged = true;
        last_known = status_known;
        last_external = external_power;
        last_raw = raw_charging;
    }
    const bool applied = set_charge_led(confirmed_charging); // unknown status -> off
    const bool failed = !status_known || !applied;
    if (failed && !was_failed)
        ESP_LOGW(TAG, "[POWER] charge LED status/control unverified; requesting OFF");
    else if (!failed && was_failed)
        ESP_LOGI(TAG, "[POWER] charge LED monitoring recovered");
    was_failed = failed;
}

extern "C" bool stopwatch_board_power_status(uint8_t *percent, bool *external_power) {
    if (!percent || !external_power || !rear_power_ready) return false;
    uint16_t battery_mv = 0;
    uint16_t input_mv = 0;
    uint8_t rear_level = 1;
    if (pmic.readVbat(&battery_mv) != M5PM1_OK ||
        pmic.readVin(&input_mv) != M5PM1_OK ||
        pmic.gpioGetInput(M5PM1_GPIO_NUM_4, &rear_level) != M5PM1_OK) return false;
    if (battery_mv <= 3300) *percent = 0;
    else if (battery_mv >= 4200) *percent = 100;
    else *percent = static_cast<uint8_t>((battery_mv - 3300U) * 100U / 900U);
    const uint8_t sources = (input_mv > 4000 ? 1 : 0) | (rear_level == 0 ? 2 : 0);
    *external_power = sources != 0;
    static uint8_t last_sources = 0xff;
    if (sources != last_sources) {
        ESP_LOGI(TAG, "[POWER] USB_VIN=%umV REAR_5V=%s external=%s", input_mv,
                 rear_level == 0 ? "ON" : "OFF", *external_power ? "ON" : "OFF");
        last_sources = sources;
    }
    return true;
}

extern "C" bool stopwatch_board_shutdown(void) {
    // Verify the retained wake configuration and recheck both supplies before
    // cutting power. A failed read or power returning cancels this shutdown.
    rear_power_ready = configure_rear_power_wake();
    uint8_t percent = 0;
    bool external_power = true;
    if (!stopwatch_board_power_status(&percent, &external_power) || external_power) {
        ESP_LOGW(TAG, "[POWER] shutdown cancelled: supply present or wake/readback unverified");
        return false;
    }
    // Called from the LVGL task: no new panel transfers can race this sequence.
    display.waitForPanelTransfer();
    if (feedback_task_handle) vTaskSuspend(feedback_task_handle);
    ESP_LOGI(TAG, "[POWER] external 5V absent for 3s after CX sleep; entering PMIC L0");
    if (pmic.ldoSetPowerHold(false) != M5PM1_OK) goto failed;
    display.setPanelBrightness(0);
    set_speaker_amplifier(false);
    ioe.digitalWrite(M5IOE1_PIN_3, LOW);
    ioe.digitalWrite(M5IOE1_PIN_8, LOW);
    if (pmic.shutdown() == M5PM1_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGE(TAG, "[POWER] PMIC power-off returned but CPU is still running");
    }
    ioe.digitalWrite(M5IOE1_PIN_8, HIGH);
    ioe.digitalWrite(M5IOE1_PIN_3, HIGH);
    display.setPanelBrightness(170);
failed:
    pmic.ldoSetPowerHold(true);
    if (feedback_task_handle) vTaskResume(feedback_task_handle);
    return false;
}

extern "C" bool stopwatch_board_imu_init(void) {
    if (imu_ready) return true;
    if (!i2c_bus) return false;
    if (!imu_device) {
        i2c_device_config_t cfg = {};
        cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        cfg.device_address = 0x68;
        cfg.scl_speed_hz = 400000;
        if (i2c_master_bus_add_device(i2c_bus, &cfg, &imu_device) != ESP_OK) return false;
    }
    imu = {};
    imu.intf = BMI2_I2C_INTF;
    imu.intf_ptr = &imu_device;
    imu.read = bmi_i2c_read;
    imu.write = bmi_i2c_write;
    imu.delay_us = bmi_delay_us;
    imu.read_write_len = 32;
    int8_t rc = bmi270_init(&imu);
    if (rc == BMI2_OK) {
        struct bmi2_sens_config config = {};
        config.type = BMI2_ACCEL;
        rc = bmi2_get_sensor_config(&config, 1, &imu);
        if (rc == BMI2_OK) {
            config.cfg.acc.odr = BMI2_ACC_ODR_100HZ;
            config.cfg.acc.range = BMI2_ACC_RANGE_4G;
            config.cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
            config.cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
            rc = bmi2_set_sensor_config(&config, 1, &imu);
        }
        if (rc == BMI2_OK) {
            uint8_t sensor = BMI2_ACCEL;
            rc = bmi2_sensor_enable(&sensor, 1, &imu);
        }
    }
    imu_ready = (rc == BMI2_OK);
    ESP_LOGI(TAG, "BMI270 %s (result %d)", imu_ready ? "ready" : "unavailable", rc);
    return imu_ready;
}

extern "C" bool stopwatch_board_imu_read(float *x, float *y, float *z) {
    if (!imu_ready || !x || !y || !z) return false;
    struct bmi2_sens_data data = {};
    if (bmi2_get_sensor_data(&data, &imu) != BMI2_OK || !(data.status & BMI2_DRDY_ACC)) return false;
    constexpr float scale = 4.0f / 32768.0f;
    // M5Stack's StopWatch demo swaps the sensor's X/Y axes to match the face.
    *x = data.acc.y * scale;
    *y = data.acc.x * scale;
    *z = data.acc.z * scale;
    return true;
}

extern "C" void Set_Backlight(uint8_t percent) {
    if (percent > 100) percent = 100;
    // AMOLED brightness is controlled through the CO5300, not a GPIO backlight.
    display.setPanelBrightness((uint32_t)percent * 255 / 100);
}
