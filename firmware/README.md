# Firmware — StopWatch C152 V1.0

**Hardware standard: M5Stack StopWatch C152 V1.0 · OBDLink CX / BLE.**

Download the [stopwatch-20261003 Release](https://github.com/cerary/OBD-HUB-WATCH/releases/tag/stopwatch-20261003). The current application is
[`OBD-HUB-WATCH-v1.0-20261003-cx-retry-app.bin`](https://github.com/cerary/OBD-HUB-WATCH/releases/download/stopwatch-20261003/OBD-HUB-WATCH-v1.0-20261003-cx-retry-app.bin), with
[manifest](stopwatch/manifest.json) and [SHA-256 checksums](stopwatch/SHA256SUMS).
This is the exact image independently flash-verified and boot-checked on 2026-10-03,
preserving all 23 logical NVS keys and JCW bootmedia.

The release includes persistent 0–100% feedback volume, the engine-off reconnect fix,
ACT half-brightness warning, LP-triggered power-off with external power present,
G-meter redraw / DMA improvements, bounded same-link CX verification retries and
independent diagnostic logs. See [complete release notes](../docs/releases/2026-10-03-stopwatch.md)
and [application details](stopwatch/README.md).

The preceding road test physically confirmed ACT dimming, power-off with USB still
connected and smooth G-meter operation. The current retry recovery and new-power / button
wake remain pending vehicle tests; rear wireless-5V transitions, gear shift accuracy and
temperature triggers also need separate validation. Host / LVGL regressions, independent
Flash verification and startup checks passed for the current app.

The unchanged [JCW bootmedia](https://github.com/cerary/OBD-HUB-WATCH/releases/download/stopwatch-20261003/bootmedia.raw.bin) is supplied independently.
Devices with this animation already installed do not need to rewrite it.
See [animation settings and partition requirements](../docs/BOOT-ANIMATION.md).
The immediately preceding [CX / ACT / G-render app](https://github.com/cerary/OBD-HUB-WATCH/releases/download/stopwatch-20261003/OBD-HUB-WATCH-v1.0-20261003-cx-act-g-render-app.bin)
is attached for rollback comparisons. The [2026-10-02 Release](https://github.com/cerary/OBD-HUB-WATCH/releases/tag/stopwatch-20261002)
retains its original snapshot and attachments.

This is **an application-only image**, not a merged first-flash package. The tested device
used `ota_0` at `0x20000`; another device may have `ota_1` active. Verify the actual
partition table and selected slot before an app-only write. For a new board, build from
source and use the generated IDF flash arguments in [BUILD.md](../docs/BUILD.md).
V1.0 rear pin 14 is 5V input; V1.0.1 pin 14 is a battery pin and must not receive 5V
using this wiring.

The embedded version `stopwatch-20261002-6-g01ebbc7-d` records the pre-publication Git state.
The release tag captures the published source; LF-normalized hashes in the manifest
identify the exact built files, while binary hashes cover the original bytes.
Firmware BINs are distributed as Release attachments. Older inherited binaries target
other boards or layouts. Device-specific backups, NVS / Bluetooth bonds and raw logs
remain local.
