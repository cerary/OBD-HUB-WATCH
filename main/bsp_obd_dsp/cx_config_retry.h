#pragma once
#include <stdbool.h>
#include <stdint.h>

#define CX_CONFIG_MAX_ATTEMPTS 3u
typedef enum { CX_CONFIG_SUCCESS, CX_CONFIG_RETRYABLE, CX_CONFIG_UNSUPPORTED } cx_config_result_t;
typedef struct {
    uint64_t next_ms;
    unsigned attempts;
    bool finished;
} cx_config_retry_t;

static inline void cx_config_retry_reset(cx_config_retry_t *r) {
    *r = (cx_config_retry_t){0};
}
static inline bool cx_config_retry_due(const cx_config_retry_t *r, uint64_t now, bool eligible) {
    return eligible && !r->finished && r->attempts < CX_CONFIG_MAX_ATTEMPTS && now >= r->next_ms;
}
static inline void cx_config_retry_record(cx_config_retry_t *r, cx_config_result_t result, uint64_t now) {
    if (r->attempts < CX_CONFIG_MAX_ATTEMPTS) ++r->attempts;
    r->finished = result != CX_CONFIG_RETRYABLE || r->attempts >= CX_CONFIG_MAX_ATTEMPTS;
    r->next_ms = r->finished ? 0 : now + (r->attempts == 1 ? 5000u : 15000u);
}
