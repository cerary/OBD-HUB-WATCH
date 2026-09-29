#pragma once

#include "app_obd_dsp/obd_data_cache.h"
#include <stdint.h>

// Stabilizes display values only. The OBD cache remains the source for alarms,
// gear calculation, ESP-NOW, and external data consumers.
void ui_display_filter_apply(const obd_data_snapshot_t *raw,
                             obd_data_snapshot_t *display,
                             uint32_t now_ms);
void ui_display_filter_reset(void);
