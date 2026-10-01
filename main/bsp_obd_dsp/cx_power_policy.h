#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { CX_RUNNING, CX_QUIET, CX_SLEEPING } cx_power_state_t;
typedef struct {
    cx_power_state_t state;
    uint64_t started_ms, rpm_ms, speed_ms, ecu_ms, quiet_ms, power_off_ms;
    uint16_t rpm;
    uint8_t speed;
    bool have_rpm, have_speed, power_off_seen;
} cx_power_policy_t;
typedef struct { char line[64]; size_t len; bool overflow; } cx_alert_stream_t;
typedef enum { CX_ALERT_NONE, CX_ALERT_ACTIVITY, CX_ALERT_SLEEP } cx_alert_t;

// A command prompt is a standalone '>' at the start of a response line.
// STSLCS also prints '>' inside voltage conditions; those are response data.
typedef struct {
    char text[1024];
    size_t len;
    bool done, overflow, line_empty;
} cx_response_stream_t;

void cx_policy_reset(cx_power_policy_t *p, uint64_t now);
void cx_policy_rpm(cx_power_policy_t *p, uint16_t rpm, uint64_t now);
void cx_policy_speed(cx_power_policy_t *p, uint8_t speed, uint64_t now);
bool cx_policy_parking_candidate(const cx_power_policy_t *p);
bool cx_policy_tick(cx_power_policy_t *p, uint64_t now);
void cx_policy_sleep_alert(cx_power_policy_t *p);
// Either USB or rear 5V keeps the watch awake. Caller passes unknown as present.
bool cx_policy_external_power(cx_power_policy_t *p, bool present, uint64_t now);
cx_alert_t cx_alert_feed(cx_alert_stream_t *s, uint8_t ch);
void cx_response_reset(cx_response_stream_t *s);
bool cx_response_feed(cx_response_stream_t *s, uint8_t ch);
bool cx_parse_pp(const char *text, uint8_t number, uint8_t *value, bool *on);
bool cx_config_verified(const char *text);
bool cx_pp_link_verified(uint8_t e, uint8_t f, bool e_on, bool f_on);
