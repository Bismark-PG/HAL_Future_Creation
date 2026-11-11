# 量产主动技能 · 扩散打滑带 v0.1

## 规则

- 车辆从物理碰撞体的后端往地面铺设。倒车仍从车尾铺设，原地发动会先留下一块圆形区域。已经铺下的地面点固定在世界坐标，每段从自身铺下的时刻起向两边扩宽，直到达到宽度上限。直线行驶留下两端圆润的近似梯形，转弯沿实际走过的路线。
- 铺设窗口和整片玩法结束时间均从发动起计时。腾空、翻倒、自救执行或地面探测无效时暂停铺设，恢复后新段从当前位置重新起笔。旧段继续扩散；玩法到期后停止生效，所有区段共同进入默认 1 s 的分块消散。时长可在 Presentation → Dissolve Seconds 调节，0 为立即消失。
- 同一条打滑带只有一个区域来源。车辆中心在适当高度的地面投影进入区域后，横向抓地降低，并有受速度限制的交替转向扰动。油门、刹车和转向仍可用，静止车辆不受摆动力。离开区域时摆动力结束，抓地按既有曲线恢复。高速横穿使用两次采样间的地面扫掠。
- 多源重叠取最低抓地倍率和单个最大摆动振幅，两个结果可以来自不同来源。同振幅时依次比较更长周期、更低满幅速度、更短渐入时间，选择与注册顺序无关。施放者免疫自己的区域，可受其他车辆的区域影响。普通冰面仍只提供低抓地。
- 这项技能只影响车辆驾驶。球、伤害、眩晕和脱球继续由各自系统结算。发动要求正立接地、车尾能投影到可支撑地面、车辆未眩晕或执行自救。冷却从发动开始，同一辆车的本次玩法区域结束前不能再次释放；视觉消散不额外占用 GAS 活跃状态。淘汰、换车、离场和比赛结束会清理所属区域。

## Editor 接线

1. 使用包含本次改动的 Editor 构建，在 Content Browser 中创建类型为 `HALSlipperyTrailConfig` 的 Data Asset，建议放入 `/Game/Data/Abilities`，命名 `DA_Ability_SlipperyTrail`。
2. 在 `/Game/Blueprints/GA` 创建父类为 `HALSlipperyTrailAbility` 的 Blueprint，例如 `GA_SlipperyTrail`。在 **Class Defaults → Slippery Trail → Skill Config** 选择上一步 DA。原生 GA 已自带冷却 GE，无需再创建 GE 或连接 Event Graph。
3. 在测试车型的 Vehicle Definition 里，将 **Abilities → Version** 设为 `Vehicle Definition V1`，在 **Grants** 将唯一的 `Input.Ability.Primary` 条目指向新 GA，**Level** 填 1。车型资产还没导入时，可以用一份已接好的车辆 Definition 副本和相应车辆 BP 副本进行测试。使用房间选车时把副本登记到测试用 Vehicle Catalog，使用现有主技能按键。
4. 在房间准备好车辆后，以 Standalone 或 Listen Server + 客户端进场测试。地面使用会阻挡车体的 `WorldStatic` 或非模拟刚体的 `WorldDynamic` 碰撞。Development 默认显示青色调试线，可以在 DA 的 **Shape → Draw Outline** 关闭。地面特效使用下文的 **Presentation → Ground Material**。

| DA 分组 | 首测值 |
| --- | --- |
| **Cooldown Seconds** | 18 s |
| **Emission** | 铺设 3 s，采样间隔 0.04 s，最小取点距离 65 cm，连段上限 440 cm，最多 128 点，物理车尾外偏移 20 cm |
| **Emission / Ground** | 向下探测 170 cm，地面法线 Z 至少 0.55 |
| **Shape** | 发动后 7 s 玩法结束，半宽从 70 cm 扩至 280 cm，每个点扩宽用时 1.6 s，同层高度容差 65 cm |
| **Presentation / Dissolve Seconds** | 玩法结束后默认 1 s 分块消散，0 为立即消失；范围在玩法截止时冻结 |
| **Handling** | 横向抓地倍率 0.30，离开后 0.35 s 恢复 |
| **Handling → Sway** | 幅值 5 rad/s²，周期 0.9 s，700 cm/s 达到满幅，渐入 0.2 s |
| **Shape / Outline** | 青色、距地 5 cm、线宽 2 |

抓地恢复沿用现有区域规则。幅值控制方向被拨动的力度，周期控制左右摆的频率，满幅速度越高，低速摆动越轻。首轮可以分别调抓地和摆动，以区分侧滑和左右拨动的贡献。

## 暗绿油墨地面材质

2026-11-11：当前涂鸦母材质已接入[分块蒸发消散](../Presentation/Slippery_Trail_Dissolve_CN_v0.1.md)。新增尾效时间在本技能 DA；图块、边缘和颜色在原 MI 的 05 Dissolve 调节，保留既有体积明暗和隆起。

当前双层流动与绿紫高光的接线、调参见[油膜特效速查](../Presentation/Slippery_Trail_OilSheen_QuickGuide_CN.md)。本节保留纹理导入、初版静态材质和技能 DA 接线步骤。

网格细分、隆起和波动已接入可复用的 `UHALGroundSurfaceComponent`。在母材质增加 WPO 接线后，通过本技能 DA 的 **Presentation → Deformation** 启用。默认高度 8 cm、起伏 2 cm、周期 3 s，默认勾选关闭。具体节点、参数约束和未来区域的复用方式见[通用地面隆起与波动](../Presentation/Ground_Surface_Deformation_CN_v0.1.md)。

地面网格使用服务器已有的轨迹、每点铺设时间和地面法线。两端按同一服务器时钟扩宽，连接段共享横截面，起止端圆润，腾空后的新段单独起笔。UV0 按固定的地面距离重复，扩宽时增加覆盖范围，纹路保持原有大小。网格在到期或 Actor 退场时清理，碰撞和导航均关闭。常规弯道使用相邻方向的平均切线，急回头时覆盖可能与青色判定线略有差别。同一轨迹绕回旧段时，半透明表面可能出现叠色。

### 1. 导入灰度纹路

1. 在 Content Browser 打开目标文件夹，例如 `/Game/Assets/FX/SlipperyTrail`。点击 **Import**，选工作区中的 [T_SlipperyTrail_OilSheen_Mask.png](../../SourceArt/SlipperyTrail/T_SlipperyTrail_OilSheen_Mask.png)。
2. 打开导入的 Texture 资产。**Compression Settings** 选 `Masks (no sRGB)`，确认 **sRGB** 关闭。**X-axis Tiling Method** 和 **Y-axis Tiling Method** 选 `Mirror`，让反光纹路在平铺接缝处连续。
3. PNG 的源尺寸是 1254 × 1254。为使用常规 mip 链，在 **Power of Two Mode** 选 `Resize to Specific Resolution`，把 **Resize During Build X/Y** 设为 `1024`，保存纹理。

### 2. 建材质和实例

1. 新建 `M_SlipperyTrail_OilSheen`，在材质 Details 里设 **Material Domain** 为 `Surface`，**Blend Mode** 为 `Translucent`，**Shading Model** 为 `Unlit`。
2. 添加 **Texture Sample Parameter2D**，命名 `OilSheenMask`，选导入的遮罩，**Sampler Type** 选 `Masks`，**Sampler Source** 用 `From texture asset`。`UVs` 使用默认 UV0。
3. 新建 `Scalar Parameter` **PatternStrength**，默认 `2.0`。遮罩的 **R** 与 `PatternStrength` 进 `Multiply`，再接 `Saturate`，输出连到 `Lerp` 的 **Alpha**。
4. 新建两个 `Vector Parameter`。`TrailBaseColor` 接 `Lerp A`，首测线性 RGB 可填 `(0.015, 0.06, 0.025)`。`TrailHighlightColor` 接 `Lerp B`，首测可填 `(0.08, 0.42, 0.18)`。`Lerp` 输出接 **Emissive Color**。
5. 新建 `Scalar Parameter` **TrailOpacity**，默认 `0.55`，直接接 **Opacity**。应用、保存，然后在 Content Browser 右键母材质创建 `MI_SlipperyTrail_OilSheen`。打开实例即可分别调整底色、反光色、纹路强度和透明度。

### 3. 接入技能 DA

1. 用本轮编译后的 Editor 打开现有 `DA_Ability_SlipperyTrail`，展开 **Slippery Trail → Presentation**。将 **Ground Material** 设为 `MI_SlipperyTrail_OilSheen`。`Ground Lift` 首测 `2 cm`，`Texture Tile Size` 首测 `320 cm`，后者表示一个纹理单元覆盖的地面距离。
2. 首次进场保留 **Shape → Draw Outline**，对照材质与青线。确认后可以关闭青线，只看材质。若地面互相闪烁，可少量提高 `Ground Lift`。材质槽留空时可继续单独使用青线。
3. 保存，重启同版 Host／Client。依次看原地圆形、直行扩宽、弯道、腾空断段、横坡和到期清理。在比赛镜头下调 `TrailOpacity` 与两种颜色，确认暗绿色在场地底色上读得清楚。

源 PNG 由内置 imagegen 生成并保留在 `SourceArt/SlipperyTrail`，由团队手动导入为 UE 纹理。绿色由材质实例指定。生成提示如下。

```text
Use case: stylized-concept. Asset type: a square seamless repeatable grayscale intensity mask for a dark-green oil-ink energy trail on the ground in a top-down racing game. Generate a single flat orthographic texture tile, edge to edge. Draw organic flowing oily liquid interference highlights as intertwined, elongated curving filaments, soft streaks and smooth ripples, on a predominantly charcoal gray base. The streaks should have medium-scale spacing, locally varying brightness and sparse, gently bright silver-gray peaks, readable from a high camera. Preserve the impression of wet black ink reflecting thin bands of light. Strict neutral grayscale in every RGB channel so the Unreal material can set dark green tint independently. Match opposite edges for a true 2D repeat. Even, stationary detail distribution and no obvious center or radial focal point. No text, border, drop shadow, vignette, isolated puddle outline, perspective, ground scenery, symbols, or colored pixels. Full square texture with dark midtones and smooth local gradients, appropriate to sample the red channel as a mask.
```

## 验收

1. 原地发动应出现一片逐步扩大的圆。直行、弯道和倒车分别确认车尾出线、老段固定、宽度随时间变化。被撞飞后继续开，确认空中没有连线，先前铺下的区域仍生效。
2. 另一辆车沿区域、横穿区域和低速进出。对比方向摆动、停住后起步、松油门和离开后的恢复。施放者回到自己的带上应正常驾驶。两条带和普通冰面交叠时检查抓地取最低，摆动没有倍增。
3. 验证球经过、车辆持球经过、上下层和斜坡。单独验证离场、淘汰和换车时的清理。双端使用同一构建，观察整条带是否在同一个时刻消失，以及车辆是否能正常恢复转向。
4. 日志可用 `hal.AbilityLog 1` 和 `hal.MovementAreaLog 1`，后者显示目标抓地、摆动幅值和来源数。联机运动历史格式为 5，客户端与 Host 使用同一版可执行文件。

GAS 执行技能许可、冷却和选中的区域抓地 GE。区域 Actor 负责服务器地面采样、占用和复制调试范围，车辆移动组件在 Chaos 步内使用服务器帧计算摆动。临时范围和后续美术可继续沿用同一条区域时间线。

## 本轮验证

- 基线为 `main/0d50d76`，引擎 UE 5.8.3-58210709。Development Editor 和 Game Target 编译通过。原生套件 **187/187**，新增覆盖原地扩宽、限宽、渐变段、高速横穿、不同楼层、断开和整片到期，以及单摆动力选择、低速缩放和历史回放。
- 本机 Listen Server 与客户端专项各 **1/1**。使用直连图和临时测试配方，Host 出站注入 80 ms。确认主技能能生成地面区域，客户端收到范围和生效时间，施放者免疫，对手在同一来源下收到抓地和摆动参数，并在共同到期后恢复。双端运行使用 NullRHI，画面、正式车型接线和摆动手感待团队实测。
- 既有减速／冰面联机回归双端各 **1/1**，覆盖重叠、最强参数切换、恢复、重进和销毁。客户端记录了实际物理重模拟。
- 日志位于 `Saved/SlipperyTrail/20261109`，专项报告位于 `Network_R4`，旧区域报告位于 `MovementAreas_R5`，原生报告位于 `Native_R3`。R3、R4 使用相同生产代码，R4 补齐直连图车型的临时配方映射。

## 2026-11-10 地面表现验证

- 以 `main/0e017ce` 接续，在 UE 5.8.2-56702186 完成 Editor 和 Game Development 构建。地面网格使用引擎的 `GeometryCore` 与 `GeometryFramework`，在扩宽期间最多每秒重建 30 次，并在达到最终宽度后复用网格。斜坡取样、未来采样过滤、断段和到期由新增原生用例覆盖。
- 工作区原生报告 177 通过、11 失败。失败用例期待旧免疫／加速主技能，而工作区玩家 Definition 已改为打滑带授权。用相同构建在隔离副本恢复两份本地变更资产的提交版后，原生 **188/188**，含打滑带 **3/3**。
- 本机 D3D12 离屏双端专项 Host／Client 各 **1/1**，验证临时材质复制、网格注册、扩至完整宽度、无碰撞及到期回收。实际暗绿材质观感待导入后验收。编译、报告和日志位于 `Saved/SlipperyTrail/Presentation_20261110_154453`。
- 开工时监听 GameMode BP 和玩家 Definition 含本地修改。验证前后 1215 份 `.uasset`／`.umap` 的 SHA256 一致。新 PNG 走源素材目录，由团队按上文导入。
