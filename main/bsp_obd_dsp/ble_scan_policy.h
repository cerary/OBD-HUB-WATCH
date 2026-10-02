#pragma once
#include <stdbool.h>

// GAP requests complete asynchronously. Remember a stop requested before the
// start callback, and defer a restart until the outstanding stop completes.
typedef struct {
    bool active, starting, stopping, stop_after_start;
    unsigned restart_seconds;
} ble_scan_policy_t;
typedef enum { BLE_SCAN_NONE, BLE_SCAN_START, BLE_SCAN_STOP } ble_scan_action_t;

static inline ble_scan_action_t ble_scan_start_request(ble_scan_policy_t *p, unsigned seconds) {
    if (p->stopping) { p->restart_seconds = seconds; return BLE_SCAN_NONE; }
    if (p->starting && p->stop_after_start) { p->restart_seconds = seconds; return BLE_SCAN_NONE; }
    if (p->active || p->starting) return BLE_SCAN_NONE;
    p->starting = true; p->stop_after_start = false;
    return BLE_SCAN_START;
}
static inline ble_scan_action_t ble_scan_stop_request(ble_scan_policy_t *p) {
    p->restart_seconds = 0;
    if (p->starting) { p->stop_after_start = true; return BLE_SCAN_NONE; }
    if (!p->active || p->stopping) return BLE_SCAN_NONE;
    p->stopping = true;
    return BLE_SCAN_STOP;
}
static inline ble_scan_action_t ble_scan_started(ble_scan_policy_t *p, bool success, bool paused) {
    p->starting = false; p->active = success;
    bool stop = p->stop_after_start || paused;
    p->stop_after_start = false;
    unsigned restart = paused ? 0 : p->restart_seconds;
    ble_scan_action_t action = stop ? ble_scan_stop_request(p) : BLE_SCAN_NONE;
    p->restart_seconds = restart;
    return action;
}
static inline unsigned ble_scan_stopped(ble_scan_policy_t *p, bool success) {
    p->stopping = false;
    if (success) p->active = false;
    unsigned next = success ? p->restart_seconds : 0;
    p->restart_seconds = 0;
    return next;
}
static inline void ble_scan_expired(ble_scan_policy_t *p) {
    p->active = false; p->starting = false; p->stop_after_start = false;
    // A stop completion can still be queued after the scan duration expires.
    // Keep its pending state so a new scan waits for that callback.
}
