#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool stopwatch_board_init(void);
void stopwatch_board_flush(int x, int y, int width, int height,
                           const uint16_t *pixels, bool last);
bool stopwatch_board_touch(uint16_t *x, uint16_t *y);
// The yellow (left) and blue (right) programmable buttons are active-low.
void stopwatch_board_buttons_read(bool *left_pressed, bool *right_pressed);
typedef enum {
    STOPWATCH_FEEDBACK_TOUCH = 1,
    STOPWATCH_FEEDBACK_OPTION = 2,
    STOPWATCH_FEEDBACK_PAGE = 3,
} stopwatch_feedback_t;
void stopwatch_board_feedback(stopwatch_feedback_t kind);
// StopWatch v1.0: external power is USB VIN OR rear pin 14 (5V IN / PORT_INT).
// Returns false on an unknown input or unverified rear-power wake setup.
// Percent is an estimate (3.3 V empty, 4.2 V full), not a fuel-gauge reading.
bool stopwatch_board_power_status(uint8_t *percent, bool *external_power);
bool stopwatch_board_shutdown(void); // PMIC L0; USB / rear 5V arrival / power button cold boot
bool stopwatch_board_imu_init(void);
// Acceleration in g, in display coordinates: +x right, +y toward screen bottom.
bool stopwatch_board_imu_read(float *x, float *y, float *z);
void Set_Backlight(uint8_t percent);

#ifdef __cplusplus
}
#endif
