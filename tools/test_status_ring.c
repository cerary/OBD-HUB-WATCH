#include "../main/app_obd_dsp/status_ring_policy.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static status_ring_config_t cfg;
static unsigned checks;
static status_ring_input_t input(void)
{
    status_ring_input_t in = {0};
    in.connected = in.ever_connected = true;
    in.page = RING_PAGE_ITEMS;
    in.items[0] = RING_RPM; in.item_count = 1;
    for (unsigned i = 0; i < RING_ITEM_COUNT; ++i) {
        in.age_ms[i] = UINT32_MAX; in.alarm[i] = INT16_MAX;
    }
    in.valid[RING_RPM] = true; in.value[RING_RPM] = 800; in.age_ms[RING_RPM] = 0;
    return in;
}
static status_ring_result_t expect(status_ring_input_t *in, status_ring_state_t state, uint32_t color)
{
    status_ring_result_t out = status_ring_evaluate(in, &cfg);
    assert(out.state == state); if (color) assert(out.color == color);
    ++checks; return out;
}
int main(void)
{
    cfg = status_ring_default_config(); assert(status_ring_config_valid(&cfg));
    status_ring_input_t in = input();
    in.connected = in.ever_connected = false; expect(&in, RING_CONNECTING, STATUS_RING_BLUE);
    in.ever_connected = true; expect(&in, RING_NO_DATA, STATUS_RING_SLEEP_COLOR);
    in.connected = in.initializing = true; in.age_ms[RING_RPM] = UINT32_MAX;
    expect(&in, RING_CONNECTING, STATUS_RING_BLUE);
    in.initializing = false; expect(&in, RING_NO_DATA, STATUS_RING_SLEEP_COLOR);
    in.valid[RING_CLT] = true; in.age_ms[RING_CLT] = 0; in.value[RING_CLT] = 85;
    in.initializing = true; expect(&in, RING_CONNECTING, STATUS_RING_BLUE);
    in.initializing = false; expect(&in, RING_LOST, STATUS_RING_LOST_COLOR);
    in = input(); in.value[RING_RPM] = 0; expect(&in, RING_NORMAL, STATUS_RING_WHITE);
    in.value[RING_RPM] = 2399; expect(&in, RING_NORMAL, 0);
    in.value[RING_RPM] = 2400; expect(&in, RING_NORMAL, STATUS_RING_GREEN);
    in.value[RING_RPM] = 3999; expect(&in, RING_NORMAL, STATUS_RING_GREEN);
    in.value[RING_RPM] = 4000; expect(&in, RING_APPROACH, STATUS_RING_GREEN);
    in.value[RING_RPM] = 4750; expect(&in, RING_APPROACH, 0xFFD166);
    in.value[RING_RPM] = 5499; expect(&in, RING_APPROACH, 0);
    in.value[RING_RPM] = 5500; expect(&in, RING_ALARM, STATUS_RING_RED);
    in.value[RING_RPM] = 10000; expect(&in, RING_ALARM, STATUS_RING_RED);
    in.value[RING_RPM] = 3000; in.age_ms[RING_RPM] = 5000; expect(&in, RING_NORMAL, STATUS_RING_GREEN);
    in.age_ms[RING_RPM] = 5001; in.valid[RING_CLT] = true; in.age_ms[RING_CLT] = 0;
    expect(&in, RING_LOST, STATUS_RING_LOST_COLOR);
    in.age_ms[RING_CLT] = 15001; expect(&in, RING_NO_DATA, STATUS_RING_SLEEP_COLOR);
    in.sleeping = true; expect(&in, RING_SLEEP, STATUS_RING_SLEEP_COLOR);
    in = input(); in.valid[RING_CLT] = true; in.age_ms[RING_CLT] = 0;
    in.alarm[RING_CLT] = 110; in.value[RING_CLT] = 111;
    assert(expect(&in, RING_ALARM, STATUS_RING_RED).item == RING_CLT);
    in.age_ms[RING_CLT] = 15001; expect(&in, RING_NORMAL, STATUS_RING_WHITE);
    in.age_ms[RING_CLT] = 0; in.alarm[RING_CLT] = INT16_MAX;
    expect(&in, RING_NORMAL, STATUS_RING_WHITE);
    in.alarm[RING_CLT] = 110; in.value[RING_CLT] = -10;
    expect(&in, RING_NORMAL, STATUS_RING_WHITE);
    in.valid[RING_OIL] = true; in.age_ms[RING_OIL] = 0;
    in.alarm[RING_OIL] = 125; in.value[RING_OIL] = 126;
    assert(expect(&in, RING_ALARM, STATUS_RING_RED).item == RING_OIL);
    in.sleeping = true; expect(&in, RING_SLEEP, STATUS_RING_SLEEP_COLOR);
    in.connected = false; expect(&in, RING_SLEEP, STATUS_RING_SLEEP_COLOR);
    in = input(); in.item_count = 3; in.items[1] = RING_OILP; in.items[2] = RING_BKT;
    expect(&in, RING_NORMAL, STATUS_RING_WHITE); // optional never-supported values
    in.age_ms[RING_OILP] = 6000; expect(&in, RING_LOST, STATUS_RING_LOST_COLOR);
    in = input(); in.items[0] = RING_CLT; in.items[1] = RING_IAT; in.item_count = 2;
    in.valid[RING_CLT] = in.valid[RING_IAT] = true;
    in.age_ms[RING_CLT] = in.age_ms[RING_IAT] = 0;
    in.alarm[RING_CLT] = 110; in.value[RING_CLT] = 105;
    in.alarm[RING_IAT] = 100; in.value[RING_IAT] = 99;
    assert(expect(&in, RING_APPROACH, 0).item == RING_IAT); // nearest enabled limit wins
    in.items[0] = RING_IAT; in.items[1] = RING_CLT;
    assert(expect(&in, RING_APPROACH, 0).item == RING_IAT); // independent of row order
    in = input(); in.items[0] = RING_SPEED; in.valid[RING_SPEED] = true;
    in.age_ms[RING_SPEED] = 0; in.value[RING_SPEED] = 100;
    expect(&in, RING_NORMAL, STATUS_RING_WHITE); // no invented road-speed limit
    in.alarm[RING_SPEED] = 100; in.value[RING_SPEED] = 95;
    expect(&in, RING_APPROACH, 0); in.value[RING_SPEED] = 100;
    expect(&in, RING_ALARM, STATUS_RING_RED);
    in = input(); in.page = RING_PAGE_G;
    expect(&in, RING_LOST, STATUS_RING_LOST_COLOR);
    in.g_valid = true; in.g_centi = 10; expect(&in, RING_NORMAL, STATUS_RING_WHITE);
    in.g_centi = 50; expect(&in, RING_NORMAL, 0);
    in.g_centi = 90; expect(&in, RING_APPROACH, 0);
    in.g_centi = 100; expect(&in, RING_ALARM, STATUS_RING_RED);
    in.g_age_ms = 501; expect(&in, RING_LOST, STATUS_RING_LOST_COLOR);
    in = input(); in.value[RING_RPM] = 5500; in.page = RING_PAGE_STATIC; in.item_count = 0;
    expect(&in, RING_NORMAL, STATUS_RING_WHITE); // settings/startup sweep
    in = input(); in.demo = true; in.connected = false; in.age_ms[RING_RPM] = UINT32_MAX;
    in.value[RING_RPM] = 3000; expect(&in, RING_NORMAL, STATUS_RING_GREEN);
    status_ring_result_t first = status_ring_evaluate(&in, &cfg);
    for (int i = 0; i < 100; ++i) assert(status_ring_evaluate(&in, &cfg).color == first.color);
    status_ring_config_t invalid = cfg; invalid.rpm_approach = invalid.rpm_green;
    assert(!status_ring_config_valid(&invalid));
    assert(status_ring_evaluate(&in, &invalid).color == first.color);
    invalid = cfg; invalid.brightness = 0; assert(!status_ring_config_valid(&invalid));
    invalid = cfg; invalid.g_warn_centi = 49; assert(!status_ring_config_valid(&invalid));
    printf("PASS: %u status scenarios, repeated colors stable, invalid config falls back safely\n", checks);
    return 0;
}
