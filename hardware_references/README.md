# Reference archive

本目录按用户要求保留集成工程的原始资料及关键图纸，方便换机。
PDF 保留原版权声明；TXT 和 PNG 为查阅用提取，不替代原 PDF。
`manifest.json` 记录 SHA-256、大小及 PDF 页数，以便核对迁移完整性。

| 归档文件 | 来源/用途 |
|---|---|
| esp32-s3-wroom.pdf | [Espressif](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)，N16R8 模块 |
| max98357.pdf | [Analog Devices](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf)，裸功放 |
| inmp441.pdf | [TDK 产品](https://invensense.tdk.com/products/digital/inmp441/)，底部声孔/九焊盘；归档 PDF 为此前获取副本 |
| ap63203.pdf | [Diodes](https://www.diodes.com/assets/Datasheets/AP63200-AP63201-AP63203.pdf)，3.3V 降压 |
| ds3231.pdf | [Analog Devices](https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf)，SN 内置晶体版本 |
| ch340.pdf | [WCH](https://www.wch.cn/downloads/CH340DS1_PDF.html)，C 型内置时钟及 3.3V 接法 |
| thb001p.pdf | C&K THB001P，[LCSC C2685355](https://www.lcsc.com/product-detail/C2685355.html)，裸摇杆针位/机械图 |
| cpg151101d13.pdf | HanElectricity CPG151101D13，[LCSC C49234235](https://www.lcsc.com/product-detail/C49234235.html)，MX 开关 |
| myoung-bs-12-b3aa003.pdf | MYOUNG，[LCSC C964723](https://www.lcsc.com/product-detail/C964723.html)，CR1220 座极性和尺寸 |
| sn74lv1t34.pdf | [TI](https://www.ti.com/lit/ds/symlink/sn74lv1t34.pdf)，RGB 电平转换 |
| tps25947.pdf | [TI](https://www.ti.com/lit/ds/symlink/tps25947.pdf)，已撤销的 eFuse 研究，非当前 BOM |
| tlv7034.pdf | [TI](https://www.ti.com/lit/ds/symlink/tlv7034.pdf)，已撤销的分立 CC 检测研究 |
| tlv431.pdf | [TI](https://www.ti.com/lit/ds/symlink/tlv431.pdf)，已撤销的 CC 参考电路研究 |
| sn74lvc2g02.pdf | [TI](https://www.ti.com/lit/ds/symlink/sn74lvc2g02.pdf)，已撤销的 CC 窗口逻辑研究 |
| sn74lvc1g11.pdf | [TI](https://www.ti.com/lit/ds/symlink/sn74lvc1g11.pdf)，逻辑研究参考 |
| sn74lvc1g02.pdf / sn74lvc1g08.pdf / sn74lvc1g27.pdf / sn74lvc1g32.pdf / sn74lvc14a.pdf | TI 逻辑研究，未采用或已撤销，非当前 BOM |
| panasonic-zk.pdf | Panasonic 储能电容研究，已撤销，非当前 BOM |
| tusb320.pdf / tusb320lai.pdf | [TI](https://www.ti.com/lit/ds/symlink/tusb320lai.pdf)，未采用的 Type-C 控制器研究；内部 Rd 不可与额外 5.1k 并联 |
| pcf8563.pdf / rkjxv.pdf | 早期 RTC/摇杆研究，**不是当前 BOM** |

USB 实选 HRO TYPE-C-31-M-12，[LCSC C165948](https://www.lcsc.com/product-detail/C165948.html)。
供应商 PDF 返回访问限制，未将错误页伪装成 PDF，也没有归档该型号 STEP。
方向判断使用 KiCad 实际封装坐标；生产前仍须工厂核对实物图纸。

标准符号/封装来自 KiCad 10 库，版权归上游作者，遵循上游
CC-BY-SA 及电路设计例外（见库许可证）。项目自建 INMP441、THB001P、
CR1220 封装由对应器件图纸推导；标准库复制和修改不改变
上游许可。本地缓存不等于完整 3D 模型归档。
