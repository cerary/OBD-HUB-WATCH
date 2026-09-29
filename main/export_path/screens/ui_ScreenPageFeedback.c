#include "../ui.h"
#include "bsp_obd_dsp/nvs_storage.h"

static lv_obj_t *s_haptic_button;
static lv_obj_t *s_haptic_text;
static lv_obj_t *s_sound_button;
static lv_obj_t *s_sound_text;

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
    if (nvs_cfg_set(&cfg)==ESP_OK)
        update_switch(s_sound_button,s_sound_text,cfg.touch_sound_enabled);
}

static void feedback_delete(lv_event_t *e)
{
    (void)e;
    s_haptic_button=s_haptic_text=s_sound_button=s_sound_text=NULL;
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
    lv_obj_align(title,LV_ALIGN_CENTER,0,-137);
    const nvs_user_cfg_t *cfg=nvs_cfg_get();
    add_row(ui_ScreenPageFeedback,"VIBRATION",-45,cfg->touch_haptic_enabled,
            &s_haptic_button,&s_haptic_text,toggle_haptic);
    add_row(ui_ScreenPageFeedback,"SOUND",30,cfg->touch_sound_enabled,
            &s_sound_button,&s_sound_text,toggle_sound);
    lv_obj_t *note=lv_label_create(ui_ScreenPageFeedback);
    lv_label_set_text(note,"Page + selection feedback\nSound defaults to OFF");
    lv_obj_set_style_text_font(note,&ui_font_FontTypoderSize16,LV_PART_MAIN);
    lv_obj_set_style_text_color(note,lv_color_hex(0x898991),LV_PART_MAIN);
    lv_obj_set_style_text_align(note,LV_TEXT_ALIGN_CENTER,LV_PART_MAIN);
    lv_obj_align(note,LV_ALIGN_CENTER,0,104);
    lv_obj_t *hint=lv_label_create(ui_ScreenPageFeedback);
    lv_label_set_text(hint,"Swipe down to return");
    lv_obj_set_style_text_font(hint,&ui_font_FontTypoderSize16,LV_PART_MAIN);
    lv_obj_set_style_text_color(hint,lv_color_hex(0x686870),LV_PART_MAIN);
    lv_obj_align(hint,LV_ALIGN_CENTER,0,164);
    lv_obj_move_foreground(ring);
    lv_obj_add_event_cb(ui_ScreenPageFeedback,ui_event_feedback_background,LV_EVENT_GESTURE,NULL);
    lv_obj_add_event_cb(ui_ScreenPageFeedback,feedback_delete,LV_EVENT_DELETE,NULL);
}
