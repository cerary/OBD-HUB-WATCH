# Firmware — StopWatch C152 V1.0

The current application is [stopwatch/OBD-HUB-WATCH-v1.0-20261002-app.bin](stopwatch/OBD-HUB-WATCH-v1.0-20261002-app.bin), with [manifest](stopwatch/manifest.json) and [SHA-256](stopwatch/SHA256SUMS).

It is the exact app independently verified and boot-checked on 2026-10-02. It includes all README UI changes. The UI / vehicle acceptance test is still in progress; real LP ALERT delivery and rear wireless-5V wake remain pending.

This is **an application-only image for M5Stack StopWatch C152 V1.0**, not a merged first-flash package. A device with the matching partition layout used `ota_0` at `0x20000`; other devices may have `ota_1` active. Confirm the active partition before an app-only write. Build from source and use IDF flash arguments for a new board. See [BUILD.md](../docs/BUILD.md).

Older inherited binaries target different boards or earlier StopWatch builds and are not this project's current release. They were removed from Git's current index while local working copies remain available for recovery. Device-specific backups and raw logs are not published.
