# 黑洞机关 Actor

## 规则与接入

原生类 `AHALBlackHole`，Blueprintable，可直接建立 BP 子类，或由服务器生成。生成位置为圆柱中心，世界 XY 是平面，旋转和缩放不改变玩法范围。无阻挡碰撞；只吸引有效的 `ATestVehiclePawn`，包括生成者，不吸球、不扣血、不加眩晕、不强制脱球。物理场不经过攻击档位／冲击抗性结算，保留原有 Z 速度及角速度。房间不允许世界互动或车辆已退役时不施力。

用户确认的流程：生成即弱吸引，扩张至最大范围后维持，在维持期间重复准备→瞬间强拉，强拉不缩小范围。到期停止吸引并销毁。总玩法寿命＝Expansion Seconds＋Duration Seconds。第一轮强拉在扩张完成后等待一个 Pulse Interval；这是实现默认时序。间隔长于维持时间时可完全没有强拉，到期时刻优先结束，不再强拉。

| BP Settings 参数 | 原生默认 | 单位／语义 |
|---|---:|---|
| Continuous Acceleration | 120 | cm/s²，持续向心加速度，不受车重影响 |
| Pulse Delta Speed | 700 | cm/s，瞬间追加向心速度，不设持续时间 |
| Pulse Interval | 2 | 游戏秒，两次强拉间隔 |
| Expansion Seconds | 2 | 游戏秒，从 1 cm 种子半径线性扩至最大，0 表示立即全范围 |
| Duration Seconds | 8 | 游戏秒，达到最大范围后起算 |
| Maximum Radius | 700 | cm，最大半径 |

高级几何参数 Half Height 默认 200 cm，为 Actor 中心上下高度容差。范围按车辆中心判定；中心没有向心方向，不施力。紫色在中心柔化区外吸力均匀、柔化区内逐渐减弱，黄色保持原瞬间增量。多个黑洞的向心速度贡献自然相加。

服务器每 0.02 游戏秒定时采样，用真实间隔积分弱吸引，再经既有 `QueueImpact/FVehicleImpactAction` 将已解析的平面速度增量送入权威网络物理动作；单机使用同入口的刚体冲量分支。网络重演不重新读取 BP、GAS 或当前位置来重算旧力。不新增 Movement 历史格式、GAS 效果、Actor Tick 或客户端命中 RPC。

这是按时间采样的弱吸引，不是连续子步力场。单次积分最多 0.1 游戏秒；服务器卡顿跨过多次强拉时只补最新一次，不集中爆发全部漏掉的强拉。每车每黑洞最多约 50 动作/游戏秒，多个黑洞的带宽／规模性能尚需实测。高速穿越小范围不保证连续扫掠命中。

共用机关生成器尚未建立，本次不建设生成器。C++ 后续通过 `SpawnActorDeferred<AHALBlackHole>`，`ConfigureBlackHole`，`FinishSpawning` 接入；配置只接受服务器且必须在 BeginPlay 前。BP 默认值不会被原生默认配置覆盖。不要从客户端直接 SpawnActor 来代替权威生成。

## Unreal Editor：建立自己的 BP

1. 编译完成后打开 Editor。在 Content Browser 新建 Blueprint Class，展开 **All Classes**，搜索 **HALBlackHole**，建立 `BP_BlackHole`。
2. 打开 BP → **Class Defaults → Black Hole → Settings**，调整上表六个主参数。Shape 的高级参数可调整 Half Height。
3. 在 **Black Hole → Debug → Debug Draw** 勾选（BP默认值或地图Actor实例均可）。Editor／BP视口中，三圈显示最大圆形范围及上下 Half Height 边界，旋转／缩放Actor不改变这些世界单位范围。游戏内显示当前生效半径的半透明圆盘、同心圈与8个向心箭头：扩张红色、准备强拉紫色、每次强拉后的0.15游戏秒黄色。只在中心上下Half Height内查询静态地面并抬高3 cm；没有地面则回退中心平面，不改变玩法范围。显示为水平平面，不逐点贴合斜坡／崎岖地形。默认关闭，Shipping无运行时debug绘制，Editor预览组件不进入非Editor构建。
4. Compile／Save，将 BP 拖入测试地图。开始游戏后立即计时并生效，到期自行移除；多人只由服务器生成及结算。
5. 不需要 Event Graph 就有完整吸引逻辑。正式特效可在 BP 增加非碰撞 Niagara／Mesh，使用下述表现事件。代码代理未创建／编辑 BP 或地图。

## 表现接口

- `On Stage Changed(New Stage)`：Expanding、PreparingPulse、Ended。强拉是瞬间事件，不是持续阶段。Ended 在自然到期及提前销毁时用于清理；不要从事件中施加玩法冲量。
- `On Field Updated(Radius Cm, Preparation Progress)`：约 50 Hz，且每游戏帧最多一次，更新圆盘／粒子半径和准备强拉强度。Preparation Progress 是上次强拉（或扩张完成）至下次强拉的 0～1 进度。
- `On Attraction Pulse(New Pulse Index)`：播放瞬时吸入、闪光或声音。客户端依据服务器复制的序号显示最新事件，网络合并可能跳号，不补播所有历史特效；晚加入不应重播旧强拉。
- 纯读 `Get Current Radius`、`Get Stage`、`Get Preparation Progress` 可用于表现查询。客户端用 GameState 服务器时钟，不修改权威规则。

延续当前柔和圆润的卡通涂鸦画风；本次只有可开关 debug 圈与 BP 表现接入口，没有制作最终黑洞材质／Niagara。结束立即移除，不新增消散时长。

## 团队手测

### 入场一次性降速（2026-11-12）

本轮UE5.8.2 Editor R3／Game R2 Development成功；普通黑洞＋Impact回归 **4/4**，0测试警告／失败／未运行／进行中。真实Chaos验证入场平面(1000,300)变(400,120)、Z=50保留，留区不重触发、短间隔重入不煞车、重新武装后1000先减至400再完整追加黄色-700得到-300。双端组合回归R1及移动入场专项R2均Host／Client各 **1/1**、0测试警告／失败／未运行／进行中；R2让双方车辆以500cm/s向场内移动，在扩张阶段明确断言各自拥有车辆的入场速度下降，随后验证黄色拉扯与到期。Host出站80ms注入，R2客户端实际重演57次。没有真人手感或新的画面验收。三份团队BP／地图哈希前后一致，未改保存资产／Config，未提交／Push。报告／日志在`Saved/BlackHole/20261112/EntrySlowdown`。

新增Settings→Attraction→Entry：`Entry Speed Multiplier`默认0.4，表示入场保留40%当前平面速度（削减60%），1关闭、0完全消除当下平面速度；`Entry Retrigger Delay`默认1游戏秒，同一黑洞再次降速的最短间隔。

服务器沿现有0.02秒范围采样跟踪每车进出。车辆中心首次进入当前有效圆柱范围时触发，之后一直在区内不会重复；离开再进入且满足间隔才重触发，边缘抖动不逐帧煞停。扩张覆盖车辆、在车辆位置生成也属于进入，圆心没有向心方向仍能触发入场降速。只影响水平XY速度，保留Z、旋转及已有伤害／脱球／控制规则；不增加持续低速Buff。若同一采样遇上黄色，先按入场倍率解析原有速度，再追加完整黄色冲量，不削弱黄色本身。

仍由服务器提交已解析速度增量到原权威物理动作，客户端回放该增量，不自行判定进出或重复触发。弱吸、中心柔化、低速逃脱辅助继续作用于入场降速后的速度。占用与冷却记录为弱引用，离开／销毁后清理。这里的“进入”仍按车辆中心采样，不是车壳接触即触发；极短的高速擦边在两次采样间完全穿过时没有连续扫掠保证。

团队手测以高速横穿／斜切边缘开始，感受一次速度损失后需要重新踩油门；停车在区内应无重复减速，离开后立即重入不应连续被煞停，等待间隔后重入再次生效。可先用0.4，再按手感试0.2；要恢复原行为设1。代理不修改保存BP／地图参数。

### 黄色后起步的低速逃脱辅助（2026-11-12）

验证：UE5.8.2 Editor R3／Game R2 Development成功；普通黑洞＋既有Impact物理回归 **4/4**，0测试警告／失败／未运行／进行中。覆盖朝外／朝内／侧向／松手／模拟油门、负向外速度、0／75／150cm/s平滑恢复、关闭辅助和非法配置；原真实Chaos黄色超门槛完整追加冲量断言保持通过。双端最终`Network_R3` Host／Client各 **1/1**、0测试警告／失败／未运行／进行中，Host出站80ms注入，Client重演51次，组合拉扯与到期回归通过。该双端夹具不是玩家实际朝外按键起步手感验收，仍须团队试玩。

首轮Network_R1运行器超时、R2双端各失败1项，状态均已观察到扩张／维持与约865cm/s峰值吸引，但误将团队地图预放的长寿命BP也纳入“测试机关应消失”判定。修正为仅跟踪夹具生成的原生类与高空测试位置，重复夹具检查仍保留，不删除团队机关；R3通过。生产逻辑自R2编译后保持，仅测试识别修正。报告／编译／失败记录完整保留在`Saved/BlackHole/20261112/EscapeAssistance`。BP与两个地图前后哈希一致，无资产／Config修改；既有相机C4305编译警告保持，未提交／Push。

针对用户报告黄色拉掉速度后无法重新对抗紫色，新增Settings→Attraction→Escape Assistance：`Escape Recovery Speed`默认150cm/s，`Low Speed Attraction Scale`默认0.3。它们是弱吸恢复门槛与保留吸力比例，不是保证／直接设置最低车速。比例1可关闭辅助；0可在完全符合条件且低速时完全免受弱吸，黄色不受影响。

服务器从Movement的线程安全`GetAcceptedDrivingInput`读取最后实际物理步消费、并通过姿态／运动／控制权限门槛的输入。新增独立的玩法快照，不挪用PresentationInput，不读取客户端Pawn本地输入，不改变输入命令或网络历史格式。未启用物理、历史输入被封锁时返回中性输入，物理早退也清空快照。黑洞只在油门有效、未按刹车且物理车头有向外分量时减弱紫色。对齐程度与模拟油门连续缩放辅助，不要求精确朝外的突变角度门槛。

向外速度≤0时辅助最大，达到Recovery Speed时辅助消失，中间用SmoothStep平滑恢复：例如正朝外满油门时，0cm/s保留30%紫色，75cm/s保留65%，150cm/s恢复100%。中心柔化与向心速度上限继续生效。黄色仍独立完整追加冲量，不直接恢复黄色损失的速度；即使正在被拉向内，只要持续朝外踩油门即可获得弱吸减免。松手、朝内、侧向或刹车无该减免。

实际能否逃出仍取决于车型推进力、抓地及原Continuous Acceleration，辅助不会穿墙或克服任意强度的吸力。团队先隔离黄色测试放手／朝内／朝外，再恢复黄色验证“被拉回→重新起步→恢复向外速度”。保存BP／地图由团队手工调整，代理不修改。

### 紫色持续拉扯调整（2026-11-12）

本轮UE5.8.2 Editor／Game Development均成功；普通黑洞＋Impact物理回归 **4/4**，双端新规则Host／Client各 **1/1**，均0测试警告／失败／未运行／进行中。普通测试覆盖向心门槛、不削黄色超限速度、横向／Z保留、中心柔化、无输入积分与给定外向加速度可克服吸力；真实Chaos读数确认向心900时黄色仍完整追加700到1600。无输入／外向驾驶的累积测试为力学模型，不是完整车型真人手感通过。双端Host出站80ms人为注入，Client重演43次，验证组合拉扯／复制与到期，非新视觉验收。日志／报告在`Saved/BlackHole/20261112/ContinuousPull`，BP与两个地图施工前后哈希一致。没有编辑保存资产或Config，没有更改原Continuous Acceleration和黄色配方；旧相机C4305警告保留。未提交／Push，下一步团队按车型调持续吸引强度并验手感。

黄色仍按原Pulse Delta Speed瞬间追加向心速度，不受紫色上限削弱。扩张及紫色准备期间的弱吸改为有限向心加速度：原Continuous Acceleration保留，新增加两个BP参数，位于Settings→Attraction→Continuous Pull：

- `Continuous Speed Limit`：默认200 cm/s；当当前向心速度已达到此值时，弱吸停止追加，不把车辆／黄色／碰撞产生的已有速度直接截断或反向减速。
- `Center Softening Radius`：默认100 cm；进入此距离后，弱吸加速度和弱吸目标速度按距离线性降至0。中心无虚构方向，不直接修改姿态／位置。

中心外吸力均匀，只沿世界XY向心方向追加速度，不修改横向或Z速度、不改变抓地／转向／全车煞车。权威按当前速度解算增量后仍走原网络物理动作，因此回放不重算环境状态。单步追加=min(加速度×采样间隔×中心系数,max(0,速度上限×中心系数−当前向心速度))，黄色增量另行完整相加。多来源／其他外力可能使实际速度超过此值，这是弱吸停止追加的门槛，不是全车硬速度上限。

已核对现有驾驶：无油门时是与速度成比例的Coasting Drag，横向抓地也与侧滑速度成比例，没有静止锁定式自动煞车，因此没有扩大修改Movement核心或给黑洞添加全车低抓地。原Continuous Acceleration数值保留；实际拖拽速度还会受当前车型阻力／抓地影响，上限200不是保证能达到200。要更有挣扎感，应在BP逐渐提高Continuous Acceleration，同时验证保持向外油门仍可逃离。

手测先设Pulse Delta Speed=0隔离弱吸：松手应逐渐内移；持续向外油门仍能克服吸力；横向转向／绕行保留；中心附近拉扯减弱。再恢复原黄色数值，确认大拉扯手感保持。程序验证结果在本轮验证中记录，真人车型手感不由公式测试代替。

在单车测试图放一个大半径黑洞，把自身放在中心外侧：先调 Pulse Delta Speed=0 验弱吸引与扩张覆盖，再调 Continuous Acceleration=0 验每个间隔只拉一次，最后组合。测试圆心、最大半径内外、上下高度、侧向行驶、高速穿越、墙角、腾空、双黑洞重叠、提前删除及到期。观察 Z 与车身姿态不被直接重置。

Listen Server 与 Client 分别驾驶进入同一黑洞，比较扩张→维持→强拉和到期；检查延迟下的强拉、客户端纠正与提前销毁。生成由服务器执行，客户端不得重复生成。发布包、校园四机、正式特效画面与手感需要团队另验，不以程序夹具代替。

## 本轮验证

### 无显示且无吸力的实际运行诊断（2026-11-12）

用户进一步报告勾选后仍无显示／效果。重新唯读确认BP和Match实例均Debug=True、仍2秒扩张＋8秒维持。早期位置(1700,2120,50)窄范围未命中地面，宽范围命中Floor Z=-160.5；这不是已确认的无显示根因，不能据此声称绘制故障。排查期间用户另保存地图，实际运行位置已为(-2217.062,-468.105,-200.274)，保留该用户更改。

新增仅Editor测试的`HAL.VisualSmoke.BlackHole.SavedMatchVisibility`与显式`-HALBlackHoleVisibilityFixture`早期观察器，不修改任何生产规则或保存资产。UE5.8.2 Editor诊断构建R3成功。D3D12实际加载当前保存的Match：Time=0.001发现`BP_HALBlackHole_C_1`且Expanding、Radius=1.2；Time=10.001观察到Actor移除，自动化开始前已消失。`Render_R2.log`完整记录。因此已证实当前10游戏秒寿命会在启动／等待或玩家到达前耗尽，同时解释无图形及无吸力。不是未载入Actor，也没有本轮实际可见像素通过结论。团队测试应在BP Class Defaults→Black Hole→Settings→Timing把Duration Seconds暂调60或120，Compile／Save后重开比赛，最后恢复正式持续时间；代理按规则不改BP。

诊断报告`Saved/BlackHole/20261112/VisibilityDiagnosis/Render_R2`以明确的“已在画面采样前到期”失败保存，不能列为显示通过。此前Render_R1因开始后已无Actor超时，Python反射尝试的错误亦保留，最终用HitResult.to_tuple读取地面。新增测试不属于普通HAL.FutureCreation回归；无新的玩法修复、Game构建、双端或真人通过结论。未提交／Push。

### Match 未显示排查（2026-11-12）

用户报告放入Match后无显示。本轮以Unreal Python唯读加载已保存BP及MVPB_Match：BP CDO与地图实例`BP_HALBlackHole`的`bDebugDraw`均为False；保存参数为扩张2秒、维持8秒、半径700cm、高度容差200cm，实例位置(1700,2120,50)，缩放1。因此当前保存配方不会绘制gizmos，并非据日志认定Actor未生成。用户在地图选实例后勾Black Hole→Debug→Debug Draw并保存；若要以后放入的实例也默认显示，另在BP Class Defaults中勾选并Compile/Save。代理未修改BP或地图。

当前黑洞在地图BeginPlay立即起算10游戏秒总寿命，不等待比赛倒数。最近日志Match于15:49:12加载、15:49:15进入Playing，故默认配方约剩7游戏秒可在Playing阶段观察。此为现有规格，不自行改为整场常驻或比赛开始后计时。只读输出`Saved/BlackHole/20261112/VisibilityDiagnosis/Inspect.log`，本轮无源码改动／新编译或测试。

### Debug gizmos 补充（2026-11-12）

新增Editor专用三层圆形Spline预览（最大半径与上下高度），关闭碰撞并在游戏隐藏；运行时按当前实际半径绘制静态地面上方圆盘、同心圈及8个向心箭头，红色扩张／紫色准备／黄色强拉闪烁0.15秒。Debug Draw从EditDefaultsOnly改为EditAnywhere，允许地图实例单独勾选。无新玩法Tick、力或网络规则变动，保留团队已建立的`BP_HALBlackHole`。

用户保存关闭Editor后，UE5.8.2 Editor R4及Game构建成功（`Build_Debug_Editor_R4.log`、`Build_Debug_Game.log`）。`Debug_Tests_R2/index.json` **3/3**，0测试警告／失败／未运行／进行中，含新增`DebugGizmos`及原黑洞规则／物理生命周期测试。断言预览64点圆的半径／高度／关闭碰撞与显示开关，以及实际提交的圆盘、线条、箭头、三色与到期不再提交。NullRHI数据检查，不代替Editor真人观察或最终画面验收；本次不重跑双端，之前双端结果属于前次玩法版本。

首轮编译修正TObjectPtr取指针和UE5.8 GetLineBatcher接口，第二轮编译成功但被用户Editor锁DLL；关闭后更新成功。首轮显示测试2/3，因测试世界是Game而IsVisible同时考虑HiddenInGame，改为检查正确的编辑预览可见标记GetVisibleFlag后R2全通过，保留隐藏游戏的原规则。团队BP施工前后SHA256一致，代理未改资产或Config。原始记录均在`Saved/BlackHole/20261112/Debug_*`；未提交／Push。

### 用户关闭 Editor 后的最终验证

最终两Target：Editor R12／Game R3 Development均构建成功（`Build_Editor_R12.log`、`Build_Game_R3.log`）。

Editor Development R12 构建成功；黑洞正式代码自 Editor R3／双端 Network R1 起保持不变，后续只修正测试世界与量测方法。最终普通回归 `Tests_R6/index.json` **9/9**，0 警告／失败／未运行／进行中，包含黑洞两项、既有 Impact SolverThreadExecutionAndReplay 与 MovementAreas 回归。物理步回呼实际读取 -12、-700、-1400、-702.4（重复更新不变）、-712 cm/s，确认持续加速度积分、瞬时强拉、叠加、去重复和卡顿上限。使用真正 Chaos PostSolve 输出，不以未更新的 GT 快照代替物理结果。

双进程 NullRHI `Network_R1/Host/index.json`、`Client/index.json` 各 **1/1**，均0测试警告／失败／未运行／进行中。Host 出站人为注入80 ms（不是实测RTT）；两个本地拥有车辆的最高向心速度约917.41／905.00 cm/s，Client 实际重演43次，双方观察扩张与2500 cm最大半径维持、服务器到期销毁。无Ensure／Assertion／Fatal；两个测试进程正常退出。此为本机程序验证，不代替保存BP、特效画面、真人手感、四机或打包性能验收。

失败记录完整保留：普通R1为8/9，新测试误用负开始时间与未消费的外部冲量；R2～R4仍8/9或黑洞专项1/2，已修正世界时钟／刚体注册，但GT快照仍未反映整合结果。R5测试进程因新探针缺少空OnPreSimulate回调而断言退出；补齐后R6全通过。Editor R4／R6遇到测试访问protected方法，R9／R10遇到探针Handle限定／缺完整类型头文件，均只涉及新测试并已修复。不删断言、不修改生产规则来使测试通过。原有相机C4305编译警告未改。

1246份保存资产／Config／uproject最终SHA256再次逐份一致；没有创建／编辑任何BP、地图或材质。当前下一步为团队按本文建立 `BP_BlackHole`，先用Debug Draw验手感，再接表现事件；共用生成器与正式特效仍未制作。未提交／Push。以下保留关闭Editor前的历史时点。

基点 `main/3cb1386`，本机 UE 5.8.2-56702186。起始保留上一轮 `Docs/Project_Status_CN.md` 接续记录修改。

Game Development R1 构建成功（`Build_Game_R1.log`）。Editor R1 因 Blueprint 事件参数和成员同名导致 UHT 生成代码 C4458，已把事件参数改为 New Pulse Index。Editor R2 的生产／普通测试／联机测试 C++ 全部编译成功，但运行中的 UnrealEditor 占用两个模块 DLL，链接 LNK1104 失败（`Build_Editor_R2.log`）。没有结束用户 Editor 或尝试 Live Coding；已请用户保存关闭后继续。完整 Editor 更新尚未完成。

新增 `HAL.FutureCreation.BlackHole.RulesAndBounds`、`AuthorityPhysicsAndLifecycle` 与双进程 `HAL.NetworkSmoke.BlackHole.AttractionLifecycle`（夹具注入 Host 出站 80 ms），当前均**尚未运行**，不得作为通过证据。普通夹具覆盖扩张／强拉／到期边界、平面和高度、车重无关速度增量、重复更新与卡顿不堆叠；联机夹具用于验证两端生命周期和本地拥有车辆实际速度，待运行。应另跑现有 `HAL.FutureCreation.Impact.SolverThreadExecutionAndReplay` 与 MovementAreas 相关回归。

1246 份保存资产／Config／uproject SHA256 对照全同；无 BP／地图／材质修改。原有相机测试 C4305 警告保持。原始编译／保护报告目录为 `Saved/BlackHole/20261112`，不纳入 Git。未提交／Push。

下一入口：用户保存并关闭 Editor 后重新构建 Editor，运行上述普通／物理回归与双端新机关夹具；按实际结果修复并更新本记录。随后团队建立 BP、接表现和做真人手感／画面验收。
