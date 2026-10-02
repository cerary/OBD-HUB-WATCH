#pragma once
#include <stdbool.h>
#include <stdint.h>

// Temporary display override; never replaces the user's saved brightness.
typedef struct {
    uint8_t requested_percent;
    bool sleep_warning;
} stopwatch_brightness_policy_t;

static inline uint8_t stopwatch_brightness_effective(const stopwatch_brightness_policy_t *p) {
    uint8_t value = p->requested_percent > 100 ? 100 : p->requested_percent;
    return p->sleep_warning ? (uint8_t)((value + 1) / 2) : value;
}
