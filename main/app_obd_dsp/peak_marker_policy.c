#include "peak_marker_policy.h"
#include <string.h>

void peak_marker_reset(peak_marker_state_t *state) { memset(state, 0, sizeof(*state)); }

void peak_marker_sample(peak_marker_state_t *state, int32_t value, bool valid,
                        uint32_t now, uint32_t lifetime_ms)
{
    if (!valid || value < 0 || lifetime_ms == 0) { peak_marker_reset(state); return; }
    if (state->falling && now - state->fall_tick >= lifetime_ms) peak_marker_reset(state);
    if (!state->armed || value > state->peak) {
        state->peak = value;
        state->armed = true;
        state->falling = false;
    } else if (value < state->peak && !state->falling) {
        state->fall_tick = now;
        state->falling = true;
    }
    // Equal or lower samples cannot extend a previously started retention period.
}

uint8_t peak_marker_opacity(const peak_marker_state_t *state, uint32_t now,
                            uint32_t lifetime_ms)
{
    if (!state->armed || !state->falling || state->peak <= 0 || lifetime_ms == 0) return 0;
    uint32_t age = now - state->fall_tick;
    if (age >= lifetime_ms) return 0;
    uint32_t hold = lifetime_ms / 5;
    if (age <= hold) return 255;
    return (uint8_t)((lifetime_ms - age) * 255U / (lifetime_ms - hold));
}
