#include "ui_peak_marker.h"
#include "ui.h"
#include "ui_ext.h"
#include "app_obd_dsp/peak_marker_policy.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/elm327_ble_client.h"
#include "bsp_obd_dsp/espnow_link.h"
#include <math.h>
#include <string.h>

ui_peak_config_t ui_peak_marker_config(void)
{
    // Use the three reserved bytes in the existing ring blob. Its size and
    // all previous thresholds stay unchanged; old zero bytes select ON/10px/5s.
    const uint8_t *saved = nvs_status_ring_get()->reserved;
    return (ui_peak_config_t){saved[0] != 2, saved[1] == 5 ? 5 : 10, saved[2] == 10 ? 10 : 5};
}
bool ui_peak_marker_save_config(const ui_peak_config_t *config)
{
    if (!config || config->enabled > 1 ||
        (config->width_px != 5 && config->width_px != 10) ||
        (config->lifetime_s != 5 && config->lifetime_s != 10)) return false;
    status_ring_config_t ring = *nvs_status_ring_get();
    ring.reserved[0] = config->enabled ? 1 : 2;
    ring.reserved[1] = config->width_px;
    ring.reserved[2] = config->lifetime_s;
    if (!memcmp(&ring, nvs_status_ring_get(), sizeof(ring))) return true;
    return nvs_status_ring_set(&ring) == ESP_OK;
}

#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
typedef struct {
    lv_obj_t *arc;
    peak_marker_state_t state;
    uint16_t scale_max;
    int32_t drawn_peak;
    uint8_t drawn_opacity;
} peak_entry_t;
static peak_entry_t s_entries[UI_PEAK_COUNT];
static ui_peak_config_t s_config;
static bool s_valid[UI_PEAK_COUNT];

static void invalidate_marker(peak_entry_t *entry, int32_t peak)
{
    if (peak <= 0) return;
    lv_obj_t *arc = entry->arc;
    lv_area_t box;
    lv_obj_get_coords(arc, &box);
    lv_coord_t left = lv_obj_get_style_pad_left(arc, LV_PART_MAIN);
    lv_coord_t top = lv_obj_get_style_pad_top(arc, LV_PART_MAIN);
    lv_coord_t r = LV_MIN(lv_obj_get_width(arc) - left - lv_obj_get_style_pad_right(arc, LV_PART_MAIN),
                          lv_obj_get_height(arc) - top - lv_obj_get_style_pad_bottom(arc, LV_PART_MAIN)) / 2;
    lv_point_t center = {box.x1 + left + r, box.y1 + top + r};
    int32_t span = lv_arc_get_bg_angle_end(arc) - lv_arc_get_bg_angle_start(arc);
    if (span <= 0) span += 360;
    int32_t degrees = lv_arc_get_bg_angle_start(arc) + ((lv_arc_t *)arc)->rotation +
                      LV_MIN(100, peak * 100 / entry->scale_max) * span / 100;
    float a = degrees * (3.14159265358979323846f / 180.f);
    lv_coord_t inner = r - lv_obj_get_style_arc_width(arc, LV_PART_MAIN);
    lv_coord_t x1 = center.x + (lv_coord_t)lroundf(inner * cosf(a));
    lv_coord_t y1 = center.y + (lv_coord_t)lroundf(inner * sinf(a));
    lv_coord_t x2 = center.x + (lv_coord_t)lroundf(r * cosf(a));
    lv_coord_t y2 = center.y + (lv_coord_t)lroundf(r * sinf(a));
    // Include the widest 10px tick and antialiasing. Fade only redraws this
    // small area, rather than refreshing the full 416px gauge or its digits.
    lv_area_t dirty = {LV_MIN(x1,x2)-8, LV_MIN(y1,y2)-8, LV_MAX(x1,x2)+8, LV_MAX(y1,y2)+8};
    lv_obj_invalidate_area(arc, &dirty);
}

static void draw_marker(lv_event_t *event)
{
    peak_entry_t *entry = lv_event_get_user_data(event);
    if (lv_event_get_code(event) == LV_EVENT_DELETE) {
        entry->arc = NULL;
        entry->drawn_opacity = 0;
        entry->drawn_peak = -1;
        return;
    }
    uint8_t opacity = peak_marker_opacity(&entry->state, lv_tick_get(), s_config.lifetime_s * 1000U);
    if (!opacity || !s_config.enabled) return;
    lv_obj_t *arc = entry->arc;
    lv_area_t area;
    lv_obj_get_coords(arc, &area);
    area.x1 += lv_obj_get_style_pad_left(arc, LV_PART_MAIN);
    area.x2 -= lv_obj_get_style_pad_right(arc, LV_PART_MAIN);
    area.y1 += lv_obj_get_style_pad_top(arc, LV_PART_MAIN);
    area.y2 -= lv_obj_get_style_pad_bottom(arc, LV_PART_MAIN);
    lv_coord_t radius = LV_MIN(lv_area_get_width(&area), lv_area_get_height(&area)) / 2;
    lv_point_t center = {area.x1 + lv_area_get_width(&area) / 2,
                         area.y1 + lv_area_get_height(&area) / 2};
    lv_coord_t band = lv_obj_get_style_arc_width(arc, LV_PART_MAIN);
    // Match lv_arc_set_value's integer percentage and endpoint angle exactly.
    int32_t percent = LV_MIN(100, entry->state.peak * 100 / entry->scale_max);
    int32_t span = lv_arc_get_bg_angle_end(arc) - lv_arc_get_bg_angle_start(arc);
    if (span <= 0) span += 360;
    int32_t start = (lv_arc_get_bg_angle_start(arc) + ((lv_arc_t *)arc)->rotation) % 360;
    int32_t end = (start + span) % 360;
    int32_t angle = start + percent * span / 100;
    float radians = angle * (3.14159265358979323846f / 180.f);
    lv_point_t p1 = {center.x + (lv_coord_t)lroundf((radius - band - 2) * cosf(radians)),
                     center.y + (lv_coord_t)lroundf((radius - band - 2) * sinf(radians))};
    lv_point_t p2 = {center.x + (lv_coord_t)lroundf((radius + 2) * cosf(radians)),
                     center.y + (lv_coord_t)lroundf((radius + 2) * sinf(radians))};
    lv_area_t outer = {center.x-radius, center.y-radius, center.x+radius-1, center.y+radius-1};
    // Keep the antialiased inner edge on the gray band, including at the
    // 270-degree scale endpoint where its angle mask meets the circle mask.
    lv_coord_t inner_r = radius - band + 1;
    lv_area_t inner = {center.x-inner_r, center.y-inner_r, center.x+inner_r-1, center.y+inner_r-1};
    lv_draw_mask_radius_param_t outer_mask, inner_mask;
    lv_draw_mask_angle_param_t angle_mask;
    lv_draw_mask_radius_init(&outer_mask, &outer, LV_RADIUS_CIRCLE, false);
    lv_draw_mask_radius_init(&inner_mask, &inner, LV_RADIUS_CIRCLE, true);
    lv_draw_mask_angle_init(&angle_mask, center.x, center.y, start, end);
    int16_t masks[] = {lv_draw_mask_add(&outer_mask, NULL), lv_draw_mask_add(&inner_mask, NULL),
                       lv_draw_mask_add(&angle_mask, NULL)};
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = ui_helpers_brand_accent_color();
    line.width = s_config.width_px;
    line.opa = opacity;
    line.round_start = line.round_end = 0;
    lv_draw_line(lv_event_get_draw_ctx(event), &line, &p1, &p2);
    for (unsigned i = 0; i < 3; ++i) lv_draw_mask_remove_id(masks[i]);
    lv_draw_mask_free_param(&outer_mask);
    lv_draw_mask_free_param(&inner_mask);
}

void ui_peak_marker_register(lv_obj_t *arc, unsigned channel, uint16_t scale_max)
{
    if (channel >= UI_PEAK_COUNT || !arc || !scale_max) return;
    peak_entry_t *entry = &s_entries[channel];
    entry->arc = arc; entry->scale_max = scale_max; entry->drawn_peak = -1;
    lv_obj_add_event_cb(arc, draw_marker, LV_EVENT_DRAW_POST_END, entry);
    lv_obj_add_event_cb(arc, draw_marker, LV_EVENT_DELETE, entry);
}

void ui_peak_marker_tick(void)
{
    ui_peak_config_t config = ui_peak_marker_config();
    bool changed = memcmp(&config, &s_config, sizeof(config)) != 0;
    s_config = config;
    const nvs_user_cfg_t *user = nvs_cfg_get();
    bool slave = user->device_role == ESPNOW_ROLE_SLAVE;
    bool connected = slave ? espnow_link_slave_has_data() : elm327_ble_is_connected();
    bool waiting = !slave && elm327_ble_cx_is_waiting();
    obd_data_snapshot_t data;
    obd_data_freshness_t freshness;
    obd_data_get_fresh_snapshot(&data, &freshness);
    const unsigned samples[] = {OBD_SAMPLE_RPM, OBD_SAMPLE_SPEED};
    for (unsigned i = 0; i < UI_PEAK_COUNT; ++i) {
        peak_entry_t *entry = &s_entries[i];
        s_valid[i] = config.enabled && connected && !waiting && !ui_ext_sweep_active() &&
                     !ui_ext_showroom_is_active() && freshness.age_ms[samples[i]] < 5000;
        if (changed || !s_valid[i]) peak_marker_reset(&entry->state);
        uint8_t opacity = peak_marker_opacity(&entry->state, lv_tick_get(), config.lifetime_s * 1000U);
        if (entry->arc && lv_obj_get_screen(entry->arc) == lv_scr_act() &&
            (opacity != entry->drawn_opacity || entry->state.peak != entry->drawn_peak || changed)) {
            if (entry->drawn_opacity) invalidate_marker(entry, entry->drawn_peak);
            if (opacity) invalidate_marker(entry, entry->state.peak);
            entry->drawn_opacity = opacity;
            entry->drawn_peak = entry->state.peak;
        }
    }
}

void ui_peak_marker_sample(unsigned channel, int32_t display_value)
{
    if (channel >= UI_PEAK_COUNT) return;
    peak_entry_t *entry = &s_entries[channel];
    peak_marker_sample(&entry->state, display_value, s_valid[channel], lv_tick_get(), s_config.lifetime_s * 1000U);
    ui_peak_marker_tick();
}
#else
void ui_peak_marker_register(lv_obj_t *arc, unsigned channel, uint16_t scale_max) {(void)arc;(void)channel;(void)scale_max;}
void ui_peak_marker_sample(unsigned channel, int32_t display_value) {(void)channel;(void)display_value;}
void ui_peak_marker_tick(void) {}
#endif
