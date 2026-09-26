# ESP32-S3 N16R8 hardware test

参考 `C:\gitroot\talking-alarm\esp32_idf_s3n16r8` 的原生 ESP-IDF 结构。
使用 ESP-IDF **5.3.5**，16 MB Quad SPI Flash（DIO、80 MHz），
8 MB Octal PSRAM（40 MHz）。屏幕驱动使用乐鑫官方
[`espressif/esp_lcd_st7796`](https://components.espressif.com/components/espressif/esp_lcd_st7796)
1.4.0 组件，经 ESP-IDF Component Manager 获取；不依赖 Arduino 或 TFT_eSPI。
首次构建需要访问 Espressif 组件仓库，之后由 `dependencies.lock` 锁定版本。
启动时通过 GPIO48 的 RMT 时序将板载 WS2812 RGB LED 熄灭，
不影响 GPIO9 控制的屏幕背光。

## 接线

完整模块参数、逐引脚接线及供电说明统一维护在
[`docs/hardware-connections.md`](../docs/hardware-connections.md)。

屏幕固定为 ST7796 SPI、480 × 320：LED=9、SCK=10、SDI=11、
DC=12、Reset=13、CS=14。摇杆使用已确认的 X=4、Y=5、K=6，
摇杆供电 **3.3 V**，所有模块共地。按最新实物反馈，默认反转 X，
Y 不反转，K 改为高电平判定按下；电平极性可配置，GPIO 上拉保持不变。
INMP441 麦克风 SD=18、WS=17、SCK=16、L/R 接地（左声道）；
MAX98357 放大器 DIN=15、LRC=17、BCLK=16、SD/MODE=7、GAIN=8。
两模块共用 16/17 时钟线，
固件按半双工方式切换通道，供电和扬声器接法见接线文档。
显示通过官方面板接口交换 XY 并关闭 X/Y 镜像，BGR 开启时为 MADCTL=0x28，
保持 480×320 分辨率；SPI 默认为 40 MHz，必要时可在菜单中降频。
扫描方向以实物画面为准，不能用“旋转 180°”代替镜像排查。

摇杆引脚可在 `idf.py menuconfig` → `Minibox hardware test` 修改。
当前限定为 GPIO1–6，三个引脚必须不同；GPIO7/8 留给放大器控制，
GPIO3 是启动配置引脚，建议沿用 4/5/6。

同一菜单提供 X/Y 反转、K 有效电平、MAX98357 默认 9 dB / 可选 12 dB 增益、
SPI 时钟、BGR 色序、屏幕反色及背光极性设置。
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
使用自定义分区表：2 MiB 应用和 10 MiB SPIFFS 字体分区，不提供 OTA。
`idf.py flash` 自动烧录应用、分区表及字体镜像。
普通构建无需运行字体转换工具；首次字体烧录时间会较长。

## 显示与摇杆测试

1. 上电显示 `Minibox 屏幕测试 480x320`、四边白框以及 R/G/B/W/K/Y 色条。
   红、绿、蓝、白、黑、黄应与标签对应，四边完整可见。
2. 显示 `CALIBRATING` 时松开摇杆和按钮；等待 1.5 秒后采样约 640 ms。
   中心值接近端点或校准波动过大会显示 `JOYSTICK ERROR`，串口输出原因。
   检查接线、松开摇杆并复位重试；不会用虚假的默认中心继续测试。
3. 左侧十字区域显示位置；右侧显示归一化 X/Y（-1.00～1.00，两位小数）、
   方向、K 状态和累计按下次数。原始 ADC 和中心值只留在串口，不在屏幕跳动。
   显示用中心迟滞（偏移达到 0.05 才离开中心，回到 0.02 内归零），
   忽略相对上次显示值不超过 0.01 的变化。
   允许同时显示两个方向。
   当前默认 X 数值增大对应左，Y 数值增大对应下。
4. 按下 K，光标变黄，`K 按下 PRESSED`，计数加一；持续按住不会重复计数。
   只推动 X/Y 不会增加按键计数。需要改变安装方向时在配置菜单反转相应轴。
5. 串口周期性打印 ADC、百分比、方向、按键和计数，便于与屏幕交叉检查。

ADC 每轴取 8 次均值，独立任务每 10 ms 采样；方向进入阈值 25%，
释放阈值 15%；按键稳定 30 ms 后更新。显示在另一任务中同步提交，
SPI 传输不会阻塞输入采样；状态队列只保留最新快照，但按键累计计数不会丢失。
冷启动已经按住 K 时显示按下状态，但不将它算作一次新按下。

## 麦克风与扬声器测试

在屏幕和摇杆校准完成后，持续按住 K **1 秒**开始录音；短按不会触发。
录音过程中保持按住，松开后从 MAX98357 播放刚录到的声音。
录音最多 3 秒，满时自动停止采集，松开 K 后才播放。
播放中忽略新的按键；播放结束、K 松开后可再次长按进行下一次测试。
需隔离播放链路时，先把摇杆推向右（方向显示“右”），保持向右长按 K
1 秒，松开后播放半秒 500 Hz 测试音；该模式不采集麦克风。
正常录音请保持摇杆居中。测试音正常但语音异常时重点检查麦克风，
测试音也异常时重点检查放大器、时钟线和扬声器。
屏幕显示等待、录音、播放等状态，串口打印采样数和音频错误。
串口还输出麦克风样本最小/最大值、非零样本数和平均绝对幅度；
若数据非零且回放无报错，但扬声器无声，先确认 MAX98357 的
VIN/GND、SD/MODE 使能电压和 SPK+/SPK-，不能仅凭 I2S 日志判定输出正常。
GPIO7 仅在回放时拉高 SD/MODE，其余时间拉低关闭放大器；GPIO8 默认为
高阻输入（约 9 dB），菜单可选低电平（约 12 dB）。接线后不要再将
SD/MODE 同时固定接到电源或地，GAIN 也不要同时接外部电阻分压网络。
使用 16 kHz 单声道 16-bit PCM，3 秒录音在 PSRAM 占用 96000 字节（约 94 KiB）；
麦克风接收使用 32-bit 左声道 I2S，播放时复制到左右两个 16-bit 声道。
每次切换先关闭并释放当前 I2S 通道，再创建另一个通道，绝不同时驱动
共用的 GPIO16/17。仅构建或查看串口日志无法证明实际声学效果，
须烧录后用扬声器和麦克风现场测试。

当前帧和上次已提交帧各约 300 KiB，存放在 PSRAM；官方面板 API 使用内部 DMA
缓冲分批传输。色条和标题只画一次；运行时比较 16×16 像素块，
只提交变化的小矩形，不再整行重刷。相同帧不传输，不先清空物理屏幕。
X/Y、方向和 K 状态/计数均未变化时，跳过绘制；按 K 不会被 X/Y 稳定状态阻挡。
数字更新不超过 4 Hz，光标/按键显示最高约 10 Hz，输入仍独立以 100 Hz 采样。
两位小数是界面输出精度，不是将芯片 ADC 配置为不支持的 2-bit 模式。
这不是面板 VSync 驱动，高速移动时仍可能有撕裂。
文字使用 SPIFFS 中的 24 px 抗锯齿中英文字体，启动时顺序加载到 PSRAM，
渲染时不再读取 Flash，也不再放大 3×5 字形。
字体、分区及静态刷新诊断详见
[`显示字体说明`](../docs/display-fonts.md)。

空白屏优先检查供电、共地、背光控制、Reset、DC 和 CS；花屏可降低 SPI 时钟。
红蓝互换可切换 BGR；黑白或整体颜色反常可切换 display inversion。
背光 GPIO9 在初始化后保持恒定电平，不使用 PWM 或周期性开关。
与数字变化同步的闪烁应先检查刷新，并可用静态显示诊断进行对照。
如果停止刷新后仍整屏忽明忽暗，再检查供电、共地及 LED 是否是裸背光供电端；
不要靠提高 GPIO 驱动电流掩盖背光过载。
屏幕是只写连接，固件不能读取控制器 ID，也不能自动证明面板确实显示正确；
断开的模拟输入可能漂浮，校准检查不能代替接线确认。

## 文件与逻辑检查

- `main\st7796.c`：乐鑫官方 ST7796 面板组件的 SPI 接线、方向、背光及同步 DMA 适配。
- `main\joystick.c`：ADC、中心校准、输入采样。
- `main\joystick_logic.c`：归一化、方向迟滞、按键去抖。
- `main\audio_test.c`：I2S 录音/回放、PSRAM 缓冲及半双工通道切换。
- `main\audio_test_logic.c`：K 长按触发、3 秒截断及松开回放状态机。
- `main\test_screen.c`：测试图形与中英文状态文字。
- `main\bitmap_font.c`：SPIFFS 字体加载、PSRAM 字形索引和抗锯齿渲染。
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
