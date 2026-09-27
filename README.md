# Minibox

| 目录 | 内容 |
|---|---|
| [`models`](models/module-dimensions.md) | 3D 外壳模型、STL、3MF、尺寸说明及模型检查脚本 |
| [`docs`](docs/hardware-connections.md) | 主控、屏幕和摇杆的模块参数、GPIO 接线及供电注意事项 |
| [`esp32_idf_s3n16r8`](esp32_idf_s3n16r8/README.md) | ESP32-S3 N16R8 的麦克风、按钮、屏幕和扬声器测试固件 |

固件项目参考 `C:\gitroot\talking-alarm` 的 ESP-IDF 结构，独立构建，
不依赖该目录；当前默认测试 GPIO40/41/42 按钮，以 GPIO41 控制录放音。

模型文档中的导出和检查命令均在 `models` 目录执行：

```powershell
Set-Location C:\gitroot\minibox\models
python -m unittest discover -s .\tests -p test_mesh_checks.py
```
