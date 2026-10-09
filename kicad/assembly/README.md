# 当前载板装配资料

本目录只有当前资料，历史由Git保存，不使用日期revision子目录或旧quote入口。
这些资料仍为工程审核输入，不等于生产批准。

| 文件 | 用途 |
|---|---|
| `bom-all-review.csv` | 全部物理件的精确型号、C编号及单板数量，上传全装配BOM |
| `positions-jlc-review.csv` | 已适配嘉立创库原点/零角的坐标，上传全装配CPL |
| `positions-all-review.csv` | KiCad自身参考中心和角度，不能替代工厂CPL |
| `jlc-placement-review.json` | 原CAD值、工厂修正值、逐孔误差及文件哈希 |
| `jlc-saved-placement.json` | 工厂实际存档刷新读回，仅对绑定的PCB哈希有效 |
| `pin-map.csv` / `assembly-pads.svg` | 逐孔网络与实际装配图 |
| `drc.json` / `manifest.json` | 实际导出门槛、配套哈希和未决审核项 |
| `minibox-assembly-review.zip` | 综合审核包，不作为纯Gerber上传 |
| `bom-smt-review.csv` / `cpl-smt.csv` | 仅贴片部分，不能用于全板焊接 |

修改PCB后运行`..\export_revision.py`同步所有当前输出；
更新渲染或文档后运行`..\package_review.py`刷新审核包及哈希。
布局改变时沿用已验证采购ID及库datum定义，从新PCB重算位置/角度；
不能复制旧工厂快照的绝对坐标。需要重新核对网页，但不应再逐件手调。
