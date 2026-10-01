#include "cx_power_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int groups;
static void zero_pair(cx_power_policy_t *p) {
    cx_policy_reset(p, 0); cx_policy_rpm(p, 0, 100); cx_policy_speed(p, 0, 200);
}
static const char *elm581 =
    "STSLCS\rCTRL MODE:  ELM327\rPWR_CTRL:   LOW PWR = LOW\r"
    "UART SLEEP: ON,  300 s\rUART WAKE:  ON,  0-0 us\r"
    "EXT INPUT:  LOW = SLEEP\rEXT SLEEP:  OFF, LOW FOR 64 ms\r"
    "EXT WAKE:   ON,  HIGH FOR 5000 ms\r"
    "VL SLEEP:   ON,  <13.00V FOR 10800 s\r"
    "VL WAKE:    OFF, >13.20V FOR 1 s\r"
    "VCHG WAKE:  ON,  0.20V IN 1000 ms\r\r>";
int main(void) {
    cx_power_policy_t p;
    cx_policy_reset(&p, 0);
    assert(!cx_policy_external_power(&p, false, 50000)); ++groups;
    cx_policy_rpm(&p, 2200, 100); cx_policy_speed(&p, 65, 200);
    assert(!cx_policy_tick(&p, 900000)); assert(p.state == CX_RUNNING); ++groups;
    cx_policy_rpm(&p, 800, 100); cx_policy_speed(&p, 0, 200);
    assert(!cx_policy_tick(&p, 900000)); ++groups;
    zero_pair(&p);
    assert(cx_policy_parking_candidate(&p));
    assert(!cx_policy_tick(&p, 60199)); assert(cx_policy_tick(&p, 60200));
    assert(p.state == CX_QUIET); assert(!cx_policy_external_power(&p, false, 70000)); ++groups;
    zero_pair(&p);
    for (unsigned t = 1000; t < 1000000; t += 1000) {
        cx_policy_rpm(&p, 0, t); cx_policy_speed(&p, 0, t);
        assert(!cx_policy_tick(&p, t));
    } ++groups; // auto start/stop: fresh zero responses keep the watch active
    zero_pair(&p); cx_policy_rpm(&p, 0, 25000);
    assert(!cx_policy_parking_candidate(&p)); assert(!cx_policy_tick(&p, 100000)); ++groups;
    zero_pair(&p); cx_policy_speed(&p, 1, 200);
    assert(!cx_policy_parking_candidate(&p)); assert(!cx_policy_tick(&p, 100000)); ++groups;
    zero_pair(&p); cx_policy_tick(&p, 60200);
    assert(!cx_policy_tick(&p, 390199)); assert(cx_policy_tick(&p, 390200));
    assert(p.state == CX_SLEEPING); ++groups;
    cx_policy_reset(&p, 0);
    assert(!cx_policy_tick(&p, 299999)); assert(cx_policy_tick(&p, 300000)); ++groups;
    const char *msg = "\r!LP ALERT\r";
    for (size_t split = 0; split <= strlen(msg); ++split) {
        cx_alert_stream_t stream = {0}; int count = 0;
        // Feed two BLE fragments through the exact same production parser.
        for (size_t i = 0; i < split; ++i) count += cx_alert_feed(&stream, msg[i]) == CX_ALERT_SLEEP;
        for (size_t i = split; i < strlen(msg); ++i) count += cx_alert_feed(&stream, msg[i]) == CX_ALERT_SLEEP;
        assert(count == 1);
    } ++groups;
    cx_alert_stream_t stream = {0}; int count = 0;
    msg = "LP ALERTX\rACT ALERT\r lp alert \r";
    int activity = 0;
    for (size_t i = 0; i < strlen(msg); ++i) {
        cx_alert_t a = cx_alert_feed(&stream, msg[i]);
        count += a == CX_ALERT_SLEEP; activity += a == CX_ALERT_ACTIVITY;
    }
    assert(count == 1 && activity == 1); ++groups;
    for (unsigned i = 0; i < 100; ++i) cx_alert_feed(&stream, 'X');
    assert(cx_alert_feed(&stream, '\r') == CX_ALERT_NONE);
    msg = "LP ALERT\r"; count = 0;
    for (size_t i = 0; i < strlen(msg); ++i) count += cx_alert_feed(&stream, msg[i]) == CX_ALERT_SLEEP;
    assert(count == 1); ++groups;
    zero_pair(&p); cx_policy_sleep_alert(&p);
    assert(!cx_policy_external_power(&p, true, 100000));
    assert(!cx_policy_external_power(&p, false, 100500)); assert(!cx_policy_external_power(&p, false, 103499));
    assert(cx_policy_external_power(&p, false, 103500)); ++groups;
    assert(!cx_policy_external_power(&p, true, 104000));
    assert(!cx_policy_external_power(&p, false, 105000)); assert(!cx_policy_external_power(&p, false, 107999));
    assert(cx_policy_external_power(&p, false, 108000)); ++groups;
    cx_policy_reset(&p, 108000);
    assert(p.state == CX_RUNNING && !p.have_rpm && !p.have_speed);
    assert(!cx_policy_external_power(&p, false, 200000)); ++groups;
    uint8_t v; bool on;
    assert(cx_parse_pp("00:FF F 0E:EA N\r0F:B5 N\r>", 0x0e, &v, &on) && v == 0xea && on);
    assert(cx_parse_pp("0E:5A F", 0x0e, &v, &on) && v == 0x5a && !on);
    assert(!cx_parse_pp("0E:?? N", 0x0e, &v, &on)); ++groups;
    assert(cx_config_verified("CTRL MODE: ELM327\rUART SLEEP: ON, 300 s\rUART WAKE: ON, 0-0 us\rEXT SLEEP: OFF, LOW FOR 64 ms\rPA SLEEP: ON, 0x01 FOR 150 s\r>"));
    assert(!cx_config_verified("CTRL MODE: NATIVE\rUART SLEEP: ON, 300 s\rUART WAKE: ON, 0-0 us\rEXT SLEEP: OFF, LOW FOR 64 ms\rPA SLEEP: ON, 0x01 FOR 150 s\r>"));
    assert(!cx_config_verified("CTRL MODE: ELM327\rUART SLEEP: ON, 1200 s\rUART WAKE: ON, 0-0 us\rEXT SLEEP: OFF, LOW FOR 64 ms\rPA SLEEP: ON, 0x01 FOR 150 s\r>")); ++groups;
    // Actual successful phone configuration: PA fields are absent in v5.8.1.
    assert(cx_config_verified(elm581));
    assert(!cx_config_verified("CTRL MODE: ELM327\rUART SLEEP: ON, 300 s\rEXT SLEEP: OFF, LOW FOR 64 ms\r>"));
    assert(!cx_config_verified("CTRL MODE: ELM327\rUART SLEEP: ON, 300 s\rUART WAKE: OFF, 0-0 us\rEXT SLEEP: OFF, LOW FOR 64 ms\r>"));
    assert(!cx_config_verified("CTRL MODE: ELM327\rUART SLEEP: ON, 300 s\rUART WAKE: ON, 0-0 us\rEXT SLEEP: ON, LOW FOR 64 ms\r>"));
    assert(!cx_config_verified("CTRL MODE: ELM327\rUART SLEEP: ON, 300 s\rUART WAKE: ON, 0-0 us\rEXT SLEEP: OFF, LOW FOR 64 ms\rPA SLEEP: OFF, 0x01 FOR 150 s\r>"));
    assert(!cx_config_verified("CTRL MODE: ELM327\rUART SLEEP: ON, 300 s\rUART WAKE: ON, 0-0 us\rEXT SLEEP: OFF, LOW FOR 64 ms\rPA SLEEP: ON, 0x01 FOR 30 s\r>")); ++groups;
    assert(cx_pp_link_verified(0xea, 0xb5, true, true));
    assert(cx_pp_link_verified(0xa8, 0xb5, true, true));
    assert(cx_pp_link_verified(0xaa, 0xb5, true, true));
    assert(cx_pp_link_verified(0xe8, 0xb5, true, true));
    assert(!cx_pp_link_verified(0xea, 0xb5, false, true));
    assert(!cx_pp_link_verified(0xea, 0xb5, true, false));
    for (unsigned bit = 0; bit < 8; ++bit) {
        if (bit != 1 && bit != 6)
            assert(!cx_pp_link_verified(0xea ^ (1u << bit), 0xb5, true, true));
        assert(!cx_pp_link_verified(0xea, 0xb5 ^ (1u << bit), true, true));
    } ++groups;
    // Exact STSLCS text from the user's STN2310 v5.8.1 capture. Its voltage
    // comparison used to terminate the command before the real prompt.
    const char *native =
        "STSLCS\rCTRL MODE:  NATIVE\rPWR_CTRL:   LOW PWR = LOW\r"
        "UART SLEEP: ON,  600 s\rUART WAKE:  ON,  0-0 us\r"
        "EXT INPUT:  LOW = SLEEP\rEXT SLEEP:  OFF, LOW FOR 3000 ms\r"
        "EXT WAKE:   ON,  HIGH FOR 2000 ms\r"
        "VL SLEEP:   ON,  <13.00V FOR 10800 s\r"
        "VL WAKE:    OFF, >13.20V FOR 1 s\r"
        "VCHG WAKE:  ON,  0.20V IN 1000 ms\r\r>";
    assert(strchr(native, '>') < native + strlen(native) - 1);
    for (size_t split = 0; split <= strlen(native); ++split) {
        cx_response_stream_t response;
        cx_response_reset(&response);
        for (size_t i = 0; i < split; ++i)
            assert(cx_response_feed(&response, (uint8_t)native[i]) == (i == strlen(native) - 1));
        for (size_t i = split; i < strlen(native); ++i)
            assert(cx_response_feed(&response, (uint8_t)native[i]) == (i == strlen(native) - 1));
        assert(response.done && !response.overflow);
        assert(strcmp(response.text, native) == 0);
        assert(strstr(response.text, "VCHG WAKE:"));
    } ++groups;
    // The configured ELM response must also survive every BLE split, including
    // the inline '>' and the final prompt, before enabling the standby policy.
    for (size_t split = 0; split <= strlen(elm581); ++split) {
        cx_response_stream_t elm;
        cx_response_reset(&elm);
        for (size_t i = 0; i < split; ++i) cx_response_feed(&elm, (uint8_t)elm581[i]);
        for (size_t i = split; i < strlen(elm581); ++i) cx_response_feed(&elm, (uint8_t)elm581[i]);
        assert(elm.done && !elm.overflow && strcmp(elm.text, elm581) == 0);
        assert(cx_config_verified(elm.text));
    } ++groups;
    cx_response_stream_t response;
    cx_response_reset(&response);
    msg = "VL WAKE: OFF, >13.20V FOR 1 s\r\nPA SLEEP: ON, 0x01 FOR 150 s\r\n \t>";
    for (size_t i = 0; i < strlen(msg); ++i)
        assert(cx_response_feed(&response, (uint8_t)msg[i]) == (i == strlen(msg) - 1));
    assert(response.done && !response.overflow && strcmp(response.text, msg) == 0);
    assert(!cx_response_feed(&response, 'X') && strcmp(response.text, msg) == 0); ++groups;
    cx_response_reset(&response);
    for (size_t i = 0; i < 1100; ++i) assert(!cx_response_feed(&response, 'X'));
    assert(response.overflow && !response.done && response.len == sizeof(response.text) - 1);
    assert(!cx_response_feed(&response, '\r'));
    assert(cx_response_feed(&response, '>') && response.overflow); ++groups;
    cx_response_reset(&response);
    msg = "ATPP0EON\rOK\r\r>";
    for (size_t i = 0; i < strlen(msg); ++i) cx_response_feed(&response, (uint8_t)msg[i]);
    assert(response.done && !response.overflow && strcmp(response.text, msg) == 0); ++groups;
    printf("PASS: %d behavior groups, fragmented alerts at every boundary, 999 fresh-zero cycles\n", groups);
    puts("PASS: STSLCS voltage comparisons at every fragment boundary, real prompt, overflow and reset");
    puts("PASS: actual v5.8.1 ELM configuration without PA fields; disabled/wrong PP and wake settings rejected");
    return 0;
}
