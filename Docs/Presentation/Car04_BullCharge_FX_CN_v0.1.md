# Car04 蛮牛冲撞：生产特效与挂点配置 v0.1

2026-11-14，从 `main/50f3237` 接续，UE 5.8.3-58210709。用户已同意三层表现方案。Car03 特效接受为初版，后续优化暂缓。

本文记录已落地的接口与操作方式，实际验证范围见末节；真人观感和发布包验收独立进行。

## 第五轮：破风罩整体柔和三阶明暗（当前）

用户明确要求类似 TireSkid 的罩面卡通体积感：暗／中／亮三档覆盖整个曲面，色差轻微、过渡自然；取消外围深色环。原第三轮的共享轮廓方案由本节替代，气流自身仍保留原勾边配置。

配置入口为 **`DA_Ability_Car04_BullCharge → Presentation → FX → Pressure Shell → Toon Shading`**。新增字段以默认值兼容当前已保存的 DA，运行时按每次发动的表现快照传给罩子独立 MID，不覆盖团队原调参。

| 参数 | 默认值 | 用途 |
| --- | --- | --- |
| Pressure Shadow Color | 线性 RGB `(0.52,0.56,0.60)` | 罩面的暗档颜色 |
| Pressure Mid Color | 线性 RGB `(0.70,0.74,0.77)` | 大面积中间档颜色 |
| Pressure Light Color | 线性 RGB `(0.88,0.91,0.94)` | 朝向虚拟光方向的亮档颜色 |
| Pressure Shadow Threshold | 0.40，范围 0～1 | 暗档向中档转换的位置；增大后暗档覆盖更多 |
| Pressure Light Threshold | 0.75，范围 0～1 | 中档向亮档转换的位置；增大后亮档覆盖更少 |
| Pressure Tone Softness | 0.18，范围 0.02～0.4 | 两处颜色过渡的完整宽度；增大更柔和，减小分档更明确 |

阈值是曲面法线明暗坐标，不是距离／厘米；亮档阈值须比暗档阈值至少高 0.02。实际过渡宽度最多取两阈值间距的 90%，保留中间档。三色 Alpha 不用于调透明度，仍由原 `Pressure Opacity` 控制；原 `Pressure Feather` 独立控制后缘和视角轮廓的透明渐隐。`Wind Contour Color/Width` 从本轮起只控制气流，不再染色罩子。母材质仍为 Translucent／Unlit，以世界空间曲面法线和固定虚拟光方向计算整体三阶明暗，不依赖场景灯光强弱。

维护程序只更新当前 `M_BullCharge_ContourShell` 的颜色／透明度求值图，保留既有 WPO、Niagara 材质引用和挂接。不新增 NS、不修改 BP、不重设技能 DA。风带长度、扰动、流速、加速渐进出现继续沿用第四轮。程序维护入口为显式 `-RefreshSoftPressureSurface`，已安装后拒绝重置美术调参；正常团队调参无需运行维护工具。

操作：重开 Editor，直接释放 Car04 原技能检查整体柔和分色；需要更明显时先小幅拉开三色明度，再调过渡宽度。修改 DA 保存后，下次发动生效。无需重新引用 Niagara 或修改 BP。

本轮最终 **Editor R2／Game R2** 完整编译通过，原生／Car03 回归／实际 D3D12 **14/14**、零测试警告／失败。深色参考背景上，独立调整暗／中／亮颜色分别改变 **6856／19573／24298** 像素；过渡宽度改变 **3136** 像素；气流勾边颜色／宽度对罩面的影响为 **0** 像素。罩面额外覆盖 **35810** 像素，原六秒持续粒子、共同罩尖、扰动、尾喷、加速渐进显现及暂停清理继续通过。已查看默认罩面和车辆组合图，默认配色刻意轻柔；浅色地板的明显对比仅 52 像素，不能把它描述成任意背景下都醒目的高对比罩子。

最终 80 ms Host 延迟的实际渲染双端各 **1/1**，两个进程退出 0，零测试警告／失败；瞬态配方用非默认中间色 `(0.61,0.66,0.72)`／过渡宽度 0.22，验证两辆被观察车辆的参数复制与 MID 绑定、原阶段／暂停恢复和清理。网络撞击仍是表现传输探针。

原生 API 只更新当前 `M_BullCharge_ContourShell`，**1303** 份既有资产／Config／uproject 中其余 **1302** 份 SHA256 不变，无新增资源、无 BP／地图／Niagara／技能 DA 保存。首次渲染为 13/14，两项断言依赖浅色地板上的高对比，不适配本轮轻柔分色；保留材质与默认值、保留原数字阈值，改用深色背景单独检查罩面覆盖和过渡，同时保留浅色预览后，最终 14/14。注意 UE 在失败轮次也返回进程码 0，必须检查报告计数。见[第五轮验证摘要](Car04_BullCharge_SoftShell_Verification_20261114.json)，本机原始报告和图片在 `Saved/Car04FX/SoftShell_20261114/Render_R2` 及同级测试目录。真人画风／真实相机和场地、四机、Cook／发布包与完整房间 BP 流程待团队验收；未提交／Push。

## 第四轮：独立长度与加速渐进显现

用户要求风带长度独立于宽高，并让持续特效随加速时间逐渐出现。本轮增加 **Presentation → FX → Front Wind → Wind Length Scale**，默认 **1**，范围 **0.1～4**。它乘在原 `Wind Size Scale` 的局部 X 轴上：0.5 是原长度一半，2 是两倍；Y／Z 不变，侧向／竖向扰动幅度、罩子尺寸和罩尖连接位置保持。原 `Wind Size Scale` 继续负责整体大小，两个倍率相乘得到最终前后长度。新字段由默认值兼容既有 DA，不需重新应用配方或重存资产；保存的团队调参不覆盖。

原实现气流阶段系数从 0.65 起、尾喷从 0.4 起，高速发动时会突然出现。本轮把风带／罩子的材质 `Envelope` 和尾喷 `SpawnRate` 阶段系数改为从零到一，复用 **Charge → Motion → Ramp Seconds** 和本次技能的服务器起始时间，不新增第二套独立时长。例：Ramp Seconds=2 时，0／0.5／1／2 秒对应阶段权重 0／0.25／0.5／1。风带实际透明度仍结合向前车速，尾喷数量随阶段增多；尺寸不做缩放入场，碰撞瞬发反馈保持及时。

`Wind Response Seconds` 只平滑车速引起的强度变化，之后再乘加速阶段，避免平滑把加速淡入整体拖后。后到观察者按原服务器阶段接续；眩晕／腾空／翻倒导致驱动暂停时仍立即隐藏风带／罩子并停止新尾喷，恢复时沿用原阶段。Ramp Seconds=0 表示没有加速过渡，此时阶段立即到一。没有修改技能推进、球、伤害、冷却、触发资格或 GA Tick。

操作：重新打开完整编译后的 Editor，在原技能 DA 调 `Wind Length Scale`，保存、等本次技能结束后再发动。调 0.5／1／2 对比长度；分别从静止和满速发动，确认特效在加速时段内逐渐增强。仅调 DA 不需要重新 Build。Niagara／材质引用和 BP 接线继续沿用第三轮资源。

第四轮已完成 **Editor R4／Game R1** 完整编译、原生／实际 D3D12 **14/14** 和 80 ms Host 延迟的双端各 **1/1**，零测试警告／失败。实际粒子读回覆盖 0.5／1／2／4 长度倍率，确认宽高、罩子和共同起点保持；满路速发动的渲染样本在 0／0.5／1／2 秒分别读回 0／0.25／0.5／1 的材质权重和 0／50／100／200 的尾喷发射量，实际画面亮度逐步增加。另覆盖后到观察者使用现有阶段、暂停立即隐藏、既有 Car03／Car04 规则与持续特效回归。网络使用瞬态 1.6 长度倍率，确认拥有者及被观察车辆的挂接参数绑定和阶段同步。

**1303** 份既有资产／Config／uproject 全部 SHA256 不变，没有新增或保存特效资产，也没有保存 BP／地图／技能 DA；团队现有调参保留。首次完整编译的验证代码有一个材质读取接口 const 类型错误，已修正并在 Editor R4 通过；保留既有引擎弃用／工具链提示。详见[第四轮验证摘要](Car04_BullCharge_LengthRamp_Verification_20261114.json)。原始报告和四步图片在本机 `Saved/Car04FX/LengthRamp_20261114/`。真人观感、四机、Cook／发布包和完整房间 BP 流程待团队验收；网络撞击仍是表现传输探针，不冒充真实远端物理碰撞测试。未提交／Push。

## 第三轮：共用罩尖、几何扰动与卡通勾边

用户要求风带从罩尖附近展开，风带本身有可见扰动，风带／罩子增加类似 Tire Skid 的深色卡通色阶，同时保留尾喷速度并缩短距离。本轮采用独立的 `NS_BullCharge_ContourWind` 配方，保留前两轮资源。

- 风带网格的七个根部收拢到共同原点，再由 `User.FlowOrigin` 移到罩子前向深度处。风带缩放与罩子深度分别配置，调罩子深度时起点自动联动。根部的形变权重为零，扰动不会把连接处扯开。
- 原材质只做了明暗流纹，网格形状固定，实际画面运动感不足。本轮增加侧向／竖向 WPO 波动，并由 C++ 每帧传入本次技能经过时间，使用显式运行时钟；Niagara 编辑器预览仍可使用粒子年龄回退。扰动属于表现，不改变碰撞、推进、伤害或技能时长。
- 统一增加偏深灰的软勾边与暗／中／亮三档明暗。`Wind Contour Color`／`Wind Contour Width` 在原技能 DA 调节，宽度为零时关闭勾边。保持圆润、柔和轮廓，避免尖锐黑线。
- `Wind Turbulence` 默认 8 cm，`Pressure Turbulence` 默认 2.5 cm；风带振幅按风带缩放作补偿。`Flow Speed` 控制流纹和扰动速度，设置零时停止动画。其余原有尺寸、透明度、挂点和颜色参数继续沿用。
- 尾喷只将 `Rear Lifetime` 收紧到 0.18 秒，保留保存的喷射速度、数量、尺寸和颜色。

新增资源为 `M_BullCharge_ContourWind`、`M_BullCharge_ContourShell`、`SM_BullCharge_ConnectedFlow`、`NS_BullCharge_ContourWind`；继续复用上一轮罩子网格。维护入口为 `-CreateContourWind` 与显式 `-ApplyContourWind -SkillAsset=...`，拒绝覆盖同名资源或重置已接入该配方的 DA。**本轮已接入，重开 Editor 释放 Car04 原主技能即可复测，当前验收见下方第三轮记录。**

罩子的勾边与主体渐隐分别处理：主体继续软化，暗边保留足够透明度，并在最外沿柔和收掉。当前轮廓颜色为线性 RGB `(0.055,0.075,0.085)`，是偏深灰，不是纯黑。此值经浅色地板的渲染检查调整；使用黑色背景预览时，深色轮廓本来就较难辨认。

## 第二轮：连续气流与软边破风罩（2026-11-14）

用户已确认首版基本逻辑正确，要求气流更强、更流畅，增加漫画突进时车头的模糊压力罩，并明显强化尾喷。本轮保留第一轮资源，新增 `NS_BullCharge_PressureWind`：七条连续曲面风带和一层椭球形软边压力罩，各用一个长寿命 Mesh 粒子。风带运动由材质随粒子时间连续流动，不依赖短寿命粒子出生／消失。组件在技能结束时销毁，不延长逻辑技能。

首版低中速时，速度同时降低发射率和透明度；14 个／秒、0.32 秒寿命在较低风力下容易只剩一个或不足一个可见网格，叠加随机缩放与生命周期渐隐，会产生断续感。本轮将连续主体与发射率解耦，风力使用更易读的速度曲线与帧率无关的指数平滑。驱动许可关闭时立即归零，眩晕／腾空／翻倒仍服从原规则。表现更新依赖车辆 Presentation 完成当帧更新，不改变物理步或权威运动。

破风罩是车头附近的灰白半透明压力曲面：软边、视角相关边缘与连续后掠条带，共同表现漫画式速度和压力。主体没有碰撞、伤害或免疫功能。风带和罩子分别控制透明度；罩子半轴尺寸与柔边宽度都在技能 DA。尾喷初调为每喷口 200 个／秒、2600 cm/s、16 × 50 cm，寿命 0.38 秒，保留原暖色与喷口位置。

新增资源为 `M_BullCharge_ContinuousWind`、`M_BullCharge_PressureShell`、`SM_BullCharge_FlowStrips`、`SM_BullCharge_PressureShell`、`NS_BullCharge_PressureWind`，均在原 `BullCharge` 目录。旧风带、尾喷和撞击资源不覆盖。维护入口新增 `-CreatePressureWind` 与显式 `-ApplyPressureWind -SkillAsset=... -PresentationAsset=...`；配方应用逐字段保护原挂点、撞击配置、颜色及所有玩法数值。**本轮已完成并接入，重开 Editor 用原 Car04 主技能即可测试，具体复测见下方第二轮记录。**

## 范围与职责

- 车头：无明显色相的近白／灰色三档卡通风带，从车头向两侧和后方掠过；局部空间，随车头方向移动。强度结合实际向前速度与技能阶段，停车时没有满强度破风。
- 车尾：柔和卡通短火星，技能正在有效驱动时持续发射，达到速度上限也继续；发射器跟随挂点，已喷出粒子在世界空间运动。
- 撞车：沿用 Car03 已接受的圆润火花风格，独立制作 Car04 配方，叠加接触点附近的短闪光和扩张冲击环。只有服务器现有冲锋接触资格及每目标 CD 接受、CombatResolver 接受后才触发。球和墙不套用同级撞车反馈；免疫不以 HP 是否减少来替代接触资格。
- 眩晕、腾空、翻倒、自救等导致主动驱动暂停时，关闭新尾喷和强破风；恢复按原技能进度继续。旧尾喷粒子可以按短寿命自然消失。技能结束、淘汰、换车、离场销毁本组件产生的持续和一次性粒子组件。

GA／GE／Tag、车辆物理、球发射和伤害／冲击结算继续使用原实现。C++ 组件只增加表现快照、挂接与事件输出，不由 Niagara 或 Blueprint OnHit 决定命中。

## 资源与引用

全部新资源放在 `/Game/Assets/FX/BullCharge/`，不修改已接受的 Car03 资源。

| 资源 | 作用 |
| --- | --- |
| `NS_BullCharge_FrontWind` | 局部空间车头曲面风带 |
| `NS_BullCharge_ContourWind` | 第三轮当前配方：共用尖端、几何扰动、深灰勾边 |
| `SM_BullCharge_ConnectedFlow`／`M_BullCharge_ContourWind` | 根部收拢的七条风带与卡通材质 |
| `M_BullCharge_ContourShell` | 罩子三档明暗、软勾边与轻微形变 |
| `NS_BullCharge_PressureWind` | 第二轮持续风带＋软边破风罩，保留作历史配方 |
| `SM_BullCharge_FlowStrips`／`M_BullCharge_ContinuousWind` | 连续后掠风带，材质驱动流动 |
| `SM_BullCharge_PressureShell`／`M_BullCharge_PressureShell` | 车头椭球压力曲面、软边与半透明流纹 |
| `SM_BullCharge_WindStreaks`／`M_BullCharge_FrontWind` | 两侧及上方风带网格、灰白三档 Unlit 材质 |
| `NS_BullCharge_RearSparks` | 世界空间持续尾喷火星 |
| `NS_BullCharge_ToonImpact` | 火花＋局部闪光＋扩张冲击环三个 Emitter |
| `M_BullCharge_ToonSpark` | Car04 独立的柔和卡通短火星材质 |
| `M_BullCharge_Flash` | 短促、圆润的局部闪光 |
| `SM_BullCharge_ShockRing`／`M_BullCharge_Shock` | 有曲面的冲击环，WPO 扩张、短时退场 |

唯一配置入口是 **`/Game/Data/Abilities/DA_Ability_Car04_BullCharge` → Presentation → FX**。既有 `GA_Car04_BullCharge` 的 Skill Config 引用沿用，不需要修改车辆 BP Event Graph 或把技能表现参数放回车辆 Presentation DA。

资源生成／配方应用工具分别有独立入口，拒绝覆盖已有资源／已启用的技能配方。配方应用只改 `FX`，并逐反射属性验证其他技能数值未变。工具不会保存 BP、Skeleton、车型 Definition 或其他技能 DA。

**本轮已生成并接好，团队正常使用无需运行生成工具。** 重开已编译的 Editor，直接用 Car04 释放原主技能测试。制作工具为 Editor 模块 `HALBuildBullChargeFXCommandlet`；维护时完整限定入口需把 `-run=HAL_Future_CreationEditor.HALBuildBullChargeFX` 整个参数用引号包起来，并启用 `-AllowCommandletRendering`。默认创建九份新资源；显式 `-ApplySkill -SkillAsset=... -PresentationAsset=...` 才会应用配方，且拒绝重置已启用的配方。不要在团队调参后重新生成覆盖。

## DA 调参

| 分组 | 首版默认值 | 调整建议 |
| --- | --- | --- |
| Rendering → Enabled | 旧资产默认关闭，接入本配方后开启 | 关闭只影响这一套表现 |
| Front Wind → Legacy Particle Wind | 旧版 Spawn Rate 14／Lifetime 0.32 s | 仅旧风带系统使用，新的持续风带不靠这两项调节 |
| Front Wind → Size Scale | 1；初次应用按模型范围校准 | 整体大小，与挂点 Scale 相乘 |
| Front Wind → Wind Length Scale | 第四轮 1，范围 0.1～4 | 只拉伸风带局部 X 长度；宽高、罩尖、罩子与扰动幅度不变 |
| Front Wind → Opacity | 第二轮 0.72 | 风带透明度，不宜遮住车头和球 |
| Front Wind → Full Speed | 第二轮 1600 cm/s | 达到满强度的向前速度；增大后相同车速的风更弱 |
| Front Wind → Flow Speed | 1.8 | 连续流纹的速度；设 0 时停止流纹动画 |
| Front Wind → Response Seconds | 0.1 s | 风力响应平滑时间，0 表示立即响应；驱动暂停仍立即隐藏 |
| Front Wind → Wind Turbulence | 第三轮 8 cm | 风带实际侧向／竖向摆动，0 关闭形变；根部保持在罩尖 |
| Front Wind → Contour → Wind Contour Color | 第三轮线性 RGB 0.055／0.075／0.085 | 第五轮起仅控制风带勾边；罩子使用独立 Toon Shading |
| Front Wind → Contour → Wind Contour Width | 第三轮 0.12，范围 0～0.3 | 轮廓宽度，0 关闭轮廓；明暗色阶仍保留 |
| Pressure Shell → Size | 默认 100／145／110 cm，应用时按模型校准 | X 前向深度、Y 半宽、Z 半高；不是完整直径 |
| Pressure Shell → Opacity | 0.42 | 单独调节模糊压力罩的可见度，0 隐藏罩子 |
| Pressure Shell → Feather | 0.28，范围 0.05～0.6 | 增大后开放边缘更柔和、更宽地渐隐 |
| Pressure Shell → Pressure Turbulence | 第三轮 2.5 cm | 罩子的轻微形变，尖端固定，不改变权威碰撞 |
| Rear Sparks → Spawn Rate | 第二轮每挂点 200 个／秒 | 两个喷口的总量约为单口两倍 |
| Rear Sparks → Lifetime | 第三轮 0.18 s，第二轮为 0.38 s | 收紧尾喷距离；速度、尺寸、数量和颜色保持 |
| Rear Sparks → Speed／Spread | 第二轮 2600 cm/s／保留 18° | 后喷速度与散开程度；采用有界速度盒，角度是散布控制量 |
| Rear Sparks → Size | 第二轮 16 × 50 cm | 卡通火星宽度与长度 |
| Contact → Count／Size | 48 个／14 × 34 cm | 撞车火花数量与大小 |
| Contact → Lifetime／Speed／Spread | 0.45 s／1600 cm/s／45° | 飞散时间、速度和散布控制量 |
| Contact → Flash Lifetime／Size | 0.12 s／150 cm | 局部闪光，保持短促 |
| Contact → Shock Lifetime／Size | 0.3 s／240 cm | 冲击环持续时间与基准直径 |
| Style → Spark Color／Highlight Color | 暖橙／浅暖白 | 尾喷、接触火花和闪光的颜色 |
| Style → Glow Strength | 1.1 | 控制暖色层亮度，不修改车头无明显色相的风带 |

速度仍统一使用 cm/s。调 DA 后重新发动技能，才会保存并同步新的这一轮表现快照；不要期待修改旧快照中的参数立即改变正在播放的技能。

## 挂点：当前无需新增骨骼

`Front Mount` 和 `Rear Mounts` 共用 `FHALVehicleFXMount`。`Part Id=Body` 指实际车辆表现 Mesh，不是物理碰撞根。其他 Part Id 必须先由已有 multipart 注册接口显式登记。

初次配方可以使用 **Socket Or Bone=None＋Offset**，工具从现有 Car04 Presentation Mesh 的范围和 Relative Transform 计算车头、左右车尾位置，并校准朝向；不会擅自改团队骨架。最多四个尾喷挂点。

团队希望精确放在模型喷口时，可以手动改用 Socket：

1. 在 Content Browser 打开 Car04 的 Skeletal Mesh／Skeleton，找到现有车身骨骼，例如 `Chassis_jnt`。
2. 在骨骼树对车身骨骼右键 **Add Socket**，按需创建 `FX_WindFront`、`FX_Exhaust_L`、`FX_Exhaust_R`。在预览中移动到对应位置。可以使用 Mesh Socket 让挂点只服务该模型。
3. 让挂点经过 Offset 修正后的 **局部 +X 朝向车头**。车尾系统按其 **-X** 喷射；导入骨架的轴向可能不同，不能假设骨骼原生 X 轴就是车头方向。
4. 保存 Skeleton／Mesh。打开技能 DA，把对应 Mount 的 `Socket Or Bone` 填为上面的名字，先将 Offset 位置清零，再微调 Offset 的位置、旋转与缩放。注意：原自动校准的 Offset 适用于 Mesh 原点，换成 Socket 后应重新校准。
5. 在车头／两个车尾分别释放技能确认。既有骨骼名称也可以直接填入，但 Socket 更方便只调整喷口而不改变骨骼姿态。

`Offset` 相对选定的 Socket／骨骼；None 时相对 Mesh 原点。父 Mesh 的导入缩放和旋转都会参与合成。`Fallback To Mesh Origin` 默认关闭：挂点拼错时跳过该层，开启后才允许明确回退到 Mesh 原点＋Offset。缺少 Part 不会回退到碰撞根。其他层与技能逻辑仍然工作。

代理不直接修改蓝图或骨架挂点；以上步骤由团队在 Editor 手动保存并验证。

## 联机、生命周期与制作约束

- 同步每次发动的表现配置快照、Activation、服务器起始时间、持续／加速时长和驱动许可。后加入或重新获得相关性的观察者按当前进度播放，不重开加速阶段。
- 驱动许可由服务器原有的运动／姿态规则发布，仅在状态改变时更新。逐帧计算风力和更新 Niagara 属于本地 C++ 表现；GA 不增加 Tick，不逐帧发送 RPC。
- **Niagara 冷加载不能用 `System->IsReadyToRun()` 阻止组件创建。** UE 的 `UNiagaraComponent::Activate/Tick` 会等待并推进编译／DDC／PSO 就绪。先创建组件、设置本次参数、注册并首次激活；后续让 Niagara 自身等待，不每帧重复 Reset。挂点缺失另行诊断。该边界已由本轮真实冷启动尾喷问题确认，未来挂接特效沿用此约束。
- 碰撞为不可靠的一次性表现事件，带 Activation、单调序号、时间、接触点、法线与强度。接收端去重、丢弃过期／错轮次事件；早于发动快照到达的事件暂存于小型有界队列。偶发丢包只允许漏一次表现，不能影响伤害。
- 车头系统必须保留局部空间；车尾系统必须保留世界空间。接触系统虽然使用局部模拟坐标，其组件只放在一次性的世界接触点，不能 Attach 到行驶中的车。
- 缺少资源、无效挂点、无渲染能力或预算关闭时跳过对应表现，不改变技能许可、CD、球权或 HP。总组件数量受项目 Particle Layers 预算及本配方挂点上限限制。
- 后续替换资源时保留 User 参数契约。风：`SpawnRate`、`Lifetime`、`ScaleMin/Max`、`WindMaterial`；尾喷：`SpawnRate`、`LifetimeMin/Max`、`VelocityMin/Max`、`SizeMin/Max`、`SparkColor`、`StyleParameters`；接触另有 `BurstCount`、`HighlightColor`、`FlashLifetime/Size`、`ShockLifetime/Scale`。完整名称都有 `User.` 前缀。
- 第二轮持续风使用 `User.WindScale`、`User.PressureScale`、`User.WindMaterial` 与 `User.PressureMaterial`。材质保留 `Envelope`、`FlowSpeed`，风带另有 `Opacity`，罩子另有 `PressureOpacity`／`Feather`。两个网格只在开始时各生成一次，内部 `User.StreamLifetime=60` 秒大于当前技能最长 30 秒，结束统一由组件清理。修改内部寿命时必须同步材质的归一化时间换算，正常调参直接使用 DA。
- 第三轮新增 `User.FlowOrigin` 模拟空间起点，跟随 `Pressure Size.X`，不要在 Niagara 内再硬编码风带前移量。材质新增 `MotionTime`／`UseRuntimeTime`／`Turbulence`／`ContourWidth`／`ContourColor`。游戏中由 C++ 显式传时钟，编辑器独立预览以粒子年龄回退；WPO 根部权重必须为零。风带尺度变化时，C++ 会补偿振幅单位，避免整体缩放意外放大扰动。
- 材质／粒子只控制外观。禁止通过特效的碰撞、位置、消失或回调改变玩法结果。保留柔和卡通的圆润轮廓、少量明暗档次和可读空隙，避免细碎写实火花与长时间满屏闪光。

诊断时在控制台输入 `hal.BullChargeFXLog 1`，观察 `BullChargeFX state`／`driving`／`impact`。挂点缺失或显式回退在诊断开启时仅每层每轮警告一次。结束后用 `hal.BullChargeFXLog 0` 关闭；此开关不修改技能规则。原球接触日志 `hal.BullChargeContactLog` 继续沿用。

维护工具的 `-Audit` 只读打印三份 Niagara 的冷加载状态与脚本状态；`Pending=1` 本身可以是正常的首次加载等待，最终必须检查运行中的组件能否完成激活。`-RefreshCompiledData` 显式刷新并保存这三份系统的编译数据，不重建形状、不改技能 DA。本轮冷启动问题通过运行时激活顺序修复，没有用强制重存资源掩盖它。

## 第三轮验证与接续（历史）

本轮 UE **5.8.3-58210709** 最终 **Editor R4／Game R1** 构建成功。最终 `Final_R2` 原生／Car03 回归／D3D12 **13/13**，零测试警告／失败／未运行／进行中。实际读回确认风带模拟空间起点位于罩子的前向深度，**7** 个网格根部均收拢到共同原点；六秒、**358** 帧持续保留两个粒子，无生灭断档。

本轮图像测试使用浅色不受光地板。深色勾边产生 **8080** 个明显变暗像素；粒子年龄固定、仅推进显式运行时钟时，扰动轮廓变化 **4978** 像素，同条件关闭形变后的纹理变化为 **3272**，验证存在额外几何运动。罩子开关产生 **7702** 个明显变化像素。实际 DA 的短尾喷能正常产生粒子，关闭新发射后推进 **0.267 s** 已无残留粒子；原世界空间轨迹和撞击表现回归继续通过。

最终 80 ms Host 延迟、低渲染 D3D12 双端各 **1/1**，零测试警告／失败，两个进程退出 0；每端确认两辆车共六个实际挂接组件，原驱动、暂停／恢复、结束、冷却与表现传输正常。网络撞击仍使用服务器表现传输探针，不代替远端真实物理撞车验收。

开工 **1299** 份既有资产／Config／uproject 中仅独立技能 DA 的授权表现字段变化，其余 **1298** 份 SHA256 一致；前两轮资源、Car03、骨架、车型与用户已有 GameMode BP 改动均保留。新增四份资源，继续复用旧罩子网格。挂点、原暖色、尾喷速度／数量／尺寸、撞击与所有玩法配置均受反射比较保护。见[第三轮验证摘要](Car04_BullCharge_ContourWind_Verification_20261114.json)。原始报告／图片在本机 `Saved/Car04FX/Contour_20261114/`，最终图片另存 `Render_Final`，未覆盖首轮失败证据。

中间结果保留：首次渲染为 12 项通过、1 项罩子对比失败，浅色地板上的罩子变化仅 878 像素。保持原验收阈值，通过加深灰色并让勾边透明度独立于主体柔边渐隐后重跑通过。Editor R3 的维护入口出现 `TObjectPtr` 容器遍历类型错误，修正后 Editor R4 通过。罩子的周期形变在资源生成前修正为闭合接缝一致。

**无需新增接线。** 在原 DA 调 `Wind Turbulence`／`Pressure Turbulence` 控制扰动，`Flow Speed` 控制动画速度，`Wind Contour Color`／`Wind Contour Width` 控制勾边。`Pressure Size.X` 调整罩子深度时，风带起点自动跟随；`Rear Lifetime=0.18 s`，尾喷速度保持保存的 **2600 cm/s**。当前仍需真人用实际相机／场地复核运动感、对比和遮挡，未执行新的四机、发布包或完整房间 BP 流程验收。离线组合图不是实时比赛截图。本轮未提交／Push。

## 第二轮验证与接续（历史）

UE **5.8.3-58210709** 的本轮 **Editor R1／Game R1** 均构建成功。最终原生／Car03 回归／D3D12 **13/13**，零测试警告／失败／未运行／进行中。新增检查连续推进六秒、共 **358** 帧有效样本，始终各保留一个风带网格和一个罩子网格，没有生灭空档；罩子在离线图上额外产生 **39063** 个明显变化像素。验证了材质流动、流速设零后静止、暂停时两层隐藏，30 与 120 fps 的风力响应一致；旧尾喷世界空间轨迹与撞击三层效果继续通过。

80 ms Host 延迟的低渲染 D3D12 双端各 **1/1**，零测试警告／失败，两进程退出 0。每端均确认两辆车共六个实际挂接组件，原驱动／速度上限、眩晕暂停恢复、结束、冷却及表现事件传输正常。传输仍使用第一轮服务器表现探针，不代替远端真实物理撞车验收。

本轮开工 **1294** 份既有资产／Config／uproject 中仅技能 DA 发生授权变化，其余 **1293** 份 SHA256 一致，包括首轮九份特效、Car03 和已有 GameMode BP 用户改动。新增五份压力风资源；反射检查确认仅本轮风／罩子／尾喷字段变化，挂点、颜色、撞击与全部玩法配置保持。可同步结果见[第二轮验证摘要](Car04_BullCharge_PressureWind_Verification_20261114.json)，原始报告与图片位于本机 `Saved/Car04FX/Polish_20261114/`。本轮没有中间构建或测试失败。

**当前保存配方已更新，无需新增接线。** 风带透明度 0.72、满风速度 1600 cm/s、流速 1.8、响应时间 0.1 秒；罩子半轴约 `(100,144.67,112.47)` cm、透明度 0.42、柔边 0.28；尾喷每口 200 个／秒、2600 cm/s、16 × 50 cm、寿命 0.38 秒。优先调 DA 的 `Wind Opacity`／`Flow Speed`、`Pressure Opacity`／`Size`／`Feather` 和 `Rear Spawn Rate`／`Rear Speed`／`Rear Size`。新的持续风不使用 `Legacy Particle Wind` 中的发射率和寿命。

用户已确认首轮基本逻辑正确；本轮仍需真人在实际相机／地图中验收强度、流畅度和遮挡。离线模型组合图按满强度展示，不是比赛实时截图。四机、发布包与完整房间 BP 流程未在本轮复测。代码、五份新资产、技能 DA、白名单和本轮文档／摘要尚未提交或 Push。

## 首轮验证（历史）

代码、九份新资源与技能 DA 接入已完成。UE **5.8.3-58210709** 最终 **Editor R9／Game R4** 构建成功。构建保留既有引擎弃用／工具链版本提示，不把它们混同于测试警告。

- 最终 `Final_R2` **12/12**，零测试警告／失败／未运行／进行中。包括 Car04 原规则、技能许可／球发射／每目标 CD 与 CombatResolver 接触边界，挂点解析／明确回退／Mesh 替换，阶段与速度盒，早到事件暂存／去重／过期／退场后拒绝，以及 Car03 回归。
- D3D12 专项检查实际风带可见、暂停遮罩、三 Emitter 撞车效果、早期闪光与后续冲击环、世界空间尾喷。读取 **20** 个实际尾喷粒子跨车辆突然平移 450 cm＋旋转 90°的数据，确认旧粒子没有跟着车跳位。另保存 Car04 模型与风／尾喷的离线组合图。图片位于本机 `Saved/Car04FX/20261114/Render/`，它不是完整比赛画面。
- 最终 `Network_Final` 使用 `MVPB_NetTest`、原生 Match GameMode、瞬态配方、D3D12 低渲染和 Host **80 ms** 延迟。Host／Client 各 **1/1**，零测试警告／失败／未运行／进行中，两个进程均退出 0。两端各确认两辆车共 **6** 个实际挂接组件完成创建；验证驱动、上限、眩晕暂停／恢复、结束、冷却、表现身份与一次性事件传输。一次性事件采用服务器表现探针，**不是远端真实物理撞车验收**；真实接触提交边界由原生解析器夹具注入碰撞前样本验证。
- 开工 **1285** 份既有资产／Config／uproject 中，仅授权的 Car04 技能 DA 发生变化；反射逐属性确认只改 `FX`。其余 **1284** 份 SHA256 一致，包括 Car03 资源、车型／骨架和开工已有的 GameMode BP 用户改动。另新增九份 Car04 特效资产，没有保存任何 BP 或地图。

中间失败完整保留：首轮元数据单位、测试头文件相对路径和一次未导出的引擎辅助函数链接问题均已修正；早期 Commandlet 的模块名／PowerShell 参数引用已修正。无有效出生点的 Entry 地图被拒绝加入，改用已有 NetTest；与 Game 构建争用 UBT 锁的启动超时不计验收。早期渲染测试主动编译资源，没暴露冷加载尾喷未激活，新增实际组件断言后正确失败，最终通过组件首次激活流程修复并重跑。没有用强制重存资源掩盖这个问题。

可同步结果与文件哈希见[本轮验证摘要](Car04_BullCharge_FX_Verification_20261114.json)。原始报告／日志位于本机 `Saved/Car04FX/20261114/`；它们不作为 Git 依赖。代码、资源、本页与摘要需随正常 Git 提交同步，本轮未提交／Push。

**下一步是团队真人验收。** 重开 Editor，用 Car04 释放原主技能即可，初版无需补 Socket。覆盖 Host／Client、起步／满速／停车、眩晕暂停后续行、腾空／翻倒／自救、真实撞车及重复 CD、撞墙／撞球区别、淘汰／换车／离场清理，以及三层特效在真实俯视相机与地图背景下的遮挡和手感。校园四机、发布包与完整房间 BP 流程未在本轮复测；精确骨骼 Socket 配置由团队按上文操作后验收。
