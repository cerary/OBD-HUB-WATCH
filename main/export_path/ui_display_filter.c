#include "ui_display_filter.h"

#include <stdbool.h>
#include <string.h>

typedef enum {
    FILTER_RPM,
    FILTER_SPEED,
    FILTER_CLT,
    FILTER_OIL,
    FILTER_IAT,
    FILTER_LOAD,
    FILTER_TPS,
    FILTER_BAT,
    FILTER_OILP,
    FILTER_BOOST,
    FILTER_BRAKE,
    FILTER_AFR,
    FILTER_COUNT,
} filter_id_t;

typedef struct {
    float average;
    int32_t shown;
    int32_t pending;
    uint32_t last_ms;
    uint32_t pending_since_ms;
    bool initialized;
    bool has_pending;
} filter_state_t;

typedef struct {
    uint16_t time_ms;    // EMA time constant; zero means already smoothed upstream
    uint16_t jump;       // larger changes follow the EMA immediately
    uint16_t settle_ms;  // smaller changes must persist before reaching the display
} filter_config_t;

// Units match obd_data_snapshot_t. Speed already has a 300 ms filter in the
// cache, so only its final 1 km/h display jitter is held here.
static const filter_config_t s_config[FILTER_COUNT] = {
    [FILTER_RPM]   = {120, 18, 350},
    [FILTER_SPEED] = {0, 2, 550},
    [FILTER_CLT]   = {450, 2, 1500},
    [FILTER_OIL]   = {450, 2, 1500},
    [FILTER_IAT]   = {450, 2, 1500},
    [FILTER_LOAD]  = {180, 2, 400},
    [FILTER_TPS]   = {180, 2, 400},
    [FILTER_BAT]   = {500, 120, 850},
    [FILTER_OILP]  = {200, 2, 400},
    [FILTER_BOOST] = {200, 2, 400},
    [FILTER_BRAKE] = {350, 5, 900},
    [FILTER_AFR]   = {220, 15, 450},
};
static filter_state_t s_state[FILTER_COUNT];

void ui_display_filter_reset(void)
{
    memset(s_state, 0, sizeof(s_state));
}

static int32_t rounded(float value)
{
    return (int32_t)(value + (value >= 0.0f ? 0.5f : -0.5f));
}

static int32_t filter_value(filter_id_t id, int32_t raw, bool valid, uint32_t now_ms)
{
    filter_state_t *state = &s_state[id];
    const filter_config_t *config = &s_config[id];
    if (!valid) {
        state->initialized = false;
        state->has_pending = false;
        return raw; // preserve the existing invalid sentinel and "--" rendering
    }

    uint32_t elapsed = now_ms - state->last_ms;
    if (!state->initialized || elapsed > 1500U) {
        state->average = (float)raw;
        state->shown = raw;
        state->last_ms = now_ms;
        state->initialized = true;
        state->has_pending = false;
        return raw; // first valid sample and page return appear without a ramp
    }

    state->last_ms = now_ms;
    if (config->time_ms == 0U) {
        state->average = (float)raw;
    } else if (elapsed != 0U) {
        float alpha = (float)elapsed / ((float)config->time_ms + (float)elapsed);
        state->average += alpha * ((float)raw - state->average);
    }

    int32_t candidate = rounded(state->average);
    int32_t difference = candidate - state->shown;
    int32_t magnitude = difference >= 0 ? difference : -difference;
    if (magnitude >= config->jump) {
        state->shown = candidate;
        state->has_pending = false;
    } else if (difference == 0) {
        state->has_pending = false;
    } else if (!state->has_pending || state->pending != candidate) {
        state->pending = candidate;
        state->pending_since_ms = now_ms;
        state->has_pending = true;
    } else if ((uint32_t)(now_ms - state->pending_since_ms) >= config->settle_ms) {
        state->shown = candidate;
        state->has_pending = false;
    }
    return state->shown;
}

void ui_display_filter_apply(const obd_data_snapshot_t *raw,
                             obd_data_snapshot_t *display,
                             uint32_t now_ms)
{
    if (!raw || !display) return;
    *display = *raw;
    display->rpm = (uint16_t)filter_value(FILTER_RPM, raw->rpm, true, now_ms);
    display->speed = (uint8_t)filter_value(FILTER_SPEED, raw->speed, true, now_ms);
    display->coolant_temp = (int16_t)filter_value(FILTER_CLT, raw->coolant_temp, raw->coolant_temp > -40, now_ms);
    display->oil_temp = (int16_t)filter_value(FILTER_OIL, raw->oil_temp, raw->oil_temp > -41, now_ms);
    display->intake_temp = (int16_t)filter_value(FILTER_IAT, raw->intake_temp, raw->intake_temp > -40, now_ms);
    display->load_pct = (int16_t)filter_value(FILTER_LOAD, raw->load_pct, raw->load_pct >= 0, now_ms);
    display->tps = (int16_t)filter_value(FILTER_TPS, raw->tps, raw->tps >= 0, now_ms);
    display->bat_mv = filter_value(FILTER_BAT, raw->bat_mv, raw->bat_mv > 0, now_ms);
    display->oil_pressure_x10 = (int16_t)filter_value(FILTER_OILP, raw->oil_pressure_x10, raw->oil_pressure_x10 >= 0, now_ms);
    display->boost_x10 = (int16_t)filter_value(FILTER_BOOST, raw->boost_x10, raw->boost_x10 != -32768, now_ms);
    display->brake_temp_x10 = (int16_t)filter_value(FILTER_BRAKE, raw->brake_temp_x10, raw->brake_temp_x10 > -1000, now_ms);
    display->afr_x100 = (int16_t)filter_value(FILTER_AFR, raw->afr_x100, raw->afr_x100 >= 800 && raw->afr_x100 <= 2200, now_ms);
}
