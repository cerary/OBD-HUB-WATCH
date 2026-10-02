#include "../ui.h"
#include "bsp_obd_dsp/nvs_storage.h"
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
#include "stopwatch/stopwatch_board.h"
#endif

static lv_obj_t *s_haptic_button;
static lv_obj_t *s_haptic_text;
static lv_obj_t *s_sound_button;
static lv_obj_t *s_sound_text;
static lv_obj_t *s_volume_slider;
static lv_obj_t *s_volume_label;
static lv_obj_t *s_volume_hint;
static bool s_volume_dirty;

static void volume_label_refresh(void)
{
    lv_label_set_text_fmt(s_volume_label,"VOLUME  %d%%",(int)lv_slider_get_value(s_volume_slider));
}

static void volume_enabled_refresh(bool enabled)
{
    if (enabled) lv_obj_clear_state(s_volume_slider,LV_STATE_DISABLED);
    else lv_obj_add_state(s_volume_slider,LV_STATE_DISABLED);
    lv_obj_set_style_opa(s_volume_slider,enabled ? LV_OPA_COVER : LV_OPA_40,LV_PART_MAIN);
    lv_obj_set_style_text_color(s_volume_label,lv_color_hex(enabled ? 0xD7D8DE : 0x898991),LV_PART_MAIN);
}

static void volume_change(lv_event_t *e)
{
    lv_event_code_t code=lv_event_get_code(e);
    if (code==LV_EVENT_VALUE_CHANGED) {
        s_volume_dirty=true;
        volume_label_refresh();
        lv_label_set_text(s_volume_hint,"");
    } else if ((code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) && s_volume_dirty) {
        s_volume_dirty=false;
        uint8_t percent=(uint8_t)lv_slider_get_value(s_volume_slider);
        if (nvs_sound_volume_set(percent)==ESP_OK) {
            lv_label_set_text(s_volume_hint,"SAVED");
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
            stopwatch_board_feedback(STOPWATCH_FEEDBACK_OPTION);
#endif
        } else {
            lv_slider_set_value(s_volume_slider,nvs_sound_volume_get(),LV_ANIM_OFF);
            volume_label_refresh();
            lv_label_set_text(s_volume_hint,"SAVE FAILED");
        }
    }
}

static void update_switch(lv_obj_t *button, lv_obj_t *text, bool enabled)
{
    lv_obj_set_style_bg_color(button,
        lv_color_hex(enabled ? 0x168B66 : 0x34343A), LV_PART_MAIN);
    lv_label_set_text(text, enabled ? "ON" : "OFF");
}

static void toggle_haptic(lv_event_t *e)
{
    (void)e;
    nvs_user_cfg_t cfg=*nvs_cfg_get();
    cfg.touch_haptic_enabled=!cfg.touch_haptic_enabled;
    if (nvs_cfg_set(&cfg)==ESP_OK)
        update_switch(s_haptic_button,s_haptic_text,cfg.touch_haptic_enabled);
}

static void toggle_sound(lv_event_t *e)
{
    (void)e;
    nvs_user_cfg_t cfg=*nvs_cfg_get();
    cfg.touch_sound_enabled=!cfg.touch_sound_enabled;
    if (nvs_cfg_set(&cfg)==ESP_OK) {
        update_switch(s_sound_button,s_sound_text,cfg.touch_sound_enabled);
        volume_enabled_refresh(cfg.touch_sound_enabled);
    }
}

static void feedback_delete(lv_event_t *e)
{
    (void)e;
    s_haptic_button=s_haptic_text=s_sound_button=s_sound_text=NULL;
    s_volume_slider=s_volume_label=s_volume_hint=NULL;
    s_volume_dirty=false;
    ui_ScreenPageFeedback=NULL;
}

static void add_row(lv_obj_t *screen, const char *name, int y, bool enabled,
                    lv_obj_t **button, lv_obj_t **label, lv_event_cb_t cb)
{
    lv_obj_t *title=lv_label_create(screen);
    lv_label_set_text(title,name);
    lv_obj_set_style_text_font(title,&ui_font_FontTypoderSize20,LV_PART_MAIN);
    lv_obj_set_style_text_color(title,lv_color_hex(0xD7D8DE),LV_PART_MAIN);
    lv_obj_align(title,LV_ALIGN_CENTER,-65,y);
    *button=lv_btn_create(screen);
    lv_obj_set_size(*button,88,46);
    lv_obj_set_style_radius(*button,14,LV_PART_MAIN);
    lv_obj_align(*button,LV_ALIGN_CENTER,90,y);
    *label=lv_label_create(*button);
    lv_obj_set_style_text_font(*label,&ui_font_FontTypoderSize20,LV_PART_MAIN);
    lv_obj_center(*label);
    update_switch(*button,*label,enabled);
    lv_obj_add_event_cb(*button,cb,LV_EVENT_CLICKED,NULL);
}

void ui_ScreenPageFeedback_screen_init(void)
{
    ui_ScreenPageFeedback=lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenPageFeedback,LV_OBJ_FLAG_SCROLLABLE);
    ui_helpers_style_screen_bg(ui_ScreenPageFeedback);
    lv_obj_set_style_border_width(ui_ScreenPageFeedback,0,LV_PART_MAIN);
    lv_obj_t *ring=ui_helpers_create_ring(ui_ScreenPageFeedback,10);
    lv_obj_t *title=lv_label_create(ui_ScreenPageFeedback);
    lv_label_set_text(title,"FEEDBACK");
    lv_obj_set_style_text_font(title,&ui_font_FontTypoderSize24,LV_PART_MAIN);
    lv_obj_set_style_text_color(title,lv_color_white(),LV_PART_MAIN);
    lv_obj_align(title,LV_ALIGN_CENTER,0,-153);
    const nvs_user_cfg_t *cfg=nvs_cfg_get();
    add_row(ui_ScreenPageFeedback,"VIBRATION",-91,cfg->touch_haptic_enabled,
            &s_haptic_button,&s_haptic_text,toggle_haptic);
    add_row(ui_ScreenPageFeedback,"SOUND",-30,cfg->touch_sound_enabled,
            &s_sound_button,&s_sound_text,toggle_sound);
    s_volume_label=lv_label_create(ui_ScreenPageFeedback);
    lv_obj_set_style_text_font(s_volume_label,&ui_font_FontTypoderSize20,LV_PART_MAIN);
    lv_obj_align(s_volume_label,LV_ALIGN_CENTER,0,25);
    s_volume_slider=lv_slider_create(ui_ScreenPageFeedback);
    lv_obj_set_size(s_volume_slider,228,10);
    lv_obj_align(s_volume_slider,LV_ALIGN_CENTER,0,66);
    lv_slider_set_range(s_volume_slider,0,100);
    lv_slider_set_value(s_volume_slider,nvs_sound_volume_get(),LV_ANIM_OFF);
    lv_obj_set_ext_click_area(s_volume_slider,17);
    lv_obj_clear_flag(s_volume_slider,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_style_bg_color(s_volume_slider,ui_theme_color_lv(UI_COLOR_ARC_TRACK),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_volume_slider,LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_volume_slider,ui_theme_color_lv(UI_COLOR_ARC_INDICATOR),LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_volume_slider,LV_OPA_COVER,LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_volume_slider,ui_theme_color_lv(UI_COLOR_ARC_INDICATOR),LV_PART_KNOB);
    lv_obj_set_style_pad_all(s_volume_slider,5,LV_PART_KNOB);
    lv_obj_set_style_clip_corner(s_volume_slider,true,LV_PART_MAIN);
    lv_obj_add_event_cb(s_volume_slider,volume_change,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_add_event_cb(s_volume_slider,volume_change,LV_EVENT_RELEASED,NULL);
    lv_obj_add_event_cb(s_volume_slider,volume_change,LV_EVENT_PRESS_LOST,NULL);
    s_volume_hint=lv_label_create(ui_ScreenPageFeedback);
    lv_label_set_text(s_volume_hint,"");
    lv_obj_set_style_text_font(s_volume_hint,&lv_font_montserrat_14,LV_PART_MAIN);
    lv_obj_set_style_text_color(s_volume_hint,lv_color_hex(0x898991),LV_PART_MAIN);
    lv_obj_align(s_volume_hint,LV_ALIGN_CENTER,0,103);
    s_volume_dirty=false;
    volume_label_refresh();
    volume_enabled_refresh(cfg->touch_sound_enabled);
    lv_obj_t *note=lv_label_create(ui_ScreenPageFeedback);
    lv_label_set_text(note,"0% mute / 100% maximum");
    lv_obj_set_style_text_font(note,&lv_font_montserrat_14,LV_PART_MAIN);
    lv_obj_set_style_text_color(note,lv_color_hex(0x898991),LV_PART_MAIN);
    lv_obj_set_style_text_align(note,LV_TEXT_ALIGN_CENTER,LV_PART_MAIN);
    lv_obj_align(note,LV_ALIGN_CENTER,0,126);
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
    ui_settings_add_back_button(ui_ScreenPageFeedback,166);
#endif
    lv_obj_move_foreground(ring);
    lv_obj_add_event_cb(ui_ScreenPageFeedback,ui_event_feedback_background,LV_EVENT_GESTURE,NULL);
    lv_obj_add_event_cb(ui_ScreenPageFeedback,feedback_delete,LV_EVENT_DELETE,NULL);
}
