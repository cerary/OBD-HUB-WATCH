#pragma once
#include "lvgl.h"
#include <stdbool.h>

// Paint a newly entered data page from the existing fresh cache before its
// first frame, then wake the normal UI timer. No adapter requests are issued.
void ui_data_entry_register(lv_obj_t *screen);
void ui_data_entry_bind_timer(lv_timer_t *timer);
bool ui_data_entry_pending(lv_obj_t *screen);
void ui_data_entry_complete(lv_obj_t *screen);
