#include "host_shim.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "esp_timer.h"
#include "ui_ext.h"
#include "bsp_obd_dsp/elm327_ble_client.h"
#include "bsp_obd_dsp/gauge_pair_ble_client.h"
#include "bsp_obd_dsp/espnow_link.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>

enum { W=466, H=466 };
static lv_color_t pixels[W*H], draw_pixels[W*32];
static bool connected, waiting, sleeping, demo, sweep;
static nvs_user_cfg_t user_cfg;
static status_ring_config_t ring_cfg;
static int16_t alarms[RING_ITEM_COUNT];
static int saves;
static int cfg_saves, scans, scan_stops, shutdowns, cx_reads, cx_resumes;
static bool linked = true;
static float imu_x,imu_y,imu_z=1.f;
static unsigned imu_reads;
static lv_dir_t gesture_direction;
static ble_scan_found_cb_t scan_cb;
static uint8_t master_mac[6];
void *lvgl_mux;
uint8_t ui_theme_gauge_page_index;
void host_update(uint16_t rpm);
void host_speed(uint16_t speed);
void host_gear(enGear gear,bool unknown);
void host_info(bool is_slave,bool ble_now);
const nvs_user_cfg_t *nvs_cfg_get(void) {return &user_cfg;}
esp_err_t nvs_cfg_set(const nvs_user_cfg_t *c) {user_cfg=*c; ++cfg_saves; return ESP_OK;}
const status_ring_config_t *nvs_status_ring_get(void) {return &ring_cfg;}
esp_err_t nvs_status_ring_set(const status_ring_config_t *c) {assert(status_ring_config_valid(c));ring_cfg=*c;++saves;return ESP_OK;}
int16_t nvs_chart_alarm_get(uint8_t i) {return alarms[i];}
void nvs_chart_alarm_set(uint8_t i,int16_t raw) {alarms[i]=raw;}
void nvs_stat_update_speed(uint8_t speed, uint32_t dt) {(void)speed;(void)dt;}
bool elm327_ble_is_connected(void) {return connected;}
bool elm327_ble_cx_is_waiting(void) {return waiting || sleeping;}
bool elm327_ble_cx_is_asleep(void) {return sleeping;}
bool espnow_link_slave_has_data(void) {return false;}
bool ui_ext_showroom_is_active(void) {return demo;}
bool ui_ext_sweep_active(void) {return sweep;}
int64_t esp_timer_get_time(void) {return (int64_t)lv_tick_get()*1000;}
uint32_t xTaskGetTickCount(void) {return lv_tick_get();}
void esp_restart(void) {assert(!"Unexpected restart");}
void Set_Backlight(uint8_t n) {(void)n;}
void stopwatch_board_feedback(stopwatch_feedback_t n) {(void)n;}
void ui_event_logo_background(lv_event_t *e) {(void)e;}

int xSemaphoreTake(void *s,int timeout) {(void)s;(void)timeout;return 1;}
int xSemaphoreGive(void *s) {(void)s;return 1;}
static uint8_t battery_level=82;
static bool supply_external=true, power_read_ok=true;
static unsigned power_reads;
bool stopwatch_board_power_status(uint8_t *percent,bool *power) {++power_reads;*percent=battery_level;*power=supply_external;return power_read_ok;}
const char *elm327_ble_get_connected_name(void) {return "OBDLink CX";}
const char *espnow_link_get_master_name(void) {return OBD_MASTER_DEVICE_PREFIX;}
bool stopwatch_board_shutdown(void) {++shutdowns;return true;}
bool stopwatch_board_imu_init(void) {return true;}
bool stopwatch_board_imu_read(float *x,float *y,float *z) {++imu_reads;*x=imu_x;*y=imu_y;*z=imu_z;return true;}
bool elm327_ble_cx_enabled(void) {return linked;}
bool elm327_ble_cx_set_enabled(bool enabled) {linked=enabled;return true;}
const char *elm327_ble_cx_status(void) {return "Ready - CX sleep linked";}
void elm327_ble_cx_read_config(void) {++cx_reads;}
void elm327_ble_cx_manual_resume(void) {++cx_resumes;}
bool elm327_ble_cx_power_shutdown_due(bool power) {(void)power;return false;}
void elm327_ble_scan_only_start(int duration,ble_scan_found_cb_t cb) {(void)duration;scan_cb=cb;++scans;}
void elm327_ble_scan_only_stop(void) {++scan_stops;}
void elm327_ble_connect_by_addr(const uint8_t *mac,const char *name) {(void)mac;(void)name;}
void elm327_ble_disconnect(void) {connected=false;}
void gauge_pair_ble_scan_start(int duration,gauge_pair_scan_cb_t cb) {(void)duration;(void)cb;++scans;}
void gauge_pair_ble_scan_stop(void) {++scan_stops;}
void gauge_pair_ble_connect(const uint8_t *mac,const char *name,gauge_pair_result_cb_t cb) {(void)mac;(void)name;(void)cb;}
const uint8_t *espnow_link_get_bound_master_mac(void) {return master_mac;}
void espnow_link_bind_master(const uint8_t *mac) {memcpy(master_mac,mac,6);}
void espnow_link_unbind_master(void) {memset(master_mac,0,6);}
uint8_t nvs_intro_enable_get(void) {return 0;}
uint8_t nvs_device_position_get(void) {return 1;}
void nvs_intro_enable_set(uint8_t v) {(void)v;}
void nvs_device_position_set(uint8_t v) {(void)v;}
void ui_ext_tick(void) {}
void ui_ext_showroom_handle_tap(void) {}
static int theme_pages;
int theme_page_list_count(void) {return theme_pages;}
bool theme_has_page(const char *name) {(void)name;return theme_pages>0;}
#define STUB_SCREEN(name) void ui_ScreenPage##name##_screen_init(void) {ui_ScreenPage##name=lv_obj_create(NULL);}
STUB_SCREEN(ThemeGauge)

STUB_SCREEN(TempCustom) STUB_SCREEN(InfoCustom)
STUB_SCREEN(ChartConfig) STUB_SCREEN(OilWarn) STUB_SCREEN(RpmWarn)
void ui_event_easter_egg_ota_button(lv_event_t *e) {(void)e;ui_ScreenPageOTAMode=lv_obj_create(NULL);lv_scr_load(ui_ScreenPageOTAMode);}
lv_indev_t *__wrap_lv_indev_get_act(void) {return NULL;}
lv_dir_t __wrap_lv_indev_get_gesture_dir(const lv_indev_t *indev) {(void)indev;return gesture_direction;}
void __wrap_lv_indev_wait_release(lv_indev_t *indev) {(void)indev;}
static void gesture(lv_dir_t dir) {gesture_direction=dir;lv_event_send(lv_scr_act(),LV_EVENT_GESTURE,NULL);}
static uint8_t audit_theme_index;
const ui_theme_t *ui_theme_active(void) {return g_ui_themes[audit_theme_index];}
uint32_t ui_theme_color(ui_color_role_t r) {return ui_theme_active()->colors[r];}
lv_color_t ui_theme_color_lv(ui_color_role_t r) {return lv_color_hex(ui_theme_color(r));}
uint8_t ui_theme_count(void) {return 1;}
const char *ui_theme_names_joined(void) {return "DEFAULT";}
void ui_theme_set_active(uint8_t n) {(void)n;}
size_t strlcat(char *dst,const char *src,size_t size)
{size_t d=strlen(dst),s=strlen(src);if(d<size-1)strncat(dst,src,size-d-1);return d+s;}

static void flush(lv_disp_drv_t *drv,const lv_area_t *a,lv_color_t *c)
{
    for(int y=a->y1;y<=a->y2;++y)for(int x=a->x1;x<=a->x2;++x)pixels[y*W+x]=*c++;
    lv_disp_flush_ready(drv);
}
static void render(const char *folder,const char *name)
{
    lv_tick_inc(100);ui_status_ring_tick();ui_peak_marker_tick();lv_obj_update_layout(lv_scr_act());
    lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL); // deterministic full frame after rapid test-only page changes
    char file[1024];snprintf(file,sizeof(file),"%s/%s.ppm",folder,name);
    FILE *f=fopen(file,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",W,H);
    for(int i=0;i<W*H;++i){lv_color32_t c;c.full=lv_color_to32(pixels[i]);unsigned char rgb[]={c.ch.red,c.ch.green,c.ch.blue};fwrite(rgb,1,3,f);}
    fclose(f);
}

void __real_free(void *p); // The linker wrapper uses the real C allocator below.
void __wrap_free(void *p) { __real_free(p); }
void *host_power_alloc(size_t n) { return malloc(n); }
void *host_g_alloc(size_t n) { return malloc(n); }
static void seed(void) {
    obd_data_set_rpm(3000);obd_data_set_speed(80);obd_data_set_coolant_temp(92);
    obd_data_set_intake_temp(31);obd_data_set_oil_temp(101);obd_data_set_load_pct(42);
    obd_data_set_tps(25);obd_data_set_bat_mv(13900);obd_data_set_boost_x10(6);obd_data_set_afr_x100(1470);
}

static unsigned entry_timer_runs;
static void entry_timer_cb(lv_timer_t *timer) {(void)timer;++entry_timer_runs;}
static void assert_missing(lv_obj_t *screen,lv_obj_t *label)
{
    lv_scr_load(screen);
    assert(strcmp(lv_label_get_text(label),"--")==0);
}
static void test_data_entry(void)
{
    sleeping=false;sweep=false;demo=false;connected=true;
    obd_data_invalidate_freshness();
    assert_missing(ui_ScreenPageRpm,ui_RpmPageArcLabelRpmText);
    assert_missing(ui_ScreenPageSpeed,ui_SpeedPageArcLabelSpeedText);
    assert_missing(ui_ScreenPageGear,ui_GearPageArcLabelGearNumText);
    assert_missing(ui_ScreenPageTemp,ui_LabelTempValue[0]);
    assert_missing(ui_ScreenPageInfo,ui_LabelInfoValue[0]);
    assert_missing(ui_ScreenPageNeedle,ui_NeedleValueLabel);
    assert_missing(ui_ScreenPageOilPressure,ui_LabelOilPressureText);

    // Entry wakes the production UI timer, without waiting out its old period.
    lv_timer_t *timer=lv_timer_create(entry_timer_cb,1000,NULL);
    ui_data_entry_bind_timer(timer);lv_timer_reset(timer);
    lv_scr_load(ui_ScreenPageRpm);lv_timer_handler();
    assert(entry_timer_runs==1);

    // Real zero remains distinct from missing data, including neutral gear.
    obd_data_set_rpm(0);obd_data_set_speed(0);obd_data_set_gear(0);
    lv_scr_load(ui_ScreenPageSpeed);lv_scr_load(ui_ScreenPageRpm);
    assert(strcmp(lv_label_get_text(ui_RpmPageArcLabelRpmText),"0")==0);
    host_update(0);
    lv_scr_load(ui_ScreenPageSpeed);
    assert(strcmp(lv_label_get_text(ui_SpeedPageArcLabelSpeedText),"0")==0);
    host_speed(0);
    lv_scr_load(ui_ScreenPageGear);
    assert(strcmp(lv_label_get_text(ui_GearPageArcLabelGearNumText),"N")==0);
    host_gear(GEAR_NEUTRAL,false);

    lv_tick_inc(500);seed();obd_data_set_gear(4);
    uint16_t cached_speed=obd_data_get_speed();
    char speed_text[16];snprintf(speed_text,sizeof(speed_text),"%u",cached_speed);
    assert(cached_speed>0); // the cache's existing road-speed smoothing still applies
    lv_scr_load(ui_ScreenPageRpm);
    assert(strcmp(lv_label_get_text(ui_RpmPageArcLabelRpmText),"3000")==0);
    host_update(3000);
    lv_scr_load(ui_ScreenPageSpeed);
    assert(strcmp(lv_label_get_text(ui_SpeedPageArcLabelSpeedText),speed_text)==0);
    host_speed(cached_speed);
    assert(strcmp(lv_label_get_text(ui_SpeedPageArcLabelSpeedText),speed_text)==0);
    lv_scr_load(ui_ScreenPageGear);
    assert(strcmp(lv_label_get_text(ui_GearPageArcLabelGearNumText),"4")==0);
    host_gear(GEAR_4,false);

    // Re-entering with an unchanged sample must still refresh a new/reset label.
    lv_scr_load(ui_ScreenPageSpeed);lv_scr_load(ui_ScreenPageRpm);
    lv_label_set_text(ui_RpmPageArcLabelRpmText,"wrong");host_update(3000);
    assert(strcmp(lv_label_get_text(ui_RpmPageArcLabelRpmText),"3000")==0);
    lv_scr_load(ui_ScreenPageGear);lv_label_set_text(ui_GearPageArcLabelGearNumText,"wrong");
    host_gear(GEAR_4,false);
    assert(strcmp(lv_label_get_text(ui_GearPageArcLabelGearNumText),"4")==0);

    // Age and connection state are checked before showing the very first frame.
    lv_tick_inc(16001);
    assert_missing(ui_ScreenPageRpm,ui_RpmPageArcLabelRpmText);
    assert_missing(ui_ScreenPageTemp,ui_LabelTempValue[0]);
    seed();connected=false;
    assert_missing(ui_ScreenPageRpm,ui_RpmPageArcLabelRpmText);
    assert_missing(ui_ScreenPageSpeed,ui_SpeedPageArcLabelSpeedText);
    assert_missing(ui_ScreenPageGear,ui_GearPageArcLabelGearNumText);
    assert_missing(ui_ScreenPageInfo,ui_LabelInfoValue[0]);

    // Missing-to-valid recovery shows the real first sample without a fake ramp.
    int32_t displayed=800;
    lv_label_set_text(ui_LabelTempValue[0],"--");
    disp_item_update(&displayed,ui_LabelTempValue[0],DISP_ITEM_CLT,92,true,5);
    assert(displayed==92 && strcmp(lv_label_get_text(ui_LabelTempValue[0]),"92")==0);
    disp_item_update(&displayed,ui_LabelTempValue[0],DISP_ITEM_CLT,93,true,5);
    assert(displayed==93);

    // The explicit sweep animation continues to own its test readings.
    connected=true;sweep=true;
    lv_label_set_text(ui_RpmPageArcLabelRpmText,"1234");
    lv_scr_load(ui_ScreenPageRpm);
    assert(strcmp(lv_label_get_text(ui_RpmPageArcLabelRpmText),"1234")==0);
    host_update(4000);
    assert(strcmp(lv_label_get_text(ui_RpmPageArcLabelRpmText),"4000")==0);
    sweep=false;ui_data_entry_bind_timer(NULL);lv_timer_del(timer);
    puts("PASS data entry: initial missing, valid zero/neutral, fresh cached values, same-value re-entry, stale/disconnected, timer wake, recovery and sweep");
}
static void frame(const char *folder,const char *name) {seed();render(folder,name);}

static void select_jcw(void)
{
    uint8_t count;
    const vehicle_profile_t *profiles=vehicle_profile_get_all(&count);
    for (uint8_t i=0;i<count;++i) {
        if (strcmp(profiles[i].name,"JCW F56 8AT")==0) {
            vehicle_profile_set_active(i);return;
        }
    }
    assert(!"JCW profile missing");
}

static char tick_labels[5][32];
static void capture_tick_label(lv_event_t *event)
{
    lv_obj_draw_part_dsc_t *part=lv_event_get_draw_part_dsc(event);
    if (part->type==LV_METER_DRAW_PART_TICK && part->text && part->id%5==0)
        snprintf(tick_labels[part->id/5],32,"%s",part->text);
}

static void test_display_ranges(const char *folder)
{
    connected=true;sleeping=false;sweep=false;demo=false;
    assert(ui_disp_item_scale(DISP_ITEM_RPM)->nmax==7000);
    assert(ui_disp_item_scale(DISP_ITEM_SPEED)->nmax==280);
    assert(disp_item_sweep_value(DISP_ITEM_RPM,1.f)==7000);
    assert(disp_item_sweep_value(DISP_ITEM_SPEED,1.f)==280);
    assert(ui_disp_item_arc_percent(DISP_ITEM_SPEED,246)==87);
    assert(ui_disp_item_arc_percent(DISP_ITEM_SPEED,280)==100);
    assert(ui_disp_item_arc_percent(DISP_ITEM_SPEED,300)==100);
    assert(ui_disp_item_arc_percent(DISP_ITEM_RPM,-1)==0);
    seed();obd_data_set_rpm(7000);lv_scr_load(ui_ScreenPageSpeed);lv_scr_load(ui_ScreenPageRpm);
    assert(lv_arc_get_value(ui_RpmPageArcRpmBack)==100);
    host_update(3500);assert(lv_arc_get_value(ui_RpmPageArcRpmBack)==50);
    lv_tick_inc(300);obd_data_set_speed(246); // advance the real cache's time-based filter
    lv_scr_load(ui_ScreenPageSpeed);
    assert(lv_arc_get_value(ui_SpeedPageArcSpeedBack)==87);
    host_speed(246);assert(lv_arc_get_value(ui_SpeedPageArcSpeedBack)==87);
    render(folder,"speed-246");

    // Inspect actual LVGL draw callbacks, not a duplicate tick-format implementation.
    user_cfg.needle_source_idx=DISP_ITEM_BOOST;ui_needle_apply_source();
    assert(ui_NeedleScale->min==0 && ui_NeedleScale->max==20);
    obd_data_set_boost_x10(12);lv_scr_load(ui_ScreenPageNeedle);
    assert(strcmp(lv_label_get_text(ui_NeedleValueLabel),"1.2")==0);
    assert(ui_NeedleIndic->start_value==12);
    lv_obj_add_event_cb(ui_NeedleMeter,capture_tick_label,LV_EVENT_DRAW_PART_BEGIN,NULL);
    render(folder,"needle-boost");
    const char *expected[]={"0.0","0.5","1.0","1.5","2.0"};
    for(int i=0;i<5;++i)assert(strcmp(tick_labels[i],expected[i])==0);

    user_cfg.chart_source_idx=DISP_ITEM_BOOST;ui_chart_apply_source();
    assert(((lv_chart_t*)ui_ChartOilPressure)->ymax[0]==20);
    alarms[DISP_ITEM_BOOST]=12;ui_ScreenPageChartAlarm_screen_init();
    lv_scr_load(ui_ScreenPageChartAlarm);
    lv_obj_t *slider=NULL,*label=NULL;
    for(unsigned i=0;i<lv_obj_get_child_cnt(ui_ScreenPageChartAlarm);++i) {
        lv_obj_t *child=lv_obj_get_child(ui_ScreenPageChartAlarm,i);
        if(lv_obj_check_type(child,&lv_slider_class))slider=child;
        if(lv_obj_check_type(child,&lv_label_class) && strcmp(lv_label_get_text(child),"1.2 bar")==0)label=child;
    }
    assert(slider && label && lv_slider_get_value(slider)==12);
    lv_slider_set_value(slider,15,LV_ANIM_OFF);lv_event_send(slider,LV_EVENT_VALUE_CHANGED,NULL);
    assert(alarms[DISP_ITEM_BOOST]==15 && strcmp(lv_label_get_text(label),"1.5 bar")==0);
    render(folder,"boost-alarm");
    lv_slider_set_value(slider,21,LV_ANIM_OFF);lv_event_send(slider,LV_EVENT_VALUE_CHANGED,NULL);
    assert(alarms[DISP_ITEM_BOOST]==INT16_MAX && strcmp(lv_label_get_text(label),"OFF")==0);

    // Fractional readings must retain needle precision on entry and normal updates.
    user_cfg.needle_source_idx=DISP_ITEM_BAT;ui_needle_apply_source();obd_data_set_bat_mv(14400);
    lv_scr_load(ui_ScreenPageNeedle);
    assert(ui_NeedleScale->min==8000 && ui_NeedleScale->max==16000);
    assert(ui_NeedleIndic->start_value==14400);
    ui_needle_page_update(-1.f,-40,-40,-41,-1,-1,14400,-1,-1001,UINT16_MAX,UINT16_MAX,INT16_MIN,-1);
    assert(ui_NeedleIndic->start_value==14400 && strcmp(lv_label_get_text(ui_NeedleValueLabel),"14.4")==0);
    render(folder,"needle-voltage");
    user_cfg.needle_source_idx=DISP_ITEM_AFR;ui_needle_apply_source();obd_data_set_afr_x100(1470);
    lv_scr_load(ui_ScreenPageRpm);lv_scr_load(ui_ScreenPageNeedle);
    assert(ui_NeedleIndic->start_value==1470 && strcmp(lv_label_get_text(ui_NeedleValueLabel),"14.7")==0);
    ui_needle_page_update(.5f,-40,-40,-41,-1,-1,0,-1,-1001,UINT16_MAX,UINT16_MAX,INT16_MIN,-1);
    assert(ui_NeedleIndic->start_value==1500 && strcmp(lv_label_get_text(ui_NeedleValueLabel),"15.0")==0);
    ui_needle_page_update(-1.f,-40,-40,-41,-1,-1,0,-1,-1001,UINT16_MAX,UINT16_MAX,INT16_MIN,-1);
    assert(ui_NeedleIndic->start_value==800 && strcmp(lv_label_get_text(ui_NeedleValueLabel),"--")==0);
    user_cfg.needle_source_idx=DISP_ITEM_SPEED;ui_needle_apply_source();
    user_cfg.chart_source_idx=DISP_ITEM_CLT;ui_chart_apply_source();
    puts("PASS vehicle ranges: entry/update/arc/sweep, actual decimal ticks, raw chart/alarm units, fractional pointer precision and missing data");
}
int main(int argc,char **argv) {
    assert(argc==2);const char *folder=argv[1];lv_init();lv_tick_inc(1001);
    for(int i=0;i<RING_ITEM_COUNT;++i)alarms[i]=INT16_MAX;
    ring_cfg=status_ring_default_config();user_cfg.brightness_day=100;user_cfg.device_role=ESPNOW_ROLE_STANDALONE;
    strcpy(user_cfg.ble_device_name,"OBDLink CX");
    vehicle_profile_set_active(0);
    assert(ui_disp_item_scale(DISP_ITEM_RPM)->nmax==8000);
    assert(ui_disp_item_scale(DISP_ITEM_SPEED)->nmax==240);
    select_jcw();
    uint8_t tmap[3]={0,1,2}, imap[5]={0,2,3,4,1};
    memcpy(user_cfg.temp_display_map,tmap,3);memcpy(user_cfg.info_display_map,imap,5);
    user_cfg.needle_source_idx=6;user_cfg.chart_source_idx=0;
    user_cfg.g_cal_valid=1;user_cfg.g_zero[2]=1.f;user_cfg.g_axis_right[0]=1.f;user_cfg.g_axis_forward[1]=1.f;
    battery_level=97;connected=true;seed();
    lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,draw_pixels,NULL,W*32);
    lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=W;drv.ver_res=H;drv.draw_buf=&buf;drv.flush_cb=flush;
    lv_disp_t *disp=lv_disp_drv_register(&drv);
    lv_theme_default_init(disp,lv_palette_main(LV_PALETTE_BLUE),lv_palette_main(LV_PALETTE_RED),false,&lv_font_montserrat_16);
    ui_ScreenPageRpm_screen_init();connected=false;lv_scr_load(ui_ScreenPageRpm);host_update(UINT16_MAX);render(folder,"rpm-connecting");connected=true;
    ui_ScreenPageEasterEgg_screen_init();lv_scr_load(ui_ScreenPageEasterEgg);host_info(false,true);frame(folder,"status");
    ui_ScreenPageGear_screen_init();lv_scr_load(ui_ScreenPageGear);lv_label_set_text(ui_GearPageArcLabelGearNumText,"4");lv_arc_set_value(ui_GearPageArcGearNumBack,50);frame(folder,"gear");
    lv_scr_load(ui_ScreenPageRpm);host_update(3000);frame(folder,"rpm");
    ui_ScreenPageSpeed_screen_init();lv_scr_load(ui_ScreenPageSpeed);for(int i=0;i<40;++i)host_speed(80);frame(folder,"speed");
    ui_ScreenPageTemp_screen_init();lv_scr_load(ui_ScreenPageTemp);
    int tv[3]={92,31,101};for(int i=0;i<3;++i)disp_item_set_text(ui_LabelTempValue[i],tmap[i],tv[i],true);frame(folder,"temperature");
    ui_ScreenPageInfo_screen_init();lv_scr_load(ui_ScreenPageInfo);
    int iv[5]={92,101,42,25,31};for(int i=0;i<5;++i)disp_item_set_text(ui_LabelInfoValue[i],imap[i],iv[i],true);frame(folder,"info");
    ui_ScreenPageGForce_screen_init();lv_scr_load(ui_ScreenPageGForce);imu_y=-.60f;imu_x=-.28f;
    for(int i=0;i<30;++i){lv_tick_inc(80);lv_timer_handler();}frame(folder,"g-force");
    ui_ScreenPageNeedle_screen_init();lv_scr_load(ui_ScreenPageNeedle);
    disp_item_set_text(ui_NeedleValueLabel,DISP_ITEM_SPEED,80,true);lv_meter_set_indicator_value(ui_NeedleMeter,ui_NeedleIndic,80);frame(folder,"needle");
    ui_ScreenPageOilPressure_screen_init();lv_scr_load(ui_ScreenPageOilPressure);disp_item_set_text(ui_LabelOilPressureText,DISP_ITEM_CLT,92,true);
    unsigned count=lv_chart_get_point_count(ui_ChartOilPressure);
    for(unsigned i=0;i<count;++i)lv_chart_set_value_by_id(ui_ChartOilPressure,ui_OilPressureChartSeries,i,(int)(82+10.f*i/count+1.2f*sinf(i*.35f)));
    lv_chart_refresh(ui_ChartOilPressure);frame(folder,"chart");
    imu_x=imu_y=0;ui_ScreenPageExpression_screen_init();lv_scr_load(ui_ScreenPageExpression);
    for(int i=0;i<10;++i){lv_tick_inc(40);lv_timer_handler();}frame(folder,"expression");
    ui_ScreenPageSettings_screen_init();lv_scr_load(ui_ScreenPageSettings);frame(folder,"settings");
    ui_ScreenPageCxSettings_screen_init();lv_scr_load(ui_ScreenPageCxSettings);
    for(int i=0;i<8;++i){lv_tick_inc(80);lv_timer_handler();}
    ui_cx_power_update();frame(folder,"cx-standby");
    lv_scr_load(ui_ScreenPageRpm);
    connected=true;seed();obd_data_set_rpm(800);host_update(800);render(folder,"rpm-idle");
    seed();host_update(3000);render(folder,"rpm-green");
    seed();obd_data_set_rpm(5500);host_update(5500);render(folder,"rpm-red");
    lv_tick_inc(6000);obd_data_set_coolant_temp(92);host_update(UINT16_MAX);render(folder,"rpm-stale");
    sleeping=true;render(folder,"rpm-cx-sleep");
    test_data_entry();
    test_display_ranges(folder);
    puts("Rendered all current carousel pages and settings from production LVGL/UI sources with illustrative inputs");
    return 0;
}
