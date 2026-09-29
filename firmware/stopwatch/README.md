# StopWatch 应用镜像

这两个文件只适用于 M5Stack StopWatch C152 的 `port/m5stopwatch` 分支，应用分区起始地址均为 `0x20000`。`stopwatch-buttons-20260930.bin` 是 2026-09-30 刷入并通过 `verify_flash` 的版本；`rollback-before-buttons-20260930.bin` 是刷机前从同一设备核对的回退版本。只刷应用镜像可保留 NVS、主题和 bootmedia。

详细 SHA-256、验证范围、构建方式和待办见项目根目录的 [STOPWATCH_HANDOFF.md](../../STOPWATCH_HANDOFF.md)。
