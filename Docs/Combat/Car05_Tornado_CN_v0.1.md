# Car05 主动技能：前进龙卷风 v0.1

2026-11-14，接续 `main/9c7640f`，UE 5.8.3-58210709。用户授权先实现并验证逻辑，再制作特效。本文记录已确认规则与首测参数，完成状态以末节和项目状态为准。

**2026-11-15 用户接受当前龙卷风为暂定初版，后续优化暂缓。** 当前已随 `f169aa9` 同步；本条记录用户对当前版本的初版接受，不代表四机、Cook／发布包、真实晚加入或完整房间流程已验收，也不将下面 11-14 的工程测试冒充本次复测。本次只更新留档。

<a id="client-travel"></a>

## 四人联机中的龙卷风移动（2026-11-28）

主机每帧沿地面移动龙卷风并检测命中。旧客户端最多从上一个位置样本向前推算 60 毫秒，再逐帧追赶这个点。只要两次更新隔得更久，风柱就会先减速，再在下一次更新时加速。施放者和旁观者都能看到这个现象。

现在客户端按技能 DA 的速度和方向逐帧推进，只累计活跃阶段的移动时间。收到新的地面高度、位置和时钟校准时，逐步修正差值。服务器确认撞墙、地面中断或台阶过高时立即同步停止点，自然消散时也同步最后实际到达的位置。长时间收不到更新时，客户端最多向前预演 350 毫秒。正式 Actor 的位置更新频率仍为 25 Hz。

联机回归 `HAL.NetworkSmoke.Tornado.FourPlayerContinuousTravel` 在本机启动一主三客，加载 `MVPB_Match`、原生测试 GameMode、正式 Car05 蓝图及技能 DA。在临时直线路段实际施放，每帧采样风柱沿发射方向的位移。两个客户端使用 D3D12 离屏渲染，直接读取画面里 Niagara 组件的位置，另外两个进程采样它挂接的 Actor。对照指标为直线段内实际速度偏离技能设定 800 cm/s 超过 20% 的帧数。

正式 25 Hz 配置下，由客户端施放，主机出站模拟 80 毫秒延迟、20 毫秒抖动及 4% 丢包。两次测试各自逐帧采样，分母为该端实际采到的帧数。

| 采样位置 | 修复前偏速帧 / 采样帧 | 修复后偏速帧 / 采样帧 |
| --- | --- | --- |
| 主机 | 0 / 91 | 0 / 101 |
| 施放者客户端，实际渲染 | 79 / 100 | 0 / 99 |
| 旁观客户端，实际渲染 | 83 / 100 | 0 / 99 |
| 旁观客户端，无头 | 89 / 98 | 0 / 96 |

另将单个龙卷风的测试更新频率压至 8 Hz，沿用相同的延迟、抖动和丢包设置。三个客户端的偏速帧从 244 / 303 降至 0 / 292，修复后采到的最长位置样本间隔为 327 毫秒。恢复正式 25 Hz、关闭模拟延迟和丢包后由主机施放，三个旁观客户端合计 294 帧均在容差内。四端测试会等每个进程的采样器就绪再开局。

双端实射验证了聚风、遇墙停住、车辆 Heavy 上抛、来球解除攻击归属和消散。18 项技能、物理和渲染自动化全部通过，覆盖两次位置更新之间的移动、首次收到的就是停止位置，以及卡通风柱的聚拢、持续和消散。报告在本机 `Saved/Car05Tornado/Travel_20261128` 下的 `BeforeDefault`、`FourFinal`、`BeforeSparse`、`SparseFinal`、`HostReady`、`WallFinal` 和 `RulesFinal`。

## 已确认规则

- 每次主技能释放生成一只独立龙卷风，经历生成／活跃／消散。生成位置在物理车头前方，生成阶段聚风提示但不攻击；活跃阶段按发动时车头方向直线贴地前进，不随施放者之后转向。
- 普通墙会让龙卷风停住，保留活跃与原始生命周期，不重置时间、不提前消散。遇到无法继续支撑的地面／过大高度落差同样停住。墙另一侧的车辆与球不受影响。
- 龙卷风接触其他车辆时仅产生 Heavy 冲击，无直接伤害、无额外眩晕。水平向外推，中心重合时使用龙卷风前进方向；上抛使用现有 Heavy 档响应，抵抗、冲击免疫及脱球继续走 CombatResolver。水平距离在技能 DA 用已有 Push Preset 调整，不改共用冲击表。
- 每车辆独立 CD，持续重叠也允许到期再次受击；离开／返回不重置。免疫目标仍消耗本次目标 CD。自身不受攻击。
- Free 球被高速发射，归属 Car05，沿用正式攻击球状态和来源快照。Launched 球被弹走并原子转换为 Free，清空原发射者／PlayerId／攻击载荷和伤害资格，提供短拾取锁。Controlled 球不被直接偷走；先由持有车辆的通用冲击判断脱球，刚脱球当帧不再次发射。
- 同一球一次连续穿越最多处理一次，防止刚解除攻击的球仍在风内就被重新武装。离开并再次进入后按独立球接触 CD 处理。球速度是服务器给定的世界初速度，不叠加车速，不给车主施加发射反作用力。
- 发动要求正立接地、非眩晕、非自救执行、存活并有合法前方生成空间。持球不妨碍释放。发动后车主眩晕不影响独立龙卷风；淘汰、换车、失去 Avatar、离场或比赛流程禁用技能时清理，包括消散残留。
- 生成阶段保持原地聚风。正常消散开始时攻击资格立即结束；外观减弱、变矮、风停后再移除。后到观察者按原服务器时钟接续，不重播生成。

## 职责与接口

`UHALTornadoAbility` 管 GAS 许可／冷却及唯一活跃实例，不增加 GA Tick；`AHALTornado` 是无 ASC、无刚体的服务器移动攻击体，车辆与球继续由 Chaos 模拟。移动使用地面投影和阻挡扫掠，接触使用相对运动扫掠、有限高度和遮挡检查。球的预测路径若先碰到真实阻挡体，则不得提前解除之后才可能接触的攻击。

`ABasicBallActor::DeflectByTornado` 仅接受服务器龙卷风当前已确认接触的窄授权，不扩大 Car03／Car04 原自由球技能入口。解除攻击先提交原子 Free 快照并关闭旧 LaunchAction，再写入新的物理速度。

可复制快照包含本次规则／表现参数、Activation、起始时刻、方向、地面位置与停住状态。服务端约 25 Hz 复制，客户端只平滑表现，不上报命中；无逐帧 RPC。运动属于这个技能 Actor，不改车辆网络物理历史格式。

## 同一技能 DA 的首测值

| 分组 | 默认值 |
| --- | --- |
| Timing → Cooldown Seconds | 16 s，自成功发动起算；活跃中不可再次释放 |
| Tornado → Rules → Timing | Gather 0.6 s／Active 5 s／Dissipate 0.8 s |
| Travel | 800 cm/s，物理车头及体积外再前偏 25 cm |
| Shape | 半径 140 cm，高度 280 cm；车辆用物理根范围参与接触 |
| Ground | Probe 120 cm、Maximum Ground Step 45 cm、Minimum Ground Normal Z 0.55 |
| Contact | 每车 0.8 s，固定 Heavy＋可调水平 Push Preset=Long |
| Ball | 水平 3500 cm/s，上抛 250 cm/s，重入 CD 0.35 s，解除攻击后的全局拾取锁 0.25 s |
| Presentation → FX | 青蓝暗／中／亮、柔和过渡、透明度、转速／流速／扰动和聚风半径；同一技能 DA，不放回车辆 Presentation DA |

这些是可调首测值，不是玩法平衡结论。车辆实际上抛由共用 Heavy 响应与目标抵抗决定；球上抛有独立参数。

## 特效目标

使用 Niagara 曲面旋风与聚风提示，青蓝配色、圆润形态、三阶明暗柔和过渡。生成时提示先行、主体渐起；活跃时持续旋转，停墙不停止本体旋转；消散时主体收矮、淡出并减速，结束无硬切。外观不得决定命中，也不得遮住危险边界与球。颜色／过渡／材质／Niagara 等调整统一在技能 DA。

## 已制作的资源与状态

2026-11-14：规格、C++ 逻辑、原生资源、Editor／Game 编译及下述专项已完成。**Car05 的 GA Blueprint 创建、Definition 接线和真人观感验收仍待团队完成。** 本轮没有直接修改任何 Blueprint；开工已有 GameMode BP 改动原样保留，未提交／Push。

| Content Browser 路径 | 用途 |
| --- | --- |
| `/Game/Data/Abilities/DA_Ability_Car05_Tornado` | 已生成的 `HALTornadoConfig`，包含全部玩法／表现调参与 Niagara 引用 |
| `/Game/Assets/FX/Tornado/NS_Tornado` | 独立 Niagara 系统，主体与聚风两层 |
| `/Game/Assets/FX/Tornado/M_Tornado_Body` | 青蓝柔和三阶主体材质，Translucent／Unlit／Two Sided |
| `/Game/Assets/FX/Tornado/M_Tornado_Gather` | 地面聚风材质，同一三色及过渡规范 |
| `/Game/Assets/FX/Tornado/SM_Tornado_Funnel` | 六条螺旋曲面风带构成的漏斗主体 |
| `/Game/Assets/FX/Tornado/SM_Tornado_Gather` | 五条向内聚集的地面螺旋风带 |

本体由曲面朝向决定暗／中／亮色，保持柔和过渡，不增加黑色硬描边。首次创建时透明度为零，聚风提示先起，本体逐渐长大；活跃时持续旋转；正常到期时立即停止命中，随后收矮、淡出并减速。减速使用累计转角的连续时钟，避免直接乘剩余时间造成反转。停墙只停止位移，本体继续旋转。

## Editor 接线：由团队执行

1. 使用本轮完整编译的 Editor，双方联机前更新到相同代码及资产。Content Browser 打开已有 `/Game/Data/Abilities/DA_Ability_Car05_Tornado`，确认类型为 **HALTornadoConfig**，**Presentation → FX → System** 为 **NS_Tornado**。不需要再创建第二份 DA。
2. 在 `/Game/Blueprints/GA` 新建 **Blueprint Class → All Classes → HALTornadoAbility**，命名 **GA_Car05_Tornado**。打开 **Class Defaults → Tornado → Skill Config**，选择上述 DA，然后 Compile、Save。Event Graph 保持为空，不需另建 GE、Tick 或技能碰撞节点；持续阶段和冷却均由原生代码管理。
3. 打开已有 `/Game/Data/MVP_B/DA_Vehicle_Car_05`。确认 **Combat → Version = Attack Snapshot V1**、**Impact → Version = Tiered V1**；**Abilities → Version = Vehicle Definition V1**。在 **Grants** 中，把现有唯一 **Input Tag = Input.Ability.Primary** 条目的 **Ability** 替换为 **GA_Car05_Tornado**，Level 保持 **1**，Save。若确无该条目才新增一条，避免同键授予两个主技能。保留其他车型与 Car05 的基础物理／攻击数值。
4. 继续使用 `/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_05`，确认其 Definition 引用上一步资产。房间选车所用 `/Game/Data/Gameflow/DA_VehicleCatalog_Default` 中需有 Car05 且指向此 BP；如果已经登记，沿用原条目。无需在车 BP 增加组件，也无需修改 `/Game/Data/Car_Presentation/DA_Car_05_Presentation`。
5. 在已允许技能的开发／比赛流程中选 Car05，按原主技能键释放。技能在车头物理体积外生成，贴墙且前方容不下完整龙卷风时会拒绝发动；移到开阔地面重试。前方没有合适地面、腾空／倒扣、眩晕或自救执行时同样拒绝。
6. Host／Client 都重新启动到同版工程后测试。技能无需新 Input Action，继续沿用现有 Primary Skill 输入。正常结束后仍有短消散尾段；淘汰、换车、离场或比赛结束会直接清理，避免跨轮残留。

## 调参入口

全部位于同一份 **DA_Ability_Car05_Tornado**。运行时捕获一次释放的快照，编辑 DA 后应重新释放技能，不影响已经生成的实例。

- **Timing → Cooldown Seconds**：成功发动后开始冷却；**Tornado → Rules → Timing** 分别控制聚风提示、有效攻击、消散时长，默认 0.6／5／0.8 秒。
- **Rules → Travel**：Speed 为位移速度，Front Offset 为车头物理范围与技能半径之外的额外生成间距。碰墙后永久停留在本次接触位置，直到原定时间结束。
- **Rules → Shape**：Radius／Height 同时决定权威攻击体和主体表现大小。不能只改 Niagara 缩放来扩大命中。
- **Rules → Ground**：Ground Probe、Maximum Ground Step、Minimum Ground Normal Z 调整地面搜索、可接受高度差和坡度门槛。失去支持或超出门槛时停住。
- **Rules → Contact**：Target Cooldown Seconds 为每车辆独立间隔；Push Preset 调横向推开距离。固定 Heavy 档，上抛值取共用冲击响应表，目标抵抗／免疫仍有效；没有在技能内新增一套伤害或冲击公式。
- **Rules → Ball**：Ball Speed 为水平出球初速度，Ball Upward Speed 为球独立竖直速度；Ball Cooldown Seconds 为离开再进入的间隔，Disarm Pickup Lock Seconds 为攻击球解除后的全局拾取锁。
- **Presentation → FX → Style**：Shadow Color／Mid Color／Light Color 为三色；Tone Softness 越大，色阶过渡越柔和；Shadow Threshold／Light Threshold 控制分色位置，亮阈值至少高于暗阈值 0.02；Opacity 控整体透明度。默认三色线性 RGB 为 `(0.02,0.25,0.32)`／`(0.08,0.60,0.66)`／`(0.55,0.93,0.95)`。
- **FX → Motion**：Spin Rate 默认 900°/s，范围 0～3600°/s；Flow Speed 默认 1.2，控制风带内部流动；Turbulence 默认 3 cm，控制轻微扰动。三者分别调整，转速不改变技能命中间隔。
- **FX → Gather**：Gather Radius Scale 默认 1.6，控制生成提示从较大范围收拢；Ground Lift 默认 4 cm，减少聚风层与地面的闪烁。
- **Rules → Debug → Draw Bounds**：显示权威尺寸。控制台 `hal.TornadoLog 1` 输出开始及车辆命中档位／上抛响应，结合 `hal.AbilityLog 1`、`hal.CombatLog 1`、`hal.BallNetLog 1` 排查许可、结算与球状态。复测后关闭日志／范围显示。

## 特效后续制作约束

这是独立技能世界特效，不挂在车辆上随车移动，不把引用放回车辆 Presentation DA。后续可以在 **FX → System** 替换自己的 Niagara 配方，但需保持下列窄协议，或同步调整 C++ 表现适配层：

| 输入 | 契约 |
| --- | --- |
| `User.WindMaterial` | 主体动态材质，系统暴露参数需有有效母材质默认值 |
| `User.PressureMaterial` | 聚风动态材质，同上；名称沿用已有持续网格模板的窄接口 |
| `User.WindScale` | 主体尺寸，单位基准网格为半径 100、高度 100 cm |
| `User.PressureScale` | 聚风平面尺寸，半径基准 100 cm |
| 两个网格粒子 | Local Space；出生 Position 均为 `(0,0,0)`，不得残留车头模板的 100 cm 偏移 |
| 材质标量 | `Envelope`、`Shape`、`MotionTime`、`SpinRate`、`FlowSpeed`、`Turbulence`、`Opacity`、`ToneSoftness`、`ShadowThreshold`、`LightThreshold`、`GroundLift` |
| 材质向量 | `ShadowColor`、`MidColor`、`LightColor`、`GroundNormal` |

两个持续网格发射器各常驻一粒子，当前粒子寿命 60 秒，高于配置允许的总生命周期上限 30 秒。由 Actor 的阶段控制显示并最终销毁，不能靠发射器随机寿命截断技能。位置、有效攻击时间、目标 CD 和球的 Free／Launched 转换均由 C++ 决定，Niagara 不上报伤害。客户端晚看到实例时用原服务器时刻接续表现，不从零重播聚风。

原生制作工具为 `HALBuildTornadoFXCommandlet`；正常开发不必重跑。默认生成只允许新建，拒绝覆盖已有资产；`-AlignOrigin` 仅修正本技能 NS 的局部出生位置。不要对已由特效成员调过的资源直接运行制作工具。Car03、Car04、TireSkid 的现有资源保持独立。

## 本轮验证与证据

| 范围 | 结果与边界 |
| --- | --- |
| 逻辑先行 | 特效制作前完成 Editor BuildLogic_R2，原生 Car05＋Car03／04 回归 Logic_R1 **13/13**，零测试警告／失败 |
| 最终编译 | UE5.8.3 **Editor BuildFX_R5／Game BuildGame_R3** 成功 |
| 原生＋真实渲染＋回归 | **Final_R2 17/17**，零测试警告／失败；使用 D3D12，包含 Car03／04 既有专项 |
| 实际渲染 | 聚风 14731、主体 45288 个可见像素；运动改变 71614 像素；三色分别改变 20980／18958／27545 像素；结束与空场差异 0；两层真实 Niagara 粒子局部原点均为零 |
| 双进程联机 | **Network_R3 Host／Client 各 1/1**，零测试警告／失败，进程退出 0；Host 出站 80 ms；两端均观察到本地／远端生命周期、停墙、Niagara 绑定、攻击球变 Free；服务器另确认真实目标 Chaos 竖直速度 >50 cm/s，共用 Heavy 请求的最低上抛为 750 cm/s |
| 原有资源保护 | 开工 **1303/1303** 份既有 Content／Config／uproject 哈希保持，新增六份独立技能资源；开工已有 GameMode BP 改动未覆盖 |

原始报告与 BMP 预览位于本机 `Saved/Car05Tornado/20261114`，不纳入 Git；[可同步验证摘要](Car05_Tornado_Verification_20261114.json)保留计数、源文件／资产哈希、命令及中间失败记录。最终全量渲染后只修改了网络测试球的显式材质设置，重新编译两 Target 并单独复跑双端，不重复未受影响的 17 项。

中间失败未计作通过：首轮宏参数名不符合 DOREPLIFETIME、工具调用未导出的 NiagaraGraph 方法导致链接失败，均已修正并重编；Niagara 模板继承的局部出生偏移已修正，并增加真实粒子原点断言。Network_R1 客户端以名称识别测试球失败，改为复制 Owner 身份；R1／R2 Host 的测试球空材质槽警告用夹具显式材质修复，未屏蔽警告，R3 双端干净通过。

## 待团队实测

以下不包含在本轮通过结论中：手动 GA／Definition 接线、真人画风与打击手感、四台机器、Cook／发布包、完整房间 BP 流程及真实晚加入。原生接触夹具不推进完整车辆物理，通过强制到期验证目标 CD 的再次受击；实际联机补充了真实上抛与防御球，但不代替连续受击平衡测试。

1. 开阔地正常释放／重复按键／冷却；聚风和消散期间不攻击；眩晕、空中、倒扣、自救和紧贴墙无法发动。发动后眩晕不取消，淘汰／换车／离场／比赛结束无残留。
2. 正面、侧面和中心重合接触，多目标、每车 CD 内／后反复接触；确认向外推＋上抛、无直接伤害、抵抗／免疫独立。持续重叠可能被多次打飞，检查是否符合 party game 的可读性。
3. 普通墙／墙角停住后持续攻击，墙后目标不命中；坡面、小台阶、断崖和不支持地面；检查停住时不重置生命周期、聚风层贴地和尺寸边界。
4. Free 球发射归属、Controlled 球不直接偷取、Launched 球解除、解除后碰车不伤害、离开再进入与拾取锁；快速交错和球先碰墙／车的顺序。解除后的 Free 球仍有物理推力，Free 不表示速度归零。
5. Host／Client／观察者和真实晚加入，开网络扰动测试移动／停墙／结束；检查无重复命中、无重播聚风及跨轮残留。最后由团队四机与发布包验收。

## 第二轮：多层独立气流重做（2026-11-14，当前）

用户明确否定首版“旋转的螺旋弹簧”外观；上文首版资源／17 项证据作为历史保留。本轮表现代码、原生资源、编译及专项完成，等待用户实际画风验收，不更改技能玩法。

新结构为低透明度漏斗底层＋22 条不同高度／方位的短气流片＋5 条外围游离风带。短风片独立环绕并向上流动，每片角速度按固定种子错开，不用整根连续螺旋整体旋转。流动循环在顶／底边界淡出接续；外围风与主体同向但有独立速度倍率。内部明暗沿用柔和三阶，青蓝／原配色与灰白以局部不同相位渐变。聚风提示与正常消散时钟沿用。

新增六份资源，保留首版资源作为回退；原技能 DA 仅自动切换 **FX → System**，原有玩法／冷却／配色／转速等值保留。原 GA 和 Car05 Definition 不需要重新接线。完整重开本轮 Editor 后，用原 Car05 主技能测试。

| `/Game/Assets/FX/Tornado/` 下的新资源 | 用途 |
| --- | --- |
| `NS_Tornado_LayeredFlow` | 当前技能 DA 已引用的新系统；三个常驻网格发射器 |
| `M_Tornado_LayeredFlow`／`SM_Tornado_LayeredFlow` | 低透明度漏斗底层和独立短气流片 |
| `M_Tornado_OuterGusts`／`SM_Tornado_OuterGusts` | 五条外围游离风 |
| `M_Tornado_GatherFlow` | 独立聚风材质副本，修正局部坐标；沿用原聚风 Mesh 与时序 |

新增调参都在 **DA_Ability_Car05_Tornado → Presentation → FX**：

| 分组／参数 | 默认值 | 调节作用 |
| --- | --- | --- |
| Layered Flow → Differential Rotation | 0.55，范围 0～0.8 | 内部各短风片的角速度差；0 为相同角速度，仍可上升流动 |
| Layered Flow → Rise Speed | 0.22 高度／秒，范围 0～1 | 内部向上循环速度；0 关闭上升，不关闭环绕 |
| Layered Flow → Core Opacity | 0.24，范围 0～1 | 连续漏斗底层透明度，过大会掩盖内部层次 |
| Outer Flow → Outer Opacity | 0.32，范围 0～1 | 外围少量游离风的透明度，0 关闭外围层 |
| Outer Flow → Outer Radius Scale | 1.15，范围 1～1.5 | 外围风相对漏斗的视觉半径；不扩大逻辑攻击范围 |
| Outer Flow → Outer Speed Scale | 0.65，范围 0.1～2 | 外围相对主体的环绕速度倍率；方向保持一致 |
| Color Flow → Gray Color | 线性 RGB (0.70,0.78,0.80) | 渐变到的灰白颜色 |
| Color Flow → Gray Mix | 0.75，范围 0～1 | 青蓝／原色与灰白渐变的最大混合量；0 保留原三色 |
| Color Flow → Color Cycle Seconds | 1.6 秒，范围 0.2～10 | 一轮局部颜色来回渐变的周期，越大变化越慢 |

**Spin Rate** 仍控制整体环绕基准速度，**Flow Speed／Turbulence** 控风体弯曲扰动。本轮不把这些值移到车辆 Presentation DA。

新增系统保持原 `User.WindMaterial`／`User.PressureMaterial`，额外使用 **`User.OuterMaterial`**，外风尺寸共用 `User.WindScale`。三个持续网格发射器各一粒子，局部出生位置为零；主体／外风材质还读取上述新增同名参数。新网格 **UV1 保存每片流的固定种子／类型，不可删除或随意重算第二 UV**。WPO 读取 **Pre-Skinned Local Position**，不再依赖顶点色还原坐标；静态网格构建会把顶点色打包为 sRGB，直接当坐标会导致风片偏离风眼。不可用粒子随机寿命或独立 Niagara Age 重启已在服务器计时的生命周期。

新增发射器必须经 Niagara 编辑器的系统级 API 同步 Spawn／Update 图；只复制 Emitter Handle 可能得到“已有 Renderer／MID，但实际不出粒子”的空层。首轮已实际捕获此问题，最终双端和 RHI 均检查三个发射器各有一粒子。本轮创建工具 `-Layered` 使用官方 AddEmitterToSystem，维护入口 `-RepairLayered` 只处理新配方资源，不保存技能 DA、BP 或首版资源；正常调试不需要重跑制作工具。

### 本轮验证与限制

- UE5.8.3 **Editor BuildEditor_R3／Game BuildGame_R2** 成功。最终 **Final_R2 17/17**，零测试警告／失败，包含 Car05 原规则及 Car03／04 回归和真实 D3D12 渲染。
- 三层粒子实际局部原点均为零。聚风／主体可见像素 **20829／52035**，0.15 秒运动改变 **53142** 像素；三色分别改变 **12929／15279／25432** 像素；结束与空场差异 **0**。
- 关闭外围风改变 **2314** 像素，关闭灰白混合改变 **13265** 像素；固定上升／扰动／颜色后，独立角速度对比共同角速度改变 **42966** 像素；关闭 Spin Rate 和颜色循环后，内部上升／流动仍改变 **34426** 像素，验证主体不是只靠整体旋转运动。
- 最终 **Network_R2 Host／Client 各 1/1**，零测试警告／失败，进程退出 0，Host 出站 80 ms。检查三层实际出粒子、新配色混合／速度差非默认值复制、生命周期／停墙／清理，并回归真实车辆上抛与攻击球转 Free。
- 开工 **1310** 份既有 Content／Config／uproject 中，仅授权技能 DA 变化：原生逐属性断言确认只改 `FX.System`，其余 **1309** 份哈希一致。用户 Car05 GA、Definition 和 GameMode BP 保持。新增六份独立资源，未修改旧 Car03／04、TireSkid 或首版龙卷风资产。
- 首轮 **Final_R1 16/17**，失败为外围发射器无实际粒子；目测还发现顶点色坐标导致风片偏移，均修正后复测，未放宽断言。早期两次制作命令因模块名／PowerShell 点号参数拆分未启动；正确入口为带引号的 `'-run=HAL_Future_CreationEditor.HALBuildTornadoFX'`。

本机原始报告／BMP 在 `Saved/Car05Tornado/Layered_20261114`，[可同步摘要](../Presentation/Car05_Tornado_Layered_FX_Verification_20261114.json)保留最终源码／资源哈希与证据。此处是专项工程验证，不替代用户的实际场地／相机／画风验收；四机、Cook／发布包、真实晚加入和完整房间 BP 流程仍待团队测试。本轮未提交／Push。

## 第三轮：主体逐渐成型与边缘提亮（2026-11-14，当前）

用户接受上一版明显改善，并在技能 DA 调整了转速、流速、扰动和透明度。本轮保留其已保存技能 DA 的完整文件哈希，未改写任何既有调参；仅更新 `M_Tornado_LayeredFlow`／`M_Tornado_OuterGusts`，原 Niagara、Mesh、GA、Definition 不需重新配置。新增字段使用原生默认值，可在原技能 DA 继续调整。

主体生成由原来整体缩放／淡入，改为**从下往上逐层显形，同时逐渐扩展体积**，与地面聚风提示分别控制。生成总时长仍为 **Tornado → Rules → Timing → Gather Seconds**；增加这个值也会推迟技能开始攻击的时间，不新增一套独立于逻辑时钟的生成延迟。活跃与消散的原时间／命中规则保持，晚看到的客户端按原服务器阶段接续。

边缘效果是沿用当前配色的柔和视角边缘提亮，主体与外围短风片均有效；连续漏斗底层的提亮更弱，避免整层洗白。保持 **Translucent／Unlit**，通过 Emissive 输出，不产生真实点光源。关闭 Bloom 仍有边缘亮度变化；环境启用 Bloom 时可产生额外泛光，最终效果与项目后处理设置有关。

在 **DA_Ability_Car05_Tornado → Presentation → FX** 调节：

| 分组／参数 | 默认值与范围 | 作用 |
| --- | --- | --- |
| Formation → Formation Exponent | 0.85；0.4～3 | 成型曲线。越小越早显出风柱，越大越偏后段成型；总时长仍由 Gather Seconds 决定 |
| Formation → Formation Feather | 0.16；0.03～0.4 | 上升显形前沿的柔和宽度，占风柱高度的比例；越大越柔和 |
| Glow → Glow Strength | 1.1；0～4 | 边缘提亮强度；0 完全关闭。保留原配色与透明度 |
| Glow → Glow Power | 2.5；1～8 | 提亮的集中程度；越大越窄，越小越宽 |

### 主体尺寸与逻辑范围

**只在技能 DA 的 Rules 配置尺寸，不需要再配置一份 Niagara 主体缩放。** 原生初始化会写入：

```text
User.WindScale = (Rules.Radius / 100, Rules.Radius / 100, Rules.Height / 100)
User.PressureScale = (Rules.Radius / 100, Rules.Radius / 100, Rules.Radius / 100)
```

- **Rules → Shape → Radius／Height** 同时控制判定圆柱和主体表现的基准半径／高度；重新释放后读取新值。
- **Outer Radius Scale** 只调整外围装饰风相对主体的外扩，**Gather Radius Scale** 只调整生成提示的收拢范围，不扩大命中体积。
- 逻辑体是圆柱，主体外形是漏斗，轮廓不逐点重合；看准确规则边界时打开 **Rules → Debug → Draw Bounds**。目标车辆按物理体接触判定，不要求其中心已经进入圆柱。
- 不需要在 Niagara 中额外设置 `User.WindScale` 来匹配技能范围，运行时会由 Rules 覆盖。当前没有另设必须配套调整的“全局视觉尺寸倍率”。
- 如果“倍率”指冲击力度，固定 Heavy 档与横向 **Push Preset** 是独立于尺寸的结算配置，扩大半径不会自动增强冲击。本技能无直接伤害倍率。

### 本轮验证

UE5.8.3 **Editor R1／Game R1** 编译通过，**Final_R1 17/17**，零测试警告／失败。隐藏全部地面提示后，生成进度 15%／50%／90% 的主体可见像素为 **3945／24241／47898**，高度为 **70／187／289** 像素；关闭边缘提亮对照中，有 **13333** 个风体像素明显变亮，证明并非只改变地面提示或参数名称。原三层流动／配色／消散及 Car03／04 回归保持通过，结束与空场差异为零。

Host 出站 80 ms 的实际渲染双端 **Network_R1 各1/1**，零测试警告／失败、退出0，检查三层实际出粒子、非默认成型／发光字段复制与材质绑定，并回归停墙、生命周期、真实上抛及攻击球解除。开工 **1316** 份既有 Content／Config／uproject 仅上述两份风材质变化，其余 **1314** 份哈希一致；技能 DA、用户 Car05 GA／Definition／GameMode BP、Niagara、Mesh 及其他车型保持。本轮用户保存快照为 Spin Rate=1800°/s、Flow Speed=1.8、Turbulence=5 cm、Core Opacity=0.6、Outer Opacity=0.8，均未重置；这些是用户此时的保存值，不替代默认参数表。

原始报告／BMP 位于本机 `Saved/Car05Tornado/FormationGlow_20261114`，[可同步验证摘要](../Presentation/Car05_Tornado_FormationGlow_Verification_20261114.json)记录当前哈希及测试边界。本轮工程检查通过，实际场景／相机下的可见度与画风仍待用户实测；四机、Cook／发布包、真实晚加入、完整房间 BP 流程不在本轮验证范围。未提交／Push。
