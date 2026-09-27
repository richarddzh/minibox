# ESP32-S3 N16R8 microphone test

当前固件开机初始化 ST7796 屏幕，显示 GPIO40/41/42 三个按钮的状态。
**按住 GPIO41** 按钮立即开始录音（消抖约 30 ms），最多录制 3 秒；
录满会停止采集并等待松开。**松开 GPIO41** 后从 MAX98357 播放录音。
播放期间的按键不触发新录音，播放完成并松开后可再次测试。GPIO40 和
GPIO42 只显示按键状态，不触发录音；不需要摇杆。
串口会报告麦克风采样数、幅度和音频错误。先前的电台模块代码仍在工程，
但当前启动不连接 Wi-Fi，也不播放电台。

## 接线

| 模块 | 信号 | ESP32-S3 |
|---|---|---|
| 三个按钮模块 | OUT | 分别接 GPIO40、GPIO41、GPIO42 |
| 三个按钮模块 | VCC / GND | 3.3 V / 共地 |
| INMP441 麦克风 | SD / WS / SCK / L/R | GPIO18 / GPIO17 / GPIO16 / GND |
| MAX98357 放大器 | DIN / LRC / BCLK | GPIO15 / GPIO17 / GPIO16 |
| MAX98357 放大器 | SD/MODE / GAIN | GPIO7 / GPIO8 |

按钮模块默认按下时 OUT 为低电平，使用 GPIO 内部上拉。若模块按下输出高电平，
先在 `idf.py menuconfig` → `Minibox hardware test` 关闭
`GPIO40/41/42 buttons pressed at low level`，再构建烧录。不要让按钮
OUT 超过 3.3 V；三路按钮共地。录音和回放共用 GPIO16/17 时钟线，
固件先释放麦克风 I2S 通道再启用放大器。GPIO7 在回放时使能放大器；
GAIN=GPIO8 默认高阻。扬声器接 SPK+/SPK-，模块按额定电压供电。
屏幕 GPIO9–14、完整模块参数和注意事项见
[`docs/hardware-connections.md`](../docs/hardware-connections.md)。

## 构建与观察

使用 ESP-IDF 5.3.5 PowerShell 环境：

```powershell
Set-Location C:\gitroot\minibox\esp32_idf_s3n16r8
idf.py build
```

明确准备覆盖板上固件时才执行 `idf.py -p COM8 flash`，之后可执行
`idf.py -p COM8 monitor` 查看串口（`Ctrl+]` 退出）。烧录会同时写入
SPIFFS 字体分区；屏幕缺字或初始化失败时核对字体镜像、GPIO9 背光和
供电。电台模式曾在 Wi-Fi 启动时触发欠压复位，本测试不启动 Wi-Fi；
如仍发生 brownout，请先检查 USB 线、供电和外设负载，而非关闭欠压保护。
构建或串口日志不能代替现场检查屏幕、麦克风和扬声器。
