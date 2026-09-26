# 仓库说明

本文从 `talking-alarm` 的仓库说明调整而来，适用于 Minibox。

## 通用

- 保持修改聚焦，优先选择简单、明确的方案，不顺带重构无关代码。
- 文件和目录使用英文无空格名称；C 源码和头文件使用小写下划线命名。
- 不自动生成 prompt 档案、开发日志或大量测试脚本；用户要求时再写。
- 用户明确要求保持构建简单，不要添加自定义构建框架或过多 Python 测试。
- 不默认创建虚拟环境；ESP-IDF 使用已安装的 Python 环境。

## 目录

- `esp32_idf_s3n16r8/`：独立 ESP-IDF 固件项目。
- `models/`：3D 模型、STL、3MF、机械尺寸说明及模型检查脚本。
- `docs/`：模块参数、接线及其他项目文档。
- `.github/skills/`：构建和开发日志技能。
- 不依赖 `C:\gitroot\talking-alarm` 运行或构建；该仓库只作参考。

## 固件与硬件

- 使用 ESP-IDF 5.3.5、C 和 FreeRTOS，不引入 Arduino 草图结构。
- 主控为 ESP32-S3 N16R8：16 MB Quad Flash（DIO、80 MHz），
  8 MB Octal PSRAM（40 MHz）；目标固定为 `esp32s3`。
- 屏幕为 ST7796 SPI、横屏 480 × 320，使用乐鑫官方
  `espressif/esp_lcd_st7796` 组件，不套用参考项目的 ST7789 初始化；
  不引入 Arduino 或 TFT_eSPI。
- 屏幕接线：LED=GPIO9、SCK=GPIO10、SDI=GPIO11、DC/RS=GPIO12、
  Reset=GPIO13、CS=GPIO14。
- 摇杆 X=GPIO4、Y=GPIO5，为 ADC 模拟信号；默认 X 反转、Y 不反转。
  K=GPIO6，按最新实物反馈默认高有效，可配置极性；不要继续套用最初的低有效假设。
- GPIO48 控制板载 WS2812 RGB LED；启动时发送黑色数据熄灯。
  不要只拉低引脚，也不要误关 GPIO9 的屏幕背光。
- 逻辑电平及摇杆供电为 3.3 V，所有模块共地；屏幕 VCC 和背光驱动须遵守模块规格。
- GPIO19/20 保留给原生 USB，避免使用被 Octal PSRAM 占用的 GPIO35/36/37。
- 接线和参数的权威文档为 `docs/hardware-connections.md`，硬件变化时同步更新。

## 构建、烧录与验证

- 在已经初始化的 ESP-IDF 5.3.5 PowerShell 中进入 `esp32_idf_s3n16r8`，
  直接执行 `idf.py build`。
- 用户明确要求烧录时执行 `idf.py -p COM8 flash`；当前实板端口为 COM8，
  换机、端口消失或存在歧义时先识别设备。
- 需要日志时使用 `idf.py -p COM8 monitor`，退出后释放串口。
- 优先增量构建，不随意执行 `fullclean`、删除构建缓存或重复 `set-target`。
- 使用最小必要验证。已有模型检查用标准库 unittest，不新增 Python 测试框架。
- 构建成功、烧录成功、初始化成功与肉眼确认屏幕/按键正常是不同结果，不能混为一谈。
- 校准时应释放摇杆；校准失败需检查日志，不掩盖错误或跳过有效性检查。

## 代码风格

- 沿用现有 C 命名和 `esp_err_t` 错误处理，配置常量集中管理。
- 模块职责清晰，不引入仅用于转发的多层封装。
- 错误通过返回码、ESP-IDF 日志或屏幕明确报告，不静默失败。
- 源码默认 ASCII，注释简短，只解释不明显的逻辑；中文文档保留 UTF-8。
- `sdkconfig.defaults` 保持 ASCII，避免 Windows 配置工具编码问题。

## 文档与版本控制

- 文档保持简洁，包含实际可用的命令和明确的硬件前提。
- 路径或使用方式变化时更新链接；机械模型的命令在 `models` 下执行。
- STL/3MF 及 `font_data/` 下带许可证的字体是需要保留的交付文件，
  不因其是导出产物而删除；普通构建不重新生成字体。
- 不提交 `build/`、生成的 `sdkconfig`、虚拟环境、缓存、临时串口日志或凭据。
- Git commit、push 和烧录需要用户明确请求；不强推，不覆盖他人的改动。
