# M5Stack StopWatch port preparation

## Source and target

- Upstream: `steveEcode/obd_brz_gauge`, baseline commit `528f542`.
- Local working branch: `port/m5stopwatch`.
- Original target: Waveshare ESP32-S3-Touch-LCD-1.85, ST77916 QSPI LCD, CST816 touch, 360 x 360 UI.
- New target: M5Stack StopWatch C152, ESP32-S3R8, CO5300 QSPI AMOLED, CST820B touch, 466 x 466.
- Hardware reference: `m5stack/M5StopWatch-UserDemo` commit `6b4aa12` (MIT), especially `main/hal/hal_display.cpp`, `hal_ioe.cpp`, and `hal_pmic.cpp`.

## Board differences to implement

| Function | StopWatch reference | Current gauge code |
| --- | --- | --- |
| Display | CO5300 via M5GFX; QSPI SCK 40, D0 41, D1 42, D2 46, D3 45, CS 39, TE 38 | ST77916 driver, different pin order, CS 21 |
| Touch | CST820B at I2C `0x15`; SDA 47, SCL 48; reset via M5IOE1 | CST816 on a board-specific I2C bus |
| Power and reset | M5PM1 and M5IOE1; enable L3B, OLED reset and touch reset before display and touch init | TCA9554 or direct GPIO and PWM LCD backlight |
| Brightness | AMOLED brightness command, 0-255 | LEDC backlight PWM, 0-100 |
| Canvas | 466 x 466 physical | 360 x 360 fixed generated LVGL screens and artwork |
| External inputs | No built-in RS485 brake sensor or ADS1115 oil-pressure ADC | V1 can initialize both |

The existing 360 x 360 UI can first be drawn centrally with black margins and touch coordinates translated to the logical canvas. This isolates board bring-up from a later 466 x 466 layout pass. Do not treat changing the resolution macro alone as a complete UI port: generated screens and images contain many fixed 360-pixel dimensions.

## Proposed implementation order

1. Add a dedicated `OBD_HW_VERSION_M5STOPWATCH` Kconfig choice. Keep original board builds selectable.
2. Add a StopWatch board layer using the official M5GFX/M5PM1/M5IOE1 versions and initialization order. Expose display flush, touch polling, and brightness to the existing LVGL 8 app. Avoid initializing the original ST77916 and CST816 drivers on this board.
3. Keep the original OBD/LVGL application intact for the first bring-up. Disable RS485 and ADS1115 paths on StopWatch. Confirm logo, page navigation, touch, brightness, and reboot on the actual device.
4. Adapt the UI and artwork to use the 466 x 466 display. Verify circular clipping, touch targets, font sizes, and memory use.
5. Test BLE with the user's actual ELM327-compatible adapter and vehicle. A working display does not establish OBD adapter or vehicle-profile compatibility.

## Local preparation status, 2026-09-28

- Connected serial port: COM6, ESP32-S3 revision 0.2, 16 MB flash, embedded 8 MB PSRAM, MAC `28:84:85:44:66:00` (read with esptool `flash_id`). No flash contents were saved or written.
- Existing local toolchain: ESP-IDF 5.4.4. The gauge lock file references 5.5.3 and the official StopWatch demo recommends 5.5.4. Use a matching 5.5.x installation for port builds before flashing.
- The repository tracks a `build/` tree containing another machine's CMake cache. Always use an out-of-tree build directory, for example `idf.py -B ..\\obd-build-stopwatch build`.
- `main/CMakeLists.txt` was changed to resolve theme and boot-media paths from its own file location; isolated CMake configure and partition generation passed on IDF 5.4.4. A complete firmware build and StopWatch firmware image have not yet been produced.
- User requested no original-firmware backup. No firmware has been flashed.
