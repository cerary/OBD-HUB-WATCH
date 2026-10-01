"""Run repository LVGL, ring adapter, cache and settings pages on a RAM display.
Hardware transport/time/NVS are replaced; UI and color policy are actual sources.
Run under Linux / WSL python3. LVGL and production UI sources compile from scratch.
"""
from pathlib import Path
import concurrent.futures
import hashlib
import json
import re
import subprocess

import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=Path('docs/images'))
args=parser.parse_args()
REPO=Path(__file__).resolve().parents[2]
OUT=args.output.resolve();OUT.mkdir(parents=True,exist_ok=True)
ROOT=REPO
MAIN = REPO / 'main'
UI = MAIN / 'export_path'
LV = REPO / 'managed_components/lvgl__lvgl'
BUILD = REPO / '.cache/ui-preview'
BUILD.mkdir(parents=True,exist_ok=True)

def write(path, text):
    path = BUILD / path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)

config = []
for line in (REPO / 'sdkconfig.stopwatch').read_text().splitlines():
    m = re.match(r'(CONFIG_(?:LV_|OBD_HW_)[A-Z0-9_]+)=(.*)', line)
    if m:
        name, value = m.groups()
        config.append(f"#define {name} {'1' if value == 'y' else value}")
write('sdkconfig.h', '\n'.join(config) + '\n')
write('esp_err.h', '#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n')
write('esp_log.h', '#pragma once\n#define ESP_LOGI(...) ((void)0)\n#define ESP_LOGW(...) ((void)0)\n')
write('esp_heap_caps.h', '#pragma once\n#include <stddef.h>\n#undef heap_caps_malloc\n#define heap_caps_free free\n#ifdef HOST_G_FORCE\nvoid *host_g_alloc(size_t size);\n#define heap_caps_malloc(size, flags) host_g_alloc(size)\n#else\nvoid *host_power_alloc(size_t size);\n#define heap_caps_malloc(size, flags) host_power_alloc(size)\n#endif\n')
write('esp_system.h', '#pragma once\nvoid esp_restart(void);\n')
write('esp_timer.h', '''#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef void *esp_timer_handle_t;
typedef struct {void (*callback)(void*); void *arg; const char *name;} esp_timer_create_args_t;
int64_t esp_timer_get_time(void);
esp_err_t esp_timer_create(const esp_timer_create_args_t*, esp_timer_handle_t*);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t, uint64_t);
''')
write('freertos/FreeRTOS.h', '#pragma once\n#include <stdint.h>\ntypedef uint32_t TickType_t;\n#define portTICK_PERIOD_MS 1\n#define pdMS_TO_TICKS(ms) (ms)\n#define pdTRUE 1\n')
write('freertos/portmacro.h', '#pragma once\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(m) ((void)(m))\n#define portEXIT_CRITICAL(m) ((void)(m))\n')
write('freertos/semphr.h', '#pragma once\n#include \"FreeRTOS.h\"\ntypedef void *SemaphoreHandle_t;\nint xSemaphoreTake(SemaphoreHandle_t, int);\nint xSemaphoreGive(SemaphoreHandle_t);\n')
write('freertos/task.h', '#pragma once\n#include "FreeRTOS.h"\nTickType_t xTaskGetTickCount(void);\n')
write('bsp_obd_dsp/elm327_ble_client.h', (MAIN/'bsp_obd_dsp/elm327_ble_client.h').read_text())
write('bsp_obd_dsp/espnow_link.h', (MAIN/'bsp_obd_dsp/espnow_link.h').read_text())
write('bsp_obd_dsp/lcd_driver/ST77916.h', '#pragma once\n#include <stdint.h>\nvoid Set_Backlight(uint8_t value);\n')
write('host_shim.h', '''#pragma once
#include "sdkconfig.h"
#include "ui.h"
#include "ui_status_ring.h"
#include "ui_peak_marker.h"
#include "app_obd_dsp/vehicle_profiles.h"
#include <stdlib.h>
#include <string.h>
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
#define heap_caps_malloc(size, flags) malloc(size)
#include "stopwatch/stopwatch_board.h"
size_t strlcat(char*, const char*, size_t);
''')

def function(source, marker):
    if marker.endswith('{'):
        start = source.index(marker)
    else:
        match = re.search(re.escape(marker) + r'[^;{}]*\{', source)
        assert match, marker
        start = match.start()
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

helpers = (UI/'ui_helpers.c').read_text()
start = helpers.index('#if CONFIG_OBD_HW_VERSION_M5STOPWATCH\n#define STOPWATCH_RING_SIZE')
write('host_helpers.c', '#include "host_shim.h"\n' + helpers[start:])
refresh = function((UI/'ui.c').read_text(), 'if (scr == ui_ScreenPageRpm) {')
speed_refresh = function((UI/'ui.c').read_text(), 'if (scr == ui_ScreenPageSpeed) {')
speed_step = function((UI/'ui.c').read_text(), 'static inline int32_t anim_step_i32(')
write('host_update.c', '#include "host_shim.h"\n#include "ui_ext.h"\n#define IN_SWEEP (ui_ext_sweep_active())\n#define ANIM_THRESH_SPD 10\n'
      + speed_step + '\nvoid host_update(uint16_t usRpm) {ui_peak_marker_tick();lv_obj_t *scr = lv_scr_act();\n' + refresh + '\n}\n'
      + 'void host_speed(uint16_t ucSpeed) {ui_peak_marker_tick();lv_obj_t *scr = lv_scr_act();\n' + speed_refresh + '\n}\n')
info_refresh = function((UI/'ui.c').read_text(), 'if (scr == ui_ScreenPageEasterEgg && ui_LabelEasterEggInfo) {')
with (BUILD/'host_update.c').open('a') as f:
    f.write('#include <stdio.h>\n#include "bsp_obd_dsp/nvs_storage.h"\n#include "bsp_obd_dsp/espnow_link.h"\n#include "bsp_obd_dsp/elm327_ble_client.h"\n'
            + 'void host_info(bool is_slave,bool ble_now) {lv_obj_t *scr=lv_scr_act(); const nvs_user_cfg_t *user_cfg=nvs_cfg_get();\n'
            + info_refresh + '\n}\n')
nav_source = (UI/'ui.c').read_text()
nav = [function(nav_source, marker) for marker in ('static bool ui_stopwatch_carousel_gesture(', 'void ui_event_gear_background(', 'void ui_event_theme_gauge_background(', 'void ui_event_rpm_background(', 'void ui_event_speed_background(', 'void ui_event_temp_background(', 'void ui_event_oil_pressure_background(', 'void ui_event_needle_background(', 'void ui_event_info_background(', 'void ui_event_gforce_background(', 'void ui_event_expression_background(', 'void ui_event_easter_egg_background(', 'void ui_event_settings_background(', 'void ui_event_feedback_background(', 'void ui_event_ble_scan_background(', 'void ui_event_multi_gauge_background(', 'void ui_event_needle_config_background(', 'void ui_event_gforce_cal_background(', 'void ui_event_obd_prot_background(', 'bool ui_stopwatch_button_navigate(')]
write('host_navigation.c', '#include "host_shim.h"\n#include "ui_ext.h"\n'
      + '#include "bsp_obd_dsp/gauge_pair_ble_client.h"\n#include "bsp_obd_dsp/elm327_ble_client.h"\n#include "bsp_obd_dsp/espnow_link.h"\n#include "bsp_obd_dsp/nvs_storage.h"\n'
      + 'int theme_page_list_count(void); extern uint8_t ui_theme_gauge_page_index;\n'
      + '#include "esp_system.h"\n#define SAVE_PROTOCOL_TIME 2000\nstatic uint16_t usSaveProtTimeCnt;\n'
      + '\n'.join(nav))
page_names=('Temp','Info','Needle','OilPressure','Expression','Gear','GForce','EasterEgg','Logo','Rpm','Speed','Settings','RingSettings','PeakSettings','SettingsMenu','Feedback','CxSettings','MultiGauge','BLEScan','ODBProtocal')
page_text='\n'.join((UI/f'screens/ui_ScreenPage{name}.c').read_text() for name in page_names)
defined=set(re.findall(r'^lv_obj_t\s*\*\s*(\w+)\s*(?:=|;)',page_text,re.M))
globals_=re.findall(r'extern lv_obj_t \*\s*(\w+)\s*;', (UI/'ui.h').read_text())
write('host_globals.c','#include "host_shim.h"\n'+'\n'.join(f'lv_obj_t *{n};' for n in globals_ if n not in defined)+'\nlv_meter_scale_t *ui_NeedleScale;\nlv_meter_indicator_t *ui_NeedleIndic;\n')
with (BUILD/'host_update.c').open('a') as f:
    markers=('static uint8_t needle_active_source(', 'void ui_needle_apply_source(', 'void ui_chart_apply_source(')
    f.write('\nstatic int32_t s_chart_ymin,s_chart_ymax;\nstatic bool s_oil_pressure_trend_ready;\nstatic uint32_t s_oil_pressure_trend_tick;\n' + '\n'.join(function(nav_source,m) for m in markers) + '\n')

flash_source = (UI/'ui_ext.c').read_text()
flash_start = flash_source.index('volatile int s_rpm_flash_test_ticks')
flash_end = flash_source.index('void ui_ext_rpm_flash_tick(')
write('host_flash.c', '#include "host_shim.h"\n#include "ui_helpers.h"\n#include "bsp_obd_dsp/nvs_storage.h"\n#include "bsp_obd_dsp/espnow_link.h"\n#include "bsp_obd_dsp/elm327_ble_client.h"\n#define USE_CUSTOM_RPM_FLASH 0\n' + flash_source[flash_start:flash_end] + function(flash_source, 'void ui_ext_rpm_flash_tick('))

sources = [UI/'screens/ui_ScreenPageEasterEgg.c', UI/'screens/ui_ScreenPageLogo.c', BUILD/'host_flash.c', Path(__file__).resolve().parent/'fixtures.c', BUILD/'host_helpers.c', BUILD/'host_update.c', BUILD/'host_globals.c', BUILD/'host_navigation.c',
           UI/'screens/ui_ScreenPageTemp.c', UI/'screens/ui_ScreenPageInfo.c', UI/'screens/ui_ScreenPageNeedle.c', UI/'screens/ui_ScreenPageOilPressure.c', UI/'screens/ui_ScreenPageExpression.c', UI/'screens/ui_ScreenPageRpm.c', UI/'screens/ui_ScreenPageGear.c', UI/'screens/ui_ScreenPageSpeed.c', UI/'screens/ui_ScreenPageSettings.c',
           UI/'screens/ui_ScreenPageRingSettings.c', UI/'screens/ui_ScreenPageSettingsMenu.c',
           UI/'screens/ui_ScreenPageFeedback.c', UI/'screens/ui_ScreenPageCxSettings.c',
           UI/'screens/ui_ScreenPageMultiGauge.c', UI/'screens/ui_ScreenPageBLEScan.c',
           UI/'screens/ui_ScreenPageODBProtocal.c', UI/'screens/ui_ScreenPageGForce.c',
           UI/'ui_status_ring.c', UI/'ui_disp_item.c', UI/'ui_peak_marker.c', UI/'screens/ui_ScreenPagePeakSettings.c',
           MAIN/'app_obd_dsp/peak_marker_policy.c',
           MAIN/'app_obd_dsp/status_ring_policy.c', MAIN/'app_obd_dsp/obd_data_cache.c']
sources += [UI/f'fonts/ui_font_FontTypoderSize{s}.c' for s in (140, 90, 24, 20, 16, 36, 40, 44)]
sources += [UI/f'images/{s}.c' for s in ('ui_img_pngblackear_png', 'ui_img_mini_jcw', 'ui_img_mini_gp3')]
sources += [UI/'ui_theme_generated.c']
flags = ['gcc', '-std=c11', '-O2', '-g', '-ffunction-sections', '-fdata-sections',
         '-DOBD_GAUGE_BUILD_TAG="port-m5stopwatch-11-7cd158a1406"', '-DLV_CONF_KCONFIG_EXTERNAL_INCLUDE="sdkconfig.h"', '-DLV_CONF_SKIP=1',
         '-I'+str(BUILD), '-I'+str(MAIN), '-I'+str(UI), '-I'+str(LV), '-include', str(BUILD/'host_shim.h')]

def compile_one(pair):
    idx, source = pair
    obj = BUILD/f'{idx:02}_{source.stem}.o'
    extra = ['-DHOST_G_FORCE=1'] if source.name=='ui_ScreenPageGForce.c' else []
    result = subprocess.run(flags+extra+['-c', str(source), '-o', str(obj)], capture_output=True, text=True)
    if result.returncode: raise RuntimeError(str(source)+'\n'+result.stderr)
    return obj

print('Compiling actual UI, color policy and cache sources...', flush=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
    objects = list(pool.map(compile_one, enumerate(sources)))
lv_sources=sorted((LV/'src').rglob('*.c'))
print(f'Compiling {len(lv_sources)} actual LVGL source files...',flush=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    lv_objects=list(pool.map(compile_one,enumerate(lv_sources,start=1000)))
exe = BUILD/'status_ring_host'
subprocess.run(['gcc', '-Wl,--gc-sections', '-Wl,--wrap=lv_indev_get_act', '-Wl,--wrap=free',
                '-Wl,--wrap=lv_indev_get_gesture_dir', '-Wl,--wrap=lv_indev_wait_release',
                *map(str, lv_objects+objects), '-lm', '-o', str(exe)], check=True)
subprocess.run([str(exe), str(OUT)], check=True)
original_inputs = [p for p in sources if p.is_relative_to(REPO) and not p.is_relative_to(BUILD)]
original_inputs += [UI/'ui.c', UI/'ui_helpers.c', REPO/'sdkconfig.stopwatch', Path(__file__).resolve()]
original_inputs += list(MAIN.rglob('*.h'))
(OUT/'sim-manifest.json').write_text(json.dumps({
    'method': 'Actual LVGL CPU renderer, unchanged UI source bodies, real ring adapter and real OBD cache',
    'stubbed': ['BLE state', 'ESP timer/FreeRTOS synchronization', 'NVS persistence', 'PSRAM malloc', 'vehicle profile', 'hardware side effects'],
    'rendered_pages': ['status','gear','rpm','speed','temperature','info','g-force','needle','chart','expression','settings','cx-standby'],
    'data': 'Illustrative input fixtures, not recorded vehicle measurements',
    'source_sha256': {str(p.relative_to(REPO)): hashlib.sha256(p.read_bytes().replace(b'\r\n', b'\n')).hexdigest() for p in sorted(set(original_inputs))},
    'source_hash_format': 'SHA-256 of source bytes with CRLF normalized to LF',
    'hardware_validation': False
}, indent=2))
