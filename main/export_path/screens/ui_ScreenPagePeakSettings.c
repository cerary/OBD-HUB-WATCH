#include "../ui.h"
#include "../ui_peak_marker.h"
#include <string.h>

lv_obj_t *ui_ScreenPagePeakSettings;
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
static lv_obj_t *s_rollers[3], *s_hint;

static void refresh(void)
{
    ui_peak_config_t config = ui_peak_marker_config();
    lv_roller_set_selected(s_rollers[0], config.enabled ? 1 : 0, LV_ANIM_OFF);
    lv_roller_set_selected(s_rollers[1], config.width_px == 10 ? 1 : 0, LV_ANIM_OFF);
    lv_roller_set_selected(s_rollers[2], config.lifetime_s == 10 ? 1 : 0, LV_ANIM_OFF);
}
static void save(lv_event_t *e)
{
    (void)e;
    ui_peak_config_t config = {
        lv_roller_get_selected(s_rollers[0]) ? 1 : 0,
        lv_roller_get_selected(s_rollers[1]) ? 10 : 5,
        lv_roller_get_selected(s_rollers[2]) ? 10 : 5
    };
    lv_label_set_text(s_hint, ui_peak_marker_save_config(&config) ? "SAVED" : "SAVE FAILED");
    ui_peak_marker_tick();
}
static void screen_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        ui_ScreenPagePeakSettings = NULL;
        memset(s_rollers, 0, sizeof(s_rollers)); s_hint = NULL;
    } else if (code == LV_EVENT_SCREEN_LOADED) {
        refresh(); lv_label_set_text(s_hint, "RPM / SPEED");
    } else if (code == LV_EVENT_GESTURE) ui_settings_detail_event(e);
}
void ui_ScreenPagePeakSettings_screen_init(void)
{
    lv_obj_t *screen = ui_ScreenPagePeakSettings = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    ui_helpers_style_screen_bg(screen);
    lv_obj_t *ring = ui_helpers_create_ring(screen, 10);
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "PEAK MARK");
    lv_obj_set_style_text_font(title, &ui_font_FontTypoderSize24, 0);
    lv_obj_set_style_text_color(title, ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -168);
    static const char *names[] = {"ENABLED", "LINE WIDTH", "FADE TIME"};
    static const char *options[] = {"OFF\nON", "5 px\n10 px", "5 sec\n10 sec"};
    for (unsigned i = 0; i < 3; ++i) {
        int y = -74 + (int)i * 78;
        lv_obj_t *label = lv_label_create(screen);
        lv_label_set_text(label, names[i]);
        lv_obj_set_style_text_font(label, &ui_font_FontTypoderSize16, 0);
        lv_obj_set_style_text_color(label, ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY), 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, y - 29);
        lv_obj_t *roller = s_rollers[i] = lv_roller_create(screen);
        lv_roller_set_options(roller, options[i], LV_ROLLER_MODE_NORMAL);
        lv_roller_set_visible_row_count(roller, 1);
        lv_obj_set_size(roller, 172, 30);
        lv_obj_set_ext_click_area(roller, 7);
        lv_obj_set_style_clip_corner(roller, true, 0);
        lv_obj_clear_flag(roller, LV_OBJ_FLAG_GESTURE_BUBBLE);
        ui_helpers_style_dark_roller(roller, &ui_font_FontTypoderSize20);
        lv_obj_align(roller, LV_ALIGN_CENTER, 0, y);
        lv_obj_add_event_cb(roller, save, LV_EVENT_VALUE_CHANGED, NULL);
    }
    s_hint = lv_label_create(screen);
    lv_obj_set_style_text_font(s_hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s_hint, ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_align(s_hint, LV_ALIGN_CENTER, 0, 132);
    lv_label_set_text(s_hint, "RPM / SPEED");
    ui_settings_add_back_button(screen, 178);
    lv_obj_add_event_cb(screen, screen_event, LV_EVENT_ALL, NULL);
    refresh();
    lv_obj_move_foreground(ring);
}
#else
void ui_ScreenPagePeakSettings_screen_init(void) {}
#endif
