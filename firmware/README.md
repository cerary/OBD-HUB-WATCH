# Firmware — StopWatch C152 V1.0

The current application is [stopwatch/OBD-HUB-WATCH-v1.0-20261002-needle-dial-short-app.bin](stopwatch/OBD-HUB-WATCH-v1.0-20261002-needle-dial-short-app.bin), with [manifest](stopwatch/manifest.json) and [SHA-256](stopwatch/SHA256SUMS).

It is the exact app independently verified and boot-checked on 2026-10-02. It includes all README UI changes. The UI / vehicle acceptance test is still in progress; real LP ALERT delivery and rear wireless-5V wake remain pending.

The needle dial is 426px across with zero widget padding, leaving about 5px inside the status ring. Numerals use the 24-size project font; major ticks are 3×14px and minor ticks 2×9px, with separate grey levels. The needle is shortened by 9px to leave 5px inside the major ticks. A rendered alignment frame has five blank pixel rows between tip and tick; the user confirmed the corrected needle length and spacing are suitable, with normal display.

Data pages now initialize with `--` and render fresh cached values before their first visible frame. Genuine `0/N` readings are retained. Actual LVGL tests, the full build, independent flash verification and the 35-second boot check passed; the user confirmed direct `--` with no jump on first entry after restart.

This image requires 5 seconds of continuous charger status before enabling the green LED. The preceding charging-LED test log showed ON after 5243ms, OFF 8ms after the charger released its status, and five 1–3.5 second charging pulses ignored. The user confirmed the visible LED stays OFF after page changes / button presses. Battery-only, unplug/shutdown and rear wireless-5V LED transitions remain untested. Battery percentage is estimated and does not control the LED.

The JCW boot-animation resource and saved settings were independently verified unchanged. The connected Watch had INTRO set to OFF and its default page set to G FORCE; those choices were preserved.

The previous UI-only application remains available as [stopwatch/OBD-HUB-WATCH-v1.0-20261002-app.bin](stopwatch/OBD-HUB-WATCH-v1.0-20261002-app.bin) for rollback; its SHA-256 is also retained.

This is **an application-only image for M5Stack StopWatch C152 V1.0**, not a merged first-flash package. A device with the matching partition layout used `ota_0` at `0x20000`; other devices may have `ota_1` active. Confirm the active partition before an app-only write. Build from source and use IDF flash arguments for a new board. See [BUILD.md](../docs/BUILD.md).

Older inherited binaries target different boards or earlier StopWatch builds and are not this project's current release. They were removed from Git's current index while local working copies remain available for recovery. Device-specific backups and raw logs are not published.
