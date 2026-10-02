#include "../main/app_obd_dsp/temperature_alert_policy.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static temperature_alert_policy_t p;
static int32_t values[3] = {100, 40, 110};
static uint32_t ages[3];
static bool valid[3] = {true, true, true};
static int16_t limits[3] = {120, 80, 135};
static void tick(uint32_t now) { temperature_alert_update(&p, now, true, values, ages, valid, limits); }

int main(void)
{
    temperature_limit_t clt = temperature_limit(TEMP_CLT, 120);
    assert(clt.enabled && clt.warm_c == 115 && clt.hot_hold_ms == 3000 && clt.hysteresis_c == 3);
    assert(temperature_limit(TEMP_IAT, 80).warm_c == 60);
    assert(temperature_limit(TEMP_OIL, 135).warm_c == 125);
    assert(!temperature_limit(TEMP_CLT, INT16_MAX).enabled);
    assert(!temperature_limit(3, 120).enabled);
    tick(0); assert(p.level[0] == TEMP_NORMAL);

    values[0] = 115; tick(100);
    tick(10099); assert(p.level[0] == TEMP_NORMAL);
    tick(10100); assert(p.level[0] == TEMP_WARM);
    values[0] = 114; tick(10200); assert(p.level[0] == TEMP_WARM);
    values[0] = 112; tick(10300); assert(p.level[0] == TEMP_NORMAL);
    values[0] = 120; tick(11000); tick(13999); assert(p.level[0] == TEMP_NORMAL);
    tick(14000); assert(p.level[0] == TEMP_HOT);
    values[0] = 118; tick(14100); assert(p.level[0] == TEMP_HOT);
    values[0] = 117; tick(14200); assert(p.level[0] == TEMP_WARM);
    values[0] = 112; tick(14300); assert(p.level[0] == TEMP_NORMAL);

    // One cached high sample cannot trigger either duration timer.
    temperature_alert_reset(&p); values[0] = 120; tick(20000);
    ages[0] = 10000; tick(30000); assert(p.level[0] == TEMP_NORMAL);
    ages[0] = 15001; tick(35001); assert(p.level[0] == TEMP_NORMAL);
    ages[0] = 0; tick(36000); tick(38999); assert(p.level[0] == TEMP_NORMAL);
    tick(39000); assert(p.level[0] == TEMP_HOT);
    valid[0] = false; tick(39001); assert(p.level[0] == TEMP_NORMAL);
    valid[0] = true; tick(40000); tick(43000); assert(p.level[0] == TEMP_HOT);
    temperature_alert_update(&p, 43001, false, values, ages, valid, limits);
    assert(p.level[0] == TEMP_NORMAL && !p.channel[0].configured);
    tick(50000); tick(52999); assert(p.level[0] == TEMP_NORMAL);
    tick(53000); assert(p.level[0] == TEMP_HOT);
    limits[0] = INT16_MAX; tick(53001); assert(p.level[0] == TEMP_NORMAL);
    limits[0] = 125; tick(53002); assert(p.level[0] == TEMP_NORMAL);
    tick(63002); assert(p.level[0] == TEMP_WARM); // changed limit restarts timer

    // Below-threshold breaks do not accumulate across separate excursions.
    temperature_alert_reset(&p); limits[0] = 120; values[0] = 115; tick(70000);
    tick(76000); values[0] = 114; tick(76001); values[0] = 115; tick(76002);
    tick(82002); assert(p.level[0] == TEMP_NORMAL);
    tick(86002); assert(p.level[0] == TEMP_WARM);

    // Independent oil/IAT hold and recovery times.
    temperature_alert_reset(&p); values[0] = 100; values[1] = 60; values[2] = 125;
    tick(100000); tick(110000); assert(p.level[2] == TEMP_WARM && p.level[1] == TEMP_NORMAL);
    tick(129999); assert(p.level[1] == TEMP_NORMAL);
    tick(130000); assert(p.level[1] == TEMP_WARM);
    values[1] = 80; values[2] = 135; tick(131000);
    tick(134000); assert(p.level[2] == TEMP_HOT && p.level[1] == TEMP_WARM);
    tick(141000); assert(p.level[1] == TEMP_HOT);
    values[1] = 75; values[2] = 130; tick(141001);
    assert(p.level[1] == TEMP_WARM && p.level[2] == TEMP_WARM);
    values[1] = 55; values[2] = 120; tick(141002);
    assert(p.level[1] == TEMP_NORMAL && p.level[2] == TEMP_NORMAL);

    temperature_alert_reset(&p); values[0] = 120;
    tick(UINT32_MAX - 1999U); tick(999); assert(p.level[0] == TEMP_NORMAL);
    tick(1000); assert(p.level[0] == TEMP_HOT); // uint32 clock wrap
    valid[2] = false; values[2] = 150; tick(1001); assert(p.level[2] == TEMP_NORMAL);
    puts("PASS: temperature two-stage durations, new-sample confirmation, hysteresis, HOT-to-WARM, independent channels, expired/invalid/OFF/disconnect reset, config change and clock wrap");
    return 0;
}
