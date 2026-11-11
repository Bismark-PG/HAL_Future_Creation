# SlipperyTrail 涂鸦风材质 v0.1

## 画风目标

用户指定参考《Need for Speed Unbound》的夸张卡通特效。首版使用粗墨线、鲜明色块、手绘笔触和局部印刷网点，沿用绿紫配色。EA 的官方介绍将该作的驾驶特效描述为街头艺术与动作表达的结合。这里据此设计适合俯视竞技场的地面表现。 [EA 特效风格介绍](https://www.ea.com/games/need-for-speed/need-for-speed-unbound/news/the-world-is-your-canvas)

**2026-11-11 用户反馈与后续纲领：** 新版画风明显接近预期，颜色由用户继续调试；后续保留夸张卡通与手绘涂鸦方向，同时让颜色、纹理和边缘稍柔和、圆润。优先较宽、饱满的笔触，减少针状尖端、细碎噪声与过强色彩反差。分色和轮廓仍需清楚，按比赛镜头验收。此要求适用于后续特效，已同步[特效团队交接中的通用视觉纲领](MVPB_Vehicle_FX_Team_Handoff_CN_v0.1.md#量产特效视觉纲领2026-11-11-补充)。首版图片、数值和生成提示词作为历史起点保留，不代表柔和化已实施。

![基于当前游戏截图的涂鸦风概念预览](../../SourceArt/SlipperyTrail/SlipperyTrail_Graffiti_Concept_v01.png)

上图是 imagegen 编辑的风格目标图。实际材质使用下列资源和节点，观感以游戏内新配方验收为准。

![新保存材质的 D3D12 独立渲染检查](../../SourceArt/SlipperyTrail/SlipperyTrail_Graffiti_D3D12_Preview_v01.png)

上图为 Unreal 的真实渲染：新保存 MI 应用到带中央权重的瞬态地面网格，使用 18 cm 隆起和 4 cm 波动。它用于确认材质分色、笔触与网点已生效；背景、曝光和镜头为独立测试设置。概念图中的线条细节和整体观感仍需在比赛画面中调试，当前没有额外悬空喷溅／烟团层。

## 已创建的资源与挂接

| 资源 | 项目路径 |
| --- | --- |
| 母材质 | `/Game/Assets/FX/Materials/M_SlipperyTrail_Graffiti` |
| 实例 | `/Game/Assets/FX/Materials/MI_SlipperyTrail_Graffiti` |
| 笔触遮罩 | `/Game/Assets/FX/Textures/T_SlipperyTrail_Graffiti_Strokes` |
| 遮罩源图 | `SourceArt/SlipperyTrail/T_SlipperyTrail_Graffiti_Strokes.png` |

在当前 `DA_Ability_SlipperyTrail → Slippery Trail → Presentation → Ground Material` 选择 `MI_SlipperyTrail_Graffiti`，保存后重新释放技能。Deformation 勾选 Enabled，首测可用 Bulge Height=18 cm、Wave Amplitude=4 cm、Wave Period=1.5 s、Wave Length=220 cm，Width Subdivisions=8、Vertex Spacing=40 cm。首测数值由本轮选择，可按比赛镜头调节。

母材质使用 **Surface / Unlit / Masked**，Opacity Mask 连接常数 1。粗线和色块覆盖整个实际区域轮廓，车辆与球通过正常深度关系显示。材质通过同一技能 DA 引用实例，实例中的画风参数按下表调节，隆起和时间参数由 DA 的 Deformation 传入。

## 日常调参

| MI 参数 | 起点 | 调节目标 |
| --- | --- | --- |
| InkColor | sRGB `#100C23` | 深色轮廓和网点 |
| ShadowColor | sRGB `#6122A8` | 紫色侧面 |
| BodyColor | sRGB `#76E600` | 主色块 |
| TopColor | sRGB `#C5FF24` | 中央亮面 |
| AccentColor | sRGB `#FFF1C9` | 手绘速度笔触 |
| ColorGain | 1 | 整体颜色亮度 |
| DotTiling | 8 | 每个 UV 单元的网点密度，填写正数 |
| DotAmount | 0.45 | 紫色区域的网点强度，0 表示关闭 |
| StrokeMask | 新笔触遮罩 | 大块笔触图形 |
| StrokeTiling | 1.4 | 笔触平铺密度，填写正数 |
| StrokeAmount | 0.85 | 笔触覆盖强度，0 表示关闭 |
| StrokeSteps | 12 | 每个服务器波动周期中的动画步数，填写正数 |

颜色十六进制值以拾色器的 sRGB 输入为准。首轮先看主色块、外轮廓和白色笔触的比例，再调网点。比赛相机下需保留足够安静的主色面积，让车、球和危险范围都清晰。

### 描边粗细与柔和度

当前首版的描边阈值是母材质中的常量，**MI 尚无 Outline Width／Outline Softness 参数**。`InkColor` 只控制颜色。团队可在 Unreal Editor 直接调整母材质，操作如下。

1. 打开 `/Game/Assets/FX/Materials/M_SlipperyTrail_Graffiti`，在材质图中找到注释／描述为 **Ink border threshold** 的 SmoothStep。它的 Value 接 `VertexColor.R`，Min 默认 0.10、Max 默认 0.12，输出控制 InkColor 与 ShadowColor 的混合。
2. 选中该节点，在 Details 修改 Min／Max。保持 `0 ≤ Min < Max`，并让 Max 小于下一层紫色到主体色的起始阈值（首版为 0.40），避免色带互相挤占。
3. Apply／Save，确认技能 DA 引用的 MI 仍以此母材质为父材质；重新释放技能，从比赛镜头检查。

| 调节目标 | Min | Max | 含义 |
| --- | --- | --- | --- |
| 首版 | 0.10 | 0.12 | 当前起点 |
| 更细 | 0.04 | 0.06 | 两值同时减小，深色带更窄 |
| 更粗 | 0.18 | 0.20 | 两值同时增大，深色带更宽 |
| 接近原粗细、过渡柔和 | 0.08 | 0.14 | 保持中心值约 0.11，拉开间距 |

两值的中心大致决定描边宽度，间距决定颜色过渡柔和度。单位为中央权重阈值，实际宽度随区域半宽变化，数值与厘米／屏幕像素并非线性对应。调整只影响显示颜色带，区域形状、碰撞与打滑判定保持原规则。

如需长期在 MI 中调参，可在后续授权修改中将两输入参数化为 `Min=max(0, OutlineWidth-OutlineSoftness/2)`、`Max=OutlineWidth+OutlineSoftness/2`，保留正的 Softness 并限制阈值顺序。该接口仅为后续建议，本次没有新增这些参数。

描边过渡和笔触形状分别调节。现有 `StrokeAmount`、`DotAmount` 可降低笔触／网点对比；针状笔触需通过替换更圆润的 `StrokeMask` 改形。母材质中 **Bold brush shapes** 的 SmoothStep 默认 0.35／0.55，扩大该区间可柔化笔触颜色过渡，但不会自动将尖端变圆。本轮只记录方法，未调整实际资产。

## 节点结构

### 1. 粗轮廓与分块明暗

取网格的 VertexColor.R，记为 `W`。外边缘为 0，中央为 1。三个 SmoothStep 分别使用下面的 Min／Max，Value 都接 W。

| 分支 | Min／Max | 连接 |
| --- | --- | --- |
| 外轮廓 | 0.10／0.12 | Lerp 的 A=InkColor，B=ShadowColor，Alpha=此分支 |
| 主体色块 | 0.40／0.42 | Lerp 的 A=上一步颜色，B=BodyColor，Alpha=此分支 |
| 中央亮面 | 0.78／0.80 | Lerp 的 A=上一步颜色，B=TopColor，Alpha=此分支 |

较窄的过渡形成明确的色带。轮廓宽度按区域宽度的比例变化，区域扩宽时描边一起扩宽。

### 2. 局部印刷网点

`UV0 × DotTiling → Frac → 减去 (0.5,0.5) → Length`，得到每个单元到中心的距离。经过 Min=0.14、Max=0.18 的 SmoothStep，再接 OneMinus，得到圆点。

圆点乘 `外轮廓分支 × (1 - 主体色块分支)`，把网点限制到紫色侧面。再乘 DotAmount，作为从当前颜色向 InkColor 插值的 Alpha。

### 3. 手绘笔触与分步运动

`HAL_GroundWavePhase × StrokeSteps → Floor → 除以 StrokeSteps`，作为 Panner 的 Time。Panner 的 Coordinate 接 `UV0 × StrokeTiling`，Speed X=1、Y=0。

Panner 输出接笔触 TextureSampleParameter2D 的 UV。纹理采样使用 Masks、关闭 sRGB，X/Y 使用 Mirror，构建尺寸 1024×1024。R 通道经过 Min=0.35、Max=0.55 的 SmoothStep，得到清楚的大笔触。

笔触遮罩乘 `SmoothStep(0.50,0.55,W)`，再乘 StrokeAmount，作为从当前颜色向 AccentColor 插值的 Alpha。结果乘 ColorGain 后接 Emissive Color。

这条动画沿地面 UV 移动，由已复制的服务器相位驱动。每个周期分为若干步，表现手绘动画的节奏。

### 4. 隆起

新材质已接入 `HAL_GroundBulgeHeight`、`HAL_GroundWaveAmplitude`、`HAL_GroundWavePhase`、`HAL_GroundWaveUV` 四个现有参数，WPO 公式与公共组件一致。具体含义及手工接线见[通用地面变形说明](Ground_Surface_Deformation_CN_v0.1.md#材质接线)。

## 验收与交接

1. 将 Ground Material 选为新 MI 后，重新释放技能。在原地、直线和弯道确认深色轮廓、紫色侧面和亮色主体能从比赛镜头读清。
2. 将 StrokeAmount 和 DotAmount 分别设为 0，再逐项打开，调整笔触与网点的覆盖比例。
3. 从低视角确认隆起，再回比赛镜头调节高度和对比。检查车辆、球及场地边界的可见性。
4. 让车辆腾空后继续铺设，检查断口和共同消失。Host／Client 使用同一材质资源，检查分步动画及生命周期。

悬空的涂鸦图形、喷溅和卡通烟团可以通过额外的 Niagara 或网格层接入。当前材质资源提供地面轮廓、色块、网点、运动笔触与隆起。

## 本轮记录

基点 `main/a5dfa12`，UE 5.8.3-58210709。使用工程已有的 Unreal Editor Python API 流程创建三个新资源，母材质 59 个节点、67 条已检查连接。没有编辑 Blueprint 或覆盖旧材质／技能 DA。开始时已有的三份资产修改与文档修改已保留。

本轮保存后重新加载，D3D12 着色器编译通过；统计为 VS 173、PS 151 条指令、1 个采样器，统计随平台／编译设置变化。独立渲染 R5 通过：512×512 像素中检测到 38145 个绿色像素与 13911 个紫色像素，已人工查看实际导出图片。该检查确认保存资源可编译及渲染，不替代技能运行、双端或比赛镜头的美术验收。未修改 C++／配置，未新跑 Game／Editor Target 构建或玩法自动化；上一轮 WPO 程序测试保持原有含义。

R1～R3 的临时验证脚本因 Python 未暴露接口失败；R4 同步创建后立即抓取时仅得到天空。修正接口并等待 Editor 更新后，R5 实际抓取通过。所有原始记录在 `Saved/SlipperyTrail/Graffiti_20261111` 本地保留。1243 份原有保存资产／Config／uproject 的 SHA-256 与开工保护基线逐份一致。团队下一步仅将技能 DA 的 Ground Material 切换为新 MI，完成原地／弯道／绕回及 Host／Client 到期清理检查。本轮未提交／Push。

## 生成资源的提示词

两张源图由内置 imagegen 生成，模式为 built-in。概念图以本轮用户截图为编辑输入，笔触遮罩为独立生成。以下保留最终提示词，便于队友延续。

概念图提示词如下。

```text
Use case: style-transfer. Asset type: a preview concept for an Unreal Engine top-down vehicle party game's slippery trail ground material. Edit target is the supplied actual gameplay screenshot. Change ONLY the existing green oil trail surface under and behind the pink-and-turquoise car. Preserve the camera, tile floor, pink car, blue wireframe sphere, gray sphere, shadows, composition and all surrounding objects exactly. Keep the trail's existing curved footprint and clear gameplay border, with decorative art contained in the footprint. Redesign that trail in an exaggerated hand-illustrated street-art racing VFX aesthetic inspired by the graffiti-driving-effects language of Need for Speed Unbound. Broad acid-lime green fill, deep-purple side/shadow color blocks, thick near-black indigo ink border around its perimeter, a few broad sharp ivory brush streaks, restrained printed halftone dots in the purple shadow areas. Bold clean graphic shapes readable at this camera distance, crisp two/three-tone cel-style shading and slightly raised rounded gummy volume. Preserve wide quiet areas so the car and boundary remain readable. The result should look like deliberately drawn animated paint/graffiti laid onto the ground, with sparse irregular punchy strokes. Avoid realistic oil interference filaments, tiny busy noise, smooth glossy gradients, excessive bloom, floating particles, added UI, extra cars, labels or words. This is an illustrative target for a new material, not a claim of an implemented engine screenshot.
```

笔触遮罩提示词如下。

```text
Use case: stylized-concept. Asset type: square seamless grayscale alpha mask texture for bold hand-drawn graffiti speed strokes in a stylized racing-game ground trail material. A flat edge-to-edge 2D texture tile on solid pure black. Only white and very slightly anti-aliased white edges. Sparse large sharp, chunky, irregular dry-marker brush swooshes and tapered lightning-like paint slashes, arranged diagonally flowing from lower-left to upper-right. Roughly 8 to 12 distinct elongated marks at varied lengths, grouped into 3 or 4 small clusters, with 80 percent of the image remaining black negative space. Shapes readable when the tile occupies only 100 pixels in a top-down game. The white strokes are broad at their roots and taper sharply with occasional notched brush-cut ends, energetic illustrated street-art VFX feel. Repeatable seamless tile with periodic edge behavior. No real-world oil reflections, thin filigree, dots, gradients inside strokes, soft smoke, colored pixels, checkerboard, border, letters, logos, symbols or text. A production mask, not a concept painting.
```
