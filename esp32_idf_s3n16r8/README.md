# ESP32-S3 N16R8 microphone test

当前固件配套 2026-10-09 载板改版，开机初始化 ST7796 屏幕，
显示 GPIO4/5/6/7 四个按钮的状态。旧载板不能直接使用本接线配置。
**按住 GPIO5** 按钮立即开始录音（消抖约 30 ms），最多录制 3 秒；
录满会停止采集并等待松开。**松开 GPIO5** 后从 MAX98357 播放录音。
播放期间的按键不触发新录音，播放完成并松开后可再次测试。GPIO4 和
GPIO6、GPIO7 只显示按键状态，不触发录音；不需要摇杆。
串口会报告麦克风采样数、幅度和音频错误。先前的电台模块代码仍在工程，
但当前启动不连接 Wi-Fi，也不播放电台。

## 接线

| 模块 | 信号 | ESP32-S3 |
|---|---|---|
| 四个直焊机械轴 | 两个常开触点 | 分别接 GPIO4/5/6/7 与 GND，不接 VCC |
| INMP441 麦克风 | SD / WS / SCK / L/R | GPIO21 / GPIO39 / GPIO40 / GND |
| MAX98357 放大器 | DIN / LRC / BCLK | GPIO41 / GPIO39 / GPIO40 |
| MAX98357 放大器 | SD/MODE / GAIN | GPIO47 / GPIO42 |
| 摇杆（当前不初始化） | X / Y | GPIO1 / GPIO2 |
| 摇杆按压（不使用） | K | 按压焊脚 NC，驱动不配置按压 GPIO；GPIO42 用于功放 GAIN |
| RTC（当前未实现驱动） | SDA / SCL | GPIO15 / GPIO16 |
| 未连接 GPIO | GPIO17 | 悬空 NC，不配置 |

INMP441 正面视角左列从上到下为 L/R、WS、SCK，右列为 GND、VCC、SD；
此处描述信号位置，不改变模块物理焊盘编号。

直焊机械轴按下接地，必须使用 GPIO 内部上拉及低有效配置。若改接高有效模块，
先在 `idf.py menuconfig` → `Minibox hardware test` 关闭
`GPIO4/5/6/7 buttons pressed at low level`，再构建烧录。不要让按钮
OUT 超过 3.3 V；四路按钮共地。录音和回放共用 GPIO40/39 时钟线，
固件先释放麦克风 I2S 通道再启用放大器。GPIO47 在回放时使能放大器；
GAIN=GPIO42 默认高阻；菜单中的 12 dB 选项将 GPIO42 输出为低电平。
扬声器接 SPK+/SPK-，模块按额定电压供电。
屏幕 GPIO9–14、完整模块参数和注意事项见
[`docs/hardware-connections.md`](../docs/hardware-connections.md)。
GPIO40/41 没有 ADC，不能用于摇杆 X/Y；摇杆配置默认值已改为
GPIO1/2，按压功能关闭。旧本地 `sdkconfig` 必须同步轴配置。编译检查会拒绝
摇杆与键盘或音频引脚冲突的配置。现有实物接线必须与新分配同步，
不能只更新固件而保持原接线。

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
