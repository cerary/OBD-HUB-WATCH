#include "../ui.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include <string.h>

lv_obj_t *ui_ScreenPageRingSettings;
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
static status_ring_config_t s_edit;
static lv_obj_t *s_sliders[5], *s_labels[5], *s_hint;
static const char *s_names[] = {"GREEN AT", "RED APPROACH", "RED AT", "G ALERT", "RING LIGHT"};

static void refresh(void)
{
    const int values[] = {s_edit.rpm_green, s_edit.rpm_approach, s_edit.rpm_red,
                          s_edit.g_warn_centi, s_edit.brightness};
    lv_slider_set_range(s_sliders[0], 500, s_edit.rpm_approach - 300);
    lv_slider_set_range(s_sliders[1], s_edit.rpm_green + 300, s_edit.rpm_red - 300);
    lv_slider_set_range(s_sliders[2], s_edit.rpm_approach + 300, 8000);
    lv_slider_set_range(s_sliders[3], 50, 200);
    lv_slider_set_range(s_sliders[4], 10, 100);
    for (unsigned i = 0; i < 5; ++i) {
        lv_slider_set_value(s_sliders[i], values[i], LV_ANIM_OFF);
        if (i < 3) lv_label_set_text_fmt(s_labels[i], "%s  %d rpm", s_names[i], values[i]);
        else if (i == 3) lv_label_set_text_fmt(s_labels[i], "%s  %d.%02dg", s_names[i], values[i]/100, values[i]%100);
        else lv_label_set_text_fmt(s_labels[i], "%s  %d%%", s_names[i], values[i]);
    }
}
static void change(lv_event_t *e)
{
    unsigned i = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        int value = lv_slider_get_value(s_sliders[i]);
        if (i < 3) value = ((value + 50) / 100) * 100;
        if (i == 3) value = ((value + 2) / 5) * 5;
        if (i == 0) s_edit.rpm_green = value;
        else if (i == 1) s_edit.rpm_approach = value;
        else if (i == 2) s_edit.rpm_red = value;
        else if (i == 3) s_edit.g_warn_centi = value;
        else s_edit.brightness = value;
        refresh();
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (memcmp(&s_edit, nvs_status_ring_get(), sizeof(s_edit)) != 0)
            lv_label_set_text(s_hint, nvs_status_ring_set(&s_edit) == ESP_OK ? "SAVED" : "SAVE FAILED");
    }
}
static void screen_event(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        ui_ScreenPageRingSettings = NULL;
        memset(s_sliders, 0, sizeof(s_sliders)); memset(s_labels, 0, sizeof(s_labels)); s_hint = NULL;
    } else if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        s_edit = *nvs_status_ring_get(); refresh();
    } else if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
        ui_settings_detail_event(e);
    }
}
static void open_peak(lv_event_t *e)
{
    (void)e;
    _ui_screen_change(&ui_ScreenPagePeakSettings, LV_SCR_LOAD_ANIM_FADE_ON, 5, 0,
                      ui_ScreenPagePeakSettings_screen_init);
}
void ui_ScreenPageRingSettings_screen_init(void)
{
    ui_ScreenPageRingSettings = lv_obj_create(NULL);
    lv_obj_t *screen = ui_ScreenPageRingSettings;
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    ui_helpers_style_screen_bg(screen);
    lv_obj_t *ring = ui_helpers_create_ring(screen, 10);
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "RING COLORS");
    lv_obj_set_style_text_font(title, &ui_font_FontTypoderSize24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -174);
    s_edit = *nvs_status_ring_get();
    for (unsigned i = 0; i < 5; ++i) {
        int y = -115 + (int)i * 50;
        s_labels[i] = lv_label_create(screen);
        lv_obj_set_style_text_font(s_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_labels[i], lv_color_white(), 0);
        lv_obj_align(s_labels[i], LV_ALIGN_CENTER, 0, y - 22);
        s_sliders[i] = lv_slider_create(screen);
        lv_obj_set_size(s_sliders[i], 228, 10);
        lv_obj_align(s_sliders[i], LV_ALIGN_CENTER, 0, y);
        lv_obj_set_ext_click_area(s_sliders[i], 17);
        lv_obj_set_style_clip_corner(s_sliders[i], true, 0);
        lv_obj_clear_flag(s_sliders[i], LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_set_style_bg_color(s_sliders[i], ui_theme_color_lv(UI_COLOR_ARC_TRACK), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_sliders[i], 255, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_sliders[i], ui_theme_color_lv(UI_COLOR_ARC_INDICATOR), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(s_sliders[i], 255, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s_sliders[i], ui_theme_color_lv(UI_COLOR_ARC_INDICATOR), LV_PART_KNOB);
        lv_obj_set_style_pad_all(s_sliders[i], 5, LV_PART_KNOB);
        ui_helpers_enable_option_feedback(s_sliders[i]);
        lv_obj_add_event_cb(s_sliders[i], change, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(s_sliders[i], change, LV_EVENT_RELEASED, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(s_sliders[i], change, LV_EVENT_PRESS_LOST, (void *)(uintptr_t)i);
    }
    s_hint = lv_label_create(screen);
    lv_label_set_text(s_hint, "");
    lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(s_hint, LV_ALIGN_CENTER, 0, 211);
    lv_obj_t *peak = lv_btn_create(screen);
    lv_obj_set_size(peak, 196, 40);
    lv_obj_align(peak, LV_ALIGN_CENTER, 0, 131);
    lv_obj_set_style_radius(peak, 14, 0);
    lv_obj_set_style_bg_color(peak, lv_color_hex(0x303038), 0);
    lv_obj_clear_flag(peak, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_t *label = lv_label_create(peak);
    lv_label_set_text(label, "PEAK MARK");
    lv_obj_set_style_text_font(label, &ui_font_FontTypoderSize16, 0);
    lv_obj_set_style_text_color(label, ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(peak, open_peak, LV_EVENT_CLICKED, NULL);
    ui_settings_add_back_button(screen, 180);
    refresh();
    lv_obj_add_event_cb(screen, screen_event, LV_EVENT_ALL, NULL);
    lv_obj_move_foreground(ring);
}
#else
void ui_ScreenPageRingSettings_screen_init(void) {}
#endif
