#pragma once
#include <stdbool.h>
#include <stdint.h>

// Same channel order as CLT/IAT/OIL in the cache and display-item table.
enum { TEMP_CLT, TEMP_IAT, TEMP_OIL, TEMP_CHANNEL_COUNT };
typedef enum { TEMP_NORMAL, TEMP_WARM, TEMP_HOT } temperature_level_t;
typedef struct {
    int16_t warm_c, hot_c;
    uint32_t warm_hold_ms, hot_hold_ms;
    uint8_t hysteresis_c;
    bool enabled;
} temperature_limit_t;
typedef struct {
    int16_t hot_c;
    uint32_t warm_since, hot_since;
    bool configured, warm_pending, hot_pending, warm_latched, hot_latched;
} temperature_channel_state_t;
typedef struct {
    temperature_channel_state_t channel[TEMP_CHANNEL_COUNT];
    temperature_level_t level[TEMP_CHANNEL_COUNT];
} temperature_alert_policy_t;

// The existing per-item ALARM slider controls the high limit (32767 = OFF).
// These are user advisory defaults, not the vehicle ECU's protection limits.
temperature_limit_t temperature_limit(uint8_t channel, int16_t hot_c);
void temperature_alert_reset(temperature_alert_policy_t *policy);
void temperature_alert_update(temperature_alert_policy_t *policy, uint32_t now_ms,
                              bool active, const int32_t value[3],
                              const uint32_t age_ms[3], const bool valid[3],
                              const int16_t hot_c[3]);
