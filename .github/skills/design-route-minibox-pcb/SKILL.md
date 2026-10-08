---
name: design-route-minibox-pcb
description: 'Design, route, review, and prepare Minibox KiCad PCBs for domestic JLC manufacturing, including four-layer grounding, I2S, USB, power routing, and front-side assembly.'
argument-hint: 'Specify the KiCad project and whether to inspect, place, route, or prepare manufacturing files.'
user-invocable: true
disable-model-invocation: false
---

# Minibox PCB 设计与布线

用于 Minibox PCB 设计、布局、布线、包地检查、嘉立创 DFM 和生产资料准备。
只完成用户授权的范围；检查任务不擅自改板，不自动下单、付款、commit 或 push。

## 首先读取

- [仓库说明](../../instructions.md)。
- [权威硬件接线](../../../docs/hardware-connections.md)。
- [嘉立创工艺规格与项目规范](../../../docs/jlc-pcb-design-spec.md)。
- 目标工程 README、原理图、PCB、项目规则及本地符号/封装库。

`kicad` 是原载板，`kicad_ex` 是独立集成方案；不得改错工程或互相覆盖。
工厂参数摘要有核对日期；生产准备前重读官方能力页并确认订单选项。
将工厂极限、工厂建议、项目推荐和实际已设置值分开，不凭记忆填写数字。

## 工作流程

1. **建立基线。** 查看 Git 状态，保留已有修改；确认实际 PCB、原理图、
   网络、已布线路径和层叠，不只相信生成脚本。将临时备份和报告放会话
   artifacts，避免生成器覆盖已有布线。执行需要的基线 ERC/DRC，
   记录违规、未连接和原理图一致性分别的数量。
2. **明确制造条件。** 选择国内站经济型/标准型、板厚、铜厚、颜色、
   表面处理、通孔/特殊工艺和装配面。经济型不强行要求标准型工艺边/Mark；
   标准型按工厂模板准备。精确 MPN、封装、库存和回流条件未确认不能发布。
3. **设置规则。** 根据工艺文档配置全局约束和网络类：常规信号
   0.20 mm 线宽/间距，普通 0.60/0.30 mm 过孔，铜到锣边至少 0.50 mm。
   单独落实插件孔焊环/孔间距、NPTH、丝印和阻焊规则；特殊封装例外
   逐项核查，不降低全板标准掩盖局部问题。电源和 USB 不沿用普通线宽。
4. **先布局后布线。** `kicad_ex` 元件全部正面；检查真实 courtyard、
   天线禁铜、声孔、键帽、电池、连接器插拔、螺钉/工具空间和外壳公差。
   去耦及降压关键元件先布局，避免用全局自动布局解决电源环路。
5. **先关键网后普通网。** 先降压局部回路、USB 差分和源端扇出，再 I2S、
   SPI 等关键网，随后控制、电源分配和普通网。每阶段保存并检查，
   不因后续失败丢弃已验证的布线。不以增加复杂电路代替合理布局。
6. **建立平面与回流。** 四层为 F.Cu 信号 / In1 GND / In2 电源及必要
   GND 区 / B.Cu 信号；按模块数据手册保留天线禁区。检查所有高速信号
   下方的真实参考铜，避免跨分割，添加有效回流过孔并清除浮铜。
7. **闭环验证。** 最终填铜后重新执行 ERC、DRC、原理图一致性和未连接
   检查；额外审查直角、参考层、差分、载流和装配。修复全部问题；
   合理例外需记录并确认，禁止批量忽略整类错误。
8. **准备资料。** 仅在发布门槛满足后导出 Gerber、PTH/NPTH 钻孔、
   工厂格式 BOM/CPL、装配/极性图和叠层说明；逐一检查 CAM 和贴装预览。
   未完成件明确标记“草稿，不可生产”，不能包装成最终下单包。

## 必须执行的布线约束

- **不允许直角或锐角转弯**，使用 45° 或圆弧；审查真实连通几何、
  T 分支和铜区尖角，自动布线设置不是合格证据。
  这是用户要求，不编造成嘉立创统一拒收规则。
- I2S 优先顶层参考 In1 连续 GND。底层参考 In2，若分割电源导致断流，
  改走顶层或规划连通主地的 In2 GND 通道；换层就近提供有效地回流。
- 包地不能替代参考平面；两侧地铜必须实际连接、适当缝合，避免浮铜、
  细长地颈和不必要的电容负载。根据真实信号边沿分析，不只看采样率。
- 源端阻尼靠近真正驱动端；共享 BCLK/WS 缩短支线，检查 MIC_SD 与
  AUDIO_DIN 方向。按器件时序确定长度预算，不为了“等长”随意蛇形。
- USB 按实际工厂叠层设计 90 Ω 差分；线宽/间距未经计算不得宣称达标。
  保持对称参考和换层、减少脱耦/支线，检查 Type-C 重复脚合并、
  TVS 引脚连接和差分长度差。器件资料决定预算，不指定无依据阈值。
- 3 A 输入、二极管 OR、开关和降压供电按铜厚、压降、温升计算铜面/
  线宽/过孔数量，检查最窄入口和热连接，不能默认 0.20 mm。
- 降压关键回路遵守芯片推荐布局，SW 与反馈分离；功放输出远离麦克风
  和其信号/供电。不得把扬声器负输出接 GND。
- 全部贴片在正面不代表所有焊接仅需 SMT；插件/异形件与 USB 固定脚
  要逐项确认工厂工序、温度和费用。

## Windows / KiCad 操作

本机 KiCad 10：

```powershell
& 'C:\Program Files\KiCad\10.0\bin\kicad-cli.exe' sch erc --output '<会话目录>\erc.txt' .\kicad_ex\minibox-integrated.kicad_sch
& 'C:\Program Files\KiCad\10.0\bin\kicad-cli.exe' pcb drc --schematic-parity --output '<会话目录>\drc.txt' .\kicad_ex\minibox-integrated.kicad_pcb
```

替换 `<会话目录>` 为当前实际 artifacts 目录；其他工程替换输入文件。
先核对本机 CLI 选项。不要为 PCB 工作引入固件构建或额外测试框架。
命令成功退出不保证报告无违规，必须读取各类结果与未连接数量。
填铜在 PCB 编辑器或已验证的 KiCad API 流程完成，DRC 命令不替代填铜。

使用脚本时保留 PCB 所有未涉及内容和连接关系；本工程曾遇到
`pcbnew` 原生对象删除的 SWIG 所有权问题，避免对快照对象批量调用
`board.Remove()`。需要安全文件编辑时复用 `kicad_ex\kicad_sexpr.py`，
只修改目标项，重新加载并执行 DRC/一致性检查。
不要导入会立即执行生成的 `generate_project.py`，也不要对已布线板
运行布局生成器；同步生成来源时保留实际布线。

自动布线只能作为辅助。使用本地工具，不上传私有设计到在线路由服务，
关闭分析上传并保护关键手工布线；验证实际只用获准的信号层。
若自动布线未收敛，保留有效成果并明确剩余问题，不声称已完成。

## 交付判定

分别报告工厂规则核对、PCB 实际整改、DRC/未连接结果和待确认条件。
零 DRC 不能代替阻抗、热设计、EMC、装配公差或硬件实测。
不编造库存、工厂批准、最终报价或生产就绪状态。
