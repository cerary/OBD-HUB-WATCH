# OBD HUB WATCH 改进与验证清单

硬件标准：**M5Stack StopWatch C152 V1.0**。此表区分本项目新增 / 修复与保留的上游能力。已刷镜像以固件目录的 SHA-256 为准；当前已刷镜像包含充电指示灯逻辑，固件中的 Git 字符串记录构建时的状态。

| 改进 | 具体行为 / 修复 | 主要实现 |
| --- | --- | --- |
| 硬件移植 | CO5300 / CST820、M5PM1 / M5IOE1、BMI270、反馈、双按键；PSRAM 画布和 DMA 内存安排 | `main/stopwatch/stopwatch_board.cpp`、`main/app_main.c`、`tools/prepare_stopwatch_deps.ps1` |
| CX BLE 连接 | FFF0 服务、FFF1 通知、FFF2 写入；订阅与配对、重试、初始化与故障日志 | `main/bsp_obd_dsp/elm327_ble_client.c` |
| CX 配置联动 | 保存每个适配器原始 PP0E / PP0F；写入后回读；失败恢复原值且不启用自动关机；关闭联动恢复已保存参数 | `elm327_ble_client.c`、`nvs_storage.c` |
| 完整配置回复 | 仅独立行 `>` 完成响应；允许 `VL WAKE: >13.20V` 行内比较；支持 BLE 任意分包；兼容没有 PA 状态行的 STN2310 5.8.1 | `cx_power_policy.c`、`tools/test_cx_power.c` |
| 停车与偶发断联区别 | 保留最后真实 RPM / SPD；真实 0/0 且后续无响应才可进入 QUIET；800/0 怠速不误停；持续新零数据也不立即停机 | `cx_power_policy.c` |
| 不反复唤醒 CX | QUIET / SLEEPING 时停止命令与自动重连；处理 `LP ALERT`；无提示时使用经过配置验证的静默超时兜底 | `cx_power_policy.c`、`elm327_ble_client.c` |
| 两路外部供电 | USB VIN 或 V1.0 PMG4 背面 5V 检测；确认 CX 休眠后供电消失 3 秒进入 PMIC L0；验证唤醒寄存器，失败不关机 | `stopwatch_board.cpp` |
| 充电指示灯 | PMIC GPIO2 CHRG 低电平且 USB / 背面 5V 存在才亮；充满 / 电池供电时灭；500ms 全页面更新，仅修改 LED 位并回读；保留充电电流与电源轨配置 | `stopwatch_board.cpp`、`main/app_main.c` |
| 充电灯稳定确认（已刷机） | CHRG 持续有效 5 秒才点亮；短脉冲不累计；充电结束、外部供电移除、状态未知立即清除；采样断档重新确认。USB 实测 5243ms 后亮、CHRG 释放后 8ms 熄灭，五次 1～3.5 秒变化未点灯；用户确认按键 / 切页后实灯保持熄灭。电池电压仅作诊断 | `stopwatch/charge_led_policy.h`、`stopwatch_board.cpp`、`tools/test_charge_led.c` |
| 断联清空 | 清空所有通道、RPM 覆盖、平滑状态与时间戳；显示缺失不能冒充 ECU 的真实零读数 | `obd_data_cache.c`、`elm327_ble_client.c` |
| 数据时效 | RPM / SPD 5 秒、其他通道 15 秒；每项独立判断；真实零有效；过期车速不统计里程 | `obd_data_cache.c`、`ui.c`、`ui_disp_item.c` |
| 切页首帧 | 初始缺失显示 `--`；页面加载前同步有效缓存并唤醒原刷新计时器；真实 0/N 保留；过期或断联不带入旧数值；从缺失恢复时直接显示首个真实值，随后保留平滑更新 | `ui_data_entry.c`、`ui.c`、`ui_disp_item.c`、七个数据页面 |
| AFR 持久化 | index 11 在 Needle / Chart 重启后保留；默认 AFR 报警 OFF；兼容旧 11 项 blob，保留自定义合法阈值 | `nvs_storage.c` |
| 转速报警恢复 | 结束闪烁恢复页面原背景；仅修改当前报警页面；删除 / 切页安全；断联 / 过期 / CX 静默终止旧值报警 | `ui_ext.c` |
| 外圈 | 456px 外径 / 10px 宽 / 边缘 5px；整圈按当前页面渐变，蓝色连接、粉色局部失效、休眠 / 全缺失变暗；不闪烁 | `ui_status_ring.c`、`status_ring_policy.c`、`ui_helpers.c` |
| 温度两级提醒（已刷机） | CLT 115/120°C、OIL 125/135°C 黄色/红色；IAT 60/80°C 黄色/橙色且仅在所在页面提示。新采样确认持续时间、3/5°C 恢复温差、无效/过期/断联清除；原生 ALARM 滑条调整高级门槛与 OFF。主机政策、实际 LVGL、NVS 迁移及启动检查通过，实车触发待测试 | `temperature_alert_policy.c`、`ui_status_ring.c`、`ui_disp_item.c`、`ui_ScreenPageChartAlarm.c`、[温度说明](TEMPERATURE-ALERTS.md) |
| 档位估算换算修复 | 移除总传动比中的重复终传因子；JCW 215/40 R18 PS5 名义半径 0.3146 m。使用平滑前车速；容差重叠选范围内最近中心；失配保留不超过 1 秒后显示 `--`，断联/过期/车型切换重置。独立圆周样例及实际 UI 回归，实车换挡对照待测试 | `obd_data_cache.c`、`vehicle_profiles.c`、`ui_data_entry.c`、`ui.c`、`test_gear.c` |
| 指示弧统一 | RPM / SPEED / GEAR 416px 外径、20px 宽，与外圈相隔 10px；RPM 90 字号内收避免五位值越界 | `screens/ui_ScreenPageRpm.c`、`ui_ScreenPageSpeed.c`、`ui_ScreenPageGear.c` |
| 指针表布局 | 426px 刻度盘、清除默认内边距、距状态圈内沿约 5px；24 字号浅灰数字；主刻度 3×14px、小刻度 2×9px，34px 标签间距避免字与刻度相碰；红针距主刻度内端约 5px | `screens/ui_ScreenPageNeedle.c` |
| 峰值标记 | RPM / SPEED 10px，5 秒渐隐；更高值刷新；低于峰值不重置时间；过期、断联、扫表、休眠清除 | `ui_peak_marker.c`、`peak_marker_policy.c` |
| G 表 | 301 点扩展点阵、175px 行程 / 1.5G 满量程；方向 MAX 约 15px 高；10px 折线端；裁剪接头；轨迹 10 秒渐隐 | `ui_ScreenPageGForce.c` |
| 页面生命周期 | G / 状态弧形文字缓存复用；子对象删除释放画布；隐藏计时器暂停；重复创建不累积内存 | `ui_ScreenPageGForce.c`、`ui_ScreenPageEasterEgg.c` |
| 六宫格设置 | OBD CONNECT / CX STANDBY / RING COLORS / FEEDBACK / BASIC / MORE；白图标；保留原滚轮和滑条；统一父级返回 | `ui_ScreenPageSettingsMenu.c`、`ui_ScreenPageSettings.c`、`ui.c` |
| 循环导航 | 状态上一页为末尾表情页；档位上一页为状态；触摸和实体键路由一致；可选主题页仍可插入 | `ui.c` |
| 项目身份 | OBD HUB WATCH；来源和 T A O 两行英文；兼容旧 SKYGAUGE 扫描名称，短无线标识保持旧报文容量 | `project_identity.h`、`ui.c`、`espnow_link.c` |
| 状态电源文字 | EXT 5V / BAT PWR 与 BAT 百分比沿底部两侧大圆弧排列，缺失电源数据单独处理 | `ui_ScreenPageEasterEgg.c` |
| 车型与标志 | JCW F56 8AT / GP3 F56 8AT、车型决定徽标；滚轮松手后延迟保存以方便继续选择 | `vehicle_profiles.c`、`ui.c`、`ui_helpers.c` |

## 保留的上游能力

车型配置框架、通用 OBD PID / 部分厂商请求、ZN/C6 专用 CAN 监听、ESP-NOW 多表、主题系统、RaceChrono、OTA 和 Android App 工程来自上游。保留代码并不代表全部经过本板实车验收；MINI 的当前主要数据链路仍是 OBD 轮询。

## 当前验证

- 最新已刷固件：ESP-IDF 5.5.4 构建通过，应用大小 2,593,856 字节，小于 3MB 应用分区；独立 Flash 校验与 35 秒启动检查通过；系统、NVS 与 OTA 选择区域写入前后逐字节一致，JCW 动画资源校验通过。
- 充电指示灯：启动时关闭灯位、随后 USB 充电信号触发开启，两次寄存器回读均通过；监测未报错。实灯和充满 / 拔线 / 背面供电变化待人工确认。
- 宿主回归：真实策略和缓存 / NVS C 代码，含断联清空、不计过期里程、AFR 配置与旧报警迁移。
- 实际 LVGL：圆环几何、峰值 / 边界像素、G 接头不越界、40 条按键 / 手势路由、状态来源行、原生控件与释放检查。
- 切页首帧：实际 LVGL 与生产缓存代码验证初始缺失、真实零值 / 空档、已有有效值、同值重新进入、过期 / 断联、计时器唤醒、缺失恢复及开机扫表；通过完整编译、刷写与启动检查，用户确认重启后首次切到转速 / 车速 / 档位直接显示 `--`，没有跳变。
- 指针页：正式 LVGL 渲染与页面回归通过；按用户反馈将红针缩短 9px，主刻度对齐图确认中间有 5 排空白像素；用户确认修正版红针长度与间距合适、显示正常。
- README：完整页面代码渲染，输入是示例数据；附 source SHA-256。没有以概念图代替当前界面。

**待实车确认：**真实 LP ALERT 发送及完整关机链、无线接收模块的背面检测 / 断电 / 重新上电唤醒，以及最新 UI 与按键的人工验收。充电指示灯已刷机并通过启动回读，需确认 USB 拔线 / 充满、背面 5V 充电和关机时的实灯变化。首次绑定默认、自动旋转、全车型数据和全部多表 / RaceChrono / 手机 App 兼容不属于本次已完成验证。
