# 载板改版元件资料

核对日期：2026-10-09。用于 `C:\gitroot\minibox\kicad` 的布局改版，
不是生产批准文件。原 `hardware\_references` 已合并到本目录，
重复文件逐一核对 SHA-256 后去重，原有资料及索引保留。
PDF 版权及使用条件归原作者；本目录只保存设计参考副本。

| 元件 | 数量 | 已核对的信息 | 来源 |
|---|---|---|---|
| HanElectricity CPG151101D13 | 4 | 立创 C49234235，直插机械键盘轴；孔位及定位孔须按 PDF 核对 | [立创商品页](https://item.szlcsc.com/51650377.html) |
| YTL YV13S-L7.85-B10Ka(60)-0-DL01 | 1 | 立创 C37323747，插件-14P、21 × 17 mm，10 kΩ、自复位 | [立创商品页](https://item.szlcsc.com/39051754.html) |
| 5.1 kΩ R0402 电阻 | 2 | CC1/CC2 独立 Rd；UNI-ROYAL 0402WGF5101TCE / C25905，±1%；保留原选型 | [用户最新指定商品页](https://item.szlcsc.com/26648.html) |
| CC1/CC2 接线端子 | 2 个接线位 | 拟用一个双位端子；5.08 mm 节距沿用现有端子系列，实际型号待定 | 用户要求；不能据现有 J7/J8 推断实购端子完全相同 |

## 本地文件

- `datasheets/cpg151101d13.pdf`：原归档副本，8 页；
  已核对当前商品页仍指向该型号的手册。
  当前公开下载地址：
  <https://atta.szlcsc.com/upload/public/pdf/source/20250618/D1C12809F8EDF275373534C56A4C291D.pdf>。
- `datasheets/yv13s-l7.85-b10ka-60-0-dl01.pdf`：从精确型号商品页下载，8 页。
  <https://atta.szlcsc.com/upload/public/pdf/source/20240806/1B8487ADF5BD9B353C3D03081821B46D.pdf>。
- `drawings/cpg151101d13-6.png`、`drawings/cpg151101d13-8.png`：
  原归档的机械图查阅副本，以 PDF 为准。
- `drawings/requested-layout.png`：用户提供的清晰布局图。

| PDF | 字节数 | SHA-256 |
|---|---|---|
| cpg151101d13.pdf | 3197551 | e49ad1b9f11d9e9d84fdec678f7172b31c23b63f83691d27e11b6a447deb1824 |
| yv13s-l7.85-b10ka-60-0-dl01.pdf | 638156 | 954709ccaf0202879e64b1c17a8ba384c11486283fe826515f4fd022c0393874 |

## 电气及装配注意

摇杆电位器手册给出的额定值为 AC 50 V 或 DC 5 V，按键接点额定值为
DC 12 V / 50 mA；不要把按键接点的 12 V 误用为模拟轴供电。
本项目两轴电位器端点接 3.3 V/GND，滑动端分别接 GPIO1/2，
该型号具有按压按键：第 1 页 S1/S2 常开开关，第 6 页开关机械性能。
用户决定**不使用按压功能**，GPIO42 已释放，不作按压预留；
按压焊脚保持无网络，不需要猜测手册未标明的物理触点分组。
PCB 中 SWA/SWB/SWC/SWD 按设计均无网络，固定脚 M1–M4 亦不擅自接地。
14 个物理引脚不等于 14 个独立信号。

## CC 下拉电阻采购标识

R1/R2：**UNI-ROYAL 0402WGF5101TCE / C25905**，
5.1 kΩ、±1%、英制0402/公制1005。
[LCSC 型号页](https://www.lcsc.com/product-detail/C25905.html)与官方检索结果
匹配这些参数；这是型号确认，不是国内嘉立创装配库存或价格承诺。
生产前仍核对国内选料、回流规格及备料。每条 CC 独立一个 Rd，不并联已有 Rd。

原厂手册记录焊锡性 235±5 °C、3±0.5 秒，焊锡耐热性 260±5 °C、
5 秒；这些是测试条件，不是整件可经过 SMT 回流炉的证明。
按键轴及摇杆拟从正面插装、背面焊接，电阻放正面贴装。

CC1 和 CC2 各自通过一只 5.1 kΩ 接 GND，不能短接两条 CC，
也不能用一只电阻共用。仅适用于外接 Type-C 母座把两条 CC 独立引出的情况。
若外接母座已有 Rd，不得重复并联；若只有 VBUS/GND 两线，
此板上的电阻无法替代缺失的 CC 连接。
Rd 用于声明受电端，不是 PD 控制器，也不保证获得 3 A。
本目录不记录实时价格或库存承诺。

## 全焊装配采购候选

以下为选料记录，不是工厂生产批准。用户已选择 WJ500V 两位/三位端子，
PCB 和审核 BOM 已适配；不表示已经采购或获准生产。
普通商城有商品、装配库有库存、封装兼容、工厂承接是四个独立核对项。
库存与报价随时变化，下单时重新查询。

| 位号 | 数量 | 精确型号 | 立创编号 / 国内装配目录 |
|---|---:|---|---|
| J1/J2 | 2 | LAIL-PM2.54-22P-L | [C54973843](https://www.jlc-smt.com/lcsc/detail/C54973843.html) |
| J3A/J3B | 2 | LAIL-PM2.54-3P-L | [C54973828](https://www.jlc-smt.com/lcsc/detail/C54973828.html) |
| J4 | 1 | LAIL-PM2.54-7P-L | [C54973832](https://www.jlc-smt.com/lcsc/detail/C54973832.html) |
| J5 | 1 | LAIL-PM2.54-6P-L | [C54973826](https://www.jlc-smt.com/lcsc/detail/C54973826.html) |
| J6 | 1 | LAIL-PM2.54-14P-L | [C54973850](https://www.jlc-smt.com/lcsc/detail/C54973850.html) |
| J7 | 1 | WJ500V-5.08-03P-14-00A | [C72334](https://www.jlc-smt.com/lcsc/detail/C72334.html) |
| J8/J9 | 2 | WJ500V-5.08-2P | [C8465](https://www.jlc-smt.com/lcsc/detail/C8465.html) |

五种排母均在国内装配目录显示为扩展库。22P 用户指定件的机械图已读取：
本体长 56.38±0.30 mm、宽 2.50±0.20 mm、高 8.50±0.20 mm，
针脚长 3.00±0.20 mm、截面 0.60×0.25 mm，节距 2.54 mm，推荐孔 Ø1.02 mm。
现板排母孔 Ø1.00 mm，须结合成品孔公差、针脚公差、全排定位和模块插入高度核对。
另外四种针数已逐份读取精确商品页的机械图，节距、宽、高、针脚和推荐孔一致。
3P/6P/7P/14P 本体长度分别为 8.12/15.74/18.28/36.06 mm，图示公差均 ±0.30 mm。
这些核对仍不代替全排针脚实物配合和工厂装配批准。
J3 必须是两个独立 1×3，不是普通 2×3。

WJ500V 两位和三位精确商品页指向同一厂家系列图：
严格 5.08 mm 节距；针脚截面 0.90×0.80 mm；厂家推荐孔 Ø1.50 mm，
本体深 10.00 mm、高 14.07 mm，长度 N×5.08 mm，另有拼接凸耳。
原板端子孔 Ø1.30 mm，且 F.Fab 使用 Phoenix 通用外形，不能直接替换。
用户现已明确采用 WJ500V，J7/J8/J9 七孔改为 Ø1.50 mm，保留 Ø2.60 mm 焊盘；
本地封装包含真实 10 mm 本体深度、拼接凸耳以及 0.50 mm 公差/0.25 mm 装配余量。
J8/J9 一起上移 1 mm，进线方向与网名不变；相关电源/CC 入口局部重接，
其余走线/过孔保持。实际 DRC、机械门槛、四层 CAM 与钻孔对齐已重新核对。

本地新增参考副本：

- `datasheets/lail-pm2.54-22p-l.pdf`：
  <https://atta.szlcsc.com/upload/public/pdf/source/20260526/5D0113ABF8CBF84CECFA584A6B591EE8.pdf>。
- `datasheets/lail-pm2.54-3p-l.pdf`：
  <https://atta.szlcsc.com/upload/public/pdf/source/20260526/F3D3938F29D8A6EAE0E0D2BECC10646B.pdf>。
- `datasheets/lail-pm2.54-6p-l.pdf`：
  <https://atta.szlcsc.com/upload/public/pdf/source/20260526/6E3B6C74074951B2DAB1C0B345B54AFF.pdf>。
- `datasheets/lail-pm2.54-7p-l.pdf`：
  <https://atta.szlcsc.com/upload/public/pdf/source/20260526/D00015F7937B9DDCB4D54B862B49BB0D.pdf>。
- `datasheets/lail-pm2.54-14p-l.pdf`：
  <https://atta.szlcsc.com/upload/public/pdf/source/20260526/E301E5DB0DC2B32F8C4DF97E16C2D142.pdf>。
- `datasheets/wj500v-5.08-series-candidate.pdf`：
  <https://atta.szlcsc.com/upload/public/pdf/source/20260331/4C76CA2ACF4350D93F8DAE8387658BA0.pdf>。

型号搜索可能返回 5.00 mm 的 KF301/KF128/DG301/WJ301V 或无关电阻。
不得仅凭搜索词“5.08”认定实际节距，也不得用“邮寄专用”条目满足商城供料要求。

### 未采用的 Phoenix 两位备选（适配前核对记录）

用户进一步提供的 [MKDS 1,5/ 2-5,08 商品页](https://item.szlcsc.com/5848879.html)
对应 Phoenix Contact **1715721 / C5183929**。
[国内装配目录](https://www.jlc-smt.com/lcsc/detail/C5183929.html)可查到精确型号。
厂家图纸第 2–3 页与实际 J8/J9 核对结果：节距 5.08 mm、推荐孔 Ø1.30 mm、
本体深 9.80 mm、进线侧到引脚中心 4.60 mm、端部到首脚中心 2.54 mm，
均匹配适配前的 Phoenix 两位封装。高度 13.80 mm、焊脚长 3.50 mm。
曾作为不改板的直接匹配候选；用户后续选择 WJ500V，不把这个备选放入采购 BOM。

这是固定式 PCB 螺钉端子，厂家不是把它定义为需要另配插头的插拔端子；
不能仅按商城“插拔式”分类采购额外配件。它只有两位，不能替代三位 J7。
此兼容性记录不代表用户选择了这个备选、已采购或工厂已批准插件焊接。
本地手册 `datasheets/phoenix-mkds-1.5-2-5.08.pdf`：
<https://atta.szlcsc.com/upload/public/pdf/source/20220930/5ACAA47A5DCD35493AC7A52B30D3CF94.pdf>。
