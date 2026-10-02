#include "cx_power_policy.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

void cx_policy_reset(cx_power_policy_t *p, uint64_t now) {
    memset(p, 0, sizeof(*p));
    p->started_ms = now;
}
void cx_policy_rpm(cx_power_policy_t *p, uint16_t rpm, uint64_t now) {
    p->rpm = rpm; p->rpm_ms = p->ecu_ms = now; p->have_rpm = true;
    p->rpm_no_data = p->speed_no_data = 0;
}
void cx_policy_speed(cx_power_policy_t *p, uint8_t speed, uint64_t now) {
    p->speed = speed; p->speed_ms = p->ecu_ms = now; p->have_speed = true;
    p->rpm_no_data = p->speed_no_data = 0;
}
void cx_policy_no_data(cx_power_policy_t *p, bool rpm) {
    uint8_t *count = rpm ? &p->rpm_no_data : &p->speed_no_data;
    if (*count < 2) ++*count;
}
void cx_policy_ecu_alive(cx_power_policy_t *p, uint64_t now) {
    p->ecu_ms = now;
    p->rpm_no_data = p->speed_no_data = 0;
}
bool cx_policy_parking_candidate(const cx_power_policy_t *p) {
    // Some ECUs disappear before delivering their final zero RPM sample.
    // Keep the genuine nonzero sample. Recently paired stationary/idle
    // readings and two explicit empty replies for BOTH PIDs are required.
    bool paired = p->ecu_ms - p->rpm_ms <= 10000 && p->ecu_ms - p->speed_ms <= 10000;
    bool missing_final_zero = p->rpm <= 1500 && p->rpm_no_data >= 2 && p->speed_no_data >= 2;
    return p->have_rpm && p->have_speed && p->speed == 0 && paired &&
           (p->rpm == 0 || missing_final_zero);
}
bool cx_policy_tick(cx_power_policy_t *p, uint64_t now) {
    if (p->state == CX_RUNNING &&
        ((cx_policy_parking_candidate(p) && now - p->ecu_ms >= 60000) ||
         (!p->have_rpm && !p->have_speed && now - p->started_ms >= 300000 &&
          now - p->ecu_ms >= 60000))) {
        p->state = CX_QUIET; p->quiet_ms = now;
        return true;
    }
    // If BLE disappeared before LP ALERT, never reconnect to fetch the alert.
    // The configured UART idle timeout is 300 s; allow a further 30 s margin.
    if (p->state == CX_QUIET && now - p->quiet_ms >= 330000) {
        p->state = CX_SLEEPING;
        return true;
    }
    return false;
}
bool cx_policy_activity_alert(cx_power_policy_t *p) {
    if (p->state != CX_QUIET || p->sleep_warning) return false;
    p->sleep_warning = true;
    return true;
}
void cx_policy_sleep_alert(cx_power_policy_t *p) {
    p->state = CX_SLEEPING;
    p->sleep_confirmed = true;
}
bool cx_policy_external_power(cx_power_policy_t *p, bool present, uint64_t now) {
    if (p->state == CX_SLEEPING && p->sleep_confirmed) return true;
    if (present || p->state != CX_SLEEPING) {
        p->power_off_seen = false;
        return false;
    }
    if (!p->power_off_seen) { p->power_off_ms = now; p->power_off_seen = true; }
    return now - p->power_off_ms >= 3000;
}
cx_alert_t cx_alert_feed(cx_alert_stream_t *s, uint8_t ch) {
    if (ch == '\r' || ch == '\n' || ch == '>') {
        s->line[s->len] = 0;
        char *line = s->line;
        while (*line == ' ' || *line == '!') ++line;
        while (s->len && s->line[s->len - 1] == ' ') s->line[--s->len] = 0;
        cx_alert_t alert = CX_ALERT_NONE;
        if (!s->overflow && strcmp(line, "LP ALERT") == 0) alert = CX_ALERT_SLEEP;
        if (!s->overflow && strcmp(line, "ACT ALERT") == 0) alert = CX_ALERT_ACTIVITY;
        s->len = 0; s->overflow = false;
        return alert;
    }
    if (ch >= 0x20 && ch <= 0x7e) {
        if (s->len + 1 < sizeof(s->line)) s->line[s->len++] = (char)toupper(ch);
        else s->overflow = true;
    }
    return CX_ALERT_NONE;
}
void cx_response_reset(cx_response_stream_t *s) {
    memset(s, 0, sizeof(*s));
    s->line_empty = true;
}
bool cx_response_feed(cx_response_stream_t *s, uint8_t ch) {
    if (s->done) return false;
    bool prompt = ch == '>' && s->line_empty;
    if (s->len + 1 < sizeof(s->text)) s->text[s->len++] = (char)ch;
    else s->overflow = true;
    s->text[s->len] = 0;
    if (ch == '\r' || ch == '\n') s->line_empty = true;
    else if (ch != ' ' && ch != '\t') s->line_empty = false;
    s->done = prompt;
    return prompt;
}
bool cx_parse_pp(const char *text, uint8_t number, uint8_t *value, bool *on) {
    char key[4]; snprintf(key, sizeof(key), "%02X:", number);
    const char *p = strstr(text, key);
    unsigned v = 0; char enabled = 0;
    if (!p || sscanf(p + 3, "%2x %c", &v, &enabled) != 2 ||
        (enabled != 'N' && enabled != 'F')) return false;
    *value = (uint8_t)v; *on = enabled == 'N';
    return true;
}
bool cx_config_verified(const char *text) {
    char compact[1024]; size_t n = 0;
    for (; *text && n + 1 < sizeof(compact); ++text)
        if (!isspace((unsigned char)*text)) compact[n++] = (char)toupper((unsigned char)*text);
    compact[n] = 0;
    // STN2310 v5.8.1 omits PA SLEEP/WAKE even in ELM327 mode. The verified
    // UART timeout remains the fallback; ATPPS must independently confirm the
    // alert and sleep bits. When a PA status is available, verify it too.
    return strstr(compact, "CTRLMODE:ELM327") &&
           strstr(compact, "UARTSLEEP:ON,300S") &&
           strstr(compact, "UARTWAKE:ON,") &&
           strstr(compact, "EXTSLEEP:OFF,") &&
           (!strstr(compact, "PASLEEP:") ||
            strstr(compact, "PASLEEP:ON,0X01FOR150S"));
}
bool cx_pp_link_verified(uint8_t e, uint8_t f, bool e_on, bool f_on) {
    // Preserve PP0E polarity / external wake delay (bits 6 and 1). Require
    // UART sleep at 300 s and LP alerts, with external sleep disabled.
    return e_on && f_on && (e & 0xBD) == 0xA8 && f == 0xB5;
}
