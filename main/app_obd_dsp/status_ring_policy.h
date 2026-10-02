#pragma once
#include <stdbool.h>
#include <stdint.h>

// Matches the display-item order, independently of LVGL or Bluetooth.
enum { RING_CLT, RING_IAT, RING_OIL, RING_LOAD, RING_TPS, RING_RPM,
       RING_SPEED, RING_BAT, RING_OILP, RING_BKT, RING_BOOST, RING_AFR,
       RING_ITEM_COUNT };
typedef struct {
    uint16_t rpm_green, rpm_approach, rpm_red;
    uint16_t g_warn_centi;
    uint8_t brightness;
    uint8_t reserved[3];
} status_ring_config_t;
typedef enum { RING_PAGE_STATIC, RING_PAGE_ITEMS, RING_PAGE_G } status_ring_page_t;
typedef enum { RING_NORMAL, RING_CONNECTING, RING_LOST, RING_SLEEP,
               RING_APPROACH, RING_ALARM, RING_NO_DATA } status_ring_state_t;
typedef struct {
    status_ring_page_t page;
    bool connected, ever_connected, initializing, sleeping, demo;
    int32_t value[RING_ITEM_COUNT];
    uint32_t age_ms[RING_ITEM_COUNT];
    bool valid[RING_ITEM_COUNT];
    int16_t alarm[RING_ITEM_COUNT]; // 32767 disables the existing high alarm.
    uint8_t items[5], item_count;
    uint16_t g_centi;
    bool g_valid;
    uint32_t g_age_ms;
    bool temperature_managed;
    uint8_t temperature_level[3]; // temperature_level_t, confirmed by hold policy
} status_ring_input_t;
typedef struct {
    uint32_t color;
    status_ring_state_t state;
    uint8_t item; // RING_ITEM_COUNT = no parameter name.
} status_ring_result_t;

#define STATUS_RING_WHITE 0xF4F6F8U
#define STATUS_RING_GREEN 0x43DE77U
#define STATUS_RING_RED   0xFF4D4DU
#define STATUS_RING_BLUE  0x55B8FFU
#define STATUS_RING_LOST_COLOR 0xF579B8U
#define STATUS_RING_SLEEP_COLOR 0x30343AU
#define STATUS_RING_YELLOW 0xFFD166U
#define STATUS_RING_ORANGE 0xFFAC45U

status_ring_config_t status_ring_default_config(void);
bool status_ring_config_valid(const status_ring_config_t *cfg);
status_ring_result_t status_ring_evaluate(const status_ring_input_t *in,
                                         const status_ring_config_t *cfg);
