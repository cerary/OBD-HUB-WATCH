#pragma once

#include <stdbool.h>
#include <stdint.h>

// The charger status is the source of truth. Battery voltage / estimated
// percentage cannot distinguish charging from a temporary load-induced dip.
#define CHARGE_LED_CONFIRM_MS 5000U
#define CHARGE_LED_MAX_SAMPLE_GAP_MS 1500U

typedef struct {
    bool tracking;
    uint64_t charging_since_ms;
    uint64_t last_sample_ms;
} charge_led_policy_t;

// Called every 500 ms. Missing input, external power loss or a released CHRG
// immediately clears both the lamp request and the pending confirmation.
static inline bool charge_led_policy_update(charge_led_policy_t *state,
                                           bool status_known, bool external_power,
                                           bool charger_active, uint64_t now_ms)
{
    if (!status_known || !external_power || !charger_active) {
        state->tracking = false;
        return false;
    }
    if (!state->tracking || now_ms < state->last_sample_ms ||
        now_ms - state->last_sample_ms > CHARGE_LED_MAX_SAMPLE_GAP_MS) {
        state->tracking = true;
        state->charging_since_ms = now_ms;
    }
    state->last_sample_ms = now_ms;
    return now_ms - state->charging_since_ms >= CHARGE_LED_CONFIRM_MS;
}
