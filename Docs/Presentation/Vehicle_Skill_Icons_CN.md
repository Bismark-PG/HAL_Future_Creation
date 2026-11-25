# 车辆技能图标 · 方案 B

这套 512 × 512 透明纹理供主界面和选车页共用。每张用一种车型色作为切角底牌，暖白剪影表示技能动作。Car03 和 Car04 沿用定稿里的陀螺和蛮牛。其他车辆的底色参考实际车型的俯视涂装，浅色车的底牌适当压暗，给白色剪影留出对比。[整套预览](Images/VehicleSkillIcons_B.png)

## 车型对应

在 Unreal 的 `/Game/UI/UI_Textures/SkillIcons` 可以按下表的文件名查找 Texture2D。[透明 PNG 源图](../../SourceArt/SkillIcons/Competition) · [制图提示词](../../SourceArt/SkillIcons/Competition/Prompts.json)

| 车型 | UI 展示 | 纹理 | 底牌 |
|---|---|---|---|
| Car01 | 车号 01 占位 | `T_Skill_Car01_Placeholder` | `#C53D8F` |
| Car02 | 车号 02 占位 | `T_Skill_Car02_Placeholder` | `#19795B` |
| Car03 | 战斗陀螺 | `T_Skill_BattleTop` | `#F2B411` |
| Car04 | 蛮牛冲撞 | `T_Skill_BullCharge` | `#14ADA9` |
| Car05 | 龙卷风 | `T_Skill_Tornado` | `#1686AE` |
| Car06 | 警笛追捕 | `T_Skill_PolicePursuit` | `#465C6F` |
| Car07 | 侧向换线 | `T_Skill_SideShift` | `#DA581C` |
| Car08 | 烟幕 | `T_Skill_SmokeScreen` | `#355FA4` |
| Car09 | 消防水炮 | `T_Skill_WaterCannon` | `#C63849` |
| Car10 | 钩锁甩投 | `T_Skill_HookThrow` | `#727D84` |
| Car11 | 滑油 | `T_Skill_SlipperyTrail` | `#6C9437` |
| Car12 | 蝙蝠飞镖 | `T_Skill_Batarang` | `#32373C` |
| 共用 | 通用加速 | `T_Skill_MovementBoost` | `#3B6483` |
| 共用 | 冲击免疫 | `T_Skill_ImpactImmunity` | `#6D598C` |

**本次确认 Car10 展示钩锁、Car11 展示滑油。** 两辆车当前的车型 DA 都沿用 PlayerState 默认加速。Car01 测试装配为滑油，Car02 测试装配为通用加速，两辆车的 UI 均采用车号占位。正式选车目录目前列有 Car01 至 Car04。

## UI 使用

UMG 的 `Image → Brush → Image` 可以直接指定对应纹理。用白色 Tint、方形尺寸框，并保留纹理内的透明边距，让蛮牛的角尖完整露出。车辆列表或 HUD 推荐从 48 × 48 试起，技能详情页可使用更大的相同纹理。

现有 Catalog 的 `Icon` 是车辆缩略图，也是静态预览图的回退来源。后续按车型切换技能卡时，将本表纹理绑定到独立的技能图标字段。[选车 UI 引脚与技能卡接法](../Networking/RoomView_Vehicle_Skill_Card_CN.md)

## 验证

在本机 UE 5.8.3 重开 Editor 进程，逐张核对 14 份 Texture2D 的尺寸、透明 PNG 重导入路径、sRGB 和 UI 纹理设置。采用 SimpleAverage 多级纹理和三线性过滤。Unreal Canvas 按表中顺序在深浅两种底色各画三行，对应 32、48、64 像素。Windows 定向 Cook 生成 14 个资源包，0 错误、0 警告。正式主界面和选车页的控件接线待 UI 集成。[引擎小尺寸实拍](Images/VehicleSkillIcons_B_HUD.png)
