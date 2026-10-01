#pragma once
#include "lvgl.h"
#include <stdbool.h>
void ui_status_ring_register(lv_obj_t *screen, lv_obj_t *ring, bool image);
void ui_status_ring_tick(void);
void ui_status_ring_set_g_sample(float magnitude, bool valid);
