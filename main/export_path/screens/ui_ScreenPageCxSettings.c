#include "../ui.h"
#include "bsp_obd_dsp/elm327_ble_client.h"
#include "stopwatch/stopwatch_board.h"
#include "esp_log.h"
#include <string.h>

#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
static lv_obj_t *status_label, *enable_label, *waiting_card, *waiting_text;
static bool shutdown_failed;
static bool have_power;
static bool last_power;
static uint32_t next_power_check;

static void action(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    if (which == 0) {
        if (!elm327_ble_cx_set_enabled(!elm327_ble_cx_enabled()))
            lv_label_set_text(status_label, "Settings save failed");
    } else if (which == 1) elm327_ble_cx_read_config();
    else if (which == 2) elm327_ble_cx_manual_resume();
    else ui_settings_handle_back();
}
static lv_obj_t *button(lv_obj_t *parent, const char *text, int y, int action_id) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 240, 42);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, y);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text); lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_add_event_cb(btn, action, LV_EVENT_CLICKED, (void *)(intptr_t)action_id);
    return label;
}
static void deleted(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        status_label = enable_label = NULL; ui_ScreenPageCxSettings = NULL;
    }
}
void ui_ScreenPageCxSettings_screen_init(void) {
    ui_ScreenPageCxSettings = lv_obj_create(NULL);
    lv_obj_t *screen = ui_ScreenPageCxSettings;
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101014), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xFFFFFF), 0);
    lv_obj_t *ring = ui_helpers_create_ring(screen, 10);
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "CX STANDBY");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -156);
    status_label = lv_label_create(screen);
    lv_obj_set_width(status_label, 330);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, -108);
    enable_label = button(screen, "", -44, 0);
    button(screen, "READ CX CONFIG", 8, 1);
    button(screen, "RECONNECT", 60, 2);
    button(screen, "BACK", 112, 3);
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "OFF restores saved CX settings\nSleep alert -> wait for 5V off");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 165);
    lv_obj_add_event_cb(screen, deleted, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(screen, ui_settings_detail_event, LV_EVENT_GESTURE, NULL);
    lv_obj_move_foreground(ring);
}
void ui_cx_power_update(void) {
    if (status_label && ui_ScreenPageCxSettings && lv_scr_act() == ui_ScreenPageCxSettings) {
        lv_label_set_text(status_label, elm327_ble_cx_status());
        lv_label_set_text(enable_label, elm327_ble_cx_enabled() ? "LINKED STANDBY: ON" : "LINKED STANDBY: OFF");
    }
    bool waiting = elm327_ble_cx_is_waiting();
    if (waiting && !waiting_card) {
        waiting_card = lv_obj_create(lv_layer_top());
        lv_obj_set_size(waiting_card, 350, 210);
        lv_obj_center(waiting_card);
        lv_obj_clear_flag(waiting_card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(waiting_card, lv_color_hex(0x101014), 0);
        lv_obj_set_style_text_color(waiting_card, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(waiting_card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(waiting_card, lv_color_hex(0xF09936), 0);
        lv_obj_set_style_radius(waiting_card, 22, 0);
        waiting_text = lv_label_create(waiting_card);
        lv_obj_set_width(waiting_text, 310);
        lv_obj_set_style_text_font(waiting_text, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(waiting_text, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(waiting_text);
    }
    if (waiting_card) {
        if (waiting && lv_scr_act() != ui_ScreenPageCxSettings) {
            lv_obj_clear_flag(waiting_card, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(waiting_text, "%s\n\n5V off: power down\nAny key: reconnect", elm327_ble_cx_status());
        } else lv_obj_add_flag(waiting_card, LV_OBJ_FLAG_HIDDEN);
    }
    uint32_t now = lv_tick_get();
    if ((int32_t)(now - next_power_check) < 0) return;
    next_power_check = now + 500;
    uint8_t percent; bool external_power;
    if (!stopwatch_board_power_status(&percent, &external_power)) {
        // An unknown input must never be counted as continued external power loss.
        elm327_ble_cx_power_shutdown_due(true);
        return;
    }
    if (!have_power || external_power != last_power) {
        ESP_LOGI("cx_power", "[POWER] external=%s battery=%u%% waiting=%d", external_power ? "ON" : "OFF", percent, waiting);
        have_power = true; last_power = external_power;
    }
    if (!waiting || external_power) shutdown_failed = false;
    if (!shutdown_failed && elm327_ble_cx_power_shutdown_due(external_power)) {
        ESP_LOGI("cx_power", "[POWER] shutdown gate: CX asleep + external 5V absent >=3s");
        shutdown_failed = !stopwatch_board_shutdown();
    }
}
#else
void ui_ScreenPageCxSettings_screen_init(void) {}
void ui_cx_power_update(void) {}
#endif

