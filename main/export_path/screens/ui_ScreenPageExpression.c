// StopWatch expression page: V5 vector contours driven by the calibrated IMU.
// The pose lattice is generated from the reviewed browser preview, not GIFs.
#include "../ui.h"

#if CONFIG_OBD_HW_VERSION_M5STOPWATCH

#include "bsp_obd_dsp/nvs_storage.h"
#include "stopwatch/stopwatch_board.h"
#include "ui_expression_poses.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>

#define EXPR_CANVAS_X 20
#define EXPR_CANVAS_Y 128
#define EXPR_CANVAS_W 426
#define EXPR_CANVAS_H 210

typedef struct {
    float x;
    float v;
    float omega;
    float damping;
} expression_spring_t;

static lv_obj_t *s_face;
static lv_obj_t *s_status;
static lv_timer_t *s_timer;
static uint8_t *s_pixels;
static expression_spring_t s_turn = {0, 0, 8.7f, .68f};
static expression_spring_t s_drive = {0, 0, 9.4f, .68f};
static float s_imu_turn, s_imu_drive;
static uint32_t s_last_tick, s_last_imu_tick;
static bool s_imu_ready;

static float clamp_unit(float value)
{
    if (value < -1.f) return -1.f;
    if (value > 1.f) return 1.f;
    return value;
}

static float axis_value(const float xyz[3], const float axis[3])
{
    return xyz[0]*axis[0] + xyz[1]*axis[1] + xyz[2]*axis[2];
}

static void advance_spring(expression_spring_t *spring, float target, float dt)
{
    int steps = (int)ceilf(dt / .006f);
    if (steps < 1) steps = 1;
    const float h = dt / steps;
    for (int i = 0; i < steps; ++i) {
        const float acceleration = spring->omega * spring->omega * (target - spring->x)
            - 2.f * spring->damping * spring->omega * spring->v;
        spring->v += acceleration * h;
        spring->x += spring->v * h;
    }
    if (fabsf(spring->x - target) < .00005f && fabsf(spring->v) < .0005f) {
        spring->x = target;
        spring->v = 0.f;
    }
}

static void pose_cell(float value, int *lower, float *fraction)
{
    const float position = (clamp_unit(value) + 1.f) * 3.f;
    if (position >= 6.f) {
        *lower = 5;
        *fraction = 1.f;
    } else {
        *lower = (int)position;
        *fraction = position - *lower;
    }
}

static float maxf(float a, float b) { return a > b ? a : b; }
static float minf(float a, float b) { return a < b ? a : b; }

// LVGL 8's polygon drawer only accepts convex contours. The V5 eyes and
// mouth have deliberate concave curves, so fill their 8-bit alpha mask with
// an even-odd scanline rasterizer instead. Four subrows smooth the boundary.
static void fill_contour(const float points[EXPR_VERTEX_COUNT][2])
{
    float lo_y = EXPR_CANVAS_H, hi_y = 0.f;
    for (int i = 0; i < EXPR_VERTEX_COUNT; ++i) {
        lo_y = minf(lo_y, points[i][1]);
        hi_y = maxf(hi_y, points[i][1]);
    }
    int first = (int)floorf(lo_y);
    int last = (int)ceilf(hi_y);
    if (first < 0) first = 0;
    if (last > EXPR_CANVAS_H) last = EXPR_CANVAS_H;
    for (int y = first; y < last; ++y) {
        for (int sub = 0; sub < 4; ++sub) {
            const float sample_y = (float)y + ((float)sub + .5f) * .25f;
            float crossings[EXPR_VERTEX_COUNT];
            int count = 0;
            for (int edge = 0; edge < EXPR_VERTEX_COUNT; ++edge) {
                const float *a = points[edge];
                const float *b = points[(edge + 1) % EXPR_VERTEX_COUNT];
                if ((a[1] <= sample_y && b[1] > sample_y) ||
                    (b[1] <= sample_y && a[1] > sample_y)) {
                    const float crossing = a[0] + (sample_y - a[1]) *
                        (b[0] - a[0]) / (b[1] - a[1]);
                    int at = count++;
                    while (at > 0 && crossings[at - 1] > crossing) {
                        crossings[at] = crossings[at - 1];
                        --at;
                    }
                    crossings[at] = crossing;
                }
            }
            for (int span = 0; span + 1 < count; span += 2) {
                float left = crossings[span], right = crossings[span + 1];
                int x0 = (int)floorf(left), x1 = (int)ceilf(right);
                if (x0 < 0) x0 = 0;
                if (x1 > EXPR_CANVAS_W) x1 = EXPR_CANVAS_W;
                for (int x = x0; x < x1; ++x) {
                    const float cover = maxf(0.f, minf(right, (float)x + 1.f) -
                        maxf(left, (float)x));
                    uint8_t *pixel = &s_pixels[y * EXPR_CANVAS_W + x];
                    int alpha = *pixel + (int)lroundf(63.75f * cover);
                    *pixel = alpha > 255 ? 255 : (uint8_t)alpha;
                }
            }
        }
    }
}

static void draw_face(void)
{
    if (!s_pixels || !s_face) return;
    memset(s_pixels, 0, EXPR_CANVAS_W * EXPR_CANVAS_H);

    int t0, d0;
    float ft, fd;
    pose_cell(s_turn.x, &t0, &ft);
    pose_cell(s_drive.x, &d0, &fd);
    const float w00 = (1.f - ft) * (1.f - fd);
    const float w10 = ft * (1.f - fd);
    const float w01 = (1.f - ft) * fd;
    const float w11 = ft * fd;
    // Match the preview's quiet breathing motion without changing eye shape.
    const float bob = 2.1f * sinf(2.f * 3.14159265f * (float)(lv_tick_get() % 4640U) / 4640.f);

    float vertices[EXPR_VERTEX_COUNT][2];
    for (int part = 0; part < EXPR_PART_COUNT; ++part) {
        for (int vertex = 0; vertex < EXPR_VERTEX_COUNT; ++vertex) {
            const int16_t (*a)[2] = expr_pose[d0][t0][part];
            const int16_t (*b)[2] = expr_pose[d0][t0 + 1][part];
            const int16_t (*c)[2] = expr_pose[d0 + 1][t0][part];
            const int16_t (*d)[2] = expr_pose[d0 + 1][t0 + 1][part];
            const float x = (w00*a[vertex][0] + w10*b[vertex][0]
                + w01*c[vertex][0] + w11*d[vertex][0]) / EXPR_COORD_SCALE;
            const float y = (w00*a[vertex][1] + w10*b[vertex][1]
                + w01*c[vertex][1] + w11*d[vertex][1]) / EXPR_COORD_SCALE + bob;
            vertices[vertex][0] = x - EXPR_CANVAS_X;
            vertices[vertex][1] = y - EXPR_CANVAS_Y;
        }
        fill_contour(vertices);
    }
    lv_obj_invalidate(s_face);
}

static void update_face(lv_timer_t *timer)
{
    (void)timer;
    const uint32_t now = lv_tick_get();
    float dt = (float)(now - s_last_tick) * .001f;
    s_last_tick = now;
    if (dt > .05f) dt = .05f;
    if (dt < 0.f) dt = 0.f;

    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    if (s_imu_ready && cfg->g_cal_valid) {
        float x, y, z;
        if (stopwatch_board_imu_read(&x, &y, &z)) {
            const float motion[3] = {x - cfg->g_zero[0], y - cfg->g_zero[1], z - cfg->g_zero[2]};
            float lateral = clamp_unit(axis_value(motion, cfg->g_axis_right));
            float longitudinal = clamp_unit(axis_value(motion, cfg->g_axis_forward));
            if (fabsf(lateral) < .04f) lateral = 0.f;
            if (fabsf(longitudinal) < .04f) longitudinal = 0.f;
            s_imu_turn = .72f * s_imu_turn + .28f * lateral;
            s_imu_drive = .72f * s_imu_drive + .28f * longitudinal;
            s_last_imu_tick = now;
            if (s_status) lv_obj_add_flag(s_status, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (now - s_last_imu_tick > 250U) {
        s_imu_turn *= .8f;
        s_imu_drive *= .8f;
    }
    advance_spring(&s_turn, s_imu_turn, dt);
    advance_spring(&s_drive, s_imu_drive, dt);
    draw_face();
}

static void expression_screen_state(lv_event_t *event)
{
    if (!s_timer) return;
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOADED) {
        s_last_tick = lv_tick_get();
        lv_timer_resume(s_timer);
    } else if (lv_event_get_code(event) == LV_EVENT_SCREEN_UNLOADED) {
        lv_timer_pause(s_timer);
    }
}

static void expression_delete(lv_event_t *event)
{
    (void)event;
    if (s_timer) { lv_timer_del(s_timer); s_timer = NULL; }
    if (s_pixels) { heap_caps_free(s_pixels); s_pixels = NULL; }
    s_face = NULL;
    s_status = NULL;
}

void ui_ScreenPageExpression_screen_init(void)
{
    ui_ScreenPageExpression = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenPageExpression, LV_OBJ_FLAG_SCROLLABLE);
    ui_helpers_style_screen_bg(ui_ScreenPageExpression);
    lv_obj_set_style_bg_opa(ui_ScreenPageExpression, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t *ring = ui_helpers_create_ring(ui_ScreenPageExpression, 10);

    s_turn = (expression_spring_t){0, 0, 8.7f, .68f};
    s_drive = (expression_spring_t){0, 0, 9.4f, .68f};
    s_imu_turn = s_imu_drive = 0.f;
    s_last_tick = s_last_imu_tick = lv_tick_get();

    s_pixels = heap_caps_malloc(EXPR_CANVAS_W * EXPR_CANVAS_H,
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    // The V5 geometry stays within x=28..438, y=133..331. A small draw
    // object limits periodic invalidation to the animated area of the dial.
    if (s_pixels) {
        s_face = lv_canvas_create(ui_ScreenPageExpression);
        lv_canvas_set_buffer(s_face, s_pixels, EXPR_CANVAS_W,
                             EXPR_CANVAS_H, LV_IMG_CF_ALPHA_8BIT);
        lv_obj_set_pos(s_face, EXPR_CANVAS_X, EXPR_CANVAS_Y);
        lv_obj_set_style_img_recolor(s_face, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_img_recolor_opa(s_face, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_clear_flag(s_face, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        draw_face();
    }

    ui_helpers_create_mini_brand(ui_ScreenPageExpression, -165, false);
    lv_obj_move_foreground(ring);

    s_imu_ready = stopwatch_board_imu_init();
    if (!s_pixels || !s_imu_ready || !nvs_cfg_get()->g_cal_valid) {
        s_status = lv_label_create(ui_ScreenPageExpression);
        lv_label_set_text(s_status, !s_pixels ? "FACE MEMORY LOW" :
                          (s_imu_ready ? "CALIBRATE G METER" : "IMU UNAVAILABLE"));
        lv_obj_set_style_text_font(s_status, &ui_font_FontTypoderSize16, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_status, lv_color_hex(0xA4A1AB), LV_PART_MAIN);
        lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 145);
    }

    s_timer = lv_timer_create(update_face, 40, NULL);
    lv_timer_pause(s_timer);
    lv_obj_add_event_cb(ui_ScreenPageExpression, expression_screen_state, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(ui_ScreenPageExpression, expression_screen_state, LV_EVENT_SCREEN_UNLOADED, NULL);
    lv_obj_add_event_cb(ui_ScreenPageExpression, expression_delete, LV_EVENT_DELETE, NULL);
    lv_obj_add_event_cb(ui_ScreenPageExpression, ui_event_expression_background, LV_EVENT_GESTURE, NULL);
}

#else

void ui_ScreenPageExpression_screen_init(void)
{
    ui_ScreenPageExpression = lv_obj_create(NULL);
    ui_helpers_style_screen_bg(ui_ScreenPageExpression);
}

#endif
