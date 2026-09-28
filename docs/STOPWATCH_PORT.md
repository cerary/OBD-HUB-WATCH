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
- The new layout pass enlarges the needle speed dial, reflows TEMP and INFO,
  enlarges RPM and speed arcs, and adds a compact MINI badge. SETTINGS offers
  a saved JCW or GP visual badge choice, independent of the vehicle profile.
  The badge is drawn in LVGL; it is a stylized wordmark, not an OEM bitmap.
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
