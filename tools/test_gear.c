#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "app_obd_dsp/obd_data_cache.h"
#include "app_obd_dsp/vehicle_profiles.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "esp_timer.h"
#include "freertos/task.h"

static int64_t now_us = 1000000;
static nvs_user_cfg_t cfg;
const nvs_user_cfg_t *nvs_cfg_get(void) { return &cfg; }
esp_err_t nvs_cfg_set(const nvs_user_cfg_t *value) { cfg = *value; return ESP_OK; }
int64_t esp_timer_get_time(void) { return now_us; }
TickType_t xTaskGetTickCount(void) { return (TickType_t)(now_us / 1000); }

int main(void)
{
    uint8_t count;
    const vehicle_profile_t *profiles = vehicle_profile_get_all(&count);
    int jcw = -1, cases = 0;
    // Independently derive road speed from circumference, not the production constant.
    const double circumference_factor = 2.0 * acos(-1.0) * 60.0 / 1000.0;
    for (int p = 0; p < count; ++p) {
        vehicle_profile_set_active(p);
        if (!strcmp(profiles[p].name, "JCW F56 8AT")) jcw = p;
        for (int gear = 1; gear <= profiles[p].gear_count; ++gear) {
            for (int rpm = 1200; rpm <= 6000; rpm += 600) {
                double speed = rpm * circumference_factor * profiles[p].tire_rolling_radius_m /
                    (profiles[p].gear_ratios[gear] * profiles[p].final_drive_ratio);
                if (speed < 5 || speed > 255) continue;
                obd_data_reset_gear_estimate();
                enGear estimated = calculate_gear(rpm, round(speed));
                if (estimated != (enGear)gear)
                    fprintf(stderr, "profile=%s gear=%d rpm=%d speed=%.3f rounded=%.0f got=%d\n",
                            profiles[p].name, gear, rpm, speed, round(speed), estimated);
                assert(estimated == (enGear)gear);
                ++cases;
            }
        }
    }
    assert(jcw >= 0);
    vehicle_profile_set_active(jcw);
    const vehicle_profile_t *p = vehicle_profile_get_active();
    assert(fabsf(p->tire_rolling_radius_m - 0.3146f) < 0.000001f);
    assert(calculate_gear(3000, 59) == GEAR_3);
    assert(calculate_gear(3000, 179) == GEAR_8);

    // Unmatched shift/slip ratios retain the old gear for at most 1 second.
    assert(calculate_gear(3000, 59) == GEAR_3);
    now_us += 500000;
    assert(calculate_gear(3000, 45) == GEAR_3);
    now_us += 499000;
    assert(calculate_gear(3000, 45) == GEAR_3);
    now_us += 1000;
    assert(calculate_gear(3000, 45) == GEAR_UNKNOWN);
    assert(calculate_gear(3000, 179) == GEAR_8);

    // Time reversal, fresh session, and vehicle changes cannot revive an old estimate.
    now_us -= 100000;
    assert(calculate_gear(3000, 45) == GEAR_UNKNOWN);
    assert(calculate_gear(3000, 59) == GEAR_3);
    obd_data_invalidate_freshness();
    assert(calculate_gear(3000, 45) == GEAR_UNKNOWN);
    assert(calculate_gear(3000, 59) == GEAR_3);
    vehicle_profile_set_active(jcw);
    assert(calculate_gear(3000, 45) == GEAR_UNKNOWN);
    assert(calculate_gear(800, 0) == GEAR_NEUTRAL);
    assert(calculate_gear(0, 0) == GEAR_NEUTRAL);
    assert(calculate_gear(1200, 3) == GEAR_UNKNOWN);
    assert(calculate_gear(0, 30) == GEAR_UNKNOWN);
    assert(calculate_gear(NAN, 30) == GEAR_UNKNOWN);
    assert(calculate_gear(3000, INFINITY) == GEAR_UNKNOWN);
    assert(calculate_gear(3000, -1) == GEAR_UNKNOWN);

    // Display ramp remains intact, but the ratio uses the accepted unsmoothed sample.
    obd_data_invalidate_freshness();
    obd_data_set_speed(60);
    now_us += 100000;
    obd_data_set_speed(81);
    obd_data_snapshot_t data;
    obd_data_freshness_t freshness;
    obd_data_set_rpm(3000);
    obd_data_get_fresh_snapshot(&data, &freshness);
    assert(data.speed < 81 && data.speed_unsmoothed == 81);
    assert(calculate_gear(data.rpm, data.speed_unsmoothed) == GEAR_4);
    now_us += 5001000;
    obd_data_get_fresh_snapshot(&data, &freshness);
    obd_data_apply_freshness(&data, &freshness);
    assert(data.speed_unsmoothed == 0 && data.gear == GEAR_UNKNOWN);

    printf("PASS: %d independent circumference cases across %u real profiles; JCW 1..8, 215/40R18, bounded shift hold, resets, unknown and unsmoothed speed\n", cases, count);
    return 0;
}
