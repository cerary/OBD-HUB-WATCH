#include "status_ring_policy.h"
#include <limits.h>
#include "temperature_alert_policy.h"

static uint32_t blend(uint32_t a, uint32_t b, int32_t n, int32_t d)
{
    if (n <= 0 || d <= 0) return a;
    if (n >= d) return b;
    uint32_t out = 0;
    for (int s = 0; s <= 16; s += 8) {
        int32_t av = (a >> s) & 255, bv = (b >> s) & 255;
        out |= (uint32_t)(av + (bv - av) * n / d) << s;
    }
    return out;
}

static uint32_t toward_red(int32_t n, int32_t d)
{
    if (n <= 0) return STATUS_RING_GREEN;
    if (n >= d) return STATUS_RING_RED;
    // Pass through yellow instead of the muddy RGB midpoint of green and red.
    if (n * 2 <= d) return blend(STATUS_RING_GREEN, 0xFFD166, n * 2, d);
    return blend(0xFFD166, STATUS_RING_RED, n * 2 - d, d);
}

status_ring_config_t status_ring_default_config(void)
{
    return (status_ring_config_t){2400, 4000, 5500, 100, 100, {0,0,0}};
}
bool status_ring_config_valid(const status_ring_config_t *c)
{
    return c && c->rpm_green >= 500 && c->rpm_red <= 8000 &&
           c->rpm_green + 300 <= c->rpm_approach &&
           c->rpm_approach + 300 <= c->rpm_red &&
           c->g_warn_centi >= 50 && c->g_warn_centi <= 200 &&
           c->brightness >= 10 && c->brightness <= 100;
}

static bool fresh(const status_ring_input_t *in, uint8_t item)
{
    uint32_t limit = (item == RING_CLT || item == RING_IAT || item == RING_OIL ||
                      item == RING_BAT || item == RING_BKT) ? 15000 : 5000;
    return in->valid[item] && (in->demo || in->age_ms[item] <= limit);
}

static status_ring_result_t result(uint32_t color, status_ring_state_t state,
                                   uint8_t item)
{
    return (status_ring_result_t){color, state, item};
}

static status_ring_result_t with_temperature(status_ring_result_t out,
                                             status_ring_result_t temperature)
{
    // Actual parameter/G alarms outrank an early temperature warning.
    return out.state != RING_ALARM && temperature.state == RING_APPROACH ?
           temperature : out;
}

status_ring_result_t status_ring_evaluate(const status_ring_input_t *in,
                                         const status_ring_config_t *config)
{
    status_ring_config_t defaults = status_ring_default_config();
    const status_ring_config_t *cfg = status_ring_config_valid(config) ? config : &defaults;
    if (in->sleeping) return result(STATUS_RING_SLEEP_COLOR, RING_SLEEP, RING_ITEM_COUNT);
    if (!in->demo && !in->connected)
        return result(in->ever_connected ? STATUS_RING_SLEEP_COLOR : STATUS_RING_BLUE,
                      in->ever_connected ? RING_NO_DATA : RING_CONNECTING, RING_ITEM_COUNT);

    bool any_fresh = false;
    for (uint8_t i = 0; i < RING_ITEM_COUNT; ++i) if (fresh(in, i)) any_fresh = true;
    if (!in->demo && !any_fresh)
        return result(in->initializing ? STATUS_RING_BLUE : STATUS_RING_SLEEP_COLOR,
                      in->initializing ? RING_CONNECTING : RING_NO_DATA, RING_ITEM_COUNT);

    status_ring_result_t temperature = result(STATUS_RING_WHITE, RING_NORMAL, RING_ITEM_COUNT);
    // Enabled temperature alarms have priority on live pages, using only real,
    // fresh samples. Unsupported or expired channels cannot create an alarm.
    if (in->page != RING_PAGE_STATIC) {
        const uint8_t global[] = {RING_CLT, RING_OIL};
        for (unsigned j = 0; j < 2; ++j) {
            uint8_t i = global[j];
            if (in->temperature_managed) {
                if (!fresh(in, i)) continue;
                if (in->temperature_level[i] == TEMP_HOT)
                    return result(STATUS_RING_RED, RING_ALARM, i);
                if (in->temperature_level[i] == TEMP_WARM && temperature.state == RING_NORMAL)
                    temperature = result(STATUS_RING_YELLOW, RING_APPROACH, i);
            } else if (in->alarm[i] != INT16_MAX && fresh(in, i) && in->value[i] >= in->alarm[i])
                return result(STATUS_RING_RED, RING_ALARM, i);
        }
    }
    if (in->page == RING_PAGE_G) {
        if (!in->g_valid || in->g_age_ms > 500)
            return with_temperature(result(STATUS_RING_LOST_COLOR, RING_LOST, RING_ITEM_COUNT), temperature);
        int32_t g = in->g_centi, limit = cfg->g_warn_centi;
        if (g >= limit) return result(STATUS_RING_RED, RING_ALARM, RING_ITEM_COUNT);
        if (g <= limit / 3) return with_temperature(result(STATUS_RING_WHITE, RING_NORMAL, RING_ITEM_COUNT), temperature);
        if (g < limit * 2 / 3)
            return with_temperature(result(blend(STATUS_RING_WHITE, STATUS_RING_GREEN,
                               g - limit / 3, limit / 3), RING_NORMAL, RING_ITEM_COUNT), temperature);
        return with_temperature(result(toward_red(g - limit * 2 / 3, limit - limit * 2 / 3),
                      RING_APPROACH, RING_ITEM_COUNT), temperature);
    }
    status_ring_result_t out = result(STATUS_RING_WHITE, RING_NORMAL, RING_ITEM_COUNT);
    bool stale = false;
    int32_t approach_score = -1;
    for (uint8_t n = 0; n < in->item_count && n < 5; ++n) {
        uint8_t i = in->items[n];
        if (i >= RING_ITEM_COUNT) continue;
        if (!fresh(in, i)) {
            // A never-supported field is ignored; a formerly valid field must
            // not keep its old green/red color when it stops updating.
            if (in->age_ms[i] != UINT32_MAX || i == RING_RPM || i == RING_SPEED) stale = true;
            continue;
        }
        int32_t v = in->value[i], thr = in->alarm[i];
        if (i == RING_RPM) {
            if (v >= cfg->rpm_red) return result(STATUS_RING_RED, RING_ALARM, i);
            if (v >= cfg->rpm_approach) {
                int32_t score = (v - cfg->rpm_approach) * 1000 / (cfg->rpm_red - cfg->rpm_approach);
                if (score > approach_score) {
                    approach_score = score;
                    out = result(toward_red(v - cfg->rpm_approach, cfg->rpm_red - cfg->rpm_approach), RING_APPROACH, i);
                }
            } else if (out.state == RING_NORMAL) {
                uint32_t color = v < cfg->rpm_green ?
                    blend(STATUS_RING_WHITE, STATUS_RING_GREEN, v - cfg->rpm_green + 500, 500) : STATUS_RING_GREEN;
                out = result(color, RING_NORMAL, i);
            }
        } else if (in->temperature_managed && i < TEMP_CHANNEL_COUNT) {
            // IAT is advisory only on pages that display it; never a global
            // engine-overheat alarm. CLT/OIL were handled above.
            if (i == RING_IAT && temperature.state == RING_NORMAL &&
                in->temperature_level[i] != TEMP_NORMAL)
                temperature = result(in->temperature_level[i] == TEMP_HOT ?
                    STATUS_RING_ORANGE : STATUS_RING_YELLOW, RING_APPROACH, i);
        } else if (thr != INT16_MAX && thr > 0) {
            if (v >= thr) return result(STATUS_RING_RED, RING_ALARM, i);
            // The existing chart limits remain authoritative. Do not invent
            // engine-specific temperature or road-speed limits.
            int32_t start = thr - thr / 10;
            if (v >= start) {
                int32_t score = (v - start) * 1000 / (thr - start);
                if (score > approach_score) {
                    approach_score = score;
                    out = result(blend(STATUS_RING_WHITE, 0xFFAC45, v - start, thr - start), RING_APPROACH, i);
                }
            }
        }
    }
    if (stale) return with_temperature(result(in->initializing ? STATUS_RING_BLUE : STATUS_RING_LOST_COLOR,
                             in->initializing ? RING_CONNECTING : RING_LOST, RING_ITEM_COUNT), temperature);
    return with_temperature(out, temperature);
}
