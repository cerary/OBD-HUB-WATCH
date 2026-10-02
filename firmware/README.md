# Firmware — StopWatch C152 V1.0

The current application is [stopwatch/OBD-HUB-WATCH-v1.0-20261002-gear-fix-app.bin](stopwatch/OBD-HUB-WATCH-v1.0-20261002-gear-fix-app.bin), with [manifest](stopwatch/manifest.json) and [SHA-256](stopwatch/SHA256SUMS).

The independent [JCW bootmedia resource](stopwatch/bootmedia.raw.bin) is available with the matching v3-compatible application. See [animation settings and partition requirements](../docs/BOOT-ANIMATION.md). The [2026-10-02 Release](https://github.com/cerary/OBD-HUB-WATCH/releases/tag/stopwatch-20261002) also provides the current files as download attachments.

It is the exact app independently verified and boot-checked on 2026-10-02. It includes all README UI changes. The UI / vehicle acceptance test is still in progress; real LP ALERT delivery and rear wireless-5V wake remain pending.

Gear estimation now uses consistent total ratios and the owner's 215/40R18 PS5 nominal tire radius of 0.3146 m. The ratio uses speed before display smoothing, and unmatched gears expire to `--` after 1 second. Independent circumference tests covered 997 cases across 18 profiles; actual LVGL, flash verification and startup checks passed. All 22 logical NVS keys and the JCW boot-animation resource were preserved. Actual shift accuracy still needs vehicle comparison; stationary N remains a placeholder.

The immediately preceding [temperature application](stopwatch/OBD-HUB-WATCH-v1.0-20261002-temperature-alerts-app.bin) and its checksum remain available for rollback.

The needle dial is 426px across with zero widget padding, leaving about 5px inside the status ring. Numerals use the 24-size project font; major ticks are 3×14px and minor ticks 2×9px, with separate grey levels. The needle is shortened by 9px to leave 5px inside the major ticks. A rendered alignment frame has five blank pixel rows between tip and tick; the user confirmed the corrected needle length and spacing are suitable, with normal display.

Data pages now initialize with `--` and render fresh cached values before their first visible frame. Genuine `0/N` readings are retained. Actual LVGL tests, the full build, independent flash verification and the 35-second boot check passed; the user confirmed direct `--` with no jump on first entry after restart.

This image requires 5 seconds of continuous charger status before enabling the green LED. The preceding charging-LED test log showed ON after 5243ms, OFF 8ms after the charger released its status, and five 1–3.5 second charging pulses ignored. The user confirmed the visible LED stays OFF after page changes / button presses. Battery-only, unplug/shutdown and rear wireless-5V LED transitions remain untested. Battery percentage is estimated and does not control the LED.

Temperature advisories use CLT 115/120°C, OIL 125/135°C and IAT 60/80°C. New samples confirm hold times, recovery hysteresis prevents color chatter, and invalid / stale / disconnected data clears alerts. CLT/OIL are global on live dials; IAT is local. These are custom observation thresholds, not factory ECU protection limits. See [temperature alerts](../docs/TEMPERATURE-ALERTS.md).

First boot adopted the three temperature defaults once. All 20 unrelated logical NVS keys and the JCW boot-animation resource were independently verified unchanged. The connected Watch had INTRO set to OFF and its default page set to G FORCE; those choices were preserved.

The previous UI-only application remains available as [stopwatch/OBD-HUB-WATCH-v1.0-20261002-app.bin](stopwatch/OBD-HUB-WATCH-v1.0-20261002-app.bin) for rollback; its SHA-256 is also retained.

This is **an application-only image for M5Stack StopWatch C152 V1.0**, not a merged first-flash package. A device with the matching partition layout used `ota_0` at `0x20000`; other devices may have `ota_1` active. Confirm the active partition before an app-only write. Build from source and use IDF flash arguments for a new board. See [BUILD.md](../docs/BUILD.md).

Older inherited binaries target different boards or earlier StopWatch builds and are not this project's current release. They were removed from Git's current index while local working copies remain available for recovery. Device-specific backups and raw logs are not published.
