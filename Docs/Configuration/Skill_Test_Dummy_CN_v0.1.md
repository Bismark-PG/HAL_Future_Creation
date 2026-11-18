# 正式技能测试木桩：配置与 Fred 场景使用

更新：2026-11-15。旧 `BP_PassiveTestVehicle`／Passive Definition 与源码保持，继续用于基础回归。本页介绍新的 `HALSkillTestDummy`；编译与自动化的实际结果见文末。

## 1. 职责与边界

新木桩继承 `TestVehiclePawn`，复用真实Chaos碰撞根、驾驶状态接收、区域GE、骨骼表现、受击／状态特效、PursuitTarget。它单独持有ASC和Health／Combat／Impact／Movement属性集，因为它不是玩家，不伪造PlayerState、Controller或房间成员。玩家车仍由PlayerState持有ASC。

核心区别：木桩可接收技能；不能因为它是合法目标，就认为它可以施放技能。`CanReceiveSkillEffects` 为独立目标接口，Car06施放者的 `VehicleReady` 检查保持。新木桩不接收驾驶输入、不授予Definition中的主动技能、不吸球、不执行自救、不占房间席位／胜负统计；可以被自然碰撞推动和由技能上抛。共用重力、物理材质及伤害／冲击抵抗仍沿用正式规则。

默认中性输入的木桩可以验证油膜GE和滑移后的物理结果，不能代替玩家驾驶验证“打滑是否好控制”。HP归零时清理区域绑定及Avatar状态效果，保留可读的0HP；之后不再作为技能目标。本版不添加自动驾驶或自动回血／复位；死亡后重新Play，或提高独立Definition的HP以支持重复测试。

## 2. 两份原生DA

程序提供两份新增原生示例（不修改既有车型DA）：

| 资产 | 用途 |
| --- | --- |
| `/Game/Data/Testing/DA_Vehicle_SkillDummy_Default` | 从Car02正式Definition复制的独立物理／外观／血量／抗性配置；HP=1000，保留合法车型配置 |
| `/Game/Data/Testing/DA_SkillTestDummy_Default` | 类型HALSkillTestDummyConfig，引用上面的Definition，配置TARGET标签与血条／WBP |

所有资产须随源码正常纳入Git同步。不要把Saved中的检查报告或Binaries当成团队交接内容。

想创建自己的变体：

1. 复制 `DA_Vehicle_SkillDummy_Default` 为 `DA_Vehicle_Dummy_Fred`。在独立副本调 **Health → Health → Max HP**；在Combat调伤害承受倍率，在Impact调Tier Reduction等。中性回归保持 Damage Taken Multiplier=1、Tier Reduction=0。
2. 外观可继续使用示例的Car02 Presentation；也可从另一辆正式车的Definition复制外观／物理配置。保持 **Collision Shape=ConvexMesh**、有效简单凸包Collision Mesh、Ready For Use、有效Local Config／Movement／BallControl。木桩不消费玩家输入，但底层正式Definition校验仍要求这些完整。
3. 不能引用旧Passive Box Definition；不能仅给组件勾选Simulate Physics来替代有效Definition。骨骼Mesh／AnimBP／Status FX仍按正式Presentation DA配置。
4. 复制 `DA_SkillTestDummy_Default` 为 `DA_SkillDummy_Fred`。**Vehicle Definition** 指向独立副本，**Label** 可填 `TARGET A`（1～40字符），**Show Health** 开启，**Widget Class** 暂留None。
5. 如果从零创建：Content Browser → Miscellaneous → Data Asset，分别选 **VehicleDefinition** 与 **HALSkillTestDummyConfig**，再按上述规则填好。复用示例通常更容易。

TestConfig为生成时配置；不支持在运行中替换整份DA来重建Avatar／物理。改配置后结束并重新Play。**Definition V1校验要求一个合法Primary Grant，保留复制来的配置，不要清空。木桩不会执行玩家Grant步骤，因此该条目不会授予木桩任何主动技能**，也不会退回PlayerState的默认技能。

## 3. 创建轻量BP

按照项目规则由团队在Editor手动完成；代理不修改Blueprint资产或Event Graph。

1. Content Browser → Blueprint Class → All Classes，搜索 **HALSkillTestDummy**，创建 `BP_SkillTestDummy`，推荐放 `/Game/Blueprints/Testing`。
2. Class Defaults → **Skill Test → Test Config**，指向 `DA_SkillTestDummy_Default` 或自己的 `DA_SkillDummy_Fred`。
3. 输入配置保持 **Auto Possess Player=Disabled、Auto Possess AI=Disabled**。不用配置AIController，不要设Player0，不要授予主技能，不要在Event Graph写BeginPlay／Tick。
4. 组件中已经具有健康、冲击、区域接收、PursuitTarget和表现组件，不需再添加副本。继承的Definition由TestConfig选择，使用TestConfig作为唯一入口；组件字段不是另一套调参来源。
5. Compile／Save。可把同一个BP多次拖进场景，每个实例选择不同TestConfig；不必为每个数值变体复制一个BP。

木桩只用于摆放目标，不要登记进正式VehicleCatalog，也不要选它作为GameMode的玩家Default Pawn Class。测试模式的车型列表已主动排除该类型。

## 4. Fred_TestScene：推荐单人离线测试

2026-11-15只读核对时，Fred保存的GameMode Override为 `BP_MVPB_ListenGameMode`，默认Pawn为Car06。为了单人快速测试，推荐在本测试地图切换现有 **BP_VehicleTestGameMode**；不修改正式房间地图或全局GameMode默认值。

1. 完整重开本轮编译后的Editor，打开 `/Game/Maps/Fred_TestScene`。
2. 推荐先复制现有 `BP_VehicleTestGameMode` 为 **BP_FredSkillTestGameMode**，再在Fred的World Settings中将GameMode Override设为这个副本。其父类必须是HALVehicleTestGameMode，Controller=HALVehicleTestPlayerController、PlayerState=HALPlayerState、GameState=HALMatchGameState。
3. 打开副本Class Defaults：Default Pawn Class可指定 **BP_VehiclePawn_Car_06**；在 **Vehicle Test → Test Vehicle Classes** 加入Car03／Car04／Car05等其他要测的正式车辆BP。默认车型自动包含在列表中，运行时通过面板切车。也可以直接沿用原测试GameMode及其Car02默认值，但修改共享BP会影响其他引用地图。
4. 地图保留一个PlayerStart，位置高于地面且能放下车辆完整碰撞体；不要让木桩或墙挡住出生点。
5. 拖入 `BP_SkillTestDummy`，让碰撞体底部在地面上方有少量落地余量。先放在PlayerStart侧面约400～600cm，避免初始重叠。测试Car06首测Radius=900cm，可先把木桩摆在这个范围内。
6. Play设置 **1 Player／Play Standalone**，正常Play。测试模式自动进入Playing，无比赛倒计时。使用左上角车型选择切换正式车辆，按原主技能键测试。
7. 新木桩在初始生成和换车后继续存在；旧Passive及地图遗留普通车辆仍沿用原清理规则。这是专门针对新木桩的保留边界，不代表所有预摆车辆都保留。新木桩不会进入可驾驶车型候选列表。
8. 选择Car06，靠近木桩发动，观察TARGET／HP、六星从左向右增长、逮捕时手铐和共享Stun、保护结束。警车移动出范围可以验证衰减。木桩不需要房间确认或占玩家席位。

观察相机、F2及Alt鼠标操作继续按 [单车离线测试文档](SingleVehicleTest_Setup_CN_v0.1.md) 使用。新木桩仅作为目标，不会抢相机；动态相机的OtherPlayers只取有PlayerState的正式玩家，因此木桩不会自动拉远玩家镜头。

## 5. 保留Listen模式时

也可以保持Fred当前Listen GameMode，正常建立允许技能的Playing流程后使用新木桩。它仍不计入最低玩家人数，因此放三个木桩不能满足四人／两人开局条件；开发房间或独立的测试模式应负责开局规则。Room Open练习期的伤害／技能权限仍生效，木桩不会绕过正式比赛阶段。

联机需要两端拥有相同代码与新增DA／BP资源。木桩在服务器生成／结算，客户端读取复制HP、区域／眩晕／保护状态与UI。木桩没有输入所有权，使用普通服务器刚体复制，不启用玩家Network Physics History。

## 6. 血条、六星和WBP替换

木桩自带屏幕空间TestHealth组件：原生TARGET、HP条和数值；Stun时血条变橙色。配置在木桩TestConfig，**Widget Class=None**使用原生回退。关闭Show Health可隐藏这套木桩提示；Car06六星是独立的技能Overlay，仍由警车技能DA控制。

自制血条WBP：父类 **HALSkillTestDummyWidget**，Designer创建实际Root／布局，填写TestConfig.Widget Class。**Get Dummy View**返回Label、HP、MaxHP、bReady、bStunned；**Receive Dummy View**每0.1秒推送只读数据。bReady=false时显示未就绪状态，不把0当成确定死亡。存在Designer布局后原生绘制退出；空WBP继续使用回退。默认组件绘制区域150×60，中心对应原生血条顶部，制作布局时保持这个锚点便于与六星对齐。

六星继续由 `DA_Ability_Car06_PolicePursuit → FX → UI` 的Star Count／Size／Gap／Stars Above Health调节；需要替换时使用HALPursuitHUDWidget，与木桩血条WBP是两个独立接口。

## 7. 验证入口与限制

新增原生过滤器：`HAL.FutureCreation.SkillDummy`。双端过滤器：`HAL.NetworkSmoke.SkillDummy`（隔离夹具使用 `-HALSkillDummyFixture`，不替代团队地图手测）。

UE **5.8.3-58210709**：最终 **Editor R8／Game R4** 构建通过。**Final_R2共49项全部通过**（47项无警告成功＋2项带警告成功，失败／未执行均0），包括3项新木桩专项、GAS Health／Impact／GF2换车、Stun、油膜、车辆测试工具、Car03／04／05／06及实际D3D12渲染回归。两项警告分别是既有无效／阻挡出生负例的故意Warning，以及龙卷风渲染夹具的D3D12保留虚拟资源258GB超过256GB预算提示；不记成零警告。

**Network_R2**：真实Listen Host／Client各1/1、零测试警告／失败、退出0，Host出站注入80ms延迟。验证每实例TestConfig复制、无Controller／PlayerState情况下目标ASC初始化、实际扣血、六星、共同Stun、逮捕后保护尾段、原生血条Widget绑定、服务器销毁后的双端清理；PlayerArray始终只有两名真实玩家。客户端先预载配置并确认木桩就绪，再由主机通过原主技能请求发动，未伪造客户端命中／效果结果。

开工 **1327/1327** 份既有资产／地图／Config／uproject及Passive源码哈希保持，新增两份原生DA。未修改保存的BP或Fred地图，未提交／Push。原始证据在 `Saved/SkillTestDummy/20261115`，可同步摘要见 [验证记录](Skill_Test_Dummy_Verification_20261115.json)。

早期失败均保留：R1／R2的指针、复制宏及测试API编译问题；R3链接被Editor占用（用户关闭后解除）；Logic_R1暴露空Primary Grant违反正式Definition校验，改为保留有效条目但跳过授予；Logic_R2／Final_R1暴露未初始化测试Actor导致EndPlay不触发，已补完整夹具；旧Health回归引用已删除的基线DA，更新为现有Car02碰撞资产；Network_R1暴露目标保护定时器误用施放者资格，已改为接收资格，并补客户端就绪握手。最终报告覆盖修正后的源码。

**未验收范围**：团队仍需按第4节手动创建BP、在Fred摆放并验证完整画面、真实驾驶／技能接触和手感；自动化不代团队编辑保存的BP或地图。原生油膜夹具显式提供接地遥测，不冒充驾驶实测；双端验证血条Widget实例及视图数据，未对新木桩血条进行逐像素图像验收。自制WBP、四机、打包／Cook、真实晚加入及非本次过滤器涵盖的机关仍待团队验证。新技能的目标判定应复用 `CanReceiveSkillEffects`，施放者继续使用独立的权限／姿态／冷却检查。

## 8. 2026-11-15：球命中后偶发缺少击退，待定位

本节保留补充日志前的只读检查结论；第9节记录诊断开发，第10节保存真人复现，第11节为修复与当前复测入口。

用户报告木桩被攻击球扣血、闪红，但经常只在原地抖动。本轮只做真实运行日志及代码审计，没有修改物理或资产、没有重新编译／运行测试。

最新`Saved/Logs/HAL_Future_Creation.log`本地20:24～20:25中，五次有效球命中均为25HP、Heavy→Heavy、冲击不免疫、未被支撑抑制。规则请求如下，均为cm/s，不能解读为已经测到的实际移动速度：

| 日志UTC时刻 | 物理帧 | 请求水平补速 | 请求最低上抛 |
| --- | ---: | ---: | ---: |
| 11:24:44.462 | 723 | 839.62 | 750 |
| 11:24:54.609 | 1332 | 824.62 | 750 |
| 11:25:04.690 | 1937 | 818.07 | 750 |
| 11:25:12.575 | 2410 | 818.41 | 750 |
| 11:25:18.395 | 2759 | 805.28 | 750 |

上一轮`HAL_Future_Creation-backup-2026.11.15-11.24.24.log`的UTC11:24:11.314，Frame4432同样Heavy命中；11.326即Frame4433又记录Floor_0接触、SupportSuppressed=1。这只是快速再次触地的证据，不能据此断言具体哪次对应用户看到的抖动，也不能把后续地面抑制误认为攻击球本身被抑制。最新一轮另有命中后约1.4秒的地面接触，说明并非所有命中都完全没有上抛。

代码边界：CombatResolver扣血后通知红闪，随后独立结算冲击。DamageFeedback只租借已注册可见Mesh的CustomDepth／Stencil并更新局部后处理，没有碰撞体、物理速度或Simulation写入。引擎SetRenderCustomDepth标记渲染状态更新，不是重建车辆物理根。当前未发现特效直接取消冲击的路径，但尚未做开启／关闭表现的对照，不能排除新增渲染工作暴露旧时序问题。

木桩明确不启用玩家NetworkPhysicsHistory。其QueueImpact直接在游戏线程调用AddImpulse及角速度Setter；玩家History路径在物理动作回调中依据Internal V/W处理。这里存在需要验证的执行／采样时序差异，尚未证明它就是根因。Combat的Impact=Applied只表示QueueImpact返回成功，不证明下一Solver帧的速度／位置；现有`hal.ImpactPhysicsLog`也只覆盖History回调，不覆盖木桩直接分支。以前木桩伤害／目标资格及渲染测试的通过不能替代连续真实球碰撞的物理验收。

下一步：先为非History路径观测同一事件的请求、实际消费及后续Solver帧V/W／位置，结合地面接触区分“未施加”与“施加后被接触／约束抵消”；同条件切换共享受击DA的Enabled做A/B。需要改执行方式时，优先考虑非History动作也在明确物理步骤消费，继续保留普通服务器刚体复制，不把木桩变成玩家或授予输入历史。不以提高Heavy数值、改抗性或放宽支撑分类代替定位。

## 9. 2026-11-15：新增非History冲击物理日志

本节是修复前的历史记录。第11节修复后仍用同一开关和采集方式，但新增`ConsumePT`；`SubmitGT`改为排队预览，实际施力转入物理步。下方旧字段`RequestedDV`及GT唤醒说明用于阅读此前日志，不适用于新排队路径。

用户授权先补日志，由团队复现。本轮扩展既有`hal.ImpactPhysicsLog`，默认0；诊断代码不进入Shipping，不新增Tick／定时器／RPC／反射调参，也不改变原AddImpulse、角速度或支撑规则。所有非History车辆共用该窄诊断，木桩包含在内；玩家History原日志继续保留。CombatResolver的AttackId／Pulse由ImpactReceiver透传，仅用于日志关联，不修改冲击载荷。

### 9.1 如何采集

1. 重开本轮完整编译后的Editor，进入Fred的正常Playing流程；单人Standalone直接操作，联机则在**服务器／Host**输入命令。
2. 控制台逐条输入：

   ```text
   hal.CombatLog 1
   hal.ImpactPhysicsLog 1
   hal.DamageFeedbackLog 1
   ```

3. 第一轮沿用目前配置，可额外输入一次`hal.DamageFeedbackStatus`留存表现准备／启用状态。连续发射6～8次真实攻击球，每发间隔至少2秒，记下哪几发只抖动、哪几发正常飞出。最后一发后也等待至少2秒，再停止Play／退出Standalone。
4. 日志仍在项目`Saved/Logs/`，Standalone常用`HAL_Future_Creation.log`，旧轮次自动成为backup。保留完整日志；只筛Combat行会丢掉新物理帧、地面接触和表现证据。告诉代理对应轮次和异常发数即可检查。
5. 继续运行而暂时不采集时，把上述三个开关分别改为0；关闭ImpactPhysicsLog后，后续物理步会归还未完成的诊断记录。
6. 第一轮后如需A/B，另开一轮，把共享受击DA的Style.Enabled设为false、保存并重新Play，用同一位置／同一球重复；日志开关相同。记录哪轮Enabled开／关，其他测试配置沿用第一轮。本轮尚未代团队执行Fred的这个对照。

### 9.2 日志含义

新行统一前缀为`Vehicle impact trace:`，按`Actor + Attack + Pulse + Event`关联；Attack／Pulse可与`HAL Combat:`及`DamageFeedback`对照。常规球与碰撞请求都会提供真实攻击标识；个别没有Combat请求的内部动作保留Event，Attack可为零值。

| Phase／字段 | 含义 |
| --- | --- |
| `SubmitGT` | 原物理命令调用前后的游戏线程物理代理读数，不代表已积分；包含请求帧、质量、AwakeBefore／After、BeforeV／W、RequestedDV／W、ImmediateV／W |
| `RequestedDV` | 实际传入AddImpulse的速度增量，包括依据当时GT读数算出的最低上抛补量；不同于Combat里仅水平的RuleDV |
| `PTEnabled` | 本移动组件的物理回调是否启用；0时不会产生本组件的后续采样 |
| `SamplePT` | 真实物理线程上下文中、当前物理帧开始时的Internal状态；后续帧可以反映前面已经积分的结果，**不是同一帧PostSolve回调或动作消费确认** |
| `Frame／SubmitFrame／AgeFrames` | 当前真实Solver帧／提交时最后观察帧／从第一次SamplePT起经过的帧数；不使用渲染帧冒充物理帧 |
| `V／W／Pos` | 当前Internal线速度cm/s、角速度rad/s、世界位置cm |
| `Sleeping／GroundedPrev` | 当前刚体休眠标志／上次驾驶接地观测；GroundedPrev不是本帧接触证明，需结合Combat里的Floor接触 |
| `PendingImpulse` | 尚待积分的Internal线冲量，单位kg·cm/s；用于区别“冲量仍待处理”和“已经产生速度” |
| `TravelFromGT／TravelFromFirstPT` | 相对提交前GT位置／首次Internal采样位置的实际位移cm；前者可能包含GT与PT时间差，后者不含首次采样之前的运动 |
| `Evicted` | 同组件并发诊断超过8条而淘汰旧观测，只影响日志，未丢弃冲击动作 |

每次冲击最多观察9次，AgeFrames为0、1、2、3、7、15、31、63、95；约覆盖96个物理帧，60Hz时约1.6秒。没有在采样点之间推算不存在的帧；停止／切换History或退出Actor时清理记录。同一窗口内其他碰撞也会改变V/W，样本不是逐攻击隔离的净效果，须结合Owner和帧号分析。

正常案例可以出现：SubmitGT.ImmediateV仍为0，首个SamplePT.V仍为0但PendingImpulse非零，下一帧才出现水平／向上线速度和位移。不要据此把即时读数为0判为丢冲击；持续多帧无速度／位移、冲量被清掉、睡眠或紧邻地面碰撞才是需要对照的线索。

### 9.3 验证边界

当前Editor R2编译成功，原生Native_R2 **13/13通过**、测试警告／失败／未执行均0；真实TaskGraph Solver验证日志开关前后V/W及三维位移一致、攻击关联、4帧观察、关闭清理和96帧自动归还。夹具为离地、无重力／无阻挡的Box；没有伪造GT为物理线程，也没有用预设V/W替代待测直接冲击的结果。首轮Native_R1有3条新位移预期失败：未计入第一观察帧仍有待积分GT冲量；依据实际PT记录对齐为Steps-1个完整运动帧后复编通过，运行时逻辑未为通过测试调整。

最终Game构建及资源保留结果见[诊断施工验证摘要](Skill_Dummy_Impact_Diagnostics_Verification_20261115.json)。本轮验证不表示Fred真实地面上的抖动已修复，不代表实际球碰撞、FX开关A/B、多人或打包验收。团队下一步按9.1采集异常样本。

## 10. 2026-11-15：真人复现7次球命中的逐发结论

团队反馈记忆中第1／5／6或7发只抖动。最新完整日志实际有7次攻击球命中，按AttackId而非所有碰撞次数排序，确认异常为**第1、5、7发**。下列时刻为JST（+09:00）：

| 发数 | 命中时刻 | 下一物理帧 | 后续结果 |
| --- | --- | --- | --- |
| 1 | 21:09:13.793 | Sleeping=1，V/W/冲量全0 | 未上抛，采样位移约0.14cm |
| 2 | 21:09:39.242 | 已醒，有待施加冲量和请求角速度 | 正常，下一采样Vz≈731.833cm/s |
| 3 | 21:10:07.726 | 已醒，有待施加冲量和请求角速度 | 正常，下一采样Vz≈731.833cm/s |
| 4 | 21:10:12.823 | 已醒，有待施加冲量和请求角速度 | 正常，下一采样Vz≈731.833cm/s |
| 5 | 21:10:16.942 | Sleeping=1，V/W/冲量全0 | 未上抛，采样位移约0.58cm |
| 6 | 21:10:28.476 | 已醒，有待施加冲量和请求角速度 | 正常，下一采样Vz≈731.833cm/s |
| 7 | 21:10:48.205 | Sleeping=1，V/W/冲量全0 | 未上抛，少量自然推移；末尾又有车辆接触 |

三次失败的SubmitGT都是AwakeBefore=0、AwakeAfter=1，且RequestedDV同时包含正常水平分量和750cm/s向上分量，ImmediateW也已写成请求值。紧接着的真实物理帧却仍休眠，角速度与PendingImpulse都为0；后续96帧中没有对应击飞。成功的3／4／6发同样可以从GT休眠开始，因此不是“所有睡眠目标都不能被打飞”，而是需要稳定处理睡眠／唤醒与GT→PT命令传递的边界。

7次的伤害、Heavy档位、冲击免疫、支撑抑制均正常；失败不是把后续地面SupportSuppressed误读成球攻击被抑制。当前能确认请求未可靠落实到物理状态，但还不能仅靠九个采样点认定是哪一底层同步写入把命令丢失。首帧保持睡眠且冲量为空的共同特征，比单纯“落地太弹／角速度过大”的解释更直接。

受击特效未写物理根或速度，且本轮正常／失败都能闪红；没有证据表明是颜色或RGB强度取消了冲击。本轮尚未关闭FX做A/B，因此不声称排除了所有间接时序影响。正确修复入口优先是非History的冲击执行：缓存接受动作，在明确物理步内唤醒刚体并依据当前Internal V/W施加一次，保留普通服务器刚体复制。不要把木桩接入玩家输入History来绕过问题，也不通过修改Heavy数值／抗性来掩盖丢动作。

补充回归应覆盖真实地面上已睡眠及已清醒的木桩、睡眠转换边界、连续攻击和同帧自然接触，并检查实际升高／位移；此前无重力／无阻挡Box测试只证明日志开关不改该夹具结果，没有覆盖此边界。

本轮只读分析和文档留档，未修改或编译运行时代码，未执行新的仿真测试。原始快照保留本机`Saved/VehicleIssues/20261115_DummyImpact/Run_20261115_211056.log`；可同步的逐发标识、请求与PT原文见[复现证据](Skill_Dummy_Impact_Reproduction_20261115.json)，原始运行日志不纳入Git。

## 11. 2026-11-15：非History休眠冲击修复，待Fred真人复测

### 11.1 实际修改

非History车辆（包含新技能木桩）的冲击不再从游戏线程直接调用AddImpulse／角速度Setter。服务器将通过规则校验的`FVehicleImpactAction`放入线程安全队列，在既有AsyncPhysicsTick的正常前向物理步中取出一次；基于同一个Internal刚体的真实状态计算增量，必要时在该步唤醒，再加入线／角冲量。木桩仍无Controller／PlayerState、不启用玩家NetworkPhysicsHistory，继续普通服务器刚体复制；玩家History冲击处理沿用原路径。没有新增Tick、RPC、反射字段或DA参数。

同物理步多次命中会计入该步已经待积分的冲量：水平补速累加；最低上抛是下限，例如500后750最终为750，不能重复补成1250；最后一次Pitch／Roll目标生效，保留Yaw和已有更高上升速度。零增量的普通地面抑制结果只消费记录，不主动唤醒刚体。队列最多128项，满时拒绝新动作而不丢弃已接受动作；停用／注销／退出、切换物理根或启用History时清理，未注册或没有物理消费者时不再返回接受成功。原伤害、三档冲击、抗性、支撑分类和脱球数值未改。

`Impact=Applied`／`bImpactApplied`继续表示规则请求已接受排队，不能理解为已经击飞；实际执行看`ConsumePT`，实际运动看后续`SamplePT`及画面。

### 11.2 为什么会发生，以及为什么以前没有发现

第10节失败样本的关键不是冲击太小：GT侧记录已醒且请求正确，但接下来的Internal刚体仍睡眠，线速度、角速度和待积分冲量均为0。旧路径把“游戏线程代理接受唤醒／施力”当成完成，未确保这些操作在同一个真实物理步落实。这次将唤醒、计算和施力统一到PT，去掉该跨线程交接的不确定边界；并不是提高Heavy力度来掩盖问题。尚未独立重现并锁定引擎内部究竟哪一次同步写入丢失，因此不把具体引擎语句列为已证实根因。

核对开工HEAD `5044110`可见，非History的GT AddImpulse＋SetPhysicsAngularVelocity分支在本轮受击特效开发前就存在。玩家有另一条History物理动作路径，且驾驶中经常保持清醒；长时间静止的木桩更容易暴露睡眠边界。旧专项验证离地清醒Box、规则／伤害与表现，未覆盖真实地面休眠；13项诊断专项通过也不能证明当时没有这个缺陷。准确说法是“以前未捕获”，无法证明“以前从未发生”。受击表现没有物理根／速度写入；本轮未做Fred的FX开关A/B，不据时间先后断言新特效制造了物理错误，也不排除工作负载影响旧问题出现时机。

### 11.3 新日志如何阅读

采集命令和完整日志位置沿用9.1，默认仍关闭，Shipping不包含诊断；每次最多9次SamplePT，观察窗和8条诊断预算不变。

| Phase／字段 | 本轮含义 |
| --- | --- |
| `SubmitGT Mode=QueuedPT` | 仅排队；GT的AwakeBefore／After或ImmediateV／W可以保持未动，这不再是异常证据 |
| `PreviewDV／PreviewW` | 根据提交时GT读数的预览；不是最终施加增量，不与旧日志RequestedDV混读 |
| `ConsumePT` | 同一Internal刚体的实际消费；含真实Frame、PTContext、SleepingBefore／After、EffectiveBeforeV／W、AppliedDV／DW及PendingDV／DW |
| `NoOp=1` | 该动作最终无需增量，例如已被规则抑制的普通地面补量；无额外唤醒／施力 |
| `EffectiveBeforeV／W` | 当前Internal速度加该步已有待积分增量，避免同帧重复补足上抛／旋转 |
| `SamplePT` | 消费之后的帧首状态；后续帧反映已积分结果，不是同帧PostSolve。结合位置／速度确认实际击飞 |
| `RejectedGT Reason=QueueFull128` | 游戏动作队列已满，规则返回未接受；不同于8条诊断预算的Evicted |

正常休眠重击可以依次看到：SubmitGT仍Awake=0 → ConsumePT的SleepingBefore=1／After=0、正常AppliedDV／DW → 后续SamplePT有向上／水平速度与位移。不要把新SubmitGT没有立刻唤醒或ConsumePT只有待积分冲量误判为失败。若有SubmitGT却长期无ConsumePT，或消费后再次归零且无位移，请保留完整接触／物理日志继续分析。

### 11.4 本轮验证和限制

UE **5.8.3-58210709**：最终 **Editor R4／Game R2** 构建成功；最终原生＋D3D12 **96/96通过**（94项无警告成功＋2项带警告成功，失败／未执行／进行中均0）。警告来自既有无效／阻挡出生负例，以及D3D12累计保留虚拟资源258GB超过256GB预算提示；不是零警告构建或测试。

新增真实TaskGraph Solver地面夹具：静态地面、重力、接触稳定后通过正常GT接口安排6次休眠／6次清醒；消费前由真实PT确认睡眠状态，12次均实际升高＞70cm、横移＞90cm。为隔离冲击消费与持续零输入抓地／转向的唤醒，夹具地板只对驾驶Visibility Trace忽略，物理碰撞和重力保留；这不是完整车辆驾驶验收。另检验同帧两动作的上抛下限／最终角速度、已有更高上升速度、连续物理步不重复施加、零补量不唤醒及停用清理。原13项及日志开关、4／96帧采样也纳入最终回归。

**Network_R2实际Listen Host／Client各1/1、零测试警告／失败、退出0，Host出站80ms。** 使用原生正式木桩及保存的凸包Definition，从生产CombatResolver提交Heavy＋25伤害，经ImpactReceiver、队列、服务器物理与普通刚体复制。为明确覆盖PT休眠，使用既有共享Stun暂时停止驾驶辅助，再通过正常接口休眠；主机ConsumePT实测SleepingBefore=1→After=0、DV=(900,0,750)cm/s及DW≈(0,5.131,0)rad/s。Host／Client分别观察到约109／146cm升高和92／84cm横移、HP=975，并验证销毁清理且真实玩家仍仅2名。先前Network_R1也验证了正常状态目标的运动，但当时GT睡眠不代表消费时PT仍睡眠，因此不把R1算作PT休眠覆盖。

早期Native_R1／R2的新夹具未达成真实休眠前置条件，均有测试失败，未算通过；修正为稳定接触并隔离持续驾驶的零力唤醒后，Native_R3专项通过，最终96项再次覆盖。运行时实现未通过强制禁止木桩休眠来绕过问题。所有记录见[本轮验证摘要](Skill_Dummy_Impact_Wake_Fix_Verification_20261115.json)，原始构建／报告／日志在`Saved/ImpactWakeFix/20261115`。

开工 **1337/1337** 份资产、Config、uproject及构建入口哈希保持，未修改／重存任何BP、DA、地图或受击特效调参，旧Passive源码不动。未提交／Push。

自动化的双端攻击是服务器解析器请求，不是真实飞行球的碰撞检测；未代替Fred真实球连续命中、墙角／复杂接触、高速／侧撞、FX开关A/B、四机或打包验收。下一步由团队复测真实球；本次完成的是代码、编译和列明的自动化，不提前标记真人验收通过。

### 11.5 团队复测步骤

1. 重开Editor，不需要重新配置BP或DA。打开Fred_TestScene，沿用目前木桩、球及受击DA参数。
2. 按9.1开启`hal.CombatLog 1`、`hal.ImpactPhysicsLog 1`、`hal.DamageFeedbackLog 1`，单人用Standalone；联机在Host打开物理日志。
3. 先让木桩落地静止数秒，再发射；连续测6～8发，每发间隔至少2秒，并补木桩刚落地／仍在运动时的连续命中。不要为了测试把睡眠禁掉。
4. 记录仍只抖动的发数，最后一发后等待至少2秒。保留完整Saved/Logs日志并告知轮次；由代理按Attack／Pulse／Event核对SubmitGT→ConsumePT→SamplePT。
5. 真人基本通过后再补墙边／侧撞、不同重力与机关、联机长时间待机后击飞；无异常可把三个日志开关设回0。
