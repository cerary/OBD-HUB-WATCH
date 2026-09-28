# M5Stack StopWatch port

This branch ports `steveEcode/obd_brz_gauge` to the M5Stack StopWatch C152
(ESP32-S3, 16 MB flash, 8 MB PSRAM). The hardware reference is the M5Stack
StopWatch UserDemo. The desktop project root is
`C:\Users\cerar\Desktop\obd-stopwatch`.

## Device status (2026-09-29)

- COM6 is the user's ESP32-S3 StopWatch, MAC `28:84:85:44:66:00`.
- ESP-IDF 5.5.4 builds the application with `sdkconfig.stopwatch`.
- CO5300 display and CST820 touch work. The user has verified page navigation,
  one tap per page change, the shared 452-pixel white bezel ring, and BLE SCAN
  layout. The previous full application was stable while idle.
- The layout pass enlarges the needle speed dial, reflows TEMP and INFO,
  and enlarges RPM and speed arcs. MINI emblems now follow the selected vehicle:
  `JCW F56 8AT` displays the F56-era JCW emblem and `GP3 F56 8AT` displays the
  original GP3 grille badge silhouette. The independent MINI LOGO setting was
  removed. Other vehicle profiles show the SKY GAUGE project name.
- The user confirmed the GP glyph and baseline on the real screen. On StopWatch,
  VEHICLE waits 2 seconds after the last release before saving and rebooting;
  another drag restarts the timer. This was also verified on the device.
- The F56 JCW 8AT and GP3 8AT profiles use MINI's published 8-speed ratios and
  2.955 final drive. GP3 was appended to preserve saved profile indices. Gear
  detection and actual OBD data still need validation on the user's car.
- Badge source: MINI Spain's F56 JCW GP page provides the original
  `jcw_logo.svg`, archived as `docs/mini_jcw_f56_source.svg`. The GP3 badge
  uses the user's full-resolution red GP reference with its white outline,
  archived as `docs/mini_gp3_reference.webp`. Only the two letters are
  extracted; the source orientation is preserved. The derived PNG is
  archived as `docs/mini_gp3_flat.png`. Both emblems are embedded as
  RGB565A8 LVGL images.
  Because StopWatch enables `LV_COLOR_16_SWAP`, image RGB565 bytes are
  stored high-byte-first before alpha, matching the existing assets.
  Sources: https://www.mini.es/es_ES/home/range/mini-jcw-gp.html and
  https://www.outmotoring.com/front-bumper-upper-grill-trim-mini-jcw-gp3-51139481307.html
- Drivetrain source: MINI's 2021 3 Door Product Guide, pages 1-2:
  https://www.press.bmwgroup.com/canada/article/attachment/T0305951EN/446631
- The first layout build rebooted because the double 40-line DMA draw buffers
  exhausted internal RAM when ESP-NOW initialized Wi-Fi. On StopWatch, the
  second build uses double 20-line buffers. The boot log then showed 88,323
  bytes of free internal RAM before ESP-NOW, successful ESP-NOW master startup,
  and no reset during a 20-second log capture. The user then confirmed more
  than one minute of idle stability and successful GP selection on screen.
- The OBDLink CX adapter is not powered. Real OBD readings remain untested.

## Build from the desktop project

```powershell
Set-Location 'C:\Users\cerar\Desktop\obd-stopwatch\work\obd_brz_gauge'
.\tools\prepare_stopwatch_deps.ps1
& 'C:\Users\cerar\esp\esp-idf-v5.5.4\export.ps1'
$env:STOPWATCH_DEPS_DIR = 'C:\Users\cerar\Desktop\obd-stopwatch\work'
idf.py -B ..\obd-build-stopwatch-clean -D 'SDKCONFIG=sdkconfig.stopwatch' build
```

The dependency preparation script pins M5GFX 0.2.19, M5PM1 1.0.6, and
M5IOE1 1.0.8. It also applies two local build/runtime fixes to these sibling
components. The current verified incremental build cache is
`work\obd-build-stopwatch`; it retains CMake paths through projectless-task
junctions. Use a fresh build directory for an independent clean build.

The app image belongs at flash offset `0x20000` in `ota_0`. The full image
`outputs\StopWatch-SkyGarage-正式版-整包.bin` is the previous known-good
rollback image. Flashing the app alone preserves NVS settings and bootmedia.

## Follow-up validation

1. Review RPM, speed, needle, TEMP, INFO, settings, and gear pages from device
   photos; adjust only the StopWatch conditional layout when needed.
2. Pair the OBDLink CX after it is powered and verify actual vehicle readings.
