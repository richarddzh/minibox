# ESP32-S3 N16R8 hardware test

参考 `C:\gitroot\talking-alarm\esp32_idf_s3n16r8` 的原生 ESP-IDF 结构。
使用 ESP-IDF **5.3.5**，16 MB Quad SPI Flash（DIO、80 MHz），
8 MB Octal PSRAM（40 MHz）。不依赖参考仓库或第三方显示库。
启动时通过 GPIO48 的 RMT 时序将板载 WS2812 RGB LED 熄灭，
不影响 GPIO9 控制的屏幕背光。

## 接线

完整模块参数、逐引脚接线及供电说明统一维护在
[`docs/hardware-connections.md`](../docs/hardware-connections.md)。

屏幕固定为 ST7796 SPI、480 × 320：LED=9、SCK=10、SDI=11、
DC=12、Reset=13、CS=14。摇杆使用已确认的 X=4、Y=5、K=6，
K 按下为低电平，摇杆供电 **3.3 V**，所有模块共地。

摇杆引脚可在 `idf.py menuconfig` → `Minibox hardware test` 修改。
当前限定为 GPIO1–8，三个引脚必须不同；GPIO3 是启动配置引脚，建议沿用 4/5/6。

同一菜单提供 X/Y 反转、SPI 时钟、BGR 色序、屏幕反色及背光极性设置。
屏幕固定接线集中在 `main\app_config.h`。

## 构建与烧录

在 **ESP-IDF 5.3.5 PowerShell** 环境执行：

```powershell
Set-Location C:\gitroot\minibox\esp32_idf_s3n16r8
idf.py build
# 当前实板为 COM8；烧录会覆盖现有固件。
idf.py -p COM8 flash monitor
```

退出串口监视器：`Ctrl+]`。默认 UART0 控制台，115200 波特率；
使用开发板的 USB-UART 接口。若只有 GPIO19/20 原生 USB 接口，
在 `menuconfig` → `Component config` → `ESP System Settings`
将主控制台改为 USB Serial/JTAG Controller 后重新构建。

`sdkconfig.defaults` 仅为首次配置的默认值，修改已有工程请使用
`menuconfig`。顶层 CMake 固定目标为 `esp32s3`。
默认单应用分区足够本测试，不提供 OTA；16 MB Flash 无需全部分配。

## 显示与摇杆测试

1. 上电显示 `MINIBOX ST7796 480X320`、四边白框以及 R/G/B/W/K/Y 色条。
   红、绿、蓝、白、黑、黄应与标签对应，四边完整可见。
2. 显示 `CALIBRATING` 时松开摇杆和按钮；等待 1.5 秒后采样约 640 ms。
   中心值接近端点或校准波动过大会显示 `JOYSTICK ERROR`，串口输出原因。
   检查接线、松开摇杆并复位重试；不会用虚假的默认中心继续测试。
3. 左侧十字区域显示实时位置；右侧显示 12-bit ADC（0–4095）、归一化百分比、
   中心值、X/Y 方向、K 状态和累计按下次数。允许同时显示两个方向。
   未反转时 ADC 较小对应 LEFT/UP，较大对应 RIGHT/DOWN。
4. 按下 K，光标变黄，`K PRESSED`，计数加一；持续按住不会重复计数。
   只推动 X/Y 不会增加按键计数。需要改变安装方向时在配置菜单反转相应轴。
5. 串口周期性打印 ADC、百分比、方向、按键和计数，便于与屏幕交叉检查。

ADC 每轴取 8 次均值，独立任务每 10 ms 采样；方向进入阈值 25%，
释放阈值 15%；按键稳定 30 ms 后更新。显示在另一任务中同步提交，
SPI 传输不会阻塞输入采样；状态队列只保留最新快照，但按键累计计数不会丢失。
冷启动已经按住 K 时显示按下状态，但不将它算作一次新按下。

整帧约 300 KiB，存放在 PSRAM；SPI 使用内部 DMA 缓冲分批传输。
色条和标题只画一次，运行时提交中间的测试区域，不先清空物理屏幕。
这不是双缓冲/VSync 驱动，高速移动时可能有撕裂。

空白屏优先检查供电、共地、背光控制、Reset、DC 和 CS；花屏可降低 SPI 时钟。
红蓝互换可切换 BGR；黑白或整体颜色反常可切换 display inversion。
屏幕是只写连接，固件不能读取控制器 ID，也不能自动证明面板确实显示正确；
断开的模拟输入可能漂浮，校准检查不能代替接线确认。

## 文件与逻辑检查

- `main\st7796.c`：ST7796 初始化、地址窗口、RGB565 SPI DMA 传输。
- `main\joystick.c`：ADC、中心校准、输入采样。
- `main\joystick_logic.c`：归一化、方向迟滞、按键去抖。
- `main\test_screen.c`：测试图形与英文状态文字。
- `main\app_main.c`：启动、输入任务、状态队列及错误显示。

在 Visual Studio x64 Native Tools 命令环境中可独立运行无硬件 C 测试：

```powershell
New-Item -ItemType Directory -Force build | Out-Null
cl /nologo /W4 /WX /I main tests\test_joystick_logic.c main\joystick_logic.c /Fobuild\ /Febuild\test_joystick_logic.exe
.\build\test_joystick_logic.exe
```

测试遍历有效中心对应的全部 ADC 数值，并检查反转、方向阈值/迟滞、
快速反向、按键抖动、长按、释放、重复按下及启动时已按住的行为。
构建和软件测试不能代替实板显示及接线测试。
