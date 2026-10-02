# Current StopWatch application

Hardware: **M5Stack StopWatch C152 V1.0**. See [firmware overview](../README.md), [manifest](manifest.json) and [checksums](SHA256SUMS).

Use the supplied `OBD-HUB-WATCH-v1.0-20261002-vehicle-ranges-app.bin` only with the matching device partition layout. This application was independently verified and boot-checked on 2026-10-02; it is not an empty-board full image. The embedded build version is `75bd702-dirty`, with code recorded at commit `8d331b2`; SHA-256 and source file hashes identify the installed code.

The JCW F56 8AT profile uses display ranges of 0–7000 rpm and 0–280 km/h. BOOST ticks now read 0.0 / 0.5 / 1.0 / 1.5 / 2.0 bar, and voltage / AFR ticks and alarm labels use the same decimal units as their readings. These are display ranges, not measured ECU limits. The existing one-byte MAP acquisition still limits displayed boost to about 1.5 bar gauge pressure. See [vehicle ranges and data limits](../../docs/VEHICLE-RANGES.md). Production LVGL regressions passed; physical confirmation of the new ranges and labels is pending.

The needle page has a 426px dial, a roughly 5px gap inside the status ring, 24-size numerals, and brighter/larger ticks. The needle was shortened by 9px following user feedback. Actual LVGL rendering confirms five blank pixel rows between the tip and a major tick; the user confirmed the corrected needle length and spacing are suitable, with normal display.

Data pages start with `--` and synchronously render fresh cached readings when entered. Valid zero and neutral readings remain visible. Production LVGL tests cover first entry, stale/disconnected data, repeated entry, timer wake and startup sweep; the user confirmed direct `--` with no jump on first entry after restart.

The charger signal must remain active for 5 seconds before the green LED turns on. USB charger-state and LED register transitions were measured, and the user confirmed page / button interaction leaves the visible lamp off. The original settings and JCW animation resource were verified preserved.

Source hashes in `manifest.json` normalize CRLF to LF, so Git line-ending conversion does not change source identity. The application binary hash is calculated over its exact bytes.
