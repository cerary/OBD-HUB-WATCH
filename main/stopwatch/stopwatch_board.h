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
void Set_Backlight(uint8_t percent);

#ifdef __cplusplus
}
#endif
