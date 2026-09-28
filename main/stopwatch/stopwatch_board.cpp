// M5Stack StopWatch hardware bridge. Power and panel sequence were validated
// on the user's device with ESP-IDF 5.5.4. Based on M5StopWatch-UserDemo (MIT).
#include "stopwatch_board.h"
#include "cst820.h"

#include <M5GFX.h>
#include <M5IOE1.h>
#include <M5PM1.h>
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

namespace {
constexpr char TAG[] = "stopwatch";

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
M5IOE1 ioe;
Cst820 touch;
bool touch_ready = false;
bool touch_filtered = false;
bool touch_candidate = false;
uint32_t touch_candidate_since = 0;
uint16_t touch_x = 0;
uint16_t touch_y = 0;
}

extern "C" bool stopwatch_board_init(void) {
    i2c_master_bus_config_t i2c_cfg = {};
    i2c_cfg.i2c_port = I2C_NUM_0;
    i2c_cfg.sda_io_num = GPIO_NUM_47;
    i2c_cfg.scl_io_num = GPIO_NUM_48;
    i2c_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_cfg.glitch_ignore_cnt = 7;
    i2c_cfg.flags.enable_internal_pullup = true;
    i2c_master_bus_handle_t bus = nullptr;
    if (i2c_new_master_bus(&i2c_cfg, &bus) != ESP_OK) return false;

    if (pmic.begin(bus) != M5PM1_OK) return false;
    pmic.setI2cSleepTime(0);
    pmic.wdtSet(0);
    pmic.ldoSetPowerHold(true);

    auto ioe_result = ioe.begin(bus, 0x4F, M5IOE1_I2C_FREQ_400K);
    if (ioe_result != M5IOE1_OK)
        ioe_result = ioe.begin(bus, 0x6F, M5IOE1_I2C_FREQ_400K);
    if (ioe_result != M5IOE1_OK) return false;
    ioe.setI2cSleepTime(0);
    ioe.pinMode(M5IOE1_PIN_8, OUTPUT);
    ioe.pinMode(M5IOE1_PIN_5, OUTPUT);
    ioe.pinMode(M5IOE1_PIN_4, OUTPUT);
    ioe.digitalWrite(M5IOE1_PIN_8, 1);
    ioe.digitalWrite(M5IOE1_PIN_4, 1);
    ioe.digitalWrite(M5IOE1_PIN_5, 1);
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
    touch_ready = touch.begin(bus, 0x15);
    ESP_LOGI(TAG, "CST820 touch %s", touch_ready ? "ready" : "unavailable");
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
    if (last) display.display();
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
    if (!touch_filtered) return false;
    *x = touch_x;
    *y = touch_y;
    return true;
}

extern "C" void Set_Backlight(uint8_t percent) {
    if (percent > 100) percent = 100;
    // AMOLED brightness is controlled through the CO5300, not a GPIO backlight.
    display.setPanelBrightness((uint32_t)percent * 255 / 100);
}
