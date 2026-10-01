#include "ui_status_ring.h"
#include "ui.h"
#include "ui_disp_item.h"
#include "ui_ext.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "app_obd_dsp/status_ring_policy.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/elm327_ble_client.h"
#include "bsp_obd_dsp/espnow_link.h"
#include <limits.h>
#include <math.h>
#include <string.h>

#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
_Static_assert((int)DISP_ITEM_COUNT == (int)RING_ITEM_COUNT &&
               (int)OBD_SAMPLE_AFR == (int)RING_AFR, "Ring/cache channel order changed");
typedef struct {
    lv_obj_t *screen, *ring;
    uint32_t color;
    uint8_t opacity;
    bool image;
} ring_entry_t;
static ring_entry_t s_rings[40];
static lv_obj_t *s_last_screen, *s_notice;
static uint32_t s_last_tick, s_connected_since, s_g_tick;
static bool s_last_connected, s_ever_connected, s_g_valid;
static uint16_t s_g_centi;

static bool live_page(lv_obj_t *screen)
{
    return screen == ui_ScreenPageRpm || screen == ui_ScreenPageGear ||
           screen == ui_ScreenPageSpeed || screen == ui_ScreenPageTemp ||
           screen == ui_ScreenPageInfo || screen == ui_ScreenPageNeedle ||
           screen == ui_ScreenPageOilPressure ||
           screen == ui_ScreenPageGForce;
}

static void update(lv_obj_t *screen, bool force)
{
    ring_entry_t *entry = NULL;
    for (unsigned i = 0; i < 40; ++i)
        if (s_rings[i].ring && s_rings[i].screen == screen) { entry = &s_rings[i]; break; }
    if (!entry) { if (s_notice) lv_obj_add_flag(s_notice, LV_OBJ_FLAG_HIDDEN); return; }
    const uint32_t now = lv_tick_get();
    bool slave = nvs_cfg_get()->device_role == ESPNOW_ROLE_SLAVE;
    bool connected = slave ? espnow_link_slave_has_data() : elm327_ble_is_connected();
    bool sleeping = !slave && elm327_ble_cx_is_asleep();
    if (connected != s_last_connected) {
        s_last_connected = connected;
        if (connected) { s_ever_connected = true; s_connected_since = now; }
        force = true;
    }
    if (!force && screen == s_last_screen && now - s_last_tick < 100) return;
    s_last_screen = screen; s_last_tick = now;

    status_ring_input_t in = {0};
    in.connected = connected;
    in.ever_connected = s_ever_connected;
    in.initializing = connected && now - s_connected_since < 5000;
    in.sleeping = sleeping;
    in.demo = ui_ext_showroom_is_active();
    obd_data_snapshot_t data;
    obd_data_freshness_t freshness;
    obd_data_get_fresh_snapshot(&data, &freshness);
    for (uint8_t i = 0; i < RING_ITEM_COUNT; ++i) {
        in.age_ms[i] = freshness.age_ms[i];
        in.alarm[i] = nvs_chart_alarm_get(i);
        in.valid[i] = disp_item_read_value((disp_item_t)i, data.coolant_temp,
            data.intake_temp, data.oil_temp, data.load_pct, data.tps, data.bat_mv,
            data.oil_pressure_x10, data.brake_temp_x10, data.rpm, data.speed,
            data.boost_x10, data.afr_x100, &in.value[i]);
    }
    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    if (screen == ui_ScreenPageRpm || screen == ui_ScreenPageGear) {
        in.items[0] = RING_RPM; in.item_count = 1;
    } else if (screen == ui_ScreenPageSpeed) {
        in.items[0] = RING_SPEED; in.item_count = 1;
    } else if (screen == ui_ScreenPageTemp) {
        memcpy(in.items, cfg->temp_display_map, 3); in.item_count = 3;
    } else if (screen == ui_ScreenPageInfo) {
        memcpy(in.items, cfg->info_display_map, 5); in.item_count = 5;
    } else if (screen == ui_ScreenPageNeedle) {
        in.items[0] = cfg->needle_source_idx; in.item_count = 1;
    } else if (screen == ui_ScreenPageOilPressure) {
        in.items[0] = cfg->chart_source_idx; in.item_count = 1;
    }
    if (screen == ui_ScreenPageGForce) {
        in.page = RING_PAGE_G;
        in.g_centi = s_g_centi; in.g_valid = s_g_valid; in.g_age_ms = now - s_g_tick;
    } else if (in.item_count) in.page = RING_PAGE_ITEMS;
    // The start-up sweep only tests the inner gauge; never present its
    // synthetic peak as a real engine/temperature warning.
    if (ui_ext_sweep_active() && !in.demo) { in.page = RING_PAGE_STATIC; in.item_count = 0; }
    status_ring_result_t out = status_ring_evaluate(&in, nvs_status_ring_get());
    // Dim the RGB channels, keeping the ring opaque. Translucent dimming would
    // blend the existing flashing background into the ring and make it blink.
    uint32_t color = 0;
    uint8_t brightness = nvs_status_ring_get()->brightness;
    for (int shift = 0; shift <= 16; shift += 8)
        color |= (((out.color >> shift) & 255U) * brightness / 100U) << shift;
    uint8_t opacity = LV_OPA_COVER;
    if (color != entry->color || opacity != entry->opacity) {
        entry->color = color; entry->opacity = opacity;
        if (entry->image) {
            lv_obj_set_style_img_recolor(entry->ring, lv_color_hex(color), 0);
            lv_obj_set_style_img_opa(entry->ring, opacity, 0);
        } else {
            lv_obj_set_style_border_color(entry->ring, lv_color_hex(color), 0);
            lv_obj_set_style_border_opa(entry->ring, opacity, 0);
        }
    }
    // A small cause label accompanies parameter alarms; connection and sleep
    // messages already have their own existing overlays. Neither is animated.
    if (!s_notice) {
        s_notice = lv_label_create(lv_layer_top());
        lv_obj_set_style_text_font(s_notice, &lv_font_montserrat_12, 0);
        lv_obj_set_style_bg_color(s_notice, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(s_notice, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_hor(s_notice, 4, 0);
        lv_obj_clear_flag(s_notice, LV_OBJ_FLAG_CLICKABLE);
    }
    bool show = out.state == RING_ALARM && out.item < RING_ITEM_COUNT &&
                live_page(screen) && !ui_ext_sweep_active();
    if (show) {
        char text[32];
        lv_snprintf(text, sizeof(text), "%s HIGH", ui_disp_item_name(out.item));
        if (strcmp(lv_label_get_text(s_notice), text)) lv_label_set_text(s_notice, text);
        lv_obj_set_style_text_color(s_notice, lv_color_hex(out.color), 0);
        lv_obj_align(s_notice, LV_ALIGN_CENTER, 0, 196);
        lv_obj_clear_flag(s_notice, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(s_notice, LV_OBJ_FLAG_HIDDEN);
}

static void ring_deleted(lv_event_t *e)
{
    ring_entry_t *entry = lv_event_get_user_data(e);
    if (entry->screen == s_last_screen) s_last_screen = NULL;
    memset(entry, 0, sizeof(*entry));
}
static void screen_loaded(lv_event_t *e) { update(lv_event_get_target(e), true); }
void ui_status_ring_register(lv_obj_t *screen, lv_obj_t *ring, bool image)
{
    for (unsigned i = 0; i < 40; ++i) {
        if (s_rings[i].ring) continue;
        s_rings[i] = (ring_entry_t){screen, ring, UINT32_MAX, 0, image};
        lv_obj_add_event_cb(ring, ring_deleted, LV_EVENT_DELETE, &s_rings[i]);
        lv_obj_add_event_cb(screen, screen_loaded, LV_EVENT_SCREEN_LOADED, NULL);
        break;
    }
}
void ui_status_ring_tick(void) { update(lv_scr_act(), false); }
void ui_status_ring_set_g_sample(float magnitude, bool valid)
{
    s_g_valid = valid && isfinite(magnitude);
    if (s_g_valid) {
        s_g_centi = (uint16_t)fminf(2000.f, fabsf(magnitude) * 100.f);
        s_g_tick = lv_tick_get();
    }
}
#else
void ui_status_ring_register(lv_obj_t *screen, lv_obj_t *ring, bool image) { (void)screen; (void)ring; (void)image; }
void ui_status_ring_tick(void) {}
void ui_status_ring_set_g_sample(float magnitude, bool valid) { (void)magnitude; (void)valid; }
#endif
