# 2026-10-02 — StopWatch V1.0 更新说明

**硬件：M5Stack StopWatch C152 V1.0；适配器：OBDLink CX BLE。**
本次汇总前次 GitHub 同步后完成并刷入设备的更新。项目继续基于
[steveEcode/obd_brz_gauge / SKYGAUGE](https://github.com/steveEcode/obd_brz_gauge)，
保留上游来源与 GPL-3.0 许可，由 T A O 改造优化。

## 主要变化

### 档位估算修复

原换算额外乘入一次终传比，造成错误匹配或长时间保留旧档位。
现在以轮胎周长推算总传动比，与“档位齿比 × 终传比”比较；
容差重叠时选范围内最近中心，范围外不强制选档。
JCW 使用车主确认的 **215/40 R18 米其林 PS5** 名义半径 **0.3146 m**、终传 **2.955** 和 8 个原厂齿比。

估算采用平滑前车速。失配时最多保留旧档 **1 秒**，随后显示 `--` 并清空档位弧；
断联、数据过期及车型切换清除估算历史。0 km/h 的 `N` 仍为静止占位，不能识别真实 P/N/D/R。
变矩器滑差、换挡过程和独立 OBD 采样仍可能影响估算，需要实车对照。

运行实际 C 代码，以独立圆周公式生成 **997 组输入、覆盖 18 个车型**。
当前 JCW 模拟中，3000 rpm / 约 59 km/h 判为 3 档，3000 rpm / 约 179 km/h 判为 8 档。
实际 LVGL 验证首帧、失配 `--`、空弧、恢复及断联。

实际代码渲染帧：[3 档](../images/gear-estimated-3.png)、[8 档](../images/gear-estimated-8.png)、
[失配占位](../images/gear-estimated-unknown.png)。图片为模拟输入，并非实车测量。
详细原理见[车型与估算说明](../VEHICLE-RANGES.md)。

### 温度两级提示

| 参数 | 黄色提前提醒 | 高级提醒 |
| --- | --- | --- |
| CLT 冷却液 | 115°C，持续 10 秒 | 120°C，持续 3 秒，红色 |
| OIL 机油 | 125°C，持续 10 秒 | 135°C，持续 3 秒，红色 |
| IAT 进气 | 60°C，持续 30 秒 | 80°C，持续 10 秒，橙色 |

需要新的有效采样确认持续时间；恢复温差避免颜色来回变化，过期 / 断联清除提醒。
CLT / OIL 在实时页面提示，IAT 仅提示所在参数，避免当作发动机过热报警。
原生 ALARM 滑条调整高级阈值，OFF 关闭两级提醒。
默认温度是用户日常观察门槛，**不是原厂 ECU 保护阈值**；详见[温度说明](../TEMPERATURE-ALERTS.md)。

实际代码效果：[温度提前提醒](../images/temperature-warm.png)、[温度高级提醒](../images/temperature-high.png)、
[跨页黄色提示](../images/rpm-clt-warm.png)、[跨页红色提示](../images/rpm-clt-high.png)。
原生设置：[CLT](../images/clt-alert-settings.png)、[IAT](../images/iat-alert-settings.png)、[OIL](../images/oil-alert-settings.png)。均为模拟采样。

### 仪表范围与指针布局

- JCW 转速 / 车速范围统一为 **0–7000 rpm / 0–280 km/h**，覆盖官方功率区间与 246 km/h 极速并留显示余量；量程不代表 ECU 限制。
- BST 按 bar 显示小数刻度与报警，电压 / AFR 指针保留小数精度。当前标准 PID 010B 只有一字节 MAP，减去固定大气参考后显示最多约 **1.5 bar 表压**；2.0 bar 量程不意味着已经能测到 2.0 bar 表压。
- 指针刻度盘外径 **426px**，距外圈内沿约 **5px**；数字使用项目 24 字号，刻度分级提亮。红针尖端距主刻度内端约 **5px**，用户已确认实物长度和显示正常。
- 数据页在首帧显示新鲜缓存或 `--`，消除初次切页先闪 `0/N` 的跳变；真实零值与静止占位保留，扫表正常运行。用户已确认重启后无跳变。

### 充电灯与开机动画

充电灯由实际充电信号控制，连续有效 **5 秒**才点亮，短暂变化不累计；
充电结束、外部供电消失或状态未知时清除。电池百分比不参与判断。
USB 实测及用户反馈确认切页 / 按键后实灯保持熄灭；背面无线供电的实灯变化仍待安装测试。

动画支持原生 **466×466、30fps RGB565 v3**，兼容旧格式。
JCW 动画共 190 帧、约 **6.33 秒**，按原视频长边缩放，重建圆形背景、稳定徽标并修复中心暗斑。
真实播放器 / LVGL 的全帧量化像素一致，设备日志曾确认 **190/190 帧、6,279ms** 完整播放。
动画资源独立于主程序；在 `MORE → MULTI-GAUGE → INTRO` 选择 VIDEO / OFF，下次启动生效。
本次刷机保留原有选择；该测试设备当前为 OFF，没有为了发布更改为 VIDEO。
来源、格式和更新方式见[开机动画说明](../BOOT-ANIMATION.md)。

## 当前固件与验证

- 应用：`OBD-HUB-WATCH-v1.0-20261002-gear-fix-app.bin`，**2,597,488 字节**；嵌入版本 **`73e5883`**，代码提交 `73e5883fd4ae1045962642df69adfdaee072faa8`。
- SHA-256：`8a8d29011469562e16654a8457cbc0ec3deb045ba5b04d6625bbd51a1c27a050`。
- ESP-IDF **5.5.4** / LVGL **8.4.0**；主机策略、缓存 / NVS 与实际 LVGL 回归通过。
- 新应用独立 Flash 校验、35 秒启动及最终重启检查通过，无崩溃；日志确认新的 JCW 齿比 / 轮胎参数。
- 最新刷机后 **22 个逻辑 NVS 键全部保留**；系统 / OTA 选择区域与开机动画资源校验通过。
- 当前固件、旧版回退文件、清单与校验值见[固件目录](../../firmware/README.md)。设备备份、身份及个人原始日志保留在本地，不随发布上传。

这是 **StopWatch V1.0 的应用更新与独立动画资源**，不是空板完整合并固件。
现有设备需核对实际活动 OTA 分区；已验证设备使用 `ota_0` 的 `0x20000`，不能直接套用到所有设备。
动画偏移 `0xA20000`、大小分区 `0x5E0000` 也须与设备清单一致。
**V1.0.1 的背面引脚为电池端，不能按 V1.0 接入 5V。**编译与刷写见[构建说明](../BUILD.md)。

## 尚待实车确认

档位换挡对照、温度高级提醒的实车触发、真实 `LP ALERT` 与熄火待机链、
背面无线 5V 断电 / 重新上电唤醒，以及该供电方式下的实灯变化。
桌面模拟、寄存器回读及启动日志不代替这些现场测试。

## English summary

This StopWatch C152 V1.0 update fixes gear-ratio conversion, adds temperature advisories,
aligns JCW dial ranges and decimal units, improves needle geometry and first-frame data rendering,
stabilizes the charging LED, and supports native-resolution v3 JCW startup playback.
The installed `73e5883` image passed production host/LVGL tests, independent flash verification and boot checks;
all 22 logical NVS keys and bootmedia were preserved. Gear and temperature vehicle acceptance,
real LP ALERT delivery, and rear wireless-power transitions remain pending.
