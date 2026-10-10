# Minibox

| 目录 | 内容 |
|---|---|
| [`models`](models/enclosure-v2.md) | 四件式 3D 外壳模型、打印文件、尺寸说明及模型检查脚本 |
| [`assets`](assets) | 开发板[尺寸与天线图](assets/esp32s3_devkit_01.jpg)、[5.08 mm 接线端子](assets/crimp_terminal.jpg)、麦克风、功放、RTC、[TFT SPI](assets/tft_spi.jpg)、[摇杆](assets/joystick.jpg)与[三键键盘](assets/keypad.jpg)参考图片（TFT 型号与当前固件配置未核对） |
| [`docs`](docs/hardware-connections.md) | 主控、屏幕和摇杆的模块参数、GPIO 接线及供电注意事项 |
| [`kicad`](kicad/README.md) | KiCad 10 双层模块载板：[正面图例](kicad/renders/minibox-carrier-front-legend.png)、[背面图例](kicad/renders/minibox-carrier-back-legend.png)、GPIO 丝印与 Gerber；实物核对后方可制板 |
| [`esp32_idf_s3n16r8`](esp32_idf_s3n16r8/README.md) | ESP32-S3 N16R8 的麦克风、按钮、屏幕和扬声器测试固件 |

固件项目参考 `C:\gitroot\talking-alarm` 的 ESP-IDF 结构，独立构建，
不依赖该目录；当前默认测试 GPIO4/5/6/7 按钮，以 GPIO5 控制录放音。
当前接线为 RTC SDA=GPIO15、SCL=GPIO16，INMP441 SD=GPIO21，
MAX98357 GAIN=GPIO42、SD/MODE=GPIO47；GPIO17 悬空 NC，
摇杆按压焊脚保持 NC，GPIO42 不用于摇杆按压。

模型文档中的导出和检查命令均在 `models` 目录执行：

```powershell
Set-Location C:\gitroot\minibox\models
python -m unittest discover -s .\tests -p test_mesh_checks.py
```
