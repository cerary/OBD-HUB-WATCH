#include "temperature_alert_policy.h"
#include <limits.h>
#include <string.h>

temperature_limit_t temperature_limit(uint8_t channel, int16_t hot_c)
{
    temperature_limit_t out = {0};
    if (channel >= TEMP_CHANNEL_COUNT || hot_c == INT16_MAX) return out;
    static const uint8_t delta[] = {5, 20, 10};
    out.hot_c = hot_c;
    out.warm_c = hot_c - delta[channel];
    out.warm_hold_ms = channel == TEMP_IAT ? 30000U : 10000U;
    out.hot_hold_ms = channel == TEMP_IAT ? 10000U : 3000U;
    out.hysteresis_c = channel == TEMP_CLT ? 3 : 5;
    out.enabled = true;
    return out;
}

void temperature_alert_reset(temperature_alert_policy_t *p)
{
    if (p) memset(p, 0, sizeof(*p));
}

static void update_level(bool *latched, bool *pending, uint32_t *since,
                         uint32_t now, uint32_t age, int32_t value,
                         int16_t threshold, uint32_t hold, uint8_t hysteresis)
{
    if (*latched && value <= threshold - hysteresis) *latched = false;
    if (*latched || value < threshold) {
        *pending = false;
        return;
    }
    if (!*pending) {
        *pending = true;
        *since = now;
    }
    uint32_t elapsed = now - *since;
    // A newly confirmed sample must cover the hold interval. Repainting one
    // cached sample for ten seconds does not establish sustained high heat.
    if (elapsed >= age && elapsed - age >= hold) {
        *latched = true;
        *pending = false;
    }
}

void temperature_alert_update(temperature_alert_policy_t *p, uint32_t now,
                              bool active, const int32_t value[3],
                              const uint32_t age[3], const bool valid[3],
                              const int16_t hot_c[3])
{
    if (!p) return;
    if (!active) { temperature_alert_reset(p); return; }
    for (uint8_t i = 0; i < TEMP_CHANNEL_COUNT; ++i) {
        temperature_channel_state_t *s = &p->channel[i];
        temperature_limit_t limit = temperature_limit(i, hot_c[i]);
        if (!limit.enabled || !valid[i] || age[i] > 15000U) {
            memset(s, 0, sizeof(*s));
            p->level[i] = TEMP_NORMAL;
            continue;
        }
        if (!s->configured || s->hot_c != hot_c[i]) {
            memset(s, 0, sizeof(*s));
            s->configured = true;
            s->hot_c = hot_c[i];
        }
        update_level(&s->warm_latched, &s->warm_pending, &s->warm_since,
                     now, age[i], value[i], limit.warm_c, limit.warm_hold_ms,
                     limit.hysteresis_c);
        update_level(&s->hot_latched, &s->hot_pending, &s->hot_since,
                     now, age[i], value[i], limit.hot_c, limit.hot_hold_ms,
                     limit.hysteresis_c);
        // Falling from HOT retains the WARM stage until its recovery limit,
        // even when HOT's shorter hold fired before the WARM timer.
        if (s->hot_latched) s->warm_latched = true;
        p->level[i] = s->hot_latched ? TEMP_HOT :
                      s->warm_latched ? TEMP_WARM : TEMP_NORMAL;
    }
}
