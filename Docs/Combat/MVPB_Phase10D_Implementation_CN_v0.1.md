# MVP-B 10-D：统一冲击档位施工与迁移（v0.1）

日期：2026-10-15（Asia/Tokyo）；状态更新：2026-10-16。用户已同意 [讨论方案](MVPB_Phase10D_Impact_Discussion_CN_v0.1.md) 并授权施工。初始代码基点 `main / 1eb02c4`；当前线程修复及四份DA迁移已在`663e1b0`提交。**规格／源码完成，修复后两个Target及28项自动化通过；用户确认击飞效果恢复，档位响应力度暂定、以后调参。可进入10-E规格确认，10-D完整物理／扰动验收仍待补齐。** 最新团队反馈见第10节，原施工与修复证据分段保留。进度入口为 [Project_Status](../Project_Status_CN.md)。

## 1. 目标与规则定稿

内容制作只配置 `None / Light / Medium / Heavy`（无／轻／中／重），不为每颗球、每个机关填写物理冲量。公共物理响应表仍由底层调参者维护，沿用当前保存的 `DA_Combat_TestBaseline_v1.BallImpacts` 三档响应，不能用C++默认值覆盖。

| 来源 | 内容配置 | 本轮处理 |
|---|---|---|
| 主动发射球／未来技能与机关攻击 | 固定档位，可为None | 接收服务器几何方向，按档位追加水平反应、最低向上速度及翻转；球发射时保存档位 |
| 普通车车碰撞 | 来源车辆Collision Tier Cap | 接收方按公共物理标尺选档，再按来源能力封顶；双方分别结算 |
| 普通静态墙／环境／着地 | 默认Environment Tier Cap | 无需ASC或专用Actor；依赖Chaos刚体Hit通知 |
| 特殊墙／碰撞机关 | 可选HALImpactSource组件：Automatic Cap或Fixed Tier | 都属于自然接触响应；Fixed也必须先达到有意义接触门槛，不使静止贴墙反复重击 |
| Free球 | 默认Free Ball Tier Cap | 走普通接触适配；Controlled球无规则冲击；Launched球由攻击入口独占 |

实际档位 = 来源档位减去目标降档数，最低None。`TierReduction`为GAS Attribute，基础值来自车辆DA；普通加法GE可叠加／移除，最终取整向下并限制0～3。只支持整数档位设计，负修饰可抵消现有抗性，但不使最终值低于0，也不自动升高来源档位。免疫标签单独使用 `State.Combat.ImpactImmune`，直接令实际档位为None；后续若需易冲击／超重等，另定规则，不偷用负抗性。

伤害与冲击各自独立：冲击免疫不减伤，伤害免疫不降冲击；0伤害／HP0仍能有有效冲击。所有规则脱球都比较**实际档位**与目标`Minimum Drop Tier`，且`Drop On Impact`必须开启。None永不因本次冲击脱球。

免疫保留自然推挤、墙体阻挡、碰撞减速与自然姿态变化；不撤销Chaos已经发生的碰撞，不承诺车辆绝不翻倒。持有者退出、球销毁、约束失效或距离安全释放也不属于冲击免疫范围。

## 2. 代码职责与扩展入口

```text
普通接触 → HALImpactReceiver.HandleContact → CombatAction/HitRequest
球／技能／机关主动攻击 → 共用CombatAction/HitRequest
    → CombatResolver（伤害分支独立）
    → HALImpactReceiver.Resolve（GAS降档／免疫、有效档位、脱球）
    → Movement.QueueImpact → Authority Network Physics Action → Chaos
```

- `HALImpactTypes`：独立的新档位枚举、显式迁移版本、来源提案、结果、公共标尺。旧`EVehicleKnockbackTier`序号0/1/2未变，新枚举None/Light/Medium/Heavy为0/1/2/3，使用显式映射。
- `HALImpactAttributeSet`：目标TierReduction复制及边界约束。玩家仍由PlayerState持有ASC，被动车由自身ASC持有；没有给球／墙／机关增加ASC。
- `HALImpactReceiverComponent`：车辆原生组件，统一所有来源的冲击结果、接触去重及脱球。PassiveTestVehicle同样接入，不要求每个墙额外接线。
- `HALImpactSourceComponent`：可选、无Tick的纯碰撞来源配置；不是技能系统。
- `FVehicleImpactAction`：服务器确定的速度变化和姿态参数。重模拟只重放物理，不再读GAS、扣血或脱球。可靠Network Physics action带引擎SourceId/ActionId/ServerFrame，项目EventSequence用于载荷身份检查；遥控PI车辆继续由权威物理状态展示。无历史的Standalone和被动车在权威端施加一次。
- `FHALImpactSummary`只读复制最近一次结果，供HUD／日志检查；不是可靠表现事件队列，连续高速事件可合并显示，不能把OnRep当作再次施力入口。其ServerPhysicsFrame是结算时观察到的帧，实际执行帧由引擎动作队列指定。

未来无ASC机关／技能：在服务器创建并持有`FHALCombatAction`，提交`bApplyDamage`／`bApplyImpact`各自开关；冲击填`Request.Impact.Mode=ExplicitTier`及Tier，方向填命中几何，伤害沿用10-C来源快照。不应继续使用旧`ResolveVehicleHit`即时兼容入口或每帧重建Action；脉冲／去重／来源寿命仍遵守10-C。Sweep的NormalImpulse可为0；主动攻击仍可用ExplicitTier，普通接触适配不会把0冲量编造成重击。

## 3. 自然物理、连续接触与重复响应

普通碰撞标尺使用`|NormalImpulse| / 接收方质量`，单位cm/s，近似本次自然法线速度变化；不会使用碰撞回调后已经变更的速度作为唯一碰撞前速度。低于Light为支撑／轻擦，不触发规则；切向滑动不单因总车速高而升档。

接触按目标车辆＋来源Primitive组件记录。持续收到报告时只允许一次有意义结果，包括降为None／免疫的结果；同一接触先支撑后发生有意义撞击可结算一次。连续停止报告超过`Contact Rearm Seconds`才重新允许，清理采用定时器；最多128个活跃接触，满时不逐出旧记录以免重新触发。这里是接触静默窗口，不是全局受击冷却；另一来源仍可合法命中。短于窗口的分离再撞可能合并，属于当前可调取舍。

Launched球在车辆回调中跳过，球命中通过源组件标记同一接触已处理，随后球转Free也不追加一次车碰撞反应。迁移后旧`BallControl.HandleVehicleCollision`的原始冲量脱球路径退出，仅消费实际档位；原锁定时间与脱球后球的释放参数保持原值。

物理接触响应只沿法线补足，**不套用球的向上发射／翻转**。先计入自然碰撞已产生的速度变化，并用双方质量、当前相对法线速度和最大规则恢复系数限制可追加量；双车按逆质量份额分配预算，避免两端把整份相对反弹再加两遍。球主动攻击同样扣除本次自然水平冲量，但仍可按档位制造击飞。Light主动攻击不人为清除已有下落／翻转。

自然接触预算是由Hit通知数据重建的保守策略，**不是完整的接触求解器证明**：多点／墙角、旋转、同时接触、运动平台都需实测。它限制规则追加能量，不改变材质本身的恢复系数，也不抵消外部持续驱动力。不得仅凭单元测试宣称墙角稳定或联网物理已验收。

## 4. 显式迁移与现有资产保护

新增字段默认`Legacy`，不自动启用新行为。`DA_Combat_TestBaseline_v1 → Collision Impacts.Version`是总开关。只有总开关为TieredV1才走新版：

- 车辆必须`VehicleDefinition.Impact.Version=TieredV1`；否则明确报错、禁用该车物理，不取原生默认值冒充已迁移值。
- 球必须`BallDefinition.Gameplay.Impact.Version=TieredV1`；否则明确报错、禁用该球物理／拒绝发射。
- 旧请求LegacyMeasurement在新版不能偷用物理强度选档；伤害分支仍独立，冲击给出InvalidMeasurement。
- 旧字段保留供回退；新版不使用旧球Knockback Strength Multiplier、旧BallImpacts选档阈值或旧Drop Collision Impulse Threshold。公共三档响应仍使用现有BallImpacts数据。

### 4.1 团队在Unreal Editor中的操作

1. 保存需要保留的内容，关闭Editor／游戏窗口和Live Coding，完整编译当前Editor Target。团队应统一补丁；本机验证环境只有5.8.2，原10-C环境为5.8.3，本轮不擅自安装／切换引擎。
2. 打开项目后，不在Play中修改配置。Content Browser定位`/Game/Data/MVP_B`，先核对并留档现有玩家、被动车、球、Combat四份DA。不要使用Reset to Default，不改Physics、Movement、BallControl、Health、Combat和LocalConfig已有字段。
3. 打开`DA_Vehicle_Player_TestBaseline_v1`，Details搜索`Impact`，展开**顶层Impact结构**（不是BallControl的Forced Release）：填Version=TieredV1，Collision Tier Cap=Heavy，Minimum Drop Tier=Medium，Drop On Impact=true，Tier Reduction=0，Save。已存在的Ready For Use保持勾选。
4. 若工程还有被动车或其他VehicleDefinition，逐一做同样迁移；`DA_Vehicle_Passive_TestBaseline_v1`也要更新，即使当前NetTest没有放被动车。更重／更抗冲击的车以后改上限、最低脱球档位或降档数即可。
5. 打开`DA_Ball_Normal_TestBaseline_v1`，展开Gameplay → **Impact**，Version=TieredV1，Tier=Medium，Save。Gameplay → Damage的10-C版本／倍率等保持已有值。
6. **最后**打开`DA_Combat_TestBaseline_v1`。保留Ball Impacts整组现有三档水平／向上／翻转及角速度配置。展开新增**Collision Impacts**，明确填Light Threshold=50，Medium Threshold=125，Heavy Threshold=300，Environment Tier Cap=Heavy，Free Ball Tier Cap=Light，Contact Rearm Seconds=0.2，Maximum Rule Restitution=0.15；最后Version=TieredV1并Save。这些是新标尺的**测试起点，不是已调好的正式物理基线**。50/125/300不能与旧球速度1000/2600混用。
7. 确认Project Settings → Game → Vehicle Knockback仍引用上述Combat DA（已有引用无需改）。完整停止并重新启动游戏／双端，运行时缓存不会靠修改DA即时刷新。
8. `BP_RoundedVehiclePawn`继承的ImpactReceiver由C++自动创建，无需Event Graph接线。若蓝图打开后看不到新组件，先完整重编再重开；由团队手工Compile/Save确有必要更新的蓝图。不要同时在蓝图OnHit中再调用冲量／脱球。

回退顺序：停止所有游戏，先把总开关Collision Impacts.Version恢复Legacy并保存，再按需要恢复车辆／球Impact版本。旧调参字段保留，其他10-C伤害规则不随冲击回退改变；不得在运行中切换版本。

### 4.2 特殊墙／碰撞机关（可选）

普通墙无须操作。特殊来源由团队创建或打开轻量Actor蓝图，保留真正阻挡车辆的Primitive根／网格，Add Component搜索`HAL Impact Source`，Mode选Automatic Cap（只封顶）或Fixed Tier（有意义碰撞固定档），Tier选所需档位，Compile／Save并放入测试地图。无需ASC、Blueprint Tick或Event Graph。固定重档碰撞墙仍采用自然接触预算，不等于重档主动弹射机关；主动弹射后续走ExplicitTier攻击接口。

## 5. 调试与团队验收

仍按既有命令行启动双进程Listen Server，或Editor多进程NetTest；两端进入Playing后测试。先在主机控制台执行`HALGASStatus`找到两名玩家的PlayerId；确认Ready=1、CombatReady=1、**ImpactReady=1、ImpactVersion=1**。ImpactReady只代表属性已初始化，Version=0仍是旧行为。

主机执行`hal.CombatLog 1`开启权威结果日志，主机日志能见`Source=Combat.Source.Collision`或Ball、SourceTier、EffectiveTier、ImpactImmune、Drop、Frame。数值0/1/2/3为无／轻／中／重。HUD左上角显示本地最近结果；两端`HALGASStatus`可核对最近档位和HP，日志目录／自定义abslog仍按原启动步骤。

为本环节提供Development调试入口（Shipping无效果、客户端不能改服务器）：

```text
HALImpactDebug 目标PlayerId 降档数(0到3) 免疫(0或1)
```

例如`HALImpactDebug 257 1 0`将257号玩家降档设为1；`HALImpactDebug 257 0 1`测试免疫；`HALImpactDebug 257 0 0`恢复测试基础值0并移除此命令加的免疫。它修改属性基础值及独立的调试免疫贡献，不用于正式Buff，也不移除其他GE授予的免疫；结束整场会销毁测试状态。重新绑定同一ASC不会重置属性／HP。10-E接入后，调试免疫由Loose Tag改为原生无限时调试GE，以保持Mixed复制策略一致；关闭仅移除本命令自己的句柄，不移除8秒技能GE或退还冷却，见[10-E实施](MVPB_Phase10E_Implementation_CN_v0.1.md)。

| 测试 | 预期 |
|---|---|
| 普通球Medium命中，无抗性 | 仍扣25HP；实际Medium，可击飞；持有另一球的目标按Medium脱球 |
| 降档1后被Medium球命中 | 伤害仍25，实际Light；MinimumDropTier=Medium时不掉球 |
| Heavy测试球＋降档1 | 实际Medium，仍可脱球；测试后将球DA恢复Medium |
| 冲击免疫＋普通球 | 伤害仍25；实际None；无规则击退／击飞／脱球，仍可自然推挤 |
| 正撞／侧撞／追尾／撞墙／重着地 | 均通过同一降档／免疫；普通车车不扣血；正常支撑／轻擦不刷受击 |
| 高速沿墙切向滑行、持续顶墙、墙角 | 不因总车速或逐帧报告不断追加；无持续注能弹射 |
| 两车相向移动，互换主机／客户端攻击 | 两边最终收敛；主动球不因后处理速度偏小而意外降档；自然碰撞仍受真实角度影响 |
| 持球／发射／脱球锁定、发射者退出、球转Free后同次接触 | 无旧冲量脱球旁路、无双冲击／双扣血／立即吸回；生命周期安全释放保留 |
| 低延迟及约100ms实测RTT、1%丢包、20ms抖动 | 权威结果一致，无持续回拉／超过1秒振荡／重复脱球；记录NetMetrics与两端日志 |

先单机验证档位／免疫，再双端互换身份，最后网络扰动。四台真实PC同一有线网络仍是MVP-B最终验收，自动化和同机双进程不能替代。

## 6. 编译、自动化与证据

最终Editor自动化 **27/27通过、0警告／0失败**：原22项加5项冲击回归，覆盖实际GE降档与移除、伤害／冲击独立、墙／车／自由球来源、来源封顶和固定档、接触去重／重新允许、球快照与双回调合并、实际脱球／旧旁路退出，以及Authority物理载荷重放约定。它们不是已保存TieredV1资产下的真人联机验收。

- 本机UE `5.8.2-56702186+++UE5+Release-5.8`的Editor／Game Win64 Development最终均Succeeded。未安装或验证5.8.3，未修改引擎／uproject插件配置。
- 只读Unreal采样确认玩家／被动车Combat和球Damage仍为AttackSnapshotV1；四份新增Impact/CollisionImpacts版本均为Legacy，原公共响应仍为250/600/900水平、0/500/750向上等已保存配置。整个施工前后320个uasset／umap哈希全部不变。
- 隐藏Listen Server／Client使用保存的NetTest，主机监听7791、Join succeeded；客户端收到两辆车HP100，主机Ready／CombatReady／ImpactReady均1，CombatVersion1、ImpactVersion0。无本次runtime Error／Ensure；两个专用进程核对PID／命令行后清理。**这是Legacy冲击的启动回归，不证明TieredV1冲击动作已通过真实网络验收。**
- 最终报告位于`Saved/Phase10D/Automation_Final/index.json`，构建／采样／网络日志同目录；Saved不随Git提交。共享JSON保存87份源码／配置哈希、资产清单和证据哈希，交接时带上源码与文档，后续再正常提交团队保存的DA。本轮未提交／Push。

首轮为24通过／3失败：旧车辆退出夹具在规则缓存未初始化时发射被拒绝，修正为发射先验证初始化；新Pawn夹具没有简单碰撞网格，改用引擎Cube并明确登记该夹具无Convex的预期警告；能量预算浮点断言改为0.001cm/s容差。首轮失败报告保留。编译中修正了复制宏要求的OutLifetimeProps参数名、Blueprint不支持uint32只读暴露、测试API拼写，以及Game构建中Editor专属夹具缺少WITH_EDITOR条件；不修改引擎、工具链或资产来绕过失败。

配套[MVPB_Phase10D_Verification_20261015.json](MVPB_Phase10D_Verification_20261015.json)记录最终构建、只读采样、资产保护及双进程结果，不覆盖10-A／B／C历史证据。资产由团队迁移，本轮代码代理不手工修改或请求Compile／Save任何蓝图或DA。

当前需要团队继续验证的边界：新的碰撞标尺和最低脱球档位需实测校准；墙角／旋转多点接触和网络动作重放的流畅性需真人双端验收；在5.8.2通过不等于已经在5.8.3复测。任何尚未执行的验证都不写为通过。

## 7. 团队现象排查：冲击免疫后很快捕获命中球（2026-10-15）

当前main/79a73f6，团队保存的四份DA仍有未提交改动，代理保持不动。用户报告冲击免疫车辆会捕获打来的球；本轮仅读码和既有双端日志，不修改玩法／配置，不启动Unreal或重跑测试。

`Saved/Logs/MVPB_GAS_Host.log`中07:51:40.516（日志UTC），攻击C89BCD124C9BC524547DCC9147349F7E命中免疫车辆：Raw／Resolved／Actual均25，SourceTier=2、EffectiveTier=0、ImpactImmune=1、Drop=0。同一时间球Launched→Free；07:51:40.750 Free→Controlled，由被命中车辆取得，间隔0.234秒。日志07:51:37的两辆玩家车均Ready／CombatReady／ImpactReady=1、CombatVersion／ImpactVersion=1。这是当前V1运行与一个免疫命中子项的证据，不代表完整10-D验收或四份DA全部字段已采样。

本次读取的Host日志SHA256：`356D672BDBEB68A1DF1324ACB04665422A96B33BA89AF5B355EC824BC05BAA55`；Saved日志可能被下次启动更新，以上时间／事件／结论留在本文用于交接，不将后来的同名文件自动视为同一份证据。

原因：`OnPhysicsRootHit`在命中被接受后消费普通球攻击，调用`FinishLaunchAsFree`；免疫只将规则冲击降为None，不取消伤害、消费或转Free。`CanBeControlledBy`只允许Free且拾取锁已结束的球，未将冲击免疫作为捕获资格。未持球车辆满足前方180°、距离及视线条件后会自动捕获。免疫车更容易留在球旁是物理位置上的解释；普通车也可能捕获，日志07:52:01同样出现未免疫目标转Free约0.214秒后取得球。

现有可调参数：`/Game/Data/MVP_B/DA_Ball_Normal_TestBaseline_v1 → Gameplay → Acquisition → Post Launch Pickup Lock Duration`，单位秒。源码默认及旧留档为0.2秒；本轮未重新导出团队最新DA数值。停止游戏，在Editor中手工调大（例如先用1.0秒观察）、Save并重新启动双方进程，可延后球结束发射／命中回Free后的拾取。此锁作用于**所有车辆**，也影响其他结束发射后的拾取时机；即使延长，球仍在范围内时锁结束后依然可以捕获。车辆`Drop Reacquire Lock Duration`用于掉球后的原持有者重捕，不处理这里的被命中者。

如后续设计要求“受击车短时间不能收回这颗球，其他人仍可争抢”，需要独立、可配置的命中目标拾取锁；目前没有该参数。它应与冲击免疫解耦，对需要该规则的命中统一应用。**用户随后明确要求暂时只留档，不施工该设计**，不将此项作为本轮新增任务，也不改变普通球命中后消费的10-C规则。V1冲击诊断应读HAL Combat的EffectiveTier，旧Ball hit日志中的impact speed／legacy tier不是免疫后的实际档位。

## 8. Heavy冲击变弱排查（2026-10-15；修复前诊断）

用户报告把球Impact调为Heavy后仍明显弱于10-D前。本轮main/79a73f6，只分析、只读采样和留档；未编辑C++／资产、未编译或开展新的动态物理测试。共享诊断与当前DA采样见[MVPB_Phase10D_HeavyImpact_Diagnosis_20261015.json](MVPB_Phase10D_HeavyImpact_Diagnosis_20261015.json)。

### 8.1 已确认的事实

- 最新Host日志的三次球命中（07:59:45.651、07:59:55.800、08:00:23.300 UTC）均SourceTier=3、EffectiveTier=3、ImpactImmune=0，ActualHPRemoved=25；目标没有把Heavy降为其他档。
- 用现有只读Python工具加载四份保存DA。车辆Impact、球Gameplay.Impact及公共CollisionImpacts均TieredV1，球为Heavy；公共Heavy仍水平900cm/s、最低向上750cm/s、1.25圈翻转、最大角速度8rad/s，与施工前保存表相同。车辆Physics／Movement／BallControl与10-D开工采样字符串一致；球当前全局拾取锁已由团队保存为1.0秒。不是代理自动覆写调参。采样前后320个资产哈希不变。
- Host 07:59:21.653、Client 07:59:31.191均有`Ensure condition failed: IsInGameThreadContext()`，项目调用栈指向`UArcadeVehicleMovementComponent::ApplyAction_Internal`第320行。该警告在第一次自然接触动作时已经发生，不能把它仅归因于后面的球Heavy参数。

### 8.2 确定的实现错误与物理语义变化

**确定的程序错误**：10-D的Physics Action在物理线程执行，却调用`FPhysicsInterface::GetLinearVelocity_AssumesLocked`、`GetAngularVelocity_AssumesLocked`和`SetAngularVelocity_AssumesLocked`。本机UE5.8.2源码`PhysicsCore/Private/ChaosEngineInterface.cpp`明确经由`GetGameThreadAPI()`读V／W和写W，而不是当前物理帧的Internal句柄；前两端Ensure正好命中该路径。仅`AddVelocity_AssumesLocked(..., true)`已选Internal，整个动作因而混用了两个线程侧状态。错误可导致向上速度补足读旧状态、翻转写到外部同步状态，不能保证当帧响应和重放一致；具体对本次可见位移削弱的占比尚无前后速度／角速度数据，不能伪造量化结论。

**确定的算法变化**：旧球命中是在自然碰撞之外完整追加该档`HorizontalDeltaSpeed`；新版ExplicitTier也先扣除`dot(NormalImpulse,Direction)/TargetMass`，只追加剩余量。示例自然响应100、Heavy表900时，旧额外900而新额外800。因此即便DA表完全相同，水平表现也可能减弱；缺少本次Natural／Supplement日志，尚不能认定这解释全部差异。普通碰撞另有MaximumRuleRestitution能量预算；本次球走ExplicitTier，**没有使用普通接触恢复系数上限**，不应靠调整该上限增强主动球。

日志`Impact=Applied`目前仅表示游戏线程将动作成功排队；不能证明物理线程已正确执行。旧Ball hit日志的`impact 0.0`是新版未填写旧物理强度字段，不代表实际冲击为零；以有效档位和实际物理执行诊断为准。

### 8.3 后续修复与复验建议

1. 优先修复Physics Action的V／W读取和姿态写入，全部使用同一物理帧Internal句柄／明确支持Internal的接口，保留Authority动作身份和重模拟，不在客户端重新结算GAS或脱球。不改已保存的Heavy表来掩盖执行错误。
2. 在相同场景、质量和三档表下比较旧响应与修复后的ExplicitTier，补可关闭的EventSequence／物理应用帧／前后V与W诊断。修线程错误与改变主动攻击补足语义分开记录；如要恢复旧的完整追加手感，应明确调整主动攻击策略，同时保留普通碰撞的重复／能量约束和统一档位抗性／脱球。
3. 新测试必须实际进入Network Physics `ApplyAction_Internal`，在服务器及拥有客户端核对Internal V／W、同帧动作／重放、Yaw保留、Light／Medium／Heavy及免疫、无Ensure与最终位置收敛。既有27项覆盖单机实际结算、数据数学和载荷约定，**没有执行真实联机Physics Action线程路径**，因此此前通过未覆盖这个缺陷。随后真人双向命中及扰动复测，才能判定体感恢复程度。

本轮结论为“已定位确定的线程API错误及施力语义差异；待修复／动态验证”，不能当作正常Heavy强度已验收，也不在用户询问原因时直接施工。

## 9. 冲击Physics Action线程修复（2026-10-15）

用户随后授权修复并询问是否需要确认。明确程序错误无需另定玩法规则；本次范围为Internal物理帧读写、真实Solver回归和可关闭的执行诊断，**保留全部现有DA、档位／抗性／脱球规则及水平补足算法**。恢复旧“完整追加”仍需单独决定，独立命中目标禁捕继续只留档。原第8节及诊断JSON作为修复前记录保留，后续状态以本节和新验证摘要为准。

### 9.1 代码与测试

- `ArcadeVehicleMovementComponent.ApplyAction_Internal`从`FBodyInstanceAsyncPhysicsTickHandle`读取当前Internal V／W；线性与角速度变化都通过明确`bIsInternal=true`的Impulse接口交给同一刚体。移除对GameThreadAPI的速度读取／姿态写入。保留当前Yaw，轻档不清除自然旋转／下落，向上补足依据实际物理帧速度。
- 新增`hal.ImpactPhysicsLog`（默认0，非Shipping）。输出`Vehicle impact physics:`、Event、Solver应用Frame、Internal BeforeV／BeforeW、QueuedDV／QueuedDW；表示动作已在物理回调消费。这里记录的是待积分变化量，不是接触求解后的最终速度，不能用它冒充最终位移测量。
- 新增`HAL.FutureCreation.Impact.SolverThreadExecutionAndReplay`：独立Chaos世界、TaskGraph Solver真实回调，在PreSim中让PT V／W明确不同于GT快照，执行生产Handler，PostSolve测量积分结果。覆盖Light、Medium、Heavy、同载荷恢复初态后重放、已有更快上升、拒绝载荷与非模拟代理不施力；确认包含实际worker线程执行。不是在GT伪造物理上下文，也不声称直接恢复了真实网络rewind历史。
- 旧实现的红测重现同一Ensure，并测出Medium期望500却得到600、Heavy翻转未进入Internal W、已有900向上速度被错误加到1650等；修复后对应值通过。测试夹具改为在PreSim记录线程身份，因为TaskGraph可把部分Solver工作放到GT处理，要求实际worker覆盖但不要求每个PostSolve都在独立OS线程。
- 首次完整回归18通过／10失败，原因是团队公共DA现已TieredV1，而早期Legacy球／配置夹具仍读取项目全局配置。新增仅测试用`FScopedHALLegacyCombatTestRules`，两个旧夹具显式使用内存Legacy规则并在析构恢复原Source／缓存；不改资产或生产迁移门槛。新版Impact夹具继续使用各自TieredV1规则。

### 9.2 验证与团队复测入口

最终Editor／Game Development Target均构建成功；Editor自动化 **28/28通过、0警告／0失败**，包含原27项及新增实际Solver测试，无线程Ensure。V1保存资产的隐藏双进程监听7793、Join succeeded，客户端收到两辆车HP100；主机自然落地接触触发4次冲击动作，并记录Internal回调消费，无线程Ensure。该smoke没有主动发射球，客户端日志没有受击动作记录，不声称已验证客户端Heavy动作或真人手感。启动仍有引擎Widget工厂等警告，不把整个运行日志写成零警告。

本次修复／验证前后320份`.uasset`／`.umap`哈希全部一致；四份团队DA未被修改。两个专用隐藏进程在核对PID、引擎路径和本次日志参数后清理。详细构建、回归、smoke、资产与源码哈希见[MVPB_Phase10D_ThreadFix_Verification_20261015.json](MVPB_Phase10D_ThreadFix_Verification_20261015.json)；与原10-D和诊断摘要分开，不覆盖历史失败。

团队继续用[双端启动命令](../Configuration/MVPB_DebugHUD_And_GAS_Test_CN_v0.1.md)运行（已追加新物理日志开关），双方加入并由主机Start后，保持当前Heavy和原公共响应表，先互相单次发射：

1. 核对`SourceTier=3、EffectiveTier=3、ImpactImmune=0`，实际击飞／翻转是否恢复，两端最终收敛。查看`Vehicle impact physics:`已消费及相关物理Frame，不只看游戏线程`Impact=Applied`。
2. 复测Medium／Light、降档／免疫、自然撞墙／车车、脱球／锁定、发射后坐和连续接触；确认两端无`IsInGameThreadContext`或`IsInPhysicsThreadContext`的Ensure。
3. 再做既定100ms实测RTT／1%丢包／20ms抖动。主观是否恢复10-D前手感由相同参数和场景的真人对照决定；线程修复通过不等于已恢复“完整追加”语义，不能把剩余水平差异当成这个修复自动消除了。

截至本节交付时，本机引擎仍5.8.2，5.8.3和四台真实有线PC的原验收边界继续保留；尚未取得团队真人复测反馈。后续反馈与进度见下一节，不回写本节历史验证。

## 10. 团队击飞回归与10-E接续（2026-10-16）

用户明确反馈「击飞效果回到了之前的效果」，并决定先保留当前档位划分，各档实际击飞效果以后再调整。将**击飞效果恢复**登记为团队功能回归通过；本次没有取得新日志、测试角色／地图明细或RTT／丢包／抖动条件，不能由此推断第5节整张矩阵均已通过。当前无需为恢复旧版完整追加而改施力策略；现有水平补足和全部DA原样保留，后续调参另记。

接续核对：`main / 663e1b0a2002afbde9edf72588a3c479757b6fdf`，本轮开始工作区干净。上一验证摘要中10份相关源码／配置和四份团队DA的SHA-256与当前文件全部一致；线程修复、回归测试和DA迁移已提交，不再是未提交交付。另有`f4b17d2`增加DrivingLab资产并修改弹簧拳／场地资产，属于团队既有提交，本次不改也不据此宣称这些内容已验收。本机安装仍UE5.8.2；本次只读核对并更新文档，没有新编译、自动化、Unreal运行或资产编辑，28/28为上一轮证据。

### 10.1 10-D还有什么需要做

- **没有发现阻塞10-E的已知程序缺陷，也没有需要现在追加的必做功能。** 档位、抗性／免疫、有效档位脱球、来源适配和权威物理动作均已有实现；力度调参按用户决定后置。
- 仍须补齐团队回归：Light／Medium／Heavy与降档／免疫／脱球独立性；正撞／侧撞／追尾／墙角及连续接触不重复注能；主机／客户端互换受击以及既定100ms实测RTT／1%丢包／20ms抖动下的动作收敛。可随10-E和10-F集成回归补齐，但不因此降低10-D验收门槛或提前写成通过；新缺陷应回到10-D修复。
- 独立命中目标禁捕继续只留档；正式淘汰／死亡、弹簧拳接战斗入口、完整比赛框架均不加入10-D收尾。

### 10.2 下一步建议与待定范围

10-E按10-A规格交付**一个会沿用的主动GA和一个持续GE**，验证服务器激活许可、冷却、持续时间、叠加／刷新／移除、重复请求及Avatar失效／退出清理，不使用逐帧Tick Ability。

推荐首例为「短时冲击抗性」：GA负责发动许可与冷却，持续GE临时增加`TierReduction`，仍由现有Combat Resolver／Impact Receiver结算有效档位。这样可以直接验证10-D扩展入口，10-F再处理加速／低抓地进入网络物理历史。本建议**尚未被用户选定，不是已实现技能**。

10-E编码前仍需定稿首个技能内容、降档数、持续时间、冷却起算、同类效果叠加／刷新、驱散及Avatar失效时的效果／冷却保留规则；数值应可配置。此轮只确认接续条件，不直接开写技能或把调试命令当作正式Buff。

同日后续用户选定**8秒冲击免疫**（不是降档抗性），20秒冷却从发动时开始、同类GE刷新时长、退出清理，并要求未来其他GE仍可叠层。以上待定项以[10-E独立规格](MVPB_Phase10E_Ability_Effect_Plan_CN_v0.1.md)为准；随后用户授权实际施工，GA／GE已实现并完成编译、自动化与双端请求／复制检查，团队实际对战验收仍待完成，见[10-E实施记录](MVPB_Phase10E_Implementation_CN_v0.1.md)。独立补球调试命令不充当技能验证。
