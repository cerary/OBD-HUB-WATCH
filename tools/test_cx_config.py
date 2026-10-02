"""Replay temporary CX failures through actual production configuration bodies.

GATT transport/time/NVS are mocked; configuration validation, arming and retry
accounting are production code. This is not a physical adapter test.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / '.cache/host-tests'
BUILD.mkdir(parents=True, exist_ok=True)
source = (ROOT/'main/bsp_obd_dsp/elm327_ble_client.c').read_text()

def function(marker):
    start = source.index(marker)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

prefix = r'''
#include "cx_power_policy.h"
#include "cx_config_retry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define TAG "cx-test"
#define ESP_LOGI(tag, fmt, ...) ((void)(tag), printf(fmt "\n", ##__VA_ARGS__))
#define ESP_LOGW(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
static int s_cx_lock;
static bool s_cx_enabled, s_cx_armed, s_cx_session_started, s_obdlink_cx_uart;
static bool s_connected, s_notify_ready, s_cx_config_attempted_this_link, s_cx_radio_paused;
static const char *s_cx_status;
static cx_config_retry_t s_cx_config_retry;
static cx_power_policy_t s_cx_policy;
static uint64_t clock_ms;
static unsigned fail_sti, sti_requests, pp_writes;
static bool unsupported, disconnect_sti;
static uint64_t cx_now_ms(void) {return clock_ms;}
typedef struct {uint8_t version,e,f,e_on,f_on;} cx_pp_backup_t;
static bool cx_backup(cx_pp_backup_t *b, bool write) {
    assert(!write); *b=(cx_pp_backup_t){1,0xea,0xb5,1,1}; return true;
}
static bool cx_write_pp(uint8_t e,uint8_t f,bool en,bool fn) {
    (void)e;(void)f;(void)en;(void)fn; ++pp_writes; return false;
}
static bool cx_query(const char *cmd,char *reply,size_t size) {
    if (!strcmp(cmd,"STI\r")) {
        ++sti_requests;
        if (disconnect_sti) {s_connected=false; return false;}
        if (fail_sti) {--fail_sti;return false;}
        snprintf(reply,size,"%s\r\r>",unsupported ? "ELM327 v1.4b" : "STN2310 v5.8.1");
    } else if (!strcmp(cmd,"STSLCS\r")) {
        snprintf(reply,size,"CTRL MODE: ELM327\rUART SLEEP: ON, 300 s\rUART WAKE: ON, 0-0 us\rEXT SLEEP: OFF, LOW FOR 64 ms\r\r>");
    } else if (!strcmp(cmd,"ATPPS\r")) {
        snprintf(reply,size,"0E:EA N  0F:B5 N\r\r>");
    } else {
        assert(!strcmp(cmd,"STSLLT\r")); snprintf(reply,size,"SLEEP: NONE\rWAKE: NONE\r>");
    }
    return true;
}
'''
suffix = r'''
static void reset(void) {
    s_cx_enabled=s_obdlink_cx_uart=s_connected=s_notify_ready=s_cx_config_attempted_this_link=true;
    s_cx_armed=s_cx_session_started=s_cx_radio_paused=unsupported=disconnect_sti=false;
    fail_sti=sti_requests=pp_writes=0;clock_ms=0;
    cx_config_retry_reset(&s_cx_config_retry);cx_policy_reset(&s_cx_policy,0);
}
int main(void) {
    reset();fail_sti=1;cx_configure_attempt(2);
    assert(!s_cx_armed && s_cx_config_retry.attempts==1 && !s_cx_config_retry.finished);
    assert(s_connected && s_cx_config_attempted_this_link);
    clock_ms=4999;assert(!cx_config_retry_due(&s_cx_config_retry,clock_ms,true));
    clock_ms=5000;assert(cx_config_retry_due(&s_cx_config_retry,clock_ms,true));
    cx_configure_attempt(2);
    assert(s_cx_armed && s_cx_config_retry.finished && sti_requests==2 && pp_writes==0);
    assert(!strcmp(s_cx_status,"Ready - CX sleep linked"));
    reset();fail_sti=10;
    for(unsigned i=0;i<3;++i) {
        clock_ms=i==0 ? 0 : i==1 ? 5000 : 20000;
        assert(cx_config_retry_due(&s_cx_config_retry,clock_ms,true));cx_configure_attempt(2);
        assert(!s_cx_armed && pp_writes==0);
    }
    assert(sti_requests==3 && s_cx_config_retry.finished);
    assert(!cx_config_retry_due(&s_cx_config_retry,999999,true));
    reset();s_obdlink_cx_uart=false;unsupported=true;cx_configure_attempt(2);
    assert(!s_cx_armed && s_cx_config_retry.finished && sti_requests==1);
    reset();cx_configure_attempt(3);
    assert(s_cx_armed && s_cx_config_retry.finished && pp_writes==0);
    reset();disconnect_sti=true;cx_configure_attempt(2);
    assert(!s_cx_armed && !s_connected && s_cx_config_retry.attempts==0);
    reset();s_cx_enabled=false;cx_configure_attempt(1);
    assert(!s_cx_armed && s_cx_config_retry.finished);
    puts("PASS production CX configuration: failed first STI -> same-link Ready, three-attempt cap, unsupported adapter, read-only arming, disconnect budget and disabled restore");
}
'''
code = prefix + '\n'.join(function(marker) for marker in (
    'static void cx_arm_verified(',
    'static cx_config_result_t cx_configure(',
    'static void cx_configure_attempt(',
)) + suffix
path = BUILD/'cx_config_replay.c'
path.write_text(code)
exe = BUILD/'cx_config_replay'
subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',
                '-I'+str(ROOT/'main/bsp_obd_dsp'),str(path),
                str(ROOT/'main/bsp_obd_dsp/cx_power_policy.c'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
