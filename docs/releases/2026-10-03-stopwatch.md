# 2026-10-03 — CX 休眠联动、G 表流畅度与声音反馈

**硬件标准：M5Stack StopWatch C152 V1.0**

**适配器：OBDLink CX / BLE / STN2310 v5.8.1**

实车验证基于 MINI JCW F56 8AT。项目来自 [steveEcode/obd_brz_gauge（SKYGAUGE）](https://github.com/steveEcode/obd_brz_gauge)，保留上游历史、来源说明和 GPL-3.0 许可。

本次发布汇总 `stopwatch-20261002` 后的改动。应用附件与当前已刷入 Watch 的文件一致，没有为 GitHub 发布重新编译或刷机。

## 更新内容

- **操作提示音与音量：**修复 DMA 播放尚未完成就关闭功放、音量过低及初始化失败后不重试；使用双声道 80ms / 1600Hz 提示音、渐入渐出与尾部静音排空。`SETTINGS → FEEDBACK → SOUND / VOLUME` 提供 0–100% 滑条，默认 60%，0% 静音；松手保存，重启保留。声音开关、响度及滑条已获实物确认。温度报警双提示音留待后续实现。
- **熄火后停止重复重连：**修复没采到最后零转速时反复 ELM 初始化和 BLE 重连的问题。最后真实车速为 0、转速处于怠速范围，并且 RPM / 车速都多次明确返回无数据、ECU 持续静默 60 秒时，可进入 QUIET。缺失值不伪造为零；正常怠速新数据、短时断联和行车故障仍保留自恢复。
- **ACT 待机预告：**在已验证联动、已进入 QUIET 时收到 `ACT ALERT`，显示即将休眠，亮度临时减为当前设置的 50%；重复通知不叠加、不修改保存值，恢复连接后还原。ACT 本身不触发关机。
- **LP 直接关机：**收到经过配置验证的真实 `LP ALERT` 后停止查询、自愈与重连，进入 PMIC L0；USB 或背面 5V 仍在也能关机。保留新上电与按键唤醒配置；没有真实通知的 330 秒超时兜底，仍要求两路外部供电都消失 3 秒。
- **G 表流畅度：**外圈按周边区域局部重绘、点阵合成单层、轨迹逐点更新、隐藏页暂停计时器；AMOLED DMA 保留整行容量，传输实际脏区。保留原有布局与颜色，局部更新和完整绘制逐像素一致。
- **CX 验证恢复与日志：**每次 BLE 连接最多验证 3 次，失败后分别等 5 秒、15 秒，间隔内继续 OBD 读取。停车静默、休眠、OTA 禁止重试；查询保护连接代次，避免使用上一连接的延迟回复。独立记录真实 RPM / SPD 样本、linked / armed、验证次数和失败阶段；跟踪异步扫描状态，避免重复停止及开始 / 停止请求交错。

详细行为见[待机测试说明](../CX-待机测试.md)、[CX / G 表实现](2026-10-03-cx-sleep-g-render.md)、[验证重试](2026-10-03-cx-verification-retry.md)和[声音反馈](../SOUND-FEEDBACK.md)。

## 实车记录与验证范围

2026-10-03 00:59–01:24 的路试使用前一版 CX / ACT / G 修复应用，SHA-256 为 `2827f2e6843281104110e7011e502a62c2d87ee4175202e54326238ecf021b40`。本次应用保留这些逻辑并补充验证重试和日志；两版的测试证据分别记录。

| 实车事件 | 时间 / 结果 |
| --- | --- |
| 首个有效 OBD 响应 | 00:59:17；文件开头缺少初次 BLE / CX 验证，不能由此确定精确点火时间 |
| 停车静默 QUIET | 01:19:32；末次真实读数 739rpm / 0km/h，能停止反复查询与重连 |
| ACT 预告 | QUIET 后 240.111 秒；日志与实物确认亮度 70% → 35% |
| LP 休眠通知 | ACT 后 60 秒，即 QUIET 后约 300 秒 |
| Watch 关机请求 | LP 后 611ms；用户确认 USB 仍插着时完全黑屏 |
| G 表与切页 | 用户确认外圈换色和切页不卡 |

QUIET 到 LP 之间没有再次发查询或自动重连。约 19 分钟的行车日志未记录自愈 / 断联警告；日志并非逐 PID 遥测，不能据此推断所有数据始终新鲜，也不能算出实车最高转速、速度或温度。初始化日志缺失的原因尚不能仅凭该文件确定。

当前应用已通过生产配置函数重放、51 组 CX / 亮度 / 扫描行为、警报分包及真实配置回读、缓存 / NVS / 音量 / 充电灯 / 温度 / 外圈 / 峰值回归、18 个车型共 997 组档位样例和实际 LVGL 页面检查。相同的 120 帧 G 输入绘制像素从 10,655,073 降到 4,438,291，减少约 58%；主机绘制耗时减少约 42%，这不是实物 FPS 测量。

ESP-IDF 5.5.4 构建、独立 Flash 写入校验、35 秒启动及最终重启检查通过；系统区域、全部 23 个逻辑 NVS 键和 JCW 动画保留。**本次新增同连接验证重试仍待车辆日志确认；再次上电、按键及背面无线 5V 唤醒需单独实测。**档位实车换挡对照和温度触发验收仍待完成。

## 下载与升级

[GitHub Release](https://github.com/cerary/OBD-HUB-WATCH/releases/tag/stopwatch-20261003) 包含：

| 文件 | 用途 |
| --- | --- |
| `OBD-HUB-WATCH-v1.0-20261003-cx-retry-app.bin` | 当前已刷入应用，2,609,808 字节 |
| `OBD-HUB-WATCH-v1.0-20261003-cx-act-g-render-app.bin` | 前一版实车验收应用，供回退对照 |
| `bootmedia.raw.bin` | 独立 JCW 动画资源，内容未改变；已安装者无需重复更新 |
| `manifest.json` / `SHA256SUMS` | 构建身份、源文件哈希、分区和测试记录 / 下载校验 |

当前应用 SHA-256：

```text
5336db5d5b66dbb33650aece0d10b2afea46f41e2ebbe0030ac6dbd8b8c16d0b
```

固件内嵌版本是 `stopwatch-20261002-6-g01ebbc7-d`，记录刷机前的 Git 状态。发布标签是本次整理后的代码与文档快照；清单中的 LF 规范化源文件哈希绑定实际构建源码，应用校验按原始字节计算。

这是 **V1.0 应用更新文件**，不是空白设备首刷合并包。当前实测设备活动应用在 `0x20000`，其他设备可能在 `ota_1`；升级前核对实际分区。首次安装从源码按 [BUILD.md](../BUILD.md) 操作。V1.0 背面误标 `BAT` 的 14 脚是 5V IN；V1.0.1 同位置为电池端，不能照接 5V。私人设备备份、NVS / 蓝牙绑定和原始日志保留在本地。

## English summary

This release targets **M5Stack StopWatch C152 V1.0 with OBDLink CX**, preserving SKYGAUGE attribution and GPL-3.0. It fixes sound playback and adds persistent 0–100% volume, avoids repeated reconnects after engine shutdown, temporarily halves brightness on a parking ACT warning, powers off on verified LP ALERT even with USB present, reduces G-meter redraw / DMA work, and adds bounded same-link CX verification retries plus independent diagnostics.

The preceding road-test application passed ACT dimming, powered LP shutdown and G-meter smoothness acceptance. The current application passed production replay, host / LVGL regressions, independent flash verification and boot checks, preserving 23 logical NVS keys and JCW bootmedia. Actual retry recovery and new-power / button wake remain pending. Release attachments contain the exact installed application, the preceding tested app, unchanged bootmedia and checksums; the embedded build version is retained rather than rebuilt for publication.
