# Minibox ESP32-S3 N16R8

默认运行从 Talking Alarm 迁移的完整应用：闲聊闹钟（豆包 ASR → Agent → TTS、
聊天记录、手动闲聊、整点播报、安静模式）、24 个网络电台、Wi-Fi 扫描与密码输入、
API 设置门户、SNTP 校时、俄罗斯方块和贪吃蛇。界面使用 **LVGL 8.4**，
输出到原有 **ST7796 480×320**；硬件接线、屏幕驱动、PCF8563、
INMP441/MAX98357、Flash/PSRAM 配置不变，不依赖参考目录。
参考版本随机闲聊次数为 0，迁移后同样默认关闭，保留调度逻辑。

## 默认应用操作

摇杆仅负责上下左右导航，开机约 640 ms 内保持居中；不使用摇杆按压。
导航映射为 Y 轴（GPIO2）控制屏幕左右、X 轴（GPIO1）控制屏幕上下；
接线及各物理轴的反转设置不变，硬件诊断仍显示原始 X/Y 轴。
GPIO4 短按确认，在闲聊中长按录音、松开发送；GPIO5 在闲聊中按住即可录音，
松开发送，最长 10 秒。GPIO6 返回/主页，GPIO7 辅助确认（闲聊动作卡可请求闲聊）。
音频使用 Core 1，网络和应用使用 Core 0。录音和播放严格半双工，
64 KiB 软件音频环形缓冲放在 PSRAM，保留内部 RAM 给 I2S DMA，
避免电台解码与 HTTPS 同时运行时出现 DMA 分配失败。
释放当前 I2S0 通道后才切换方向，共用 GPIO39/40；GPIO47 仅播放时拉高，
GPIO42 保留默认高阻。原始录放音回声测试仍可通过诊断模式使用。

## 本地文件配置分区

私密配置只放在 Git 忽略的 `config_data` 中，**不写入 C 头文件或默认配置**：

| 文件 | 格式 |
|---|---|
| `config_data\wifi_config.txt` | 第 1 行 SSID，第 2 行 Wi-Fi 密码，末尾换行 |
| `config_data\app_secrets.txt` | ASR、Agent、TTS API key 各一行，按此顺序，末尾换行 |

从全新检出构建时，先自行创建这些文件；留空也可构建，启动后在设备设置中配置
Wi-Fi，并进入 API 设置模式，连接 `Minibox-Setup`（接入密码 `minibox-setup`），
浏览 `http://192.168.4.1/`。此门户仅手动启用，完成后在设备上退出。
API URL、资源 ID、Bot ID 和音色在 `main\app_config.h` 中固定；
ASR WebSocket 的固定主机和路径在 `doubao_asr_client.c`。
HTTPS/WSS 使用证书包校验，不沿用参考工程的跳过证书验证设置。
没有迁移参考工程的具体位置资料。

配置使用独立 `storage` SPIFFS 分区（0xC10000，0x3F0000 字节），挂载到
`/spiffs`；原 `fonts` 分区及应用分区地址不变。构建自动生成
`build\storage.bin`；用户明确要求后执行 `idf.py -p COM8 flash` 才写入设备。
**完整 flash 会覆盖设备上已保存的 Wi-Fi/API 配置，恢复为本地配置文件的内容**。
仅更新应用、不覆盖配置时使用 `idf.py -p COM8 app-flash`（仍需明确授权烧录）。
配置和镜像包含明文凭据；不要提交或分享 `config_data` 私密文件、`storage.bin`
或包含配置的整包固件。普通 `idf.py build` 不会访问豆包服务或消费 API 配额。

## 可选原始硬件诊断

在 `idf.py menuconfig` → `Minibox` 启用
`Boot the original hardware diagnostics instead of the LVGL apps`。
以下测试说明仅适用于该模式；默认应用不会执行开机录放音或显示测试色条。

当前固件配套 2026-10-09 载板改版，测试按钮、摇杆、麦克风、扬声器、
PCF8563 RTC 和 ST7796 屏幕。开机显示 RGB/W/K/Y 色条、四键状态、
摇杆 ADC 原始值及百分比、RTC 时分秒和音频状态。旧载板不能直接使用本接线配置。
开机先播放 250 ms 的低幅度 440 Hz 提示音，再录制 1 秒环境声音并回放。
摇杆开机时应保持居中、不要触碰，约 640 ms 完成中心校准；失败会显示错误，
不阻止其他外设测试，松开摇杆后按开发板 RESET 重试。
**按住 GPIO5** 按钮立即开始录音（消抖约 30 ms），最多录制 3 秒；
录满会停止采集并等待松开。**松开 GPIO5** 后从 MAX98357 播放录音。
播放期间的按键不触发新录音，播放完成并松开后可再次测试。GPIO4 和
GPIO6、GPIO7 只显示按键状态，不触发录音。
串口会报告麦克风采样数、幅度和音频错误。默认不连接 Wi-Fi；
可启用下述动感101网络电台模式专门测试喇叭。

## 动感101网络电台测试

在 `idf.py menuconfig` → `Minibox` 的硬件诊断模式中启用
`Speaker test: play Donggan 101 instead of microphone recording`，
并在同一菜单本地设置 Wi-Fi 名称和密码。密码只保存在 Git 忽略的
`sdkconfig` 和构建产物中，不写入源码、默认配置或文档，不分享带凭据的固件。
该选项只影响硬件诊断，不影响默认应用的电台或语音功能。

该模式只播放动感101，固定流地址为
`https://lhttp.qtfm.cn/live/274/64k.mp3`，不切换其他电台。
连接 2.4 GHz Wi-Fi、取得 IP 后通过 `ntp.aliyun.com` 同步系统时间，
再用证书校验的 HTTPS 拉取 MP3、解码成 16-bit PCM 并输出到 MAX98357。
输出数字幅度为原始 PCM 的一半，功放 GAIN 仍默认高阻。
断网会尝试重连，音频流出错后等待 5 秒重试同一电台；
串口报告 HTTP 状态、MP3 格式和累计 I2S 输出量，屏幕显示连接/播放状态。

四键、摇杆、RTC 和屏幕仍继续测试，但开机提示音、环境录音及 GPIO5
录音回放均禁用，避免同时驱动共用的 I2S 时钟线。
关闭电台模式并重新构建烧录后恢复麦克风测试。
若 Wi-Fi 启动时发生欠压复位，检查 USB 线、外部电源和负载，
不能通过关闭欠压检测掩盖供电问题。

## 接线

| 模块 | 信号 | ESP32-S3 |
|---|---|---|
| 四个直焊机械轴 | 两个常开触点 | 分别接 GPIO4/5/6/7 与 GND，不接 VCC |
| INMP441 麦克风 | SD / WS / SCK / L/R | GPIO21 / GPIO39 / GPIO40 / GND |
| MAX98357 放大器 | DIN / LRC / BCLK | GPIO41 / GPIO39 / GPIO40 |
| MAX98357 放大器 | SD/MODE / GAIN | GPIO47 / GPIO42 |
| 摇杆 | X / Y | GPIO1 / GPIO2 |
| 摇杆按压（不使用） | K | 按压焊脚 NC，驱动不配置按压 GPIO；GPIO42 用于功放 GAIN |
| PCF8563 RTC（I2C 地址 0x51） | SDA / SCL | GPIO15 / GPIO16 |
| 未连接 GPIO | GPIO17 | 悬空 NC，不配置 |

INMP441 正面视角左列从上到下为 L/R、WS、SCK，右列为 GND、VCC、SD；
此处描述信号位置，不改变模块物理焊盘编号。

直焊机械轴按下接地，必须使用 GPIO 内部上拉及低有效配置。若改接高有效模块，
先在 `idf.py menuconfig` → `Minibox` 关闭
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

## 现场测试

- 四键：分别按下和松开，确认对应 KEY1–4 的 UP/DOWN 状态和串口 GPIO 事件。
- 摇杆：向四个方向推到端点，确认 X/Y 原始值及百分比变化，再松开回中。
  百分比使用启动中心和 ADC 0–4095 范围估算，不是实测端点标定；
  3.3 V 端可能超过 ADC 有效量程而饱和，不能据此测量精确电压。
- 音频：听开机提示音和环境声回放；正常运行后按住 KEY2（GPIO5）说话，
  松开后听回放。串口报告采样数、最小/最大值、非零数和平均绝对幅度；
  软件 I/O 成功不代表实际听到了声音，非零采样也不能单独证明麦克风正常。
- RTC：每秒读取完整日历，确认秒数持续变化后显示 `TICK OK`。
  读失败、无应答、无效日历或超过约 3 秒不走时均明确报告错误。
  只在停振或低电压标志置位时写入本机编译时间并重启计时，正常 RTC 时间不覆盖。
  编译时间有构建/烧录延迟，不是精确校时；掉电保持和走时精度需单独实测。
- 屏幕：确认横屏 480×320、文字、边框及色条正常，R/G/B 颜色不互换；
  初始化日志不能代替肉眼检查。

## 构建与观察

使用 ESP-IDF 5.3.5 PowerShell 环境：

```powershell
Set-Location C:\gitroot\minibox\esp32_idf_s3n16r8
idf.py build
```

明确准备覆盖板上固件时才执行 `idf.py -p COM8 flash`，之后可执行
`idf.py -p COM8 monitor` 查看串口（`Ctrl+]` 退出）。烧录会同时写入
SPIFFS 字体分区；屏幕缺字或初始化失败时核对字体镜像、GPIO9 背光和
供电。电台模式曾在 Wi-Fi 启动时触发欠压复位；默认应用会启动 Wi-Fi，
只有未启用电台测试的硬件诊断不启动 Wi-Fi。
如仍发生 brownout，请先检查 USB 线、供电和外设负载，而非关闭欠压保护。
构建或串口日志不能代替现场检查屏幕、麦克风和扬声器。
