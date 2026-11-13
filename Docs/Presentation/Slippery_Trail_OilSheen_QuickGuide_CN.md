# 扩散打滑带 · 油膜特效速查

暗绿底纹缓慢流动，较亮的纹路独立移动并在绿、紫之间换色。材质用于显示地面范围，打滑强度、扩散和寿命由技能配置控制。

## 资产与挂接

- 遮罩在 `/Game/Assets/FX/Textures/T_SlipperyTrail_OilSheen_Mask`。从[源 PNG](../../SourceArt/SlipperyTrail/T_SlipperyTrail_OilSheen_Mask.png)重新导入时，使用 `Masks (no sRGB)`、关闭 sRGB、X/Y 平铺设为 `Mirror`，构建尺寸设为 1024 × 1024。材质采样使用 `Masks`。
- 母材质 `M_SlipperyTrail_OilSheen` 和实例 `MI_SlipperyTrail_OilSheen` 都在 `/Game/Assets/FX/Materials`。母材质使用 **Surface / Unlit / 半透明**，输出接 **Emissive Color** 和 **Opacity**。
- 打开 `DA_Ability_SlipperyTrail → Slippery Trail → Presentation`，将 **Ground Material** 指向上述实例。**Ground Lift** 和 **Texture Tile Size** 的起点分别是 **2 cm** 和 **320 cm**，控制网格抬高与纹路尺寸。

## 母材质接线

| 分支 | 接线 |
| --- | --- |
| 底纹 | `TexCoord[0]` → 底纹 `Panner` → 遮罩采样的 **UVs**。采样 **R** × `PatternStrength` → `Saturate` → 底色 `Lerp.Alpha`。底色 A 接 `TrailBaseColor.RGB`，B 接 `TrailBaseTint.RGB`。 |
| 高光范围 | `TexCoord[0]` → 高光 `Panner` → 另一份遮罩采样的 **UVs**。采样 **R** → `SmoothStep.Value`（Min **0.18**、Max **0.30**）→ 最后一个 `Lerp.Alpha`。 |
| 高光颜色 | `Time × OilShimmerSpeed + TexCoord[0].R × 0.35` → `Sine`（Period **1**）→ **× 0.5，再 + 0.5** → 变色 `Lerp.Alpha`。变色 A 接 `TrailHighlightColor.RGB`，B 接 `OilAccentColor.RGB`，输出再乘 `OilGlow`。 |
| 最终输出 | 最后一个 `Lerp` 的 **A** 接底色输出，**B** 接乘完 `OilGlow` 的高光色，结果接 **Emissive Color**。`TrailOpacity` 直接接 **Opacity**。 |

两套 `Panner` 的 **Time** 均可留空。速度可从底纹 **(0.016, −0.008)**、高光 **(0.025, 0.008)** 起调。这些是调参参考值。

## 日常调参

- 打开材质实例并勾选要覆盖的参数。用 `TrailBaseColor` 和 `TrailBaseTint` 调两种底色，用 `PatternStrength` 调底纹的明暗变化。
- 用 `TrailHighlightColor` 和 `OilAccentColor` 调两端的高光颜色，用 `OilGlow` 调高光亮度，用 `OilShimmerSpeed` 调换色速度。整片的透明度用 `TrailOpacity`。
- 纹路移动速度在母材质各自的 `Panner` 上调。高光太少时，先小幅降低 `SmoothStep` 的阈值。
- 保存后在比赛镜头下释放一次技能，观察底纹流动、高光换色以及整条地面带到期清理。需要对照实际打滑范围时，可在技能 DA 打开 **Shape → Draw Outline**。

技能授权、规则和联机验收见[扩散打滑带技能说明](../Combat/Vehicle_Slippery_Trail_Ability_CN_v0.1.md)。

地面隆起与缓慢蠕动可在母材质接入公共 WPO 参数，再在技能 DA 的 **Presentation → Deformation** 启用和调节。逐节点接线及参数见[通用地面隆起与波动](Ground_Surface_Deformation_CN_v0.1.md)。
