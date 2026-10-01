# Contributing

The supported hardware standard is **M5Stack StopWatch C152 V1.0**. Preserve upstream attribution and GPL-3.0. Do not generalize V1.0 rear 5V pin wiring to V1.0.1.

Describe concrete behavior and the validation boundary in changes: a successful build, host simulation, device boot, physical UI check and vehicle acceptance are different evidence. Run `python3 tools/test_host.py` for policy / cache / NVS changes; render actual LVGL pages for layout changes. Build with ESP-IDF 5.5.4 and `sdkconfig.stopwatch` before flashing.

Keep generated build trees, device backups / NVS dumps, raw personal logs and local absolute paths out of commits. Firmware updates need the matching partition layout and source. Preserve the existing setting layout / indices; migrate old blobs when adding data items.

For upstream bug reports, verify the latest original code, include reproducible steps and a small focused diagnosis, and distinguish port-specific changes from defects in upstream logic.
