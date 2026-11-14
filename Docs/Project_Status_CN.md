# HAL Future Creation：项目状态与接续入口

**2026-11-14 Car04 第五轮完成，待真人画风复测：破风罩整体柔和三阶明暗。** 取消罩子外围深色硬边与边缘加深，改为类似 TireSkid 的轻微暗／中／亮曲面分色。技能 DA 的 `Pressure Shell → Toon Shading` 增加三色、暗／亮阈值与过渡宽度；气流轮廓独立，罩子外缘透明渐隐，保留扰动和加速渐进出现。已用原生 API 更新现有罩子母材质，无需改引用或 BP 接线。

本轮 UE5.8.3-58210709 **Editor R2／Game R2** 完整编译通过；原生／Car03 回归／实际 D3D12 最终 **14/14**，80 ms Host 延迟双端各 **1/1**，零测试警告／失败。深色背景上三色各改变 6856／19573／24298 像素，过渡宽度改变 3136 像素，气流勾边对罩面的影响为 0；默认轻柔色阶在浅色地板上对比很轻，已保留该预览。首轮 13/14 后仅调整验收背景以分离罩面覆盖／过渡，保留材质和原数字阈值，最终复验通过。只保存 `M_BullCharge_ContourShell`，其余 **1302/1303** 份既有资产／Config／uproject 哈希不变，保留团队技能 DA／GameMode BP、全部既有 Niagara 和先前未提交改动。见[配置与当前入口](Presentation/Car04_BullCharge_FX_CN_v0.1.md#第五轮破风罩整体柔和三阶明暗当前)及[第五轮验证摘要](Presentation/Car04_BullCharge_SoftShell_Verification_20261114.json)。下一步：重开 Editor，在原技能 DA 调三色／过渡，真人复核实际相机与场地。未提交／Push；四机、Cook／发布包、完整房间 BP 与真实远端物理撞击未在本轮验收。

**2026-11-14 Car04 第四轮完成，待真人观感复测：独立气流长度＋跟随加速时间渐进显现。** 新增技能 DA 的 `Presentation → FX → Front Wind → Wind Length Scale`，默认 1、范围 0.1～4，只额外拉伸风带 X，保留宽高、罩尖连接、扰动幅度和罩子尺寸。持续风／罩子透明度与尾喷数量复用 `Charge → Motion → Ramp Seconds` 从零渐进增强；去除原先 65%／40% 的入场阶段下限。实际车速响应平滑与加速时钟分开处理，保留驱动暂停立即隐藏、后到观察者接续原阶段及原技能规则。

本轮 UE5.8.3-58210709 **Editor R4／Game R1** 完整编译通过；原生／Car03 回归／实际 D3D12 **14/14**，80 ms Host 延迟的双端各 **1/1**，零测试警告／失败。实际粒子覆盖 0.5／1／2／4 长度倍率；满路速发动时，0／0.5／1／2 秒的风／罩子权重为 0／0.25／0.5／1，尾喷量同阶段渐增，实际渲染亮度逐步增加。网络覆盖非默认 1.6 长度复制与挂接绑定、渐进阶段及暂停恢复。开工 **1303** 份既有资产／Config／uproject 全部 SHA256 不变，未新增或重存特效资产、BP、地图或技能 DA，保留全部先前未提交改动与团队调参。首次验证 helper 的 const 编译错误已修正；完整过程见[第四轮验证摘要](Presentation/Car04_BullCharge_LengthRamp_Verification_20261114.json)。下一步：重开 Editor，在原 DA 调独立长度，从静止／满速发动真人复核渐进观感。见[配置与验收入口](Presentation/Car04_BullCharge_FX_CN_v0.1.md#第四轮独立长度与加速渐进显现)。未提交／Push；未进行本轮四机、Cook／发布包、完整房间 BP 或真实远端车辆物理撞击验收。

**2026-11-14 Car04 表现第三轮已完成：罩尖连接、几何扰动、深灰色阶勾边、短尾喷，待真人复测。** 当前配方 `NS_BullCharge_ContourWind`：七个风带根部收拢并随罩子深度自动定位到罩尖，根部形变为零；显式运行时钟驱动风带 **8 cm**／罩子 **2.5 cm** 的轻微几何摆动。风带和罩子加入暗／中／亮色阶及可调深灰勾边。尾喷速度保持 **2600 cm/s**，仅将寿命 **0.38 → 0.18 s** 收紧距离。参数仍统一在原技能 DA，无需改 BP 接线，玩法、挂点、暖色、尾喷数量／尺寸和撞击配置保持。

本轮 UE **5.8.3-58210709** 最终 **Editor R4／Game R1** 构建通过；最终原生＋Car03 回归＋D3D12 **13/13**，零测试警告／失败。确认七个共同根部、358 帧无生灭空档；浅色地板上勾边有 **8080** 个明显变暗像素，显式时钟驱动的轮廓变化 **4978** 对比关闭形变后的 **3272**，罩子开关变化 **7702**。短尾喷停发后 0.267 秒无残粒子。80 ms 延迟的实际渲染双端各 **1/1**，退出 0，阶段／挂接／暂停恢复与传输回归通过。首次浅色背景对比检查失败后实际加强材质，保留阈值复测通过；详情与中间编译结果见[第三轮验证摘要](Presentation/Car04_BullCharge_ContourWind_Verification_20261114.json)。

开工 **1299** 份既有资产／Config／uproject 仅技能 DA 授权表现字段变化，其余 **1298** 份哈希一致；前两轮、Car03 和已有 GameMode BP 用户改动均保留。新增四份独立资源，不覆盖旧配方。下一步：重开 Editor 释放 Car04 原技能，真人复核起点连贯、扰动感、浅色场地勾边和尾喷距离。见[当前配置及第三轮实测](Presentation/Car04_BullCharge_FX_CN_v0.1.md#第三轮验证与接续当前)。网络撞击仍为表现传输探针，未冒充真实远端撞车、四机、发布包或完整房间 BP 验收；本轮未提交／Push。

**2026-11-14 Car04 表现第二轮：连续强气流、软边破风罩与强化尾喷已完成，待真人观感复测。** 用户已确认首轮基本逻辑正确，要求解决气流弱、断续，增加漫画突进的车头压力罩并强化尾喷。当前配方改用 `NS_BullCharge_PressureWind`：七条持续曲面风带＋一层灰白半透明软边罩子，材质连续流动，风力作帧率无关平滑，暂停仍立即隐藏。尾喷每口 **200 个／秒、2600 cm/s、16 × 50 cm**。新增流速、响应时间、罩子尺寸／透明度／柔边等 DA 参数，均在原独立技能 DA；已接入，无需新增 BP 接线。挂点、颜色、撞击及玩法数值保持。

本轮 UE **5.8.3-58210709** **Editor R1／Game R1** 构建通过；原生＋Car03 回归＋D3D12 **13/13**、零测试警告／失败。六秒内 **358** 帧持续保持两个网格粒子，无生灭空档，罩子额外覆盖 39063 个变化像素；30／120 fps 平滑、流速调节、暂停隐藏、尾喷轨迹与撞击回归通过。80 ms 延迟的实际渲染双端各 **1/1**，两端各确认六个挂接组件，退出 0。网络撞击仍为表现传输探针，不冒充真实远端撞车或四机／发布包／完整房间流程验收。

本轮 **1294** 份既有资产／Config／uproject 仅技能 DA 的授权表现字段变化，其余 **1293** 份哈希一致；首轮九份资源、Car03 和已有 GameMode BP 改动保留，另新增五份压力风资源。下一步：重开 Editor，用 Car04 原主技能复测流畅度、破风罩与尾喷力度／遮挡，再在 DA 调参。见[二轮配方、参数与复测](Presentation/Car04_BullCharge_FX_CN_v0.1.md#第二轮验证与接续历史)及[验证摘要](Presentation/Car04_BullCharge_PressureWind_Verification_20261114.json)。本轮未提交／Push，以下保留首轮及其他团队的历史记录。

**2026-11-14 Car04 蛮牛冲撞生产特效初版已完成，待团队真人验收。** 从 `main/50f3237` 接续，UE **5.8.3-58210709**。已制作无明显色相的车头曲面风带、世界空间双尾喷卡通火星，以及有效撞车时的火花＋局部闪光＋扩张冲击环；新增可复用的表现 Mesh／Socket／骨骼挂点配置与明确回退，资源引用和可调项统一在 `DA_Ability_Car04_BullCharge → Presentation → FX`，已接入，初版无需改 BP 或新增骨骼。原驾驶、球发射、伤害、冲击与每目标 CD 规则沿用；持续表现快照与驱动许可复制，接触表现由服务器接受入口发出。Car03 保持用户接受的初版。

最终 **Editor R9／Game R4** 构建成功；原生＋Car03 回归＋D3D12 **12/12**、零测试警告／失败。最终 80 ms 延迟的实际渲染双端各 **1/1**，两进程退出 0，两端分别确认两辆车合计六个挂接组件实际创建，覆盖眩晕暂停／恢复、结束、冷却和一次性事件传输。冷启动尾喷的就绪检查曾阻止 Niagara 自己完成编译等待，已改为组件首次激活后由 UE 管等待，并用实际挂接断言复测通过；旧轮次事件不会在退场后重新点亮特效。原始证据在本机 `Saved/Car04FX/20261114`，可同步[验证摘要](Presentation/Car04_BullCharge_FX_Verification_20261114.json)。网络撞击事件使用表现传输探针，真实接受入口由原生碰撞样本夹具验证；不冒充远端真实物理撞车／四机／发布包／完整房间 BP 验收。

开工 **1285** 份既有资产／Config／uproject 中只修改授权的 Car04 技能 DA，逐反射属性确认仅 `FX` 变化，其余 **1284** 份哈希一致；Car03、骨架、车型和已有 GameMode BP 用户改动保持。新增九份 Car04 特效资产。**下一步：重开 Editor，选 Car04 释放原主技能，真人验收气流／尾喷方向、强度、真实撞车及暂停清理，再调 DA；需要精确喷口时由团队手动加 Socket。** 配置与后续制作约束见[Car04 特效配置／挂点文档](Presentation/Car04_BullCharge_FX_CN_v0.1.md)。本轮未提交／Push，新增代码、资产、文档与白名单需随正常 Git 同步。以下保留讨论与其他团队阶段的历史记录。

**2026-11-14 Car03 特效由用户接受为初版，后续优化暂缓；Car04 进入生产特效方案讨论。** 本轮从 `main/50f3237` 核对现有冲锋技能与表现接口，用户提出车头无明显色相的破风气流、车辆接触时更强的卡通火花／闪光／冲击、车尾推进火星三层需求。建议沿用独立技能 DA 配置，并补轻量 Socket／骨骼挂接及 Car04 表现同步入口；具体建议见[Car04 特效讨论草案](Combat/Car04_Bull_Charge_CN_v0.1.md#生产特效讨论草案2026-11-14)。当前仅方案讨论，没有开始 Car04 特效施工，没有修改代码／资产或重跑构建／测试；本轮不将 Car03 的用户初版接受扩大为四机／发布包验收通过。开工已有 GameMode BP 改动，原样保留。以下为各环节历史记录。

**2026-11-14 被丢弃的 Car03 气流 Git 白名单与进度记录已恢复。** 当前 `main/10d7723`，保留以下闪电新星等团队更新，仅补回本页两轮气流入口及 `.gitignore` 中两份验证摘要／三张实图的白名单。只读核对：最新气流源码、ToonWind资源及实图与调参验证记录一致；技能DA此后已有修改，原样保留，下面保存值属于当时验证快照，不冒充当前配置。本次只恢复文档／忽略规则，没有重新构建或执行测试，没有修改代码／资产／其他配置，未提交／Push。

**2026-11-14 Car03 气流调参范围与实际倾角已修复，程序／资源／构建／专项完成，待团队目测。** 速度UI及校验范围同步扩大至0～3600°/s，倾角至0～80°，默认值不变。旧算法将随机起始方位与倾角耦合在轴角向量内，请求30°时实际只有约0.555～24.282°；上轮只验不越上限，未验证达到配置角度。本轮出生姿态改为独立方位四元数×配置倾角四元数，持续阶段沿世界Z同向推进。30°现在实际约30°；当时保存的720／30°／弧段6／寿命0.8／宽度0.8／火花28原样保留。见[当前配置、原因与0°／30°实图](Presentation/Car03_BattleTop_Cartoon_FX_CN_v0.1.md#倾角真实值与调参范围2026-11-14-修正)。

该轮UE5.8.3 Editor R2／Game R1构建成功；原生＋D3D12 **16/16**，0测试警告／失败。0／12／30／60／80°共3000次姿态、2935次推进均达到配置角度并保持竖直轴，1440及3600°/s推进正确；高参数隔离双端 **各1/1** 验证同向1440、45°参数、显示与清理。测试使用原生Match GameMode及低渲染设置，不覆盖房间BP完整流程／四机／发布包。1282份开工资产／Config／uproject仅授权ToonWind改变，其余1281份哈希一致。原始证据在 `Saved/BattleTop/WindTilt_20261114`，见[调参验证摘要](Presentation/Car03_BattleTop_Wind_Tuning_Verification_20261114.json)。这是此前验证结果，本次恢复操作未重跑。

**2026-11-14 Car03 气流轴向约束的前一轮记录。** 从 `main/2a5bfac` 接续，UE5.8.3-58210709。修复初始Random全方向翻转及绕粒子局部轴推进，改为沿技能主自转方向绕世界竖直轴，底盘转向／翻滚不再额外旋转气流；历史负速度按大小兼容读取。当时采用12°默认、0～30°上限和旋转向量算法，该倾角实现及范围现已被上面的独立实际倾角方案替代。见[轴向修复历史](Presentation/Car03_BattleTop_Cartoon_FX_CN_v0.1.md#气流方向与总倾角2026-11-14-修正)。该轮Editor R7／Game R4构建、16项专项及隔离双端各1/1通过；1200次姿态／1179次推进观察到最大9.078°和最小顺时针步进4.000°。早期房间BP直进超时未计作通过，最终双端使用原生Match GameMode及低渲染设置、Host出站80ms。原始证据在 `Saved/BattleTop/WindAxis_20261114`，见[轴向验证摘要](Presentation/Car03_BattleTop_Wind_Axes_Verification_20261114.json)。本段保留历史时点，不代表当前倾角定义或本次复测。

**2026-11-14 闪电新星第十二轮：收窄地面环，Loop 的边界对比翻倍拉回来，12.17 节的「已知未解」结案。** 两个改动：(1) 新增 **`MI_FX_NovaRing`** 并把 `GroundRing` 的渲染器改指向它——这层原本**直接用母材质** `M_FX_NovaRing`，是整个系统里唯一没有材质实例的一层（其余都有 `MI_FX_*`），调参不再需要动母材质预设；(2) **`RingWidth` 0.16 → 0.06**。`M_FX_NovaRing` 的环是 UV 平面上的径向带 `band = 1 - saturate(abs(r-Rad)/Wid)` 再平方，`Rad=0.8`／`Wid=0.16` 等于环跨 **r=0.64…0.96**（佔半径 ±20%），所以在 Loop 被电弧和雾淹掉时完全凸不出来；**`band` 峰值恆为 1.0、这材质没有亮度旋钮**，唯一能拉对比的手段就是收窄。

| 阶段 | `RingWidth` | 峰值 | 外侧中位 | **阶梯** |
|---|---|---|---|---|
| Loop | 0.16 | 241.1 | 178.0 | **63.1** |
| Loop | **0.06** | 241.5 | 111.6 | **129.9** |
| Loop | 0.035 | 241.5 | 107.0 | 134.5 |
| Begin | 0.16 | 207.8 | 54.9 | **152.9** |
| Begin | **0.06** | 209.6 | 53.3 | **156.3** |

**Loop 的阶梯翻了一倍，Begin 完全没有被牺牲**（峰值位置 x=111/448 不变）。**停在 0.06 而不是更窄是量出来的决定**：0.035 只多拿到 4.6，因为那时外侧剩下的 107 已经不是环的宽度而是**电弧餵出来的 bloom**，收窄对它无效；同时环的半高宽由 53px 掉到 22px，再窄就要开始担心 12.15 节记的 TAA 门槛。**0.06 拿到九成好处又留着安全馀裕。**

**三阶段状态**：Begin 地面环清楚、−60° 下是躺在地面的椭圆（车子真正要闪的足迹）；Loop 地面环在球体下缘与左右两侧明显分离出来；Over 0.55 前后只差 3,109 个像素（火花随机抖动，不是环），**仍然没有边界线，这是刻意的**（场正在关闭，见 12.17 节）。

**已验证**：上述量测；`validate_system` `valid: true`，警告仍是既有那七笔 `Particles.MeshScale`（12.12 节，不是误报）；两个资产已存档；`User.Radius` 400 / Phase 0 / PhaseElapsed 0 已还原，`list_dirty_packages` 为 0。**全程未提交。** **未验证**：PIE 实际画面——**地面环是贴在地板上的，它的对比最容易被场地底色吃掉，黑底预览裡的数字偏乐观**；本轮未重拍四张正俯视正式截图（同 12.16 节理由）。

**现行值（12.16–12.18 三轮净结果）**：`MI_FX_NovaShell.RingBoost` = **0**（外圈白线关闭）、`VeilGroundZ`/`VeilFadeZ` = **−0.16**/**+0.19**、`FillGlow` = 0.15（未改，12.16 节留的待决项）；`MI_FX_NovaRing`（新）`RingWidth` = **0.06**、`RingRadius` = 0.8（未改）。

**下一步入口**：进 PIE 用 −60° 看真场地底色下的地面环对比（黑底数字偏乐观），并决定 12.16 节留下的 `FillGlow` 要不要调高。详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12.18 节。

**2026-11-14 闪电新星第十一轮：拿掉壳层的外圈白线（`RingBoost` 1.5 → 0）。Begin 没问题，但 Loop 的边界对比掉到三分之一，这一项未解。** 团队要求「不要显示最外围那个白色圆形，也就是遮罩的轮廓」。那条线是壳层材质的 `edge` 项（`smoothstep(RingS, RingS+rbw, r) * RingB`），12.15 节已用两个测试确认过归属，所以只要 `RingBoost = 0`，不必改 HLSL。

> **这和 12.15 节独立评审的建议方向一致**：球壳剪影画在**球心**高度、`GroundRing` 画在**地面**高度，−60° 下两者会分离成两个半径，评审说「让 `GroundRing` 当唯一边界，不要让球壳剪影跟地面圈抢『半径在哪』」。团队这个要求刚好把那件事做掉了。

**用 −60° 真实俯角逐阶段量边界**（沿通过环最宽处的水平扫描线，量峰值与刚好在外侧的中位数，差值即边界阶梯）：

| 阶段 | 峰值位置 | 峰值 | 外侧中位 | **阶梯** |
|---|---|---|---|---|
| Loop（改前，有 `edge`） | x=108/449 | 243.5 | 45.8 | **198.2** |
| Loop（改后） | x=141/412 | 234.7 | 178.0 | **63.1** |
| Begin（改后） | x=110/449 | 206.8 | 50.8 | **157.2** |

**Begin 没问题**：`GroundRing` 完整接手，阶梯 157.2，位置与改前的 `edge` 几乎重合，而且在 −60° 下它读起来是**躺在地面的椭圆**——那才是车子真正要闪的足迹。**Loop 是问题所在**：边界阶梯由 198.2 掉到 **63.1**（剩约三分之一），峰值位置还向内移到 x=141/412，那已经不是边界而是壳层本体与电弧。原因是 Loop 的 `rim` 闸门全开、细丝与缠绕弧把能量堆在外圈，内部中位衝到 202、外侧中位 178，`GroundRing` 淹在里面凸不出来。**Loop 正是碰到会中的阶段**，AGENTS.md 第 12 节要求危险区域必须能被快速识别——**此项尚未解决，本轮只记录不自行处理**（团队要的是拿掉那条线，不是换一条）。

**可用的修法（未做，待决定）**：(1) **收窄 `GroundRing` 的 `RingWidth`**——`M_FX_NovaRing` 的 band 是 `1 - saturate(abs(r-Rad)/Wid)` 再平方、峰值恆为 1.0、**没有亮度旋钮**，只能靠收窄让它更锐利；注意 `GroundRing` 直接用母材质 `M_FX_NovaRing`、**没有 MI**，要嘛建实例要嘛改母材质预设。(2) **给 `M_FX_NovaRing` 加一个亮度参数**，比收窄直接，但要动母材质。(3) 把 `RingBoost` 设成 0.3–0.5 而非 0——与本次要求相反，列出只为完整。

**Over 段**：`GroundRing` 在 Over 被 spawn 阶段的 Phase 阵列关掉（12.15 节），`edge` 现在也没了，所以 Over 0.55 完全没有边界线，只剩塌缩中的电弧球。**这是合理的**——场已在关闭，不再需要告诉玩家闪哪裡，也符合 12.1 节「全部向中心塌缩归零」。

**已验证**：上述三阶段的 −60° 撷取与量测（`capture_scene_preview`、`rotation: [-60,0,0]`、`fov 60`、560×420、`User.Radius` 110）。`MI_FX_NovaShell` 已存档；`User.Radius` 400 / Phase 0 / PhaseElapsed 0 已还原，`list_dirty_packages` 为 0。**全程未提交。** **未验证**：PIE 实际画面；本轮未重拍四张正俯视正式截图（同 12.16 节理由）。

**下一步入口**：决定怎么把 Loop 的边界对比拉回来（收窄 `RingWidth` 或给 `M_FX_NovaRing` 加亮度参数），以及 12.16 节留下的 `FillGlow` 要不要调高。详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12.17 节。

**2026-11-14 闪电新星第十轮：雾感遮罩改为随高度淡出，并第一次用真实的 −60° 相机俯角验证。** 团队回馈「外围半透明遮罩太显眼，希望越往穹顶顶点越透明，只有大概由地面往上 30% 是可视的白色雾感遮罩」。`M_FX_NovaShell` 的 Custom 节点新增两个输入与两个纯量参数：`VeilGroundZ`（预设 **−0.16**，此局部高度以下雾是满值）与 `VeilFadeZ`（预设 **+0.19**，此高度以上雾完全消失），`rim` 项乘上 `1.0 - smoothstep(VeilLo, VeilHi, d.z)`。**座标换算**：`d.z` 是 `normalize(LPos).z`、球底 −1 顶点 +1，**不是世界高度**；`z_local = (h_world − 球心高度) / Radius`，球心在车身高度（约 65cm）、Radius 400 时地面是 −0.16、穹顶可见高度 30% 是 +0.19——**球心高度或 Radius 改了这两个值要重算**。

> **这是整个壳层材质里唯一以世界上方向为基准的项。** 其余五项（`rim` 的 `r`、`edge`、`core`、`fil`、`gc`）全部活在视线基底上——这正是 12.15 节必改 1 讲的「这颗球从任何角度看都是同一个正圆、没有线索把它绑在地面」。这个高度淡出让壳层**第一次有了地面锚点**，附带解掉一部分那个问题。

**验证方式本身也换了**：`editor_query / capture_scene_preview` 可以指定 `camera: {location, rotation, fov}`，所以这轮第一次用游戏真正的俯角拍（`rotation: [-60,0,0]`、`fov 60`、Loop、`User.Radius` 110）。`preview_system` 只有 front／top／three_quarter／side 四个预设，拍不出 −60°。**机制正确**：开关淡出相差 64,812 个像素、单点最大差 462；逐列平均亮度在接近顶点处 59.0→28.9（**−30.1**），接近地面处 90.2→92.5（**+2.3**）——顶点掉最多、地面不动，方向完全正确。把 `FillGlow` 暂拉到 0.6 再拍一张诊断图，梯度形状一眼可见。**形状对，剩下的只是强度问题。**

**但使用者看到的「外围遮罩」大部分不是这一层。** 把 `FillGlow` 设 0 重拍，那层外围雾几乎没变。拆出来是两个来源：(1) **`GroundRing`**（12.15 节量到佔全图 46% 亮度），它的渲染器用 `M_FX_NovaRing`，是一个**躺在地面的 UV 平面环**——**它没有「穹顶顶点」可言，「越往上越透明」对它没有意义**，要收只能直接降亮度或宽度；(2) **Bloom**，近白细丝与 242 亮度的边界环喂出来的后处理光晕。所以 `FillGlow` 停在 0.15（12.9 节定的值，本轮没改）时，这个改动在正式设定下**看得出来但幅度不大**。

**两个方向待团队决定，本轮未做**：(a) **把 `FillGlow` 调高**（例如 0.4–0.6）——雾现在被限制在底部 30%、上半部不会再被填满，12.9 节「盘面会洗白开进去的车」的理由**部分**失效，但底部 30% 正好是车子所在的高度带，这个取捨必须在 PIE 看真场地底色才能判；(b) **降 `GroundRing` 的亮度或宽度**——它是外围雾的真正主力，但 12.15 节刚确认它不是那条硬白边界（那是壳层的 `edge`）。

**已验证**：27 个输入脚位逐一确认 `connected: true`（`inputs` 整组取代会全部断线，见 12.13 节）、`recompile_material` 无错误、`validate_material` 只剩既有 warning、两个资产都已存档；上述 −60° 量测。`FillGlow` 已还原 0.15、`User.Radius` 400 / Phase 0 / PhaseElapsed 0 已还原，`list_dirty_packages` 为 0。**全程未提交。**

**未验证**：PIE 实际画面与真实场地底色下的对比；本轮**未重拍四张正俯视的正式截图**——那四张是正俯视验收用，这个改动在正俯视下会变成「中间透明、外圈一圈雾」，不是这个效果该被判读的角度（见 12.15 节）。

**下一步入口**：决定 `FillGlow` 要不要调高、`GroundRing` 要不要收，两者都建议在 PIE 用 −60° 看真场地底色再定。详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12.16 节。

**2026-11-14 闪电新星第九轮：没有改造型，查了两个方法学问题——截图角度与 `GroundRing`。迴圈未停止。** 独立评审（刻意不给前次结论、用同一组画面重量一次）提出两项必改，两项都不是「数字调错」，而是**前面六轮的验收方法本身有盲点**。

**必改 1：四张截图都是正俯视（−90°），而游戏相机是 −60°。** 前提已从工程核对：`VehicleDefinition.h` 的 `FVehicleCameraConfig::BoomTransform` 预设 `FRotator(-60,0,0)`、`TestVehiclePawn.cpp:136` 也硬写 −60（`HALRoundMatchTypes.h` 的 `SpectatorPitch=-65` 是**观战**相机，别混用）；`Docs/Combat/Electric_Sphere_CN_v0.1.md` 写明 Actor 原点是**球心**、且「测试时将球心放到车身高度」；`M_FX_NovaShell` 的 Custom 节点第 2 行是 `V = normalize(Parameters.CameraVector)`，**rim／edge／core／fil／gc 五项全部活在视线基底上**，所以这颗球从任何角度看都是同一个正圆，**没有任何透视线索把它绑在地面上**。亮边界圆画在**球心**的萤幕投影位置，而 `camera_angle=top` 正是球心与其地面投影重合的**唯一**角度——12.9～12.14 六轮所有「边界没变动／边界够硬」的结论，都是在一个**系统性隐藏此问题的角度**下得到的。

> **我不同意评审给的修法方向。** 它建议把 `RingBoost` 降到 0.6–0.8、让 `GroundRing` 当唯一边界。但**车身中心也在同一个高度**（球心就是放在车身高度），两者投影偏移相同，所以「环画在球心」对判断「车能不能进去」其实是**对的**。真正的风险是球壳剪影（球心高度）与 `GroundRing`（地面高度）在 −60° 下**分离成两个不同半径**，玩家不知道该信哪一个。**这必须在 PIE 用真实相机角度看，不能在黑底预览拍板。**

**必改 2：`GroundRing` 查清楚了，12.12 留下的印象是错的。** 两个直接测试：(A) `RingBoost=0` 重拍 Begin——只有 r=123 的尖峰塌掉（242.7→211.1），**r=126–150 整段完全没变**，那段在球壳剪影之外，既不是 `rim` 也不是亮环的 bloom。(B) 直接停用 `GroundRing` 重拍——r=130 由 174.5 掉到 **10.0**、r=136 由 131.5 掉到 **1.9**、r=144 由 63.0 掉到 **0.9**，**全图总亮度 8.357M → 4.475M，少掉 46%**；而 r=123 的尖峰只由 242.7 动到 240.4。

**结论两条都要记住**：(1) 那条硬白尖峰**确实**是壳层的 `edge`，12.12 这点是对的；(2) **但 `GroundRing` 绝不是「白做」的一层——它贡献全图 46% 的亮度，r=126–150 整圈外晕几乎全来自它。** 12.12／12.14 读起来会让人以为这层没有画面，**那个印象是错的**，下一个接手的人若照那印象把它停用，会直接少掉一半的光。**它在 Over 会消失是因为阶段闸门不在曲线上而在 spawn**：`emitter_update / SpawnRate` 的 `Spawn Probability` 挂 `SelectFloatFromArray`，`Direct Array Index` 绑 **`User.Phase`**。所以 12.12 说它的 `ScaleMeshSize` 曲线是死的**仍然成立**，只是那不等于这层没有阶段行为——**同一个系统裡有两套完全不同的阶段机制，这是目前最容易让人误判的地方。**

**已验证**：上述两组量测；相机俯角与球心高度已从 C++ 与 `Docs/Combat` 核对。两个探针截图已删除、**四张正式截图没有被覆盖**；`RingBoost` 已还原 1.5、`GroundRing` 已重新启用、`User.Radius` 400 / Phase 0 / PhaseElapsed 0 已还原，存档后 `list_dirty_packages` 为 0。**全程未提交。**

**本轮判定不处理**：`Sparks` 飞出半径——独立评审自己量完认为可结案（Loop 在 r>1.14×R 只有 5 个离散亮点、最远 1.41×R，是几颗火星不是误导边界的光带），此项已连续列出三轮，就此标记「已量测、不处理」。另外边界光晕外溢到 1.22×R（误差方向保守，不急）、细丝 TAA 门槛（**球在萤幕上小于约 100px 直径**就照 12.9 节降 `FilamentCount` 22→14，不要加粗）。

**下一步入口**：造型本身两轮量测都达标，但**停止条件尚未满足**（第九轮有必改项）。剩下的必改只有 PIE 验得掉——请在 `Content/Maps/Test` 用 −60° 实拍 Begin 与 Loop，量亮环中心与地面危险圈是否重合，并一併看 additive 蓝叠在实际场地底色上的对比、车开进 Loop 的遮挡、球的萤幕直径。详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12.15 节。

**2026-11-14 闪电新星第八轮：核心的饱和白盘与剪影硬圆已修，三项门槛全部达标，硬边界环没动。** 评审只给一项必改，但底下两个症状同一个病根——第六轮为了放大核心把 `CoreFalloff` 设成 **1.0**，`body = pow(1-r, fo)` 在 `fo=1` 时是线性的，乘上 `CoreGlow 2.4` 后蓝通道在核心网格七成半径以内全部 clip：核心不是柔光球，是**硬边的饱和白盘**。为什么是必改不是美术洁癖：12.9 节当初把 `FillGlow` 0.28 砍到 0.15 的理由是「盘面 150–170 会洗白开进去的车」，改前盘心是 **209–255**，换算 `User.Radius = 400` 就是**一块直径约 350cm 的近饱和白盘压在区域正中心**；而 Over 0.55 那道硬圆落在 0.45×R，是危险区里的**第二条同心硬边**，正是 12.10 节花一整轮修掉的失败模式。

**两个改动**：**(1)** `MI_FX_NovaCore`：`CoreFalloff` 1.0→**2.0**、`CoreGlow` 2.4→**1.8**、`HaloAmount` 0.5→**0.45**。**(2)** `M_FX_NovaCore` 的 HLSL：halo 项乘上 `(1.0 - smoothstep(0.86, 1.0, r))`——硬阶梯不是 `body` 画的是 `halo` 画的，`pow(1 - r*0.85, fo*0.35)` 在 `r=1`（正好是网格剪影）仍残留约 **0.26**，additive 把它画成一条贴着核心剪影的硬圆。**没有回头缩小核心网格**，第六轮「核心太小」的回馈仍然有效，这轮动的是亮度分布的形状不是尺寸。

| 量测（球半径 123px） | 改前 | 门槛 | 改后 |
|---|---|---|---|
| 核心 luma≥220 连续半径 | 44px (0.36×R) | ≤31px | **29px** |
| 剪影处相邻 1px 环带 G 落差 | 117（Over055 达 163） | <40 | **26** |
| 核心可读半径 luma≥128 | 55px | 50–58px | **51px** |

> **两个门槛必须同时看，只盯一个会来回摆荡。** `HaloAmount` 没有照评审建议降到 0.30——实测 0.30 会让 luma≥128 半径掉到 **48px**，低于评审自己订的 50–58 下限，等于把「核心太小」换个方式带回来。smoothstep 起点也一样：先试 0.78（阶差降到 17，但可读半径掉到 48），改成 **0.86** 后阶差 26、可读半径 51，两项才同时进窗。
>
> **材质参数与 HLSL 都是资料／编译层的改动，不是 Niagara 改图，所以不必重开 Editor 就会进 `preview_system`**——这点与 12.10 节的限制不同，12.11 节改壳层 HLSL 时已确认过一次。`update_custom_hlsl_node` 只送 `code` 不送 `inputs` 时脚位不会断线（`updated_fields` 只有 `code`、`input_count` 仍是 7），但仍核对过连线才编译。

**已验证**：`recompile_material` 无错误、`validate_material` 只剩既有的 `translucent_no_opacity` warning、两个材质资产都已存档；四张截图重拍覆盖，三项门槛如上表。**硬边界环完全没动**：环峰值 Begin **242.7** / Loop **242.4**、位置 r=123/122，与 12.11 节的 243.0/243.2 在误差内。**Over 段仍收乾净**：Over055 环峰 202.2 @ r=92、Over090 核心 ≥220 只到 r=1。Begin 白心由 ≥220 r=14 缩到 r=9，但 ≥128 范围由 19px 扩到 **28px**——falloff 由线性改平方的预期结果。拍完 `User.Radius` 已还原 400 / Phase 0 / PhaseElapsed 0，`list_dirty_packages` 为 0。**全程未提交。**

**未验证**：PIE 实际画面、效能、双端、Cook；本轮未跑自动化测试（只改资产未改 C++）。

**评审留下、本轮刻意没做的四件事**：(1) **`User.Color.A` 现在是核心的静默相依**，闸门实际值 = `User.Color.A × 曲线`，哪天被填 0 整颗核心会消失且极难追查——建议在参数契约测试补一条 `User.Color.A == 1` 断言；(2) Over 收束由 `FieldShell` 与 `CoreSphere` 两条曲线分别描述、刻意错开做「壳先塌、核心后收」，这是第二个维护点；(3) `CoreSphere` 曲线尾段 30.9→31.0 只有 0.1 个曲线单位、小于 LUT 格宽（21.2÷128≈0.166），之后精修尾段时**最小有意义的 key 间距是 0.17**；(4) **`Sparks` 仍飞出危险半径**（Loop 最远 1.44×R、Over 0.9 达 1.82×R），第三轮就被指出、第三次没处理。

**下一步入口**：评审复验中，若连续两轮无必改项就跑 `ElectricSphere` + `ElectricWall` 收尾。详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12.14 节。

**2026-11-14 闪电新星第七轮：补掉 Over 段残留的亮球，上一轮记下的回归已修并重开 Editor 验证。** 第六轮把核心放大 2.2 倍之后，Over `3 / 0.9` 会剩下一颗不会缩小也不会变暗的亮球悬在已经塌掉的壳层里。根因查清楚了：`M_FX_NovaCore` 的白炽中心 `c += hot * Hot` **刻意不乘 `PColor`**（12.3 节为了让中心保持白炽不被蓝色染掉），而 `Color` 模组的 `Scale Color` 通道走的正是 `PColor`——**所以 `Scale Color` 永远调不暗那颗白心**。两个改动：**(1)** 材质加第七个输入 `Amt`，接 `ParticleColor` 的 `A` 输出（output index 4），末段改成 `return c * saturate(Amt);`——additive 忽略 alpha，这个通道是免费的逐粒子乘数，壳层早就这样用，核心本来漏掉。**(2)** `CoreSphere` 的 `particle_update` 加 `Color` 模组（`Color` 绑 `User.Color`，不绑会把 `InitializeParticle` 给的蓝色洗掉），`Scale Alpha` 挂 `FloatFromCurve`，`CurveIndex = User.Phase*10 + User.PhaseElapsed`，线性键 `9.8:0 / 10:0.3 / 12:0.85 / 20:1.0 / 22:1.0 / 30:1.0 / 30.25:1.0 / 30.55:0.45 / 30.9:0.12 / 31:0`。这是绕开第六轮查出的 `ScaleMeshSize` 死通道，不是修好它。

> **三个设定上的陷阱，接手前先看**：
> 1. **`set_dynamic_input_value` 设不出参数绑定。** 对 `MakeFloatFromInt002.Int` 送 `value: "User.Phase"` 会回报成功，但 `get_dynamic_input_tree` 显示 `source: "literal", value: "User.Phase"`——那是字串字面值，整条索引链会算成 0。要绑定必须改用 **`set_module_input_binding`**，`module_node` 填 dynamic input 的 GUID，接完用 `get_dynamic_input_tree` 核对必须看到 `source: "linked"`。
> 2. **重开 Editor 后头几张 `preview_system` 可能全黑，即使 `ShaderCompileWorker` 已经是 0。** 本轮 Begin 连拍两张全黑，改 `User.Phase` 拍 Loop 正常，再回头拍 Begin 就正常了。比 11.5 节原本写的「等编译」更精确：**等进程数归零还不够，要先丢一次参数变更＋重拍**，别看到黑画面就以为改坏了。
> 3. `update_custom_hlsl_node` 的 `inputs` 是**整组取代**，送出后七个脚位全断线，必须逐一重接并用 `get_expression_connections` 核对。`Amt` 漏接读到 0，整颗核心会全黑且**不会报错**。

**已验证**：材质七个输入逐一确认 `connected: true`、重编译、`validate_material` 只剩既有的 `translucent_no_opacity` warning、已存档；`NS_LightningNova` `request_compile` + `validate_system` `valid: true`，警告仍是既有那七笔 `Particles.MeshScale`（第六轮已查明不是误报）、已存档。重开 Editor（改图不重开验不掉）、等 `ShaderCompileWorker` 归零并连续四次确认后重拍四张截图：**Over 0.9 的亮球已变成很暗的小点加稀疏火花**、Over 0.55 读得到「壳层向核心收进去」（核心曲线 0.45 对壳层约 0.27）、**Begin 与 Loop 都没有被误调暗**（充能光点／边界环／放射细丝都在）。拍完 `User.Radius` 已还原 400 / Phase 0 / PhaseElapsed 0，存档后 `list_dirty_packages` 为 0。**全程未提交。**

**未验证**：`ScaleMeshSize` → `Particles.Scale` 的绑定问题仍在（本轮是绕开）；PIE 实际画面、效能、双端、Cook；本轮未跑自动化测试（第五轮的 12/12 仍是最近一次绿灯，本轮只改资产未改 C++）。

**下一步入口**：造型评审若连续两轮无必改项，就跑 `ElectricSphere` + `ElectricWall` 做最后确认并收尾；另外待决定是否修 `ScaleMeshSize` → `Particles.Scale` 的绑定。详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12.13 节。

**2026-11-14 闪电新星第六轮：核心放大、细丝改为由核心长出；同时查出 `ScaleMeshSize` 的曲线是死的，并更正上一轮留档的一个错误结论。** 团队回馈「中间可视的核心太小」「希望电弧像由核心散发出去」——这两件事是同一个决定的后果：12.8 节为了让核心看得见，用 `FilamentInner = 0.3` 在核心外围挖了一圈空隙，那圈「护城河」正是让电弧看起来与核心断开的原因。本轮反过来做：`FilamentInner` 0.30→**0.12**（细丝起点落在核心辉光内部）、`CoreSphere` 的 `Mesh Uniform Scale` 係数 0.009→**0.020**、`MI_FX_NovaCore` 的 `CoreFalloff` 2.6→**1.0**／`CoreGlow` 1.6→**2.4**／`HaloAmount` 0.35→**0.5**。量测（Loop，球半径 123px）：白炽核心 7→**13px**、核心辉光 22→**35px**（占球半径 18%→**28%**），边界环峰值 243.2→242.4 不变。

> **本轮最重要的发现，比造型本身重要：`CoreSphere` 与 `GroundRing` 的 `ScaleMeshSize` 曲线完全没有作用。** 证据：`CoreSphere` 的缩放曲线**整条设 0** 与**设 4.0** 的徑向剖面几乎逐格相同（d00:255 d05:246 d10:225），而 `MI_FX_NovaCore` 三个参数整组设 0 则核心确实消失；`GroundRing` 缩放曲线整条设 0，Begin 的白环依然在，但把 `MI_FX_NovaShell.RingBoost` 设 0 白环就消失。**结论有二**：(1) 实际决定网格大小的是 `InitializeParticle` 的 `Mesh Uniform Scale`（spawn 阶段），`ScaleMeshSize` 写的 `Particles.MeshScale` 没被 renderer 读取——`validate_system` 那七笔一直被当成「既有误报」的 `MeshScale` warning **不是误报**；(2) 那条硬白边界环是壳层材质的 `edge` 项，不是 `GroundRing`。

**更正上一轮留档**：12.10 节写「把 `GroundRing` 的 Over 段改成 30.3 就归零，修掉了双同心环」——**那项改动其实无效**，双环消失只能归功于同一轮的 `FieldShell / Scale Alpha` 改动。12.4 节写的 `CoreSphere` Over「0.95 爆闪」也从来没发生过。12.4／12.8 的 `CoreSphere` 数值冲突（0.36↔0.44 对 0.58）到此结案：**两个都没生效过**。`FieldShell` 是例外，Over 时球确实变小（环半径 Loop 123px、Over 0.55 为 92px），三者差异还没查清。

**本轮引入的回归（尚未修）**：核心尺寸现在三阶段恒定（本来就是，但 `k` 放大 2.2 倍后明显了）。最明显的是 **Over `3 / 0.9`：壳层已收乾净，核心却还是一颗不会缩小的亮球悬在那里**，与 12.1 节「全部向中心塌缩归零」不符。**正确修法**：照 `FieldShell` 的模式给 `CoreSphere` 加一个 `Color` 模组，`Scale Alpha` 与 `Scale Color` 各挂一条以 `Phase*10 + PhaseElapsed` 为索引的曲线——Color 模组的曲线是确定有效的，这是目前唯一验证过能让 `CoreSphere` 随阶段变化的通道。那是改图，**必须重开 Editor 才验得掉**。

**已验证**：四张截图重拍覆盖，Begin 仍无电弧、边界环不变，Loop 的核心与细丝如上。拍完 `User.Radius` 已还原 400 / Phase 0 / PhaseElapsed 0，存档后读回确认，`list_dirty_packages` 为 0。**未验证**：Over 0.9 的回归、PIE 实际画面、效能、双端、Cook；**本轮未跑自动化测试**。

**下一步入口**：给 `CoreSphere` 加 `Color` 模组修掉 Over 0.9 的回归（需重开 Editor 验），并决定是否一併修 `ScaleMeshSize` → `Particles.Scale` 的绑定问题。详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12.12 节。

**2026-11-14 闪电新星外观第五轮：补上外围缠绕电弧，参考图的三项要求到齐，自动化 12/12，PIE 画面与效能待验。** 这是评审排第一的参考图差距——细电弧缠绕在最外围的半透遮罩上。卡住它的不是参数值，是两个结构问题：**(1)** HLSL 迴圈写死 `for (int k = 0; k < 6; k++)`，`BoltCount` 的实际上限是 6，设 8 不会多出任何一条；本轮把上限改成 10 并把 `n` 改成 `clamp(Count, 1, 10)`。**(2)** `az` 是大圆**平面法线**的 Z 分量，俯视下 `az→±1` 贴着剪影跑、`az→0` 变成横过盘面的直线；原本均匀分布在 ±0.8，中间多数横过盘面，而且**大圆由平面决定、`ax` 与 `−ax` 是同一个圆**，正负成对重复、有效条数再打对折。改成只取正值、在 `0.735…0.945`（相对赤道倾斜约 19°–43°）铺开，每条都在外圈绕行，正好补在放射细丝留下的空档上。参数：`BoltCount` 5→8、`BoltDropout` 0.35→0.20、`BoltSpan` 1.25→1.8、`ArcBoost` 1.1→1.4。

**已验证**：`recompile_material` 无错误、`validate_material` 仅剩既有的 `translucent_no_opacity` warning；四张截图重拍覆盖——Loop 外圈出现数条沿剪影绕行的弧并与放射细丝交织，**Begin 仍完全没有电弧**（`Amt` 闸门没漏），Over 0.55 弧与细丝一起淡掉、Over 0.9 仍在 1 秒内归零。**边界可读性**（AGENTS.md 第 12 节）量了徑向剖面：环峰值 243.0→243.2 不变，环内侧低点 139.8→150.8 被弧提亮，**对比 103.3→92.4**，约降一成但仍是清楚的硬阶梯。拍完 `User.Radius` 已还原 400 / Phase 0 / PhaseElapsed 0，存档后读回确认，`list_dirty_packages` 为 0。

**自动化测试**：`ElectricSphere` + `ElectricWall` **12/12**，0 失败／0 警告／0 未执行，报告在 `Saved/ElectricSphere/20261114/NovaLook_R5`（`reportCreatedOn` 为 UTC `2026.11.13-21.33.35`，即本地 06:33，不是旧报告）。其中 `ElectricSphere.NovaFXParameterContract` 通过，等于由测试独立确认 `User` 参数契约没有被这几轮的截图操作破坏。用另开的 `UnrealEditor-Cmd.exe … -nullrhi` 行程跑，没有动到开着的 Editor；本轮只改资产、未改 C++，不需要重编译。

**本轮唯一的效能风险**：大圆迴圈由实际执行 5 次变成 8 次（+60%），`BoltDropout` 降低让 `continue` 提早跳出的机会变少，`BoltSpan` 加大也让更多像素进入线宽计算。请在 PIE 用 `stat GPU` 看 `M_FX_NovaShell` 的成本；要退优先降 `BoltCount` 回 6。**自动化测试涵盖不到这一项**——它跑在 `-nullrhi` 下，根本没有绘制。**仍未验证**：PIE 实际画面、效能、双端、Cook。

**下一步入口**：照第 8 节在 `Content/Maps/Test` 进 PIE，一次验四件事——盘面降亮后范围还读不读得到、细丝与缠绕弧在实机相机距离会不会抖、车辆开进 Loop 区域的遮挡、以及 `stat GPU` 下 `M_FX_NovaShell` 的成本。详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12.11 节。

**2026-11-14 闪电新星外观第三、四轮：盘面降亮、细丝收细、Over 段收束已全部修好并验证（含重开 Editor 后的复验）。** 由特效评审逐轮比对截图与参考图驱动。**第三轮**（留档 12.9 节）：`FillGlow` 0.28→0.15 并把材质 `rim` 项挂上 `Amt` 阶段闸门 `(0.40 + 0.60*saturate(Amt))`，让 Begin 盘面只剩 Loop 的 40%、Over 跟着变暗——additive 圆盘在实机是 8 公尺直径，原亮度会把开进去的车洗白；`FilamentThickness` 0.0075→0.0030、`FilamentBoost` 1.25→1.55，原本 `hwf = FilT*FilN/r` 在 r=0.5 时单条细丝就吃掉角度格距的 66%，必然糊成亮面。**第四轮**（留档 12.10 节）：`FieldShell` 的 `Scale Alpha` 与 `GroundRing` 的缩放曲线整条重写，Over 段提早起跌、地面环 30.3 就归零——原本 Over 0.55 只读得到「变小」、读不到「已经安全」，且两层脱钩裂成两道等亮同心环。

**第三项改动**：材质回传 `(arcs + rim + edge + core) * PColor`，`edge`（硬白环）与 `core` 刻意不吃 `Amt`（Begin 的 `Amt` 是 0，挂上去会让 Begin 失去危险边界），所以场已关掉的 Over 那条边界环仍是 100% 亮度。已在 `FieldShell / Color / Scale Color` 接上同索引的 `VectorFromCurve`（结构与 `Scale Alpha` 同形），重开 Editor 后复验生效：Over 0.55 的壳层本体相对 Loop 的同正规化半径降到约 **26%**，而 Begin 与 Loop 逐格不变。

> **本轮最重要的两个工具发现**：
> 1. 曲线 DI 的**资料**改动会即时进到 `preview_system`，**改图**（新增 dynamic input）不会。把新曲线整条压成 0.3 重拍，像素与改动前逐格相同（环峰值 226.6 两次一致），而 `get_system_diagnostics` 同时回报该 script 为 `UpToDate`——**资产编译状态与预览画面是两回事**。要验证任何改图必须重开 Editor 或进 PIE。这是 11.5 节第 1 点的精确版本。
> 2. 重开 Editor 后著色器编译完成前，`preview_system` 会拍出**全黑**图（诊断一切正常）。拍摄前先等 `ShaderCompileWorker` 行程归零。
> 3. **环峰值不是好指标**：Over 0.55 的环峰值只从 243 掉到 200，但环两侧掉了约 74%——additive 高光在 sRGB 下已近饱和。判断有没有变暗要看剖面，不要看峰值。

**已验证**：材质 `recompile_material` 无错误、`validate_material` 仅剩既有的 `translucent_no_opacity` warning；`validate_system` 无 error；四张截图重拍覆盖并量徑向亮度剖面——Loop 细丝已是细硬折线、盘面中段由 150–170 降到约 105，Over 0.55 的双同心环消失、细丝降到约 27%、环峰值 243→226.6，Begin／Over 0.9 行为不变（`Saved/Screenshots/Monolith/20261114_NovaLook_*.png`，`User.Radius` 暂设 110 拍摄，拍完已还原 400 / Phase 0 / PhaseElapsed 0 并读回确认）。**本轮未跑自动化测试**（Editor 保持开启，依 12.7 节禁止用 Monolith 在 Editor 内跑）。**仍未验证**：PIE 实际画面、双端、效能、Cook；**本轮未跑自动化测试**。

**下一步入口**：跑一次 `ElectricSphere` + `ElectricWall` 确认资产改动没影响玩法测试；接着处理评审排第一的参考图差距——外围剪影的缠绕电弧不足（`BoltCount` 5→8、`BoltDropout` 0.35→0.20、`BoltSpan` 1.25→1.8，并把 HLSL 内 `az` 分布往 ±0.5 以外偏）。详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12.9、12.10 节。

**2026-11-14 闪电新星细丝已改为折线、核心可见度已修正，自动化 12/12，PIE 画面待验。** 团队回馈两点：细丝没有明确折线、太像触须；细丝出现后中心核心变得不太可见。**折线**：原本的中心偏移是两个 `sin()` 叠加，数学上就是平滑曲线，振幅再大也只会变成更夸张的触须；改成分段线性杂讯——把 `r` 切成 `FilamentKinks` 段，取节点杂凑值、段内 `lerp`（不是 `smoothstep`），每段是直线、节点是硬转折。每条细丝的节点间距与相位各自乘上自己的杂凑，否则所有细丝会在同一半径同时转折、形成同心圆纹路。**核心可见度**三处一起改：`FilamentInner` 0.3 在核心周围留空隙、细丝从空隙外缘才开始；壳层自身的中心柔光收窄（`CoreGlow` 0.5→0.22、`CorePower` 4.5→6.0）以免填满空隙；核心球本体加大（`CoreSphere` Loop 缩放 0.44→0.58）并把白炽中心加亮（新建 `MI_FX_NovaCore` 实例，`HotCenter` 2.5→3.5）。同时细丝数量由 34 降到 22、线稍微加粗到 0.0075——34 条在这个半径下会互相叠成一片亮面，反而让折线和核心都读不出来；`FillGlow` 0.55→0.28 让盘面中段暗下来。未改 C++、Blueprint 或 `User` 参数契约。详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12.8 节。

**已验证**：材质重建 `has_errors: false`；三段截图重拍并逐张确认——Loop 的细丝有明确硬转折、白炽核心清楚可辨，Begin 仍只有范围提示与核心、无电弧，Over 仍在 1 秒内收乾净（`Saved/Screenshots/Monolith/20261114_NovaLook_*.png`，`User.Radius` 暂设 110 拍摄、拍完已还原 400）；`ElectricSphere` + `ElectricWall` **12/12**，0 失败／警告／未运行，报告在 `Saved/ElectricSphere/20261114/NovaLook_R2`。**仍未验证**：PIE 实际游戏画面、双端、效能、Cook。

**2026-11-14 闪电新星已改为等离子球造型，三段截图通过、自动化 12/12，PIE 画面待验。** 团队提供参考图，要求「最外围有向内渐淡的范围提示、中间由发光体散出细电弧、细电弧缠绕在最外围的半透遮罩上」，并明确漸层是**外缘最亮、向核心渐淡**（因此不存在模糊危险边界的问题），同时给出三段对应：Begin 只显示范围提示与核心、Loop 为参考图完整样貌、Over 让范围显示与电弧一起收进核心内缩小消失。本轮重写 `M_FX_NovaShell` 的 Custom HLSL：边界渐层改为 `pow(r, RimFalloff)`（`r` 为投影半径，边界 1、中心 0），新增以视线轴对齐、用**角度域重复**而非迴圈逐条绘制的放射细丝（34 条与 6 条成本几乎相同；若改用迴圈，条数会直接变成每像素成本），原有的球面大圆电弧调细后保留作为缠绕层。核心球另建 `M_FX_NovaCore`（Additive／Unlit／双面）取代 `MI_FX_ToonNova_Core`——后者父材质是三阶色卡通烟、设计目的就是制造色块分界，与参考图柔和外扩的白炽球本质冲突，调参数救不回来；旧材质保留未删，可随时切回。三段的分别用一个新通道完成：`FieldShell` 的 `particle_update` 加一个 `Color` 模块，`Scale Alpha` 由 `FloatFromCurve(User.Phase * 10 + User.PhaseElapsed)` 推，材质把 `ParticleColor.A` 接到 `Amt` 只乘在电弧项上，`rim`／`edge`／`core` 不受影响（Additive 混合不使用 alpha，此通道安全）；Over 的「收进核心」由 `FieldShell`／`CoreSphere` 的缩放曲线在 31.05 归零完成。`ArcSpokes` 与 `MicroArcs` 已停用——放射与缠绕都改由壳层材质绘制，这两层在新造型里没有存在意义，关掉剛好抵销壳层变复杂的开销，第 9.2 节「ArcSpokes 从上方被遮住」因此成为历史条目。未改 C++、未改 Blueprint、未改 `User` 参数契约、未新增贴图（全部程序化）。规格、HLSL 要点与参数值见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 12 节。

**已验证**：两份材质 `build_material_graph` 回报 `has_errors: false`，材质属性（Additive／Unlit／双面／Niagara Mesh）在重建后保留；`validate_system` 无 error（七笔 `MeshScale`／`SpriteSize` warning 为既有误报，改版前即存在）；三段俯视截图取得并逐张确认——Begin 为外围亮环向内渐淡加白炽核心、**无电弧**，Loop 为细丝由核心放射至边界、外围亮环包覆（与参考图接近），Over 0.55 秒时范围与电弧一起缩小、火花向外散开，0.9 秒只剩一个小点，1 秒内归零（`Saved/Screenshots/Monolith/20261114_NovaLook_Begin`／`_Loop`／`_Over055`／`_Over090.png`，`User.Radius` 暂设 110 拍摄、拍完已还原 400）；`ElectricSphere` 与 `ElectricWall` 合并复测 **12/12**（含 `NovaFXParameterContract`），0 失败／警告／未运行，NullRHI、单进程，报告在 `Saved/ElectricSphere/20261114/NovaLook_R1`。**仍未验证**：PIE 实际游戏画面（场地亮度下 additive 蓝的对比、细线在远距离的 TAA 闪烁）、双端、效能、Cook。细丝若在远距离抖动，先调 `FilamentThickness` 加粗或降 `FilamentCount`。**新增一个工具坑**：不要用 Monolith 在开着的 Editor 里跑 `run_automation_tests`——本轮这样做让 Editor 直接 assert 崩溃（`Assertion failed: LatentCommands.IsEmpty()`，`AutomationTest.cpp:1370`），资产在崩溃前已存档、没有损失，但自动化测试请一律用 `UnrealEditor-Cmd.exe … -nullrhi` 另开行程跑；Editor 目前是关闭状态，需要重开才能继续在编辑器内作业。下一步：重开 Editor，开 `Content/Maps/Test` 跑 PIE，确认三段在真实场地亮度与相机距离下仍然好读。

**2026-11-14 闪电新星三阶段视觉已重编曲线并存档，画面待重开 Editor／PIE 验收。** 团队反馈「整个新星看不出明显的三阶段」。根因有二：其一，曲线索引 `Phase * 1000` 让曲线 Key 区间横跨 0～3002，而 Niagara 曲线 DI 预设用 **128 格 LUT** 覆盖整个 Key 区间，每格约 23.5 个单位，阶段内 0～2 秒的形状整段塞在一格以内被抹平，三段于是取到几乎相同的值；其二，`7648c5d` 停用了 `GroundRing`，俯视角下没有任何一层把范围画在地面，区域提示读不出来。本轮把七个 emitter 的索引倍率由 `1000` 改为 `10`、曲线 Key 全部改编在 **9.8～32**（Idle 0／Begin 10–12／Loop 20–22／Over 30–32，每格约 0.17 单位），按新的视觉规格重写七层曲线，并重新启用 `GroundRing` 当作区域底盘；`ArcSpokes` 的 Loop 值压到 0.62，避免放射臂伸出危险边界之外（AGENTS.md 第 12 节）。`User` 参数名称／型别、Blueprint 推送与 `HALNovaFX::Contract` 都没有改，倍率与曲线必须同时改、只改一边会让该层永远取到 0。规格与各层数值见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 11 节。

**已验证**：七个 emitter 的倍率读回皆为 `10.0`；存档后的 `.uasset` 内可找到各曲线特征 Key 时间与电墙色 `0.438499`；`User.Phase`／`PhaseElapsed` 预设已还原为 0、七层全部启用；`ElectricSphere` 与 `ElectricWall` 合并复测 **12/12**，0 测试警告／失败／未运行，NullRHI、单进程，报告在 `Saved/ElectricSphere/20261114/ThreeStage_R1`。重开 Editor 后已取得可信的三段俯视截图（`Saved/Screenshots/Monolith/20261114_045840`／`045851`／`045910`／`045920`）：Begin 为地面环撑满半径、粒子向中心落、中央只有小蓄力球；Loop 为环内整片电弧填满；Over 在 1 秒内收乾净，先核心爆闪、后地面环消失只剩散开火花，三段在俯视角下可明确区分。**截图注意**：`preview_system` 相机固定、可见范围仅约 440 cm 宽，用预设 `User.Radius = 400` 拍会把球壳与地面环拍到画面外、看起来像没变化，截图时需暂时把半径调到 110 左右再改回。**仍未验证**：PIE 实际游戏画面与手感、双端、晚加入、效能、Cook。**两个工具坑已留档**：Monolith 的 `preview_system` 会持续回传旧编译结果，即使 `request_compile` 回报 `UpToDate`、`save_system` 已存档也一样，本轮曾因此误判写入没生效、来回改了多轮，资产内容只能用读取动作确认、画面要重开 Editor 或 PIE 才可信；`EditorLoadingAndSavingUtils.reload_packages` 会跳对话框卡住 Editor 主执行绪、Monolith 整个连不上，要重载资产请直接重开 Editor。过程中资产一度处于半套状态，已用 `git checkout` 还原、重开 Editor 后一次写完并重新套用电墙色。下一步：重开 Editor 后开 `Content/Maps/Test` 跑 PIE，确认 Begin 只有地面环、Loop 满径、Over 在 1 秒内收乾净。

**2026-11-14 闪电新星配色已统一到电墙色系，两份资产已保存，待团队 PIE 画面复核。** 用户指出两者造成的效果相同、颜色应属同一色系。实读确认 `NS_LightningNova` 七个发射器的 `InitializeParticle.Color` 全部链接到 `User.Color`，而 `BP_ElectricSphereGimmick` 不推送该参数（与 11-11 的 T3D 逐节点核对一致），因此资产默认值即实际主色；电墙侧 `NS_ElectricArc` 的 `Core`／`ArcInner`／`ArcOuter` 三层 `Color` 模块同为 `(0, 0.438499, 1, 1)`。本轮把 `NS_LightningNova` 的 `User.Color` 默认值从 `(0.35, 0.72, 1)` 改为与电墙同值；核心球另有硬编码的卡通三段分色，`MI_FX_ToonNova_Core` 的 `MidColor` 改为同一电墙色、`ShadowColor` 改为 `(0, 0.298, 0.68)`、`LitColor` 改为 `(0.88, 0.96, 1)`，使三段落在同一色相。其余 Nova 材质（`M_FX_NovaRing`／`M_FX_NovaSpokes`／`M_FX_NovaMicroArc`／`M_FX_NovaShell`）没有颜色参数，`ChargeMotes`／`Sparks` 用引擎默认 Sprite 材质，全部跟随粒子颜色，无需另改。

改动透过 Monolith 编辑器 API 完成，只改两份资产的参数值：`Content/Assets/FX/NS_LightningNova.uasset` 与 `Content/Assets/FX/Materials/MI_FX_ToonNova_Core.uasset`；没有改 Blueprint、发射器堆叠、曲线、C++ 或 Config，`HALNovaFX::Contract` 的名称／型别名单不受影响。**已验证**：改后读回两份资产的参数值与目标一致，`MI_FX_ToonNova_Core` 缩图渲染为电墙同色；`ElectricSphere` 与 `ElectricWall` 合并复测 **12/12**（含 `NovaFXParameterContract`，确认改后的 `NS_LightningNova` 仍可载入且五个参数名称／型别不变），0 测试警告／失败／未运行，NullRHI、单进程，报告在 `Saved/ElectricSphere/20261114/NovaColor_R2`。首轮我多加 `-NoShaderCompile`，引擎在初始化即 `Null assigned to TNotNull` 崩溃，与本次资产改动无关，移除该旗标后 R2 通过。**未验证**：PIE 实际画面、三阶段、双端、效能、Cook。Niagara 系统预览只能在 `User.Phase=0` 的 Idle 状态截图、画面为空，不能当作验收。下一步由团队在 `Content/Maps/Test` 的 PIE 内同时看电墙与新星确认色系一致，必要时再微调 `MI_FX_ToonNova_Core` 的三段分色；详见[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 2.4 节。

**2026-11-14 Car03 柔和卡通火花＋立体气流施工、保存资源、DA 迁移与本机专项完成，下一步团队画风验收。** 从 `main/69a16d5` 接续，UE5.8.3-58210709。接触火花改圆润笔触与概括分色，保留已认可的数量7和全部运动参数；气流改 Niagara 曲面弧网格，支持独立流速、数量／寿命、高低错层、倾角、宽度、上升及发光，所有新增控制仍在技能 DA。只切换现有 DA 的 Wind／Wind Material／Contact Sparks 三项，Ground=None、原暖色及其他调参保留。五个新资源与旧资源并存；玩法接触／伤害／推送／技能规则未改，未编辑 BP。

最终 Editor R13／Game R4 编译链接成功；最终统一原生＋D3D12 渲染 **16/16**、0 测试警告／失败；当前地图双进程 Host 出站额外80ms，拥有者／观察者、原 ObserveContact 的合成权威接触→真实 multicast、数量7、参数复制和到期清理均各 **1/1**。已查看真实车辆与独立粒子图片。开工1277份资产／Config／uproject只有授权技能DA改变，其余1276份哈希一致，保留团队监听GameMode BP和地图修改。早期空Mesh槽、透明内外面条纹与副本编译数据未就绪的失败记录均保留，最终测试已复验；没有四机／发布包／全量历史套件或新真人风格验收结论，未提交／Push。下一入口：重开Editor，在技能DA的 Presentation→FX 调流速／层高／倾角／宽度，验真实撞墙、擦碰和遮挡。见[配置、实际截图与验收](Presentation/Car03_BattleTop_Cartoon_FX_CN_v0.1.md)和[验证摘要](Presentation/Car03_BattleTop_Cartoon_Verification_20261113.json)。下方“讨论／未施工”保留历史时点。

**2026-11-13 Car03 接触专用资源已由团队切换，当前进入柔和卡通造型与立体气流方案讨论，尚未施工。** 只读确认保存技能 DA 的 Contact Sparks 现为 `NS_BattleTop_ContactSpray`，Contact Count仍为7；用户反馈效果有进步且粒子强度可以，因此不再将迁移数量32作为本轮必须动作。新反馈是火花样式偏写实，暖色风环也偏光效且缺乏立体气流层次。保存材质复核与生成源码确认：接触火花采用高发光、热芯及连续渐变；风环为单个平面Sprite上的三组径向细弧，颜色图案整体跟随外观自转。建议保留已认可强度／触发／散射，改圆润短粗飞溅笔触和概括分色；风环升级为Niagara弧面网格粒子，使用错落高度、开放弧段、独立推进／消退和三档方向明暗，新增美术控制仍集中技能DA。见[风格反馈与方案](Presentation/Car03_BattleTop_Polish_FX_CN_v0.1.md#柔和卡通造型与立体气流方案讨论2026-11-13)。本轮只读资源／源码检查与文档同步，无代码／资产修改、构建或新渲染／联机测试；保留团队技能DA、监听GameMode BP和地图修改。下方“仍引用旧系统／下一步数量32”为切换前快照。

**2026-11-13 Car03 特效进度重新核对：接触飞溅加强的代码／资源已提交，保存技能 DA 尚未切换，当前处于接线与本版验证前。** 当前 `main/69a16d5`（add battleTop vfx），本机 UE5.8.3-58210709。只读加载确认 `NS_BattleTop_ContactSpray`／`M_BattleTop_ContactSpray` 均已存在，修正下方“未生成资源”的历史时点；新尺寸／寿命／散射等反射字段可读取。但实际 `DA_Ability_Car03_BattleTop` 的 Contact Sparks 仍为 `NS_BattleTop_Sparks`、Contact Count=7，尚未按加强方案切换为专用 ContactSpray／32。Ground Sparks=None 保持团队隔离选择；已保存风环 Radius Scale=1.45、Opacity=0.55、转速1500°/s、持续10 s、进入0.15 s／退出0.2 s、目标CD0.8 s。完整核对见[当前接续入口](Presentation/Car03_BattleTop_Polish_FX_CN_v0.1.md#当前接续核对2026-11-13)。

原16/16及双端通过属于 `eb1079c`／UE5.8.2 的首版记录；现版渲染测试源码哈希已不同，不能当作加强版或本机UE5.8.3复验通过。下一步只迁移 Contact Sparks 引用及数量，保留底部None和其他调参，再完成匹配现版的构建核对、接触飞溅渲染／运动专项、相关规则与双端清理验证，最后验收真人画面。本轮仅Git／源码／只读保存配方核对及文档同步，未改代码／资产、未运行新构建或玩法／渲染测试；保留监听GameMode BP及NetTest地图已有修改，未提交／Push。早先Editor PID及“资源未生成”记录不代表当前运行状态。只读报告在 `Saved/BattleTop/Progress_20261113/Saved_Recipe_Audit.json`。

**2026-11-13 丢弃后的 Movement／Git 忽略规则已恢复。** 当前 `main/c697601`，保留后续 Black Hole 提交及其他工作区修改，仅恢复 `ArcadeVehicleMovementComponent.cpp` 发布碰撞前角速度的一行，以及 BattleTop 源码／工具／测试／说明文档的七条 Git 白名单。旧验证摘要基于 `eb1079c`，其整文件哈希不能替代当前已合入黑洞功能的版本；本轮按当前提交做最小补回，不回滚后续代码。仅检查恢复差异、忽略规则和文件保护，没有重编或运行新测试；接触飞溅加强仍未收尾，下方构建／施工记录保留各自时点。

**2026-11-13 Car03 接触飞溅加强施工中：源码／新 Niagara 配方及验证代码已写，待关闭 Editor 后完成资源与验证。** 用户确认放大后可见，要求更大、更密、更分散；主方向继续保留反向接触切向，新增表面法线驱动朝外／向上散射、随机大小／寿命／速度、视觉重力和阻力。新建接触专用资源，计划仅迁移技能 DA 的 Contact Sparks 与 Contact Count=32，保留原火花、底部设置及其他调参。参数与验证入口见[施工留档](Presentation/Car03_BattleTop_Polish_FX_CN_v0.1.md)。当前未生成新资源／迁移保存 DA，未宣称构建或新测试通过；Editor（PID 43740）仍运行。下方记录保留早先时点。

**2026-11-13 团队确认接触火花粒子数 > 0，但看不见；当前转查表现可见性。** 用户按手动检查回报该结果，优先级由“触发链缺失”调整为尺寸／寿命、位置、遮挡、材质与渲染器。此前脚本未观察到火花不足以证明没触发，旧结论以本段修正；不将该案例当成需要放宽碰撞资格的证据。下一步由团队复制接触专用 `NS_BattleTop_ContactSparks`，先试 10×50 cm／0.35 s，Contact Count=16，仅 Contact Sparks 引用副本；调试筛选同步改为副本名，底部火花隔离后恢复原资源。见[可见度对照步骤](Presentation/Car03_BattleTop_Polish_FX_CN_v0.1.md)。当前资源哈希仍与上轮真实像素复核一致，本轮无代码／资产修改、构建或新测试通过结论；尚未确认不可见的最终原因，也未标为修复。以下保留早先排查记录。

**2026-11-13 Car03 已由团队接线，撞墙接触火花缺失已复现，尚未修复；气流增强参数已说明。** 当前仍为 `main/eb1079c` 及上轮未提交施工，新增保留团队技能 DA 修改。只读加载确认四个 FX 槽正确，保存转速 1500°/s、持续 10 s、Opacity=0.55／Radius Scale=1.15。独立 Editor 当前地图／配方对照中，排除机关并保持原生碰撞委托后，风环及底部火花正常，普通围墙接触仍未观察到接触火花；具体拒绝点需补原生 Hit→分类／阈值→RPC→Spawn 诊断，不能当作只需放大粒子。现有严格普通静态墙分类对部分机关的适用边界是可疑点，未宣称为最终根因。本轮无源码／保存资产修改或新构建；当前资源像素复核结果见下段，以前的 16/16 不覆盖此次缺失。见[复现结论、下一入口和增强配置](Presentation/Car03_BattleTop_Polish_FX_CN_v0.1.md#配置后反馈撞墙火花与气流强度2026-11-13)。下一步先定位接触触发；风环可先在技能 DA 试 Radius Scale=1.45、Opacity=0.85，再增亮 RGB。同名 MI 参数会被运行时覆盖。未提交／Push。

**同轮资源复核补充：** `NS_BattleTop_Sparks` 在检查期间于 16:26 保存，已不同于原哈希，保留未覆盖；针对当前资源的 D3D12 像素专项 **1/1**、0 测试警告／失败，证明粒子可输出，仍不能证明普通墙触发。详见上方说明文档。没有重跑原 16 项全组合，没有将此次触发缺失标为修复。

**手动排查步骤已补充：** 先暂时清空 Ground Sparks，使用内置 `fx.Niagara.Debug.Hud` 筛选接触火花，结合 `hal.AbilityLog 1` 的 started 区分技能请求、组件创建、粒子输出与可见性。见[手动检查说明](Presentation/Car03_BattleTop_Polish_FX_CN_v0.1.md)。命令／参数仅按本机引擎源码核对，本轮无代码或资产修改、新构建／场景测试，也未增加专用火花日志；缺失仍未修复。

**2026-11-13 Car03 过渡／重复命中／暖色 Niagara 首版代码、资源和程序验证完成，下一步团队接线。** 从 `main/eb1079c` 接续，保留已有监听 GameMode BP 修改。进入 0.15 s／退出 0.2 s 可配置且包含于原持续时间；外观平滑归位，玩法窗口不缩短。每目标独立 0.8 s CD，服务器每 0.05 s 用真实凸包复核持续接触；伤害与额外规则推送共用 CD。接触火花按包含外观自转的撞前相对切向运动反向喷出，底部稀疏火花按近地／移动门槛显示；眩晕、自救和清理沿用已确认边界。新增 `/Game/Assets/FX/BattleTop` 下四个 Niagara／材质资源；风环依实际外观中心和 Bounds 显式适配尺寸。见[接线、参数与验收边界](Presentation/Car03_BattleTop_Polish_FX_CN_v0.1.md)。

本机 UE **5.8.2-56702186** 最终 Editor R13／Game R2 构建成功；最终统一专项＋D3D12 渲染 **16/16**，0 测试警告／失败。当前地图双进程低延迟及 Host 出站 80 ms 注入两轮，双方实际请求、拥有者／观察者自转与气流、到期归位／清理、冷却均各 **1/1**，报告均无测试警告／失败，已检查实际 Car03 截图。原有保存资产、Config／uproject 共 **1270** 项逐份哈希一致，仅新增四个 FX 资源；未改保存的技能 DA／GA／BP／地图，未提交／Push。见[验证摘要](Presentation/Car03_BattleTop_Polish_Verification_20261113.json)。团队下一步重开 Editor，在现有技能 DA 的 Presentation → FX 填入 Wind、Contact Sparks、Ground Sparks 和 Wind Material，按文档验收手感及真实接触／近地火花；四机与发布包仍待项目收尾。本轮未重跑全量历史套件，不扩大为全量通过。

以下为施工前及更早时点记录。

**2026-11-13 Car03 过渡、重复命中和暖色 Niagara 首版规格已确认，尚未施工。** 当前 `main/eb1079c`，保留监听 GameMode BP 的已有修改。进入 0.15 s／退出 0.2 s 均可配置且计入既定持续时间；只平滑自转／气流和结束归位，规则仍从发动至到期生效。用户确认每目标独立 **0.8 s** CD，持续贴靠到期可再次伤害／推送；不是整次只能攻击一辆车，也不照搬 Car04 的车头角度限制。真实接触火花独立限频，按包含视觉自转的相对切向运动反向喷出；底部火花只在技能期间接近地面且移动时少量出现。气流改为琥珀橙／珊瑚红／浅金暖色层次，眩晕、腾空、自救与退场按已确认边界处理。见[已确认规格与施工顺序](Combat/Car03_Battle_Top_Ability_Design_CN_v0.1.md)。本轮仅代码核对、方案与状态留档，无 C++／配置／资产改动、构建或新测试；现有实现仍为固定自转和每目标每次技能仅受击一次。下一施工先做过渡／归位与持续接触 CD，再接三类粒子和实际画面验证。

**2026-11-12 Car04 冲锋接触球不发射已修复代码、构建与程序验证，待真人画面复测。** 从 `main/8af4c4d` 接续，保留团队技能 DA／GA、Car04 Definition 和监听 GameMode 改动。真实碰撞复现确认 UE 在自身 Chaos 处理器中直接触发组件碰撞，技能采样可能尚未到达；原实现因此持续缺少／拒绝同帧采样。现在将相关接触预约至同帧全部事件收齐后，仍由原服务器入口校验并提交，保留角度／速度／拾取锁、规则冲击与自然碰撞边界；无新增 Tick／RPC／复制字段，格式 **6** 不变，球速公式和保存参数未改。新增默认关闭的 `hal.BullChargeContactLog`，见[修复、日志与团队复测](Combat/Car04_Bull_Charge_CN_v0.1.md#接触发射修复与复测2026-11-12)。

本机 UE **5.8.2-56702186** 最终 ContactFix Editor R5／Game R2 成功；原生专项 **11/11**。保存配方真实单 Host 碰撞从修复前失败转为 **1/1**，实际发射 **9000 cm/s**。低延迟及 Host 出站 **80 ms** 注入的真实双进程接触，两端均各 **1/1**，覆盖双方车辆经本人技能请求、真实 Chaos 接触出球和两端归属；服务器确认两球均为 9000 cm/s。既有眩晕／恢复／冷却双端也各 **1/1**，测试报告均 0 警告／失败。全量另记 **199 通过、1 带告警通过、4 失败**；四项均依赖已重命名的旧玩家 DA 路径，本轮未修这些夹具，未宣称全量通过。该轮 1251 保存资产及 Config／uproject 共 **1256** 项与开工逐份一致。无代理资产修改／接线或提交／Push；同机 NullRHI 不替代真人画面、校园四机或发布包。团队下一步重开 Editor 按原保存配方复测，无新增接线。详见[修复验证摘要](Combat/Car04_Bull_Charge_ContactFix_Verification_20261112.json)；下方排查中记录保留原时点。

**本次状态文档恢复核对：** 当前 `main/7648c5d` 已包含后续前后碰撞端面工具及 Nova 电击表现更新，保留这些记录。本次核对 Car04 修复的 5 份源码／测试与验证摘要逐份 SHA256 一致；只恢复本文件，没有改代码／资产、重编或重跑测试。以上为原修复时点的验证，不扩大为后续工具／表现／资产组合的复测结论。

**2026-11-12 Car04 推球问题已检查团队本次日志，接触发射未见提交证据，具体拒绝点仍待定位（修复前记录）。** 本地 17:18:47 结束的 `Saved/Logs/HAL_Future_Creation.log` 确认 NetTest Listen Host、Car04 和单人 Playing；17:18:15.768～17:18:40.840 的 BallNetLog 开启期间没有球状态转换。用户确认仅测技能中接触球。当时应排查接触发射入口，不能将现象认定为 90 m/s 不足；日志没有细分拒绝原因，不能断言具体条件。下一步为增加可关闭的拒绝原因／速度诊断并覆盖真实物理接触；现有人工样本测试未覆盖该真实场景。详见[排查记录](Combat/Car04_Bull_Charge_CN_v0.1.md#球速计算与接触推球排查2026-11-12)。该次仅日志分析和文档，未改代码／资产或运行新测试；后续已按顶部修复完成。

**2026-11-12 Car04 接触球后持续推球，当前为排查中（修复前记录）。** 已接收 `main/8af4c4d` 的技能代码；团队本地新增技能 DA／GA，并修改 Car04 Definition 与监听 GameMode。只读加载确认技能基础／最大球速均为 90 m/s、车速继承 0.75，故车速贡献被上限截断，实际发射初速应为 90 m/s；Car04 保存基础车速 24 m/s、技能速度倍率 1.5，无其他效果时目标上限约 36 m/s。当时尚不能将推球现象认定为单纯球速不足，需要以 Host 对应球的 `Free -> Launched` 日志区分接触发射未触发与发射后的物理／表现问题。见[公式、配置观察与排查](Combat/Car04_Bull_Charge_CN_v0.1.md#球速计算与接触推球排查2026-11-12)。该次两次只读命令行加载退出 0，未修改 C++／资产、未重编或新增玩法复现结论；后续以顶部修复和验证为准。

**2026-11-12 车辆碰撞生成器已支持前后端面独立调整，Editor Target 构建通过。** 从干净 `main/474d90a` 接续。面板 **Front / Rear** 从碰撞根 `X=0` 平面分别量到 +X 前端和 −X 后端，俯视／侧视、渲染网格和简单凸包共用端面坐标。俯视圆角与侧面圆角在端面确定后计算，各组总半径超出总长时按比例缩小实际生成半径，配方保留输入值。原点与轮底基准沿用现有定义。v1～v3 总长载入时平分到两端，v4 分别保存前后距离。见[碰撞生成器参数说明](Configuration/Vehicle_Collision_Generator_CN_v0.1.md#3-参数含义)。

本机 UE5.8.3-58210709 Editor Win64 Development R2 编译与链接成功，`git diff --check` 通过。前后端面、圆角适配、原位更新、轮底和保存重载的自动化代码已编译。独立 Editor-Cmd 启动退出 0，未生成 Unreal 日志或自动化报告，运行结果及场景实测待确认。保存资产未变更。下一步从车型载入已有碰撞配方，分别调整 Front 和 Rear，检查侧视／简单凸包后保存。构建记录位于 `Saved/CollisionGenerator/IndependentEndPlanes_20261112/Build_Editor_R2.log`。

**2026-11-12 Car04「蛮牛冲撞」代码、两 Target 与专项验证完成，待团队配置和手感验收。** 从 `main/3cb1386` 接续。独立技能 DA／原生 GA／GE 接入自动前进、渐进过载、削弱转向、冲击免疫、车头速度伤害、每目标内置 CD、持球／自由球发射及结束收速。眩晕进入帧清零水平速度、停止主动驱动，技能继续计时，解除后按原进度恢复。运动曲线进入同一服务器帧历史，协议升至 **6**；双方使用同版构建。暂用 5 秒总时长含 2 秒爬升、20 秒冷却、1.7 倍上限、0.8 秒每目标 CD，均可配置。见[技能规则、默认值与 Editor 接线](Combat/Car04_Bull_Charge_CN_v0.1.md)。

该轮本机 UE **5.8.2-56702186** 最终 Editor R16／Game R9 构建通过，Release 专项原生 **11/11**，实际 Car04 的低延迟及 Host 出站 80 ms 注入两轮双端均各 **1/1**，报告均 0 测试警告／失败；失效持球发动撤销、未来帧拒绝及眩晕停止回弹并保留序号均已纳入最终复测。全量原生 **192 通过、1 带告警通过、11 旧默认技能夹具相关失败**单独保留，未记为全量通过。该轮 1246 保存资产／Config／uproject 与开工基线逐份一致，保留原有两份资产和状态文档改动。测试使用瞬态技能配方及 Editor `-game`／NullRHI，团队需手动创建技能 DA／GA、替换 Car04 Primary，再验收画面／手感与复杂碰撞。详见[验证摘要](Combat/Car04_Bull_Charge_Verification_20261112.json)。

**本次恢复核对：** 当前已同步至 `main/83a1e60`，保留后续车辆碰撞生成器、碰撞网格及 Car01 DA 重命名记录。Car04 运行时源码与上述验证摘要哈希一致，技能代码／文档仍为未提交改动；本次仅恢复本状态文档，没有重新编译或运行测试，不把原验证扩大为后续 Editor 工具／资产组合的复测结论。

**2026-11-12 车辆碰撞生成器已接入静态轮底测量，Editor Target 构建通过。** 从干净 `main/bd3a670` 接续。**Load Selected Vehicle** 读取实际 Definition 的 Presentation／Skeletal Mesh，复用 Visual Alignment 的四个 SpinBone 与 LOD0 参考几何测量。最低轮底 Z 同时用于侧视预览、渲染网格和简单凸包。Height 从该平面向上计量，网格原点保留在碰撞根。创建时可按车型测量，原位更新时重新测量当前引用者，共享网格的轮底基准冲突时可创建车型专用网格。v3 配方保存轮底，兼容旧 v1／v2。另修正侧面圆角凸包只读取部分生成顶点的问题。见[操作说明](Configuration/Vehicle_Collision_Generator_CN_v0.1.md#车型轮底对齐)。

本机 UE5.8.3-58210709 Editor Win64 Development R6 编译与链接成功，`git diff --check` 通过。首轮构建定位临时路径与 UBA 缓存权限，改用工程 Saved 下的构建临时目录，后续修正编译器报告的局部变量遮蔽后复建。新增轮底固定、侧圆角、共享冲突的回归用例，并扩充配方保存重载测试，测试代码随 Editor 构建。两个独立自动化启动均退出 0，但无 UE 日志或报告，自动化执行和团队场景实测仍待确认。工作区的保存 Mesh／DA／BP／地图未变更。下一步在 Editor 从车型载入，比较两档 Height 的底／顶 Z，保存并 Play 核对实际接地。构建记录位于 `Saved/CollisionGenerator/WheelBottom_20261112/Build_Editor_R6.log`。

**2026-11-12 SlipperyTrail 暗绿色边缘＋中间色缓冲已完成材质施工与本机验证，待比赛地图观感调试。** 用户反馈三档朝向明暗已非常接近预期，本轮从 `main/9a673e3` 接续，只改边缘颜色分布：MI **07 Three tone shading** 新增 `ToonEdgeColor`、`ToonEdgeBufferWidth`（默认24 cm）和 `ToonEdgeBufferSoftness`（12 cm），形成暗绿外缘→中间色缓冲→内部三档。颜色独立于原InkColor，用户调亮的高光和其他MI参数保持；宽度0可关缓冲，配合原InkColor可精确恢复施工前三档，ToonStrength=0仍恢复旧模式。Paint Custom输出扩为float4，RGB原合同保持、A提供同一不规则轮廓内深度，UV／网格／C++接口／WPO／消散／玩法不变。见[配置与对照](Presentation/Slippery_Trail_Edge_Buffer_CN_v0.1.md)、[验证摘要](Presentation/Slippery_Trail_Edge_Buffer_Verification_20261112.json)。

UE5.8.3-58210709，保存材质重载后D3D12 shader编译通过，115节点／VS179／PS453／1采样器。真实生产网格R1 13帧及原生退出0：新／旧三档和更早旧模式恢复差异0，各颜色设置Alpha覆盖189123像素且轮廓差异0，半程消散98262像素且与施工前遮罩差异0，终点及回长0。外缘保守抽样带15027像素中，暗部／高光从7854降到0，内部三档仍均可见；宽度、柔和度、波动及来光响应已检查。实际材质D3D12 Listen Server／Client各1/1，0测试警告／失败／未运行／进行中、两原生进程退出0，验证原MID、扩宽、消散时钟、尾效时玩法／GAS已结束与回收。1246份原保存资产／Config／uproject仅授权母材质变化，其余1245份及349份Source逐份一致，保留用户Definition改动及原MI／技能DA／地图／纹理／轮胎烟尘。本轮仅材质、工具及文档，无C++／反射变更，未重建Target或重跑完整原生规则测试；未改BP、未提交／Push。下一步重开材质或工程，以原技能在比赛地图调暗绿、缓冲宽度／柔和度；复杂弯道／绕回／坡面与真人、校园多机、发布包、大规模性能仍待团队验收，Niagara／液团继续暂缓。

**2026-11-11 SlipperyTrail 地面主体表面朝向三档明暗已实现，待团队比赛镜头调色验收。** 用户明确先用方案一：保留 C++ 地面网格，仅试暗／中／亮三档，不加液团、不迁移 Niagara、不改现有消散。从 `main/e1ded4d` 接续，保留车辆 Definition DA 和前轮文档改动。母材质增加 MI **07 Three tone shading**，从隆起／波动的平滑朝向计算三色，支持独立三色、虚拟来光、门槛、柔和度和强度；默认 ToonStrength=1，0 精确恢复旧配方。新模式保留随形墨线、笔触、WPO、溢边及消散，满强度时关闭旧 04 的重复明暗。见[配置与对照](Presentation/Slippery_Trail_Three_Tone_CN_v0.1.md)、[验证摘要](Presentation/Slippery_Trail_Three_Tone_Verification_20261111.json)。

UE5.8.3-58210709，保存材质重载后 D3D12 shader 编译通过，111节点／VS179／PS438／1采样器。真实生产网格 R2 十帧检查及原生退出0：关闭与施工前对照逐通道差异0；新分色、反向来光、波动、柔和度分别改变156447／166590／44637／19752像素，三档诊断均可见。独立逆透明度 Alpha 覆盖在旧／新／反向／波动／诊断／柔和／恢复之间差异均0，半程消散98262像素、终点0、回长0。R1颜色差计覆盖的误判保留，R2已改用独立Alpha复核。实际材质 D3D12 Listen Server／Client各1/1，0测试警告／失败／未运行／进行中、原生退出0，验证原扩宽、消散时钟、尾效可见时来源／GAS已结束及清理。1246份原保存资产／Config／uproject仅授权母材质变化，其余1245份及349份Source逐份一致，原MI、技能DA、地图、纹理、轮胎烟尘与用户Definition改动保持，未改BP。本轮仅材质／工具／文档，不重建Target或重跑完整原生规则测试；无新增真人、校园多机、发布包或大规模性能结论，未提交／Push。下一步打开原MI的07分组，按文档对照0／1及调三色，原地／弯道／坡面／波动／消散观感由团队验收。以下讨论中的Niagara／液团备选继续暂缓。

**2026-11-11 用户反馈油漆溢边效果不错，下一项仅讨论向 TireSkid 卡通烟尘靠拢，尚未施工。** 当前 `main/e1ded4d` 已包含溢边提交，保留车辆 Definition DA 的已有改动。只读加载／T3D 导出当前保存的 `NS_TireSkid`，确认新增 `DriftSmoke` 为 Sphere 网格＋`M_FX_ToonSmoke_Mesh`，后者为 Unlit／Opaque、以表面朝向和固定虚拟光向切换 Shadow／Mid／Lit 三色并乘 ParticleColor.RGB。建议 SlipperyTrail 采用同源卡通分色，保留地面油漆底层，叠少量低矮液团／泡团及可选退场小团，技能专用 Niagara 由 C++ 表现接口管理，资源与控制参数归同一技能 DA。当前 SlipperyTrail 尚无这层 Niagara 引用与管理，不视为已有能力；精确孔洞跟踪不是首版建议。见[方案记录](Presentation/Slippery_Trail_Paint_Edge_CN_v0.1.md#向-tireskid-卡通烟尘靠拢方案讨论)。本轮只读检查与文档记录，没有修改代码／资产或新增编译／渲染／联机测试；一般正向画面反馈不扩大为完整工况验收。下一步确认视觉方向与首版范围后再开发。

**2026-11-11 SlipperyTrail 圆润油漆／液体溢边已完成程序与材质施工、本机验证，待原比赛地图观感验收。** 从 `main/53d3fa2` 接续，保留用户 Definition DA 已有改动。原主体外新增薄的纯表现余量，由固定地面低频圆润噪声裁剪，墨线跟随新轮廓；主体覆盖、配色、隆起、体积明暗与结束消散保持。技能 DA 的 `Presentation → Paint Edge` 默认开启／15 cm／0.08 半宽比例，双重上限约束溢出；MI **06 Paint edge** 调强度、大形尺度、细节、柔和度及厘米制描边。公共地面结构默认关闭，UV2 新保留为边缘距离／余量，UV1 保持体积梯度。没有新增玩法范围、碰撞、GA Tick、RPC 或独立小滴。见[配置与团队验收](Presentation/Slippery_Trail_Paint_Edge_CN_v0.1.md)、[可同步验证摘要](Presentation/Slippery_Trail_Paint_Edge_Verification_20261111.json)。

UE5.8.3-58210709 最终 Editor／Game R2 构建通过，原生专项 **13/13**、公共 WPO D3D12 回归 **1/1**、实际涂鸦材质 D3D12 Listen Server／Client 各 **1/1**，均 0 测试警告／失败／未运行／进行中。联机用非默认 12 cm／0.06 配方、1.2 s 消散及 Host 出站 80 ms 注入，确认参数复制、无碰撞、本地 MID、完整扩宽、尾效时玩法／GAS 已结束与最终回收。生产网格＋保存材质的 D3D12 R2 十帧像素检查通过且退出 0：原主体丢失／越出包络／重采样漂移／消散回长／终点残留／关闭恢复差异均 0，新增溢边 10279 像素，半径抽样外扩约 1.17～14.65 cm。首次空画面夹具失败已修正并保留原记录。母材质98节点／VS179／PS373／1采样器。1246份原保存资产／Config／uproject仅授权母材质变化，其余1245份及5份核心玩法源码逐份一致；原MI调色、技能DA、地图、纹理与用户Definition改动保持，未改BP，未提交／Push。下一步重开Editor，以原技能按文档调试；原地／弯道／绕回重叠／坡面／断段和双端强制清理由团队验收，尚无新增真人、校园多机、发布包或大规模性能结论。下方历史“最新／尚未施工”以本段为准。

**2026-11-11 SlipperyTrail 分块蒸发消散完成代码、构建、原生／材质／双端程序验证，待团队原地图画面验收。** 从 `main/d08eae9` 接续，保留用户 Definition DA 改动。玩法寿命保持：到期移除区域来源、停止摆动并按既有曲线恢复抓地、结束 GAS；随后默认 1 s 的纯视觉消散，DA 的 `Presentation → Dissolve Seconds` 可调 0～5 s，0 为立即消失。范围冻结于玩法截止，固定地面噪声缺口扩大连接，MI 的 **05 Dissolve** 调图块尺度、柔和度和边缘颜色；原颜色／体积明暗／WPO 保留，形变关闭也支持。复制的服务器时间驱动进度，晚观察可重建当前阶段，视觉截止即隐藏；强制退场仍清理。见[配置、实现与图示](Presentation/Slippery_Trail_Dissolve_CN_v0.1.md)和[验证摘要](Presentation/Slippery_Trail_Dissolve_Verification_20261111.json)。

本机 UE5.8.3-58210709，最终 Editor／Game R2 完整构建通过，原生专项 **12/12**、实际涂鸦材质 D3D12 Listen Server／Client 各 **1/1**，均 0 测试警告／失败／未运行／进行中。联机使用非默认 1.2 s 尾效及 Host 出站 80 ms 注入，双方均记录 `Clock=1 GameplayOffWhileVisible=1`，验证复制时长、尾效可见时来源／GAS 已结束、恢复与最终清理。实际材质 D3D12 R4 八帧检查及原生退出 0：剩余覆盖 158450→140268→78294→12417→0，回长为0、恢复差异0、初期3个内部缺口。中间读回单位／地面噪声与进程包装器的验证问题均保留后修正；历史 Volume 计量单位已澄清。母材质84节点／VS175／PS296／1采样器。1246份原有保存资产／Config／uproject仅授权母材质变化，其余1245份逐份一致，MI、技能DA、地图、纹理和用户Definition改动保持，未改BP，未提交／Push。没有额外蒸汽粒子、真人／校园多机／发布包或大规模性能结论。下一步重开Editor，以原技能测试并调整Dissolve Seconds及05 Dissolve；原场景的拐弯／重叠／坡面／强制清理继续按文档验收。下方历史“最新／尚未施工”以本段为准。

**2026-11-11 SlipperyTrail 平滑卡通体积明暗与可选真实投影已完成，待团队比赛镜头验收。** 从 `main/0c53a5d` 接续，保留用户已提交的调色与开工前 Definition DA 修改。现有涂鸦母材质继续 Unlit／Masked，在原颜色输出后增加可关闭的虚拟方向明暗；MI 的 `04 Volume shading` 提供强度、方向、法线表现强度、色块门槛、柔和度及亮／暗强度。公共地面网格 UV1 增加平滑权重梯度，MID 增加 `HAL_GroundInvTileSize`，按同一隆起／波动公式计算平滑朝向，避免初版三角面块；基础 WPO／UV0／中央权重／范围／玩法与权威管线保持。技能 DA 的 `Presentation → Deformation → Cast Shadow` 可选开启真实投影，默认关闭。见[配置、对比图与实现](Presentation/Slippery_Trail_Graffiti_Material_CN_v0.1.md#体积明暗与投影2026-11-11)、[公共 UV1 合同](Presentation/Ground_Surface_Deformation_CN_v0.1.md#平滑体积明暗的附加合同2026-11-11)和[验证摘要](Presentation/Slippery_Trail_Volume_Verification_20261111.json)。

本机 UE5.8.3-58210709，最终 Editor／Game R2 构建成功，原生相关 R2 **11/11**、生产组件 D3D12 位移回归 **1/1**，均 0 测试警告／失败／未运行／进行中。保存材质的 D3D12 六图 R3 对比通过：体积／方向／波动／投影变化分别 209547／239442／87823／4346 像素，关闭后恢复差异 0；71 节点，VS175／PS225／1采样器。原接口、配色检查误判及块状初版均保留后修正。1246 份原有保存资产／Config／uproject 中只修改授权母材质，其余1245份逐份一致，原 MI 配色、技能 DA、纹理、地图及用户 Definition 改动均保留；未修改 BP，未提交／Push。真实投影为可选场景阴影，没有额外私有软阴影层。下一步打开原 MI 调整 Volume 参数；先关闭波幅验静态隆起，再开轻波动并按需启用 Cast Shadow，验收急弯／绕回／斜坡及 Host／Client 生命周期。当前没有新增真人、双端、发布包或多区域投影性能结论，以下早期节点数／未施工记录保留原时点。

**2026-11-11 量产特效画风纲领已补充：保持夸张卡通／手绘涂鸦，颜色、纹理与边缘稍柔和、圆润。** 用户实测反馈 SlipperyTrail 新材质比旧版明显接近预期，配色由用户继续调试；当前首版的尖锐笔触与强烈对比需要在后续制作中适度缓和，同时保留卡通分色、轮廓和比赛可读性。要求适用于后续技能／地面区域／车辆状态／机关特效，已记录到[特效团队交接纲领](Presentation/MVPB_Vehicle_FX_Team_Handoff_CN_v0.1.md#量产特效视觉纲领2026-11-11-补充)和[当前材质说明](Presentation/Slippery_Trail_Graffiti_Material_CN_v0.1.md#画风目标)。当前母材质 `Ink border threshold` 的 SmoothStep Min／Max 为 0.10／0.12，宽度尚未暴露给 MI；已补充加粗、变细及柔和过渡的 Editor 手工调节步骤。此轮仅三份文档修改，保留全部已有改动，没有修改材质／纹理／BP／DA 或 C++，没有新增编译／渲染／联机测试；用户一般画面反馈不扩大为复杂场景或双端验收通过。下一步由用户调色并按说明调描边，后续特效遵循新纲领。本轮未提交／Push。

**2026-11-11 SlipperyTrail 新涂鸦风材质已创建并完成独立 D3D12 渲染检查，下一步团队切换技能 DA 验收比赛画面。** 用户反馈隆起已生效，但原油膜观感不符合《Need for Speed Unbound》的夸张卡通方向，授权重做材质。从 `main/a5dfa12` 接续，新增 `M_SlipperyTrail_Graffiti`、`MI_SlipperyTrail_Graffiti` 和独立笔触纹理，使用粗深色轮廓、绿紫硬分色、局部网点和分步移动笔触；保留公共 WPO 四参数接口。通过 Unreal Editor API 创建新资源，没有编辑 Blueprint 或覆盖旧材质／技能 DA。概念图与实际渲染图分别留档，见[材质配置、参数与交接](Presentation/Slippery_Trail_Graffiti_Material_CN_v0.1.md)。此前“待补 WPO 接线”是历史时点，用户现已反馈变形有效。

本机 UE 5.8.3-58210709，保存资源重新加载后 D3D12 着色器编译通过，59 节点／67 条已检查连接；独立瞬态地面网格 R5 渲染通过，512×512 中绿色 38145／紫色 13911 像素，已查看导出图片。VS 173／PS 151 指令、1 采样器为本设置统计。R1～R3 验证脚本接口错误及 R4 未等待更新的空画面均留原记录，修正后 R5 通过；不记作技能画面／联机验收。1243 份原有保存资产／Config／uproject 与开工保护基线逐份一致，保留三份用户资产及地面说明文档的已有改动。没有 C++／配置修改或新 Target 构建／玩法自动化，未提交／Push。下一步将 `DA_Ability_SlipperyTrail → Presentation → Ground Material` 设为新 MI，按文档试 18 cm／4 cm／1.5 s 后在比赛镜头验收轮廓、笔触、弯道／重叠和双端到期清理；完整悬空涂鸦／烟团属于额外表现层。

**2026-11-11 通用地面隆起／波动程序及验证完成，已接入 SlipperyTrail，待团队补母材质 WPO 接线。** 从 `main/b06ecae` 接续并保留上一轮进度文档改动。新增可复用的 `FHALGroundSurfaceDeformation`、地面条带／圆端细分及 `UHALGroundSurfaceComponent`，输出中央到边缘权重、独立 MID、服务器相位与 WPO 包围盒。技能 DA 的 `Presentation → Deformation` 管统一参数，默认 8 cm 隆起、2 cm 起伏、3 s 周期，通过 Enabled 显式启用。UE5.8.3 Editor／Game R3 编译链接通过，原生专项 **11/11**，D3D12 测试材质实际轮廓／波动像素检查 **1/1**，D3D12 本机双端专项各 **1/1**。1243 份保存资产／Config／uproject 保护基线逐份一致。日志位于 `Saved/GroundSurface/20261111`。团队按[通用地面隆起与波动](Presentation/Ground_Surface_Deformation_CN_v0.1.md)在 Editor 接母材质 WPO，再启用 DA 并验收实际油膜画面。

**2026-11-11 SlipperyTrail 联合开发进度核对，当前进入油膜观感调参与保存配方验收。** 核对基点为干净 `main/b06ecae`，本机 UE 5.8.3-58210709。程序已接入按服务器轨迹、地面法线和时间扩宽的动态地面网格，技能 DA 提供 `Presentation → Ground Material / Ground Lift / Texture Tile Size`。`987a129` 已提交遮罩、母材质、实例和技能 DA 的更新，本次只读确认保存的 `DA_Ability_SlipperyTrail` 包引用 `MI_SlipperyTrail_OilSheen`。团队留档已记录游戏内看到油膜，后续完成暗绿底纹与绿紫高光的独立流动接线；最新一版实际观感、急弯／绕回叠色、断段和双端到期清理待复测。当前先按[油膜特效速查](Presentation/Slippery_Trail_OilSheen_QuickGuide_CN.md)调参并在比赛镜头下验收。既有构建、隔离原生 188/188 和 D3D12 双端专项属于 11-10 UE 5.8.2 的地面表现施工记录，本轮为 Git、源码、保存包引用与文档核对。

**2026-11-11 闪电新星接线已核对，参数契约已用自动化测试锁住；团队 PIE 已确认三个阶段都有。** 本机从落后 73 个提交快进到 `main/b06ecae` 后接续。**开工第一个发现是本机 binary 停留在 11/02，`HALElectricSphereGimmick` 不在 DLL 内**，导致 `BP_ElectricSphereGimmick` 的 generated class 为 `None`、Blueprint Status 为 `BS_UNKNOWN`，`MVPB_NetTest` 与 `Test` 两张图里摆好的球一起载入失败，而且只有 LogLinker 警告、没有错误弹窗。重编后恢复，接续这个环节必须先重编再开 Editor。

`NS_LightningNova` 实际宣告的五个参数型别已实读确认：`User.Phase` 为 **Int32**，`PhaseElapsed`／`Radius`／`Intensity` 为 Float，`Color` 为 Linear Color。把 `BP_ElectricSphereGimmick` 导出 T3D 逐节点核对，存档接线与 `b06ecae` 说明**一致**、无 guard：四个推送节点的名称字符串与型别全部正确，`User.Color` 与 `User.Intensity` 目前无推送。查引擎源码 `FNiagaraUserRedirectionParameterStore` 确认 `User.` 前缀与裸名等价，commit 里「拼法未确认」一项结案；真正的风险是 `SetVariable*` 的 `bAdd=true` 在名称或型别对不上时静默新增参数、特效安静不动，新增测试即针对这一点。

新增 `HAL.FutureCreation.ElectricSphere.NovaFXParameterContract`（并入既有 `HALElectricWallTests.cpp`，沿用该档世界夹具），锁住：两个存档资产可载入（直接抓未重编状况）、五个名称均已宣告、元件指向 `NS_LightningNova` 且 Auto Activate 关闭、**override store 内不存在未宣告或型别不符的参数**、以及夹具自检。该夹具**无法驱动 Blueprint 事件本体**——Loop 转换后世界时钟已到 0.15 秒而 Blueprint 自己的 `PhaseStartTime` 仍为精确的 0——所以推送的值不做断言，只锁契约；已逐一排除延后队列、`Activate()` 重置、名称型别错误与元件抓错，**未发现接线缺陷**；随后**团队在 PIE 内确认三个阶段都有**，确定这纯粹是夹具限制。该限制对任何「原生事件→Blueprint 表现」的接线都成立，若要让自动化涵盖这类推送需另立课题。

本机 UE **5.8.2-56702186** Editor Target 编译通过（警告仅既有 `HALCameraTests.cpp` 的 C4305）。`ElectricSphere` 与 `ElectricWall` 合并复测 **12/12**（球体 3 项含本轮新增 1 项，电墙既有回归 9 项），0 测试警告／失败／未运行。NullRHI、单进程；报告在 `Saved/ElectricSphere/20261111/NovaFX_R3`，过程诊断留在同目录 `NovaFX`、`NovaFX_Diag`～`NovaFX_Diag3`。团队 PIE 三阶段为实际画面观察，非本机量测。**仍未验证**：双端、晚加入、效能、Cook。本轮未修改任何 `.uasset`／`.umap`／Config。

下一步入口：[闪电新星参数契约与验收](Presentation/MVPB_LightningNova_FX_Authoring_CN_v0.1.md)第 7 节的移除 Event Tick（改推 `User.PhaseStartTime`、Niagara 侧用 `Engine.Time` 相减，同一次变更要改测试里的 `HALNovaFX::Contract` 名单；**必须先改 Niagara 再改 Blueprint**，只删 Tick 会让曲线停在每个阶段第一帧）与第 9 节两个缺口（`User.Intensity` 未接线、`ArcSpokes` 俯视被遮）。Niagara 与 Blueprint 的实际编修一律由团队在 Editor 执行：本轮重新确认 UE 5.8 的 Python 无法编写 emitter 堆叠，新的 `NiagaraExt_*` 外部编辑 API 只有 USTRUCT 进了 Python、函式没有 `UFUNCTION`。

开工时本机有 5 份未提交改动，其中 `BP_MVPB_ListenGameMode.uasset` 与 `BP_ElectricWallGimmick.uasset` 与上游同档冲突（二进制无法合并），用户已确认两份都不保留，改动留在 `git stash` 未还原；`DefaultEngine.ini` 的 Python 远程执行设定及其余不冲突改动保持未提交。

**2026-11-11 单车测试观察摄像机已改为Alt鼠标操作，Editor编译链接通过。** 从干净`main/19b5bf5`接续。在现有测试Controller中实现Alt左键绕观察中心旋转、Alt中键缩放、Alt右键平移及无需Alt的滚轮缩放；面板提示已同步，原观察模式右键WASD飞行移除。观察拖动期间暂时释放车辆输入，松开／失焦／切换镜头／换车恢复，车辆自身摄像机继续沿用原右键观察。未增加公共Controller接口或源码文件；现有四项测试中的相机用例扩充轨道半径、同步平移、距离缩放／限制、重新聚焦及Alt／滚轮绑定检查。UE5.8.3 R1发现局部Player变量遮蔽基类成员，改名后NoLink R2成功；Editor释放DLL后完整Editor R3成功，运行／工具模块均已写入。专项进程退出0但无UE日志／报告，实际执行及鼠标操作仍未验证；未重做Game Target。本轮1245份保护基线逐份一致，保存资产／Config／公共Controller／公共相机和车辆源码保持，未提交／Push。日志与基线位于`Saved/VehicleTest/20261111/AltMouseNavigation`；详见[接线与操作](Configuration/SingleVehicleTest_Setup_CN_v0.1.md)。

**2026-11-11 车辆资产生成器默认碰撞网格已更新，完整Editor链接已完成。** 从干净`main/5d9d79a`接续。`HALVehicleAssets::DefaultCollisionMeshPath`改为`/Game/Meshes/Collision/SM_Car_01_Collision.SM_Car_01_Collision`，创建前校验和新Vehicle Definition的赋值共同使用该路径。已确认目标保存资产存在；其他默认参数与已有资产保持。UE5.8.3-58210709 Editor模块`-NoLink`编译成功，含现有生成器测试源码；随后Alt鼠标操作调整的完整Editor R3已成功链接工具模块，解除此前Editor占用限制，尚未实际生成验收。说明同步到[车辆资产生成工具](Configuration/Vehicle_Asset_Generator_CN_v0.1.md)，原编译日志`Saved/VehicleAssetGenerator/DefaultCollision_20261111/Compile_Editor_NoLink.log`，完整链接日志`Saved/VehicleTest/20261111/AltMouseNavigation/Build_Editor_R3.log`；保存资产／Config无改动，未新增源码文件、未提交／Push。

**2026-11-10 单车测试逻辑已移入专用文件，Editor／Game构建通过。** 从`main/987a129`接续现有未提交测试功能。`HALPlayerController.h/.cpp`、`HALPlayerControllerRound.cpp`与`HALGameFlowPermissions.h/.cpp`保持仓库原实现。测试Controller保留HAL继承以复用真实GAS换车与调试命令，在自身生命周期中调用UE基类实现，跳过房间UI、准备轮询、房间菜单和比赛观战更新。测试世界识别、Room Ready例外与相机输入规则均位于现有四份测试源码，公共车辆／相机只保留少量调用入口；未新增源码文件。UE5.8.3-58210709 Editor R3／Game R1编译链接成功，四项测试已扩充普通Controller行为与鼠标输入回归检查。两次专项启动均退出0但没有UE日志／报告，不能确认四项单车测试及九项相机测试实际执行，面板操作与驾驶验收仍待团队完成。1235份资产／Config／公共头文件／构建配置保护基线逐份一致，蓝图接线保持，未提交／Push。详见[单车测试接线与验证](Configuration/SingleVehicleTest_Setup_CN_v0.1.md)和[本轮记录](Configuration/SingleVehicleTest_Verification_20261110.json)。

**2026-11-10 圆形打滑范围的操控效果已对齐 Slippery Trail，生成者不免疫。** 从干净 `main/ae389ba` 接续。改用共享 Handling 结构及 Trail Config 原生默认：抓地 0.30、恢复 0.35 秒、摆动幅值 5 rad/s²／周期 0.9 秒／满幅速度 700 cm/s／渐入 0.2 秒。旧平铺抓地字段改为新 Handling 分组，实际保存圆形 BP 只读加载后已验证自动采用与保存 Trail DA 相同的参数。圆形范围、持续时间、红蓝 Debug Draw 与生成命令保持，生成者也受抓地与摆动影响。用户关闭 Editor 后本机 UE5.8.2 Editor／Game Development 构建成功，Debug＋MovementAreas＋SlipperyTrail 专项 **11/11**，0 测试警告／失败／未运行／进行中。含保存配方／BP 比对、生成者受影响、离区移除摆动及既有重叠／历史回放。NullRHI 单进程，未新做双端或真人手感验收。报告 `Saved/ExpandingSlipperyArea/TrailParity/index.json`，日志 `TrailParity.log`；圆形 BP 与 Trail DA 验证前后哈希全同，未改资产或 Config，未提交／Push。[当前规则与参数](Combat/Expanding_Slippery_Area_CN_v0.1.md#规则与入口)。

**2026-11-10 地面 Debug Draw 更新已完成编译和专项复测。** 用户保存关闭 Editor 后，UE 5.8.2 Editor 链接成功；Game 构建已通过。Debug＋MovementAreas 本次 **8/8**，0 测试警告／失败／未运行／进行中，含新地面网格勾选、红蓝颜色、地面高度和到期隐藏检查。NullRHI 单进程，不代替团队实际画面或双端验收。报告 `Saved/ExpandingSlipperyArea/GroundDebug/index.json`，日志 `GroundDebug.log`。团队现在重开 Editor，在自建 BP 的 Class Defaults → Slippery Area → Debug 勾选 **Debug Draw**，Compile／Save 后进游戏输入 `HALSpawnSlipperyArea`。无需旧显示命令。下方“待关闭／未运行”保留上轮历史时点，现已解除；未提交／Push。

**2026-11-10 圆形打滑区改为 BP 勾选的地面 debug 网格，Editor 更新待关闭后继续。** 按用户反馈新增 **Class Defaults → Slippery Area → Debug → Debug Draw**（默认关闭，服务器复制）；扩张红色、维持蓝色，填充圆盘＋内部网格＋圆边界。取当前生效半径，在中心地面切平面上方 3 cm 显示。原 `hal.SlipperyAreaDebug` 开关已移除，生成仍用 `HALSpawnSlipperyArea`。代码已完成，Game Development 构建通过；Editor 的 C++ 编译完成，但最终链接被正在运行的 UnrealEditor（PID 21456）锁定 DLL 阻止，已请用户保存并关闭。新增显示自动化尚未运行，旧 7/7 不算本次复测。下一入口：关闭 Editor 后重新构建 Editor，运行 Debug＋MovementAreas（含新网格勾选／颜色／地面高度测试），团队在 BP 勾选 Compile／Save 后实测。保留用户新 BP、地图和 DefaultGame.ini 修改，代理未编辑资产或配置，未提交／Push。详见[当前用法](Combat/Expanding_Slippery_Area_CN_v0.1.md#调试显示与下一步)。

**2026-11-10 扩散打滑区域 debug 生成命令已完成。** `HALSpawnSlipperyArea` 默认在当前车辆前方 350 cm 生成；Project Settings → HAL Debug → Slippery Area 可选择自建 BP 与距离，保留该类默认范围／时间／抓地。服务器自行选择类与位置，客户端只发无参数请求；Shipping 禁用。显示开关为 `hal.SlipperyAreaDebug 1`，每个窗口独立开启。UE 5.8.2 Editor／Game Development 构建通过，debug 与区域专项 **7/7**，0 测试警告／失败；未做真实客户端 RPC、保存 BP 画面或真人验收。报告在 `Saved/ExpandingSlipperyArea/DebugCommand`；[用法、验证与手测](Combat/Expanding_Slippery_Area_CN_v0.1.md#debug-生成命令)。本轮保留前轮未提交改动，未改保存资产／Config，未提交／Push。下方“下一步命令”保留此前时点。

**2026-11-10 新增可生成的圆形扩散打滑区域，下一步接 debug 生成命令。** 从干净 `main/b11f305` 接续，新增 `AHALExpandingSlipperyArea`：生成时立即检查，默认从 1 cm 半径在 2 秒内扩至 500 cm，扩张完成后维持 5 秒再销毁。用户确认持续时间从扩张完成后计算。复用原 Slippery GE／Receiver 和离区恢复，不加摆动或生成者免疫。`hal.SlipperyAreaDebug 1` 显示扩张红色、维持蓝色圆柱；生成命令尚未实现。中心／高度判定与 0.02 秒采样边界见[说明与下一入口](Combat/Expanding_Slippery_Area_CN_v0.1.md)。

本机 UE **5.8.2-56702186** Editor／Game Development 构建通过；最终区域专项 **5/5**，0 测试警告／失败。首轮编译的复制宏参数名错误已修复；第一次专项 4/5，新增夹具未开启世界生命周期导致销毁断言失败，补齐后 R2 通过。日志／报告保留于 `Saved/ExpandingSlipperyArea/Tests*`，不将其作为跨设备提交证据；详细结果已记入上述文档。本轮未修改保存资产、现有驾驶或 GE 实现，未执行新双端、画面、真人或全量原生验收，未提交／Push。

**2026-11-10 扩散打滑带已接入地面网格和材质槽。** 基点 `main/0e017ce` 已含技能 DA 和 GA。运行时按复制的轨迹、地面法线与时间生成地面带，支持逐段扩宽、圆端、断段和到期清理。`DA_Ability_SlipperyTrail → Slippery Trail → Presentation → Ground Material` 已可配置，源 PNG 已生成。团队下一步导入灰度纹路，制作暗绿材质实例，独立调底色、反光、纹路强度和透明度。见[纹理导入与材质接线](Combat/Vehicle_Slippery_Trail_Ability_CN_v0.1.md#暗绿油墨地面材质)。

本机 UE 5.8.2 Editor／Game 构建通过，D3D12 离屏双端专项各 1/1。全量原生在当前玩家打滑带授权下为 177 通过／11 失败，旧测试依赖免疫／加速主技能。以相同构建和提交版资产隔离复测为 188/188。1215 份已保存资产校验一致，开工时两份本地测试资产改动已保留。正式油墨材质仍待团队导入接线和画面验收。见[验证记录](Combat/Vehicle_Slippery_Trail_Ability_CN_v0.1.md#2026-11-10-地面表现验证)。

**2026-11-10 扩散打滑带代码已同步，进入 Editor 配置和车型实测。** 本机 `main` 与 GitHub `main` 均为 `e12a1d9`，核对前工作区干净。技能代码和自动化已随 `b60b2a0` 提交。此后 Car04 的模型、动画 BP、车辆 BP、Definition、表现 DA 和自定义碰撞体已入库，监听 GameMode BP 已引用 Car04，比赛图另更新了场地墙体。默认房间选车目录目前仍列 Car01／Car02。

仓库现有技能资产中尚未找到打滑带配置 DA、派生 GA 或车型授权引用。按[Editor 接线](Combat/Vehicle_Slippery_Trail_Ability_CN_v0.1.md#editor-接线)创建 `HALSlipperyTrailConfig` DA，将其赋给 `HALSlipperyTrailAbility` 子蓝图的 `Skill Config`，再把目标车型 Definition 的 `Input.Ability.Primary` 授权指向该 GA。通过房间测试 Car04 时，先把它登记进测试用 Vehicle Catalog。接线后用保存资产验收区域轮廓、车辆摆动、离区恢复与双端清理。

11-09 的 UE 5.8.3 构建、原生 187/187 及双端专项来自当时的临时配方。当前源码、Config 与 uproject 对照技能提交一致，后续车型和地图资产已变化。本机安装 UE 5.8.2-56702186，本次进度核对的范围为 Git、源码与保存包引用。本机重编、保存配方、实际画面和手感仍待验证。

**2026-11-09 量产主动技能「扩散打滑带」程序与联机专项验证完成。** 从 `main/0d50d76` 接续。独立技能 DA 管铺设、扩散、共同消失、抓地、随车速缩放的摆动、冷却和青色调试线。服务器使用车尾地面轨迹和地面扫掠判定，沿用现有区域 GE 与抓地恢复，运动历史格式升至 5 以支持摆动重演。UE 5.8.3-58210709 Editor 与 Game Target 编译通过，原生 **187/187**，本机双端新技能专项和旧移动区域回归均各 **1/1**。正式车型模型尚待导入，保存配方、画面与真人手感待验收。见[规则、参数与 Editor 接线](Combat/Vehicle_Slippery_Trail_Ability_CN_v0.1.md)。

**2026-11-09 Car03 自转速度移入技能 DA，编译与复测通过。** `DA_Ability_Car03_BattleTop → Presentation → Spin Rate` 默认 900°/s。发动时服务器将速度与开始时间一起复制给拥有者和观察者。关闭 Editor 后重链接成功，Game 和 Editor Target 均通过。原生 **185/185**，双端用 540°/s 配方验证各 **1/1**。只读确认最新保存的 Car03 主技能指向 `GA_Car03_BattleTop`，GA 引用已配置的技能 DA，新字段读取 900°/s。以关闭 Editor 后的资产校验值为基准，本次验证期间四份相关资产保持一致。见[配置步骤](Combat/Car03_Battle_Top_Ability_Design_CN_v0.1.md#7-editor-接线与首测)。

**2026-11-09 MVP-B 暂时收官，量产首个主动技能 Car03「战斗陀螺」程序和自动化验证完成。** 已接通固定时长 GE、独立技能 DA、物理手刹限制、受击水平推送、车辆／墙面／球接触、自转及 Avatar 清理。眩晕保留接触能力，自救翻身暂停陀螺接触，技能计时继续。下步由团队保存 Car03 技能 DA／GA 引用并验收实际反弹、出球和外观。MVP-B 特效验收结果另由队友回填。见[战斗陀螺设计与 Editor 接线](Combat/Car03_Battle_Top_Ability_Design_CN_v0.1.md#7-editor-接线与首测)。

基点 `main/2644859`，UE 5.8.3-58210709 R7 两 Target 编译通过，原生 **185/185**。相同生产代码的 R6 Car03 专项及 GF7 三回合均双端各 **1/1**；Car03 专项用临时配置，包含 Host D3D12 外观变换和手刹状态起止，未替代团队实际保存配置验收。1193 份保存资产与 FX-5 基线一致，Config 和 uproject 保持。联机使用同版构建，运动历史为格式 4。见[验证记录](Combat/Car03_Battle_Top_Ability_Design_CN_v0.1.md#8-本轮验证)。

**2026-11-07 车辆特效团队交接已整理。** 从 `main/e43e1fa`（已含 FX-5 提交）只读核对三车型、选车目录、表现 DA 与电击／拖尾／持球资源。当前共享电击仍未启用，默认选车目录只有 Car01、Car02；Car03 的 BP／Definition／Presentation 齐备，需在测试分支临时登记后做第三车型的联机验收。本轮只新增交接文档和入口，无新构建或画面验收。见[特效团队交接](Presentation/MVPB_Vehicle_FX_Team_Handoff_CN_v0.1.md)。

**2026-11-07 FX-5 程序与自动化完成，保存配方接线和团队画面验收待做。** 已从保存的车辆 BP 和状态 FX DA 导出 27 个旧配置字段。Car01 有现用电击材质和丝带，Car02／Car03 的旧组件未配置这两个引用。共享配方以 Car01 的实值为接线基准；保存 DA 仍关闭渲染，配置好后由团队显式启用。现有回弹提前显示、远端观察、权威确认接续、去重和 1.5 秒未确认收尾条件保持原实现。共享路径使用同一通用实例和 Overlay 协调器，保留专用闪光、丝带、淡出与焦痕时间轴。下步团队按[FX-5接线与验收](Presentation/MVPB_Vehicle_Status_FX5_Implementation_CN_v0.1.md)保存、比对实际画面，随后进入 FX-6 组合回归。

基点 `main/7dbe29c`，UE 5.8.3-58210709 Editor／Game 均编译通过，全量原生 **183/183**，共享电击 D3D12 离屏夹具 **1/1**，双端预测确认、否决及练习房至比赛电击测试各角色 **1/1**。离屏夹具用保存的电击着色器和临时配方，双端回归用现有保存配置。**1193 份保存资产校验值一致**，Config／uproject 保持，未提交。实际保存共享配方的三车丝带／材质、Cook 和性能未验收。详见[保存基线](Presentation/MVPB_Vehicle_Status_FX5_Saved_Baseline_20261107.json)和[构建／测试记录](Presentation/MVPB_Vehicle_Status_FX5_Implementation_CN_v0.1.md#5-本轮构建与验证)。

**2026-11-07 FX-4A～4D代码完成，并按用户要求在四步接通后集中验证；团队保存资产接线／粒子画面仍待验。** 4A统一Overlay所有权和原材质恢复，保留旧电击时序、参数与渲染；4B按共享布局、活跃／淡出优先于纯残留、DA优先级与稳定Tag排序选择通道；4C接入Bounds、强度和粒子预算；4D绑定真实Static／Skeletal组件、检查CPU采样LOD及缓冲，默认跳过失败层，只有显式配置才使用Bounds替代。**两个兼容表面通道是首版能力与验收目标；状态数量独立管理，显示通道允许扩展。** 见[施工合同](Presentation/MVPB_Vehicle_Status_FX4_Plan_CN_v0.1.md)、[实施／Editor配置](Presentation/MVPB_Vehicle_Status_FX4_Implementation_CN_v0.1.md)及[验证保护记录](Presentation/MVPB_Vehicle_Status_FX4_Verification_20261107.json)。

最终UE5.8.3 R9 Editor／Game通过，全量原生**181/181**（新增FX-4九项），实际D3D12离屏双通道**1/1**（红蓝共显，关闭蓝后保留红）；专项双端与GF7三回合分别各角色**1/1**，六份最终报告均0测试警告／失败／未运行／进行中。专项Host为D3D12并实录旧电击Overlay挂载SkeletalVisual，Client与三回合为NullRHI，Host出站80ms注入。原生检查实际DI绑定、预算、显式替代和回收；新粒子图／表面采样画面需团队配置模板后验收，不冒充保存资产画面、Cook／Shipping、校园四机或性能验收。中间UHT／编译／链接、原生及RHI失败均保留并说明，最终以R9为准。

开工干净`main/73e0c13`。**1193保存资产逐份全同；320既有输入改12份，其余308份保持；新增8份，最终328份输入冻结。** Config／uproject保持，Build.cs仅增加Editor夹具用RHI／RenderCore。未改保存BP／DA／材质／Niagara／地图；专用进程已退出，未提交／Push。新配方显式Enable Rendering，旧电击通用路径仍只观察；团队按配置文档验收新输出，下一开发入口FX-5先保全实际旧电击基线再迁移。以下此前“FX-4未施工”保留历史时点。

**2026-11-06 FX-4重点约定（用户已确认）：两个通道作为首版能力与验收目标；状态数量独立管理，显示通道允许扩展。** “两个通道”指同一Mesh首版同时组合的两个兼容表面材质通道，不限制车辆同时拥有的状态数，也不把粒子层数量限定为两个。玩法状态／持续时间／层数继续由GAS与规则层管理；表面通道按兼容性和优先级选择，未显示状态仍保留自身计时。后续增加通道时扩展共用母材质与参数映射，复用既有观察、来源、计时和生命周期系统，不逐车型重做。详见[总体方案第4节](Presentation/MVPB_Vehicle_Status_FX_Discussion_CN_v0.1.md#4-多状态共存必须一起设计)。本轮仅记录此项约定，未开展FX-4代码开发或新增构建／测试；不据此将其他提议标为全部确认。

**2026-11-06 FX-3C观察入口／诊断完成代码与同版验证，FX-3的A／B／C已完成，下一步FX-4。** 车辆基类自动接入ASC显式视觉Tag当前快照及根Tag计数通知，同Tag一个ASC来源、已有状态首次采集、重复刷新不重启、弱引用与绑定代次保护；增加本地命名调试来源、详细来源／实例／资源诊断和默认关闭变化日志。未配置不阻断玩法，资源就绪前批量重核状态，覆盖UE5.8.3单loose tag部分移除的通知缺口。见[C实施／调试命令](Presentation/MVPB_Vehicle_Status_FX3C_Implementation_CN_v0.1.md)与[验证保护记录](Presentation/MVPB_Vehicle_Status_FX3C_Verification_20261106.json)。新组件仍仅准备资源，现有电击继续由旧组件显示；FX-4绘制协调／真实采样、FX-5电击迁移、FX-6组合仍后续。

本机UE5.8.3最终R4 Editor／Game成功，全量原生**172/172**（新增C五项）；专项观察与GF7三回合的Host／Client分别各**1/1**，五份最终报告均0测试警告／失败／未运行／进行中。专项经房间选车／确认／开赛与跨图，实测客户端调试不改服务端Tag、HP或物理版本，真实GE两端观察、多来源合并、已有复制状态重绑及结束释放。当前保存FX配方仅表面材质，双端实际MID=1／Niagara=0；粒子对象生命周期由原生瞬态夹具覆盖，不冒充实际Niagara网络画面／GPU验收。双端均本机Listen Server、Host出站80ms、NullRHI；无校园四机、Cook／Shipping或真人画面复验。R1夹具NativeTag类型编译失败、R2原生171／1及R3联机未选车中止记录保留，详见C留档。

开工干净`main/94cad64`，团队已提交B。**1193保存资产逐份全同；已有318输入仅修改5份C++，其余313份保持；新增2份C++，最终320输入冻结一致。Config／Build.cs／uproject保持。** 无需团队修改BP／DA／地图；专用进程已退出，未提交／Push。下一独立入口FX-4先确定每Mesh的Overlay唯一所有者、优先级／有限组合与恢复，再做Bounds／Static／Skeletal粒子适配；保持与旧电击的所有权边界。下方旧“下一C／C未施工”保留各自时点，以本段及第2／6节为准。

**2026-11-06 FX-3B本车资源／生命周期完成代码、两Target及程序验证，下一步FX-3C。** 新增C++基类组装的`StatusFXPresentation`，持有A账本并接FX-1目标／FX-2缓存：需求等待与撤销、首次观察时钟补偿、每车MID候选／粒子层所有权、Mesh代次、弱事件回调、重绑与退场清理。MID按本车Mesh＋基础材质复用，Niagara保持未注册／未激活；尚不写Overlay或接管电击，ASC观察／诊断仍属C。见[B实施／团队说明](Presentation/MVPB_Vehicle_Status_FX3B_Implementation_CN_v0.1.md)与[验证保护记录](Presentation/MVPB_Vehicle_Status_FX3B_Verification_20261106.json)。

本机UE5.8.3最终R5 Editor／Game成功，全量原生**167/167**（新增B六项），Host／Client三回合各**1/1**，均0测试警告／失败／未运行／进行中。双端Listen Server、Host出站80ms注入、NullRHI；验证原流程回合／换图／返房回归，不冒充新状态FX绘制、真实ASC观察、GPU／Cook／真人验收。GC测试实际核对粒子销毁和MID回收；此前R2／R3退出码3分别暴露旧自救／冲击组件EndPlay空World，已补保护；R4退出码0但166通过／1失败，因夹具回收独立地图资产，改用标准Editor GC保留规则后重验。失败证据全部保留。

开工干净`main/84e3098`，已包含团队上传的A。**1193保存资产逐份全同；已有315输入仅改车辆装配cpp／h及上述两处退出保护，其他311份保持；新增3份C++，最终318输入冻结一致。Config／Build.cs／uproject保持。** 无需团队修改BP／DA／地图；专用进程已退出，未提交／Push。下一独立入口C接ASC视觉状态观察、受控调试来源与状态／资源诊断；FX-4绘制、FX-5电击迁移、FX-6组合仍后续。下方A／B“尚未施工／下一B”保留各自时点，以本段及第2／6节为准。

**2026-11-06 FX-3A实例／来源模型完成代码、构建与原生验证，下一步FX-3B。** 按确认顺序先交付纯C++本地模型：绑定代次／永久撤销的来源租约、精确视觉Tag聚合、等待／活跃／淡出／残留、刷新不重启与重入复用、实例句柄及确定快照。无效输入／旧回调不改变时钟或新实例；零时长／跨阶段推进有明确清理边界。模型不持有Actor／资源／ASC、不注册Tick，尚未挂车；现有电击／车辆规则保持。见[A实施与后续接口](Presentation/MVPB_Vehicle_Status_FX3A_Implementation_CN_v0.1.md)和[验证保护记录](Presentation/MVPB_Vehicle_Status_FX3A_Verification_20261106.json)。

开工main/06eb9ab、保留前轮方案／状态文档改动。本机UE5.8.3最终Editor R2／Game R2成功，新增A八项及全量原生**161/161**通过，0测试警告／失败／未运行／进行中。此前R1已通过，复查补定义晚到的初始年龄后重新完整验证，旧记录独立保留。**1193保存资产逐份全同，已有312输入保持，只新增3份C++，最终315输入冻结一致；Config／Build.cs／uproject未改。** 没有代理保存BP／DA／地图，专用进程已退出，未提交／Push。本步无新联机／GPU／Cook／画面验收，不把模型测试写成已接管电击。下一独立入口B为每车组件、FX-1／2资源与Mesh生命周期；C观察／诊断、FX-4绘制协调、FX-5电击迁移及FX-6组合仍后续。下方“尚未施工／下一A”保留各自时点。

**2026-11-06 FX-3分步方案已确认，当前入口为FX-3A，尚未施工。** 按用户最新同步说明更新[已确认方案](Presentation/MVPB_Vehicle_Status_FX3_Plan_CN_v0.1.md)：依次进行A实例／来源与阶段、B本车资源／生命周期、C观察入口／诊断，每步单独编译、自动化和留档。默认同Tag一份持续实例、多源只维持存在、刷新不重启、淡出重入复用；来源带绑定代次，旧退出不能影响新绑定。FX-3采用旁路／受控调试，现有电击继续由旧组件负责；材质协调／真实粒子适配与电击接管仍归FX-4／FX-5。

本次从干净`main/06eb9ab`接续，已含FX-1／FX-2、两份状态FX DA及默认目录配置，以及之后的电墙电弧／拖尾和资产生成器更新。本机UE5.8.3-58210709，与原FX-1／2留档UE5.8.2区分。核查源码尚无FX-3运行组件；目录资产及配置路径存在不等于已Ready或画面验收。**本轮仅同步方案／状态文档，未改C++／Config／资产，未构建或新增测试，未提交／Push。** 旧153项等证据按原时点阅读，不冒充当前最新组合复验。下方“待确认”保留此前讨论时点，以本段及第2／6节为准。

**2026-11-06 FX-3进入方案讨论，尚未施工。** 当前干净基点`main/62d5a35`，已包含FX-2提交及团队创建的两份状态FX DA，`Config/DefaultGame.ini`已配置默认目录；下方“默认仍None／待创建DA”是此前交付时点。本轮核对引用及源码，未重新导出资产或运行Ready／Cook／画面验证。新增[FX-3讨论稿](Presentation/MVPB_Vehicle_Status_FX3_Plan_CN_v0.1.md)，建议拆为A实例／来源、B本车资源／生命周期、C观察入口／诊断，默认同Tag一份持续实例、多源仅维持存在、刷新不重启、淡出重入复用；这些默认规则待用户确认。材质协调／表面采样和电击接管继续归FX-4／FX-5，FX-3先旁路／受控调试，不让新旧系统同时写电击表面。仅更新讨论稿／状态／白名单，无C++／Config／资产改动、构建或新增测试结论，未提交／Push。

**2026-11-06 FX-2共用状态FX配置、资源准备与Cook入口完成代码／构建／程序验证，待团队创建并指定DA。** 从干净`main/4e5d586`接续已提交FX-1，本机UE5.8.2-56702186。新增效果定义、精确视觉Tag目录、Project Settings → Game → Vehicle Status FX的Default Catalog；普通车型None继承，少量例外按Tag覆盖，其余继续继承公共目录。GameInstance子系统异步准备、缓存只读资源、去重请求及取消退出回调；Pawn仅请求准备，不新增驾驶／开局就绪门槛。项目模块收集默认目录Cook根，硬引用保留定义／材质／Niagara／曲线及车型例外依赖。没有新增Tick／复制字段／RPC／插件，现有电击尚未迁移。见[Editor接线／结构／验证](Presentation/MVPB_Vehicle_Status_FX2_Implementation_CN_v0.1.md)及[完整保护记录](Presentation/MVPB_Vehicle_Status_FX2_Verification_20261106.json)。

最终Editor R3／Game R1成功；新增FX-2测试5/5、全量原生153/153，原GF7同机NullRHI双进程三局Host／Client各1/1，四份最终报告均0测试警告／失败／未运行／进行中。两端确认新子系统Unconfigured且流程正常；没有把它记成新保存DA已Ready或实际打包通过。首轮重复Tag夹具TArray自引用断言已修成独立副本，失败日志保留；完整Editor构建原有相机测试C4305警告另记。**1190保存资产逐份全同、312最终输入冻结一致，Config／Build.cs／uproject保持**；未代建／改BP、DA、地图，未提交／Push，专用进程已退出。团队按实现第3节手动建两份DA并指定一次默认，重启后用`hal.StatusFXStatus`检查Ready；目前默认仍None，不阻塞后续施工。下一独立环节为**FX-3每车运行实例、来源聚合及生命周期**；FX-4～FX-6、实际Cook／GPU／画面／校园四机及其他MVP-B真人欠项保持。下方FX-1及此前咨询记录保留各自历史时点，当前以本段为准。

**2026-11-06 FX-1统一外观目标完成代码、构建和原生回归，待团队可见画面检查。** 从干净`main/1f046a8`接续，已保留最新弹簧拳／Car_03更新，本机实际UE5.8.2-56702186。复用VehiclePresentation按实际骨骼／静态路径选择主外观，额外部件明确注册、确定排序；快照提供Bounds中心／半尺寸、完整Mesh变换及物理根空间合并包围体，处理非零偏移／旋转／非均匀／镜像缩放。重配置、部件变更、退场及EndPlay有绑定失效边界；新增一次性`hal.VehicleFXTargets`日志与10秒包围框。没有新增Tick、复制字段／RPC或插件，现有电击播放尚未迁移。见[实现／团队操作](Presentation/MVPB_Vehicle_Status_FX1_Implementation_CN_v0.1.md)及[验证保护记录](Presentation/MVPB_Vehicle_Status_FX1_Verification_20261106.json)。

本机Editor／Game首轮成功，新增FX-1测试4/4、全量原生148/148，两报告均0测试警告／失败／未运行／进行中；原有相机测试C4305编译警告单独记录。**1190保存资产逐份全同、303最终构建输入冻结一致，Config／Build.cs／uproject保持**；没有代改BP／DA／地图，未提交／Push。NullRHI验证不代替屏幕画面、真实表面采样或校园四机；本轮没有新Listen Server／四机测试。团队可按实现第3节检查三车型绑定，无需新DA或蓝图接线。下一独立环节为**FX-2共用效果定义／Tag目录／项目默认引用与资源校验**；FX-2～FX-6未施工，原有MVP-B真人与发布包欠项保持。下方“尚未开工／main/7c7ede7”等为之前咨询时点，当前以本段为准。

**2026-11-06车辆通用状态FX总体方案已获用户确认，已拆为六环节，尚未开工。** 施工顺序为FX-1统一外观目标／空间尺寸、FX-2共用目录／效果定义／项目默认、FX-3每车运行组件与生命周期、FX-4材质共存／通用粒子适配、FX-5迁移现有电击并保留即时反馈／权威接续、FX-6第二占位视觉状态组合回归与交接。框架建设包含公共能力、电击迁移及共存验证；中毒／着火完整玩法与正式美术为后续独立制作。团队手动创建／接线DA、材质、Niagara并做视觉验收，代码代理维护C++和操作文档。工作量及每步完成标准见[方案第6节](Presentation/MVPB_Vehicle_Status_FX_Discussion_CN_v0.1.md#6-建议施工顺序与验收)。

当前仍为`main/7c7ede7`，本轮仅维护方案与状态，保留前轮文档／白名单改动；没有C++、配置或资产修改，也没有新构建／测试，未提交／Push。下一步入口为FX-1的独立施工，先完成一环节再推进；下方“尚待确认”是前次咨询时点。

**2026-11-06车辆通用状态FX方案咨询完成初步检查，尚未施工。** 当前`main/7c7ede7`开工干净。现有Shocked元件自动给可见非物理根的Mesh加Overlay，但资源／曲线仍配在各BP组件；主电弧读首个Mesh尺寸构造椭球并挂物理根，并非真实表面采样，完整坐标／体型适配不足；状态结束直接清Overlay也不支持多状态共同拥有。建议项目默认状态FX目录＋每效果共享定义＋每车本地运行组件，复用VehiclePresentation取得有效Mesh／变换，统一材质合成与生命周期；正常新车继承项目默认，例外才覆盖。GAS仍管规则，保留当前电击回弹提前表现和权威接续。见[检查结果、建议结构与施工顺序](Presentation/MVPB_Vehicle_Status_FX_Discussion_CN_v0.1.md)。

本轮仅源码／引擎接口和Epic主文档核对、建议留档及Git白名单更新，**没有C++／配置／资产修改、重编或新引擎实测**，通用FX施工尚待确认。现有BP资源值未重新全量导出，不据旧Car_02缺资源日志断言当前保存配置；本轮未提交／Push。相机原场景复测、团队UI制作、独立电墙高速／发布包／校园四机等安排保持。

**2026-11-06 Game Flow UI团队定制交接说明已整理。** 用户将界面定制交由团队；新增[Game Flow UI定制开发交接说明 v1.0](Networking/GameFlow_UI_Customization_Handoff_CN_v1.0.md)，按当前`main/328b78e`核对入口、选车、练习／比赛HUD、菜单、载入／倒计时、观战、结算与返房用途。明确GI的Entry／Room／Pause三个Screens槽、各自HAL父类、Receive View／按钮／焦点接口、品牌与Catalog配置，以及原生跨World加载幕没有WBP配置槽。补GF7数据和相机输入边界，提供Editor操作、分工与验收清单；UI素材／布局由团队手工制作，未代理创建或编辑资产。

本轮仅新增说明、加入Markdown Git白名单并更新本入口，保留原有NetTest地图修改和前轮相机接续文档／JSON。没有C++／配置／BP／DA／地图修改、构建或新引擎界面验收，未提交／Push。下一步团队先制作Entry与Room最小可用WBP，经Entry验证完整流程，再制作可选Pause、比赛／观战／结果表现；原相机真人复测、独立电墙高速／发布包／校园四机及其他MVP-B欠项保持。

**2026-11-06接续核对：相机边界修复已随`main/328b78e`提交，当前进入原场景真人复测。** 已核对实际源码：临时Movement Recovery不撤销同一Controller／PS／Epoch的首次输入就绪；房间HUD不因此切入菜单；菜单／失焦只清观察输入，保留自动取景距离；长帧限步长而不全量重种，换车／跨图／真实跳变仍有明确重置与`hal.CameraLog`。保留Loading就绪屏障与物理／GAS规则。原施工文件中的“未提交／尚未修复”是历史时点，当前以本段、[实现说明](Presentation/MVPB_Camera_Input_Boundaries_CN_v0.1.md)与[本次接续核对](Presentation/MVPB_Camera_ProgressSync_20261106.json)为准。

对照已提交验证清单，299份Source／Config／uproject中155份字节相同、144份仅换行符差异，无实质源码差异。留档为另一设备UE5.8.3的两Target、144原生与9份干净报告，包含Entry练习／Playing重复电墙、无注入／Host出站80ms＋替换UI、旧GF4两局及GF7三局；这些是原施工证据，**本设备D盘实际UE5.8.2-56702186，原始Saved报告不在此设备，本轮没有重编、重跑或新增真人验收**。当前1190份资产与清单相比，仅已有未提交`Content/Maps/MVPB_NetTest.umap`不同，保持该团队修改，不当作地图仍与原验证版本完全一致。本轮只更新接续留档；下一步双方同版构建后，经Entry在实际保存DA／当前地图重复原事故，开启`hal.CameraLog 1`并配合CameraDebug检查。真实鼠标／手柄、校园四机／发布包及独立电墙高速轨迹欠项继续分开验收。

**2026-11-05相机输入／恢复／生命周期边界拆分完成代码、构建与程序验证，待原场景真人复测。** 按用户授权，首次Avatar就绪按Controller／PS／Movement Epoch检查；同一绑定临时Recovery保留首次就绪，不重建菜单输入。菜单／失焦清理驾驶和观察输入，保留自动距离／聚焦历史；长帧限步长，换配置／重新接管／退场／跨图与真实位置跳变明确处理。新增默认关闭`hal.CameraLog`，全部UI仍原生回退＋WBP替换。无需重建或修改DA／BP。见[实施／日志／复测](Presentation/MVPB_Camera_Input_Boundaries_CN_v0.1.md)及[验证保护清单](Presentation/MVPB_Camera_Input_Boundaries_Verification_20261105.json)。

本机UE5.8.3最终Editor／Game成功，**144/144原生、无注入双端／80ms出站注入＋替换UI双端／旧GF4两局／GF7三局各角色1/1**，共9份最终报告均0测试警告／失败／未运行／进行中。相机双端实际从Entry进入练习／Playing，各阶段两车合计6次电墙接触并开关菜单；Client实测`Menu=0 PhysicsReady=0 Recovery=1`且取景距离连续。两端D3D12离屏、瞬态测试Profile，不冒充保存DA画面／真实手柄或校园四机；原事故仍待团队复测。缺include、原生夹具未BeginPlay及未迁移测试球导致的失败全部保留，严格修正重验。原生最终报告后仅修联机夹具球类，生产／原生测试源码不变。**1190保存资产逐份全同、299最终输入冻结、Config／Build.cs／uproject保持**；专用进程已退出。基点main/e4f0b09，保留此前进度文档修改，未代改资产，未提交／Push。以下旧“尚未修复／仅诊断”保留当时时点，当前以本段及第2／6节为准。

**2026-11-05进度接续复核：当前优先入口是Client电墙后的镜头短暂缩近，仍处于诊断完成、尚未修复。** 当前工作区开工干净，`main/e4f0b09`已包含GF-7提交`86f513e`、返房再加入修正`10f19ed`、电墙后续更新及Car_03素材／动画BP。对照相机诊断时`ee17c2b`核查，所列8份相关源码内容均一致；4份原始哈希差异仅来自LF／CRLF，不能误记为新修复。当前1190份保存资产仅作数量核对，不把历史1183份保护清单扩写为本次完整资产验证。本机引擎为UE5.8.3-58210709，原诊断的UE5.8.2保留原时点。本轮仅同步本文件，未改代码／配置／资产，未构建、复现或重跑测试，未提交／Push。

**2026-11-05 Client反复撞电墙后镜头短暂缩近：完成初步定位，尚未修复。** 团队确认经Entry房间／练习或比赛触发，无明显同时卡顿，Host不出现。源码首要候选为移动物理恢复使Ready短暂波动，房间HUD将其当输入模式变化，`SetMenuInputActive`经NeutralizeInputForMenu全量ResetCamera；相机下帧把距离直接种回当前DA的1600cm，再恢复动态拉远。已确认这条代码耦合存在，但没有事故当帧Ready／Reset日志，不能写成实际触发原因已证实；此前距离须大于1600cm才直接产生缩近。次候选为长帧／大跳变重种或旧1200cm相机fallback。两车型共享Camera DA、FOV均90°、Spring Arm碰撞关、阈值0.25秒／1000cm已只读核对。

本轮从干净`main/10f19ed`检查，团队期间更新至`ee17c2b`，相关相机／HUD／移动路径未变。本机UE5.8.2，仅源码与只读资产导出，1183保存资产逐份未变，**未改C++／配置／资产，未构建／实际联机复现或修复，未提交／Push**。后续先记录Reset原因、距离与Ready／Recovery／UI切换关联，再分离运行中物理恢复、菜单输入切换与相机全量重置；初次Avatar／Loading保护保持。见[定位、方案与辅助观察](Presentation/MVPB_Dynamic_Camera_Implementation_CN_v0.1.md#10-client反复撞电墙后短暂缩近2026-11-05仅诊断)及[配置／源码证据](Presentation/MVPB_Client_ElectricWall_Camera_Diagnosis_20261105.json)。此前电墙轨迹／特效验证不等于本次相机组合场景通过。

**2026-11-05 電牆三條電弧與車身電流去重。** 本機 main/2213239、UE5.8.2-56702186。自控與觀察車輛從回彈立即起效，權威 shocked 標籤可接續原播放。兩者間的連續空窗以 1.5 秒為上限，逾時則淡出。電牆執行時沿用已配置的 Niagara，中央線兩側各加一條，碰撞區仍為單一電牆。Editor／Game 構建成功。D3D12 離屏雙端重跑及 NullRHI 否決均各 1/1，驗證單次啟動及三條分離路徑。早期短接續造成兩次啟動，以及調整後一次觀察標記漏取的失敗報告保留。Car_02 去重以元件狀態驗證，正式地圖攝影機畫面待真人複測。見[修正與驗證第11節](Combat/MVPB_Electric_Wall_CN_v0.1.md#11-車身電流去重與電牆三條電弧2026-11-05)。

**2026-11-05 Client 自車及觀察車輛車身電流起點修正。** 本輪基點為乾淨 main/b3aa5bf，本機 UE5.8.2-56702186。同 ASC 重新綁定時更新本機控制權，觀察車輛從收集移動時同步的回彈標記開始表現，Niagara 在啟動前填入首批電弧資料。Editor／Game 構建通過。兩輪高速雙端都通過新增特效斷言，Client 仍觸發既有的初始反向速度波動斷言，沒有額外反向脈衝。D3D12 離屏 Accept 各 1/1，夾具 Car_02 日誌無材質及 Ribbon，僅核對觸發。卡頓原因與正式地圖畫面同步仍待複測。見[車身電流同步第10節](Combat/MVPB_Electric_Wall_CN_v0.1.md#10-client-自車與觀察車輛的電流起點2026-11-05)。

**2026-11-05 車身電流接上 Client 本機回彈。** 本輪基點為乾淨 `main/3a8af57`，本機 UE5.8.2-56702186。自控 Client 在物理更新後開始車身電流，權威 shocked 標籤抵達時接續原時間軸。雙端夾具加入反向速度與視覺狀態重疊、以及兩者結束後釋放的檢查。首輪 Editor 因占用 DLL 連結失敗，使用者關閉後完整構建通過，Game Target 亦通過。首次高速雙端的兩項特效斷言均通過，Client 觸發既有的回彈速度波動斷言。以相同條件重跑，Host／Client 各 1/1 通過。Host 否決接觸的回歸也各 1/1。測試使用 NullRHI，正式地圖可見效果待複測。見[車身電流同步](Combat/MVPB_Electric_Wall_CN_v0.1.md#9-車身電流與本機回彈同步2026-11-05)。

**2026-11-05 GF-7基础比赛闭环完成代码、构建及本机程序验证，待团队真人／发布包验收。** 用户已确认并列／全灭、退场观战、8秒结算返房及Client弃权继续四组规则。正式Playing默认300秒，权威批次淘汰／绝对HP超时判定、值快照、独立本地观战、自动返房／Host提前返房已接入；全部UI保持原生回退＋Widget Blueprint可替换。Room DA新增Match Rules，现有资产读取默认值，无必建UI／输入／GA资产。见[已确认规格](Networking/GF7_Match_Loop_Plan_CN_v0.1.md)、[配置／UI接线与验收](Networking/GF7_Match_Loop_Implementation_CN_v0.1.md)及[验证保护清单](Networking/GF7_Match_Loop_Verification_20261105.json)。

本机UE5.8.3最终Editor／Game成功，**138/138原生、真实双端三局／四端Exhibition／旧GF4双端两局各角色1/1**，共9份最终自动化报告均0测试警告／失败／未运行／进行中。四端验证Host淘汰后继续服务、目标弃权后自动切换、其余继续、两人超时并列及Host通过Widget提前返房；双端覆盖最后存活／超时并列／同批全灭和自动返回。D3D12检查原生结果及内存UMG替换接口；切观战截图含移动模糊，不作最终镜头手感验收。GF7 Host出站80ms为同机注入，非实测RTT／校园四机。首轮旧世界NetGUID错误与夹具接入时序失败保留，补远端车辆接管确认屏障后严格复验。**1183保存资产逐份全同、295最终构建输入留档，Config／uproject／Build.cs保持**；专用进程已退出。保留开工前草案／状态／白名单修改，未代改BP／DA／地图，未提交／Push；下一步按实现第4节短时局真人验收，再重新打包／校园四机及原MVP-B集中收尾。以下旧“待确认／未施工”均保留各时点，当前以本段与第2／6节为准。

**2026-11-05 GF-7基础比赛闭环进入方案讨论，尚未施工。** 从干净`main/726b804`核对，临时血条已提交；现有手动返房／连续两局基础可复用，计时、HP归零淘汰、观战、胜负与Results／自动返房均未实现。用户提出纳入下一步，已留[待确认草案](Networking/GF7_Match_Loop_Plan_CN_v0.1.md)：沿既定300秒／最后存活／超时剩余绝对HP规则，建议补统一服务器裁决、只读结果与WBP接口；并列／全灭、退场观战、8秒结算返房、Client弃权继续四组默认待用户确认。当前Playing中有人退出仍会全员返房，现有返回接口只接受Playing，需在实施中按阶段明确调整；Host淘汰与Host进程退出分开。没有据此启动C++修改，本轮仅方案／状态／Git白名单，无新编译或测试结论，未提交／Push。

**2026-11-04临时车顶血条完成代码、构建及相关回归，待正式场地真人扣血／双端画面验收。** 从干净`main/93f18e6`接续，此前车型技能接口与留档已提交，旧“未提交”按原时点阅读。按用户要求在原生车顶P1／P2绘制下增加68×10 Slate单位血条，文字水平居中并上移字体高度＋6单位；读取当前Pawn的Health组件／GAS CurrentHP与MaxHP，绿／黄／红分档，未就绪隐藏、0血空框，不新增复制字段／RPC／Tick或Widget资产。保留菜单及Designer布局排除规则，无需BP／DA接线。见[使用与验收](Presentation/MVPB_Temporary_Overhead_HealthBar_CN_v0.1.md)及[验证记录](Presentation/MVPB_Temporary_Overhead_HealthBar_Verification_20261104.json)。

本机UE5.8.3最终Editor／Game成功，现有Health回归2/2、D3D12原生UI流程1/1，均0测试警告／失败／未运行；不沿用此前134项或联机作为本次复测。旧UI夹具PracticeHUD截图实际为同帧打开的菜单，不冒充血条外观验收；历史7图逐份还原。本轮1183保存资产全同、289构建输入留档，唯一源码修改NativePaint所在文件，Config／Build.cs／uproject保持；未改BP／DA／地图，未提交／Push。下一步重开Editor看默认HUD，进入Playing再用球／机关扣血测试，练习房间仍抑制伤害；重新打包后再分发给组员。原技能DA迁移、手感参数与学校局域网／四机验收保持独立。

**2026-11-04车型技能配置完成代码、构建与程序验证，待团队保存DA并真人复测。** 用户确认Car01加速／Car02抗冲击，两车共用Primary按键。从`main/1d1c0c2`接续，保留开工前方案／状态／白名单改动；Definition新增显式Legacy／Vehicle Definition V1，按当前Pawn来源授予、整体校验、换车前预检与服务器／拥有者就绪检查。直接Avatar替换／退休／销毁均清旧来源，迟到清理不撤新车，普通重绑不刷新冷却，显式换车／新局沿用GF-2。见[实现与DA迁移步骤](Combat/MVPB_Vehicle_Ability_Loadout_Implementation_CN_v0.1.md)及[验证保护清单](Combat/MVPB_Vehicle_Ability_Loadout_Verification_20261104.json)。

本机UE5.8.3最终Editor／Game成功、**134/134原生，V1及原保存Legacy两套真实双端连续两局均各端1/1**，共5份最终自动化报告均0测试警告／失败／未运行。V1分别通过正常Primary／GAS RPC发动加速／免疫，校验唯一Spec、当前Pawn来源、拥有者冷却和跨图清理；V1出站80ms注入、60FPS／NullRHI、同机，不代表实测RTT／画面／四机。只读打包预检通过，严格迁移检查正确拒绝当前Legacy；初次Tag类型编译错误和旧夹具回归失败均独立保留并修正后重验。**1183保存资产逐份全同，289最终源码／配置／uproject冻结，7份Config／uproject／Build.cs保持**；专用进程已退出，未提交／Push。代理未修改BP／DA／地图，两份保存DA仍Legacy：团队将Abilities.Version设为Vehicle Definition V1，各填唯一Primary，Car01引用GA_MovementBoost、Car02引用GA_ImpactImmunity；全部迁移后再清旧PlayerState Primary，旧Legacy仍需时可保留。原手感DA／代理调整、真人技能验收、实际打包及四机／MVP-B集中收尾继续后置。

**2026-11-04打包前三项车辆手感调整完成代码、构建及程序验证，下一步团队调Gravity Scale／生成并引用新碰撞体后实测。** 从干净`main/d85a24d4f93fa55033ff28c0508775e5ea66d89a`接续，已含此前GF-6工具修正及团队独立复跑留档。用户确认共用车辆重力、侧面圆角和普通墙独立脱球门槛后授权：现有共享稳定性DA增加Gravity Scale（默认1，0.1～5）与墙门槛（默认600cm/s、60°），原生Chaos车辆组1保留球／世界组0，翻滚按有效重力估算，参数加入不可变签名／一致性校验；墙门槛仅补脱球资格，不改变档位、自然碰撞或追加响应，特殊来源／车辆／球及普通支撑保留原规则，超距释放增加可关闭原因日志。生成器增加XZ前后端上下圆角、侧视预览与v2配方／v1兼容，继续新资产／单凸包。见[配置与Collision Mesh引用步骤](Movement/MVPB_Prepackage_Gameplay_Polish_CN_v0.1.md)、[本轮验证／保护清单](Movement/MVPB_Prepackage_Gameplay_Polish_Verification_20261104.json)。最终UE5.8.3 Editor／Game成功，**132/132普通、5/5工具、2/2实际界面、1.5倍真实NetTest双端及房间连续两局双端各端1/1**，均0测试警告／失败；直接NetTest Client54次重模拟，Host出站注入80ms，不等于实测RTT／四机。**1183保存资产逐份全同，286源码／配置／uproject留档**，Config／uproject未改，Editor Build.cs仅新增GeometryCore依赖，物理历史格式3保持。只读确认当前共享DA Gravity Scale仍1.0，团队可试1.5；两车型Definition均引用`SM_Car01_Collision`，需手工生成新代理并在Definition→Physics→Collision Mesh切换，未代理保存BP／DA／地图。早期编译／探针断言及几何退化失败与修正如实留档。尚无新代理竖立概率、真实画面手感／全部坡面工况、实际打包或四机验收；GF-6／F4／VP-4C／FX余项独立。下一步先团队配置和基础反馈，再回实际Win64打包／四机；本轮未提交／Push。

**2026-11-04团队独立复跑确认：** 已核查用户提供的`Saved/GameFlow/Loop_20261104_103440`，Host17076／Client11876实际退出码均0，`GF4.TwoRounds`各1/1，0测试警告／失败／未运行／进行中；两端日志均`PlayingRounds=2 Identity=1 Fresh=1 Entry=1`，主机Round1返回房间后进入Round2，最终两端回入口退出。本次确认修正后的文档命令可在团队Windows PowerShell环境运行；仅同机Development自动化，不追加发布包／真人画面／四机结论。证据补入[工具修正验证](Networking/GF6_LoopTool_Compatibility_Verification_20261104.json)，下一步实际打包与四机验收保持。

**2026-11-04 GF-6循环工具Windows PowerShell 5.1误报已修复并实跑验证；当前仍待实际打包／四机验收。** 开工干净`main/a09b4551b0bbb6beebd2266eed09fc67a4221dc0`，已含此前GF-6工程收尾提交，旧“本轮未提交／main125395c”保留历史时点。团队原`Loop_20261104_101003`的Host／Client报告均1/1、0警告／失败，旧脚本因5.1重定向进程ExitCode为null而报`Unclean result: Host`，不是本次游戏流程失败。真实子进程复现并修正：立即保留Process.Handle，退出后排空输出，继续严格拒绝空／非零退出码及不合格报告；失败也保存全部角色Summary并打印具体原因／目录。文档改为子进程范围ExecutionPolicy Bypass调用。Windows PowerShell **5.1.26100.9711**与PowerShell **7.6.5**各6项工具夹具通过；5.1真实Development双端连续两局Host／Client各1/1、0测试警告／失败、实际退出码均0，工具退出0。详见[说明第4.1节](Networking/GF6_Engineering_Closeout_CN_v0.1.md#41-windows-powershell-51误报修正2026-11-04)、[本轮证据](Networking/GF6_LoopTool_Compatibility_Verification_20261104.json)。**1183资产、285源码／配置／uproject与用户原测试文件逐份全同**；仅工具／夹具／文档变化，无C++编译或新增130项／展示四进程／发布包／真人／四机结论。本轮修改未提交／Push，旧GF-6功能证据保持；下一入口仍为实际Win64 Development包和四台有线PC验收。

**2026-11-03 GF-6工程收尾完成代码、构建及程序验证；下一步实际打包／四机验收，GF-6整体尚未验收结束。** 从干净`main/125395cb4464654f9f6374f7f1b9e70e2e2ae50e`接续，已含此前GF-5 Widget接口提交，旧“未提交／下一GF-6未开始”按原时点阅读。新增服务器`ReturnActiveRoundToRoom()`共用返回接口；Development保留手动返回，Exhibition默认拒绝，仅非Shipping主机启动带`-HALAllowTestRoundEnd`时开放原按钮／命令，保持Host／阶段／版本／重复请求校验。补PracticeRoom／Match显式Cook地图及Gameflow窄目录根、只读依赖预检、可复跑双端／四进程循环工具。详见[工程收尾与操作步骤](Networking/GF6_Engineering_Closeout_CN_v0.1.md)、[本轮验证／保护基线](Networking/GF6_Engineering_Verification_20261103.json)。UE5.8.3最终Editor／Game成功，**130/130普通回归、Exhibition同机四进程连续两局各1/1、Development保存BP双端两局各1/1、D3D12原生UI流程1/1**，测试报告均0警告／失败；只读预检成功，86个项目依赖包存在，GA／GE及两车资源可追踪。**1183保存资产SHA-256逐份全同，285源码／配置／uproject留档**；Config仅新增3条Cook根，Build.cs／uproject未改，无代理保存BP／DA／地图。Exhibition夹具使用原生GameMode／PlayerState配保存DA，不冒充团队BP技能／WBP接线或四台实体PC；当前三项Screen类仍None，原生回退。未执行UAT Cook／Stage／实际发布包、完整扰动／突然断线或真人硬件验收；没有正式比赛计时／淘汰／观战／结算，F4／VP-4C／FX余项独立。失败预检迭代如实留档，专用进程均退出；本轮修改未提交／Push，需正常Git提交同步。

**2026-11-03 GF-5 Widget Blueprint接口补充完成代码、构建及程序验证，当前原生布局作为未配置时回退，团队可开始制作自定义界面。** 从干净`main/35aa00c41566b5ba2161445574b61950006a024b`接续，原GF-5已提交。用户授权新增入口／房间／独立暂停页的可配置类，引用有效Widget Blueprint时使用其Designer WidgetTree，清空回原生；暂停未配时沿用Room页面。补只读Entry／Room／Seat／Vehicle显示快照、初次及变化通知、语义按钮请求、嵌套焦点与UMG导航／子面板返回处理，保留C++输入、权限和跨图清理；旧入口开关／MenuWidgetClass不再参与新选择。修正已聚焦时SetUserFocus返回false的处理，并在房间状态变化时立即同步输入／界面。详见[完整Blueprint接线步骤](Networking/GF5_Widget_Blueprint_Interfaces_CN_v0.1.md)、[本轮验证及保护基线](Networking/GF5_Widget_Interfaces_Verification_20261103.json)。最终UE5.8.3 Editor／Game成功，**129/129普通回归**、D3D12 1280×720内存UMG完整流程及原生回退分别**1/1**、原生Host＋自定义UMG Client真实双端连续两局各**1/1**，均0测试警告／失败；原生Playing输入断言保留并通过。**1183保存资产逐份全同，285源码／配置／uproject哈希留档**，Config／Build.cs／uproject未改，没有代理制作或保存BP／DA／地图。内存UMG夹具不冒充实际新编译Widget Blueprint／真人硬件验收；本轮未提交／Push，专用进程已退出。阻塞跨World加载幕仍为原生Slate；实际Blueprint Designer／按钮／动画／嵌套导航、硬件／失焦／打包／四机及原F4／VP-4C／FX余项待团队。下一开发仍为GF-6，尚未开始；下段保留原GF-5交付时点，第2／6节为当前入口。

**2026-11-03 GF-5街机入口／选车表现完成代码、构建及程序验证，待团队硬件与素材初测；下一开发环节GF-6。** 本轮接续干净`main/817e443e48bc8647862ff9ba74eb9c5098f93df9`，已含GF-4实现、正式MVPB_Match和两份Room DA更新。用户报告GF-4初步测试暂无问题、后续继续更多测试，记为基础正向反馈，不追加细项／网络工况／四机结论。新增默认可用的原生待机／Host／IPv4 Join入口、四席位与静态车型预览卡、可选品牌DA／Catalog图片说明、本地键鼠／手柄焦点、菜单与车辆IMC切换、加载／倒计时及可读错误；保留服务器权威与物理协议3。修正取消Join残留PendingNetGame、隐藏页面焦点、长名裁切和菜单编号穿透，房满／已开始分别提示。详见[GF-5实施与配置](Networking/GF5_Arcade_FrontEnd_Implementation_CN_v0.1.md)、[验证与保护基线](Networking/GF5_Verification_20261103.json)。UE5.8.3最终Editor／Game成功、**127/127普通回归**、**实际D3D12 1280×720 UI流程1/1**、使用保存正式地图／DA的真实双端连续两局各1/1，均0测试警告／失败。UI通过真实Slate焦点注入键盘与手柄A／B，测试进程隔离额外XInput事件，不冒充物理硬件验收。**1183保存资产逐份全同，278源码／配置／uproject哈希留档**，Config／Build.cs／uproject未改；未保存BP／DA／地图或提交／Push，专用进程已退出。原生界面无需先改WBP或新增GA／GE；品牌DA及真实预览图可选手配，真人按住键／失焦／多窗口、全部分辨率／图片分支、PIE／打包／完整扰动／四机仍待补。GF-6、原F4／VP-4C／FX余项独立记账；下段保留GF-4原交付时点，第2／6节为当前入口。

**2026-11-03 GF-4真实跨图／全员就绪／同步倒计时完成代码、构建及程序验证，待团队正式地图和DA接线／双端初测；下一开发环节GF-5。** 用户确认GF-3测试暂无问题，记为基础初测正向反馈。本轮接续`main/2e7d72c826abf26c33da4b688abccf63aba8e182`，保留GI修改及新建PracticeRoom地图／GameMode BP；已只读确认Host入口指向PracticeRoom及现有BP_HALPlayerState技能。新增按目标World一次性消费的纯值交接、跨World身份／生命周期计数、同车型新Pawn、新局初始化、带Flow／Round／Token／实际Avatar验证的客户端就绪屏障及服务器倒计时；加载失败／超时或Client离房返回房间，Host离开回入口，提供Development返回房间验证第二局。普通重绑、伤害／冲击解耦及物理协议3保持。详见[GF-4实施与详细Editor步骤](Networking/GF4_Round_Travel_Implementation_CN_v0.1.md)、[验证与保护基线](Networking/GF4_Verification_20261103.json)。本机UE5.8.3最终Editor／Game成功，**125/125普通回归**，真实双端连续两局／超时／Client离房／Host离房，以及GF-3／GF-2兼容六组均各端1/1，0测试警告／失败；Listen单Host与D3D12 Standalone两局也通过，1280×900倒计时画面已检查。**1182保存资产逐份哈希全同**，266份最终源码／配置／uproject留档，Config未改。新增Round Travel默认关闭，团队须按第4节保存独立正式图并配置两份Room DA；GI Host目标继续指练习房。GF-4真人接线、PIE／打包、真实硬件／突然断线／完整扰动与四机仍待验收；无正式比赛计时／淘汰／结算，GF-5／6及原F4／VP-4C／FX余项继续独立。早期失败及修正保留，本轮未提交／Push，验证进程已退出。下方旧段落保留各自历史时点，第2／6节为当前入口。

**2026-11-02 GF-3灰盒房间／选车／练习完成代码、构建和程序验证，待团队专用资产接线／真人初测；下一开发环节GF-4。** 本轮从干净`main/d1add4bf9e8b065f525ad4c1dd6c17042f30dc99`接续，已包含GF-2和三份Gameflow DA；只读确认Development Profile现已正确，不再列为待改。新增专用Practice GameMode：连接先选车，确认后校验车型／版本／席位出生点及真实物理根，生成所选Pawn、走显式生命周期及服务器就绪；取消先撤销Ready再清理旧车，失败可重试。原生灰盒界面提供四席位／身份色／编号、选车确认／取消、练习菜单及离房，车上方编号和球权光束消费同一身份色；原NetTest仍用旧模式。补齐GAS首次无车时Owner=Avatar=PlayerState的窄初始状态，错误Avatar仍拒绝。Client离房释放席位且Host继续练习，受控Host退出两端回入口并保留原因，修复teardown期间旧委托重建Widget。详见[GF-3实施与详细Editor接线](Networking/GF3_Practice_Room_Implementation_CN_v0.1.md)、[本轮验证／保护基线](Networking/GF3_Verification_20261102.json)。UE5.8.3最终Editor／Game成功，**122/122普通回归**（新增4项），GF-3 Flow／Client Exit两组真实独立进程双端均各**1/1**，GF-1／GF-2双端兼容复验也均各1/1，0测试警告／失败；最终D3D12 1280×900占位界面已看图。1180保存资产逐份哈希全同、260份源码／配置／uproject留档，Config未改，早期失败独立保留。**尚需团队按文档建立Room GameMode BP／独立房间地图并修改GI Host目标；当前保存GI仍指向NetTest。GF-3开始只到Locked，真实跨图／载入屏障／倒计时留GF-4。** 未新增真人鼠标／手柄、团队接线、突然断网、打包／四机或原F4／VP-4C／FX验收结论；本轮未提交／Push。下方GF-2等保持历史时点，第2／6节为当前入口。

**2026-11-02 GF-2换车／新局GAS生命周期与练习伤害策略完成代码及程序验证，下一步GF-3。** 本轮从`main/653320d27a66fe50135eef320dd36dec6fa7e863`接续，GF-1已提交；保留团队新增的三份Gameflow DA。新增服务器显式换车／新局接口、单调操作序号与Round／Avatar复制身份，以新Pawn隔离物理历史；旧车退休时清理球权、来源能力／效果、区域及输入，重新应用正确车型种子，普通重复绑定仍不回血或清冷却。标准Resolver在练习阶段抑制扣血，冲击／脱球／命中消费保持独立。详见[GF-2实施与接续调用](Networking/GF2_Vehicle_Lifecycle_Implementation_CN_v0.1.md)、[验证与保护基线](Networking/GF2_Verification_20261102.json)。本机UE5.8.3最终Editor／Game成功，**118/118普通回归**（新增4项）、GF-2真实NullRHI双端各**1/1**、既有GF-1双端兼容复验各**1/1**，均0测试警告／失败；A→B→A→新局Round1的复制绑定、属性／效果清理及新Epoch有证据。早期编译错误与117/118夹具漏开冲击分别留档，修正后最终通过，没有降低断言。**1180保存资产逐份哈希全同**，254份源码／配置／uproject留档，Config未改、专用进程已退出。只读核对两份Room的Catalog及Car_01／Car_02有效，**磁盘上的`DA_Room_Development → Rules → Profile`仍是Exhibition，待团队改为Development并保存**。本轮没有修改资产，三份新DA仍未跟踪，当前`main/653320d`加未提交GF-2源码／文档，未提交／Push。GF-3至6未开始；现用NetTest的Room Config继续None，不将原生生命周期测试当作完整选车UI、生车编排、真实跨图、真人／四机验收。下方GF-1／GF-0及旧“未开始”段保持历史时点，第2／6节为当前入口。

**2026-11-02 GF-1数据与权限基础完成代码及程序验证，下一步GF-2。** 用户授权后，以`main/eadd240`为基点实现车型目录／房间配置、PlayerState席位／颜色／选车版本及具体ReadyAvatar、公用房间快照／冻结名单／RoundId、仅操作本人Controller的选车／确认／取消／Host开始契约；默认Room Config为空，旧NetTest继续原流程。新增统一权限查询，驾驶阻断沿既有权威物理快照及历史，技能／观察／自救／控球／电墙／标准命中阶段门控消费对应查询，未改变协议3或清除GAS初始化保护。详见[GF-1实施与数据资产准备](Networking/GF1_Data_And_Permissions_Implementation_CN_v0.1.md)、[验证与保护基线](Networking/GF1_Verification_20261102.json)。本机UE5.8.3最终Editor／Game成功、**114/114普通回归**（新增6项）、真实NullRHI独立进程Host／Client各**1/1**且0测试警告／失败；两端同车型不同席位／颜色、非Host开始拒绝、Ready物理解锁及锁房再次阻断（Revision4）均有证据。首轮缺头文件、测试发布时序误判与数组自引用断言独立留档，修正后通过；另补PlayerState EndPlay清席位并重做最终两Target／114项／双端。1177保存资产逐份哈希全同、248份源码／配置／uproject留档，保留开工已有上轮进度同步文档；当前`main/eadd240`加本轮未提交代码／文档，专用进程退出，未提交／Push。**GF-1不生车、不做UI或转场；练习HP策略只定义查询，实际统一结算留GF-2。** 下一步换车／新局GAS生命周期及练习伤害独立性，再按GF-3起接生产房间；当前不替换回归地图Room Config。本次完整构建包含既有FX源码，但没有追加其Niagara画面／真人／四机或完整扰动通过结论。下方GF-0及“GF-1未开始”等保留历史时点，第2／6节为当前入口。

**2026-11-02本机进度接续复核：房间／进入流程方案已明确，仍处于GF-0完成、GF-1待施工。** 当前实际为干净`main/eadd240a67f6c3c83857ecf5011e22f29a145e8e`；该提交已收录流程v0.1、v0.2、Git白名单和状态恢复记录，不再把下方“恢复尚未提交／流程文档未跟踪”当作当前状态。本机读取`D:/Program Files/Epic Games/UE_5.8/Engine/Build/Build.version`为**5.8.3-58210709**，旧5.8.2记录保持原设备／时点含义。复核当前源码：Host仍直接打开配置地图，MatchPhase仍仅WaitingForPlayers／Playing，尚无车型目录、房间选车／练习状态或全员跨图开赛实现，与GF-1至GF-6未开始一致。当前执行依据继续为[v0.2确认基线](Networking/Room_VehicleSelection_Practice_Match_Construction_CN_v0.2.md)：六项规则已经确认，下一步GF-1数据与权限基础，没有新的玩法确认阻塞。本次只同步本文件，没有修改C++／Config／保存资产，没有新编译／自动化／真人验收，也不补记受电／球权FX或四机通过结论；本次文档更新未提交／Push。

**2026-11-02房间流程六项确认及GF施工入口已恢复；GF-0留档完成，下一步GF-1。** 用户确认允许重复车型并用颜色／编号区分、允许取消确认、不另设准备按钮、练习不扣血、展示四人／开发单人完整流程，以及开始前允许加入／开始后拒绝／Host离开全员回入口。执行依据为[v0.2确认基线与分段施工](Networking/Room_VehicleSelection_Practice_Match_Construction_CN_v0.2.md)，依次为GF-1数据与权限基础→GF-2换车／新局GAS生命周期→GF-3灰盒房间／选车／练习→GF-4统一转场／全员就绪／倒计时→GF-5街机入口与选车表现→GF-6退出返回／连续两局／打包与四机验收。GF-1至GF-6均未开始；每段分别记录代码、构建及测试，不一次执行整个流程阶段，没有新的必须阻塞GF-1的玩法确认项。

用户因冲突丢弃了Git白名单与状态文件的本地更改，本次按仍保留的v0.1／v0.2文档及此前确认恢复这些入口。当前实际`main/256be6f`，已包含其后提交的受电／球权光束等更新；原规格记录时的`0486607`及下方旧分支记录保留历史意义。本机D盘UE5.8.2-56702186。本次只恢复`.gitignore`两条流程文档白名单、状态与接续链接，保留远端新增FX记录及其他内容，不修改两份规格正文、C++、配置或保存资产，没有新构建／测试，不追加FX或四机通过结论；恢复尚未提交／Push。

**2026-11-02 球權歸屬識別（車頂磁力光束）已完成 C++ 與資產，尚未編譯。**（本段以繁體撰寫，下方既有各段維持原簡體，未改寫他人段落。）球被持有時幾乎貼合車頭（球半徑 50cm、車體 Box Extent (100,70,35)），水平方向沒有畫特效的空間，故歸屬識別改走垂直方向：由持有者車頂拉一道弧形光束落到球上。相機為 `FRotator(-60,0,0)`，60° 仰角下垂直方向在螢幕上仍佔實際長度的一半，且車身上方是空螢幕區。新增 `HALMagnetBeamGeometry.h`／`.cpp`（二次 Bezier 加螺旋偏移，弧頂方向**鎖世界 +Z** 而非車體區域上方，否則翻車時光束會朝地裡打；純函式，不碰 World）與 `UHALMagnetBeamPresentationComponent`（`Idle`／`Attach`／`Hold`／`Release`／`Launch` 五階段，`Release` 從球端收回、`Launch` 從車頂端塌縮，只在執行中開 Tick、`TG_PostPhysics`）。元件掛在 `ABasicBallActor` 而非車輛上：驅動它的 `Controlled` 與 `Holder` 已在球上複製、`OnRep` 就是現成事件鉤子，所有車輛自動生效（受電特效掛在車輛藍圖上導致四個車輛藍圖至今為空、被電沒有任何表現，正是這個代價）。雙路接線：權威 `CommitRepState` 與客戶端 `OnRep_BallRepState` 各呼叫一次 `SyncToBallState`，因為 `ApplyReplicatedPresentation()` 開頭就是 `if (HasAuthority()) return;`，Listen Server 主機玩家否則是唯一看不到球權的人。球端錨點用 `GetDisplayedRootPose()` 而非物理姿態，因為 `Controlled` 時客戶端由持有者相對樣本顯示球，用物理根算出的光束會打不到玩家看得見的球。第二層球殼染色脈衝與光束同色同相位，走球材質的 `HolderColor`／`HolderPulse` 參數，參數不存在時為無操作。顏色按 `PlayerId` 取模索引 `HolderPalette`，是窗口極窄的適配層：工程裡**沒有**玩家配色系統，將來接入正式配色時 `ResolveHolderColor()` 是唯一需要改的地方；調色盤刻意不含藍色，藍色已表示「被電」。新增診斷開關 `hal.MagnetBeamLog`。

**資產：User Parameter 名稱為 `Beam*`，專用 Ribbon 材質已產生。** 團隊已在 `NS_BallMagnetBeam` 上完成改名，已核對存檔後的 `.uasset`：`BeamPositions`／`BeamWidths`／`BeamPointsPerRibbon`／`BeamRibbonCount`／`BeamAlpha`／`BeamColor` 六個全部存在，`Arc*` 殘留為 0，C++ 讀的就是這六個名稱。期間曾因回報「名稱鎖住無法改」而一度把 C++ 改成讀 `Arc*`，團隊改名成功後已改回，此事僅留作經過。原先要求刪除的 Sprite Renderer **在這份拷貝裡並不存在**（`Render` 分組下只有 `Ribbon Renderer`），該步驟作廢。新增 `M_FX_MagnetBeam_Ribbon`，由 `Tools/UnrealBridge/build_magnet_beam_ribbon_material.py` 經橋接在執行中的 Editor 產生（`failures: 0`、155 條像素指令），並已回讀核對：Additive、Unlit、two_sided、`used_with_niagara_ribbons=True`，七個標量參數預設值符合預期。關鍵設計差異：**沒有白熱芯**（Additive 會把超過 1 的部分裁成白色，而光束唯一的工作就是表達持有者顏色），改用柔和管狀截面加一道沿線流動的磁通帶（`FlowSpeed` 正值＝流向車頂，讀作「吸住」）。這份材質有可重現的產生腳本，但按使用者決定，**橋接僅在本機使用、永不入庫**（`Tools/UnrealBridge/` 已列入 `.gitignore`，`Config/DefaultEngine.ini` 的 Python 區塊也不提交），所以換設備時只能直接用這份 `.uasset`。

**本輪未完成的驗證（不得當作通過）。** UHT 已跑通（`7 generated files written`），說明反射巨集正確；但 **C++ 編譯根本沒有開始**：UBT 因執行中的 `UnrealEditor` 與 `LiveCodingConsole` 而拒絕（`Unable to build while Live Coding is active`、退出碼 6）。因此**沒有編譯通過證據、沒有跑自動化、沒有任何真人畫面驗收**，材質也只有屬性回讀這一層證據。連帶後果：`MagnetBeam` 是 C++ 建構式建立的元件，執行中的 Editor 載的是舊 DLL，所以在重新編譯並重開 Editor 之前，`BP_BasicBall` 的元件列表裡**不會出現**`MagnetBeam`，也就無法指派 `Beam System`。團隊需先關掉 Editor 與 Live Coding Console 再重編 Editor Target。Editor 製作與接線步驟見[球權磁力光束特效步驟](Presentation/MVPB_BallMagnetBeam_FX_Authoring_CN_v0.1.md)，其中仍未完成的是：**取消勾 Local Space**（本特效推世界座標，這是與受電弧光唯一的結構性差異）、Burst 對上 63 個粒子、Ribbon Renderer 指派新材質，以及編譯後到 `BP_BasicBall` 指派系統。地面共享光環與車頭磁爪按討論結論後置。本輪未提交／Push。

**2026-11-01打开车辆Blueprint崩溃已修复。** 团队打开`BP_TestVehiclePawn`即崩溃，`Saved/Crashes`两次堆栈相同：`ATestVehiclePawn::PreInitializeComponents()`第788行`EXCEPTION_ACCESS_VIOLATION reading address 0x0`。蓝图编辑器会在EditorPreview世界生成预览Actor并走`PreInitializeComponents`；预览车没有有效`Definition`，`ApplyDefinition(false)`返回false后进入“配置无效即关闭物理”分支，对空的`CollisionRoot`做虚函数调用。该分支此前从未在预览世界被执行过，且预览Actor原本跑的是非预览路径（内含`verify(...)`）。`TestVehiclePawn`／`PassiveTestVehicle`／`BasicBallActor`三处为同一份复制代码，一并修正：非游戏世界直接早退（预览视觉仍由既有`OnConstruction` → `ApplyDefinition(true)`负责，游戏世界行为不变），并对`CollisionRoot`／`PhysicsRoot`／`ImpactReceiver`加判空与错误日志，真正缺元件时打印而非崩溃。本机UE5.8.2-56702186 **Editor Target构建成功**（`Result: Succeeded`、退出码0），`HAL.FutureCreation`自动化**108/108通过，0失败／0警告／0未运行**，报告在`Saved/Automation/PreviewCrashFix_20261101`。只有`BP_TestVehiclePawn`会崩溃的原因已查明：四个车辆BP中只有它没有指派`VehicleDefinition`，其余（`BP_RoundedVehiclePawn`→`DA_Vehicle_Player_TestBaseline_v1`、`BP_VehiclePawn_Car_02`→`DA_Vehicle_Car_02`、`BP_PassiveTestVehicle`→`DA_Vehicle_Passive_TestBaseline_v1`）都不会进入该分支。**预览Actor的`CollisionRoot`为何为空仍未定位**；早退使该问题在编辑器内不再有影响，若游戏世界也发生，新日志会给出。**团队尚未重开Editor确认BP可以打开**，修复只完成代码与自动化，不等于实测通过。本轮未提交／Push。

**2026-11-01车身受电特效改为「C++算几何、Niagara只画线」，已完成代码、编译与团队部分肉眼验收。** 当日稍早的`Arc System`方案（复制`NS_ElectricArc`为`NS_VehicleShockArc`）已废弃，下一段保留作历史。现行实现三层叠加：(1) 车身overlay材质，用`UMeshComponent::SetOverlayMaterial`挂在运行时实际可见的网格上——CDO显示`VisualMesh`可见、`SkeletalVisual`隐藏，运行时相反，已用`hal.ShockedPresentationLog 1`确认挂到`SkeletalVisual`；(2) 英雄弧光，新增`HALShockArcGeometry.h`／`.cpp`在C++用中点位移算折线，端点落在车体extent内接椭球上、按`BranchChance`分叉，整条经`UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition`／`SetNiagaraArrayFloat`推给不含任何逻辑的Ribbon emitter；(3) 可选点光源。`UHALShockedPresentationComponent`改为`Idle`／`Active`／`Release`／`Cooling`四阶段时间轴，只在运行中开Tick，由`State.Visual.Shocked`标签经`RegisterGameplayTagEvent`事件驱动（非轮询），在`ATestVehiclePawn::InitializeAbilitySystem()`以`ShockedPresentation->Bind(ASC)`接上、`UninitializeAbilitySystem()`解绑。新增诊断开关`hal.ShockedPresentationLog`。

**本机UE5.8.2-56702186 Editor Target构建成功**，产物`Binaries/Win64/UnrealEditor-HAL_Future_Creation.dll`时间11-01 19:39，晚于最后一次改码19:38。**本轮没有重跑自动化**：`Saved/Automation`最新两份为00:17的`VehicleShockArc_20261101`与02:51的`PreviewCrashFix_20261101`，均早于本轮改动，不得当作本轮证据。团队肉眼已确认的只有英雄弧光会出现、且**蓝色符合要求**；**尚未确认**的有两项——弧光「由尾到头收回」（`bRetractArcsOnRelease`／`ArcRetractFeather`）没有得到明确回报，overlay那层的焦黑与电流细丝**没有人在游戏里看过**，目前只有日志证明材质确实挂上、离线截图证明该材质在真实时间轴数值下会画出来。

**资产范围与已知缺口。** 已接线的只有`BP_RoundedVehiclePawn`（指派`M_FX_VehicleShock_Overlay`与`NS_VehicleShockRibbon`）；`BP_TestVehiclePawn`／`BP_TestVehicle`／`BP_DrivingLabVehicle`／`BP_VehiclePawn_Car_02`四个车辆BP的ShockedPresentation仍为空，被电不会有任何表现。`NS_VehicleShockRibbon`内团队手搭时留下的Sprite Renderer未删，每次多画136个无用sprite，不影响正确性。元件内废弃的`ArcSystem`／`ArcOffsetCm`／`ArcScale`仍在`Shocked|Arc FX`分类下并留空，留空时不生成元件。按用户决定，本次**不入库**Python桥接通道（`Config/DefaultEngine.ini`的`PythonScriptPlugin`区块与`Tools/UnrealBridge/`四支脚本）与两份FX制作文件（被`.gitignore`第5行`*.md`挡住，需加白名单才能收），因此`M_FX_VehicleShock_Overlay`与`M_FX_ShockArc_Ribbon`是**成品入库、没有可复现的生成脚本**，换设备只能直接用资产或手工重建。废弃路线的`NS_VehicleShockArc`、无任何引用的`M_FX_Spark_Additive`以及`M_FX_Beam_Additive`的本地改动一并不入库，`M_FX_Beam_Additive`保持仓库版本以备两柱电弧日后指派。以下各段保留各自历史时点，以本段及第2／6节为准。

**2026-11-01车身受电电弧的C++接线完成代码、编译与本机回归。** 用户要求制作车辆被电墙电到的电弧特效，并把需要本人操作的部分整理成步骤清单。本会话没有Unreal MCP，无法操作Editor或改写`.uasset`，故只做C++接线与留档。`UHALShockedPresentationComponent`新增`Arc System`／`Arc Offset Cm`／`Arc Scale`三项`EditDefaultsOnly`，由既有`State.Visual.Shocked`标签（与Stun同生命周期，电墙`Stun Seconds`默认1秒）驱动`Activate`／`Deactivate`；特效以`SpawnSystemAttached`挂在Pawn的Root（`VehiclePhysicsBody`）跟随权威物理姿态，元件只建立一次并重用（连续受电不叠加），结束用`Deactivate`让已生成粒子播完，headless／NullRHI／自动化不生成元件且`IsShocked()`仍可读。本机UE5.8.2-56702186首轮构建被运行中的`UnrealEditor`与`LiveCodingConsole`按设计中止，用户关闭后重编**Editor Target成功**（30项任务、两个DLL连结，`Result: Succeeded`、退出码0）；`HAL.FutureCreation`自动化**108/108通过，0失败／0警告／0未运行**，含8项ElectricWall（`ShockedFollowsExtendedStun`在内）。既有`HALCameraTests.cpp`的C4305警告保持，本轮改动文件无新警告。

**本轮没有制作Niagara资产、没有指派到任何车辆Blueprint、也没有真人画面验收。** `Arc System`留空时不生成任何元件，故当前游戏内仍看不到车身电弧，编译与回归通过不等于特效已上线。团队手动步骤见车辆受电电弧特效文件（`Docs/Presentation/MVPB_VehicleShockArc_FX_Authoring_CN_v0.1.md`，按用户决定未入库）：复制既有`NS_ElectricArc`为`NS_VehicleShockArc`、按车体`Box Extent (100,70,35)`改端点与Jitter／段长比例、确认`Local Space`勾选／`Absolute Beam Start`不勾／`Loop Behavior = Infinite`三项、Ribbon Renderer指派`M_FX_Beam_Additive`（现仍为引擎预设`DefaultRibbonMaterial`），再到各车辆BP的ShockedPresentation元件指派。本轮未提交／Push。**本段方案已于同日被上方「C++算几何、Niagara只画线」一段取代，仅保留作历史。**
**2026-10-30中間電牆Client高速回彈二次抖動修正完成代碼、構建與原生雙端驗證，待真人複測。** 本輪實際基點`main/0486607`，開工工作區乾淨，接續保留上次暫停的10份源碼改動；本設備實際UE5.8.2-56702186，其他設備5.8.3與舊分支記錄保持各自歷史意義。長距離真實Throttle雙端夾具曾在2391cm/s重現一次額外反向脈衝與57.332cm位置偏差：確認後過早禁止原預測重播，回滾先恢復向前再套用權威回彈。現將確認解除等待與否決禁止重播分離，保留epoch隔離；掃描改讀物理姿態、補查掃掠進入點，動作消費時以該點補回深入距離（總距離上限1000cm）。柱子繼續普通牆碰撞，Stun／shocked／球權仍Host權威，未改全局配對窗口、保存資產或引擎。

最終**Editor／Game構建成功、普通109/109，真實高速及誤判否決兩組Host／Client各1/1**，最終5份報告均0測試警告／失敗／未運行；既有相機C4305與啟動警告保留。最終實駕峰值2389.902cm/s，額外反向脈衝0、再加速0、最大單幀位置殘差0.445cm、向牆校正累計0.007cm；先前接續版另一次2399cm/s也通過。條件為NullRHI／60FPS、Host出站150ms±40ms與10%丟包，非實測RTT／真人畫面驗收。早期高速失敗、setup超時及人工4000cm/s不一致速度夾具的校正均保留，不冒充連續性通過。1172份保存資產相對本輪基線逐份SHA256全同，最終專用程序已退出。動作載荷新增欄位，兩端須同版完整編譯；History序列化格式保持。

下一步可重開Editor／雙端，在正式地圖以原先速度重測中間電牆，並測低速、斜撞、倒車、柱角、騰空、連續進出與多牆；一般權威校正仍可能發生，不承諾任意網況零抖動。詳見[修正說明第8節](Combat/MVPB_Electric_Wall_CN_v0.1.md#8-client高速回彈二次抖動修正2026-10-30)與[本輪完整證據](Combat/MVPB_ElectricWall_Jitter_Verification_20261030.json)。本輪未提交／Push，其他MVP-B與F4欠項保持。

**2026-10-30 Car_02输入不响应已定位为车型Impact未迁移。** 当前实际`main/781da01`已包含上轮预览及统一构建提交，保留团队`BP_MVPB_ListenGameMode`未提交修改。16:12:53和16:13:54两次Car_02日志均已读取本车型Definition及其Local Config，随后明确报`TieredV1 rules require migrated VehicleDefinition.Impact`；Car_01在16:14:29使用既有玩家DA并开始正常物理步进。Car_02保存包未覆盖Impact，沿用Legacy原生初值；共享规则要求TieredV1，初始化将车辆配置标无效、关闭物理并跳过输入绑定，故共享Local引用正确仍不能驾驶。

当前下一步为团队在`DA_Vehicle_Car_02 → Impact → Version`手动设置TieredV1、保存并重新Play验证，无需重新编译。生成器仍沿用原生Legacy初值，前轮Legacy临时夹具没有覆盖此保存车型门槛；本轮为只读诊断，尚未修改生成器、Blueprint或DA，没有新构建／自动化／修正后驾驶结论。详见[诊断与手工修正](Configuration/Vehicle_Input_Preview_CN_v0.1.md#2026-10-30-car_02输入不响应诊断)及[证据](Configuration/Car02_Input_ConfigDiagnosis_20261030.json)。以下各段的分支和未提交状态保留各自历史时点，以本段为当前入口。

**2026-10-30统一构建入口与生成蓝图引用预览收尾核对完成。** 当前仍为`main/945e616`，保留现有未提交改动及用户删除`Build_Editor.cmd`的状态。现有Editor模块已注册只读Input预览，已有Car_02和之后生成的车型直接读取`Definition.LocalConfig`中的来源及十项输入引用；运行初始化仍从同一共享Local读取。统一入口补回旧脚本的`-NoHotReload`并显式带`-NoLiveCoding`，普通构建及`-Rebuild`均覆盖Editor模块，无需另一个重构脚本。没有复制输入默认值或修改保存的Blueprint／DA／地图。

本轮主工程UE5.8.3-58210709经统一入口完成29项编译／链接／元数据任务，UBA启用时在收尾阶段未退出，三个专用构建会话已结束；随后以相同完整构建参数加`-NoUBA`调用引擎Build.bat，确认主工程Target最新并返回`Result: Succeeded`、退出码0。临时TMP／TEMP及UBA_ROOT仅作用于验证进程，未改保存构建配置或引擎文件。新增禁用热重载断言后的脚本隔离检查重新**13/13通过**；218份主Source与已成功构建的隔离快照相同，1172份保存资产逐份SHA-256全同。既有相机C4305及UBA缓存提示保留。UE启动重试仍无日志／报告，**六项UE专项仅确认已编译，实际Input界面及保存车型驾驶尚未验收**。

当前下一步为重开Editor，在已有或新生成BP的Class Defaults → Vehicle → Input核对共享来源及引用，并在Session Frontend运行`HAL.Editor.VehicleInputPreview`和`HAL.Editor.VehicleAssetGenerator`。新车型碰撞、表现／AnimBP与两份DA的Ready校验保持原流程，预览正确不能替代车型玩法验收。详见[引用显示与本轮验证](Configuration/Vehicle_Input_Preview_CN_v0.1.md#2026-10-30主工程收尾核对)及[结构化证据](Configuration/Vehicle_Input_Preview_Verification_20261030.json)。本轮未提交／Push，其他开发与团队验收欠项保持。

以下同日早先记录保留各次验证时点，当前主工程构建结果以上段为准。

**2026-10-30生成蓝图输入引用显示代码补齐，隔离Editor构建通过。** 仍基于`main/945e616`，保留已整合的BuildAndLaunch、说明／测试改动及用户删除Build_Editor.cmd的状态。此前当前源码缺少车辆Details定制，本轮在原Editor模块加入只读Input预览及注册：从实际Definition.LocalConfig直接显示来源和十项输入，不向Pawn复制默认值；已有Car_02及生成器之后创建的车型均适用，Legacy／游戏世界保持原行为。生成器原有四份资产引用链不变，补充预览来源断言与临时有效配置下的生成蓝图运行初始化测试；没有保存或修改原始蓝图／DA／地图。

本机实际UE5.8.3-58210709在工程内独立快照完整构建Editor Target成功，最终新增测试源已显式触发编译并完成链接；218份主Source与最终快照哈希相同，1172份原保存资产前后逐份哈希全同。原C4305及UBA结果存储提示保持。**六项相关UE测试只完成编译，Cmd／普通Editor离屏启动均无日志／报告，不能确认执行或标记通过；实际Input界面、保存车型玩法校验及真人驾驶尚未本轮验收。** 此结果不替代主工程在关闭Editor后由统一入口生成并加载新版DLL。详见[读取位置、生成／运行链与接续步骤](Configuration/Vehicle_Input_Preview_CN_v0.1.md)及[证据](Configuration/Vehicle_Input_Preview_Verification_20261030.json)。两份新生成DA的Ready和车型配置门槛保持；当前下一步为保存退出Editor后运行BuildAndLaunch，打开已有或新生成BP检查Input并运行两组专项。本轮未提交／Push，自救调参／F4／四机欠项保持。

**2026-10-30构建脚本重新整合完成。** 当前实际基点`main/945e616a9d03ba776f1def0b69a52a36b1af700c`，开工工作区干净；本机实际UE5.8.3-58210709，下方5.8.2为先前设备验证。根目录BuildAndLaunch.cmd实际仍为旧MSBuild流程，已重新整合为独立单文件入口：直接Build.bat构建Editor Target，包含引擎／目标／占用检查、真实退出码、日志／Result.json、NoLaunch／CheckOnly／Rebuild／Pause及成功后启动／早退检查。旧Build_Editor.cmd保持原样供用户删除，Tools/BuildEditor.ps1仅转发NoLaunch，统一入口不依赖二者。Windows PowerShell5.1语法、**13/13新一轮独立模拟**、本机路径CheckOnly通过；没有执行本轮真实Unreal编译、启动或玩法验收，不使用旧测试冒充本次运行。只修改脚本／测试及相关文档，Source／Config／Content保持；未提交／Push。详见[当前用法](Configuration/Build_Editor_Tool_CN.md#2026-10-30重新整合后的当前入口)与[本轮验证](Configuration/Build_And_Launch_Reintegration_Verification_20261030.json)。自救保存参数／真人复测、F4及四机欠项保持原范围。

**2026-10-29自救角速单位澄清。** 团队截图末行Maximum Recovery Angular Speed显示229.183121deg/s，对应之前只读内部4rad/s；较快建议内部6rad/s应在当前deg/s输入框填写约343.774677，不填裸数字6。前步口头说明未注明显示单位，已补[配置表与输入方法](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md#92-团队editor调参步骤)。截图Minimum Recovery Tilt与Drive Exit Tilt均为65°，该角度关系现已正确；是否已保存并恢复实际驾驶仍待团队确认，不以截图冒充运行验收。本轮仅改说明，无C++／资产修改、构建或新测试，原诊断JSON和验证结果保持各自时点。

**2026-10-29调参后Host无法操作已定位为保存配置无效，待团队改值复测。** 最新实际游戏日志先报Invalid Recovery／Vehicle blocked，随后已进入Playing；只读当前共享DA发现Minimum Recovery Tilt=60°小于Drive Exit Tilt=65°，导致共享规则初始化主动阻止车辆。团队按[自救9.4节](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md#94-调参后host无法操作保存配置诊断2026-10-29)将Minimum Recovery Tilt改为65°并保存、新开局；Maximum Recovery Angular Speed仍为4，如使用较快组合另改6，与启动阻塞原因独立。无需新编译或改输入。保留本地Stability／Local两份DA和前步源码／文档，代理本轮无资产或C++修改；只读诊断1/1，不将其记为无效DA通过校验、恢复驾驶或真人自救验收。新证据见[配置诊断](Movement/MVPB_Vehicle_Recovery_ConfigDiagnosis_20261029.json)，原104项与双端结果保持原时点。

**2026-10-29侧翻自救修正已完成代码、构建与自动化，待团队调参／真人复测。** 实际基点`main/ac9ffe1`，上次集成、本地Visual功能及团队车型／地图更新已提交到达，开工工作区干净。用户确认有时无进度、有时执行后翻不起，并要求加快实际翻正。已允许Minimum Recovery Tilt等于Drive Exit Tilt以关闭65°～70°空档，补充仅自救使用、保持原距离／坡度的代理范围支撑后备射线，以及服务器复制HUD／日志Block原因；普通驾驶／轮胎接地和PD本体保持。当前保存资格等待实际为2秒、长按1.5秒，未按旧原生1秒回填；更快响应由团队按[自救说明第9节](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md#9-侧面着地自救与较快翻正响应2026-10-29)手动调整共享DA，未覆盖保存调参。

本机UE5.8.2 **Editor／Game成功，最终普通104/104，真实左右侧自救Host／Client各1/1**，最终3份报告均0测试警告／失败／未运行。受控Solver原响应侧翻／倒扣约1.017／1.417秒，建议组合约0.583／0.817秒；双端临时地板夹具两车Succeeded、序号2、客户端49次重模拟，80ms为主机出站注入，非实测RTT。早期两个测试旧网格引用失败和旧地图坐标导致双端NoGround超时均保留，分别修正测试资源读取和隔离地板后严格重验；不放宽生产规则。1172份保存资产前后逐份哈希全同、219份最终构建输入未变，专用进程已退出，原C4305等编译警告保留。见[本轮验证](Movement/MVPB_Vehicle_SideRecovery_Verification_20261029.json)。失败原局具体原因仍需Block／Outcome日志，不把边界修正写成全部根因已证实；65°和较快组合尚未写入保存DA，真人实际地图／墙角／手感未验收。本轮改动未提交／Push，F4／四机安排保持。

**2026-10-29本机拉取与集成验证完成。** 用户要求处理Project_Status和TestVehiclePawn的拉取覆盖提示。10份本地改动先逐文件备份并stash，从98b1a8d快进至远端ac14e60；TestVehiclePawn自动合并，已核对同时保留shocked组件组装／ASC绑定清理与静态Visual可选初始化。状态文档3段实际冲突均合并保留两边记录，当前入口以本段及第2／6节为准。远端新增电墙BP与NetTest地图更新正常接收，没有二进制冲突；本地静态回退可选源码／测试／工具提示／留档全部保留，历史验证JSON未改写。

本机UE5.8.2-56702186重新完整构建**Editor／Game成功，普通回归103/103、车型生成器3/3、电墙和车辆表现两组真实双进程Host／Client均各1/1**，6份报告均0测试警告／失败／未运行。编译既有C4305／C4996及启动警告保持，不代表编译零警告。电墙每车接触一次、无新Impact结算、HP100、Stun／shocked结束；表现双端为现有保存车辆的数据链路回归。1173份资产与拉取后基线逐份SHA-256全同，219份构建输入在验证后未变；4个专用游戏进程均已退出。NullRHI数据检查不代表真人画面、保存Mesh=None迁移、F4扰动或四台有线PC验收；电墙80ms为主机出站注入，非实测RTT。详见[本次集成验证](Combat/MVPB_ElectricWall_OptionalVisual_Merge_Verification_20261029.json)。本次未创建提交／Push，集成后的本地改动仍未提交；逐文件备份、新stash及原两份stash全部保留。


**2026-10-29同步合并完成。** 合并本地dc3372e电墙／Bounce Back与origin/main/98b1a8d的10笔更新。用户指定MVPB_NetTest地图采用远端版本；本地电墙Blueprint保留，远端地图中的摆放以远端为准。两个C++冲突分别保留Bounce Back与区域倍率初始化、shocked与MovementAreaReceiver组装／解绑；自动合并的运动历史同时保存两类数据。本机UE5.8.2 Editor／Game完整编译成功，普通101/101、真实电墙双端与区域双端共4份报告均各1/1且0测试警告／失败。电墙保持每车一次、无新Impact事件、HP100、Stun／shocked结束；区域弱区／离区恢复与Revision7保持。1173份保存资产相对远端地图选取后的基线逐份哈希相同；4个专用进程已退出。相机／驾驶／地图摆放的真人验收未新增。详见[合并验证](Combat/MVPB_Electric_Wall_RemoteMerge_Verification_20261029.json)。本次创建合并提交，不Push。

**2026-10-29电墙改为独立Bounce Back。** 用户确认高速取消原水平速度、按实际进入方向反弹，可调距离、每次进入一次、保留Z速度、不新增旋转、不受Impact免疫影响。机关闭合接触锁，回弹完成且取得完全离开样本后才重置，避免高速完整穿墙后的回程自行再触发。默认100cm／0.3秒线性减速；Stun／shocked保持。移除该机关的Impact档位／推送预设／周期重触发与CombatResolver结算，新增独立Authority网络物理动作及可回溯状态。原ContactImpactProfile仅保留None抑制柱子普通碰撞附加Impact，不负责回弹；自然Chaos碰撞仍存在。

本轮UE5.8.2 Editor／Game完整构建成功，普通**88/88**、最终真实双端**Host／Client各1/1**，报告均0测试警告／失败。真实Solver验证6000cm/s来速被替换、Z及W保持、无阻挡100cm回弹与停止、恢复重播；联机夹具以4000cm/s向东进入并开启Impact免疫，实际向西回弹、每车一次、无新Authority Impact事件、HP100及Stun／shocked到期通过。80ms为主机出站注入，非实测RTT；Client完成点35次重模拟／2次桥接回滚。首轮普通离开夹具缺少完全清空样本、首轮联机误将历史Impact记录当新事件的失败均保留，修正夹具后通过。尚未真人验证柱角、坡面、空中／连续车碰及视觉手感；距离是无阻挡目标，实际碰撞／阻尼可缩短。详见[当前说明](Combat/MVPB_Electric_Wall_CN_v0.1.md#7-bounce-back-調整與手測2026-10-29)及[本轮验证](Combat/MVPB_Electric_Wall_BounceBack_Verification_20261029.json)。

下一入口：团队重开Editor，手动Compile／Save现有BP_ElectricWallGimmick，在Class Defaults → Electric Wall → Config调Bounce Back Distance／Seconds与独立Stun Seconds，再做正式地图双端手感。代理未编辑地图／Blueprint保存资产，1158份保存资产前后SHA-256逐份相同，保留团队已存在的MVPB_NetTest和BP_ElectricWallGimmick改动。用户已授权将当前源码、文件及这两份团队资产一起建立版本提交；此前验证结果沿用本轮记录，本次提交操作不新增运行验收。


**2026-10-27电墙击退方向修正完成。** 用户要求“往东撞墙，往西击退”；将柱子／墙统一改为优先按进入前水平位移反推，没有位移才用水平速度，完全静止才退回几何方向。连续重叠保持首次进入方向，避免后续脉冲把已向外移动的车拉回墙；离开再进入会重新采样，冷却内的新进入也更新来向。档位／Short力度、Stun与shocked保持。保留团队新增`BP_ElectricWallGimmick`及`MVPB_NetTest`改动，1158份保存资产前后SHA-256逐份相同。

本轮UE5.8.2完整Editor／Game成功，普通**87/87**、最终双端**Host／Client各1/1**，报告均0测试警告／失败。新增东西南北／斜向、已反弹速度、连续重叠／离开再进入回归；真实双端夹具将墙旋转90°、向东进入，两端实际向西速度峰值125cm/s，HP100及Stun／shocked到期保持；Client完成点39次重模拟、2次桥接回滚。首轮联机夹具误选团队放入地图的另一组电墙而超时，修正只选择测试自身原生空中夹具后，原严格断言通过；初次失败保留。不是所有速度下的强制反向，幅度仍遵循共用衝击表；实际驾驶手感需团队复测。四个专用进程均已退出，Config／uproject／Build.cs未改，未提交／Push。详见[方向修正证据](Combat/MVPB_Electric_Wall_DirectionFix_Verification_20261027.json)与[当前规则](Combat/MVPB_Electric_Wall_CN_v0.1.md)。

**2026-10-27成对电柱／电墙第一阶段完成代码、构建与原生验证。** 用户明确授权制作车辆接触后小幅击退、Stun与shocked表现接口，球穿墙强化后置。接收干净 `main/0b28303d1bb02a51fd101901bbb52831baa27353`，先前Stun／debug与资料已随提交到达，不再把下方旧“未提交”当作本次基线。本机实际UE5.8.2-56702186。新增`AHALElectricWallGimmick`管理两根实体Capsule柱与Query Only电墙，服务器定时重叠＋实际车体Sweep补查高速穿越，三表面共用每车重触发间隔；默认Light＋Short、Stun1游戏秒、间隔1.5秒、扫描0.05秒。既有Combat／Impact动作与Stun入口复用，不接触HP或球载荷。

shocked新增Avatar作用域纯表现GE／`State.Visual.Shocked`与车辆原生`HALShockedPresentationComponent`：`IsShocked`／`OnShockedChanged`供团队接表现。电击标记后跟随当前Stun最终结束／驱散，后续其他来源延长也保留；普通Stun不自动带shocked，换Avatar清理，晚绑定先读当前标签。不增加逐帧Ability／物理Tick，不修改蓝图Event Graph或添加实际电流资产。

本轮**Editor／Game完整构建成功，普通86/86、真实Listen Server双端Host／Client各1/1，报告均0测试警告／失败**。5项新普通测试覆盖配置／方向、三表面去重、免疫与Stun独立、完整快速穿越、间隔后再触发、shocked随延长／驱散／Avatar清理与来源销毁。双端实际由机关注入两次接触，两端HP100不变、Light＋Short水平Y峰值125cm/s、Stun／shocked均结束；Client完成点37次实际重模拟、3次桥接回滚请求。测试为空中隔离原生夹具、60FPS上限、主机出站80ms注入，不代表实测RTT、最终位移／画面或F4验收。

1157份uasset／umap前后逐份SHA-256一致，Config／uproject／Build.cs未改；两专用联机进程已自行退出。**下一步团队按[Editor建立／调参／shocked接线步骤](Combat/MVPB_Electric_Wall_CN_v0.1.md)手动创建轻量子类、指定模型材质并放进测试地图，再做真人双端低／高速与连续接触验证。** 原生Actor没有指定美术，电墙为触发区域，小幅推送不保证高速车完全停止。球强化、实际电流特效、动态生成、VP-4C与F4仍后置。详见[本轮验证](Combat/MVPB_Electric_Wall_Verification_20261027.json)。本轮修改未提交／Push。

**2026-10-29静态Visual回退改为按可用表现路径校验，代码与原生验证完成。** 用户同意前步建议并授权施工，当前`main/98b1a8d`，保留开工前NetTest地图及两份检查文档改动。驾驶ConvexMesh车有完整有效且Ready的骨骼Presentation时，静态Visual可缺省；骨骼不可用时要求有效静态Visual，两条都无效则明确拒绝初始化。被动Box车与球仍需静态Visual。初始化会安全清除无用的旧静态网格／变换并隐藏组件，保留有效静态回退的原显隐行为，不修改保存DA、CollisionMesh／Physics、骨骼变换、驾驶／GAS或网络历史格式3。工具提示和现行配置说明同步，不再要求骨骼车型补占位静态网格。

本机UE5.8.2-56702186，完整**Editor／Game成功，普通96/96、车型生成器3/3、真实NetTest双端Host／Client各1/1**，4份报告均0测试警告／失败／未运行；构建既有C4305／C4996及启动警告保留。新增原生测试覆盖路径矩阵、Ready／轮参数不完整、被动Box／球、静态缺失／无效变换的真实保存骨骼初始化、旧预览清除、物理根保持及双路径缺失拒绝。无静态配置使用临时实例副本；双端为现有保存车辆的输入／观察者／归零链路回归，NullRHI／60FPS，不是已保存None配置的真人双端或F4验收。1172份资产相对用户保存关闭后的基线逐份哈希全同，210份源码／配置及原始日志／报告哈希见[本次验证](Configuration/MVPB_Optional_Static_Visual_Verification_20261029.json)。最终验证后未再改C++，专用进程已退出。

下一步团队重开已完整编译的Editor；需要时按[DA说明第6节](Configuration/MVPB_DataAsset_QuickGuide_CN_v1.0.md#6-静态回退可选团队配置与验证2026-10-29)确认骨骼表现有效后，将实际车型Visual.Mesh设None、保存／Validate并作真实驾驶／画面初测，也可保留原有效回退。独立Collision Mesh和其他已调配置保持；本轮没有代理编辑或保存资产，未提交／Push。动态相机配置／输入／画面及原MVP-B集中收尾继续，不新增运行中基础配置热重载；下方旧“仍强制静态／仅检查／94项”保持各自时点，以本段及第2／6节为当前入口。

**2026-10-29 Visual静态回退依赖核对，仅检查／留档。** 当前`main/98b1a8d`已包含主球编辑修复、回归记录及团队Camera DA／IA／IMC／Local配置提交；旧“本轮未提交／16b39eb／待拉7b53c6a”仅为前步时点。开工只有`MVPB_NetTest.umap`本地修改，保留不动。团队询问玩家Definition.Visual用途与阻塞后，确认其仍是静态外观回退，正常骨骼表现会隐藏它；实际碰撞独立使用CollisionMesh／Physics。当前Definition仍无条件要求Visual.Mesh有效且变换合法，清空可导致资产校验／运行初始化失败、物理及输入不可用，依赖就绪的测试可能失败或超时；不是必须保持SM_CarCollisionProxy名称或旧外观数值的验收标尺。校准／碰撞生成／资产创建保存／编译入口没有把这个字段作为相同门槛，资源本身另被旧碰撞测试引用。详见[DA简明说明第5节](Configuration/MVPB_DataAsset_QuickGuide_CN_v1.0.md#5-骨骼车型仍需填写-visual-吗2026-10-29源码核对)。本轮没有改C++／配置／资产，没有新编译或UE测试；按可用表现路径调整校验只是后续建议，尚未施工。94项及更旧相机双端保持原历史证据，真人接线／动态相机画面与MVP-B收尾仍待团队；仅本检查说明未提交。

**2026-10-29接续与主球编辑修复完成，动态相机正在团队配置。** 当前`main/16b39eb`，动态相机实现及上一设备UE5.8.3／93项与真实视口双端证据已提交；本设备实际UE5.8.2-56702186。团队已创建Camera DA、两项相机IA，并保存IMC／Local Config修改，全部保留；不将文件存在或修改状态当作Ready／完整接线或真人验收。截图中的关卡实例选择正确，不能勾选来自`ABasicBallActor::CanEditChange`的Definition迁移保护误锁新增主球身份；现只放行`bPrimaryCameraTarget`，保留实例专用、服务器复制和旧DA参数锁定。

用户保存关闭Editor后，本机完整Editor Target成功，**普通94/94（相机6项），0测试警告／失败／未运行**。新增`PlacedPrimaryBallEditability`覆盖Definition模式可编辑主球／仍锁旧伤害、实例与默认身份隔离及原Legacy行为；1172份保存资产相对用户关闭后的基线逐份哈希全同，210份源码／配置留档。生产改动仅WITH_EDITOR；Game Target／双端／真实视口本次未重跑，10月28日5.8.3结果保持原历史意义。编译既有C4305／C4996等警告未当作0警告；未修改运行时镜头、球权、网络历史或保存资产。见[修复与重新配置步骤](Presentation/MVPB_Dynamic_Camera_Implementation_CN_v0.1.md#9-主球勾选置灰原因与修复2026-10-29)和[本次验证](Presentation/MVPB_Dynamic_Camera_Primary_Edit_Verification_20261029.json)。

下一步重开Editor，按实施第9节在World Outliner选择主球实例，勾选Primary Camera Target并保存地图，再开有效Profile检查`PrimaryCandidates=1`；真人勾选／输入／驾驶及双端画面仍待团队。修复、测试和说明未提交／Push。收尾缓存`origin/main`另有`7b53c6a`仅更新Car_02两份DA，未由本轮Fetch／Pull，也未以其资产复测；保留本地相机配置。原F4／VP复杂组合及四机收尾安排保持；下方“本机5.8.3／未提交相机实现／尚无Camera资产”等按各自时点阅读，以本段及第2／6节为当前入口。

**2026-10-28动态相机0.1代码与原生验证完成，待团队接线／真人调参。** 用户确认FOV和基础距离进入共用Camera DA并授权施工，保存关闭Editor后实现`UHALCameraConfig`、本地无Tick相机组件与`CalcCamera`输出、显式服务器复制主球标记、鼠标相对累积／摇杆速度积分／延时回中、最终屏幕硬框／边界反馈、数值热读和原生调试面板。保留实际旧镜头1200cm／FOV90／Pitch约−75°，提供团队手工复制按钮；无有效Profile时使用旧组件。相机输入不进车辆物理命令／RPC／历史，运动协议3和权威玩法保持。详见[实施与详细接线](Presentation/MVPB_Dynamic_Camera_Implementation_CN_v0.1.md)、[验证与迁移基线](Presentation/MVPB_Dynamic_Camera_Verification_20261028.json)。本机UE5.8.3最终Editor／Game成功、**93/93普通回归**、真实D3D12离屏Host／Client各**1/1**且0测试警告／失败；分别FOV65／100进入各自CameraManager，主球标记两端可读，调试框／文字截图确认。两次早期编译及首轮未迁移测试球导致的主机失败独立保留，修正夹具后重验通过，未忽略错误。1169保存资产逐份哈希全同、210份源码／配置／uproject留档，保留原BuildAndLaunch、Car_01表现DA与NetTest改动；当前`main/36ed9b4`加本轮未提交代码／文档，专用进程退出，未提交／Push。保存资产尚未创建Camera DA／IA／IMC映射或设置主球，下一步按文档由团队手工配置并做鼠标／摇杆／驾驶／双端画面初测；硬件手感、真实UI切换、完整网络扰动与四机验收尚未通过。下方历史“未授权／未施工／只讨论”保留各自时点，第2／6节为当前入口。

**2026-10-28动态相机总体方案获原则认可。** 用户对上一版0.1方案表示暂时没有问题并询问剩余确认项；固定姿态／FOV、目标加权与距离分算、自车屏幕软／硬框、鼠标相对累积／延时回中及摇杆速度积分、共用Camera DA与实时调试路线按原则认可记录。具体数值不冻结，不要求逐参数预先审批；主球显式身份和现有静态镜头迁移由施工规格细化。建议失控／自救时仍允许观察，UI／失焦／失去Pawn时暂停并清理输入，见[方案第9节](Presentation/MVPB_Dynamic_Camera_Plan_CN_v0.1.md#9-首版边界的建议默认处理)。当前没有新的必须阻塞施工的玩法确认项，但用户尚未要求开始实现，本轮只有留档，无相机C++／资产／编译或新测试。

**2026-10-28拖尾基础初测正向反馈，进入动态相机0.1方案讨论。** 当前已接收`main/36ed9b4`的拖尾代码／文档提交，团队反馈测试暂时没有较大问题，见[反馈](Presentation/MVPB_Vehicle_TireSkid_Implementation_CN_v0.1.md#7-团队初测反馈与后续入口2026-10-28)。工作区现有BuildAndLaunch、Car_01表现DA与NetTest地图修改保留；本轮不保存资产、不重编或运行新UE测试，也不扩写具体双端／斜坡画面通过。用户提出本车／其他玩家／唯一主球加权聚焦与距离、自车中心范围限制、右摇杆／鼠标有限观察；已选择鼠标相对移动累积偏移、停止后延时回中。已形成[动态相机讨论草案](Presentation/MVPB_Dynamic_Camera_Plan_CN_v0.1.md)，固定镜头姿态／FOV、目标软权重、屏幕软／硬框、摇杆速度积分、共用Camera DA与实时调参可视化等为建议，尚未授权相机施工。当前静态相机／输入接口不变，物理辅助必要性讨论和F4／VP-4C后置安排保持。

**2026-10-28轮胎拖尾代码接入与原生验证完成，等待团队启用DA／画面初测。** 用户授权先试装已有`NS_TireSkid`，现于Presentation DA增加默认关闭的Tire Skid分组，由C++表现层按手刹＋车速／侧滑或高速反向制动触发，按轮接地、姿态／Stun／自救过滤；停止后旧段消退，重新触发另起段，重配／退出清理。复用视觉轮点Trace，关闭视觉悬挂也能使用；物理线程只额外发布已消费输入需求，沿原15Hz表现通道复制一个字节，运动历史格式3及权威玩法不改。详见[实现与手工配置](Presentation/MVPB_Vehicle_TireSkid_Implementation_CN_v0.1.md)及[验证摘要](Presentation/MVPB_Vehicle_TireSkid_Verification_20261028.json)。本机UE5.8.3 Editor／Game成功、88/88普通回归、真实D3D12离屏组件检查1/1、连续两轮真实双进程Host／Client各1/1，报告均0测试警告／失败；早期夹具链接／GAS初始化及一次双端超时独立留档，最终全部复验后未再改C++。1169保存资产逐份哈希与用户保存关闭后的本轮基线全同，199份源码／配置／uproject哈希留档；原NetTest地图、BuildAndLaunch及文档修改保留，未提交／Push。当前`main/08b43d0`加本轮未提交代码／文档，普通Editor未自动打开。DA仍须团队手工启用，真实宽度／残留／轮位／远端画面未验收；原素材RibbonFacing为世界向上，斜坡贴合按文档手改。四点物理辅助必要性已讨论为按实测优化的建议，未擅自记为已完成或正式取消；当前优先完成拖尾接线反馈，动态摄像机／F4／VP-4C等按后续排期。

**2026-10-28区域基础实测收尾与MVP-B剩余范围核对：** 用户确认减速／打滑两类区域各自独立效果及重叠效果均已测试，基本符合预期，按[区域实施第7节](Combat/MVPB_Movement_Areas_Implementation_CN_v0.1.md#7-两类区域独立与重叠的团队反馈2026-10-28)记录为团队基础功能初测通过；未追加具体客户端、普通Buff组合或扰动工况结论。当前已接收`main/08b43d0`，新增两份区域BP及NetTest地图修改，保存资产1169份；Source／Config／uproject相对9c37216无变化，工作区原有`BuildAndLaunch.cmd`修改保留。核对原MVP-B承诺及现有代码，**四点物理悬挂／有限空中与自然落地辅助、按玩家分布调整距离的动态摄像机仍未实现**；VP-3只有视觉悬挂，长按自救不是自然落地辅助。固定地面伤害区与指定Stun机关的具体接入另见第6节，不以减速／打滑替代伤害区。完整比赛淘汰／观战／结算、道具拾取施放等仍未建设且不属于既定最小必做范围。F4／VP-4C与四机验收后置安排保持，本轮只核对与留档，没有启动新功能、编译或测试。

**2026-10-28减速区域真人初测反馈更新：** 团队确认关卡实例使用加速度倍率`1.0`／速度上限倍率`0.3`／恢复`0.15s`后，区内低速行驶、主动停车及再次起步均符合预期。当前UE5.8.3的PIE Listen Server日志确认区内实际倍率`1.0/0.3/1.0`、输入正常、`ControlsBlocked=0`，离区速度参数用9个物理帧恢复至1。此前“1.0/0.3仍不能起步”的反馈由用户撤回为配置错误，不能继续作为驾驶锁死证据。加速度0.3下停车仍属于低推进力与接触阻力的待量化限制，不据本次对照宣称已修复或完成摩擦诊断。详见[配置说明第6节](Combat/MVPB_Movement_Areas_Implementation_CN_v0.1.md#6-团队减速区真人初测与配置纠正2026-10-28)。本次仅核查日志与更新文档，无源码／资产修改或新增编译／自动化；打滑、多个区域、其他车型、客户端及F4仍保留原验收边界。

**2026-10-28接续同步与BuildAndLaunch修复完成，当前本机`main/9c3721631cfa18afebfba22fe68efefcba20c460`。** VP基础表现已按用户反馈阶段性收尾，VP-4C复杂组合／生命周期与F4继续留MVP-B末尾；三档／水平推送、减速区／打滑区和共用Stun的合并代码均已提交，运动历史格式3保持。历史UE5.8.2的85项及区域／Stun分别双端证据保留，不作为本轮新运行。本轮收到9c37216的四份Car_02车型配置／BP／ABP、Car_01表现DA重命名及玩家Definition引用更新，当前1167份保存资产；未从文件存在推定AnimGraph接线、Ready或真人驾驶验收。下一步仍为团队区域配置／驾驶基础反馈、指定Stun机关接入及新车型检查，没有启动新玩法开发。

本机实际为**UE5.8.3-58210709**，此前5.8.2是另一设备历史。已复现旧BuildAndLaunch整份解决方案MSBuild报NETSDK1004、57个引擎C#工程缺失NuGet文件错误；改为直接调用本机引擎Build.bat构建当前Editor Target，保留独立单文件入口，增加官方Rebuild、日志／结果JSON、失败码保留、失败不启动和早退检查。实际`-Rebuild -NoLaunch`成功116.20秒，默认构建并启动Editor成功，PID49912／日志Startup complete；保留窗口供用户使用。运行时占用检查返回1，5项独立夹具通过，详见[操作说明](Configuration/Build_Editor_Tool_CN.md#2026-10-28buildandlaunchcmd修复与用法)及[本轮验证／接续摘要](Configuration/Build_And_Launch_Verification_20261028.json)。本轮只改工具与文档，1167资产及197份Source／Config／uproject相对本轮基线全同，没有重跑85项、Game Target、PIE或联机／新车型验收。

与10月27日旧清单比对为98份原字节一致、98份仅LF／CRLF差异，1份Build.cs旧哈希未匹配；当前Git的Source／Config／uproject相对739d5bc没有内容变更，本轮以自己的保护基线和实际Editor重编为证据，不把旧“197份逐字一致”挪作本机结论，也不改玩法源码来修正历史摘要。开工干净，本轮工具／留档未提交／Push；以下旧“当前739d5bc／1163资产／5.8.2／等待完整构建”按各自时点阅读，第2／6节为当前入口。

**2026-10-27 Git接续合并与本机代码验证完成。** 用户授权处理拉取覆盖阻塞，已校验备份39份本地改动并保留新旧stash，从914cc53快进至cd957ba（Stun与电弧素材），恢复本地区域实现；7份文本冲突逐项合并，两边规则／历史留档均保留。运动历史格式升为3，同时保存Stun权限、三路区域恢复曲线和实际消费值；桥接保留六项属性与Stun事件，纯权限变化不被相同倍率去重。补充恢复中眩晕／解除的连续曲线、历史回放和旧版本2拒绝检查，真实Solver中的Stun／抓地测试均保留。

本机UE5.8.2-56702186，**Editor／Game完整构建成功，普通85/85，区域与Stun两轮真实NetTest各Host／Client 1/1**，5份报告均0测试警告／失败。区域Client完成点56次重模拟／6次运动回滚，Stun Client为30／2；主机80ms出站注入、60FPS／NullRHI，不是实测RTT、真人画面或F4／四机验收。两轮网络场景分别运行；区域恢复与Stun组合由原生历史回归覆盖。详细证据见[合并验证摘要](Combat/MVPB_Movement_Stun_Merge_Verification_20261027.json)。两份旧81项JSON保持原各自施工证据，不覆盖。

收尾检测到并行Git操作：17:06:10拉入`5591039`（Car_02与Car_01资源分目录／AnimBP更新），17:06:22将合并后的区域源码／留档提交为`739d5bc`，当前`main`及本地`origin/main`均到该提交；不是本代理创建或推送。197份源码／配置逐份哈希与刚完成构建／测试完全相同，**但资源更新晚于全部测试报告，没有以新Car_02／Car_01资产复测**。本代理拉取至cd957ba时原1157资产全同、只新增2份电弧；随后资源提交使当前数量1163，差异仅为5591039已提交的10条增／删／改路径（含重命名），无额外工作区资产改动。没有手工编辑蓝图、DA或地图；当前仅本收尾状态／验证补充未提交，需正常提交才能跨设备同步。备份位于忽略的`Saved/GitMerge/20261027_165350`，新旧stash均保留。下一步按区域实施配置并进行真人基础验证，Stun仍待指定机关接入；VP复杂组合／F4保持MVP-B收尾。联机两端须同步完整合并代码、同一引擎补丁并重编，不能混用任一独立版本2旧程序；下方历史“当前／未提交／待构建”按各自时点阅读，以本段及第2／6节为准。

**2026-10-27 Stun debug指令完成。** 保留本轮此前未提交的共用Stun与文档，新增主机控制台 `HALStun <PlayerId> <游戏秒数>`，由当前局PlayerId选择自己／其他玩家，复用原`ASC.ApplyStun`，不新增客户端任意目标RPC。仅Development等非Shipping实现有效；校验本地主机、`hal.StunDebug`（默认1）、正有限时长、Playing、唯一且配置／HP就绪的存活受控Avatar。`HALGASStatus`新增`Stunned`与`StunRemaining`，结果日志／屏幕反馈已接入。用户保存关闭Editor／Live Coding后，本机UE5.8.2完整Editor／Game均成功，普通**81/81**、实际双端**Host／Client各1/1**，报告均0警告／失败。真实控制台字符串施加主机／Client两车、Client控制台拒绝、自然到期及原时长延长通过；Client完成点40次实际重模拟、2次桥接回滚请求。主机出站注入80ms、60FPS上限不等于实测RTT／F4或真人文字画面验收。

1157份保存资产前后SHA-256逐份全同，Config／uproject／Build.cs未改，专用Host4980／Client44668已自行退出；没有Stun Zone、清除指令或新蓝图。用法见[Stun Debug指令](Configuration/Stun_Debug_Command_CN.md)，本轮证据见[验证记录](Combat/MVPB_Stun_DebugCommand_Verification_20261027.json)。下一步团队重开Editor／双端游戏，Playing后先`HALGASStatus`查ID，再在主机输入如`HALStun 257 3`；实际ID按该局输出。仍待用户指定机关接入与真人验证；修改未提交／Push。

2026-10-27日文资料准备：[Stun再利用与参数日文指南](Configuration/Stun_Reuse_And_Parameters_JA.md)已按中文版整理，保留接口、重复施加规则、Editor字段尚未创建与非GAS道具接入边界。目前没有连接中的文档会话，Google文档目标链接待用户提供，尚未更新远端文档。本次仅文档／Git白名单修改，无代码／资产变更或新编译／测试。

2026-10-27文档补充：[Stun複用與參數調整指南](Configuration/Stun_Reuse_And_Parameters_CN.md)已新增，包含来源C++接入、Class Defaults／Data Asset时长位置、重复施加示例、状态／剩余时间查询、非GAS道具门控及合法解除示例。示例不代表已创建机关或调参字段；本次仅文档／Git白名单更新，无新增玩法代码、资产修改或编译／测试，以下原验证结果保持原时点。

**2026-10-27共用 Stun 状态代码／构建／原生验证完成，未制作 Stun Zone。** 用户授权制作可复用限时 Stun，明确机关另行规划。起始干净 `main/914cc53fd56a550bd06228edee04b1a9b2a393ad`，本机实际 UE **5.8.2-56702186**，下方 5.8.3／77 项为另一设备历史。新增原生 Stun GE、服务器共用 `ASC.ApplyStun(秒数, 来源)`、较晚结束时间规则、Stun 标签事件瞬间落球及发射／捕球／技能／自救限制；Bridge 将禁止操控位送入既有可靠动作和物理时间线，格式升至2，保留惯性／外部碰撞，不在PT访问GAS。

本轮完整 **Editor／Game Win64 Development构建成功，普通80/80、实际Listen Server双端Host／Client各1/1，测试报告均0警告／0失败**。普通回归包括新增3项Stun测试及真实Solver历史Stun重放。双端60 FPS上限、主机出站注入80ms；客户端完成点实际35次重模拟、1次桥接回滚请求，双方Stun物理Revision2→到期3，短施加／延长不新增边界。注入值不是实测RTT，不代表F4／真人画面。首轮Editor被打开的DLL阻止链接，用户保存关闭后完成；Game首轮测试少PlayerState头文件已修复。双端首轮客户端未Ready最终超时；修正测试两端重力一致、非零输入预热／等待客户端Ready和完成握手后，原严格断言通过，未为测试放宽生产物理规则，失败记录保留。

**1157份uasset／umap前后SHA-256逐份全同**，Config／uproject／Build.cs未改；四个专用联机进程已自行退出。现有加速／免疫继续计时，球掉落不新增冲量。当前没有库存／道具使用框架，本轮提供统一查询和GAS激活阻挡，未来非GAS道具须接服务器查询；未创建机关Actor、蓝图、地图、DA、UI／特效或玩家施加Stun的调试RPC。下一入口为用户指定的机关C++接入与团队真人验证；VP-4C／F4后置保持。见[Stun说明](Combat/MVPB_Stun_State_CN_v0.1.md)与[本轮验证](Combat/MVPB_Stun_Verification_20261027.json)。当前修改未提交／Push；下方旧“当前／未施工”按各步时点阅读，以本段及当前任务为准。

**2026-10-27 减速区／打滑区代码、构建与原生双端验证完成，下一步团队布置区域并验驾驶观感。** 用户确认逐参数选最强、离区短过渡和可配明显强度后授权施工。新增服务器`HALMovementArea`／每车来源接收器、两类内部GE、普通抓地增量及区域独立贡献，沿用ASC／Avatar生命周期与原DA基线；桥接按GT合并变化，权威动作／版本2时间线保存三路恢复曲线及实际消费值，客户端回滚复用同一规则。默认减速0.5／0.5、抓地0.2、恢复0.4游戏秒可调，不创建逐帧GA或每帧GE／RPC。HUD／HALGASStatus增加Grip和AreaFade，`hal.MovementAreaLog`可关。

本机实际UE5.8.2-56702186，最终Editor／Game成功、普通**81/81**、实际NetTest **Host／Client各1/1**，均0测试警告／失败；客户端50次重模拟、6次运动回滚，三项全退出恢复均24帧、最终三倍率1／Revision7。60FPS／主机80ms出站延迟是夹具条件，不是实测RTT／四机验收。1157份保存资产SHA-256全同，Config／uproject／Build.cs未改；Native源码／配置现192份。早期78/81、80/81及Sparse Delegate诊断编译失败、同帧退出恢复时间被覆盖的发现与修复全部留档，不以首轮宽断言双端通过代替最终证据。详见[实施与Editor操作](Combat/MVPB_Movement_Areas_Implementation_CN_v0.1.md)、[规则](Combat/MVPB_Movement_Areas_Rules_CN_v0.1.md)及[验证摘要](Combat/MVPB_Movement_Areas_Verification_20261027.json)。区域BP／地图摆放与真人驾驶／画面仍待团队，5.8.3／VP复杂组合／F4／四机保持后置。当前`main/914cc53`，保留开工全部文档改动，本轮源码和留档未提交／Push；以下各历史“尚未施工／仅文档／无新构建”按原时点阅读，以本段及第2／6节为当前入口。

**2026-10-27 VP当前功能开发可阶段性收尾，复杂情况统一并入MVP-B末尾验收。** 用户明确调整排期并反馈“目前测下来表现没有太大问题”。现有轮转／转向、驾驶倾斜、视觉悬挂、发射／有效冲击回弹已有实现与既有原生证据，收到总体基础表现初测正向反馈，当前没有必须为VP追加的新功能或本轮已复现的阻塞缺陷。VP-4C不再作为立即施工入口，保留为[收尾组合／生命周期清单](Presentation/MVPB_Vehicle_Presentation_VP4C_Plan_CN_v0.1.md#8-vp阶段性收尾与集中验收2026-10-27)，届时与[F4](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)合并去重、补必要审计／测试和实际缺陷修复；未执行的复杂验收不标为通过。一般反馈不扩大为Short／Long量化距离、各来源／角色／FPS／扰动或四机全部通过。打滑FX接入、落地回弹／微振动／相机震动及四点物理悬挂／自然辅助都是独立后续事项，不作为当前VP必做项。当前仍main/914cc53、本机UE5.8.2，本轮仅维护文档、保留全部已有改动，无新代码／资产／配置、构建／运行或提交／Push。以下旧“确认后C-1／等待一般画面反馈”按原时点阅读，当前以本段及第2／6节为准。

**2026-10-27 VP-4C进入方案阶段，代码与验证尚未开始。** 用户要求“先开始VP-C，我们先确定一下方案”，按上下文为VP-4C。当前仍`main/914cc53`、实际UE5.8.2；保留前一轮3份接续文档及1份JSON未提交改动。已核对表现组件的配置／停止／重置、AnimInstance取数、A／B身份／时间／淡出与合成及相关测试，新增[VP-4C组合与生命周期收尾方案](Presentation/MVPB_Vehicle_Presentation_VP4C_Plan_CN_v0.1.md)：沿用现有组合优先级和装饰通知，分C-1组合审计／缺口测试、C-2必要修复／构建／基本双端、C-3团队画面与留档。此为讨论草案，确认后先执行C-1；本轮未修改C++／配置／资产，未构建／运行／新增测试结果，原75／77项不当作本步验收。VP-4B画面与水平距离／手感人工记录可随C-3补齐，不能由进入C推定通过；F4、物理悬挂／自然辅助、打滑FX接入及相机震动保持独立后续。本轮仅文档及Git白名单修改，未提交／Push；以下旧“下一入口”按原时点阅读，以本段及第2／6节为准。

**2026-10-27接续同步：VP-4B与水平推送预设均已提交，当前入口仍为团队画面／距离反馈，再进入VP-4C。** 本轮接收并核对开工干净的`main/914cc53fd56a550bd06228edee04b1a9b2a393ad`，VP-4B为`bff8753`、水平预设为`a09a3c4`；旧“未提交／Push”仅描述10月26日施工时点，已不是当前状态。按VP-4A全量清单→VP-4B变更→水平预设变更逐层核对当前182份源码／配置，178份字节一致、4份仅LF／CRLF差异，无内容差异；水平预设提交之后没有Source／Config／uproject变化。当前1157资产，最新提交只新增轮胎打滑Niagara／两份材质共3资产，没有配套运行时接入代码，不能把素材导入当作特效接线／验收完成。详见[本次接续快照](Presentation/MVPB_Project_Handoff_Review_20261027.json)。

VP-4B既有75项与双端、水平预设77项与双端及完整构建证据均保持10月26日的UE5.8.3原始含义；本机实际安装**UE5.8.2-56702186**，Saved原始报告未随Git到达，本轮没有重编／运行／新自动化／DA值导出，不声称做过本机复测。两项代码完成；尚无新增团队启用DA／实际受击画面、Heavy＋Short／Long距离与手感的验收反馈，VP-4C未施工，F4仍留MVP-B末尾。运行前统一团队引擎补丁并按当前提交完整重编。本轮仅维护接续文档，未改源码／配置／资产、未Fetch／Pull或提交／Push。以下历史“本机5.8.3／未提交／当前基点”按各自设备和时点阅读，以本段及第2／6节为当前入口。

**2026-10-26水平推送预设代码／构建／原生验证完成，待团队配置与真人距离初测。** 用户确认三档不变、Heavy＋近推／远推及旧球／弹簧拳兼容并授权；开工为干净`main/bff8753040a50b5e11811453a8692024b9ebb878`，此前VP-4B已提交。公共Combat DA增加Short／Standard／Long／ExtraLong倍率，默认0.5／固定1／1.5／2；球／拳／爆炸在原动作快照捕获预设，统一结算先降档再缩放水平目标再扣除自然响应，普通接触保持原稳定性路径，向上／翻转／伤害／脱球及骨骼幅度仍按既有规则。团队关闭Editor后已完成完整链接，最终Editor／Game与强制Unity构建成功，普通**77/77**、真实NetTest **Host／Client各1/1**，均0测试警告／失败。Host近推／远推实际X速度450／1350cm/s，两端均Heavy、HP100不变；Client观察窗口315.074／1350cm/s、36次重模拟。60 FPS上限／主机出站注入80ms不代表实测RTT或F4；NullRHI空中夹具不代表实际模型／驾驶距离验收。

1154份保存资产SHA-256全同，Config／uproject／Build.cs保持，专用UE进程已退出；完整代码构建后未再修改源码。首轮Game的既有VP-4B Unity私有函数重名已修复；前两轮双端夹具每帧写阻尼干扰PT状态导致0/1超时，改为准备时一次性设置并隔离测试前地图扣血后，通过原断言；没有改生产物理Handler。首次失败和最终证据均保留，详见[规则／配置／验证](Combat/MVPB_Impact_Push_Presets_CN_v0.1.md)和[摘要](Combat/MVPB_Impact_Push_Presets_Verification_20261026.json)。原MSVC／C4996及引擎启动警告保留；0测试警告仅指报告。本轮未提交／Push。下一步团队手动配置Heavy＋Short／Long并作距离／画面初测，旧Standard不需重填；VP-4B画面反馈、VP-4C及F4仍按后续入口，以下旧“等待关闭／未链接／75项／未提交”保留各步时点。

**2026-10-26 VP-4B有效冲击回弹代码／原生验证完成，当前待团队启用DA与画面初测。** 用户接受方案并明确授权施工；基点`main/5c84320c71977f86a97139e45a64063dac3be2d9`，保留开工前进度同步文档和接续JSON。统一ImpactReceiver在最终结果收尾发布有效档位与目标局部方向，复用现有表现组件／DA／Frame／AnimInstance及Chassis Rotation节点，新增默认关闭的Impact Rebound。免疫／None／普通支撑不播放，0伤害或自然追加补足为0的有效冲击仍可播放；不改变伤害、物理冲量、脱球或技能规则。服务器结果驱动、不做受击预测，序号／年龄去重、8条有限包络、严重倾斜／自救永久淡出及既有生命周期清理已实现。

本机UE5.8.3-58210709，Editor／Game Win64 Development构建成功，本轮普通**75/75**、真实NetTest双端**Host／Client各1/1**，均0测试警告／失败。双端60 FPS上限、主机出站注入80 ms延迟，两端各自本车／观察车均恰好2次回弹并归零；客户端31次物理重模拟无额外回弹。1154份uasset／umap逐份SHA-256全同，Config／uproject／模块依赖保持，专用进程已退出。NullRHI／临时配置／空中隔离夹具只证明数据链路，未做实际画面、最新模型几何或完整正常驾驶组合验收；80 ms注入也不是实测RTT或F4。原MSVC／弃用及引擎启动警告保留，“0测试警告”指报告计数。详见[配置与本轮结果](Presentation/MVPB_Vehicle_Presentation_VP4B_Implementation_CN_v0.1.md)和[验证摘要](Presentation/MVPB_Vehicle_Presentation_VP4B_Verification_20261026.json)。**本轮未提交／Push**。

施工前已从干净`fc9f2b9`快进接收远端15个提交至当前`5c84320`，包括VP-3／VP-4A实现、团队接线／调参及Car_01模型修复，过程见[接续快照](Presentation/MVPB_Project_Handoff_Review_20261026.json)；该JSON只描述施工前同步，不能代替本步证据。VP-3／4A的10月23日基础画面反馈保持。当前下一步由团队在现有车型表现DA勾选**Impact Rebound → Enabled**并保存；原AnimBP不增加节点。Light／Medium／Heavy默认各轴3／6／10°，起势0.04秒，回正0.18／0.25／0.32秒可调。完成本步基本画面反馈后再进入VP-4C组合收尾，F4继续留MVP-B末尾；以下历史记录中的“未施工／未授权／未构建”按各自时点阅读，当前以本段及第2／6节为准。

**2026-10-23 VP-4A基础画面初测通过，当前入口为VP-4B方案讨论。** 用户反馈“暂时表现没有问题”；本轮只读核对干净`main/0efa4100962b258a41b2bfa7c254f812deed7a21`，VP-4A实现／验证已随`3d42dd8`提交，团队表现DA／车辆Definition调参已随`0efa410`提交。另有`8609f4f`弹簧拳曲线及原测试补充保留，不将它们回退。本机实际UE5.8.2-56702186、当前887资产；本次没有新构建／运行／自动化、DA数值导出或资产修改，旧71项／双端报告保留原时点，不能当作新曲线集成验证。一般画面反馈不扩写为完整角色／FPS／拒绝／扰动或四机通过。详见[VP-4A反馈](Presentation/MVPB_Vehicle_Presentation_VP4A_Implementation_CN_v0.1.md#7-团队画面初测与下一步2026-10-23)及[VP-4B方案／接续核对](Presentation/MVPB_Vehicle_Presentation_VP4B_Plan_CN_v0.1.md)。4B建议按服务器最终有效档位／方向生成统一受击骨骼回弹，过滤免疫／None／普通支撑，保持伤害与物理独立；**尚未授权施工或修改C++**。本轮仅文档与Git白名单修改，未提交／Push。以下旧“待启用／未提交／当前基点”按原时点阅读，第2／6节为当前入口。

**2026-10-23 VP-4A发射视觉回弹代码／原生验证完成，当前下一步为团队启用DA及画面检查。** 开工为干净`main/bf21ebc024df84c00a67a78aba949806776d3664`，已接收团队VP-3接线、单Host测试GameMode及弹簧拳更新；本机UE5.8.2-56702186。现有表现链路增加默认关闭的Launch Rebound、即时预播／确认去重／拒绝淡出、观察者轻量通知和最终限幅，未改变物理后坐或球规则。Editor／Game均成功，最终普通**71/71**、实际双进程**Host／Client各1/1**，均0测试警告／失败；885资产前后SHA-256全同，Config／uproject／模块依赖保持，专用UE进程正常退出。原生夹具／NullRHI不代表实际画面验收，首两轮70/71的夹具生命周期问题及修正独立留档。详见[操作／增强回弹说明](Presentation/MVPB_Vehicle_Presentation_VP4A_Implementation_CN_v0.1.md)与[本次验证](Presentation/MVPB_Vehicle_Presentation_VP4A_Verification_20261023.json)。团队须手工启用当前车型表现DA的Launch Rebound；原Chassis Rotation节点复用。真实后坐可另调车辆Definition的Ball Control→Launch，未代改已调资产。反馈后再进入VP-4B有效冲击，4C组合、物理悬挂／自然辅助和F4保持后续。**本轮源码／文档未提交／Push**；以下各历史“当前／未施工／待接线”按各自时点阅读，以本段及第2／6节为当前入口。

**2026-10-23 VP-3基础接线与画面初测通过，下一入口VP-4方案确认。** 用户确认“配置完成，经过测试暂时没有表现问题”；核对开始时工作区干净，当前为`main/093e758`，最新提交包含Car_01 AnimBP及表现DA，前续VP-3代码与资产生成器独立Definition修正亦已提交，不再将旧“未提交／待接线”作为当前状态。本机实际UE5.8.2-56702186。反馈留档及当前两份资产哈希见[VP-3第7节](Presentation/MVPB_Vehicle_Presentation_VP3_Implementation_CN_v0.1.md#7-团队接线与画面初测反馈2026-10-23)；未提供逐场景／角色／FPS／扰动结果，不扩写为完整矩阵验收。本轮只读核对并更新文档，没有改代码／资产或新构建／测试，旧67项／双端证据保持历史含义。下一建议按[VP-4A发射→4B有效冲击→4C组合／联机清理](Presentation/MVPB_Vehicle_Presentation_Rig_Plan_CN_v0.1.md#112-vp-4建议施工顺序2026-10-23)逐步开发，先确认范围、表现规则与通知策略，尚未施工；物理悬挂／自然落地辅助、生成器人工欠项及F4后置保持。以下各历史“当前／未提交／待接线”按各自时点阅读，当前以本段及第2节为准。

**2026-10-23资产生成工具按车型独立Definition修正。** 本机已拉取`main/ad3e4d4`，保留云端VP-3视觉悬挂及其历史验证；开工已有前轮工具的6份源码／说明修改和1份验证留档。本轮移除Shared Vehicle Defaults输入与全部参数复制来源，从C++初始值新建`DA_Vehicle_<车型>`，将其连接到新`BP_VehiclePawn_<车型>.Definition`；LocalConfig固定引用共享`DA_Local_TestBaseline_v1`，Presentation引用本车型新`DA_<车型>_Presentation`。Network Physics继续由面板选择共享DA并连接原生Settings组件；Pawn直接继承`ATestVehiclePawn`，引用位置沿用BP_RoundedVehiclePawn。最终只创建两个DA、一个玩家BP和一个默认空AnimGraph的ABP，输出目录保持。两份新DA仍待Ready，Collision Mesh／回退Visual.Mesh及车型参数需要配置。UE5.8.3 Editor Win64 Development构建成功，三项核心测试已编译；本轮Unreal启动没有日志／报告或标准输出，不能确认测试执行，实际生成／保存重启及真人待验证。667份资产和151份运行时源码／配置逐文件SHA-256全同，Build.cs／模块入口保持；未提交／Push。详见[工具现行流程](Configuration/Vehicle_Asset_Generator_CN_v0.1.md)及[本轮验证摘要](Configuration/Vehicle_Asset_Generator_IndependentDefinition_Verification_20261023.json)。下方VP-3的5.8.2验证属于另一设备历史，本轮未调整其实现或验收范围。

**2026-10-22 VP-3视觉悬挂代码／原生验证完成，待团队AnimBP接线与画面。** 本次实际接收干净`main/faf5d067b85140ddd108969f6b2c48a0ac63b8b9`，此前资产生成工具／校准／炸弹及场地对象均已提交；本机实际UE5.8.2-56702186，旧顶部5.8.3是另一设备历史。沿用表现组件、DA及AnimInstance，新增参考轮心四路WorldStatic简单Trace、每轮世界cm压缩／伸长／平滑、法线／侧翻／自救门控及自动Bone Space平移。新通道默认关闭，未改物理根、驾驶／球／GAS／网络物理协议或保存资产。完整Editor／Game成功、普通**67/67**、实际NetTest双端**Host／Client各1/1**且均0测试警告／失败，667资产前后全同；客户端18次重模拟。双端使用临时配置及本地QueryOnly平台／关闭重力夹具，证明数据链路而非正常驾驶画面或地形复制；四车16条查询微测约19.555µs，不代表实际渲染性能。首轮C4458及65/67测试符号预期问题保留，修正后重编／完整回归通过。详见[VP-3实施／详细手工步骤](Presentation/MVPB_Vehicle_Presentation_VP3_Implementation_CN_v0.1.md)与[本轮证据](Presentation/MVPB_Vehicle_Presentation_VP3_Verification_20261022.json)。当前下一步为团队在原AnimBP增加四个Translation节点、启用车型DA并检查正常地图／双端画面；之后再讨论VP-4。物理悬挂／自然落地辅助与F4仍未施工，生成器旧人工待测项保持。本轮未提交／Push。下方各旧“当前／未施工／未提交”段保留各自时点，以本段和第2节为当前入口。

**2026-10-22资产生成工具菜单恢复，当前接续基点`main/5f6c9ef`。** 用户已拉新冲突文件，本轮在最新`HALFutureCreationEditorModule.cpp`补回资产生成工具的窗口注册、Tools菜单和关闭注销，保留云端碰撞／视觉高度校准入口。最新Build.cs已含所需依赖及平铺头文件路径，本轮保持云端内容，没有增加模块。此前两份DA／空AnimBP／新车辆BP的生成源码保留，补齐创建、保存、手工AnimGraph配置、车型／高度校准、Ready及测试接入流程。UE5.8.3 Editor Target完整构建成功，新增测试源已编译；再次尝试Unreal自动化仍未产生日志／报告，不能标记运行通过，实际菜单和生成／保存后重启仍待团队验证。本轮667份资产数量及SHA-256前后全同，保留用户SKM_Car_01修改，没有保存Content资产；未提交／Push。见[工具说明与本次验证](Configuration/Vehicle_Asset_Generator_CN_v0.1.md#5-同步后菜单恢复验证)及[摘要](Configuration/Vehicle_Asset_Generator_Reconnect_Verification_20261022.json)。下方旧基点／测试数保留各次历史含义，不代替本次检查；VP-3与F4状态保持。

**2026-10-22拉新与状态合并（当前接续基点）**：本机`main`已从`fc9f2b9`快进到GitHub最新`8571fa443dfcb880058447f95a98917fa248dd76`（新增炸弹本体与调试生成）。合并保留远端炸弹记录和本地Editor自动校准／跨设备核对记录；原有16份本地源码、文档及团队手调表现DA已备份并恢复，工具和合并后的文档仍未提交／Push。下方炸弹段“未提交”与旧`fc9f2b9`接续段均为各自施工时点，不代表本次HEAD。此次只完成Git拉新及文档合并，没有重新编译／运行Unreal或重跑测试；两项功能各自既有验证不能视为合并后集成验证。运行新增炸弹功能前，保存并关闭旧Editor／游戏窗口，关闭Live Coding后完整编译Editor Target；需要独立Game构建时再编译Game Target。VP-3尚未施工，F4继续后置。

**2026-10-22炸弹范围收敛（最新用户指令）：** 道具框架／拾取／施放尚未决定，本次删除此前新增的拾取Actor、库存组件、道具GA、UseItem输入／Tag及HUD，并还原相关车辆／Local Config／网络输入脚本。只保留实体炸弹落地倒数、独立爆炸范围及统一GAS伤害／TieredV1冲击；新增开发命令HALSpawnBomb，服务器生成一颗后立即倒数，直接放场景则仍落地才计时。测试默认3秒／500cm／25伤害／Heavy可调。基点main/fc9f2b9，本机UE5.8.2；删除后重新Editor／Game构建成功、普通63/63及7808双端指令Host／Client各1/1、0测试警告／失败；客户端HALSpawnBomb实际服务器生成、立即3秒计时／销毁／范围同步、近车扣25及Heavy、远车HP不变已验证。三项炸弹普通测试包含落地、GAS免疫／降档及真实定时器；旧拾取GA测试已移除，旧结果不作为本轮证据。团队模型／prefab接线、真人画面／手感／扰动／晚加入／四机仍未验收。入口见[本体与测试指令](Combat/MVPB_Bomb_Rebuild_CN_v0.1.md)及[验证摘要](Combat/Bomb_Rebuild_Verification_20261022.json)。未改资产或保存Config，未提交／Push；VP-3与F4保持。

**2026-10-22 Editor静态外观自动校准已实现**：用户授权后，在原Editor模块新增 **Tools → HAL Vehicle Visual Alignment**，选择实际Vehicle Definition，测量LOD0刚性轮组参考几何与真实简单凸包底面，预览绝对Z；团队点击应用，仅改表现DA整体Z，支持Undo／Redo、过期预览拒绝、共面容差及共享DA保护，手动保存。无新Runtime规则、Blueprint／地图／保存DA编辑或VP-3施工。UE5.8.2 Editor／Game Development成功，最终真实渲染工具测试**11/11、0测试警告／失败**，面板截图已检查；364份资产SHA256全同，保留团队手调DA与之前接续文档。实际Car_01当前与建议Z均-134.5，重复分析显示Already aligned，不累积偏移。首次include目录与UI测试异步选择／DPI问题已修正，失败记录保留。详见[操作说明](Configuration/Vehicle_Visual_Alignment_Tool_CN_v0.1.md)和[本轮验证](Configuration/Vehicle_Visual_Alignment_Verification_20261022.json)。尚未做真人Apply／保存、新车型画面、完整玩法60项／游戏双端／5.8.3复测或F4；下一开发仍为VP-3具体方案，当前工具代码未提交／Push。

**2026-10-22静态对齐后续反馈与自动化建议（施工前记录）**：用户确认已手动调整、表现暂时正常，并提出多车型应有自动校准。团队当前表现DA修改保留；不虚构完整姿态／双端验收。当时Editor模块只有碰撞生成，提出按车型配置分析／预览、团队应用／Undo及共享DA保护的[方案](Presentation/MVPB_Vehicle_Model_Import_And_AnimBP_Setup_CN_v0.1.md#45-editor自动校准工具2026-10-22)。此方案现已实施，以上段及工具独立说明为准；VP-3未启动。

**2026-10-22 VP-3前静态对齐讨论**：团队反馈车轮底高于碰撞代理底面、游玩悬空。只读核对当前玩家DA／代理／模型文件与已有基线相同，代理底Z=-134.5、Car_01参考轮底约0；最新表现DA的变换未重新导出，134.5 cm只在零旋转／缩放1／原轮底0的条件下作为偏移起点。建议先通过车型表现DA的Relative Transform整体Z建立轮底＝物理支撑底面的静态基准，再设计VP-3分轮视觉伸缩。269 cm代理厚度大于约195 cm视觉总高，外形合理性需另行确认，正立对齐不代表侧翻／车顶也贴合。本轮只有分析与[手工校准方法](Presentation/MVPB_Vehicle_Model_Import_And_AnimBP_Setup_CN_v0.1.md#44-静态轮底与物理支撑平面校准2026-10-22)，未改资产、重编／运行或启动VP-3，也未实现自动校准工具；此前团队“无功能问题”反馈不扩写为静态外观校准完成。

**2026-10-22跨设备接续核对：本机已收到GitHub最新`main/fc9f2b962e6f5144a822cbf7d22b32114583ac42`，开工工作区干净，无需再次Pull。** 相较本会话原`fd41bbb`，已纳入车辆共享物理材质／规则、普通静止Movable地面支撑追加过滤、姿态驾驶门控、服务器长按自救及Local Config输入接线、VP-2车身俯仰／侧倾和各30°配置上限。`ABP_Car01`与`DA_Car01_Presentation`已随最新提交到达，不再把旧“未提交／尚未接线”作为当前状态。按各施工记录先后叠加清单核对155份源码／配置，50份字节完全一致、105份仅LF／CRLF差异，无其他差异；现有364份资产，AnimBP与30°快照一致，表现DA已是团队后续保存并提交的新版本，保留且不从旧快照回填。详见[本次接续复核](Presentation/MVPB_Project_Handoff_Review_20261022.json)及[VP-2接续说明](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md#9-跨设备接收与当前提交2026-10-22)。

当前开发入口仍为**VP-3视觉悬挂的具体方案／接口准备，尚未施工**；VP-1结束、自救基本功能真人确认、VP-2俯仰真人初测通过及参数后调保持。侧倾／漂移、完整画面／角色／FPS组合继续跟踪；VP-4、四点物理悬挂与自然落地辅助未实现，F4完整扰动／四台有线PC验收继续后置。本机实际UE**5.8.2-56702186**，本次收到的新施工验证使用另一设备UE**5.8.3-58210709**；后续联机前应统一补丁和构建。本轮只读核对并更新文档，未新编译／运行Unreal／自动化／读取最终DA字段或AnimGraph，不把另一设备60项、双端各1项及30°专项7项报告当成本机新复测，也未开始下一功能或提交／Push。下方各段“未提交”等说明保留各自历史时点，当前提交状态以本段及第2节为准。

**2026-10-22团队反馈：VP-2俯仰表现真人初测成功，参数后续再调。** 用户明确确认已能看到俯仰变化；记录为该通道基本表现链路初测通过，不再将“尚未接线／看不到俯仰”作为当前阻塞。各30°可配上限和现有DA／AnimBP保留，参数尚未冻结。详细见[团队反馈](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md#8-团队俯仰表现初测反馈2026-10-22)。下一开发入口可进入VP-3视觉悬挂的具体方案／接口准备，尚未施工；侧倾／漂移、双端角色及完整画面矩阵没有本次逐项反馈，不扩写为VP-2或F4全部验收。本轮仅更新文档，保留`main/846474d`下既有源码／测试／两份资产修改；没有新构建或运行，未提交／Push。

**2026-10-22 VP-2调参补充：俯仰／侧倾各通道配置上限由10°放宽到30°。** 用户反馈实际前后倾不明显并明确要求放宽；本轮基点`main/846474d656b11648becff51545e8e4216f949dd5`，VP-2代码已提交，保留团队`ABP_Car01`与`DA_Car01_Presentation`两份未提交修改。同步Editor ClampMax与运行时校验，默认值仍各4°，DA实际数值不改。UE5.8.3 Editor／Game成功，相关表现回归**7/7、0测试警告／失败**，364资产相对本轮开工SHA256全同。团队继续在原DA调参、做实际模型画面验收；本轮未重跑完整60项或双端，也未宣称30°观感／穿模通过。[配置及本轮记录](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md#7-俯仰侧倾上限放宽至302026-10-22)／[验证摘要](Presentation/MVPB_Vehicle_Presentation_VP2_Limit30_Verification_20261022.json)。本轮修改未提交／Push。

以下10月21日记录保留各自施工时点；当前提交状态、参数范围与本轮验证以上段为准。

**2026-10-21 VP-2代码与原生验证完成，待团队DA／AnimBP接线及画面验收。** 用户已确认“自救真人验收已经完成，功能可以工作，表现可以后面再优化”，自救基本功能不再列当前待办；本次随后获授权实现VP-2。基点`main/eae910e`，保留前次两份未提交接续文档，未改Unreal资产。新增世界加速度滤波驱动Chassis俯仰／侧倾、车型DA调参、空中／翻倒／自救衰减及校正重种；远端复用15Hz节奏复制一个表现许可bool。UE5.8.3 Editor／Game成功，普通**60/60**、最终双端**各1/1**通过，364资产SHA256全同。原网络首测因主机初始倒扣未产生驾驶表现失败，夹具正立接地就绪后严格复测通过；失败证据保留。无本次AnimBP节点、渲染／真人VP-2或F4最终验收。下一步按[VP-2配置与手工步骤](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md#4-animbp手工配置)在原车型DA启用Chassis，并增加一个Modify Bone节点；[验证摘要](Presentation/MVPB_Vehicle_Presentation_VP2_Verification_20261021.json)。未提交／Push。

以下VP-2接续核对、输入修正和自救施工记录保留各自时点，当前阶段与真人反馈以上段为准。

**2026-10-21 VP-2接续核对：下一开发环节为VP-2车身驾驶表现，当前完成范围核对，尚未开始VP-2代码施工。** 当前为`main/eae910e57d1b405b9a4c2e4e55a8bb38b1da7499`，检查开始时工作区干净；该提交已包含Self Right的Local Config字段／初始化复制、配置回归、团队Local DA／共享DA／IA／IMC。此前“修正未提交”不再是当前状态，远端Push未核验。VP-1结束、车轮恢复正常及支撑修正正向反馈保持；姿态门控／自救已有构建与自动化证据，但尚无输入修正后的真人验收反馈。本次仅核对代码与文档，没有新构建／运行，不追加测试通过结论。VP-2具体范围见[接续范围](Presentation/MVPB_Vehicle_Presentation_Rig_Plan_CN_v0.1.md#111-vp-2接续范围2026-10-21)；自救实测作为独立待办保留，可与表现准备并行。

以下输入修正及自救记录保留各自施工时点，提交状态以本段为准。

**2026-10-21输入入口修正：BP灰色Input是Definition模式的预期锁定，实际填写位置为玩家Vehicle Definition引用的Local Config DA → Input → Self Right Action。** 上次自救实现漏接该DA字段及初始化复制，且配置文档误写为在BP填写，现已补齐并修正文档；不切Legacy、不解除旧字段锁定。本次基点`main/c3f90ca`，Editor／Game成功，普通回归重新57/57、0测试错误／警告；新增Local可选／bool独立性、初始化前复制、旧Pawn字段锁定／Local可编辑及三个Enhanced Input绑定断言。保留团队共享DA／IMC／新IA，364资产前后全同。尚无本次修正后的真人输入或新双端验收；下文三组网络结果属于前次施工证据。[正确配置](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md#4-独立长按输入editor手工步骤)／[补充验证](Movement/MVPB_Vehicle_SelfRight_InputDefinitionFix_Verification_20261021.json)。未提交／Push。

**2026-10-21 当前进度：VP-1保持结束；用户实测确认支撑修正后“弹动有效减少”，姿态操控限制＋长按自救已完成代码、构建与自动化，待团队手工Local Config输入接线及真人验收。** 姿态／自救原施工基点为`main/f634292`，此后团队已提交为`c3f90ca`；最新输入接线补充以上文为准。新增共享DA的Recovery组、物理姿态门控、服务器长按许可、有限翻正、球／技能执行期限制及网络物理动作／历史。详见[实现与详细配置](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md)、[本轮验证](Movement/MVPB_Vehicle_Pose_And_SelfRighting_Verification_20261021.json)。当前修改未提交／Push；上一轮53项及各历史证据不改写为本轮结果。

已确认规则现已实现：近地持续低速侧翻1秒、线速≤150cm/s／角速≤60°/s，合格后长按1.5秒；松手或条件失效清零。有限翻正最多2秒，执行允许滚转短暂离地，成功仍需近地且姿态／角速达标；外部有效Medium／Heavy打断，执行结束后冷却3秒。实际接受执行时Free脱球、无额外释放冲量，执行中禁接球／发射／新技能，既有效果自然计时。规则在同一共享DA可调并纳入World冻结快照／签名；单纯侧翻和长按阶段保持原发射／技能。团队须手工创建独立IA_SelfRight、映射建议R，并在玩家Definition引用的Local Config DA中填写Self Right Action，不新增GA／GE或Event Graph。

本轮UE5.8.3 Editor／Game Development均成功；最终普通**57/57**、0测试错误／警告；新增真实Solver5工况／900步通过。实际MVPB_NetTest的30／60／120 NullRHI限帧双进程均各**1/1**，客户端重模拟分别29／44／61次；主机单向注入80ms，非实测RTT，临时签名2346082778一致。正式保存DA在普通回归的新签名915860017与临时资产路径不同，不跨夹具比较。363资产相对开工全同、Config未改。首次反射／测试编译问题、Editor占用、短暂离地过早退出及网络夹具启动同步问题已修复并保留失败证据。没有本轮真人接线／画面／几何手感或F4四机验收。

以下2026-10-20说明保留各次工作的历史基点与证据，不能将其中的“当前工程”当作10月21日HEAD。

2026-10-20车辆碰撞／控球点团队反馈：用户在调整并测试后确认“暂时没有功能性问题”。当前工程为干净`main/fd41bbb`，该提交包含生成器修复及验证留档、玩家Vehicle Definition／MVPB_NetTest更新和新`SM_Car01_Collision1`资产。本次把该反馈记录为团队基础功能初测未见问题；未提供逐项清单、双端／网络条件、四机或新日志，不扩写为完整验收。代理本轮仅更新文档，未重新编译、运行Unreal或导出最终资产数值；上次269 cm／Z92／旧DA引用是诊断时点数据，不代表团队调整后的基线。详见[操作文档第9.4节](Configuration/Vehicle_Collision_Generator_CN_v0.1.md#94-团队调整后测试反馈2026-10-20)。

2026-10-20车辆碰撞工具修复与出生排查（当前会话）：从`main/7d0f180`接续，团队已提交`SM_Car01_Collision`。修复生成器可视三角形绕序与UE约定相反造成的缺顶面外观，凸包顶点／物理规则不变；旧网格须读取配方生成新版本，参数Version1兼容，新结果记录GeneratorRevision2。UE5.8.2 Editor／Game Development成功，真实渲染工具回归6/6通过、0测试错误／警告，实际缩略图已检查。原日志明确出生碰撞拒绝于(-390,-1060,92)；新代理456×288×269 cm、起点Z92与地面高度不匹配，独立物理世界验证Z92重叠／Z200避开。保存玩家DA却仍引用旧200×140×70代理，独立当前保存配置Listen启动成功生成Pawn，故不能把原PIE内存配置等同保存基线。354份资产哈希未变，代理未改BP／DA／地图；待团队手动重新生成、接DA、核对四起点高度及球控制点，并做新代理真人双端回归。详见[操作与排查第9节](Configuration/Vehicle_Collision_Generator_CN_v0.1.md#9-缩略图面朝向修复与出生碰撞排查2026-10-20)和[本轮验证摘要](Configuration/Vehicle_Collision_Generator_WindingFix_Verification_20261020.json)。以下弹簧拳等其他工作记录保持。

2026-10-20弹簧拳第二步（最新）：已授权并完成阶段/起始时间/时长/距离/原点/拳次的RepNotify快照、客户端服务器时间插值与旧BP非复制标记兼容；权威命中不变，不新增比赛阶段门控。UE5.8.2 Editor/Game成功，专项1/1，普通回归49/50（唯一仍为Car_01接线后的旧无Rig假设）。真实正常时间60FPS、端口7799双进程以原生拳+现有BP混测，Host/Client各1/1通过，双方观察两拳伸出/收回且两玩家HP75/Medium，无客户端Action。原生初测通过；加速时间混测曾超时，改正常时间后相同严格断言通过，失败证据保留。不代表画面/手感、实际晚加入、网络扰动、持球/免疫/加速组合或四机完成；详见[同步摘要](Combat/SpringPunch_Replication_Verification_20261020.json)与[交接](Combat/MVPB_SpringPunch_Combat_Handoff_CN_v0.1.md)。保留团队4份资产改动，代理未改资产、未提交/Push。

2026-10-20弹簧拳屏幕扣血调试：新增BP默认开关Debug Damage On Screen，实际扣血时黄色文字显示6秒，包含拳头/目标名称、HP前后、实际扣血、有效冲击档位及Applied状态；只读展示，不改变结算，不额外向远端复制。保留团队DrivingLab GameMode/DA/地图改动。用户关闭Editor后，UE5.8.2 Editor/Game Development均构建成功。最新模块弹簧拳专项1/1通过、0测试警告/失败，报告Saved/SpringPunch/ScreenDebug_20261020/index.json。此为无视窗结算回归，尚未验证实际文字渲染及开关画面；前次49/50不冒充本轮结果。

2026-10-20 DrivingLab不能操控诊断：当前15:20:05本地Editor日志显示BP_DrivingLabVehicle_C_0使用DA_DrivingLabVehicle后报`TieredV1 rules require migrated VehicleDefinition.Impact`，随后物理模拟与控球关闭。源码PreInitializeComponents在配置校验失败时禁用模拟/碰撞，输入初始化也跳过；本次证据指向DrivingLab专用DA未迁移Impact，不能以更换PlayerState解决。团队须手动在DA顶层Impact将Version设TieredV1，保存后重新Play；代理未改资产，保留团队DrivingLab_Test.umap修改。本轮只读诊断，尚无修正后复测。HALGASStatus由HALPlayerController提供，普通DrivingLab控制器不保证支持，不能单凭命令无输出判断GAS缺失。

2026-10-20弹簧拳第一步施工：用户授权接入统一战斗结算，已新增可调25伤害/Medium测试默认值、独立机关SourceTag、每拳Action快照、固定出拳方向ExplicitTier、接受后去重与阶段/销毁清理；保持原时序及仅Extending车辆检测，不恢复炸弹、不新增联机表现复制。专项测试代码覆盖重复/多目标/下一拳、两类免疫、HP0/零伤害、拒绝后重试与Sweep。用户关闭Editor后，UE5.8.2 Editor/Game Win64 Development均构建成功。完整普通自动化49/50：新增SpringPunch.CombatLifecycle通过；唯一失败Presentation.OptionalRigPreservesGameplay仍假设保存玩家资产未选骨架Rig，与已接入Car_01的当前资产不符，本轮不修改表现测试/资产。没有本轮真人或Listen Server双端受击验收，联机阶段表现仍未实施。报告Saved/SpringPunch/Combat_20261020/index.json，共享证据见Docs/Combat/SpringPunch_Combat_Verification_20261020.json。详见[交接文件新增第一步](Combat/MVPB_SpringPunch_Combat_Handoff_CN_v0.1.md)，本轮未改资产或提交。

2026-10-20本机接续分析：实际为干净`main/a11228a`，团队Car_01接线资产已提交。用户说明已discard旧炸弹；本次检索Source／Config／Docs未发现HALBombActor、HALUseBomb、HALBombTest或PrepareStandaloneCombatTarget，旧炸弹验证不适用于当前源码。相较f4b17d2已纳入10-D物理线程修复、10-E免疫技能、10-F运动桥接及加速GA／GE、补球调试与VP-1表现底座。此设备`D:/Unreal Shit/UE_5.8`实际是5.8.2-56702186，与下文另一设备5.8.3历史环境不同；49项及双端结果均为既有记录，本轮仅核对Git、源码、配置与验证摘要，没有新编译、Unreal运行、资产字段导出或新验收。只补本接续说明，未恢复炸弹、修改玩法或资产；F4后置与表现待测事项保持。

最后更新：**2026-10-29（Asia/Tokyo）**。本文件维护最新进度、已确认决策和后续入口；详细规则、资产数值和验证证据由链接文档提供。10-F-1～3及加速GA、团队接线和基础双端实测均完成，相关资产和复核已提交`c825d8d`。**用户已决定将10-F-4完整集成／扰动／四机测试后置至MVP-B收尾，当前暂不施工F4-A；后续开发按用户指定事项推进。** 最终验收未完成，后续各项开发仍保留相关编译／自动化及必要双端基本检查。

当前**VP-2俯仰真人初测通过，参数精调后置**；下一开发可进入VP-3视觉悬挂方案／接口准备，尚未施工。各30°可配范围保留，侧倾／漂移、双端及完整画面回归继续跟踪，不据本次反馈写成全部验收。支撑过滤正向反馈、车轮正常及自救功能真人验收保持，自救表现以后优化。具体见[实施与团队反馈](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md)。VP-4与自然有限落地辅助尚未实现。

## 1. 先读什么、先检查什么

量产技能当前先读[Car04 蛮牛冲撞规则、配置与验收](Combat/Car04_Bull_Charge_CN_v0.1.md)，打滑带接续见[扩散打滑带规则、配置与验收](Combat/Vehicle_Slippery_Trail_Ability_CN_v0.1.md)，车型技能授予见[车型技能实现与DA迁移](Combat/MVPB_Vehicle_Ability_Loadout_Implementation_CN_v0.1.md)。

**2026-11-10 扩散打滑带油膜已完成本地接线。** 当前工作区已有遮罩、母材质和实例，技能 DA 的保存包包含该实例引用。用户已在游戏中看到油膜，后续完成了独立底纹流动与变色高光接线。最后一版接线按截图核对，最新观感和双端画面待团队复测。见[油膜特效速查](Presentation/Slippery_Trail_OilSheen_QuickGuide_CN.md)。

1. 完整阅读根目录 [AGENTS.md](../AGENTS.md) 和本文件。
2. 按用户当前指定的后续功能读取相关规格；涉及GAS／运动时先读[加速GA及团队反馈](Combat/MVPB_Phase10F3_Movement_Ability_CN_v0.1.md)、[手动日志](Combat/MVPB_Phase10F3Ability_ManualLogReview_20261019.json)、[施工验证](Combat/MVPB_Phase10F3Ability_Verification_20261019.json)、[F2桥接](Combat/MVPB_Phase10F2_Implementation_CN_v0.1.md)、[F1规格](Combat/MVPB_Phase10F1_Spec_CN_v0.1.md)与[保存基线](Combat/MVPB_Phase10F1_Baseline_20261017.json)。F1～F3及团队接线／基础反馈完成，331资产已提交；[F4方案](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)与[总计划](Combat/MVPB_Phase10F_Movement_Effects_Plan_CN_v0.1.md)保留，但A～D整体按用户决定后置至MVP-B收尾，不能沿旧入口自动开工。10-D／10-E欠项见各自[施工记录](Combat/MVPB_Phase10D_Implementation_CN_v0.1.md)、[实施](Combat/MVPB_Phase10E_Implementation_CN_v0.1.md)及[手动日志](Combat/MVPB_Phase10E_ManualLogReview_20261016.json)。规则背景见[GAS总体方案](Combat/MVPB_GAS_Architecture_Overview_CN_v0.1.md)，双端启动见[操作说明](Configuration/MVPB_DebugHUD_And_GAS_Test_CN_v0.1.md)，DA职责见[速查](Configuration/MVPB_DataAsset_QuickGuide_CN_v1.0.md)。
3. 以 [MVP-B Listen Server v0.2 计划](MVP_B_Listen_Server_Plan_CN_v0.2.md)和对应 `Docs/Networking/` 实施记录确认已存在的网络边界。早期 [MVP-A→MVP-B 交接](MVP_A_to_MVP_B_Handoff_CN_v1.0.md)用于追踪背景，不能把旧参数当成当前 DA。
4. 在本机执行 `git status --short`、`git branch --show-current`、`git rev-parse HEAD`，对照 `.uproject`、模块 `Build.cs`、`Source/`、`Config/`、相关测试和邻近说明；以 Editor 的 About 或引擎 `Engine/Build/Build.version` 核对实际补丁版本。
5. 修改前先汇报当前工程与本记录的差异和本次任务范围；保留已有改动。不因历史记录写了“下一步”就自动施工未获用户授权的环节。

车辆接续先读[VP-3实施／手工接线](Presentation/MVPB_Vehicle_Presentation_VP3_Implementation_CN_v0.1.md)及[本轮摘要](Presentation/MVPB_Vehicle_Presentation_VP3_Verification_20261022.json)，既有Chassis节点与调参见[VP-2](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md)。物理背景见[姿态／自救](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md)、[已确认计划](Movement/MVPB_Vehicle_Stability_And_SelfRighting_Plan_CN_v0.1.md)、[支撑分类](Movement/MVPB_Vehicle_Support_Classification_Fix_CN_v0.1.md)及[共享DA／PM](Movement/MVPB_Vehicle_Stability_MinimalFix_CN_v0.1.md)。保持Movable普通地面、共享规则和静态高度校准；VP-3只消费参考几何并输出骨骼位移，团队增加四悬挂节点。自救许可／回滚和副作用边界、VP-1结束及F4后置保持。

特效接续先读[电弧特效制作步骤](Presentation/MVPB_ElectricArc_FX_Authoring_CN_v0.1.md)：该文件只覆盖两柱之间电弧的Niagara本体制作（资产位置与命名、System建立与模块版本相依性修正、端点开放为User Parameter、三层Ribbon叠加、材质及团队手动验证／排错），不含碰撞体、伤害判定、机关状态机与球是否被电流阻挡的玩法规则。按AGENTS.md第8.3节由团队在Editor内手动完成；当前仓库只到达素材与步骤留档，没有电流画面接入或画面验收，不把素材导入当作特效接线完成。机关侧已另行实现，接线以[电墙说明](Combat/MVPB_Electric_Wall_CN_v0.1.md)为准：`AHALElectricWallGimmick`的两柱实体Capsule＋Query Only电墙、独立Authority Bounce Back、共用Stun及`UHALShockedPresentationComponent`表现接口均已有代码与双端证据。电弧文件第9／10节写于该实现之前，其中“机关Actor／状态机待建立”“伤害走CombatResolver”“柱子是否带实体碰撞未确认”“Build.cs不需修改”四项已过期：机关回弹不经CombatResolver、没有Idle／Warning／Active状态机，`Niagara`已在Private依赖中。仍未确认的是球碰到电流的规则（当前球穿墙不附加效果）与撞歪柱子是否中断电流。车身受电特效已于2026-11-01实现并接到`BP_RoundedVehiclePawn`，做法与两柱电弧不同：几何在C++算（`HALShockArcGeometry`），Niagara只负责画线，制作步骤文件按用户决定不入库，接续请读本文件第1节顶部该段与`UHALShockedPresentationComponent`源码；其余四个车辆BP尚未指派。两柱电流画面仍未接入。

**2026-10-30进入房间／选车／练习／正式关卡流程讨论，尚未施工（恢复原时点记录）。** 当时实际`main/4209445`、开工工作区干净，已包含Car_02配置更新及上一轮客户端球相对表现；球改造仍由团队复测，不追加四机通过结论。用户提出街机入口→加入房间→选车→练习等待Host→全员进入正式关卡，已结合现有Host／Join、唯一默认Pawn、两状态GameState及PlayerState ASC检查形成[v0.1讨论草案](Networking/Room_VehicleSelection_Practice_Match_Plan_CN_v0.1.md)。建议选车与练习共用房间地图、按每位玩家确认进入练习，Host开始时锁定名单并统一ServerTravel，正式地图等待全员就绪后倒计时；该日尚待确认的六项已于11月2日确认，当前按v0.2执行。

该次检查指出HP／Combat／Impact均有一次初始化保护，换车型／开启新局须补正式重置契约，不能只重复Possess；练习权限也不能只把Waiting改成Playing。10月30日仅只读核对及草案／入口留档，无C++／Config／资产修改或新构建／测试；本段恢复历史依据，不代表本次重新检查或完成了相应功能。

## 2. MVP-B 工程位置与阶段留档

量产阶段当前状态以页首日期记录为准。下表保留 MVP-B 接续时的工程快照。

| 项目 | 最新接续核对结果与既有团队反馈 |
|---|---|
| 开发阶段 | 状态FX：FX-1～3完成；FX-4A～4D与FX-5程序及自动化通过。团队保存模板／共享电击配方接线和实际画面待验。后续FX-6组合／性能以及GF／相机／发布包／四机欠项独立 |
| 当前本机分支／代码提交 | `main / 06eb9ab`；保留开工前两份文档改动，A仅新增3份C++及留档，1193资产不变，本轮未提交／Push |
| 引擎基线 | 本机 **5.8.3-58210709**，最终R2 Editor／Game与161原生通过；旧FX-1／2环境、148／153项及本轮R1保留原时点，本轮没有新联机／GPU／Cook结论 |
| 本机运行前置 | A无需Editor接线，继续原电击；下一B才组装每车组件并接资源／目标，默认目录Ready和最终绘制独立验收，初次资源等待不新增驾驶／开局门槛 |
| 工程绑定 | `.uproject` 的 `EngineAssociation="5.8"`，不代表所有电脑的补丁版本相同 |
| GAS 当前实现 | Definition.Abilities显式V1按当前Pawn授予唯一Primary并加入Ready；旧DefaultAbilities只用于Legacy。换车／新局清理沿GF-2，普通重绑保留计时；两份保存DA仍Legacy，待团队迁移Car01加速／Car02免疫 |
| 场地持续运动效果 | 原生区域无ASC／GA，服务器按来源逐参数选最强并施加两类内部GE；加速度／上限／抓地与普通Buff增量合并，合并版3物理历史保存恢复曲线、实际消费值与Stun权限，DA不改；无需修改PlayerState.DefaultAbilities |
| 当前 HP／冲击 | 三档／免疫／降档／水平预设及回弹保持；GF-2练习规则在标准Resolver抑制扣血，冲击／脱球／命中消费继续，Playing正常扣血。伤害与冲击抗性继续独立，旧Room关闭入口行为保持 |
| 当前 Editor 接线 | 车辆Definition.Abilities需手动设V1／唯一Primary；GA／GE／IA／IMC不重建。房间地图和BP_HALPlayerState引用保持；新WBP仍按独立接口配置，空类保留原生回退 |
| 电墙／Bounce Back | 远端ac14e60已接收：独立Authority水平回弹100cm／0.3秒、每次进入一次，保留Stun／shocked；与静态Visual可选集成后本机重新构建、普通回归及电墙双端通过，真人手感仍按机关说明复测 |
| 测试地图 | Entry→Host进入`/Game/Maps/MVPB_PracticeRoom`，GF-4正式目标`/Game/Maps/MVPB_Match`已保存；本轮UI及双端均实际使用这些保存引用，无临时RoundMap覆盖。旧NetTest的Room Config仍供旧兼容回归，旧`/Game/Maps/Test`按专项目的使用 |

代码提交号是本次交接的比较基点，后续提交会变化。F1保存基线及F2历史证据保持；10月17日F3的原生GE／调试入口证据见原摘要。本轮运动GA补充不改保存配置、DA／地图或F2物理协议；329资产前后哈希全同，134份源码／配置及最终两Target／45项／正常GA双端证据见[本轮摘要](Combat/MVPB_Phase10F3Ability_Verification_20261019.json)。首次Live Coding阻塞、补Period防护前的通过记录独立保留；历史结果不冒充本次复测。

上段329资产是代码施工时的保护结果。随后团队新建2份加速资产并修改PlayerState，本次已记录331份资产及3项变化哈希，134份源码／配置未变化；新结论见[手动复核](Combat/MVPB_Phase10F3Ability_ManualLogReview_20261019.json)。本次没有重跑构建／45项自动化，也不把团队基本双端反馈写成F4完整验收。

随后c825d8d已提交上述3项资产和手动复核文档；不再把“资产未提交”作为当前阻塞。新的[F4方案](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)只记录待执行矩阵、分工和门槛，不代表F4测试已完成。

## 3. 已完成与验收状态

| 环节 | 已有结果 | 证据边界 |
|---|---|---|
| FX-3通用运行层 | A纯实例／来源及阶段模型完成，两Target、新增8项及161原生通过；下一B资源／生命周期，C观察／诊断后续 | A尚未挂车或创建资源，不代表整FX-3／渲染／电击迁移完成；见[A实施](Presentation/MVPB_Vehicle_Status_FX3A_Implementation_CN_v0.1.md) |
| FX-2共用配置／资源 | 定义／Tag目录、项目默认／车型例外、异步缓存与Cook收集已交付；历史两Target、153原生及GF7双端通过，团队后续已建两DA并配置默认 | 本轮未检查新保存DA的Ready／Cook／画面；旧未配置夹具证据保留原时点，见[实施](Presentation/MVPB_Vehicle_Status_FX2_Implementation_CN_v0.1.md) |
| FX-1统一外观目标 | 有效Mesh／变换／尺寸快照、重绑失效及诊断入口完成；历史两Target与148原生通过 | 不代表真实表面粒子适配或新FX已接管电击；见[实施](Presentation/MVPB_Vehicle_Status_FX1_Implementation_CN_v0.1.md) |
| Client电墙后镜头缩近 | 已拆分首次就绪、运行恢复、输入清理及全量重置；两Target、144原生及Entry无注入／80ms＋替换UI双端等通过，实测Recovery时Menu=0且距离连续 | 1190资产保持；瞬态夹具并非原DA画面验收，团队需复测原场景／硬件。见[实施与复测](Presentation/MVPB_Camera_Input_Boundaries_CN_v0.1.md) |
| GF-7基础比赛闭环 | Playing计时、HP批次淘汰／弃权、最后存活／超时并列／全灭、独立观战、结果及自动／Host提前返房完成；两Target、138原生和8份联机角色报告通过 | 原生回退与Designer接口有D3D12证据；1183资产全同，无代改BP／DA。团队真人、发布包／四机待验收，见[配置与步骤](Networking/GF7_Match_Loop_Implementation_CN_v0.1.md) |
| 临时车顶血条 | 编号居中上移、按真实HP／MaxHP绘制绿黄红血条；Editor／Game、Health2项与原生UI1项通过 | 原生默认HUD，无资产接线；真实双端受击／血条外观及重新打包待团队，见[说明](Presentation/MVPB_Temporary_Overhead_HealthBar_CN_v0.1.md) |
| 车型主技能配置 | V1／Legacy、当前Pawn来源授予、整体校验及Ready、显式换车与迟到清理完成；两Target、134普通、V1／Legacy双端两局均通过 | 1183资产全同；保存DA仍Legacy，团队迁移并真人复测，见[配置步骤](Combat/MVPB_Vehicle_Ability_Loadout_Implementation_CN_v0.1.md)及[验证](Combat/MVPB_Vehicle_Ability_Loadout_Verification_20261104.json) |
| 打包前车辆手感调整 | 共享车辆Gravity Scale／有效重力翻滚、普通墙独立脱球门槛、XZ侧面圆角生成与兼容完成；两Target、132普通、5工具、2UI及1.5倍双端／两局均通过 | 1183资产全同，286源码／配置；DA仍1、新代理未生成／引用，待团队手感；见[配置与引用](Movement/MVPB_Prepackage_Gameplay_Polish_CN_v0.1.md)及[验证](Movement/MVPB_Prepackage_Gameplay_Polish_Verification_20261104.json) |
| GF-6工程收尾 | 服务器共用返回、Exhibition非Shipping主机测试开关、Cook根与只读资源预检／循环工具完成；Editor／Game、130普通、四进程两局／双端两局各端1项、原生UI1项通过 | 1183资产全同，285源码／配置哈希；预检86项目包。实际Cook／发布包、完整异常／真人／四机待验收，见[操作步骤](Networking/GF6_Engineering_Closeout_CN_v0.1.md)及[验证](Networking/GF6_Engineering_Verification_20261103.json) |
| GF-5 Widget Blueprint接口补充 | 可配置入口／Room／Pause类、Designer完整布局及原生回退、只读视图／变化事件、语义动作／焦点和状态即时同步完成；最终两Target、129普通、内存UMG／原生UI各1项、混合界面双端两局各1项通过 | 1183资产全同，285源码／配置哈希；实际新WBP／Designer／动画及真人硬件待团队，详见[接线](Networking/GF5_Widget_Blueprint_Interfaces_CN_v0.1.md)与[验证](Networking/GF5_Widget_Interfaces_Verification_20261103.json) |
| GF-5入口与表现 | 原生待机／Host／Join、四席位／静态车型卡、可选品牌素材、焦点／输入切换及加载错误完成；最终两Target、127普通、D3D12 UI完整流程1项及双端两局各1项通过 | 1183资产全同、278源码／配置留档；真实手柄／鼠标、按住键／失焦、素材及四机待团队，下一开发GF-6。详见[实施与接线](Networking/GF5_Arcade_FrontEnd_Implementation_CN_v0.1.md) |
| GF-4真实跨图与开赛 | 原交付证据保留；团队已保存正式地图与Room DA，用户报告基础初测暂无问题；本轮使用保存引用的双端连续两局重新通过 | 后续更多测试仍待进行，不追加真人细项／网络扰动／四机结论，见[跟进](Networking/GF4_Round_Travel_Implementation_CN_v0.1.md#7-2026-11-03团队初测及gf-5接续) |
| GF-3灰盒房间／选车练习 | 原代码／构建／122项／双端证据保留；团队已接Room BP／地图／GI入口，并报告基础测试暂无问题 | 未提供逐项／网络工况／四机日志，不扩写；真实跨图已由GF-4接续，详见[更新](Networking/GF3_Practice_Room_Implementation_CN_v0.1.md#7-2026-11-03团队反馈及gf-4接续) |
| GF-2换车／新局生命周期 | 显式权威转换、Avatar复制身份／幂等、新车型种子和旧车清理、练习HP抑制完成；原两Target／118普通证据保留，本轮旧NetTest真实双端兼容复验各1项通过 | 原1180资产保护结论保持，GF-3已接生产生成并补首次无车状态；GF-4已接跨图新局初始化。详见[实施](Networking/GF2_Vehicle_Lifecycle_Implementation_CN_v0.1.md) |
| GF-1数据与权限基础 | 车型目录／人数Profile／玩家及房间复制状态／权威请求／权限快照完成；原最终两Target、114普通、真实双端各1项通过 | 原1177资产保护结论保持；三份DA及Profile已正确，练习HP已由GF-2消费，生产房间代码已由GF-3接入；真人配置与跨图分开验收。详见[实施](Networking/GF1_Data_And_Permissions_Implementation_CN_v0.1.md) |
| 车辆表现路径校验 | 驾驶车允许有效骨骼或静态路径之一，被动Box／球保持静态；独立96项历史保留，与电墙集成后本机两Target、103普通、3工具及表现双端各1项通过 | 缺省静态用原生临时副本验证，双端为现有保存车；团队实际清空DA／保存／画面仍待测，见[配置第6节](Configuration/MVPB_DataAsset_QuickGuide_CN_v1.0.md#6-静态回退可选团队配置与验证2026-10-29) |
| 动态相机0.1 | 实现已随16b39eb提交，上一设备5.8.3的Editor／Game、93项、真实视口双端保持；10月29日本机5.8.2修复主球编辑保护，Editor／94项通过 | 团队正在Camera DA／IA／IMC接线，主球待重新勾选保存；本步1172资产全同。真实输入、驾驶／UI／斜坡及双端／扰动仍待验，见[实施第9节](Presentation/MVPB_Dynamic_Camera_Implementation_CN_v0.1.md#9-主球勾选置灰原因与修复2026-10-29) |
| MVP-A 基础玩法、MVP-B DA 迁移 | 用户报告基础玩法及 DA 更新后初测无功能问题；保存的测试参数继续使用 | 危险区、模型／Rig 表现原已延期；早期迁移表不能覆盖新 DA |
| Listen Server 阶段 1～8 | 已有服务器权威输入、车辆 Resimulation、远端 PI、球状态／控球表现、发射后坐及退出清理；用户报告对应双人功能测试暂未见问题 | 各自证据见 `Docs/Networking/`；用户反馈、自动化测试和量化网络门槛分别记录，不能互相替代 |
| 阶段 9 菜单与四机验收 | Host／Join、主机 Start、诊断代码与资产已存在；用户报告线下双人测试暂未有其他问题 | [阶段 9 记录](Networking/MVPB_Listen_Phase9_Implementation_CN_v0.1.md)保留未关闭事项；尚无四台真实有线 PC 最终验收结果／记录，不标记第一里程碑全部通过 |
| 10-A | 规格、保存 DA 数值／哈希、总体说明及跨设备入口完成；本次复核 14/14 基线文件未变化 | 文档和只读检查环节；未接入 GAS，未修改玩法源码或资产 |
| 10-B | 两个 Development Target 编译、自动化 16/16、Test／NetTest 保存接线确认；双向命中 HP 日志复核通过。2026-10-14 用户确认驾驶／持球／冲击测试完成，并同意进入 10-C | 新结论来自团队反馈，测试日志尚未提供／可能未上传；未说明测试补丁版本、逐地图／被动车子项及约 100 ms RTT／丢包／抖动工况，不把这些细项写成已有日志证明 |
| 10-B 调试显示补充 | 新增 C++ 半透明只读 HUD，`hal.DebugHUD 1/0`；`HALGASStatus` 增加 PlayerId；最终两个 Target 编译、既有自动化 16/16 及双端初始数值／实际渲染检查通过 | 自动化在最终字号微调前执行，微调后重新编译并看截图；不改玩法或资产。后续驾驶／持球／冲击回归已有团队完成反馈，具体延迟工况和 HUD 观感未单独留档，详见操作说明和 [HUD 验证摘要](Configuration/MVPB_DebugHUD_Verification_20261013.json) |
| 10-C | 共用Action／请求／结果、快照、战斗Attribute和独立伤害完成；原UE5.8.3两个Target／22项自动化通过。用户2026-10-15报告另一设备已完成相关资产更新和测试，三个DA迁移已提交 | 10-D本轮只读导出确认三份Damage/Combat配置为AttackSnapshotV1，并验证当前代码下的初始HP复制；没有重做10-C真人扰动验收，不覆盖用户已完成反馈 |
| 10-D | 全来源档位、GAS降档／免疫、统一脱球及动作、DA迁移和线程修复已提交；上一轮两个Target／28项自动化通过。用户本轮确认击飞效果恢复，接受先保留档位、实际力度以后调整 | 本轮是团队功能反馈，未提供新日志或角色／网络条件细项。其余三档／抗性／连续接触及扰动回归仍待补齐，不写成完整验收；5.8.3未复测 |
| 10-E | 原生GA／GE、授予／清理、输入／调试完成；原Editor／Game、33/33普通回归与真实激活smoke通过。团队接线已提交，报告双端免疫开始／消失及效果正常；本轮两局五次命中确认双方受击、免疫Heavy→None且伤害25、HP收敛，客户端目标后续命中恢复Heavy | 手动日志未启用hal.AbilityLog，缺精确8／20计时／重复请求／实际GE类名记录；团队一般观感与日志子项分别留档。按住输入、目标持球保球、活动效果退出／换Avatar、100ms RTT／1%丢包／20ms抖动及四机未有本次新实测证据，未新增正式技能演出 |
| 独立调试补球 | 新HALSpawnBall由服务器按调用者车辆生成配置的BP_BasicBall；Editor／Game和29/29自动化通过。实际NetTest主机执行成功、客户端接收动态球通过，324资产前后未变 | 客户端实际输入命令发起RPC、驾驶捕获／发射及真人观感待团队反馈；无网络扰动测试，不算10-E技能验证，见独立验证JSON |
| 10-F | F1／F2规格、基线、桥接及F3正式GE／调试入口完成；原两Target／42项／80ms调试GE证据保持，见[F3实施](Combat/MVPB_Phase10F3_Implementation_CN_v0.1.md)及[验证](Combat/MVPB_Phase10F3_Verification_20261017.json)。加速GA追加证据及团队反馈见下一行 | F3状态共存和基本双端反馈不是完整物理组合；F4驾驶／控球／发射／冲击／免疫／退出、完整扰动与四机待完成。F2回滚驾驶补充回调仍需覆盖组合体验 |
| 10-F-3技能补充 | 加速GA、Tag、可配非周期GE／许可／刷新、3项新增普通测试、真实GA双端夹具及配置步骤完成；之前Editor／Game、45/45及双端通过。团队接线与基础双端反馈／日志复核通过，c825d8d已提交；主机／客户端各4次激活、4段效果到期恢复正常 | 完整输入边沿／发射／冲击／免疫／带GE退出／换Avatar及F4网络矩阵仍待覆盖；见[详细记录](Combat/MVPB_Phase10F3_Movement_Ability_CN_v0.1.md) |
| 10-F-4准备／后置 | [F4执行方案](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)已留档，HALNetMetrics缺RTT等诊断亦列待补；用户决定整个A～D在MVP-B末尾统一执行，当前暂不施工 | 完整集成／网络扰动／四机仍未验收；旧45项／80ms单向smoke不代替F4。后续开发继续相关编译／自动化／必要双端验证，阻塞缺陷及时修复 |
| 独立弹簧拳接入分工 | 已接统一CombatResolver伤害／档位、每拳去重和阶段RepNotify／服务器时间插值；此前专项与基本双端证据见[既有摘要](Combat/SpringPunch_Replication_Verification_20261020.json)。后续仍由另一成员负责，见[交接](Combat/MVPB_SpringPunch_Combat_Handoff_CN_v0.1.md) | 本次仅读码确认已有接入，不修改机关或重跑验证；不再将其写成尚无战斗／复制。表现手感、实际晚加入、完整组合／扰动及四机验收边界保持 |
| 炸弹本体与开发生成指令 | `8571fa4`已接收实体炸弹、独立爆炸范围、统一GAS伤害／TieredV1冲击和HALSpawnBomb；此前两Target／63项及双端各1项证据见[实施](Combat/MVPB_Bomb_Rebuild_CN_v0.1.md)及[摘要](Combat/Bomb_Rebuild_Verification_20261022.json) | 道具框架／拾取／施放尚未决定，不恢复已移除的库存／道具GA／输入；模型prefab、真人手感、晚加入／扰动／四机仍待验收，本次未做合并后复测 |
| 车辆表现VP-0／VP-1已结束 | 原生实现及10月19日数据双端历史证据保持，素材／接线已提交，用户确认VP-1收尾；随后车轮配置已修复、表现恢复正常的反馈保持。旧无Rig测试已修复，见[收尾](Presentation/MVPB_Vehicle_Presentation_VP1_Implementation_CN_v0.1.md) | 不重开已解决的轮轴／半径排查，不虚构最新保存数值；完整扰动／四机未验收。VP-2以新行和摘要为准 |
| VP-2车身驾驶表现 | C++与原生验证完成，30°范围补充两Target／7项回归通过；用户确认实际俯仰初测成功，参数以后调整，见[反馈](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md#8-团队俯仰表现初测反馈2026-10-22)。此前60项／双端历史证据保留 | 真人反馈限俯仰可见，不追加侧倾／漂移、穿模／裁剪或完整双端／屏幕FPS结论；参数未冻结。VP-3以新行记录为准 |
| VP-3视觉悬挂 | 原两Target／普通67/67／双端各1/1保持；2026-10-23团队确认配置完成且画面初测无问题，ABP／DA已提交，见[反馈](Presentation/MVPB_Vehicle_Presentation_VP3_Implementation_CN_v0.1.md#7-团队接线与画面初测反馈2026-10-23) | 不要求重复接线；未据一般反馈补写逐场景／角色／FPS／扰动及四机通过，也无本次新编译／运行。物理悬挂未实现 |
| VP-4A发射视觉回弹 | `3d42dd8`实现及团队调参已提交，10月23日基础画面初测通过，见[反馈](Presentation/MVPB_Vehicle_Presentation_VP4A_Implementation_CN_v0.1.md#7-团队画面初测与下一步2026-10-23)；本轮75项包含其原生回归 | 本轮未重做4A真人逐角色／FPS／拒绝／扰动或四机矩阵；旧885／887资产及5.8.2结果保留原时点，当前1154资产／模型修复不回填旧验收 |
| VP-4B有效冲击回弹 | 统一结果／档位／方向、支撑过滤、解析包络、去重通知／生命周期及合成已完成并提交；原UE5.8.3两Target、75/75及双端各1/1证据保持，见[实现／配置](Presentation/MVPB_Vehicle_Presentation_VP4B_Implementation_CN_v0.1.md) | 用户2026-10-27反馈VP总体表现无大问题，记录为基础初测正向反馈；未细分的复杂来源／角色／FPS／网络／生命周期验收随4C统一放到MVP-B收尾，不要求现在重复接线 |
| 轮胎拖尾试装 | 实现及原两Target、88/88、真实RHI1/1、连续两轮Host／Client各1/1保持；用户本次报告测试暂时没有较大问题，见[配置与反馈](Presentation/MVPB_Vehicle_TireSkid_Implementation_CN_v0.1.md#7-团队初测反馈与后续入口2026-10-28) | 基础反馈不扩写为逐车型／角色／斜坡Facing或完整组合验收；Car_01表现DA与地图本地修改保留，当前不重新导出保存值或运行测试 |
| 水平推送预设扩展 | 三档＋Short／Standard／Long／ExtraLong，公共DA与球／拳／爆炸快照完成；完整两Target／强制Unity、77/77及真实双端各1/1通过，见[配置与证据](Combat/MVPB_Impact_Push_Presets_CN_v0.1.md) | 旧Standard固定×1，1154资产全同；前两轮夹具超时及修正留档，最终无测试警告／失败。实际模型／真人距离、驾驶组合和F4未验收 |
| Car_01后续模型修复 | 最新`5c84320`已接收SKM_Car_01修复；当前模型文件哈希见[接续摘要](Presentation/MVPB_Project_Handoff_Review_20261026.json) | 本轮仅核对提交／文件，未重新导出骨骼／几何或运行画面，不以10月23日反馈代替修复后的新验收 |
| Editor静态轮底校准 | 自动测量／预览、显式应用、事务撤销及共享／过期保护完成；本机两Target／11项工具回归通过，见[操作说明](Configuration/Vehicle_Visual_Alignment_Tool_CN_v0.1.md) | 只处理参考姿态与正立基准，无运行时校准或碰撞拟合；团队实际Apply／保存、新车型及各姿态画面仍待反馈，不替代VP-3／完整玩法矩阵 |
| Car_01素材检查 | 2e4176d新增20份素材全部可加载；17骨骼直接父级符合约定，5823源顶点均单骨骼权重1，四轮绑定各自spin；未见缺失依赖，351资产未改，见[检查](Presentation/MVPB_Car01_AssetInspection_CN_v0.1.md)／[原始数据](Presentation/MVPB_Car01_AssetInspection_20261020.json) | 仅NullRHI只读几何／资产数据检查；无真实AnimGraph姿态／材质渲染／性能／玩法双端验证。1个LOD／12个材质section留待实际四车测量，PA保留但不作为当前车辆物理根 |
| 车辆反弹／姿态门控／长按自救 | 原10月21日基本功能真人反馈保留；10月29日侧翻边界／Block反馈完成，本机两Target、104普通及自救双端通过，详见[当前调参与验证](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md#9-侧面着地自救与较快翻正响应2026-10-29) | 65°及较快响应需团队保存DA并复测实际地图；未将原局唯一原因、夹缝脱困、完整攻击组合／扰动／四机记为通过，有限自然落地辅助未实现 |

本次 10-B 构建证据：UE 5.8.2 的 Editor/Game Development Target 均成功，`HAL.FutureCreation` 自动化 16/16 通过，报告原位于 `Saved/Automation/MVPB_Phase10B_Final/index.json`；隐藏双进程 Join succeeded，客户端收到两辆车的 GAS HP=100／MaxHP=100。历史阶段 9 的 14/14 结果保留在对应记录中。本次没有真人驾驶／命中及延迟测试。`Saved/` 不随 Git 交接，新电脑不存在这些报告不代表测试失败，也不能声称已重跑；共享摘要见 10-B Verification JSON。

上述是原施工验证的边界。团队随后进行了真人双向命中测试，本次已核对双方日志的三次 HP 变化，见 [HP 实测日志复核](Combat/MVPB_Phase10B_ManualHP_LogReview_20261013.json)和 [10-B 实施文档第 5.1 节](Combat/MVPB_Phase10B_GAS_Foundation_CN_v0.1.md)。另只读复核 [团队接线后的两个地图](Combat/MVPB_Phase10B_EditorBindings_PostTeam_20261013.json)，319 资产检查前后未变。此次没有新编译／新跑自动化，不将历史报告当作本次复测。

2026-10-14 接续补充：用户明确报告「10-B 的驾驶／持球／冲击都已经测试完毕，可以开始 10-C」，并说明测试日志可能未上传。该反馈作为进入 10-C 的授权及团队功能回归结论，详细边界见 [10-B 第 5.2 节](Combat/MVPB_Phase10B_GAS_Foundation_CN_v0.1.md#52-团队回归完成与进入-10-c2026-10-14)。本轮只更新文档，没有新编译、自动化或真人测试；原始证据保持原样。

随后用户确认规则并授权10-C施工。本轮新验证使用UE5.8.3：Editor/Game成功，全部自动化22/22通过（最终0警告／0失败），只读加载三份Combat DA和两个地图成功，整个施工／验证前后320资产哈希均未变；隐藏主机监听7786、客户端Join succeeded并收到两辆车HP100。详细证据及测试夹具修正见 [10-C实施记录](Combat/MVPB_Phase10C_Implementation_CN_v0.1.md)和 [验证JSON](Combat/MVPB_Phase10C_Verification_20261014.json)。未执行团队V1资产接线或真人网络模拟，不将代码完成等同阶段全部验收。

以上是10月14日交付时点。2026-10-15用户报告已在另一电脑完成10-C及相关资产更新／测试；当前1eb02c4已包含三个DA迁移，旧的“等待迁移”不再作为当前进度。接收初时本机仍有5.8.2和旧DLL，见[当时Handoff记录](Combat/MVPB_Phase10C_Handoff_20261015.json)。随后同日获授权进行10-D，已在本机可用5.8.2完整构建Editor／Game、27/27自动化通过，并只读导出四份DA；隐藏Listen Server监听7791、Join succeeded，客户端收到两辆车HP100，主机显示CombatVersion=1／ImpactReady=1／ImpactVersion=0。320资产整个施工前后未变，两个专用进程已清理。此smoke只证明现有保存资产的启动／初始复制，新版TieredV1冲击仍需团队迁移和真人验收；证据独立保存于[10-D摘要](Combat/MVPB_Phase10D_Verification_20261015.json)。

## 4. 已确认的阶段 10 决策

- GAS 管规则、结果和状态生命周期；车辆／球的 Chaos 移动、碰撞与 Network Physics 仍由 Actor／Component 管理。核心逻辑 C++，蓝图轻量配置／表现，由团队手工接线。
- 玩家 `PlayerState` 是 ASC Owner，当前车辆 Pawn 是 Avatar；玩家 ASC 采用 Mixed Replication。球、普通道具与机关默认无 ASC，球携带轻量攻击来源／效果载荷。
- 玩家主动攻击采用 `CapturedAttack × max(0, ActionMultiplier + sum(MultiplierDeltas))`。动作倍率只有一个，增量可正可负；机关用自身基础伤害。
- 飞行球在服务器接受发射时保存攻击力、倍率和稳定来源；发射者退出或飞行期间 Buff 到期不回算这次攻击。目标防御／免疫在命中时读取。
- 伤害和冲击是独立通用入口：允许 0 伤害强冲击、高伤害低冲击／无冲击。在正式淘汰规则加入前，HP 保持 0，但有效冲击仍生效。
- 2026-10-14 已确认 10-C 的 25×1 迁移、目标受伤倍率初值1且无平减护甲、伤害免疫后普通球仍消费攻击回 Free。伤害抵抗／免疫只改伤害分支，冲击抵抗／免疫只改冲击分支；两者不得互相作为接受条件或使用减免后的伤害反推冲击。冲击免疫不自动取消 Chaos 自然碰撞，具体冲击抗性公式留给 10-D。
- 冲击可按来源的物理测量选档，也可直接指定档位；最低脱球档位属于目标响应。统一档位结果，不强迫球速与车车碰撞冲量共用输入阈值。
- 10-D已定稿：主动攻击固定档；自然碰撞按公共标尺选档并由来源能力封顶；GAS整数降档最终0～3，免疫另用Tag，脱球比较有效档位。保留自然Chaos阻挡／推挤，普通碰撞不扣血；详细连续接触和物理补足边界见10-D施工记录。
- 车辆已调 DA 是基础值；局内加速、减速、低抓地等由 GE 影响有效参数，移动组件实际施力。变化参数必须能在网络物理重模拟中按帧复现，GAS Prediction 不自动解决车辆物理预测。
- 阶段 10 结构沿用到正式开发，允许小规模重构；逐环节验证，不提前创建大型通用技能／比赛框架。

## 5. 保存参数基线与已知限制

2026-09-30只读检查导出了五个DA保存值及14个项目／配置／蓝图／地图／网络资产哈希；10-B只修改其中`.uproject`和DefaultGame.ini。10-C整个施工／验证前后320资产均未变，保存值继续沿用；新增Combat版本字段只读加载仍为LegacyFixedDamage，不能以新种子显示值当作已迁移。**未重新采样PIE或证明运行时实例无覆盖**。

上段为历史保护结果；随后团队在1eb02c4有意更新玩家／被动车／普通球DA。本次保存这三份新文件哈希，接受团队已迁移／测试反馈，但未重新采样全部参数；下面表格继续表示迁移前的调参基线，不能当作新的完整字段导出。

| 内容 | 当前保存基线 |
|---|---|
| 玩家 | MaxHP `100`；质量覆盖 `800 kg`；前进加速度 `1800 cm/s²`、上限 `2400 cm/s`；正常／手刹抓地率 `12 / 0.2` |
| 发射／脱球 | 后坐最低／最高速度变化 `400 / 600 cm/s`；旧脱球冲量阈值 `100000`，重捕锁定 `1.5 s` |
| 普通球 | 固定伤害 `25`，旧击退倍率 **`1`**，半径 `50 cm`，质量 `35 kg`，线性阻尼 **`0.25`** |
| Combat | 中／重档等效速度阈值 `1000 / 2600 cm/s`；轻／中／重档水平速度变化 `250 / 600 / 900 cm/s` |

早期迁移说明里的球击退倍率 `5`、线性阻尼 `1` 等与当前快照不同，不能据此恢复旧数值或用 C++ 默认值自动覆盖当前配置。

当前必须记住的限制：

1. 伤害／冲击继续独立，10-D已实现档位与抗性；自动化不代表新碰撞标尺、墙角及真人网络手感已通过。自然接触补足是受预算限制的策略，需实测校准。
2. **仅启用TieredV1后**脱球消费实际档位，旧原始冲量路径退出。最新团队日志确认两辆玩家运行V1，免疫命中实际档位None、伤害25；只确认该子项，不等于完整物理／网络验收。
3. 等待界面不是完整冻结／比赛流程；目前无正式淘汰、观战、胜负、主机迁移或重连闭环。
4. 过去曾有自制网格误放 `/Engine/EditorMeshes` 的资源缺失问题，当前提交已修正相关引用。共享自制资产应在项目 `/Game` 路径，不能依赖仅存在于某台电脑引擎安装目录的文件。
5. 本次实测发现 Entry UIOnly 聚焦不可聚焦 Widget 的菜单错误，待整理焦点配置／目标；不影响已验证的 GAS HP 复制。主机的 `75→75` 等同值属性通知不是二次伤害；Health 订阅者仍收到通知，不能按其次数触发未来受击演出。
6. 命中球消费后转Free，现有全局拾取锁到期即可被满足条件的车辆捕获。已复核免疫目标约0.234秒后取得球；不是免疫赋予接球能力。可调球DA的Post Launch Pickup Lock Duration延后所有车辆拾取；目前无独立“被命中者禁捕时间”，若需要应另行施工，详见[10-D排查第7节](Combat/MVPB_Phase10D_Implementation_CN_v0.1.md#7-团队现象排查冲击免疫后很快捕获命中球2026-10-15)。
7. `ApplyAction_Internal`线程缺陷已修复，用户本轮确认击飞效果恢复并接受当前档位响应暂用。仍保留水平补足，不再把恢复旧完整追加作为当前待办；后续力度按配置调参另记。原第8节为历史诊断，最新结论见[10-D第10节](Combat/MVPB_Phase10D_Implementation_CN_v0.1.md#10-团队击飞回归与10-e接续2026-10-16)。

## 6. 下一环节和待确定项

**2026-11-12 Car04 当前入口为技能配置和手感验收。** 按[Editor 接线](Combat/Car04_Bull_Charge_CN_v0.1.md#editor-接线)创建 `HALBullChargeConfig` DA 与 `HALBullChargeAbility` 子蓝图，替换 Car04 Definition 的唯一 `Input.Ability.Primary`，确认 Combat／Impact 使用 V1，并在测试用 Vehicle Catalog 登记 Car04。双方使用运动历史格式 **6** 的同版构建，测试渐进过载、前方接触伤害与每目标 CD、技能出球、眩晕续行和结束收速。程序专项通过不替代真实画面、手感、复杂碰撞、校园四机或发布包验收；技能特效可另行制作。

**2026-11-11 SlipperyTrail 当前入口为油膜表现调参和保存配方验收。** 技能 DA／GA、动态地面网格及油膜材质资产均已提交，保存的技能 DA 包引用油膜实例。按[油膜特效速查](Presentation/Slippery_Trail_OilSheen_QuickGuide_CN.md)检查双层流动与变色高光，在同版 Host／Client 对照原地、直线、弯道、断段、到期清理和画面可读性，回填最新观感与问题。新车型授权仍按[技能配置与验收](Combat/Vehicle_Slippery_Trail_Ability_CN_v0.1.md#editor-接线)操作；当前已随 Car04 升至运动历史格式 6，原打滑带独立验证时点为格式 5。

地面隆起／缓慢蠕动的程序已交付，新增入口为[母材质 WPO 节点和 DA 启用步骤](Presentation/Ground_Surface_Deformation_CN_v0.1.md#材质接线)。当前自动化使用临时测试材质，保存油膜配方接线后的画面由团队验收。

2026-11-07当前入口：FX-4A～4D与FX-5程序验证通过。团队按[FX-4 Editor配置](Presentation/MVPB_Vehicle_Status_FX4_Implementation_CN_v0.1.md#2-测试表面先做两种颜色)验收实际模板，同时按[FX-5接线](Presentation/MVPB_Vehicle_Status_FX5_Implementation_CN_v0.1.md#3-editor-配置)保存共享电击配方并核对三车型效果。FX-6 进入第二视觉状态组合、退出次序和压力验收。

**FX-4接续必须保留：两个兼容表面通道是首版能力与验收目标；状态数量独立管理，显示通道允许扩展。** 验收需区分状态仍有效与表面当前是否显示，不能用表面通道容量截断GAS状态、运行实例或来源；粒子共存另按可读性与性能预算管理。

2026-11-06当前入口：FX-3 A／B／C均已完成，C最终两Target、172原生、专项双端及三回合回归通过，见[C实施／命令](Presentation/MVPB_Vehicle_Status_FX3C_Implementation_CN_v0.1.md)。下一步FX-4：先明确Overlay唯一所有者、优先级／有限组合与原材质恢复，再落实Bounds及Static／Skeletal粒子适配。新运行组件目前只准备资源；FX-5电击迁移、FX-6组合后续，旧电击继续原路径，不能让新旧同时写同一Overlay。团队当前无需改资产，画面／GPU／Cook等欠项独立验收。

2026-11-05当前优先入口：相机边界已修正并通过本机验证，按[实施第3节](Presentation/MVPB_Camera_Input_Boundaries_CN_v0.1.md#3-配置与团队复测)重开完整编译Editor，沿Entry→练习／比赛用实际保存DA反复撞墙，复测镜头、真实开关菜单、Alt-Tab、鼠标／手柄、换车／跨图／新局；需要时Client开`hal.CameraLog 1`，完成以0关闭。保持现有调参，无需重建资产。原事故画面／发布包与校园四机、独立电墙高速轨迹问题及F4／VP-4C仍单独验收；不要把本机离屏夹具通过扩大为这些项目已完成。

2026-11-05最新入口：GF-7四组规则已确认且完成代码／本机验证，重开完整编译Editor，按[实现第4节](Networking/GF7_Match_Loop_Implementation_CN_v0.1.md#4-团队配置与快速验收)在实际Room DA临时设15秒／3秒做真人淘汰、计时／并列、观战、Client退出、自动／提前返房及下一局验收，随后恢复300／8。新增UI继续原生回退＋WBP可替换，配置已有Screen Class并接只读Room View即可。实际打包／校园四机、F4／VP-4C等仍待集中验收，不把同机程序通过写成完整MVP-B验收。

2026-11-04临时血条入口：重开完整编译的Editor，确认驾驶时编号与血条；正式Playing用球／机关扣血，双端核对比例与换车／两局。原生HUD无需额外配置，练习房间不扣血；打包分发前重新生成包。见[验收步骤](Presentation/MVPB_Temporary_Overhead_HealthBar_CN_v0.1.md#2-团队快速验收)。

2026-11-04车型技能入口：重开已完整编译的Editor，按[第2节](Combat/MVPB_Vehicle_Ability_Loadout_Implementation_CN_v0.1.md#2-editor逐步配置)将Car01／Car02真实Definition设V1、各填唯一Primary／Level1及加速／免疫GA，保存／Validate后新开房间，做不同车型、同车型、取消换车及两局真人测试。无需新按键或新GA／GE；旧PlayerState列表仅Legacy使用，正式车型迁移后可清旧Primary。此步不代替下列重力／代理调参或MVP-B集中验收。

2026-11-04当前流程入口：**先按[打包前手感调整](Movement/MVPB_Prepackage_Gameplay_Polish_CN_v0.1.md)在共享DA试Gravity Scale=1.5及墙脱球门槛，通过工具生成侧面圆角新代理，在每车型Definition→Physics→Collision Mesh切换，补团队驾驶／带球／碰撞／落地反馈。** 程序验证已完成，保存DA仍Gravity Scale=1、原代理未替换；本轮不保证消除所有竖立。之后继续[GF-6实际打包／四机](Networking/GF6_Engineering_Closeout_CN_v0.1.md#6-下一步验收)，WBP接线、F4／VP-4C／FX及正式比赛规则保持独立。

2026-10-29自救最新入口：重开已完整重编Editor，按[自救第9.2节](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md#92-团队editor调参步骤)将共享DA的Minimum Recovery Tilt按需设为实际Drive Exit Tilt（当前65°），试60／12／80／6的Response组合；保留当前资格等待2秒、长按1.5秒与执行／冷却。手动保存／Validate后新开双端，左右侧／斜靠／倒扣／墙角复测，查看Block及Outcome；没有自动覆盖保存资产，也不改变下述相机和MVP-B收尾排期。

2026-10-29本次拉取补充：远端电墙与本地静态回退可选已集成并重新构建、回归及双端检查，Git无未解决冲突。重开已重编的Editor继续下述手工配置；电墙参数／shocked表现按[电墙说明](Combat/MVPB_Electric_Wall_CN_v0.1.md)核对。所有联机设备应同步同一源码／资产并完整重编，不能混用拉取前DLL。当前本地集成改动及新验证留档尚未提交，跨设备使用前须纳入正常Git提交；本次没有新增真人验收或扩大阶段范围。

2026-10-29最新补充：静态回退可选已实现并完整构建／原生回归，现可重开Editor，按[DA说明第6节](Configuration/MVPB_DataAsset_QuickGuide_CN_v1.0.md#6-静态回退可选团队配置与验证2026-10-29)选择保留回退或在骨骼配置有效时清空Visual.Mesh，手工保存并Validate；CollisionMesh／Physics独立保留。后续继续下面动态相机与原MVP-B测试安排，不将本步当作新车型／真人驾驶全部验收。

2026-10-29当前入口：主球实例编辑误锁已修复且本机重编／94项通过，先按[实施第9节](Presentation/MVPB_Dynamic_Camera_Implementation_CN_v0.1.md#9-主球勾选置灰原因与修复2026-10-29)重开Editor、勾选唯一主球并保存地图；保留已创建Camera DA／IA与IMC／Local修改，按[第2～4节](Presentation/MVPB_Dynamic_Camera_Implementation_CN_v0.1.md#2-首次配置共用-camera-da)补齐／核对绑定与Ready，再按第5～7节开Debug、热调数值和真人测试。现阶段不是继续等待方案授权，也不需要重新开发自动取景。区域／拖尾基础反馈保持；更细车型／角色／斜坡画面继续跟踪。物理悬挂／自然落地辅助保留按实测优化建议，未自动施工或正式取消。Stun、Car_02、其他Buff组合与VP-4C／F4仍按原分工及集中收尾处理。

### 6.1 MVP-B剩余功能与验收边界（2026-10-28核对）

本节保留10月28日阶段核对；后续动态相机／房间选车与GF-7正式比赛已有新增交付，当前状态以顶部及第2／3节为准。四点物理悬挂与自然落地辅助仍按party game实测需要讨论，未自动施工或写作已完成。

依据[MVP-A→MVP-B已确认目标第5.4／6节](MVP_A_to_MVP_B_Handoff_CN_v1.0.md#54-模型基础-rig物理辅助与摄像机)，不把Listen Server阶段1～10全部写完等同整个MVP-B开发结束：

| 剩余事项 | 实际缺口／范围 |
|---|---|
| 四点物理悬挂 | VP-3的四轮Trace只驱动骨骼，尚无四点弹簧／阻尼支撑力；需要具体力学、接触与网络物理重模拟方案，避免破坏已调驾驶／碰撞 |
| 有限空中／自然落地辅助 | 当前已有低回弹材质、支撑分类、姿态驾驶限制及长按自救；尚无原计划有限Pitch／Roll辅助扭矩与落地接管。不得以已有自救或视觉悬挂代替 |
| 动态摄像机 | 0.1自动取景／距离、有限观察与调参／调试代码及原生验证已完成；保存资产保持旧镜头，等待团队按文档手工接线及真人驾驶／输入／双端画面反馈 |
| 小型场景功能与分工 | 共用Stun已实现，指定机关的正式命中接入仍待团队决定。原计划固定危险区若继续要求独立地面区域扣血＋冲击形态，现无独立伤害区Actor；现有拳／炸弹已贯通机关伤害链路，但不能未经确认把无伤害的减速／打滑区记作该项交付，也不自动新建伤害区 |
| F4／VP-4C及四机 | 属于未完成的集成／验收工作，主要是诊断、组合测试、必要缺陷修复、实际网络工况及四台有线PC，沿既定后置排期，不列为已经通过 |

10月28日的最小规则只有WaitingForPlayers／Playing，正式计时／淘汰／观战／结算当时尚缺；11月5日GF-7已经授权并实现上述基础闭环，增加Finished／Results及返房，旧[Listen Server v0.2第21节](MVP_B_Listen_Server_Plan_CN_v0.2.md#21-本阶段不包含)是历史范围，不能再作为未开发结论。房间车型选择也已由GF-1～6完成。道具拾取／库存／施放、完整技能库、大量动态机关调度、多球／属性球正式扩展仍不能从现有炸弹或调试补球推定已实现，不在GF-7扩范围。

当前先完成已施工动态相机0.1的手工接线／参数初测；物理辅助必要性按后续实测再讨论，最后合并F4／VP-4C及四机收尾。未自行创建新阶段编号或将原物理辅助承诺写成已取消。

**单Host测试入口已由团队配置并提交**：此前建议复用MVPB_NetTest、将测试GameMode的Minimum Players To Start设为1，Maximum Players保留4，经Entry→Host→手动Start。当前`bf21ebc`已包含团队GameMode更新，用户反馈“没问题”并授权VP-4A，单人入口不再列为阻塞。原生Minimum支持1～4、默认2保持；直接Standalone仍不等同该Listen Server入口。双人测试须Start前Join，Playing后拒绝加入保持。本轮新双端夹具仍等两端就绪再Start，不把其当作真人菜单画面验收。

**排期调整：用户决定将剩余10-F-4完整测试统一放到MVP-B收尾，当前不再以F4-A作为下一施工入口。** 用户随后已授权车辆表现代码侧准备与接口，VP-0代码核对／VP-1底座及原生验证完成，完整矩阵与门槛仍见[F4方案](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)，后置不等于最终验收通过。

当前**VP不再安排必做的新功能开发或立即执行C-1**。按用户最新排期，[VP-4C组合／生命周期清单](Presentation/MVPB_Vehicle_Presentation_VP4C_Plan_CN_v0.1.md)统一放到MVP-B收尾，与F4合并；届时根据实际缺口补审计／测试并修缺陷。现有VP总体基础表现反馈正常，当前独立开发事项已按授权完成下述减速／打滑区域代码，不重新展开VP。

**区域基础接线与驾驶初测已完成。** 用户已确认减速／打滑独立与重叠基本符合预期，不重复要求创建两个BP。按[实施第3／4节](Combat/MVPB_Movement_Areas_Implementation_CN_v0.1.md#3-unreal-editor-配置步骤)保留后续配置参考和未细分的强弱交接、重入、普通Buff／Stun共存及客户端检查；这些具体项不从一般反馈自动推定通过。

本次合并同时保留[Stun调试指令](Configuration/Stun_Debug_Command_CN.md)与共用调用。使用完整合并版本测试；必要时将Stun与离区恢复组合检查，不把原生数据通过当作实际驾驶／画面验收。刚收到的5591039车型资源另验引用与表现；没有新增Stun Zone或电弧机关逻辑。本次85项与两轮双端结果的资产时点、协议3和已提交／未提交边界见本文件顶部及合并JSON。

用户确认的**逐参数分别取最强**及短时间恢复已施工：默认0.5减速／0.2抓地／0.4秒，仍可配置，正式平衡不冻结。区域贡献与普通Buff先求和后钳制，离区不清除其他效果／惯性；区域和运动核心、每帧实际消费值、日志均在C++。两Target、81项及原生双端为区域独立历史验证，合并后为格式3及85项；团队资产摆放和独立／重叠基础驾驶已获本次确认，具体证据见本文件顶部及[共享摘要](Combat/MVPB_Movement_Areas_Verification_20261027.json)。未增加伤害／大型机关框架、自动轮胎FX或正式阶段编号。

复杂人工待办统一留收尾：按[推送配置](Combat/MVPB_Impact_Push_Presets_CN_v0.1.md#3-unreal-editor配置步骤)记录Heavy＋Short／Long及原Standard的标定距离，补驾驶／刹车／墙角／腾空和双端互换；现有基础画面反馈保持，不要求现在重配DA。总体正向反馈不代替这些逐项测量／复杂矩阵，后置也不记为通过。

2026-10-27同步补充：上述两项代码与原验证已正常提交并收到，当前不需要重新开发VP-4B或水平预设。最新3份轮胎打滑资产只记录为素材到达，尚无本轮源码接入或画面验证，不自动另开特效施工。本次用户只要求同步进度，未开始VP-4C；若团队已完成相应人工初测，后续记录具体反馈再更新验收状态。本机5.8.2与原验证5.8.3的差异须在实际跨机测试前解决。

**VP-2／VP-3基础表现链路保持**：俯仰初测和VP-3接线／画面初测通过，不推定全部画面组合验收。VP-4只增加装饰性车身回弹和必要表现通知，复用现有组件／DA；不会追加第二次后坐力或修改规则击飞。物理悬挂／自然有限落地辅助仍分开且尚未施工。

MVP-B收尾时使用届时冻结的同一提交／构建／资产，统一覆盖驾驶／漂移／持球／发射／冲击／免疫／退出、墙角／腾空／连续接触、100ms实测RTT／1%丢包／20ms抖动及四台有线PC；150ms／3%／50ms压力档单列。合并阶段9和10-D／10-E重叠欠项，逐项保留结果，不能用基础日志代替。F4-A诊断／夹具也暂不施工，届时按A→B→C→D恢复。

后续每项开发仍保留完整编译、相关自动化、涉及联机时必要双端基本验证；真实阻塞缺陷及时处理。阶段10最终验收未完成。弹簧拳已有基础战斗／阶段复制实现及原验证，后续维护仍由另一成员负责，本轮不扩展其规则或修改资产；接口和未完成边界见[交接注意事项](Combat/MVPB_SpringPunch_Combat_Handoff_CN_v0.1.md)。完整矩阵仍并入MVP-B末尾统一测试。

`ArcadeVehicleMovementComponent`保持基础配置，纵向驾驶／上限在消费时乘当帧倍率，转向继续用基础速度参照。物理历史已加入Epoch／Revision／快照及分段时间线。F2发现UE5.8通用Async Tick在回滚时被跳过，已补仅重模拟时执行原驾驶的Chaos回调，详见[F2实施第3节](Combat/MVPB_Phase10F2_Implementation_CN_v0.1.md#3-本轮发现并修复的既有回滚缺口)；不得把该回调再用于正常帧重复施力。

10-E接线已提交，基础双端免疫反馈与日志子项保持，不再重复已完成的接线。其余精确GE／冷却、输入边沿、目标持球／到期恢复、带活动效果退出及100ms实测RTT／1%丢包／20ms抖动矩阵随集成回归补齐；下一次双方先开`hal.AbilityLog 1`，具体边界见[10-E实施第7节](Combat/MVPB_Phase10E_Implementation_CN_v0.1.md)和[手动复核](Combat/MVPB_Phase10E_ManualLogReview_20261016.json)。四台真实有线PC仍是最终环境，不以同机双端替代。

10-D目前没有发现阻塞10-E的程序缺陷，也无必需追加功能；用户确认击飞恢复，档位响应力度以后调参。尚缺的降档／免疫／脱球、连续接触、双向角色及网络扰动细项继续随集成回归补齐，不把进到10-E等同10-D完整验收；新发现的缺陷仍须修。独立命中目标禁捕继续只留档，弹簧拳接入由另一成员负责，本会话不扩展该模块。

旧10-C操作只用于未迁移副本，不要求团队重复已完成内容。10-D／10-E的5.8.2证据保持；本轮5.8.3已完成最终构建、45项普通回归与正常加速GA的真实双端请求／刷新／到期检查；未重做完整真人物理组合，四台有线PC仍待另记。跨设备提交时一起带上源码、文档及团队手工保存的资产，不提交Saved／Binaries。

10-B回归反馈与旧证据保留；它们不代替10-C新规则和V1接线后的测试。后续记录实际提交／UE补丁／DA哈希／单向注入与实测RTT，不能将同机日志时间差当RTT。弹簧拳原型及接入由另一成员独立负责，10-C／10-D提供共用结算与物理入口；本次仅交接文档，该模块与资产未修改。

| 何时确定 | 尚需具体确定的内容 |
|---|---|
| 10-C 后续证据补充 | 已有迁移提交、新资产哈希和团队完成反馈；若要宣称特定RTT／丢包／抖动门槛已通过，再补当次细项日志；本机重编后可另导出V1字段与运行状态，不覆盖旧记录 |
| 10-D 后续回归／调参 | 击飞恢复已获团队反馈；补三档／降档／免疫／脱球、连续接触、双向及网络扰动记录；档位响应与碰撞标尺数值以后调整，不作为新增开发功能 |
| 10-E 团队验收 | 无额外核心算式待定；基础双端免疫反馈和双向命中日志子项已记录，接线已提交。下次开AbilityLog，补精确计时、输入边沿、目标持球、带效果退出及扰动细项；正式表现素材暂未指定，当前用调试HUD |
| VP-4后续 | 现有功能总体基础表现获正向反馈；4C清单统一并入MVP-B末尾F4，当前不另开功能／审计／组合测试施工。复杂验收仍待完成，已发现阻塞缺陷及时修；可选新表现独立立项 |
| 电弧特效与机关接入 | 机关本体已实现，见[电墙说明](Combat/MVPB_Electric_Wall_CN_v0.1.md)；缺的是实际电流画面：`NS_ElectricArc`已存在（三层Ribbon、`User.BeamStart`／`User.BeamEnd`、Tessellation Disabled，材质仍为引擎预设），待定的是由机关哪个状态驱动与端点怎样喂两个User参数。车身受电特效已实现并接到`BP_RoundedVehiclePawn`（C++算几何＋Ribbon只画线，见第1节顶部段），待办是其余四个车辆BP指派、overlay焦黑层的肉眼验收与弧光收回确认。球碰到电流的规则与撞歪柱子是否中断电流仍未确认，直接影响判定宽度与争球路线；Ribbon强制CPU Sim的开销待电流数量增多后评估。电弧文件第9／10节的机关／CombatResolver／柱子碰撞／Build.cs四项写于机关实现之前，已过期 |
| MVP-B收尾统一验收 | 10-F-4完整A～D矩阵已由用户后置；届时合并原阶段9／10-D／10-E欠项，覆盖真实物理组合／角色互换／退出、100ms实测RTT／1%loss／20msjitter、压力档与四台有线PC。F2回滚驾驶需覆盖，不以单局日志代替完整验收 |
| 团队环境变更时 | 本轮设备5.8.2已有新两Target／71项／基本双端证据；另一设备5.8.3旧结果保留。联机前统一补丁／构建及RPC版本，真人组合／完整扰动和四台有线PC证据仍待补齐 |

## 7. 两台电脑如何接续和维护本文件

**离开设备前**：保存需要保留的 Editor 资产；结束正在运行的代理和游戏进程；更新本文件中的阶段、证据、限制与下一步；检查 Git 差异并提交要交接的源码、配置、项目资产和文档，Push 到团队仓库。不要把 `Binaries/`、`Intermediate/`、`Saved/` 或 Codex 本地数据库当作代码交接包。

**接收设备开工前**：检查本机未提交改动，确认分支后再 Pull；核对提交和实际引擎补丁版本。需要 C++ 验证时关闭旧 Editor／Live Coding 路径，使用本机引擎重新生成必要的项目文件和编译。路径可以不同，但文件内容、配置引用和版本必须可核对。并行开发应使用独立分支，避免同时改同一个 `.uasset`／`.umap`。

**维护时**：本文件保留最新状态摘要；详细算法、参数、日志和验证矩阵放在对应环节文档。更新日期、核对基点、实际交付、验证来源、未关闭事项及下一步。新的团队测试结论可以单独更新，不要求重新施工；历史基线快照保持不可覆盖，资产变化时新增快照并更新链接。规则设计改变时同时修改对应规格，避免本入口与详细文档互相矛盾。

新设备开场提示可直接使用：

> 这是同一 Unreal 项目的接续开发。请完整读取根目录 AGENTS.md、Docs/Project_Status_CN.md 和当前环节规格，再核对 Git、实际引擎版本与相关源码／配置。先汇报已完成、未验收及差异，再执行我本次指定的环节。涉及蓝图／Data Asset 编辑，请提供 Unreal Editor 操作步骤，不直接修改资产。

## 8. 本次更新记录

- 2026-11-07：FX-4A～4D代码完成，四步后统一验证，R9两Target／181原生／D3D12像素1项／两套双端各1/1通过。1193资产保持、328输入冻结，团队模板接线及粒子画面待验，下一FX-5。
- 2026-11-07：FX-5程序接通，保留原触发和接续。两Target／183原生／共享电击离屏夹具及三套双端回归通过，1193份保存资产保持。团队接线共享配方并比对三车型后进入FX-6。
- 2026-11-06：FX-3C ASC观察／本地调试／诊断完成；R4两Target、172原生、专项观察与三回合各角色1/1通过。1193资产保持、320输入冻结；单loose tag漏通知的快照批量协调及夹具失败迭代留档，下一FX-4，未提交／Push。
- 2026-11-06：FX-3B本车组件／资源／Mesh生命周期完成；R5两Target、167原生及Host／Client三回合各1/1通过。1193资产全同、318最终输入冻结；两处旧组件退出空World保护与GC夹具修正及失败日志留档。未提交／Push，下一C。
- 2026-11-06：FX-3A纯实例／来源／阶段模型完成，含定义晚到的初始时钟补偿；最终R2两Target与161原生（含新8项）通过。1193资产及已有312输入全同，新增3份C++、最终315输入冻结。无新联机／画面／Cook，本轮未提交／Push，下一B。
- 2026-11-06：按用户最新说明同步FX-3分步方案已确认，核对干净main/06eb9ab、UE5.8.3及默认目录引用，当前代码仅FX-1／2，FX-3A／B／C未施工；更新方案及第2／3／6节，无新构建／测试或资产改动。
- 2026-11-05：按授权完成相机输入、首次就绪、临时恢复与全量重置拆分；Editor／Game、144原生和8份联机角色报告通过，Client Recovery实际保持Menu=0／距离连续。1190资产全同、299输入冻结；失败迭代留档，待原场景真人／发布包／四机，未提交／Push。
- 2026-11-05：接续核对干净main/e4f0b09／UE5.8.3，相机诊断引用的8份源码内容未变（4份仅换行差异）；当前仍为Client电墙镜头缩近的诊断／待修复阶段。同步第2／3／6节，未改代码／资产、未构建或重跑测试。
- 2026-11-05：用户确认GF-7四组规则后完成比赛闭环、Room DA及原生／WBP接口；Editor／Game、138原生及双端三局／四端Exhibition／旧双端两局全部通过。1183保存资产全同、295输入冻结；详见GF7实现与验证JSON，真人／发布包／校园四机待验收，未提交／Push。
- 2026-11-05：从干净main/726b804核查正式比赛缺口并留[GF-7待确认草案](Networking/GF7_Match_Loop_Plan_CN_v0.1.md)；计时／淘汰／观战／结果自动返房未施工，四组推荐规则待用户确认。本轮仅文档／白名单，无新构建／测试。

- 2026-11-04：用户确认Car01加速／Car02免疫，完成车型V1／Legacy来源授予与校验、直接Avatar替换清理及Ready，Editor／Game成功、134/134和V1／Legacy真实双端连续两局各端1/1。1183资产全同、289构建输入冻结；保存DA迁移／真人验收待团队。详见[实现](Combat/MVPB_Vehicle_Ability_Loadout_Implementation_CN_v0.1.md)与[证据](Combat/MVPB_Vehicle_Ability_Loadout_Verification_20261104.json)，未提交／Push。

- 2026-11-03：用户授权现有UI作为回退、引用WBP后以Designer为准。从干净`main/35aa00c`完成三类页面配置、只读显示快照／变化通知、按钮请求、UMG导航／嵌套焦点及返回优先级；房间状态变化立即同步输入，原生Playing断言保留。最终两Target、129/129、D3D12内存UMG及原生UI各1/1、原生Host＋自定义Client双端两局各1/1，0测试警告／失败；1183资产全同、285源码／配置留档。实际新WBP／Designer／硬件待团队按[接线文档](Networking/GF5_Widget_Blueprint_Interfaces_CN_v0.1.md)制作；不冒充四机或打包，未保存资产或提交／Push，早期失败独立保留。

- 2026-11-03：从干净`main/817e443`接续GF-4正向初测，完成GF-5原生街机入口、四席位／静态预览、本地焦点／IMC切换、加载／错误反馈。修复取消Join后Host崩溃、隐藏页焦点、长名裁切及编号穿透，分离房满／已开始原因；旧GF-3夹具以临时配置隔离已启用的Round Travel。最终两Target、127/127、D3D12 1280×720受控Slate UI流程1/1、保存地图／DA双端两局各1/1，0测试警告／失败。1183资产全同、278源码／配置留档；真实硬件／素材／网络及四机未追加结论。按[配置及测试](Networking/GF5_Arcade_FrontEnd_Implementation_CN_v0.1.md)真人初测，下一GF-6；未保存资产或提交／Push，早期失败独立保留。

- 2026-11-03：接收GF-3基础测试正向反馈，从`main/2e7d72c`完成GF-4跨World身份／新车新局、全员客户端就绪、服务器倒计时及超时／离房恢复。最终Editor／Game、125/125、跨图连续两局／三组恢复及GF-3／GF-2兼容六组双端均各端1/1，Listen单Host与D3D12 Standalone两局通过，倒计时画面已检查。1182保存资产逐份全同、266源码／配置留档；保留团队GI及两份Room资产。按[GF-4配置](Networking/GF4_Round_Travel_Implementation_CN_v0.1.md)手配正式图和DA后真人复测，下一开发GF-5；未追加PIE／打包／真实扰动／四机结论，未提交／Push。早期失败独立保留，见[共享验证](Networking/GF4_Verification_20261103.json)。

- 2026-11-02：从干净`main/d1add4b`完成GF-3生产选车／生车／取消重选／原生房间UI及基本退出。Development Profile正确保存已核对。补首次Owner-only ASC及teardown Widget重建保护，最终Editor／Game、122/122、两组GF-3和GF-1／2兼容双端均各1/1通过，D3D12占位界面已检查；1180资产全同，260源码／配置哈希留档。团队按[详细配置](Networking/GF3_Practice_Room_Implementation_CN_v0.1.md)手动建专用Room资产及GI Host入口，再真人初测；GF-3开始仅锁房，下一开发GF-4真实跨图与倒计时。没有代理资产编辑或新增真人／四机／完整扰动结论，未提交／Push。

- 2026-11-02：团队提交GF-1后创建三份Gameflow DA，本轮从`main/653320d`完成GF-2显式换车／新局GAS生命周期、Avatar复制身份与练习伤害策略。最终Editor／Game、118/118普通、GF-2真实双端各1/1及GF-1双端兼容各1/1通过；1180保存资产全同、254份源码／配置哈希留档。Development资产保存Profile仍为Exhibition，已提示团队手动修正；三份DA原样保留且未跟踪。本轮没有UI／生产生车／真实跨图或真人／四机验收，下一入口GF-3；未提交／Push。详见[实施](Networking/GF2_Vehicle_Lifecycle_Implementation_CN_v0.1.md)与[验证](Networking/GF2_Verification_20261102.json)。

- 2026-11-01（本轮）：车身受电特效改用「C++算几何、Niagara只画线」，取代同日稍早的`Arc System`方案。新增`HALShockArcGeometry.h`／`.cpp`；`UHALShockedPresentationComponent`重写为四阶段时间轴，驱动overlay材质、Ribbon弧光与可选点光源，改由`State.Visual.Shocked`标签事件驱动。修掉分叉构建时`Points.Add(Points.Last())`的TArray自引用崩溃（撞电墙即崩溃），改法是让分叉与主干用同样的细分次数，顺带消掉零长度ribbon段。按用户要求把弧光调蓝、并改为由尾到头收回。入库资产：`M_FX_VehicleShock_Overlay`与`M_FX_ShockArc_Ribbon`（经Python桥接生成）、`NS_VehicleShockRibbon`（团队手搭）及`BP_RoundedVehiclePawn`的指派。Editor Target构建成功；**本轮未重跑自动化**，团队只肉眼确认了弧光与蓝色，overlay焦黑层与收回动作未验收。桥接脚本、`DefaultEngine.ini`的Python区块与两份FX制作文件按用户决定不入库。

- 2026-11-01：按用户要求实现车辆被电墙电到的电弧特效。本会话无Unreal MCP，故只改C++并留档，不操作Editor、不改`.uasset`。`UHALShockedPresentationComponent`新增Arc FX三项`EditDefaultsOnly`与`StartArc`／`StopArc`，沿用`State.Visual.Shocked`标签与轮胎痕的headless判断；Build.cs未改（`Niagara`已在Private依赖）。首轮构建被运行中的Editor与Live Coding中止，用户关闭后Editor Target重编成功，`HAL.FutureCreation` 108/108通过、0失败／0警告。新增车辆受电电弧特效文件（`Docs/Presentation/MVPB_VehicleShockArc_FX_Authoring_CN_v0.1.md`，按用户决定未入库）并更新第1／6节入口。没有Niagara资产、BP指派或真人画面验收；未提交／Push。

- 2026-11-01：按用户要求拉取最新版本。`main`从`9c37216`合并`origin/main`的19笔提交（动态相机、电墙／Bounce Back、球表现、输入预览、Car_02等），唯一文本冲突为本文件第2节“当前 Editor 接线”行，采用远端新内容并保留其电墙／Bounce Back行，本代理原在该行加的链接撤回。远端已实现电墙机关，故同时修正第1节特效接续段与第6节电弧行：标明机关本体、Bounce Back、Stun与shocked接口已存在，电弧文件第9／10节的机关／CombatResolver／柱子碰撞／Build.cs四项已过期，仅电流画面本体与球碰电流／撞歪柱子规则仍未定。本轮只做Git合并与本文件修正，未改源码／资产／配置，未重编或运行测试，合并进来的19笔提交在本机没有新构建／自动化证据。

- 2026-10-31：补齐电弧特效制作步骤的接续索引。该文件随`cd957ba`已提交，但第1／6节没有入口，按第1节流程换设备接手会漏读；现于第1节新增特效接续段、第2节当前Editor接线行补链接、第6节待确定项新增电弧特效与机关接入行。明确只有特效本体步骤已留档，碰撞／伤害／机关状态机与球阻挡规则未确认，没有运行时接入代码或画面验收。本轮仅修改本状态文件，无新源码／资产／配置修改、构建／Unreal运行或测试结论，未提交／Push。

- 2026-10-27：用户明确同类区域逐参数分别取最强，并询问车辆数据中间层。将此定义标为已确认，补充DA基础值→区域／GAS修饰→运行时有效倍率／桥接→权威物理帧快照／历史→Movement的职责说明；每车独立、DA不被覆盖，离区保留其他效果和惯性。既有加速度／速度通道与待新增区域／抓地／过渡明确区分；仅文档，无新源码／资产／配置、构建／测试或施工授权推定。

- 2026-10-27：用户明确减速／打滑区域三点：测试强度明显可配、离区短过渡恢复、多个同类区域取最强。新增规则记录，区分用户要求与建议：每参数最小倍率、强区到弱区／其他Buff状态的0.4秒过渡、恢复中重入、0.5减速／0.2抓地起点，以及区域选择与既有GAS增量／物理历史的边界。未将建议记成用户已确认；尚未实施区域、动态抓地或可回放过渡。本轮只文档／白名单，保留其他改动，无新代码／资产／配置、编译／运行或提交／Push。

- 2026-10-27：用户询问VP后是否进入GAS减速／打滑区域。核对现有加速度／速度Attribute、事件桥接、权威物理帧快照／历史、纵向超速修正与侧向抓地；建议区域生命周期＋减速区→动态抓地／网络历史扩展＋打滑区→基本组合验证。明确自动区域可直接施加GE、无需每个区域GA或ASC；打滑尚需原生运动与历史扩展，具体效果／离区／重叠规则待定。只更新下一步建议，无新代码／资产／配置、构建／运行或施工授权推定，VP复杂验收后置保持。

- 2026-10-27：用户明确VP复杂情况在MVP-B收尾集中测试，反馈目前表现没有太大问题。将VP现有功能标为可阶段性收尾、总体基础表现反馈正常；4C从当前方案／立即C-1入口调整为收尾验收清单，与F4合并去重，后置不记通过。明确当前没有必需追加VP新功能；打滑FX／落地回弹／振动／相机震动和物理悬挂／自然辅助继续独立。本轮只维护状态／方案／F4索引，保留已有未提交改动，不新建源码／资产／测试、不构建或运行、不提交／Push；其他开发事项未自动开工。

- 2026-10-27：用户要求先确定VP-C方案，按上下文进入VP-4C方案阶段。复核现有A／B合成、身份／时间和清理及测试，新增C-1审计／组合缺口测试→C-2必要小修／构建／基本双端→C-3团队画面留档草案；明确输出回到允许的驾驶姿态、旧动作不能重播、不改物理／HP／脱球，以及暂停游戏时间与后台长帧的区别。保留前一轮4份未提交接续记录，仅新增方案／索引及白名单；无新源码／配置／资产编辑、构建／Unreal运行或测试通过结论，尚未实施C-1。VP-4B及推送人工待办可随C-3补记录，F4／四机及新表现独立后置；未提交／Push。

- 2026-10-27：按用户要求同步VP-4B及水平推送预设进度。核对干净main/914cc53、实际UE5.8.2，确认两项均已提交；按三层验证清单核对182份源码／配置，178份字节一致、4份仅换行，无内容差异或缺项。最新提交仅增加3份轮胎打滑FX／材质，当前1157资产，没有配套运行时引用代码。新增接续JSON、修正顶部／当前提交／引擎与入口；保留原UE5.8.3的75／77项及双端证据、Unity修复与失败夹具历史，Saved报告本机不存在，不冒充新复测。本轮无新源码／资产／配置修改、构建／Unreal运行、DA导出、Fetch／Pull或提交／Push；团队画面／距离反馈待记录，4C未施工，F4后置保持。

- 2026-10-23：接收干净main/0efa410，VP-4A实现已随3d42dd8提交、团队表现DA／车辆Definition调参已提交；另有弹簧拳曲线及原测试补充，全部保留。用户反馈VP-4A暂时表现正常，记录为基础画面初测通过，当前进入4B方案讨论。核对统一CombatResolver／ImpactReceiver、结果／摘要、普通支撑过滤和现有表现合成，新增4B建议草案，明确服务器最终档位／方向、伤害独立、免疫／None／支撑过滤、装饰通知与去重／清理。UE5.8.2、887资产，两份当前DA哈希留档；本轮仅文档／Git白名单，无新源码／资产编辑、构建／自动化／运行或DA字段导出，不复用旧71项冒充弹簧拳新集成验证；4B未授权施工，未提交／Push。

- 2026-10-23：用户授权VP-4A并要求增强发射回弹；接续干净main/bf21ebc、UE5.8.2，保留团队单Host GameMode及弹簧拳更新。实现专用视觉包络、原发射序号／时间通知、拥有者即时预播／确认不重播／拒绝淡出、观察者年龄补偿、有限缓存及统一限幅；驾驶Pitch在发射峰值让位，避免后坐减速倾斜抵消演出，不改物理后坐。最终Editor／Game成功、普通71/71、真实双端各1/1且均0测试警告／失败，客户端记录25次重模拟、一次拒绝回收。首两轮70/71为新增Authority夹具Legacy范围／遗漏BeginPlay导致快照拒绝，修正后专项4/4和全量通过，生产校验未绕过。885资产全同，178份源码／配置留档，仅11份预期源码／测试变更，进程正常退出；团队手工启用DA和画面仍待反馈。新增详细配置／物理后坐增强说明及验证JSON，未提交／Push；4B／4C及F4保持后续。

- 2026-10-23单人测试入口讨论：用户希望VP-4前先解决测试地图须双端加入的不便；只读核对已存在的人数配置、主机权限和原生大厅判断。建议测试GameMode最小人数设1、同地图单Listen Server窗口手动开局，避免复制地图或改变基础逻辑；直接Standalone仍不被当前开局接口接受。本次仅记录建议，没有修改C++／蓝图／地图、编译或运行测试，未将建议写成已选方案或测试成功。

- 2026-10-23：核对干净main/093e758及实际UE5.8.2，团队ABP／表现DA已提交；用户确认VP-3配置完成、测试暂未发现表现问题，记录为基础接线与画面初测通过。补充反馈／两份当前资产哈希及VP-4三小步建议，更新当前入口；只改文档，没有新代码／资产／编译／运行或完整矩阵验收，VP-4尚未施工。

- 2026-10-22 VP-3：从干净main/faf5d06、实际UE5.8.2实现四轮视觉悬挂，沿用既有表现链路，默认关闭、无物理／网络协议改动。最终两Target／普通67/67／实际NetTest双端各1/1成功，667资产全同；四车16条查询微测19.555µs。首轮命名冲突及两项测试Roll符号期望修正后完整重验，原失败记录保留。当前团队手工悬挂节点、启用DA和正常地图画面待完成；无5.8.3／渲染／F4验收或VP-4施工，未提交／Push。详见新增VP-3实施和验证摘要。

- 2026-10-22：按用户要求Fetch并将main快进至8571fa4，合并状态文档顶部的炸弹与Editor校准记录，更新当前提交及运行前置。16份原有本地改动先做逐文件备份，再暂存／恢复；原stash保留作恢复备份，校准工具和团队手调DA保持未提交。此次没有改动玩法实现、保存资产、重编或运行新测试，历史专项证据不扩写为合并后验收。

- 2026-10-21自救Definition输入入口补全：以main/c3f90ca接收团队共享DA／IMC修改与新IA，确认BP旧Input被CanEditChange锁定，前次漏加Local.SelfRightAction及复制且文档入口错误。新增可选bool独立输入、初始化复制及配置／实际绑定断言，修正操作为Definition→Local Config→Input；不解锁Pawn或切Legacy。Editor／Game成功，重新57/57、364资产全同，未新跑双端／真人；保留前次网络证据，下一步团队填写Local DA并实测。见顶部补充记录及JSON；未提交／Push。

- 2026-10-21姿态门控／长按自救：从干净main/f634292接续，用户报告支撑修正后弹动有效减少。扩展原共享DA、服务器许可时钟、独立按住输入、有限物理翻正及动作／历史；执行边界Free脱球，执行中禁接球／发射／新技能，原效果及伤害／抗性分支保持。最终Editor／Game成功、57/57普通、真实Solver5组900步，30／60／120限帧双端各1/1及重模拟29／44／61次。363资产与Config保持，IA／IMC／Local Config引用待团队手工保存，真人画面／几何手感与F4未验收。反射／测试编译、DLL占用、短暂离地与双端夹具同步失败记录保留，最终严格断言通过；见[实现](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md)和[证据](Movement/MVPB_Vehicle_Pose_And_SelfRighting_Verification_20261021.json)。未提交／Push。

- 2026-10-21普通静止支撑分类修正：以main/c747e4b接收用户已修复车轮DA，保留363资产和Config。用普通来源／WorldStatic／非模拟、组件及物理速度、当前姿态同步判定取代Mobility=Static门槛，保留特殊来源、有效档、伤害／抗性／免疫、脱球和ExplicitTier；补来源／预算可关闭日志、实际保存两图回归及Movable Solver。UE5.8.3 Editor／Game成功、普通53/53、Solver1/1（16组／11520步）、实际NetTest Host／Client各1/1，重模拟56次／单向80ms。实际Floor接触两玩家均StationaryWorld／Suppressed1／RuleDV0。首轮地图夹具复制物理运行体断言已修复，最终重编／重测通过；失败日志保留。侧面／车尾真人手感与自然摇摆仍待复测，姿态门控／自救未实现，F4后置保持；新增[实施](Movement/MVPB_Vehicle_Support_Classification_Fix_CN_v0.1.md)与[验证](Movement/MVPB_Vehicle_Support_Classification_Verification_20261021.json)，未提交／Push。

- 2026-10-21两项实测反馈诊断：main/747eac7，保留团队启用Config、新DA／PM和原文档改动。实际0／Min与游戏快照生效，确认Test／NetTest地面均Movable／WorldStatic／非模拟，漏过上一轮Static条件；普通碰撞旧日志仍排队向上约350～535cm/s，接触Actor未记录不能逐条归属Floor。只读实际AnimBP六节点／引脚正确，C++无轮平移；四轮Radius10与缩放1模型47.5不匹配，约4.75倍转速，默认TSR／模糊列待画面对照。363资产全同，无修复／构建／新游戏或双端测试；新增诊断与原始检查留档，未提交／Push，下一步先讨论方法，见本文顶部入口。

- 2026-10-21设置页定位排查：接收main/747eac7，团队新增共享DA及Physics目录资产仍未提交并保持。本机已有成功Editor构建，最近正常Editor日志也已加载该运行时DLL并打开VehicleStabilityRules资产；尚不能据搜索无结果认定未编译。按本机UE5.8.3的UDeveloperSettings实现确认Project／Game分类与Vehicle Stability页名，设置搜索支持当前页范围；修正文档为先清空搜索、分类侧栏Game→Vehicle Stability，再定位Shared Rules。仅源码／日志核对和文档更新，无新编译、UE运行、界面查看或资产修改，实际菜单可见性待团队核对；不把可能的搜索范围问题写成已复现原因。

- 2026-10-21最小接触修正：接收干净main/699fc0c，新增VehicleStability共享DA／项目设置／World冻结快照，统一车辆材质并在Authority过滤普通静态支撑的规则追加；档位、独立伤害／抗性、脱球和显式击飞保持。配置签名复制并阻止不一致客户端模拟，补可关闭动作诊断及测试。最终UE5.8.3 Editor／Game均成功，52/52普通回归0测试警告／失败，16组真实Solver诊断1/1通过；最终Host／Client各1/1、签名4097445932、客户端重模拟55次，主机单向80ms。361资产哈希全同，无保存Config／BP／DA／地图或弹簧拳修改。首轮普通51/52因夹具立即读取未积分的冲量而失败；后续编译因UE材质枚举拼写差异失败，均修正并保留证据。提供PM→DA→项目引用详细手工步骤，正式开关仍关闭，姿态门控／自救及真人手感待做；下一代码入口扩展同一DA实现已确认自救，再VP-2。本轮未提交／Push，见[实施](Movement/MVPB_Vehicle_Stability_MinimalFix_CN_v0.1.md)及[证据](Movement/MVPB_Vehicle_Stability_MinimalFix_Verification_20261021.json)。

- 2026-10-21反弹诊断／基线完成：以main/21d29b0、UE5.8.3保留既有文档施工，发现旧DLL落后后完整重编。只读导出当前DA／Test和NetTest，361资产前后全同；确认默认材质恢复系数0.3、代理456×288×269cm和墙体高度／地面露出情况。新增16组临时Solver对照、11520轨迹／2701接触数据；区分真实离地、接地摇摆和规则追加，详见[结论](Movement/MVPB_Vehicle_Stability_Diagnostic_CN_v0.1.md)、[基线](Movement/MVPB_Vehicle_Stability_Baseline_20261021.json)与[证据](Movement/MVPB_Vehicle_Stability_Diagnostic_Verification_20261021.json)。修正旧无Rig夹具为独立Definition副本，最终Editor／Game成功，普通50/50、诊断1/1均0测试警告／失败；首次Python受保护字段、诊断编译及事件交接失败保留。无玩法参数／资产修改、真人／新双端／F4验收或自救／全局DA实现。下一入口最小物理修正规格／实现；未提交／Push。

- 2026-10-21自救规则定案／共享DA：用户明确接受触发／长按、执行打断／重试、球／技能三组首版测试规则，并要求参数放入全局通用DA。将计划第5.1节改为已确认，新增第5.2节：项目级统一引用窄范围车辆稳定／自救DA，统一时长／门槛／响应参数，运行实例持有计时和状态，初始化形成一致的纯值物理规则快照，首版保存后重新开局生效。尚无该原生类型／uasset、运行诊断或新构建／测试；本轮仅核对既有全局CombatRules加载惯例并更新计划／状态，保留已有文档改动，未改玩法／资产、提交或Push。当前无其他已知玩法待确认项，下一入口仍为反弹诊断／基线。

- 2026-10-21自救待确认项收敛：用户接受插入流程并询问是否还有确认项，将首版建议集中到计划第5.1节的三组：持续侧翻1秒、低线速／角速及长按1.5秒；有界2秒翻正、新外部有效中／重档冲击打断和执行后3秒冷却；实际翻正开始时Free脱球／执行阶段禁接球发射及新技能、原有效果自然计时。全部仍为待确认建议，未当作用户决定；反弹诊断无新增玩法确认依赖。本轮只更新计划／状态，无源码／资产修改、UE运行、构建／测试或提交。

- 2026-10-21车辆物理插入计划：用户新增侧翻／倒扣驾驶及触地／撞墙持续弹跳反馈，并选择低速、持续侧翻后长按自救。核对现有单射线接地／施力、仅作用于脱墙辅助的姿态条件、环境档位与受预算限制的法向追加、DA BodyInstance应用和网络历史／Authority动作，新增[计划](Movement/MVPB_Vehicle_Stability_And_SelfRighting_Plan_CN_v0.1.md)与Git白名单。在VP-2之前插入反弹诊断／修正、姿态操控限制及自救，四点物理悬挂／有限落地辅助仍独立，VP-1保持结束。弹跳主因与具体恢复门槛／力度尚未定案；本轮保留上轮4份文档改动，只新增／更新计划和状态，无玩法源码／资产改动、UE运行、构建或新测试，未提交／Push；F4后置不变。

- 2026-10-21 VP-1收尾同步：用户明确当前应为VP-1结束。核对干净main/21d29b0、本机UE5.8.3及a11228a接线提交，确认现有ABP_Car01／DA_Car01_Presentation资产已入库，fd41bbb包含碰撞调整；按用户确认将阶段和下一入口同步为VP-1结束／VP-2方案准备。保留10月19日构建／49项／数据双端历史证据，明确10月20日另一设备49/50的OptionalRigPreservesGameplay旧无Rig假设尚待适配；没有新增逐项双端、扰动或四机证据，不伪称重跑全绿。本轮只改状态、实施／方案和配置说明，未改C++／资产、运行UE、编译／测试、提交或Push；F4后置与其他成员工作记录保持。

- 2026-10-20转向校准排查：核对main/44cbaf8及UE5.8.3，保留团队未提交的玩家BP／Definition修改和新增AnimBP／表现DA。用户报告接线后游戏可见小车，但左右输入令前轮绕前向轴侧倾。对照现有AxisRotation、组件输出、UE ModifyBone实现和已留档参考姿态，确认DA采用骨骼局部轴：Car_01 steer局部Z对应Mesh前向、局部Y对应Mesh向下；提供FL／FR转向Y/-1、Bone Space／Add to Existing及完整Rotator接线步骤。仅更新配置指南与状态，未读取新资产具体字段／图、修改源码或资产、启动UE、重编或运行自动化；原因与修正仍待团队实测确认，不记为已解决或VP-1画面验收通过。

- 2026-10-20 Car_01检查：接收干净main/2e4176d，确认VP-1已随9983c5f提交，新增20素材使总数331→351。通过UE5.8.3进程级Python／GeometryScripting只读检查实际SKM／SK／PA、17骨骼参考变换、源LOD顶点和刚性权重、材质父级／贴图依赖，新增[可复查报告](Presentation/MVPB_Car01_AssetInspection_20261020.json)、[结论](Presentation/MVPB_Car01_AssetInspection_CN_v0.1.md)与Tools/InspectVehicleModel.py。结构／权重／依赖满足接线基础条件，转向Y/-1、轮转Y/+1和约47.5cm原始半径均留档；实际缩放、单轴预览、AnimBP／DA、画面与双端仍待团队。351资产SHA256前后全同，无生产源码／配置数值／资产修改、新构建／自动化或最终F4验收；专用UE进程正常退出，未提交／Push。
- 2026-10-19车辆表现VP-1：用户授权代码准备及接口并要求导入说明。保留已有未提交方案／截图，以main/ca90ffb、UE5.8.3实现可选车型骨骼配置、原生轮转／转向组件及AnimInstance，Pawn新增稳定默认子对象和旧视觉回退，Movement以现有锁发布实际消费输入；观察转向采用SkipOwner的一字节变化值、最多15Hz，无新驾驶RPC或历史序列化改动。新增4项普通测试及真实双进程数据检查，最终Editor／Game成功、49/49普通测试0警告／0失败、Host／Client各1/1成功，331资产哈希全同，专用进程正常退出。首次复制宏命名编译失败和47/49夹具失败独立保留并修正；具体见[实施](Presentation/MVPB_Vehicle_Presentation_VP1_Implementation_CN_v0.1.md)、[验证](Presentation/MVPB_Vehicle_Presentation_VP1_Verification_20261019.json)与[导入配置流程](Presentation/MVPB_Vehicle_Model_Import_And_AnimBP_Setup_CN_v0.1.md)。实际模型／AnimBP／DA接线及画面待团队，VP-2～4／物理辅助／弹簧拳未扩展，F4仍后置；未提交／Push。
- 2026-10-19车辆表现方案：用户要求将已讨论方向详细留档供团队同步。核对干净main/ca90ffb与实际UE5.8.3，修正当前入口仍记旧提交／未提交F4文档的状态。新增[车辆模型与基础Rig方案v0.1](Presentation/MVPB_Vehicle_Presentation_Rig_Plan_CN_v0.1.md)及用户骨骼截图原样归档，记录骨骼／轮位、C++组件／AnimInstance／薄AnimBP职责、固定骨骼映射限制、局部轴／参考姿态、参数建议、线程快照／更新顺序、不同网络角色取数、回弹去重、视觉／物理悬挂分界、VP-0～4与团队Editor／验收流程。总体方法已认可，具体素材／数值／部分接口待核对；本轮只有文档、图片与Git白名单，无生产源码／配置数值／Unreal资产修改、构建／UE运行／自动化或新团队实测，不声称Rig已实现或阶段10最终验收通过；未提交／Push，弹簧拳仍交另一成员。
- 2026-10-19弹簧拳交接：用户明确将战斗／联机接入交另一成员并询问注意事项。本次核对当前原型与CombatResolver／Action／ImpactReceiver接口，新增[交接说明](Combat/MVPB_SpringPunch_Combat_Handoff_CN_v0.1.md)，覆盖无ASC机关固定伤害来源、独立抵抗／免疫、持久每拳Action与bHitAccepted消费、默认bApplyImpact=false／LegacyMeasurement的易漏点、方向法线约定、V1配置、Sweep末段、服务器时序复制／客户端表现、待定玩法及最小验证。保留现有未提交F4方案改动，未修改源码／资产、构建／启动UE或运行新测试；未给新机关数值定案，也未提交／Push或代替成员施工。
- 2026-10-19排期调整：用户提出10-F剩余测试在MVP-B末尾统一进行，已采纳为F4-A～D整体后置，当前不施工诊断／夹具或执行完整矩阵；F1～F3和加速GA、团队基础双端结论保持。保留原F4方案与验收门槛，最终冻结同一构建后合并阶段9／10-D／10-E欠项统一执行；后续功能仍按改动做编译／相关自动化／必要双端验证，阻塞缺陷及时处理。仅调整文档排期，保留上轮未提交说明，无源码／资产／新构建或测试，不将后置写成最终验收通过，也不自动开工弹簧拳／其他新模块。
- 2026-10-19 F4方案：用户报告已提交Git并询问下一步范围／方案；确认干净`main/c825d8d`包含3项团队加速接线资产和手动复核文档，实际UE5.8.3。新增[F4执行方案](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)，细分A本机程序验证／必要诊断、B低延迟双端玩法、C主验收／压力扰动、D四台有线PC；沿用既定10分钟、1秒振荡、球状态收敛与幂等门槛。识别现有HALNetMetrics只记物理帧／ResimRuns，不能证明RTT／loss／jitter；加速与免疫共存使用测试夹具或独立配置轮次，不加第二正式槽位。仅方案／文档／Git白名单，无源码／资产修改、构建／UE运行或新F4验收；下一施工入口F4-A，本轮未提交／Push。
- 2026-10-19团队实测接续：接收`main/a38498e`，GA源码／文档已提交，团队新GA／GE及PlayerState修改仍未提交。用户报告双端符合逻辑；本次只读核对12:04～12:05主机／Client_2日志及两个启动参考日志，8次成功激活（双方各4）、4段完整效果，实际GA／GE子类和唯一Primary正常；末次刷新至GE移除5.000／5.003／5.005／5.001日志秒，倍率1.5／1.25且同值刷新不新增Revision，到期均恢复1／1。客户端1681条物理采样均Ready1／Recovery0，实际Replay且回滚请求最大5；无崩溃／GAS告警／对战异常断线，既有Entry不可聚焦Widget错误与启动提示保留。134份源码／配置与原验证一致，331资产中的团队新增2／修改1已留哈希，代理不改资产／源码、不重编／跑自动化、不提交／Push。基本接线与双端实测通过，当前下一入口F4完整集成；见[手动复核JSON](Combat/MVPB_Phase10F3Ability_ManualLogReview_20261019.json)及[实施第9节](Combat/MVPB_Phase10F3_Movement_Ability_CN_v0.1.md#9-团队双端反馈与日志复核2026-10-19)。
- 2026-10-19：用户授权补完整加速GA并复用原Primary替换免疫。新增窄ServerOnly运动GA、可配非周期GE、能力Tag，接现有输入／GAS激活；施加后结束GA，默认无费用／冷却，重复发动刷新5秒。新增3项普通测试与真实双端夹具，提供GE→GA→PlayerState→原输入的详细步骤。首次Editor构建被Live Coding阻止，用户保存关闭后完整编译成功；复核补非零Period拒绝以防基础属性残留，最终重新顺序构建Editor／Game均成功，45/45普通回归0测试警告／0失败，正常GA Host／客户端各1/1成功（主机单向注入80ms）。双方刷新保持Revision2，最后刷新至物理恢复约5.034／5.150游戏秒且变3，客户端完成时4次回滚请求。Waiting客户端尝试是本地拒绝，权威拒绝另有普通测试；不伪称预赛请求到达服务器。329资产全同，134份源码／配置哈希留档，最终专用进程全部正常退出。保留起始4份文档改动，无蓝图／地图／保存Config／DA或弹簧拳编辑，未提交／Push。当前下一步团队手工替换Primary及实际按键测试，随后F4；详见[配置](Combat/MVPB_Phase10F3_Movement_Ability_CN_v0.1.md)与[本轮证据](Combat/MVPB_Phase10F3Ability_Verification_20261019.json)。
- 2026-10-19：用户明确需要与此前免疫一样，通过技能释放发动加速，而非debug命令。复核main/e8b18ce，源码仍无运动GA，现有Primary路由／授予可复用，免疫GA的专用校验不能套用。将当前下一入口从F4调整为F3补充运动GA，再做F4；原GE／桥接／调试验证保留，不宣称完整技能链已完成。本轮仅说明缺口与更新文档，未新增C++／资产／配置、编译或运行测试；保留此前3份未提交说明改动。
- 2026-10-18：复核干净main/e8b18ce及UE5.8.3，F3实现已经提交。用户询问通过GA／GE配置测试的详细流程；核对现有免疫专用GA、加速GE、DefaultAbilities、Primary路由和HAL Debug选类，补充F3第3.1～3.2节：当前可配置GE并用调试命令测试，尚无运动GA／通用施加GE基类；不能把免疫GA的GE引用直接替换为加速。记录原生运动GA就绪后所需授予／输入／GameMode接线，以及保留免疫双技能需第二路由的边界。本轮仅说明文档，无新源码／资产／配置修改、编译或运行测试，F4仍待实测；旧验证JSON保持。
- 2026-10-17：用户提交F2后授权下一步；接收干净main/599d60a，完成10-F-3原生5游戏秒运动GE、按目标跨来源一层刷新、服务器控制的自身请求／选择性清除、可选GE子类设置、关闭施加开关及HUD／日志。原免疫Primary、HP／其他冷却和F2物理协议保持。最终UE5.8.3 Editor／Game成功，42/42普通回归0测试警告／0失败；80ms单向注入最终Host／Client各1/1成功，客户端Waiting请求被拒绝，双方同值刷新Revision2保持、自然到期3、再施加4／清除5，免疫与约18.9秒冷却保留。主机／客户端从刷新到物理恢复约5.033／5.150游戏秒，非实测RTT。首轮跨来源夹具编译错误及一次客户端错过Waiting的启动时序失败独立留档；后者改为客户端完成检查后经已有可靠改名请求标记就绪，主机再Start，未削弱断言。329资产哈希全同，119份源码／配置留档，专用进程退出；无新蓝图／保存Config／DA／弹簧拳编辑。下一入口F4真人组合／完整扰动／四机，HUD新增行未渲染检查；本轮未提交／Push。详见[F3实施](Combat/MVPB_Phase10F3_Implementation_CN_v0.1.md)与[验证摘要](Combat/MVPB_Phase10F3_Verification_20261017.json)。
- 2026-10-17：用户提交F1后指定开始10-F-2；以干净main/ca38226实施最小运动Attribute、原生Avatar桥接、可靠Authority参数动作、离散有界时间线、历史比较与Recovery、有效施力和诊断。第一轮真实客户端测试因驾驶重模拟计数0失败，定位UE5.8通用Async Tick被PT-only回滚跳过，补仅回滚执行原驾驶的Chaos回调；另清理过时恢复前缀与未知上限。最终UE5.8.3 Editor／Game成功，39/39普通自动化0测试警告／0失败；80ms／1500ms单向注入各Host／Client共4份报告全部成功，客户端实际重模拟79／52次、回滚请求3／4次，后者检测Recovery并恢复Ready／1/1。329资产前后SHA-256全同，专用进程退出，未改配置／DA／弹簧拳。F2网络用2秒原生夹具GE，F3正式5秒GE／刷新入口和F4完整真人组合／扰动／四机仍待完成；代码文档未提交／Push。见[F2实施](Combat/MVPB_Phase10F2_Implementation_CN_v0.1.md)与[验证摘要](Combat/MVPB_Phase10F2_Verification_20261017.json)。
- 2026-10-17：用户授权开始当前小步，完成10-F-1具体技术规格、只读新基线与两个UE历史／动作机制探针；选定可靠Authority绝对参数动作＋有界时间线State检查点，固定PostSolve的F+1语义、迟到／缺帧恢复及线程／身份边界。本机UE5.8.3完整Editor／Game成功、35/35自动化0警告／0失败；最终基线导出0错误／0警告，329资产整个施工／验证前后未变。首轮无效CVar警告与并行UBT争用Trace.uba失败分别留档，修正后顺序构建／最终导出通过。仅新增测试／工具／文档，未改生产移动／GAS源码、DA或弹簧拳，无新运动GE／真实效果联机或四机验收；三个专用命令行UE进程均正常退出。下一入口10-F-2；交付尚未提交／Push，见[F1验证摘要](Combat/MVPB_Phase10F1_Verification_20261017.json)。
- 2026-10-17：用户进一步明确同意10-F首例三组规则：前进／倒车同步加成且保留其他基础参数与转向参照；两类倍率分别clamp(1+增量和,0,3)；同GE按目标跨来源一层刷新、不同GE贡献独立、到期保留速度及Avatar作用域清理。首例当前无其他已知玩法待确认项；10-F-1仍须完成物理帧技术规格与新基线，尚无施工／编译／运行或新验收。本轮仅维护计划与状态文档，保留此前未提交修改。
- 2026-10-17：用户确认10-F首例5游戏秒、加速度×1.5、速度上限×1.25、一层刷新作为测试方案。更新10-F计划第4节与状态；第6.1节集中记录作用范围、不同GE合并／0～3测试保护范围、多来源与到期／Avatar清理三组待确认建议，未擅自记为用户决定。物理帧同步／迟到恢复等技术路线由后续引擎检查定稿。本轮仅文档，无源码／资产编辑、新编译／运行或10-F验收。
- 2026-10-17：接收干净main/b8feb7b，10-F四小步计划已随Git到达，确认下一入口仍为10-F-1，具体规格／新参数基线及10-F代码未完成。完整读取AGENTS／状态／10-F计划并核对10-E、运动组件、物理历史、ASC／Avatar生命周期、球发射与冲击入口；原10-E清单99项源码／配置内容一致（含换行归一），仅Git忽略白名单有已知新增，8份团队资产哈希一致、当前329资产。本机实际UE5.8.3，Editor模块DLL仍为10月14日旧文件，运行最新代码前须完整重编；不将原5.8.2测试当作本机新验证。登记速度上限与转向共用基础字段的规格关注点。本轮只更新本状态文档，无玩法／配置／资产编辑，无编译／自动化／UE运行、DA字段导出或新团队测试。
- 2026-10-13：复核 10-A 已有交付及当前代码／资产；确认原快照 14 个文件全部一致，保留 2026-09-30 数值基线；建立本入口并在 `AGENTS.md` 中加入读取与维护要求。
- 2026-10-13：完成 10-B GAS 基础、唯一 HP 来源和 Avatar 生命周期；Editor/Game 编译、自动化 16/16、319 资产只读检查及双进程初始 HP 复制成功。定位 Test 旧 GameMode 的 PlayerStateClass 尚待手改；真人回归未完成。阶段 9 文档已有未提交改动保持原样。
- 2026-10-13：核对 10-B 已提交到 `c543c41`；新增无需蓝图接线的 C++ 本地调试 HUD 与双端操作文档，状态命令补 PlayerId。新增显示代码的两个 Target 编译及既有自动化 16/16 成功；真人扣血与延迟验收状态保持待测。
- 2026-10-13：当前提交 `6150896`，团队 Test GameMode 接线已只读确认；复核真实双端日志，客户端车 100→75，主机车 100→75→50 两端一致。登记菜单焦点错误与同值 HP 通知的原因；仅标记本地双向 HP 子项通过，单机／完整物理／网络模拟仍待确认。
- 2026-10-13：按用户要求讨论10-C兼容球／技能／机关／场地机制的统一结算方案，新增待确认草案；未修改玩法源码、配置数值或资产，未将后续功能标为已实现。
- 2026-10-14：接续核对 `main / 272ba87`、UE 5.8.3 实际安装及现有 Editor Target 编译成功日志；用户补充 10-B 驾驶／持球／冲击测试已完成并授权进入 10-C。记录团队反馈与未上传证据的边界，当前转为 10-C 规则确认；独立弹簧拳的伤害／冲击接入延期至阶段 10 完成后。本轮只改文档，未新编译／测试或修改玩法与资产。
- 2026-10-14：用户接受 10-C 三项规则；补充伤害抵抗和冲击抵抗的独立分支、普通球消费及 Chaos 自然碰撞边界和验收组合。10-C 方案已定稿，源码／编译／运行验收尚未开展；未改弹簧拳或资产。
- 2026-10-14：完成10-C源码、显式Legacy/V1迁移、攻击快照、独立结算和动作／脉冲／目标去重。UE5.8.3 Editor/Game通过，最终22/22自动化0警告／0失败，320资产保护与双进程连接／初始HP复制通过；记录首轮测试夹具修正与最终证据，提供团队手工接线步骤。当前10-C代码完成，V1资产迁移及真人验收待完成；弹簧拳与物理调参未改。
- 2026-10-15：接收main/1eb02c4，工作区起始干净；73份源码与10-C清单一致（44份仅换行差异），三个DA迁移已提交，记录用户另一设备完成资产更新／测试的反馈。原文档待迁移状态已更新；本机UE5.8.2及10月13日旧DLL尚需统一版本／完整重编，本轮仅读码与留档，不启动10-D。
- 2026-10-15：讨论10-D全来源档位制度，确认冲击免疫保留Chaos自然碰撞，记录普通碰撞选档、降档脱球、重复事件／连续接触与物理追加响应的风险；只留讨论文档，未改玩法／资产或开展编译测试。
- 2026-10-15：用户同意方案并授权10-D。完成原生全来源Receiver、显式版本门槛、发射档位快照、独立GAS降档／免疫、有效档位脱球、Authority网络物理动作及诊断。UE5.8.2 Editor／Game通过，最终27/27自动化0警告／0失败；只读确认10-C迁移保持、10-D字段仍Legacy，320资产未变；双进程初始连接／HP复制通过并清理。首轮失败与修复见施工记录。当前等待团队手工迁移及真实TieredV1物理／网络验收，未开展10-E／10-F或弹簧拳接入。
- 2026-10-15：当前main/79a73f6，四份DA有团队未提交修改并保留。按用户要求更新[双端启动说明](Configuration/MVPB_DebugHUD_And_GAS_Test_CN_v0.1.md#1-在一台电脑启动两个独立游戏进程)，两端默认开启HUD及GAS／球／Combat／车辆网络／历史日志，保留独立Host／Client日志文件。仅核对源码CVar与PowerShell语法，不新编译／启动游戏或宣称团队迁移已验收。
- 2026-10-15：复核用户本次既有Host日志：两辆玩家ImpactVersion=1，免疫目标被球命中仍扣25、实际档位None；球先Free再于0.234秒后Controlled，符合拾取锁＋自动捕获管线。留档现有球DA全局拾取锁路径、作用范围与独立命中目标锁的后续建议；仅分析／文档，未改源码或资产，未新编译／运行测试，不把局部日志扩大为10-D整体验收。
- 2026-10-15：用户要求独立命中目标禁捕只留档；另报告Heavy弱于10-D前。核对最新三次Heavy均3→3且未免疫；只读DA确认四份Version1、Heavy表及车辆原调参未变，球为Heavy、拾取锁1秒，320资产采样前后不变。两端日志均Ensure于ApplyAction_Internal，经本机UE源码确认物理线程误用GameThreadAPI；另定位主动攻击水平由完整追加改为补足。记录原27项不覆盖真实动作线程、待修复与同参数复验；本轮无源码／资产修改或新编译／动态测试。
- 2026-10-15：随后用户授权修复。改用Internal同帧V／W及线性／角冲量，新增可关闭物理执行日志和真实TaskGraph Solver回归；旧实现红测复现Ensure／错误结果，修复后Editor／Game成功、28/28自动化0警告／0失败。旧Legacy夹具与已迁移全局DA配置隔离，仅测试内存修改并恢复。当前V1双进程监听7793、Join／两车HP100复制及主机自然接触动作消费通过，无线程Ensure；未测试主动Heavy球／客户端受击／真人手感及网络扰动。320资产前后未变，两个专用进程清理；DA与水平补足算法不改，禁捕设计仍延期。详见[本次验证摘要](Combat/MVPB_Phase10D_ThreadFix_Verification_20261015.json)。
- 2026-10-16：接收main/663e1b0，工作区起始干净；相关10份源码／配置和四份DA与线程修复摘要SHA一致，上一轮修复／迁移已提交，DrivingLab等团队提交保持。用户确认击飞恢复，接受当前档位、实际力度以后调整。登记可进入10-E规格确认，首例推荐短时冲击抗性但尚未获选／编码；10-D其余回归与扰动证据仍待补齐，禁捕与弹簧拳接入仍延期。本轮只读核对和更新文档，没有新编译／自动化／Unreal运行或资产修改。
- 2026-10-16：随后用户选定8秒冲击免疫、20秒冷却且确认从发动时起算，同类效果刷新、未来允许其他GE叠层、退出清理。新增10-E规格，未编码GA／GE；独立新增HALSpawnBall及服务器请求、配置引用与生成／阻挡回归。UE5.8.2最终Editor／Game通过，29/29自动化0警告／0失败；早期夹具／本地RPC吸收失败与最终证据独立留档，不以进程退出码代替报告。NetTest监听7794、主机实际补球、客户端Join及动态球接收通过；未执行客户端亲自发起或真人捕获／发射。324资产前后未变，两个专用隐藏进程核对身份后清理，见[补球验证摘要](Combat/MVPB_DebugBall_Verification_20261016.json)。
- 2026-10-16：随后用户授权GA／GE本体。以干净main/aa278bd为基点，实现ServerOnly冲击免疫GA、8秒GE／发动时起20秒冷却GE、可配置默认授予、Avatar作用域清理保冷却／Owner退出清理、可选Bool动作输入与HALUseSkill／HUD／日志。HALImpactDebug免疫贡献改为独立无限GE以兼容Mixed复制，关闭不驱散技能。最终UE5.8.2 Editor／Game构建成功，33/33普通自动化0警告／0失败；真实NetTest双进程7795和独立60FPS复验7796均Host／Client各1项通过，验证客户端先发、同帧重复请求服务器拒绝、双方Tag复制／到期与20游戏秒冷却；60FPS服务器游戏／墙钟均20秒。首次链接／CDO构造／测试夹具／复制警告及初次联机墙钟断言失败分别留档，未伪装通过。324资产哈希前后未变，四个最终专用进程均自动退出。团队输入资产接线、真实双向持球受击／到期恢复／网络扰动、5.8.3和四机未验收；10-F／弹簧拳／禁捕未扩展。源码和文档未提交，详见[10-E实施](Combat/MVPB_Phase10E_Implementation_CN_v0.1.md)与[验证摘要](Combat/MVPB_Phase10E_Verification_20261016.json)。
- 2026-10-16：随后核对main/517b497，10-E实现已提交、工作区起始干净，本机仍5.8.2；与10-E摘要100份源码／配置全部一致。按用户问题将[实施记录第5节](Combat/MVPB_Phase10E_Implementation_CN_v0.1.md#5-以后改时长换技能时怎么配置)展开为GE／GA／PlayerState／GameMode逐步配置和日志核对，区分通用接入、免疫专用技能与调试工具；明确新Buff不统一继承免疫父类、多输入槽／车型技能库未建设、运动桥接仍待10-F。本轮仅文档变更，无编译／运行／新测试或资产修改，不回写原验证JSON，也未收到新增团队验收反馈。
- 2026-10-16：用户询问五份DA用途。按当前源码修订[DA速查](Configuration/MVPB_DataAsset_QuickGuide_CN_v1.0.md)，补充10-C攻击力／球动作倍率与10-D明确档位、公共物理响应／自然碰撞标尺，标明Legacy字段不再作为V1调参入口，并说明Local／GA／GE／PlayerState职责。本轮接续已见用户正在保存的GameMode、GA／GE／PlayerState及IA资产改动，全部保留，未导出保存值或核验其接线／效果，不将其视作新的团队验收。仅文档修订，无源码／资产修改或新编译／运行测试。
- 2026-10-16：接收干净main/6850e8a，团队接线及此前文档均已提交，源码／配置100项与原10-E摘要一致。用户报告免疫Buff正常生效／消失、效果有效、双端观感正常；复核最新及紧邻备份两局四日志，5次唯一球命中包含双向受击与2次免疫Heavy→None／Suppressed／Drop0，伤害各25且HP两端一致；客户端目标第三次命中恢复Heavy。无Ensure／Assertion／Fatal或GAS Tag复制警告；Entry焦点错误继续存在，备份客户端Host closed在主机ViewportClosed退出时出现。手动启动未开AbilityLog，未独立证明8／20精确计时、重复技能或GE类名；持球／活动效果退出／输入边沿／扰动／四机仍待补。329资产为团队新增5／改3，本轮不改源码／资产，不重编或跑新测试。日志归档到Saved并新增[手动复核JSON](Combat/MVPB_Phase10E_ManualLogReview_20261016.json)，原验证JSON保留；没有自动施工10-F。
- 2026-10-16：用户同意先将下一阶段计划／方案留档供换机开发。以干净main/e811b58、UE5.8.2为基点新增[10-F运动桥接计划v0.1](Combat/MVPB_Phase10F_Movement_Effects_Plan_CN_v0.1.md)，记录四小步、首例5秒／1.5／1.25建议测试方案、服务器权威纯值快照／物理帧历史与重模拟边界、未定规则、验收矩阵、相关源码和跨设备提示。更新总体方案状态与本入口，添加新文档Git忽略例外。只有计划留档，10-F-1具体规格／新基线尚未完成，代码未施工；无新编译／运行／自动化、DA导出或资产改动。换机前须将这些文档正常提交／Push，本轮未执行提交或远端操作。
