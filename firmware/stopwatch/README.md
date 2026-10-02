# Current StopWatch application

Hardware: **M5Stack StopWatch C152 V1.0**. See [firmware overview](../README.md), [manifest](manifest.json) and [checksums](SHA256SUMS).

Use the supplied `OBD-HUB-WATCH-v1.0-20261002-charge-led-stable-app.bin` only with the matching device partition layout. This application was independently verified and boot-checked on 2026-10-02; it is not an empty-board full image. The embedded build version is `e17a46d-dirty`, with code recorded at commit `84a4fc0`; SHA-256 and source file hashes identify the installed code.

The charger signal must remain active for 5 seconds before the green LED turns on. USB charger-state and LED register transitions were measured, and the user confirmed page / button interaction leaves the visible lamp off. The original settings and JCW animation resource were verified preserved.

Source hashes in `manifest.json` normalize CRLF to LF, so Git line-ending conversion does not change source identity. The application binary hash is calculated over its exact bytes.
