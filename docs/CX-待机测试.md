# CX 联动待机

从状态页下滑进入 SETTINGS → CX STANDBY。LINKED STANDBY 启用、连接真实 CX 后，读取 STI、STSLCS、STSLLT 和 ATPPS，保存该 MAC 的原 PP0E/PP0F，再配置并回读。仅配置验证通过才启用自动关机。关闭联动会恢复保存的原参数并验证。

首次验证暂时失败时，同一连接最多尝试3次，失败后分别间隔5秒、15秒；间隔内继续普通OBD读取。静默/休眠或OTA时不重试。用尽后显示 `CX verify failed - reconnect`；成功显示 `Ready - CX sleep linked`。验证步骤、重试次数及 linked/armed 状态会记录日志；真实RPM/SPD最多每5秒独立记录，即使联动未armed也保留。详见 [验证重试更新](releases/2026-10-03-cx-verification-retry.md)。

省电设置为 ELM327 兼容模式、UART 无通信 300 秒。PP0F 同时设置 HS-CAN 无活动 150 秒，但 STN2310 v5.8.1 没有显示 PA 状态，150 秒触发效果未确认。实车已确认 UART 静默约 300 秒会通过 BLE 发出 LP ALERT；电压唤醒配置保留。

## 运行逻辑

持续收到真实数据时保持工作，包含原地怠速 800rpm/0km/h，以及自动启停时仍有新的 0/0 响应。行驶中丢链、纯 BLE 超时和短时数据中断继续自恢复。

停车静默有两种有效证据，最后 RPM/车速必须是间隔不超过 10 秒的真实配对读数，车速必须为 0：

- 最后 RPM 为真实 0；随后 ECU 连续 60 秒没有有效响应。
- 熄火漏采最终零 RPM，最后真实 RPM 在 1–1500 的怠速区间；CX 对 RPM 和车速各明确返回至少两次 NO DATA 或 UNABLE TO CONNECT；随后 ECU 连续 60 秒没有有效响应。

第二种情况保留真实非零 RPM，不伪造 0。任意有效 ECU 响应会取消无数据证据。未知车速、非零车速、超过 1500rpm、只缺一个 PID 或单纯 BLE 超时均不能使用第二种停车判定。首次连接始终没得到 RPM/车速且也没有其他新 ECU 响应，给 5 分钟启动等待后才暂停。

QUIET 时停止查询、初始化、扫描和自动重连，但保留现有 BLE 订阅接收异步通知。粉圈/缺失值不能单独证明 BLE 已断开；手机终端的 Connection lost 也可能只是 Watch 的 USB 日志线被拔掉。

**收到 ACT ALERT：**仅在已进入 QUIET 且联动配置已验证时，提示 `CX sleep soon - ACT ALERT`，屏幕亮度临时减为当前亮度的 50%。重复通知不累加降低；调整亮度仍按新设置的一半显示，不更改保存的设置。按键恢复连接、关闭联动或退出等待后恢复亮度。ACT ALERT 本身是可取消的通信空闲预告，不能确认车辆熄火或触发关机；继续保持 BLE 接收，不额外查询 CX。当前 PP0E=EA/PP0F=B5 配置及实车日志对应收到后约 60 秒进入 LP ALERT 阶段。

**真正收到 LP ALERT：立即停止通信并进入 PMIC L0，USB 或背面 5V 是否仍在不再阻止关机。** 不再等待外部供电延迟关闭。关机前再次验证背面边沿唤醒、清除旧唤醒标志与定时唤醒、关闭主控/显示/功放和 IMU/RTC 电源保持。唤醒配置无法验证或 OTA 进行中时不关机。

**收不到 LP ALERT 的本地兜底：**QUIET 后等待 330 秒，超过配置的 300 秒 UART 超时并留 30 秒余量；仅两路外部 5V 都连续消失 3 秒才关机。这不是收到真实休眠通知，保持保守判定。

USB 新上电、V1.0 背面 5V 新上电，或电源按键可冷启动并恢复绑定 CX。现有电源一直保持高电平不算新的插入事件；必须经历断电再上电才自动唤醒。关机后车辆在供电尚未消失时立即重新启动，可按电源键唤醒。等待页上的表盘按键/RECONNECT 可以取消静默。

## 已确认的实车证据

用户日志：23:15:38.163 进入 QUIET；23:19:38.269 收到 ACT ALERT；23:20:38.274 收到 LP ALERT，随后 23:20:38.313 CX BLE 断开。静默至 LP ALERT 约 300.110 秒。23:20:19.006 的 Connection lost 已由用户明确是拔掉 Watch 与手机的 USB 链接，不能当成 CX 断联。用户确认旧版本无外部供电时可以正常待机。

2026-10-03 修复依据：另一轮末次 RPM 仍为 657/车速 0，连续反复自愈和重连；现在允许满足双 PID 明确无数据的停车补充判定。后续00:59–01:24实车记录最后保留739rpm/0km/h，01:19:32进入QUIET，240.111秒后ACT，60秒后LP，再611ms执行关机；用户确认70%→35%变暗、USB仍插着时黑屏、G表不卡。本次新增的首轮验证重试待下一次车辆日志核对；再次上电唤醒仍需单独实测。

## 下一次测试

1. 连接并确认 Ready - CX sleep linked；正常怠速有数据时保持运行。
2. 熄火后继续保留 USB 供电和日志，等待 QUIET；约 4 分钟收到 ACT ALERT，显示即将休眠、亮度减半；约 1 分钟后收到 LP ALERT，应直接黑屏断开 CDC，不再等 USB 断电。
3. USB 保持供电时应持续关闭；拔线再插线后应冷启动并恢复 CX 连接。
4. 同样测试背面稳压 5V 的持续供电关机、断电再上电唤醒与电源按键唤醒。
5. 正常行车偶发断联仍应重连；G 表外圈切换蓝/粉/绿等颜色时，白点和切页应保持响应。

日志关注：[CX] stationary ECU absent / ECU / state=QUIET / asynchronous ACT ALERT / asynchronous LP ALERT / SLEEP_FALLBACK，以及 [POWER] ACT ALERT dim / shutdown gate / entering PMIC L0 / cold boot wake source。关机后 USB 日志停止属于正常现象。

## V1.0 背面无线供电

仅适用于用户确认的 V1.0：无线接收板稳压 5V 正极接背面 14 脚（该版误标 BAT），负极接 11 脚 GND。V1.0.1 的 14 脚是真电池端，不能照接 5V。

readVin() 检测 USB 分支；背面 5V 由 Q2 转成 PMG4/PORT_INT 的低有效信号。PMG4 设为输入、无上下拉、下降沿唤醒并回读验证。背面上电为 REAR_5V=ON，断开为 OFF。USB 唤醒标志 0x02、背面 0x20、电源按键 0x04，可同时出现。真实 LP ALERT 路径可以在 USB 供电记录日志时测试；本地超时兜底仍受两路供电限制。

硬件依据：[M5Stack StopWatch 电源 API](https://docs.m5stack.com/en/arduino/stopwatch/m5pm1_m5ioe1)、[M5PM1 数据手册](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_CN.pdf)。VIN 插入唤醒与 GPIO 边沿唤醒保留，PMIC 关机与充电独立。软件编译不替代持续供电下真实关机/唤醒测试。

## 2026-10-01 配置回读修复

实车 STN2310 v5.8.1 的 STSLCS 包含 `VL WAKE: OFF, >13.20V FOR 1 s`。初版配置查询把其中的 `>` 当成命令提示符，回读在电压行中断，验证失败后回滚 PP0E/PP0F，自动关机未启用。新解析器只接受独立一行（允许前导空格）的 `>`，并保持 BLE 分包间的行状态。回读超长、没有完整提示符或参数不匹配仍视为失败。

完整手机回读还确认第二处误判：STN2310 v5.8.1 在 ELM327 模式也没有 PA SLEEP/PA WAKE 行。新验证要求 ELM327 模式、UART SLEEP ON/300 s、UART WAKE ON、EXT SLEEP OFF，并独立确认 ATPPS 中的 PP0E/PP0F 值与启用状态，以验证休眠与 LP ALERT 配置。若固件提供 PA SLEEP 行，仍要求它为 ON/0x01/150 s。READ CONFIG 也检查 PP，只有 Watch 已启用联动且验证通过才显示 Ready。

新日志记录具体被拒绝的命令、失败时的回复，以及写入后的 PP0E/PP0F 值和启用状态。回归测试使用手机抓到的完整原生和 ELM327 配置，覆盖每个分包边界、行内电压比较、最终提示符、缓存溢出、缺失 PA 字段及错误/禁用 PP 配置。

该台适配器原始值已由手机确认：`0E:5A F`、`0F:FF F`。手机已按顺序单独发送 `ATPP 0E SV EA`、`ATPP 0F SV B5`、`ATPP 0E ON`、`ATPP 0F ON`、`ATZ`。01:47:43 ATPPS 确认 `0E:EA N`、`0F:B5 N`；01:47:50 STSLCS 确认 ELM327 模式、UART SLEEP ON/300 s、UART WAKE ON。参数写入成功，当时 LP ALERT 实际发送仍待确认；后续 23:20:38 实车通知及用户关机观察已确认，见下面的记录。发送换行使用 CR（0x0D），每次一条命令，等最终 `>` 后再发下一条。

手机写入完成不代表初版 Watch 已修复。初版仍可能因回读错误再次回滚；需要更新 Watch 后再确认 `LINKED STANDBY: ON`、`Ready - CX sleep linked` 与完整参数回读。手机配置和 CAN 监听实验时保持 Watch 关闭。
