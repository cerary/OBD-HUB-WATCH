#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t peak;
    uint32_t fall_tick;
    bool armed, falling;
} peak_marker_state_t;

void peak_marker_reset(peak_marker_state_t *state);
void peak_marker_sample(peak_marker_state_t *state, int32_t value, bool valid,
                        uint32_t now, uint32_t lifetime_ms);
uint8_t peak_marker_opacity(const peak_marker_state_t *state, uint32_t now,
                            uint32_t lifetime_ms);
