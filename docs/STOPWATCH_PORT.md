# M5Stack StopWatch port status

Upstream is `steveEcode/obd_brz_gauge` at `528f542`. This branch targets
M5Stack StopWatch C152 (ESP32-S3, 16 MB flash, 8 MB PSRAM), using the official
`m5stack/M5StopWatch-UserDemo` at `6b4aa12` as the hardware reference.

## Verified on the connected device

- COM6 identified as ESP32-S3 revision 0.2, MAC `28:84:85:44:66:00`.
- ESP-IDF v5.5.4 is installed at `C:\Users\cerar\esp\esp-idf-v5.5.4`.
- PMIC and IOE initialization powers the CO5300 AMOLED. Its 468 x 466 panel
  has a centered 466 x 466 round UI canvas. A frame buffer is required for
  M5GFX primitive rendering on this panel.
- A separate display bring-up firmware showed text, circles and a cross.
  The user confirmed all of them. A separate 466 x 466 SkyGarage dial preview
  showed an RPM and speed page; the user confirmed a single tap switches pages.
- The preview has fixed zero readings. It does not run LVGL or read OBD data.

## Full application changes in this branch

- Added `OBD_HW_VERSION_M5STOPWATCH` and its own CO5300/CST820B/M5PM1/M5IOE1
  bridge in `main/stopwatch`. The old ST77916, CST816 and GPIO backlight init
  are bypassed for this board.
- LVGL is configured for a 466 x 466 canvas. The bezel artwork, RPM, speed and
  gear arc sizes have a first StopWatch layout pass. Other pages retain their
  centered layout pending an on-device full-application review.
- RS485 brake sensing and ADS1115 oil sensing are not started on StopWatch.
- The application build uses an out-of-tree build directory because upstream
  tracks `build/` from another machine.

The full application has **not** completed compilation or been flashed yet.
The first ESP-IDF 5.5.4 build was stopped after dependency compilation proved
unusually slow on this Windows host. BLE/OBD, OTA, all pages, full-app touch and
brightness therefore remain unverified. Keep the preview firmware as the known
working display baseline until the full image builds and passes device tests.

## Reproducing the full build

From PowerShell, with ESP-IDF v5.5.4 installed:

```powershell
cd C:\Users\cerar\Documents\Codex\2026-09-28\g-i\work\obd_brz_gauge
.\tools\prepare_stopwatch_deps.ps1
& 'C:\Users\cerar\esp\esp-idf-v5.5.4\export.ps1'
& 'C:\Users\cerar\.espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe' 'C:\Users\cerar\esp\esp-idf-v5.5.4\tools\idf.py' -B '..\obd-build-stopwatch' '-DSDKCONFIG=C:\Users\cerar\Documents\Codex\2026-09-28\g-i\work\obd_brz_gauge\sdkconfig.stopwatch' build
```

The dependency script clones pinned M5GFX 0.2.19, M5PM1 1.0.6 and M5IOE1
1.0.8 beside this repository and adds `M5GFX` to the latter two components'
public include dependencies. That patch is required: otherwise the M5 driver
headers select different C++ class layouts in the main and library translation
units, causing a runtime vtable overwrite.

## Remaining acceptance

1. Finish a full application build and flash its partition table, bootloader,
   app and boot-media images to the StopWatch.
2. Verify the LVGL logo, RPM/speed/gear and settings pages on the round screen;
   adjust labels and touch targets from a device photo.
3. Verify brightness, reboot, UI touch navigation and memory stability.
4. Pair the user's actual BLE OBD adapter and verify readings in a vehicle.

The user approved replacing the factory firmware without a backup. No factory
image was retained.
