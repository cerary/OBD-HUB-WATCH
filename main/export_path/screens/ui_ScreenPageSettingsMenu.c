// StopWatch settings directory. Detail pages retain the original LVGL widgets.
#include "../ui.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/elm327_ble_client.h"
#include "bsp_obd_dsp/espnow_link.h"
#include <string.h>

#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
enum { OPEN_OBD, OPEN_CX, OPEN_RING, OPEN_FEEDBACK, OPEN_BASIC, OPEN_MORE,
       OPEN_SCAN, OPEN_PROTOCOL, OPEN_G_CAL, OPEN_MULTI, OPEN_OTA };
static lv_obj_t *s_summary[6], *s_obd_name, *s_obd_state;
static lv_obj_t *s_rc_button, *s_rc_label;
static bool s_g_cal_from_settings, s_protocol_from_settings;

static void go(lv_obj_t **screen, void (*init)(void))
{
    _ui_screen_change(screen, LV_SCR_LOAD_ANIM_FADE_ON, 5, 0, init);
}

bool ui_settings_handle_back(void)
{
    lv_obj_t *screen = lv_scr_act();
    if (screen == ui_ScreenPageSettings) {
        go(&ui_ScreenPageEasterEgg, ui_ScreenPageEasterEgg_screen_init);
    } else if (screen == ui_ScreenPagePeakSettings) {
        go(&ui_ScreenPageRingSettings, ui_ScreenPageRingSettings_screen_init);
    } else if (screen == ui_ScreenPageBLEScan) {
        ui_ble_scan_stop();
        go(&ui_ScreenPageOBDConnect, ui_ScreenPageOBDConnect_screen_init);
    } else if (screen == ui_ScreenPageODBProtocal) {
        if (s_protocol_from_settings) go(&ui_ScreenPageOBDConnect, ui_ScreenPageOBDConnect_screen_init);
        else go(&ui_ScreenPageTemp, ui_ScreenPageTemp_screen_init);
    } else if (screen == ui_ScreenPageMultiGauge ||
               (screen == ui_ScreenPageGForceCal && s_g_cal_from_settings)) {
        go(&ui_ScreenPageMoreSettings, ui_ScreenPageMoreSettings_screen_init);
    } else if (screen == ui_ScreenPageGForceCal) {
        go(&ui_ScreenPageGForce, ui_ScreenPageGForce_screen_init);
    } else if (screen == ui_ScreenPageBasicSettings || screen == ui_ScreenPageOBDConnect ||
               screen == ui_ScreenPageMoreSettings || screen == ui_ScreenPageFeedback ||
               screen == ui_ScreenPageCxSettings || screen == ui_ScreenPageRingSettings) {
        go(&ui_ScreenPageSettings, ui_ScreenPageSettings_screen_init);
    } else return false;
    return true;
}

void ui_settings_detail_event(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_GESTURE) return;
    // Vertical movement belongs to rollers/lists, never to another settings page.
    if (lv_indev_get_gesture_dir(lv_indev_get_act()) != LV_DIR_RIGHT) return;
    lv_indev_wait_release(lv_indev_get_act());
    ui_settings_handle_back();
}

static void back_clicked(lv_event_t *e)
{
    (void)e;
    ui_settings_handle_back();
}

void ui_settings_add_back_button(lv_obj_t *screen, lv_coord_t y)
{
    lv_obj_t *button = lv_btn_create(screen);
    lv_obj_set_size(button, 136, 44);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, y);
    lv_obj_set_style_radius(button, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x303038), LV_PART_MAIN);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, "BACK");
    lv_obj_set_style_text_font(label, &ui_font_FontTypoderSize16, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, back_clicked, LV_EVENT_CLICKED, NULL);
}

void ui_settings_open_g_calibration(bool from_settings)
{
    s_g_cal_from_settings = from_settings;
    go(&ui_ScreenPageGForceCal, ui_ScreenPageGForceCal_screen_init);
}

static void open_item(lv_event_t *e)
{
    switch ((int)(intptr_t)lv_event_get_user_data(e)) {
    case OPEN_OBD: go(&ui_ScreenPageOBDConnect, ui_ScreenPageOBDConnect_screen_init); break;
    case OPEN_CX: go(&ui_ScreenPageCxSettings, ui_ScreenPageCxSettings_screen_init); break;
    case OPEN_RING: go(&ui_ScreenPageRingSettings, ui_ScreenPageRingSettings_screen_init); break;
    case OPEN_FEEDBACK: go(&ui_ScreenPageFeedback, ui_ScreenPageFeedback_screen_init); break;
    case OPEN_BASIC: go(&ui_ScreenPageBasicSettings, ui_ScreenPageBasicSettings_screen_init); break;
    case OPEN_MORE: go(&ui_ScreenPageMoreSettings, ui_ScreenPageMoreSettings_screen_init); break;
    case OPEN_SCAN: go(&ui_ScreenPageBLEScan, ui_ScreenPageBLEScan_screen_init); break;
    case OPEN_PROTOCOL:
        s_protocol_from_settings = true;
        go(&ui_ScreenPageODBProtocal, ui_ScreenPageODBProtocal_screen_init);
        break;
    case OPEN_G_CAL: ui_settings_open_g_calibration(true); break;
    case OPEN_MULTI: go(&ui_ScreenPageMultiGauge, ui_ScreenPageMultiGauge_screen_init); break;
    case OPEN_OTA: ui_event_easter_egg_ota_button(e); break;
    }
}

static lv_obj_t *text(lv_obj_t *parent, const char *value, int y,
                      const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, value);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, y);
    return label;
}

static lv_obj_t *make_screen(const char *title)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    ui_helpers_style_screen_bg(screen);
    ui_helpers_create_ring(screen, 10);
    text(screen, title, -158, &ui_font_FontTypoderSize24,
         ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY));
    lv_obj_add_event_cb(screen, ui_settings_detail_event, LV_EVENT_GESTURE, NULL);
    return screen;
}

static void set_text(lv_obj_t *label, const char *value)
{
    if (label && strcmp(lv_label_get_text(label), value)) lv_label_set_text(label, value);
}

void ui_settings_refresh(void)
{
    lv_obj_t *active = lv_scr_act();
    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    bool slave = cfg->device_role == ESPNOW_ROLE_SLAVE;
    bool connected = slave ? espnow_link_slave_has_data() : elm327_ble_is_connected();
    const char *link = !slave && elm327_ble_cx_is_asleep() ? "CX asleep" :
                       !slave && elm327_ble_cx_is_waiting() ? "Waiting CX sleep" :
                       connected ? "Connected" : "Disconnected";
    if (active == ui_ScreenPageSettings) {
        set_text(s_summary[OPEN_OBD], link);
        set_text(s_summary[OPEN_CX], elm327_ble_cx_enabled() ? "Linked / ON" : "OFF");
        char value[32];
        const status_ring_config_t *ring = nvs_status_ring_get();
        lv_snprintf(value, sizeof(value), "%u / %u", ring->rpm_green, ring->rpm_red);
        set_text(s_summary[OPEN_RING], value);
        lv_snprintf(value, sizeof(value), "VIB %s / SND %s",
                    cfg->touch_haptic_enabled ? "ON" : "OFF", cfg->touch_sound_enabled ? "ON" : "OFF");
        set_text(s_summary[OPEN_FEEDBACK], value);
    } else if (active == ui_ScreenPageOBDConnect) {
        set_text(s_obd_name, cfg->ble_device_name[0] ? cfg->ble_device_name : "No saved device");
        set_text(s_obd_state, link);
    } else if (active == ui_ScreenPageMoreSettings && s_rc_label) {
        bool enabled = cfg->rc_enabled;
        set_text(s_rc_label, enabled ? "ON" : "OFF");
        lv_obj_set_style_bg_color(s_rc_button, lv_color_hex(enabled ? 0x00AA55 : 0x333333), 0);
    }
}

static void screen_event(lv_event_t *e)
{
    lv_obj_t *screen = lv_event_get_target(e);
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) ui_settings_refresh();
    else if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        if (screen == ui_ScreenPageSettings) {
            ui_ScreenPageSettings = NULL;
            memset(s_summary, 0, sizeof(s_summary));
        } else if (screen == ui_ScreenPageOBDConnect) {
            ui_ScreenPageOBDConnect = NULL; s_obd_name = s_obd_state = NULL;
        } else if (screen == ui_ScreenPageMoreSettings) {
            ui_ScreenPageMoreSettings = NULL; s_rc_button = s_rc_label = NULL;
        }
    }
}

void ui_ScreenPageSettings_screen_init(void)
{
    static const char *names[] = {"OBD CONNECT", "CX STANDBY", "RING COLORS", "FEEDBACK", "BASIC", "MORE"};
    static const char *icons[] = {LV_SYMBOL_BLUETOOTH, LV_SYMBOL_POWER, LV_SYMBOL_LOOP,
                                 LV_SYMBOL_VOLUME_MID, LV_SYMBOL_SETTINGS, LV_SYMBOL_LIST};
    ui_ScreenPageSettings = make_screen("SETTINGS");
    lv_obj_t *screen = ui_ScreenPageSettings;
    text(screen, "WATCH / CONNECTION / DISPLAY", -126, &lv_font_montserrat_12,
         ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY));
    for (int i = 0; i < 6; ++i) {
        lv_obj_t *button = lv_btn_create(screen);
        lv_obj_set_size(button, 148, 76);
        lv_obj_align(button, LV_ALIGN_CENTER, i % 2 ? 82 : -82, -69 + (i / 2) * 85);
        lv_obj_set_style_radius(button, 14, 0);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x222228), 0);
        lv_obj_set_style_pad_all(button, 0, 0);
        lv_obj_clear_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
        text(button, icons[i], -22, &lv_font_montserrat_16, ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY));
        text(button, names[i], 0, &lv_font_montserrat_14, ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY));
        s_summary[i] = text(button, i == OPEN_BASIC ? "Display / boot" : i == OPEN_MORE ? "Tools / OTA" : "",
                            24, &lv_font_montserrat_12, ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY));
        lv_obj_add_event_cb(button, open_item, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
    ui_settings_add_back_button(screen, 174);
    lv_obj_add_event_cb(screen, screen_event, LV_EVENT_ALL, NULL);
}

static void action_button(lv_obj_t *screen, const char *name, int y, int id)
{
    lv_obj_t *button = lv_btn_create(screen);
    lv_obj_set_size(button, 240, 46);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, y);
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x303038), 0);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    text(button, name, 0, &ui_font_FontTypoderSize16, ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY));
    lv_obj_add_event_cb(button, open_item, LV_EVENT_CLICKED, (void *)(intptr_t)id);
}

void ui_ScreenPageOBDConnect_screen_init(void)
{
    bool slave = nvs_cfg_get()->device_role == ESPNOW_ROLE_SLAVE;
    ui_ScreenPageOBDConnect = make_screen(slave ? "MASTER CONNECT" : "OBD CONNECT");
    lv_obj_t *screen = ui_ScreenPageOBDConnect;
    s_obd_name = text(screen, "", -87, &ui_font_FontTypoderSize20, ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY));
    lv_obj_set_width(s_obd_name, 300);
    lv_obj_set_style_text_align(s_obd_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_obd_name, LV_LABEL_LONG_DOT);
    s_obd_state = text(screen, "", -43, &ui_font_FontTypoderSize16, ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY));
    action_button(screen, slave ? "SCAN FOR MASTER" : "SCAN & SELECT", 30, OPEN_SCAN);
    if (!slave) action_button(screen, "PROTOCOL", 92, OPEN_PROTOCOL);
    ui_settings_add_back_button(screen, 166);
    lv_obj_add_event_cb(screen, screen_event, LV_EVENT_ALL, NULL);
}

static void rc_toggle(lv_event_t *e)
{
    (void)e;
    nvs_user_cfg_t cfg = *nvs_cfg_get();
    cfg.rc_enabled = !cfg.rc_enabled;
    nvs_cfg_set(&cfg);
    ui_settings_refresh();
}

void ui_ScreenPageMoreSettings_screen_init(void)
{
    ui_ScreenPageMoreSettings = make_screen("MORE");
    lv_obj_t *screen = ui_ScreenPageMoreSettings;
    action_button(screen, "G CALIBRATION", -78, OPEN_G_CAL);
    action_button(screen, "MULTI-GAUGE", -16, OPEN_MULTI);
    action_button(screen, "FIRMWARE / OTA", 46, OPEN_OTA);
    lv_obj_t *rc_name = text(screen, "RACECHRONO", 110, &ui_font_FontTypoderSize16,
                             ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY));
    lv_obj_align(rc_name, LV_ALIGN_CENTER, -50, 110);
    s_rc_button = lv_btn_create(screen);
    lv_obj_set_size(s_rc_button, 76, 44);
    lv_obj_align(s_rc_button, LV_ALIGN_CENTER, 94, 110);
    lv_obj_set_style_radius(s_rc_button, 13, 0);
    lv_obj_clear_flag(s_rc_button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    s_rc_label = text(s_rc_button, "", 0, &ui_font_FontTypoderSize16, lv_color_white());
    lv_obj_add_event_cb(s_rc_button, rc_toggle, LV_EVENT_CLICKED, NULL);
    ui_settings_add_back_button(screen, 174);
    lv_obj_add_event_cb(screen, screen_event, LV_EVENT_ALL, NULL);
}
#endif
