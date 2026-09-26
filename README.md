# Minibox

| 目录 | 内容 |
|---|---|
| [`models`](models/module-dimensions.md) | 3D 外壳模型、STL、3MF、尺寸说明及模型检查脚本 |
| [`docs`](docs/hardware-connections.md) | 主控、屏幕和摇杆的模块参数、GPIO 接线及供电注意事项 |
| [`esp32_idf_s3n16r8`](esp32_idf_s3n16r8/README.md) | ESP32-S3 N16R8 的 ST7796 屏幕和模拟摇杆测试固件 |

固件项目参考 `C:\gitroot\talking-alarm` 的 ESP-IDF 结构，独立构建，
不依赖该目录，不包含闹钟、网络或音频功能。

模型文档中的导出和检查命令均在 `models` 目录执行：

```powershell
Set-Location C:\gitroot\minibox\models
python -m unittest discover -s .\tests -p test_mesh_checks.py
```
