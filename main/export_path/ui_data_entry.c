#include "ui_data_entry.h"
#include "ui.h"
#include "ui_ext.h"
#include "ui_disp_item.h"
#include "ui_display_filter.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "app_obd_dsp/vehicle_profiles.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/elm327_ble_client.h"
#include "bsp_obd_dsp/espnow_link.h"
#include <string.h>

static lv_timer_t *refresh_timer;
static lv_obj_t *entry_screen;

static void paint_metric(lv_obj_t *label, disp_item_t item,
                         const obd_data_snapshot_t *data, uint16_t rpm, uint16_t speed)
{
    int32_t raw = 0;
    bool valid = disp_item_read_value(item, data->coolant_temp, data->intake_temp,
        data->oil_temp, data->load_pct, data->tps, data->bat_mv,
        data->oil_pressure_x10, data->brake_temp_x10, rpm, speed,
        data->boost_x10, data->afr_x100, &raw);
    disp_item_set_text(label, item, raw, valid);
    if (valid && (item == DISP_ITEM_OILP || item == DISP_ITEM_BKT))
        disp_item_update(&raw, label, item, raw, true, 0); // preserve alarm cooldown
    else
        disp_item_set_value_color(label, item, raw, valid);
}

static void page_load_start(lv_event_t *event)
{
    lv_obj_t *screen = lv_event_get_target(event);
    entry_screen = screen;
    if (refresh_timer) lv_timer_ready(refresh_timer);
    // The active sweep has its own simulated readings and owns their display.
    if (ui_ext_sweep_active()) return;

    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    bool demo = ui_ext_showroom_is_active();
    bool connected = cfg->device_role == ESPNOW_ROLE_SLAVE ?
        espnow_link_slave_has_data() : elm327_ble_is_connected();
    obd_data_snapshot_t data;
    obd_data_freshness_t freshness;
    obd_data_get_fresh_snapshot(&data, &freshness);
    if (!demo) {
        if (!connected) memset(&freshness, 0xff, sizeof(freshness));
        obd_data_apply_freshness(&data, &freshness);
    }
    uint16_t rpm = demo || obd_data_sample_is_fresh(&freshness, OBD_SAMPLE_RPM) ?
        data.rpm : UINT16_MAX;
    uint16_t speed = demo || obd_data_sample_is_fresh(&freshness, OBD_SAMPLE_SPEED) ?
        data.speed : UINT16_MAX;
    // The next regular refresh starts from this same cached value, rather than
    // stepping back towards an old page's smoothing state.
    ui_display_filter_reset();

    if (screen == ui_ScreenPageRpm) {
        disp_item_set_text(ui_RpmPageArcLabelRpmText, DISP_ITEM_RPM, rpm, rpm != UINT16_MAX);
        lv_arc_set_value(ui_RpmPageArcRpmBack, rpm == UINT16_MAX ? 0 : ui_disp_item_arc_percent(DISP_ITEM_RPM, rpm));
    } else if (screen == ui_ScreenPageSpeed) {
        disp_item_set_text(ui_SpeedPageArcLabelSpeedText, DISP_ITEM_SPEED, speed, speed != UINT16_MAX);
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
        lv_arc_set_value(ui_SpeedPageArcSpeedBack, speed == UINT16_MAX ? 0 : ui_disp_item_arc_percent(DISP_ITEM_SPEED, speed));
#else
        lv_arc_set_value(ui_SpeedPageArcSpeedBack, speed == UINT16_MAX ? 0 : (uint32_t)speed * 100 / SWEEP_SPEED_PEAK);
#endif
    } else if (screen == ui_ScreenPageGear) {
        static const char *names[] = {"N","1","2","3","4","5","6","7","8"};
        bool known = rpm != UINT16_MAX && speed != UINT16_MAX;
        enGear gear = GEAR_NEUTRAL;
        if (known && data.gear >= 0 && data.gear <= GEAR_8) gear = (enGear)data.gear;
        else if (known && vehicle_profile_get_active()->obd_gear_did == 0)
            gear = calculate_gear(data.rpm, data.speed_unsmoothed);
        else known = false;
        if (!known) obd_data_reset_gear_estimate();
        if (gear == GEAR_UNKNOWN) known = false;
        uint8_t count = vehicle_profile_get_active()->gear_count;
        if (count < 1) count = 6;
        if (gear > GEAR_8) gear = GEAR_8;
        lv_label_set_text(ui_GearPageArcLabelGearNumText, known ? names[gear] : "--");
        lv_arc_set_value(ui_GearPageArcGearNumBack, known ? (uint16_t)gear * 100 / count : 0);
    } else if (screen == ui_ScreenPageTemp) {
        for (int i = 0; i < 3; ++i)
            paint_metric(ui_LabelTempValue[i], cfg->temp_display_map[i] % DISP_ITEM_COUNT, &data, rpm, speed);
    } else if (screen == ui_ScreenPageInfo) {
        for (int i = 0; i < 5; ++i)
            paint_metric(ui_LabelInfoValue[i], cfg->info_display_map[i] % DISP_ITEM_COUNT, &data, rpm, speed);
    } else if (screen == ui_ScreenPageOilPressure) {
        paint_metric(ui_LabelOilPressureText, cfg->chart_source_idx % DISP_ITEM_COUNT, &data, rpm, speed);
    } else if (screen == ui_ScreenPageNeedle) {
        uint8_t src = cfg->needle_source_idx;
        if (src >= DISP_ITEM_COUNT) src = DISP_ITEM_CLT;
        paint_metric(ui_NeedleValueLabel, src, &data, rpm, speed);
        int32_t raw = 0;
        bool valid = disp_item_read_value(src, data.coolant_temp, data.intake_temp, data.oil_temp,
            data.load_pct, data.tps, data.bat_mv, data.oil_pressure_x10, data.brake_temp_x10,
            rpm, speed, data.boost_x10, data.afr_x100, &raw);
        const needle_scale_meta_t *scale = ui_disp_item_scale(src);
        int32_t minimum = scale->nmin * scale->div;
        int32_t maximum = scale->nmax * scale->div;
        int32_t value = valid ? raw : minimum;
        if (value < minimum) value = minimum;
        if (value > maximum) value = maximum;
        lv_meter_set_indicator_value(ui_NeedleMeter, ui_NeedleIndic, value);
    }
}

void ui_data_entry_register(lv_obj_t *screen)
{
    lv_obj_add_event_cb(screen, page_load_start, LV_EVENT_SCREEN_LOAD_START, NULL);
}

void ui_data_entry_bind_timer(lv_timer_t *timer) { refresh_timer = timer; }
bool ui_data_entry_pending(lv_obj_t *screen) { return screen && screen == entry_screen; }
void ui_data_entry_complete(lv_obj_t *screen)
{
    if (entry_screen == screen) entry_screen = NULL;
}
