#include "app_obd_dsp/peak_marker_policy.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
int main(void)
{
    peak_marker_state_t rpm = {0}, speed = {0};
    peak_marker_sample(&rpm, 5500, true, 100, 5000);
    assert(rpm.peak == 5500 && !peak_marker_opacity(&rpm, 100, 5000));
    peak_marker_sample(&rpm, 3000, true, 200, 5000);
    assert(peak_marker_opacity(&rpm, 1200, 5000) == 255);
    assert(peak_marker_opacity(&rpm, 3200, 5000) == 127);
    peak_marker_sample(&rpm, 4750, true, 3400, 5000);
    peak_marker_sample(&rpm, 5500, true, 3500, 5000);
    assert(rpm.fall_tick == 200 && peak_marker_opacity(&rpm, 5200, 5000) == 0);
    peak_marker_sample(&rpm, 3000, true, 5200, 5000);
    assert(rpm.peak == 3000 && !rpm.falling);
    peak_marker_sample(&rpm, 4750, true, 5300, 5000);
    peak_marker_sample(&rpm, 3000, true, 5400, 5000);
    assert(rpm.peak == 4750 && peak_marker_opacity(&rpm, 5400, 5000) == 255);
    peak_marker_sample(&rpm, 5500, true, 5500, 5000);
    assert(rpm.peak == 5500 && !rpm.falling);
    peak_marker_sample(&rpm, 3000, true, 5600, 5000);
    assert(rpm.fall_tick == 5600);
    peak_marker_sample(&speed, 100, true, 5600, 5000);
    peak_marker_sample(&speed, 30, true, 5700, 5000);
    assert(speed.peak == 100 && rpm.peak == 5500);
    peak_marker_sample(&rpm, 8000, false, 6000, 5000);
    assert(!rpm.armed && speed.armed);
    peak_marker_sample(&rpm, 0, true, 6100, 5000);
    assert(rpm.armed && rpm.peak == 0 && !rpm.falling);
    peak_marker_sample(&rpm, 10, true, UINT32_MAX - 200, 5000);
    peak_marker_sample(&rpm, 5, true, UINT32_MAX - 100, 5000);
    assert(peak_marker_opacity(&rpm, 2899, 5000) == 127);
    assert(!peak_marker_opacity(&rpm, 4899, 5000));
    peak_marker_sample(&rpm, 100, true, 5000, 10000);
    peak_marker_sample(&rpm, 30, true, 5100, 10000);
    assert(peak_marker_opacity(&rpm, 7100, 10000) == 255);
    assert(peak_marker_opacity(&rpm, 11100, 10000) == 127);
    assert(!peak_marker_opacity(&rpm, 15100, 10000));
    puts("PASS: peak hold/fade, equality/lower unchanged timer, expiry rebasing, new maximum, independent channels, invalid/zero and clock wrap");
    return 0;
}
