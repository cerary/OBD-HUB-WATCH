#include "obd_data_cache.h"
#include "vehicle_profiles.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include <math.h>


// Simple globals protected by a critical section
static int16_t  s_coolant_temp = -40;
static int16_t  s_oil_temp = -100;
static int16_t  s_intake_temp = -40;
static int16_t  s_load_pct = -1;
static int16_t  s_tps = -1;
static int32_t  s_bat_mv = -1;
static int16_t  s_oil_pressure_x10 = -1;
static int16_t  s_brake_temp_x10 = -1000;
static int16_t  s_boost_x10 = -32768;
static int8_t   s_gear = 127;
static int16_t  s_afr_x100 = -1;
static brake_rs485_status_t s_brake_rs485_status = BRAKE_RS485_IDLE;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static int64_t s_sample_us[OBD_SAMPLE_COUNT];

#define SPEED_SMOOTH_TIME_MS 300   // speed ramp-up/down time constant (ms); the UI side already has anim_step, keep this small
#define FALL_TO_ZERO_MS      500   // fall-to-zero ramp-down time constant (ms)

// Smoothing state (advanced on the setter side, getters only read; unaffected by the number of callers)
static uint16_t s_rpm_smooth = 0;
static uint8_t  s_speed_smooth = 0;
static uint8_t  s_speed_unsmoothed = 0;
static TickType_t s_speed_last_tick = 0;
static float s_speed_smooth_f = 0.f;
static enGear s_estimated_gear = GEAR_UNKNOWN;
static int64_t s_gear_match_us;
static const vehicle_profile_t *s_gear_profile;
#define GEAR_TRANSITION_HOLD_US 1000000LL

// RPM override layer: during multi-gauge linkage tests, the master gauge injects simulated RPM here.
// When enabled, obd_data_get_rpm() returns the override value (used for both local display and ESP-NOW broadcast).
static bool     s_rpm_override_en = false;
static uint16_t s_rpm_override_val = 0;

void obd_data_rpm_override_set(bool en, uint16_t val)
{
    portENTER_CRITICAL(&s_mux);
    s_rpm_override_en = en;
    s_rpm_override_val = val;
    if (en) s_sample_us[OBD_SAMPLE_RPM] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_set_rpm(uint16_t rpm)
{
    portENTER_CRITICAL(&s_mux);
    s_rpm_smooth = rpm;  // CAN 100Hz data is already clean, no smoothing needed
    s_sample_us[OBD_SAMPLE_RPM] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_reset_temp_cache(void)
{
    portENTER_CRITICAL(&s_mux);
    s_coolant_temp = -40;
    s_oil_temp = -100;
    s_intake_temp = -40;
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_set_oil_temp_invalid(void)
{
    portENTER_CRITICAL(&s_mux);
    s_oil_temp = -100;
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_set_speed(uint8_t kmh)
{
    TickType_t now_tick = xTaskGetTickCount();
    portENTER_CRITICAL(&s_mux);
    uint32_t dt_ms = (now_tick - s_speed_last_tick) * portTICK_PERIOD_MS;
    if (dt_ms > 1000) dt_ms = 1000;
    s_speed_last_tick = now_tick;

    uint32_t tc = (kmh == 0) ? FALL_TO_ZERO_MS : SPEED_SMOOTH_TIME_MS;
    float alpha = s_sample_us[OBD_SAMPLE_SPEED] == 0 ? 1.0f : (float)dt_ms / (float)tc;
    if (alpha > 1.0f) alpha = 1.0f;
    s_speed_smooth_f += alpha * ((float)kmh - s_speed_smooth_f);

    uint8_t smoothed = (uint8_t)(s_speed_smooth_f + 0.5f);
    s_speed_smooth = smoothed;
    s_speed_unsmoothed = kmh;
    s_sample_us[OBD_SAMPLE_SPEED] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_set_coolant_temp(int16_t temp)
{
    portENTER_CRITICAL(&s_mux);
    s_coolant_temp = temp;
    s_sample_us[OBD_SAMPLE_CLT] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_set_oil_temp(int16_t temp)
{
    if (temp < -20 || temp > 150) return;
    portENTER_CRITICAL(&s_mux);
    s_oil_temp = temp;
    s_sample_us[OBD_SAMPLE_OIL] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_set_intake_temp(int16_t temp)
{
    portENTER_CRITICAL(&s_mux);
    s_intake_temp = temp;
    s_sample_us[OBD_SAMPLE_IAT] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

uint16_t obd_data_get_rpm(void)
{
    uint16_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_rpm_override_en ? s_rpm_override_val : s_rpm_smooth;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

uint8_t obd_data_get_speed(void)
{
    uint8_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_speed_smooth;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

int16_t obd_data_get_coolant_temp(void)
{
    int16_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_coolant_temp;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

int16_t obd_data_get_oil_temp(void)
{
    int16_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_oil_temp;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

int16_t obd_data_get_intake_temp(void)
{
    int16_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_intake_temp;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

void obd_data_set_load_pct(int16_t pct)
{
    portENTER_CRITICAL(&s_mux);
    s_load_pct = pct;
    s_sample_us[OBD_SAMPLE_LOAD] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

int16_t obd_data_get_load_pct(void)
{
    int16_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_load_pct;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

void obd_data_set_tps(int16_t pct)
{
    portENTER_CRITICAL(&s_mux);
    s_tps = pct;
    s_sample_us[OBD_SAMPLE_TPS] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

int16_t obd_data_get_tps(void)
{
    int16_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_tps;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

void obd_data_set_bat_mv(int32_t mv)
{
    portENTER_CRITICAL(&s_mux);
    s_bat_mv = mv;
    s_sample_us[OBD_SAMPLE_BAT] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

int32_t obd_data_get_bat_mv(void)
{
    int32_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_bat_mv;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

void obd_data_set_oil_pressure_x10(int16_t pressure_x10)
{
    if (pressure_x10 < 0 || pressure_x10 > 200) return;
    portENTER_CRITICAL(&s_mux);
    s_oil_pressure_x10 = pressure_x10;
    s_sample_us[OBD_SAMPLE_OILP] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

int16_t obd_data_get_oil_pressure_x10(void)
{
    int16_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_oil_pressure_x10;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

void obd_data_set_boost_x10(int16_t boost_x10)
{
    if (boost_x10 < -15 || boost_x10 > 300) return;
    portENTER_CRITICAL(&s_mux);
    s_boost_x10 = boost_x10;
    s_sample_us[OBD_SAMPLE_BOOST] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

int16_t obd_data_get_boost_x10(void)
{
    int16_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_boost_x10;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

void obd_data_set_brake_temp_x10(int16_t temp_x10)
{
    if (temp_x10 < -500 || temp_x10 > 12000) return;
    portENTER_CRITICAL(&s_mux);
    s_brake_temp_x10 = temp_x10;
    s_sample_us[OBD_SAMPLE_BKT] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_set_brake_rs485_status(brake_rs485_status_t status)
{
    portENTER_CRITICAL(&s_mux);
    s_brake_rs485_status = status;
    portEXIT_CRITICAL(&s_mux);
}

int16_t obd_data_get_brake_temp_x10(void)
{
    int16_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_brake_temp_x10;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

void obd_data_set_gear(int8_t gear)
{
    portENTER_CRITICAL(&s_mux);
    s_gear = gear;
    portEXIT_CRITICAL(&s_mux);
}

int8_t obd_data_get_gear(void)
{
    int8_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_gear;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

void obd_data_set_afr_x100(int16_t afr_x100)
{
    if (afr_x100 < 800 || afr_x100 > 2200) return;
    portENTER_CRITICAL(&s_mux);
    s_afr_x100 = afr_x100;
    s_sample_us[OBD_SAMPLE_AFR] = esp_timer_get_time();
    portEXIT_CRITICAL(&s_mux);
}

int16_t obd_data_get_afr_x100(void)
{
    int16_t val;
    portENTER_CRITICAL(&s_mux);
    val = s_afr_x100;
    portEXIT_CRITICAL(&s_mux);
    return val;
}

static void copy_snapshot_locked(obd_data_snapshot_t *out)
{
    out->rpm = s_rpm_override_en ? s_rpm_override_val : s_rpm_smooth;
    out->speed = s_speed_smooth;
    out->speed_unsmoothed = s_speed_unsmoothed;
    out->coolant_temp = s_coolant_temp;
    out->oil_temp = s_oil_temp;
    out->intake_temp = s_intake_temp;
    out->load_pct = s_load_pct;
    out->tps = s_tps;
    out->bat_mv = s_bat_mv;
    out->oil_pressure_x10 = s_oil_pressure_x10;
    out->boost_x10 = s_boost_x10;
    out->brake_temp_x10 = s_brake_temp_x10;
    out->gear = s_gear;
    out->afr_x100 = s_afr_x100;
    out->brake_rs485_status = s_brake_rs485_status;
}

void obd_data_get_snapshot(obd_data_snapshot_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_mux);
    copy_snapshot_locked(out);
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_get_fresh_snapshot(obd_data_snapshot_t *out, obd_data_freshness_t *freshness)
{
    if (!out || !freshness) return;
    portENTER_CRITICAL(&s_mux);
    copy_snapshot_locked(out);
    const int64_t now = esp_timer_get_time();
    for (unsigned i = 0; i < OBD_SAMPLE_COUNT; ++i) {
        uint64_t age = s_sample_us[i] > 0 ? (uint64_t)(now - s_sample_us[i]) / 1000 : UINT32_MAX;
        freshness->age_ms[i] = age >= UINT32_MAX ? UINT32_MAX : (uint32_t)age;
    }
    portEXIT_CRITICAL(&s_mux);
}

void obd_data_invalidate_freshness(void)
{
    portENTER_CRITICAL(&s_mux);
    for (unsigned i = 0; i < OBD_SAMPLE_COUNT; ++i) s_sample_us[i] = 0;
    s_rpm_smooth = 0;
    s_speed_smooth = 0;
    s_speed_unsmoothed = 0;
    s_speed_smooth_f = 0.f;
    s_speed_last_tick = 0;
    s_rpm_override_en = false;
    s_rpm_override_val = 0;
    s_coolant_temp = s_intake_temp = -40;
    s_oil_temp = -100;
    s_load_pct = s_tps = s_oil_pressure_x10 = s_afr_x100 = -1;
    s_bat_mv = -1;
    s_boost_x10 = -32768;
    s_brake_temp_x10 = -1000;
    s_gear = 127;
    s_estimated_gear = GEAR_UNKNOWN;
    s_gear_match_us = 0;
    s_gear_profile = NULL;
    s_brake_rs485_status = BRAKE_RS485_IDLE;
    portEXIT_CRITICAL(&s_mux);
}

bool obd_data_sample_is_fresh(const obd_data_freshness_t *f, obd_sample_channel_t channel)
{
    if (!f || (unsigned)channel >= OBD_SAMPLE_COUNT) return false;
    uint32_t limit = (channel == OBD_SAMPLE_CLT || channel == OBD_SAMPLE_IAT ||
                      channel == OBD_SAMPLE_OIL || channel == OBD_SAMPLE_BAT ||
                      channel == OBD_SAMPLE_BKT) ? 15000 : 5000;
    return f->age_ms[channel] <= limit;
}

void obd_data_apply_freshness(obd_data_snapshot_t *out, const obd_data_freshness_t *f)
{
    if (!out || !f) return;
#define EXPIRE(channel, field, invalid) \
    if (!obd_data_sample_is_fresh(f, channel)) out->field = invalid
    EXPIRE(OBD_SAMPLE_RPM, rpm, 0);
    EXPIRE(OBD_SAMPLE_SPEED, speed, 0);
    EXPIRE(OBD_SAMPLE_SPEED, speed_unsmoothed, 0);
    EXPIRE(OBD_SAMPLE_CLT, coolant_temp, -40);
    EXPIRE(OBD_SAMPLE_IAT, intake_temp, -40);
    EXPIRE(OBD_SAMPLE_OIL, oil_temp, -100);
    EXPIRE(OBD_SAMPLE_LOAD, load_pct, -1);
    EXPIRE(OBD_SAMPLE_TPS, tps, -1);
    EXPIRE(OBD_SAMPLE_BAT, bat_mv, -1);
    EXPIRE(OBD_SAMPLE_OILP, oil_pressure_x10, -1);
    EXPIRE(OBD_SAMPLE_BKT, brake_temp_x10, -1000);
    EXPIRE(OBD_SAMPLE_BOOST, boost_x10, -32768);
    EXPIRE(OBD_SAMPLE_AFR, afr_x100, -1);
#undef EXPIRE
    if (!obd_data_sample_is_fresh(f, OBD_SAMPLE_RPM) ||
        !obd_data_sample_is_fresh(f, OBD_SAMPLE_SPEED)) out->gear = 127;
}

/**
 * @brief Compute and determine the gear from RPM and vehicle speed
 * @param rpm engine speed (RPM)
 * @param speed vehicle speed (km/h)
 * @return the computed gear
 */
void obd_data_reset_gear_estimate(void)
{
    portENTER_CRITICAL(&s_mux);
    s_estimated_gear = GEAR_UNKNOWN;
    s_gear_match_us = 0;
    s_gear_profile = NULL;
    portEXIT_CRITICAL(&s_mux);
}

enGear calculate_gear(float rpm, float speed) {
    if (!isfinite(rpm) || !isfinite(speed) || rpm < 0 || speed < 0) {
        obd_data_reset_gear_estimate();
        return GEAR_UNKNOWN;
    }
    if (speed == 0) {
        obd_data_reset_gear_estimate();
        return GEAR_NEUTRAL; // Stationary placeholder, not a P/N/D/R selector reading.
    }
    if (rpm == 0 || speed < 5) {
        obd_data_reset_gear_estimate();
        return GEAR_UNKNOWN; // Creep and integer-speed quantization cannot identify a reliable gear.
    }

    // Estimate total ratio from wheel speed, and compare it with gear * final drive.
    const vehicle_profile_t *profile = vehicle_profile_get_active();
    float calc_const = vehicle_profile_calc_constant(profile);
    if (!profile || profile->final_drive_ratio <= 0 || !isfinite(calc_const) || calc_const <= 0) {
        obd_data_reset_gear_estimate();
        return GEAR_UNKNOWN;
    }
    float total_ratio = rpm / (speed * calc_const);
    enGear matched = GEAR_UNKNOWN;
    float best_error = INFINITY;
    uint8_t range_count = 0;
    const gear_ratio_range_t *ranges = vehicle_profile_get_gear_ranges(&range_count);
    for (int i = 0; i < range_count; i++) {
        if (total_ratio >= ranges[i].min_ratio &&
            total_ratio <= ranges[i].max_ratio) {
            // Some inherited profiles have overlapping tolerances. Within the
            // accepted ranges, prefer the closest center, not the first gear.
            float center = (ranges[i].min_ratio + ranges[i].max_ratio) * 0.5f;
            float error = fabsf(total_ratio / center - 1.0f);
            if (error < best_error) {
                best_error = error;
                matched = ranges[i].gear;
            }
        }
    }

    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&s_mux);
    if (s_gear_profile != profile) {
        s_estimated_gear = GEAR_UNKNOWN;
        s_gear_profile = profile;
    }
    if (matched != GEAR_UNKNOWN) {
        s_estimated_gear = matched;
        s_gear_match_us = now;
    } else if (now < s_gear_match_us || now - s_gear_match_us >= GEAR_TRANSITION_HOLD_US) {
        s_estimated_gear = GEAR_UNKNOWN;
    }
    enGear result = s_estimated_gear;
    portEXIT_CRITICAL(&s_mux);
    return result;
}


/**
 * @brief Mileage statistics timer callback
 * @param pvParameter argument
 * @return none
 * @note
 * @note Mileage statistics task
 */
static void mileage_timer_cb(void* arg)
{
    (void)arg;
    obd_data_snapshot_t snap;
    obd_data_freshness_t freshness;
    obd_data_get_fresh_snapshot(&snap, &freshness);
    if (obd_data_sample_is_fresh(&freshness, OBD_SAMPLE_SPEED))
        nvs_stat_update_speed(snap.speed, 1000);
}

/**
 * @brief Initialize the mileage statistics task
 * @return none
 * @note
 * @note Initialize the mileage statistics task
 */
void vMileageDataStatisticTask(void)
{
    static esp_timer_handle_t s_timer = NULL;
    if(!s_timer){
        const esp_timer_create_args_t args={
            .callback = mileage_timer_cb,
            .arg = NULL,
            .name = "mile_stat"
        };
        if(esp_timer_create(&args,&s_timer)==ESP_OK){
            esp_timer_start_periodic(s_timer, 1000000); //1s
        }
    }
}
