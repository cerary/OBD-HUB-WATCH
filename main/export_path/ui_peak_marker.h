#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

enum { UI_PEAK_RPM, UI_PEAK_SPEED, UI_PEAK_COUNT };
typedef struct { uint8_t enabled, width_px, lifetime_s; } ui_peak_config_t;
ui_peak_config_t ui_peak_marker_config(void);
bool ui_peak_marker_save_config(const ui_peak_config_t *config);
void ui_peak_marker_register(lv_obj_t *arc, unsigned channel, uint16_t scale_max);
void ui_peak_marker_sample(unsigned channel, int32_t display_value);
void ui_peak_marker_tick(void);
