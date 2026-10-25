# Minibox

| 目录 | 内容 |
|---|---|
| [`models`](models/module-dimensions.md) | 载板两件式外壳、错位连通键盘开孔、打印文件、尺寸与避让说明 |
| [`model-v2`](model-v2/README.md) | 低矮改版外壳：左侧喇叭、后壁偏左三角布局（开关＋双Type-C）；[截面与视角数据报告](model-v2/fit-review.html) |
| [`assets`](assets) | 开发板[尺寸与天线图](assets/esp32s3_devkit_01.jpg)、[5.08 mm 接线端子](assets/crimp_terminal.jpg)、麦克风、功放、RTC、[TFT SPI](assets/tft_spi.jpg)、[摇杆](assets/joystick.jpg)与[三键键盘](assets/keypad.jpg)参考图片（TFT 型号与当前固件配置未核对） |
| [`docs`](docs/hardware-connections.md) | 主控、屏幕和摇杆的模块参数、GPIO 接线及供电注意事项 |
| [`kicad`](kicad/README.md) | KiCad 10 双层模块载板：[正面图例](kicad/renders/minibox-carrier-front-legend.png)、[背面图例](kicad/renders/minibox-carrier-back-legend.png)、GPIO 丝印与 Gerber；实物核对后方可制板 |
| [`esp32_idf_s3n16r8`](esp32_idf_s3n16r8/README.md) | ESP32-S3 N16R8 的 LVGL 闲聊闹钟、电台、设置、游戏和可选硬件自检固件 |

固件项目参考 `C:\gitroot\talking-alarm` 的 ESP-IDF 结构，独立构建，
不依赖该目录；默认启动 LVGL 应用，保留 GPIO4/5/6/7 四键及 GPIO1/2 摇杆。
GPIO4 确认，闲聊中长按录音；GPIO5 在闲聊中按住录音、松开发送，
GPIO6 返回，GPIO7 辅助确认。原录放音自检通过菜单切换。
新增 [NES 模拟器](esp32_idf_s3n16r8/README.md#nes-游戏)，从本地 SPIFFS
加载用户自备的 `.nes` 游戏；游戏内 A/B 为动作键、X/Y 为 Select/Start，
同时长按 X+Y 退出，复用当前摇杆、ST7796 屏幕与 MAX98357 音频。
NES 画面为 **256×240 原生像素居中显示，不缩放**；目标 60 次/秒模拟、
最高 30 fps 显示，实际流畅度仍需实板确认。
支持 mapper 0、1、2、3、4、7、66、112、178、245；游戏文件用英文名称，
已配置的标题在设备上中英文双行显示。按用户要求，当前 10 个指定 ROM
随仓库提供以便异地复现；私密配置、存档和分区镜像仍不纳入 Git。
菜单四方向、NES 八方向；NES 直向容错更宽，斜向要求两轴各至少 40%、
副轴/主轴至少 85%，边界隔离区不动作。
[摇杆规则与前后区域图](esp32_idf_s3n16r8/README.md#nes-摇杆活跃区域旧版与新版)
及 [ROM 列表](esp32_idf_s3n16r8/README.md#rom-来源记录) 见固件说明。
实现取舍与验收边界见 [NES 详细计划](docs/nes-implementation-plan.md)。
当前接线为 RTC SDA=GPIO15、SCL=GPIO16，INMP441 SD=GPIO21，
MAX98357 GAIN=GPIO42、SD/MODE=GPIO47；GPIO17 悬空 NC，
摇杆按压焊脚保持 NC，GPIO42 不用于摇杆按压。

16 MiB Flash 已分配：启动/系统区域 64 KiB、应用 2 MiB、
字体文件系统 10 MiB、游戏与配置文件系统 3.9375 MiB。
分区内空闲空间不会自动借给其他分区；8 MiB PSRAM 是易失运行内存。
完整布局与更新注意事项见 [显示与存储说明](docs/display-fonts.md#文件系统分区)。
仅更新固件使用 `idf.py -p COM8 app-flash`；完整 `flash` 或 `storage-flash`
会覆盖设备 storage 中的配置和存档，须先备份。

模型文档中的导出和检查命令均在 `models` 目录执行：

```powershell
Set-Location C:\gitroot\minibox\models
python -m unittest discover -s .\tests -p test_mesh_checks.py
```
