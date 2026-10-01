# AFR persistence and default-alarm defects in upstream

Checked against `steveEcode/obd_brz_gauge` main commit `826a5071c842558771498507aa3335e906405d71` (2026-10-02). The upstream repo had no matching open issue when checked.

## Reproduction with original code

The comparison compiles upstream `nvs_storage.c` and its original header unchanged. In-memory NVS, time and hardware API stubs replace the device; the initialization/clamping/default logic is original production C.

```text
upstream: fresh needle=0 chart=0 AFR_alarm=0
upstream: saved needle=0 chart=8 AFR_alarm=0
fixed: fresh needle=0 chart=0 AFR_alarm=32767
fixed: saved needle=11 chart=11 AFR_alarm=32767
```

A saved settings blob contains `needle_source_idx=11` and `chart_source_idx=11`, the valid `DISP_ITEM_AFR`. `nvs_storage_init()` in upstream rejects both with `>= 11`, resetting them to CLT (0) and OILP (8). Selecting AFR works until the next boot.

Also, `CHART_ALARM_N` is 12 but `s_chart_alarm` has only 11 explicit initializers. C initializes its last entry to 0, so the fresh AFR threshold is active at zero instead of OFF (`32767`); a normal positive AFR value is then treated as over threshold.

## Run the comparison

With this project and a separate upstream checkout:

```bash
git -C /path/to/upstream checkout 826a5071c842558771498507aa3335e906405d71
python3 tools/compare_upstream_afr.py --upstream /path/to/upstream
```

Expected behavior: retain valid index 11 across initialization; set a fresh AFR threshold to OFF; preserve the first 11 existing thresholds and valid custom AFR thresholds when migrating old alarm blobs. Our host regression covers fresh state, saved AFR selection, old 11-entry blobs and custom thresholds.

This report concerns upstream NVS logic, not StopWatch-specific hardware or the still-pending CX sleep acceptance test.
