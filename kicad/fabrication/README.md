# 当前 PCB CAM 资料

本目录固定保存当前板的Gerber、PTH/NPTH钻孔与钻孔图。
上传PCB使用`minibox-gerber-review.zip`；它仅含CAM，不含BOM或操作文档。
历史版本由Git保存，不创建日期revision目录或并存旧版ZIP。

运行`..\export_revision.py`重新检查实际PCB、四层CAM与钻孔配套。
装配BOM、工厂坐标及哈希清单在`..\assembly`。
零DRC与数字对位不代表工厂叠层、插件工艺、实物配合或生产批准。
