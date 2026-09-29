# StopWatch 项目交接（2026-09-30）

下次从本项目目录打开时，先确认当前 Git 分支为 `port/m5stopwatch`。这是 M5Stack StopWatch C152（ESP32-S3）的移植分支；`main` 是其他硬件的主线，分区和外设配置不能直接混刷。本机项目路径为 `C:\Users\cerar\Desktop\obd-stopwatch\work\obd_brz_gauge`。

## 当前设备与已实现功能

- 当前测试设备的 MAC 为 `28:84:85:44:66:00`；本次连接时是 `COM6`，下次插拔后串口号可能变化。Flash 16 MB、PSRAM 8 MB，使用 ESP-IDF 5.5.4 和 `sdkconfig.stopwatch`。
- 车型选项包含 `JCW F56 8AT` 与 `GP3 F56 8AT`，车标由车型决定。用户已在实屏确认两种标识、颜色、GP 字形与方向，并确认车型滑动结束后约 2 秒再切换。
- G 表根据 StopWatch 的 BMI270 与当前安装方向校准，白点显示当前平面加速度，红色轨迹点分别在约 10 秒内淡回原来的灰色。用户已在实车确认 G 值方向和响应；G 表布局、INFO 间距及白环均经过实屏确认。
- 表情页使用代码实时绘制，接收与 G 表相同的 IMU 方向输入。触摸翻页和原页面震动反馈保留。声音设置默认关闭。
- 左侧黄键（GPIO2）切上一页，右侧蓝键（GPIO1）切下一页；G 表右键双击清除 MAX，右键单击等待 380 ms 后翻到下一页。按键有 30 ms 消抖，页面震动沿用触摸翻页的路径。**刷机和启动已验证；实体按键手感尚待用户确认。**
- 设备角色为 `STANDALONE` 时不启动 Wi-Fi／ESP-NOW。本次开机日志明确显示 `STANDALONE (BLE/OBD, no WiFi/ESP-NOW)`。

## OBDLink CX 当前边界

早先实车测试能建立 BLE 连接但没有确认有效 OBD 读数。之后代码修正了重复 GATTC 注册、OBDLink CX 的 FFF1 通知／FFF2 写入方向、通知订阅与配对时序、超时重连和相关日志；**修正后尚无新的实车有效数据证据**。电脑 USB 供电下只验证了 BLE 栈启动和 `OBD GATTC registered`，这不能代替车辆数据验证。

下次上车时先用手机串口终端以 `115200 8N1` 记录上电、连接、等待读数的完整日志。优先检查 `OBD UART handles`、`OBD notifications enabled`、`First ELM prompt received`、`First valid OBD response received`，以及是否有配对、CCCD 或 GATT 超时。相关实现位于 `main/bsp_obd_dsp/elm327_ble_client.c`。不要把“BLE 已连接”当成“OBD 已有数据”。

## 本次镜像与验证

| 文件 | 用途 | SHA-256 |
| --- | --- | --- |
| `firmware/stopwatch/stopwatch-buttons-20260930.bin` | 当前已刷入应用镜像，2,532,432 字节 | `6FBA42B66C7A95362999C831C93C4D8F7EBB379C729BF71929EA19C7ABD3931C` |
| `firmware/stopwatch/rollback-before-buttons-20260930.bin` | 按键更新前的设备同版应用镜像，2,531,200 字节 | `176515E41FFC8BEC487E529E788DB3B1055D5A9A7EE01EF7D5A22F00DA27B4C8` |

本次先从设备读取 `ota_0` 的整个 3 MB 应用分区，核对其中的镜像与更新前构建包逐字节一致；随后只向 `0x20000` 写入新应用镜像。`verify_flash` 返回 `digest matched`，分区检查剩余约 19%。复位后采集 35 秒串口日志：只见一次正常启动，CO5300 显示屏、CST820 触摸和 BMI270 均初始化成功，没有反复重启或 panic。实体按键尚未做人工操作验收。

本次构建复用了设备原有固件的依赖库，只重编译按键相关对象并重新链接应用镜像；**没有声称完成一次全新的 clean build**。镜像内的构建标识沿用链接时的 `8263d1d` 基线元数据；精确识别请使用上表 SHA-256。`firmware/release/` 和 `firmware/obd_brz_gauge.bin` 属于仓库原有通用发布包，本次没有替换它们。

## 下次构建与刷机

在本机项目目录执行：

```powershell
Set-Location 'C:\Users\cerar\Desktop\obd-stopwatch\work\obd_brz_gauge'
.\tools\prepare_stopwatch_deps.ps1
& 'C:\Users\cerar\esp\esp-idf-v5.5.4\export.ps1'
$env:STOPWATCH_DEPS_DIR = (Split-Path (Get-Location).Path -Parent)
idf.py -B ..\obd-build-next '-DSDKCONFIG=sdkconfig.stopwatch' build
```

`prepare_stopwatch_deps.ps1` 检查兄弟目录中的 M5GFX、M5PM1、M5IOE1 和 Bosch BMI270 API 固定版本，并应用 M5GFX 的 AMOLED DMA 空指针保护。新的 clean build 可能较慢；构建成功后先检查 DIO 启动头、应用分区大小和目标串口。只更新应用时使用 `0x20000`；不要重写 NVS、主题和 bootmedia。其他移植背景见 `docs/STOPWATCH_PORT.md`。

## 接下来先做

1. 在设备上确认黄键上一页、蓝键下一页；G 表蓝键单击翻页、双击清 MAX，且触摸翻页与震动保持正常。
2. 用 OBDLink CX 再做一次完整实车测试，保存手机串口日志，确认出现有效读数；如果没有，沿上面的 BLE／ELM 日志节点定位。
3. 若继续改动固件，先从 `port/m5stopwatch` 分支工作；不要把 StopWatch 应用镜像刷到其他硬件分支。
