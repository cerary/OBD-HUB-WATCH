/* Hardware storage/allocation stubs; the actual maintained decoder and LVGL. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../../main/app_obd_dsp/boot_block_player.c"

static const char *package;
static size_t allocation_bytes;
void *heap_caps_malloc(size_t size, int caps) {
    (void)caps;
    allocation_bytes += size;
    return malloc(size);
}
size_t boot_media_raw_manifest_max(void) { return 4096; }
static FILE *open_part(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", package, name);
    return fopen(path, "rb");
}
esp_err_t boot_media_raw_read_manifest(uint8_t *buf, size_t size) {
    FILE *f = open_part("boot_block.txt");
    if (!f) return ESP_FAIL;
    memset(buf, 0, size);
    fread(buf, 1, size, f);
    fclose(f);
    return ESP_OK;
}
esp_err_t boot_media_raw_read_bin(uint8_t *buf, size_t size, size_t *out_len) {
    FILE *f = open_part("boot_block.bin");
    if (!f) return ESP_FAIL;
    fseek(f, 0, SEEK_END);
    size_t available = ftell(f);
    rewind(f);
    *out_len = buf ? fread(buf, 1, size, f) : available;
    fclose(f);
    return ESP_OK;
}
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colors) {
    (void)area; (void)colors;
    lv_disp_flush_ready(drv);
}
static bool fixture(const uint8_t *stream, size_t size, const char *format) {
    static lv_color_t colors[8];
    static uint16_t x_edges[] = {0,1,2,3,4}, y_edges[] = {0,1,2};
    reset_state();
    memset(colors, 0xff, sizeof(colors));
    s_state.manifest.canvas_width = s_state.manifest.grid_width = 4;
    s_state.manifest.canvas_height = s_state.manifest.grid_height = 2;
    strcpy(s_state.manifest.stream_format, format);
    s_state.canvas_buf = colors;
    s_state.x_edges = x_edges; s_state.y_edges = y_edges;
    s_state.stream_data = (uint8_t *)stream;
    s_state.stream_size = size;
    return apply_next_frame();
}
static void check_edge_cases(void) {
    const uint8_t good[] = {2,0,0,0, 8,0,0xf8, 15,4}; // red 0..1, black 5..7
    assert(fixture(good,sizeof(good),BOOT_BLOCK_STREAM_FORMAT_V3));
    assert(s_state.canvas_buf[0].full == lv_color_make(255,0,0).full);
    assert(s_state.canvas_buf[1].full == lv_color_make(255,0,0).full);
    assert(s_state.canvas_buf[2].full == lv_color_white().full);
    assert(s_state.canvas_buf[5].full == lv_color_black().full);
    assert(s_state.canvas_buf[7].full == lv_color_black().full);
    const uint8_t contiguous[] = {2,0,0,0, 8,0,0xf8, 13};
    assert(fixture(contiguous,sizeof(contiguous),BOOT_BLOCK_STREAM_FORMAT_V3));
    assert(s_state.canvas_buf[2].full == lv_color_black().full);
    assert(s_state.canvas_buf[4].full == lv_color_black().full);
    const uint8_t zero[] = {1,0,0,0, 0};
    const uint8_t overflow[] = {1,0,0,0, 37};
    const uint8_t bad_start[] = {1,0,0,0, 7,8};
    const uint8_t truncated[] = {1,0,0,0, 4,0};
    const uint8_t overlap[] = {2,0,0,0, 8,0,0xf8, 7,0};
    const uint8_t bad_varint[] = {1,0,0,0, 128,128,128,128,128};
    const uint8_t too_many[] = {9,0,0,0};
    assert(!fixture(zero,sizeof(zero),BOOT_BLOCK_STREAM_FORMAT_V3));
    assert(!fixture(overflow,sizeof(overflow),BOOT_BLOCK_STREAM_FORMAT_V3));
    assert(!fixture(bad_start,sizeof(bad_start),BOOT_BLOCK_STREAM_FORMAT_V3));
    assert(!fixture(truncated,sizeof(truncated),BOOT_BLOCK_STREAM_FORMAT_V3));
    assert(!fixture(overlap,sizeof(overlap),BOOT_BLOCK_STREAM_FORMAT_V3));
    assert(!fixture(bad_varint,sizeof(bad_varint),BOOT_BLOCK_STREAM_FORMAT_V3));
    assert(!fixture(too_many,sizeof(too_many),BOOT_BLOCK_STREAM_FORMAT_V3));
    const uint8_t v1[] = {2,0, 0,0,0xf8, 5};
    const uint8_t v2[] = {2,0,0,0, 0,0,0xf8, 5};
    assert(fixture(v1,sizeof(v1),BOOT_BLOCK_DEFAULT_STREAM_FORMAT));
    assert(s_state.canvas_buf[0].full == lv_color_make(255,0,0).full);
    assert(s_state.canvas_buf[2].full == lv_color_black().full);
    assert(fixture(v2,sizeof(v2),BOOT_BLOCK_STREAM_FORMAT_V2));
    assert(s_state.canvas_buf[0].full == lv_color_make(255,0,0).full);
    assert(s_state.canvas_buf[2].full == lv_color_black().full);
    // Runs also work with a smaller grid expanded to the canvas.
    lv_color_t expanded[16] = {0};
    uint16_t edges[] = {0,2,4};
    const uint8_t scaled[] = {1,0,0,0, 8,0xff,0xff};
    reset_state();
    s_state.manifest.grid_width = s_state.manifest.grid_height = 2;
    s_state.manifest.canvas_width = s_state.manifest.canvas_height = 4;
    strcpy(s_state.manifest.stream_format, BOOT_BLOCK_STREAM_FORMAT_V3);
    s_state.canvas_buf = expanded; s_state.x_edges = s_state.y_edges = edges;
    s_state.stream_data = (uint8_t *)scaled; s_state.stream_size = sizeof(scaled);
    assert(apply_next_frame());
    for (int i=0;i<16;i++) assert(expanded[i].full == (i<8 ? lv_color_white().full : lv_color_black().full));
    reset_state();
    puts("v1/v2 compatibility, native/scaled runs and seven malformed-stream cases passed");
}
int main(int argc, char **argv) {
    assert(argc == 3);
    package = argv[1];
    lv_init();
    lv_color_t draw_pixels[466 * 10];
    lv_disp_draw_buf_t draw;
    lv_disp_draw_buf_init(&draw, draw_pixels, NULL, 466 * 10);
    lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = 466; drv.ver_res = 466;
    drv.draw_buf = &draw; drv.flush_cb = flush_cb;
    assert(lv_disp_drv_register(&drv));
    lv_obj_t *canvas = NULL;
    assert(boot_block_player_create(lv_scr_act(), &canvas));
    assert(canvas && s_state.manifest.canvas_width == 466 && s_state.manifest.canvas_height == 466);
    uint32_t frames = s_state.manifest.frame_count;
    uint32_t interval = s_state.frame_interval_ms;
    FILE *output = fopen(argv[2], "wb");
    assert(output);
    for (uint32_t i = 0; i < frames; ++i) {
        if (i) boot_block_player_update(i * interval);
        assert(s_state.next_frame_index == i + 1);
        for (uint32_t j = 0; j < 466 * 466; ++j) {
            uint16_t color = s_state.canvas_buf[j].full;
#if LV_COLOR_16_SWAP
            color = (color >> 8) | (color << 8);
#endif
            uint8_t bytes[2] = { color & 255, color >> 8 };
            assert(fwrite(bytes, 1, 2, output) == 2);
        }
    }
    assert(boot_block_player_is_finished());
    fclose(output);
    printf("frames=%u canvas=466x466 interval_ms=%u allocations=%zu LV_COLOR_16_SWAP=%d\n",
           frames, interval, allocation_bytes, LV_COLOR_16_SWAP);
    boot_block_player_destroy();
    check_edge_cases();
    return 0;
}
