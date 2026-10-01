# StopWatch 开机动画

在状态页下滑进入六宫格，选择 **MORE → MULTI-GAUGE → INTRO**：

- `VIDEO`：播放独立资源分区内已安装的动画。
- `OFF`：关闭视频动画。
- `RACE`：保留上游 RACE / AS / ONE 多表同步动画。

选项保存在 NVS，下次启动生效。当前只有一个视频槽位；Watch 本身不编辑动画或导入 MP4 / GIF。开机先显示静态标志约 1 秒，然后播放动画并进入默认页面。

## 更新内容

动画放在 `bootmedia` 分区，偏移 `0xA20000`，大小 `0x5E0000`。前 4KiB 是清单，后面是 RGB565 数据。更换内容可以通过现有手机配套 OTA 的动画上传功能，或 USB 单独刷资源分区；无须每次重刷主程序。

2026-10-02 增加 `delta_runs_rgb565_black_v3` 播放支持，使用连续同色像素压缩保持原生分辨率。安装这类资源前需先升级一次支持 v3 的主程序。新版播放器同时兼容 v1 / v2 动画。

视频先转换成项目格式，再按需压缩并打包：

```sh
python tools/make_boot_block.py animation.mp4 --canvas 466 --grid 466 --fps 30 --output bootmedia/pixels
python tools/compact_boot_block.py bootmedia/pixels/boot_block.txt bootmedia/pixels/boot_block.bin bootmedia/runs
python tools/pack_bootmedia_raw.py bootmedia/runs/boot_block.txt bootmedia/runs/boot_block.bin bootmedia/bootmedia.raw.bin --partition-size 0x5E0000
```

单独刷资源的命令如下。实际串口与分区布局应按设备核对；刷入前备份旧资源，主程序需支持包内格式。

```sh
python -m esptool --chip esp32s3 --port COM6 write_flash 0xA20000 bootmedia/bootmedia.raw.bin
python -m esptool --chip esp32s3 --port COM6 verify_flash 0xA20000 bootmedia/bootmedia.raw.bin
```

## v3 格式

每帧开始为小端 `uint32_t` 区间数，每个区间依次包含：

1. varint 标记：`length << 2 | explicit_delta << 1 | black`。
2. 若 `explicit_delta=1`，跟随 varint 起点增量；首段为绝对起点，其他段相对上一段的末像素。省略时紧接上一段，首段从 0 开始。
3. 若 `black=0`，跟随小端 `uint16_t` RGB565 颜色；否则颜色为 0。

区间写入可以包含本帧颜色相同的未改变像素，减少量化边界上的碎片。它不改变解码结果。长度为零、越界、回退区间、截断颜色和过长 varint 会被拒绝。原生网格直接填充画布；旧的小网格扩展路径仍可用。

## JCW 动画来源与验证

参考原片：[NBT BMW MINI John Cooper Works Startup Animation](https://www.youtube.com/watch?v=LdM9xwQ3OXY)，上传者 Sergio Bezruk。原片为 1280×480、25fps。我们按长边缩放到 466px，重建完整圆形背景、稳定徽标、修平中心暗斑，输出 190 帧、30fps、约 6.33 秒的无声动画。

最终资源保留完整 466×466 网格，RGB565 数据约 5.23MB。真实 C 播放器与 LVGL 的全部 190 帧输出均与批准母版量化后的像素逐字节一致；旧格式兼容、原生 / 缩放网格及七种错误流检查通过。

2026-10-02 已刷入 StopWatch V1.0。设备启动日志确认 190/190 帧播放完成、流数据 5,232,645/5,232,645 字节全部消费，播放时间 6,279ms；35 秒检查无崩溃或异常重启。原有 INTRO 从 RACE 切为 VIDEO，其他 NVS 设置逐字节保持原值。屏幕观感仍以实机验收为准。

项目内的回归检查可在已有 LVGL 预览缓存的 Linux / WSL 环境运行：

```sh
python3 tools/test_boot_runs.py /path/to/bootmedia_package
```
