# StopWatch V1.0 build and flash

This repository's supported target is M5Stack StopWatch C152 **V1.0**. Use `sdkconfig.stopwatch`, ESP-IDF **5.5.4**, 16MB Flash / 8MB PSRAM, and the included `partitions.csv`. Upstream board configurations are retained as references, not as this fork's tested hardware standard.

## Reproducible dependencies

Run `tools/prepare_stopwatch_deps.ps1` before configuring. It clones dependencies into the repository's parent directory and verifies revisions:

| Component | Version / revision | Project |
| --- | --- | --- |
| M5GFX | 0.2.19 / `53a7184` | m5stack/M5GFX |
| M5PM1 | 1.0.6 / `8f1f1a6` | m5stack/M5PM1 |
| M5IOE1 | 1.0.8 / `37db048` | m5stack/M5IOE1 |
| BMI270 SensorAPI | `41129fcfe39c583ee5462d79195741945d51c1fe` | boschsensortec/BMI270_SensorAPI |
| LVGL | 8.4.0 | IDF `dependencies.lock` |

The preparation script makes M5PM1 / M5IOE1 see the same M5GFX I2C ABI in every translation unit. M5GFX's AMOLED transfer also receives a null-DMA-allocation fallback to the register path. These changes are local dependency patches, not claims that upstream libraries include them.

## Build

Activate the ESP-IDF PowerShell environment, then:

```powershell
.\tools\prepare_stopwatch_deps.ps1
$env:STOPWATCH_DEPS_DIR = (Split-Path (Get-Location).Path -Parent)
idf.py -B build-stopwatch '-DSDKCONFIG=sdkconfig.stopwatch' build
```

Use a new build directory when moving the checkout; CMake caches contain absolute paths. Do not reuse another board's cached libraries or flash images. Linux users can prepare the same revisions and patches with the Python helper:

```bash
python3 tools/prepare_stopwatch_deps.py
export STOPWATCH_DEPS_DIR="$(dirname "$PWD")"
idf.py -B build-stopwatch -DSDKCONFIG=sdkconfig.stopwatch build
```

GitHub Actions runs the host regressions and a full StopWatch V1.0 build in the official `espressif/idf:v5.5.4` container. A build checks compilation; it does not validate physical power or vehicle behavior.

The application name is `obd_hub_watch`. Do not use stale `obd_brz_gauge.bin` files from older build directories. Builds derive Git metadata from the checkout; the published tested binary predates the publication commit and its exact identity is recorded by SHA-256.

## Flash

For a new board, confirm V1.0 and use IDF's generated flash arguments:

```powershell
idf.py -B build-stopwatch '-DSDKCONFIG=sdkconfig.stopwatch' -p COMx flash monitor
```

Current partition layout:

| Partition | Offset | Size |
| --- | --- | --- |
| nvs | 0x9000 | 0x6000 |
| otadata | 0xF000 | 0x2000 |
| phy_init | 0x11000 | 0x1000 |
| ota_0 | 0x20000 | 0x300000 |
| ota_1 | 0x320000 | 0x300000 |
| theme_0 | 0x620000 | 0x400000 |
| bootmedia | 0xA20000 | 0x5E0000 |

An application-only update must target the **actual active OTA partition**, after comparing the device partition table and saving a backup. The currently verified device used `ota_0` at `0x20000`. Independent `esptool verify_flash` and boot-log checks verified the update; NVS / OTA / system bytes were unchanged. Device-specific backups and personal raw logs are kept locally, not published.

The app-only download does not include bootloader / partition / OTA initialization or the optional boot-media data. Build from source for a first flash. Default boot-media settings fall back when media is unavailable; see retained upstream boot-media documentation for custom assets.

## Checks and gallery

Linux / WSL with GCC and Python 3:

```bash
python3 tools/test_host.py
python3 tools/ui_preview/render.py --output docs/images
python3 -m pip install Pillow
python3 tools/ui_preview/assemble.py --output docs/images
```

The UI renderer compiles production LVGL 8.4.0 sources and current page constructors. BLE, time, NVS, PMIC and IMU inputs are fixtures. It does not attach to a vehicle, flash a device, or imply physical validation. Each regenerated gallery includes source hashes in its manifest.
