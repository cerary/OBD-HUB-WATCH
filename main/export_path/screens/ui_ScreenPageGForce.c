// StopWatch inertial G meter. Screen coordinates and artwork follow the 466 px UI.
#include "../ui.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
#include "stopwatch/stopwatch_board.h"
#endif

#define G_GRID_HALF 9
#define G_GRID_N 19
#define G_GRID_STEP 17
#define G_GRID_RADIUS 158
#define G_DOT_LIMIT 158.0f
#define G_SCALE 1.5f
#define G_TRAIL_FADE_MS 10000U
#define G_TRAIL_FADE_STEPS 32U

static lv_obj_t *s_grid[G_GRID_N][G_GRID_N];
static uint8_t s_grid_base[G_GRID_N][G_GRID_N];
static bool s_trail_active[G_GRID_N][G_GRID_N];
static uint32_t s_trail_lit_ms[G_GRID_N][G_GRID_N];
static uint8_t s_trail_level[G_GRID_N][G_GRID_N];
static uint16_t s_trail_cells[G_GRID_N * G_GRID_N];
static uint16_t s_trail_count;
static lv_obj_t *s_live_dot;
static lv_obj_t *s_peak_chars[4][10];
static void *s_peak_buffers[4][10];
static lv_obj_t *s_status;
static lv_timer_t *s_update_timer;
static lv_timer_t *s_cal_timer;
static lv_obj_t *s_cal_roller;
static lv_obj_t *s_cal_status;
static float s_max_g[4]; // front-screen, right-screen, rear-screen, left-screen
static float s_filtered_x, s_filtered_y;
static int s_last_x, s_last_y;
static float s_cal_sum[3];
static int s_cal_samples, s_cal_attempts;
// lv_line keeps a pointer to its points, so the corner coordinates must persist.
static lv_point_t s_arc_corners[8][2];

static void draw_peak(int group)
{
    char value[16];
    snprintf(value,sizeof(value),group==0 ? "%.2fg MAX" : "%.2f",s_max_g[group]);
    const int count=(int)strlen(value);
    const float center_angle[4]={-90.f,0.f,90.f,180.f};
    const bool reverse=group==2;
    for (int i=0;i<10;++i) {
        if (s_peak_chars[group][i]) { lv_obj_del(s_peak_chars[group][i]); s_peak_chars[group][i]=NULL; }
        if (s_peak_buffers[group][i]) { free(s_peak_buffers[group][i]); s_peak_buffers[group][i]=NULL; }
    }
    for (int i=0;i<count && i<10;++i) {
        if (value[i]==' ') continue;
        const float offset=((float)i-(count-1)*0.5f)*12.f;
        const float angle=center_angle[group]+(reverse ? -offset : offset)*180.f/(185.f*3.14159265f);
        const float radians=angle*3.14159265f/180.f;
        lv_obj_t *glyph=lv_canvas_create(ui_ScreenPageGForce);
        void *buf=malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(24,28));
        if (!buf) { lv_obj_del(glyph); continue; }
        s_peak_chars[group][i]=glyph;
        s_peak_buffers[group][i]=buf;
        lv_canvas_set_buffer(glyph,buf,24,28,LV_IMG_CF_TRUE_COLOR_ALPHA);
        lv_canvas_fill_bg(glyph,lv_color_black(),LV_OPA_TRANSP);
        lv_draw_label_dsc_t dsc;
        lv_draw_label_dsc_init(&dsc);
        dsc.font=&ui_font_FontTypoderSize16;
        dsc.color=lv_color_hex(0xE5E2E7);
        dsc.align=LV_TEXT_ALIGN_CENTER;
        char letter[2]={value[i],0};
        lv_canvas_draw_text(glyph,0,3,24,&dsc,letter);
        int tangent=(int)lroundf((angle+(reverse ? -90.f : 90.f))*10.f);
        lv_img_set_angle(glyph,tangent);
        lv_obj_align(glyph,LV_ALIGN_CENTER,
                     (int)lroundf(185.f*cosf(radians)),
                     (int)lroundf(185.f*sinf(radians)));
        lv_obj_clear_flag(glyph,LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
}

static float dot3(const float a[3], const float b[3])
{
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static bool normalize3(float v[3])
{
    float n = sqrtf(dot3(v, v));
    if (n < 0.12f) return false;
    for (int i = 0; i < 3; ++i) v[i] /= n;
    return true;
}

static lv_obj_t *g_label(lv_obj_t *parent, const char *text, int x, int y,
                          const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, x, y);
    return label;
}

static void g_arc(lv_obj_t *parent, int rotation, int span, int quadrant)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, 370, 370);
    lv_obj_center(arc);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_bg_angles(arc, 0, span);
    lv_arc_set_rotation(arc, rotation);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_width(arc, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0xD9D6DD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    // The MINI dial turns each arc briefly toward the center at both ends.
    // Calculate these from the same angles as the arcs so every corner joins
    // its arc exactly and the left/right pairs remain mirrored.
    const int ends[2] = {rotation, rotation + span};
    for (int end = 0; end < 2; ++end) {
        float angle = ends[end] * 3.14159265f / 180.f;
        int outer_x = (int)lroundf(184.f * cosf(angle));
        int outer_y = (int)lroundf(184.f * sinf(angle));
        int inner_x = (int)lroundf(176.f * cosf(angle));
        int inner_y = (int)lroundf(176.f * sinf(angle));
        int min_x = outer_x < inner_x ? outer_x : inner_x;
        int min_y = outer_y < inner_y ? outer_y : inner_y;
        lv_point_t *points = s_arc_corners[quadrant * 2 + end];
        points[0] = (lv_point_t){outer_x - min_x, outer_y - min_y};
        points[1] = (lv_point_t){inner_x - min_x, inner_y - min_y};
        lv_obj_t *corner = lv_line_create(parent);
        lv_line_set_points(corner, points, 2);
        lv_obj_set_style_line_width(corner, 3, LV_PART_MAIN);
        lv_obj_set_style_line_color(corner, lv_color_hex(0xD9D6DD), LV_PART_MAIN);
        lv_obj_set_style_line_opa(corner, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_pos(corner, LV_HOR_RES / 2 + min_x, LV_VER_RES / 2 + min_y);
        lv_obj_clear_flag(corner, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
}

static void set_trail_level(int gx, int gy, uint8_t level)
{
    s_trail_level[gy][gx] = level;
    lv_obj_t *dot = s_grid[gy][gx];
    if (!dot) return;
    const int base = s_grid_base[gy][gx];
    const int red = base + ((0xE7 - base) * level + G_TRAIL_FADE_STEPS / 2) / G_TRAIL_FADE_STEPS;
    const int green = base + ((0x79 - base) * level + G_TRAIL_FADE_STEPS / 2) / G_TRAIL_FADE_STEPS;
    const int blue = base + ((0x80 - base) * level + G_TRAIL_FADE_STEPS / 2) / G_TRAIL_FADE_STEPS;
    lv_obj_set_style_bg_color(dot, lv_color_make(red, green, blue), LV_PART_MAIN);
    lv_obj_set_size(dot, level == 0 ? 4 : (level > G_TRAIL_FADE_STEPS / 2 ? 6 : 5),
                    level == 0 ? 4 : (level > G_TRAIL_FADE_STEPS / 2 ? 6 : 5));
}

static void fade_trail(uint32_t now_ms)
{
    // Only active dots are touched. Each cell uses its own last-lit timestamp;
    // changing another dot never restarts this one's fade.
    uint16_t i = 0;
    while (i < s_trail_count) {
        uint16_t cell = s_trail_cells[i];
        int gx = cell % G_GRID_N, gy = cell / G_GRID_N;
        uint32_t elapsed = now_ms - s_trail_lit_ms[gy][gx];
        if (elapsed >= G_TRAIL_FADE_MS) {
            if (s_trail_level[gy][gx] != 0) set_trail_level(gx, gy, 0);
            s_trail_active[gy][gx] = false;
            s_trail_cells[i] = s_trail_cells[--s_trail_count];
            continue;
        }
        uint8_t level = G_TRAIL_FADE_STEPS - elapsed * G_TRAIL_FADE_STEPS / G_TRAIL_FADE_MS;
        if (level != s_trail_level[gy][gx]) set_trail_level(gx, gy, level);
        ++i;
    }
}

static void mark_grid(int x, int y, uint32_t now_ms)
{
    int gx = (int)lroundf((float)x / G_GRID_STEP) + G_GRID_HALF;
    int gy = (int)lroundf((float)y / G_GRID_STEP) + G_GRID_HALF;
    if (gx < 0 || gx >= G_GRID_N || gy < 0 || gy >= G_GRID_N || !s_grid[gy][gx]) return;
    if (!s_trail_active[gy][gx]) {
        s_trail_active[gy][gx] = true;
        s_trail_cells[s_trail_count++] = gy * G_GRID_N + gx;
    }
    s_trail_lit_ms[gy][gx] = now_ms;
    if (s_trail_level[gy][gx] != G_TRAIL_FADE_STEPS)
        set_trail_level(gx, gy, G_TRAIL_FADE_STEPS);
}

static void update_g_meter(lv_timer_t *timer)
{
    (void)timer;
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
    uint32_t now_ms = lv_tick_get();
    fade_trail(now_ms);
    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    float x, y, z;
    if (!cfg->g_cal_valid || !stopwatch_board_imu_read(&x, &y, &z)) return;
    float motion[3] = {x-cfg->g_zero[0], y-cfg->g_zero[1], z-cfg->g_zero[2]};
    float right = dot3(motion, cfg->g_axis_right);
    float forward = dot3(motion, cfg->g_axis_forward);
    // An accelerometer shows inertial displacement opposite vehicle acceleration.
    float tx = -right * G_DOT_LIMIT / G_SCALE;
    float ty = forward * G_DOT_LIMIT / G_SCALE;
    float len = sqrtf(tx*tx + ty*ty);
    if (len > G_DOT_LIMIT) { tx *= G_DOT_LIMIT/len; ty *= G_DOT_LIMIT/len; }
    s_filtered_x = 0.72f*s_filtered_x + 0.28f*tx;
    s_filtered_y = 0.72f*s_filtered_y + 0.28f*ty;
    int px = (int)lroundf(s_filtered_x), py = (int)lroundf(s_filtered_y);
    lv_obj_align(s_live_dot, LV_ALIGN_CENTER, px, py);
    float values[4] = {-forward, -right, forward, right};
    for (int i=0; i<4; ++i) {
        if (values[i] > s_max_g[i]) {
            int old_hundredths=(int)(s_max_g[i]*100.f);
            s_max_g[i] = values[i];
            if ((int)(s_max_g[i]*100.f) != old_hundredths) draw_peak(i);
        }
    }
    // Trace every traversed grid cell so fast movement cannot skip a red dot.
    int segments = (int)fmaxf(1.f, (float)(abs(px-s_last_x) > abs(py-s_last_y)
        ? abs(px-s_last_x) : abs(py-s_last_y)) / 8.f);
    for (int i=0; i<=segments; ++i) {
        int sx = s_last_x + (px-s_last_x)*i/segments;
        int sy = s_last_y + (py-s_last_y)*i/segments;
        mark_grid(sx, sy, now_ms);
    }
    s_last_x = px; s_last_y = py;
    if (s_status) lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
#endif
}

static void g_page_delete(lv_event_t *e)
{
    (void)e;
    if (s_update_timer) { lv_timer_del(s_update_timer); s_update_timer = NULL; }
    memset(s_grid, 0, sizeof(s_grid));
    s_live_dot = NULL;
    s_status = NULL;
    for (int i=0;i<4;++i) for (int j=0;j<10;++j) {
        if (s_peak_buffers[i][j]) { free(s_peak_buffers[i][j]); s_peak_buffers[i][j]=NULL; }
        s_peak_chars[i][j]=NULL;
    }
}

static void g_screen_state(lv_event_t *e)
{
    if (!s_update_timer) return;
    if (lv_event_get_code(e)==LV_EVENT_SCREEN_LOADED) lv_timer_resume(s_update_timer);
    else if (lv_event_get_code(e)==LV_EVENT_SCREEN_UNLOADED) lv_timer_pause(s_update_timer);
}

void ui_ScreenPageGForce_screen_init(void)
{
    ui_ScreenPageGForce = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenPageGForce, LV_OBJ_FLAG_SCROLLABLE);
    ui_helpers_style_screen_bg(ui_ScreenPageGForce);
    lv_obj_set_style_bg_opa(ui_ScreenPageGForce, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_ScreenPageGForce, 0, LV_PART_MAIN);
    lv_obj_t *ring = ui_helpers_create_ring(ui_ScreenPageGForce, 10);
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
    // Mirror each pair across the vertical axis. The longer top label gets a
    // wider gap, while the side and bottom gaps preserve similar text clearance.
    g_arc(ui_ScreenPageGForce, 296, 52, 0);   // upper right: 296..348
    g_arc(ui_ScreenPageGForce, 12, 64, 1);    // lower right: 12..76
    g_arc(ui_ScreenPageGForce, 104, 64, 2);   // lower left: 104..168
    g_arc(ui_ScreenPageGForce, 192, 52, 3);   // upper left: 192..244
    for (int gy=-G_GRID_HALF; gy<=G_GRID_HALF; ++gy) {
        for (int gx=-G_GRID_HALF; gx<=G_GRID_HALF; ++gx) {
            int px=gx*G_GRID_STEP, py=gy*G_GRID_STEP;
            float dist=sqrtf((float)(px*px+py*py));
            if (dist > G_GRID_RADIUS || (py < -105 && abs(px) < 85)) continue;
            int shade = 95 - (int)(80.f*powf(dist/G_GRID_RADIUS, 1.5f));
            if (shade < 13) shade=13;
            lv_obj_t *dot = lv_obj_create(ui_ScreenPageGForce);
            lv_obj_set_size(dot, 4, 4);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            lv_obj_set_style_bg_color(dot, lv_color_make(shade,shade,shade), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
            lv_obj_align(dot, LV_ALIGN_CENTER, px, py);
            lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            s_grid[gy+G_GRID_HALF][gx+G_GRID_HALF] = dot;
            s_grid_base[gy+G_GRID_HALF][gx+G_GRID_HALF] = (uint8_t)shade;
        }
    }
    // Screens may be recreated. Reconstruct only still-active trail dots at
    // their current age; elapsed time while this page was hidden still counts.
    for (uint16_t i = 0; i < s_trail_count; ++i) {
        uint16_t cell = s_trail_cells[i];
        s_trail_level[cell / G_GRID_N][cell % G_GRID_N] = UINT8_MAX;
    }
    fade_trail(lv_tick_get());
    s_live_dot = lv_obj_create(ui_ScreenPageGForce);
    lv_obj_set_size(s_live_dot, 14, 14);
    lv_obj_set_style_radius(s_live_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_live_dot, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_live_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_live_dot, 0, LV_PART_MAIN);
    lv_obj_center(s_live_dot);
    lv_obj_clear_flag(s_live_dot, LV_OBJ_FLAG_CLICKABLE);
    for (int i=0;i<4;++i) draw_peak(i);
    ui_helpers_create_mini_brand(ui_ScreenPageGForce, -132, false);
    bool imu_ok=stopwatch_board_imu_init();
    if (!imu_ok || !nvs_cfg_get()->g_cal_valid)
        s_status=g_label(ui_ScreenPageGForce,imu_ok ? "SWIPE DOWN TO CALIBRATE" : "IMU UNAVAILABLE",
                         0,135,&ui_font_FontTypoderSize16,0xA4A1AB);
    s_update_timer=lv_timer_create(update_g_meter,80,NULL);
    lv_obj_add_event_cb(ui_ScreenPageGForce,g_page_delete,LV_EVENT_DELETE,NULL);
    lv_obj_add_event_cb(ui_ScreenPageGForce,g_screen_state,LV_EVENT_SCREEN_LOADED,NULL);
    lv_obj_add_event_cb(ui_ScreenPageGForce,g_screen_state,LV_EVENT_SCREEN_UNLOADED,NULL);
#else
    g_label(ui_ScreenPageGForce,"G-FORCE",0,0,&ui_font_FontTypoderSize24,0xFFFFFF);
#endif
    lv_obj_move_foreground(ring);
    lv_obj_add_event_cb(ui_ScreenPageGForce,ui_event_gforce_background,LV_EVENT_GESTURE,NULL);
}

void ui_gforce_clear_max(void)
{
    for (uint16_t i = 0; i < s_trail_count; ++i) {
        uint16_t cell = s_trail_cells[i];
        int gx = cell % G_GRID_N, gy = cell / G_GRID_N;
        set_trail_level(gx, gy, 0);
        s_trail_active[gy][gx] = false;
    }
    s_trail_count = 0;
    memset(s_max_g,0,sizeof(s_max_g));
    if (ui_ScreenPageGForce) for (int i=0;i<4;++i) draw_peak(i);
    s_last_x=(int)lroundf(s_filtered_x);
    s_last_y=(int)lroundf(s_filtered_y);
    if (s_cal_status) lv_label_set_text(s_cal_status,"MAX HISTORY CLEARED");
}

static void clear_g_history(lv_event_t *e)
{
    (void)e;
    ui_gforce_clear_max();
}

static void finish_cal(lv_timer_t *timer)
{
    (void)timer;
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
    float x,y,z;
    ++s_cal_attempts;
    if (stopwatch_board_imu_read(&x,&y,&z)) {
        s_cal_sum[0]+=x; s_cal_sum[1]+=y; s_cal_sum[2]+=z;
        ++s_cal_samples;
    }
    if (s_cal_samples < 32 && s_cal_attempts < 160) return;
    lv_timer_del(s_cal_timer); s_cal_timer=NULL;
    if (s_cal_samples < 32) { lv_label_set_text(s_cal_status,"IMU READ FAILED"); return; }
    nvs_user_cfg_t cfg=*nvs_cfg_get();
    for (int i=0;i<3;++i) cfg.g_zero[i]=s_cal_sum[i]/s_cal_samples;
    float gravity[3]={cfg.g_zero[0],cfg.g_zero[1],cfg.g_zero[2]};
    if (!normalize3(gravity)) { lv_label_set_text(s_cal_status,"HOLD STILL AND RETRY"); return; }
    cfg.g_front_mode=lv_roller_get_selected(s_cal_roller);
    float front[3]={0,0,cfg.g_front_mode==0 ? -1.f : 1.f};
    if (cfg.g_front_mode==2) { front[1]=-1; front[2]=0; }
    float proj=dot3(front,gravity);
    for (int i=0;i<3;++i) front[i]-=proj*gravity[i];
    if (!normalize3(front)) { lv_label_set_text(s_cal_status,"CHOOSE TOP FRONT MODE"); return; }
    float right[3]={cfg.g_front_mode==1 ? -1.f : 1.f,0,0};
    proj=dot3(right,gravity);
    for (int i=0;i<3;++i) right[i]-=proj*gravity[i];
    proj=dot3(right,front);
    for (int i=0;i<3;++i) right[i]-=proj*front[i];
    if (!normalize3(right)) { lv_label_set_text(s_cal_status,"ROTATE MOUNT AND RETRY"); return; }
    for (int i=0;i<3;++i) { cfg.g_axis_right[i]=right[i]; cfg.g_axis_forward[i]=front[i]; }
    cfg.g_cal_valid=1;
    if (nvs_cfg_set(&cfg) == ESP_OK) {
        s_filtered_x=s_filtered_y=0;
        lv_label_set_text(s_cal_status,"CALIBRATED");
    } else lv_label_set_text(s_cal_status,"SAVE FAILED");
#endif
}

static void start_cal(lv_event_t *e)
{
    (void)e;
#if CONFIG_OBD_HW_VERSION_M5STOPWATCH
    if (s_cal_timer) return;
    if (!stopwatch_board_imu_init()) { lv_label_set_text(s_cal_status,"IMU UNAVAILABLE"); return; }
    memset(s_cal_sum,0,sizeof(s_cal_sum));
    s_cal_samples=s_cal_attempts=0;
    lv_label_set_text(s_cal_status,"HOLD STILL...");
    s_cal_timer=lv_timer_create(finish_cal,20,NULL);
#endif
}

static void cal_delete(lv_event_t *e)
{
    (void)e;
    if (s_cal_timer) { lv_timer_del(s_cal_timer); s_cal_timer=NULL; }
    s_cal_roller=s_cal_status=NULL;
}

static void cal_unloaded(lv_event_t *e)
{
    (void)e;
    if (s_cal_timer) { lv_timer_del(s_cal_timer); s_cal_timer=NULL; }
}

static lv_obj_t *g_button(lv_obj_t *parent,const char *name,int x,int y,lv_event_cb_t cb)
{
    lv_obj_t *btn=lv_btn_create(parent);
    lv_obj_set_size(btn,158,46);
    lv_obj_align(btn,LV_ALIGN_CENTER,x,y);
    lv_obj_set_style_radius(btn,14,LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn,lv_color_hex(0x303038),LV_PART_MAIN);
    lv_obj_t *text=lv_label_create(btn);
    lv_label_set_text(text,name);
    lv_obj_set_style_text_font(text,&ui_font_FontTypoderSize16,LV_PART_MAIN);
    lv_obj_center(text);
    lv_obj_add_event_cb(btn,cb,LV_EVENT_CLICKED,NULL);
    return btn;
}

void ui_ScreenPageGForceCal_screen_init(void)
{
    ui_ScreenPageGForceCal=lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenPageGForceCal,LV_OBJ_FLAG_SCROLLABLE);
    ui_helpers_style_screen_bg(ui_ScreenPageGForceCal);
    lv_obj_set_style_border_width(ui_ScreenPageGForceCal,0,LV_PART_MAIN);
    lv_obj_t *ring=ui_helpers_create_ring(ui_ScreenPageGForceCal,10);
    g_label(ui_ScreenPageGForceCal,"G CALIBRATION",0,-149,&ui_font_FontTypoderSize24,0xFFFFFF);
    g_label(ui_ScreenPageGForceCal,"Mount direction",0,-99,&ui_font_FontTypoderSize16,0xA0A0A0);
    s_cal_roller=lv_roller_create(ui_ScreenPageGForceCal);
    lv_roller_set_options(s_cal_roller,"FACE DRIVER\nFACE FRONT\nTOP FRONT",LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_cal_roller,1);
    lv_roller_set_selected(s_cal_roller,nvs_cfg_get()->g_front_mode,LV_ANIM_OFF);
    lv_obj_set_size(s_cal_roller,200,42);
    lv_obj_set_ext_click_area(s_cal_roller,3);
    ui_helpers_style_dark_roller(s_cal_roller,&ui_font_FontTypoderSize20);
    lv_obj_align(s_cal_roller,LV_ALIGN_CENTER,0,-62);
    lv_obj_clear_flag(s_cal_roller,LV_OBJ_FLAG_GESTURE_BUBBLE);
    g_label(ui_ScreenPageGForceCal,"Park on level ground",0,-13,&ui_font_FontTypoderSize16,0xB3AFB7);
    g_label(ui_ScreenPageGForceCal,"Keep the car still",0,10,&ui_font_FontTypoderSize16,0xB3AFB7);
    g_button(ui_ScreenPageGForceCal,"SET ZERO",0,63,start_cal);
    g_button(ui_ScreenPageGForceCal,"CLEAR MAX",0,119,clear_g_history);
    s_cal_status=g_label(ui_ScreenPageGForceCal,"SWIPE UP TO RETURN",0,163,
                         &ui_font_FontTypoderSize16,0x8F8A94);
    lv_obj_move_foreground(ring);
    lv_obj_add_event_cb(ui_ScreenPageGForceCal,ui_event_gforce_cal_background,LV_EVENT_GESTURE,NULL);
    lv_obj_add_event_cb(ui_ScreenPageGForceCal,cal_delete,LV_EVENT_DELETE,NULL);
    lv_obj_add_event_cb(ui_ScreenPageGForceCal,cal_unloaded,LV_EVENT_SCREEN_UNLOADED,NULL);
}
