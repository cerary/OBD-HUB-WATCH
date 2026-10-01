# StopWatch 设置目录

状态页下滑进入 SETTINGS，替代原来的设置表单。主仪表横向切页顺序不变，状态页上滑不再启动 BLE 扫描。

一屏六个入口：OBD CONNECT、CX STANDBY、RING COLORS、FEEDBACK、BASIC、MORE。

- OBD CONNECT 显示保存的设备和连接状态。点击 SCAN & SELECT 才进入扫描；PROTOCOL 使用原协议滚动选择框和长按确认。
- CX STANDBY 复用休眠联动详情页。打开目录或详情不会读写 CX；READ CX CONFIG、RECONNECT 和联动开关仍需明确点击。
- RING COLORS 保留五个滑条及松手保存，使用与原亮度滑条相同的主题样式。
- FEEDBACK 保留原振动、声音 ON/OFF 控件。
- BASIC 保留原开机页、车型、主题滚动选择框与亮度滑条。原有车型/主题变更重启逻辑保留。
- MORE 集中 G 校准、多表联动、固件更新和原 RaceChrono 开关。

BACK、右滑、硬件切页键统一返回上一级。滚动选择框、拖动滑条不触发页面跳转。BLE 扫描退出时停止，重新进入时重新扫描。G 校准从 MORE 进入返回 MORE；原 G 表/表情页校准快捷入口仍返回 G 表。

OTA 入口统一为状态页下滑 → SETTINGS → MORE → FIRMWARE / OTA，状态页不再显示重复的 OTA 按钮。保留原更新机制：进入时暂停 BLE，退出时重启以恢复 BLE。

转速表几何：466px 屏幕，白圈直径 456px、宽 10px，转速灰/橙圈直径 416px、宽 20px。同心圈设计间距为 `(456-416)/2-10 = 10px`。抗锯齿边缘可能含半透明像素。

修改仅调整界面与设置导航；CX 休眠策略、外部 5V 检测、OBD 数据读取和既有 NVS 配置结构不变。

电脑验证：ESP-IDF 编译；实际 LVGL 页面、六宫格文字边界、原生控件、保存、手势/按键返回、扫描退出/重入与校准来源路由模拟。设备显示、触摸和车辆休眠仍需刷机后实测。
