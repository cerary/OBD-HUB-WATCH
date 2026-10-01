# StopWatch current handoff — 2026-10-02

Read the root [README](README.md) first. Supported hardware is **M5Stack StopWatch C152 V1.0**, with OBDLink CX BLE and ESP-IDF 5.5.4 / LVGL 8.4.0. The current project is **OBD HUB WATCH**; older SkyGarage / SKYGAUGE build notes describe earlier revisions.

Current application and exact hash: [firmware/stopwatch/manifest.json](firmware/stopwatch/manifest.json). It contains the source state flashed on 2026-10-02, including curved power text, project credits, circular navigation, 15px G labels / 10px corner ends, and unified 416px gauge tracks.

## What is confirmed

- User's earlier vehicle test produced valid OBD readings and recovered from intermittent BLE disconnects.
- Latest application built, wrote successfully, independently verified, then booted once normally during a 35-second log capture. Display / LVGL, external power detection and CX module initialization succeeded without panic or watchdog reset. Settings were preserved.
- Host policy / cache / NVS tests and real LVGL geometry, navigation, freshness, peak marker, alarm background and page-recreation checks passed.
- README images are actual UI-code renders with example inputs; they are not fresh device photographs.

## Open acceptance items

1. Latest physical UI, buttons, gestures and G behavior during the user's current vehicle test.
2. Actual OBDLink CX `LP ALERT` delivery after ignition-off and command silence, followed by suppression of reconnect and Watch power-off after external supply disappears.
3. Wireless receiver 5V stability, temperature and current; rear pin detection, power-off and rear 5V power-on wake on V1.0.
4. First-time empty-NVS binding/default behavior and complete compatibility of retained upstream features on this board.

Do not classify idle `800 rpm / 0 km/h` as parking. Only genuine zero-value samples may support the parking candidate; invalidated display zeros are not ECU data. USB loss alone does not currently bypass the verified CX-sleep state.

Build instructions: [docs/BUILD.md](docs/BUILD.md). Historical port / asset provenance: [docs/STOPWATCH_PORT.md](docs/STOPWATCH_PORT.md). Personal COM identifiers, MAC addresses, flash backups and raw vehicle logs are intentionally excluded from the public handoff.
