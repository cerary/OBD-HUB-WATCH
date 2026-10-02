# Current StopWatch application

Hardware: **M5Stack StopWatch C152 V1.0**. See [firmware overview](../README.md), [manifest](manifest.json) and [checksums](SHA256SUMS).

Use the supplied `OBD-HUB-WATCH-v1.0-20261002-gear-fix-app.bin` only with the matching device partition layout. This application was independently verified and boot-checked on 2026-10-02; it is not an empty-board full image. The embedded build version is `73e5883`, with code recorded at commit `73e5883`; SHA-256 and source file hashes identify the installed code.

The gear estimator now compares consistent total ratios without duplicating final drive. The JCW profile uses the owner-confirmed 215/40R18 PS5 nominal radius of 0.3146 m, with unsmoothed speed input. Unmatched ratios hold the old gear for no more than 1 second, then show `--` and an empty arc; disconnect, stale data and vehicle changes reset estimation. Independent circumference tests cover 997 cases across 18 actual profiles, plus production LVGL tests. Physical shift accuracy remains pending; stationary N is only a placeholder. All 22 logical NVS keys and JCW bootmedia were verified preserved. See [gear behavior and limitations](../../docs/VEHICLE-RANGES.md).

Two-stage temperature advisories use CLT 115/120°C, OIL 125/135°C and IAT 60/80°C by default. New samples confirm the hold duration, and recovery hysteresis prevents color chatter. CLT/OIL warn across live dials; IAT is local and orange at its high stage. These are user advisory thresholds, not factory ECU protection limits. The original ALARM slider adjusts the high threshold; OFF disables both stages. First boot enables previously disabled temperature defaults once; all unrelated NVS keys and JCW bootmedia were verified preserved. See [temperature behavior and settings](../../docs/TEMPERATURE-ALERTS.md). Vehicle validation is pending.

The JCW F56 8AT profile uses display ranges of 0–7000 rpm and 0–280 km/h. BOOST ticks now read 0.0 / 0.5 / 1.0 / 1.5 / 2.0 bar, and voltage / AFR ticks and alarm labels use the same decimal units as their readings. These are display ranges, not measured ECU limits. The existing one-byte MAP acquisition still limits displayed boost to about 1.5 bar gauge pressure. See [vehicle ranges and data limits](../../docs/VEHICLE-RANGES.md). Production LVGL regressions passed; physical confirmation of the new ranges and labels is pending.

The needle page has a 426px dial, a roughly 5px gap inside the status ring, 24-size numerals, and brighter/larger ticks. The needle was shortened by 9px following user feedback. Actual LVGL rendering confirms five blank pixel rows between the tip and a major tick; the user confirmed the corrected needle length and spacing are suitable, with normal display.

Data pages start with `--` and synchronously render fresh cached readings when entered. Valid zero and neutral readings remain visible. Production LVGL tests cover first entry, stale/disconnected data, repeated entry, timer wake and startup sweep; the user confirmed direct `--` with no jump on first entry after restart.

The charger signal must remain active for 5 seconds before the green LED turns on. USB charger-state and LED register transitions were measured, and the user confirmed page / button interaction leaves the visible lamp off. All settings unrelated to the one-time temperature adoption, and the JCW animation resource, were verified preserved.

Source hashes in `manifest.json` normalize CRLF to LF, so Git line-ending conversion does not change source identity. The application binary hash is calculated over its exact bytes.
