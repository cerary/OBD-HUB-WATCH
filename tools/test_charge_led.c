#include "stopwatch/charge_led_policy.h"
#include <assert.h>
#include <stdio.h>

static void active_until(charge_led_policy_t *state, uint64_t start, uint64_t end)
{
    for (uint64_t now = start; now < end; now += 500)
        assert(!charge_led_policy_update(state, true, true, true, now));
}

int main(void)
{
    charge_led_policy_t state = {0};
    // Initial continuous charging lights at 5 seconds, never earlier.
    active_until(&state, 0, 5000);
    assert(!charge_led_policy_update(&state, true, true, true, 4999));
    assert(charge_led_policy_update(&state, true, true, true, 5000));
    assert(charge_led_policy_update(&state, true, true, true, 5500));
    assert(!charge_led_policy_update(&state, true, true, false, 6000));

    // Repeated 2-second pulses from interaction never accumulate into ON.
    for (uint64_t start = 6500; start < 20000; start += 2500) {
        active_until(&state, start, start + 2000);
        assert(!charge_led_policy_update(&state, true, true, false, start + 2000));
    }
    active_until(&state, 22000, 27000);
    assert(charge_led_policy_update(&state, true, true, true, 27000));

    // Battery-only operation cannot light even if the raw charger pin is low.
    assert(!charge_led_policy_update(&state, true, false, true, 27500));
    for (uint64_t now = 28000; now < 40000; now += 500)
        assert(!charge_led_policy_update(&state, true, false, true, now));
    active_until(&state, 40000, 45000);
    assert(charge_led_policy_update(&state, true, true, true, 45000));

    // Unknown status clears a previously lit LED and restarts confirmation.
    assert(!charge_led_policy_update(&state, false, true, true, 45500));
    active_until(&state, 46000, 51000);
    assert(charge_led_policy_update(&state, true, true, true, 51000));

    // An unsampled interval or clock rollback cannot qualify stale evidence.
    assert(!charge_led_policy_update(&state, true, true, true, 53001));
    active_until(&state, 53501, 58001);
    assert(charge_led_policy_update(&state, true, true, true, 58001));
    assert(!charge_led_policy_update(&state, true, true, true, 100));
    active_until(&state, 600, 5100);
    assert(charge_led_policy_update(&state, true, true, true, 5100));
    puts("PASS: sustained charging, short pulses, no accumulation, supply loss, unknown input, stale samples and clock rollback");
    return 0;
}
