# MVP-B 10-F-1：运动效果施工规格与基线（v0.1）

日期：2026-10-17（Asia/Tokyo）。接续基点`main / b8feb7b88e9cf871828a154696b0c82688c0a914`，实际UE`5.8.3-58210709+++UE5+Release-5.8`。用户已确认[计划第6.1节](MVPB_Phase10F_Movement_Effects_Plan_CN_v0.1.md#61-首例玩法规则2026-10-17用户已确认)并授权开始当前小步。本文定稿10-F-1的技术契约；**下述新类型／接口均是10-F-2／3的施工目标，当前没有生产运动桥接或加速GE，不得把机制探针当成联机运动效果验收。**

当前保存配置／引用／329资产哈希见[本轮基线](MVPB_Phase10F1_Baseline_20261017.json)，构建与探针证据见[验证摘要](MVPB_Phase10F1_Verification_20261017.json)。旧10-A／10-E快照保持，不覆盖历史。

后续实施入口：2026-10-17已完成[10-F-2生产桥接与真实回滚验证](MVPB_Phase10F2_Implementation_CN_v0.1.md)，原始规格／F1证据作为历史保持。F2补充发现通用Async Physics Tick在UE5.8回滚时被跳过，实际施工新增只在回滚时执行原驾驶的Chaos回调；具体差异与证据见F2第3节。正式5秒GE仍待F3。

## 1. 已确认玩法与最小数值模型

| 项目 | 施工规则 |
|---|---|
| 首例 | 持续5游戏秒，加速度×1.5、驾驶速度上限×1.25；不是正式平衡值 |
| 作用范围 | 前进／倒车驱动加速度各乘同一加速度倍率；前进／倒车驾驶上限各乘同一速度倍率 |
| 其他参数 | 刹车减速度、ReverseEngageSpeed、滑行／超速修正率、抓地、转向／脱困、质量／惯性／重心及碰撞体保持保存值 |
| 转向参照 | `ApplySteering`继续使用基础MaxForwardSpeed；不得写回该字段。实际车速变化仍按原公式影响转向 |
| 不同GE | 加速度和上限分别`Clamp(1 + Sum(活动增量), 0, 3)`；+0.5和-0.3得到1.2，不连乘 |
| 0／非法值 | 0不冻结刚体，不清零速度，不免伤／免冲击；非有限贡献拒绝进入快照，不污染最后合法状态 |
| 同类多来源 | 首例定时GE按目标聚合、上限1，跨来源合法重施均从本次起再计5秒，不增加强度／累计时长；其他GE贡献独立 |
| 到期／驱散 | 恢复有效参数，保留线速度／角速度；接地沿用原阻尼／超速修正，腾空不额外制动 |
| 生命周期 | 首例GE有`Effect.Scope.Avatar`；换车／失去Avatar清理，保留HP和其他冷却；Owner退出沿用ASC清理 |

增加一个最小`UHALMovementAttributeSet`，仅有`AccelerationMultiplierDelta`与`SpeedLimitMultiplierDelta`，Owner初值均0、按已有Attribute方式复制。GE用Additive改变增量；**钳制的是最终有效倍率，不是聚合器中的增量**，例如总增量暂为-2时移除一个GE后仍须正确重算。有限负增量允许保留，非有限输入拒绝／记录。基值1和0～3测试保护范围属于运动修饰规则，不覆盖任何DA基础值。

PlayerState注册该AttributeSet，初始化／换Avatar不得重置Owner上尚合法的其他GE。被动车本轮不新增驾驶能力；没有Movement的目标可以保有独立战斗／冲击规则。首例无需新GA、不接DefaultAbilities、不借用免疫冷却或替换Primary输入。

## 2. 现有配置基线与消费路径

以下来自本轮只读导出，不是C++默认值：

| 玩家字段 | 当前保存值 | 测试GE生效后的计算目标 |
|---|---:|---:|
| ForwardAcceleration（cm/s²） | 1800 | 2700 |
| ReverseAcceleration（cm/s²） | 1200 | 1800 |
| MaxForwardSpeed（cm/s） | 2400 | 3000 |
| MaxReverseSpeed（cm/s） | 900 | 1125 |
| BrakeDeceleration（cm/s²） | 2800 | 2800 |
| CoastingDragRate／OverspeedCorrectionRate（s⁻¹） | 0.35／3 | 原值 |
| LateralGripRate／HandbrakeGripRate（s⁻¹） | 12／0.2 | 原值 |
| HighSpeedSteeringStart／Scale | 1600／0.8 | 原值，速度标尺继续2400 |

玩家Combat为AttackSnapshotV1、Attack25／OutDelta0／Taken1；Impact为TieredV1、CollisionCap Heavy、MinimumDropTier Medium、TierReduction0。球Damage为V1、动作倍率1，Impact为Heavy，PostLaunchPickupLockDuration为1秒。不能恢复旧文档中的100000脱球阈值作为V1入口。

Test绑定原生HALPlayerState、NetTest绑定团队BP_HALPlayerState，二者都引用BP_RoundedVehiclePawn及当前玩家DA；放置球／被动车也指向对应保存DA。NetTest的PlayerState默认授予团队GA_ImpactImmunity，引用8秒免疫GE和20秒冷却GE。本轮补充导出了实际类引用／时长，仍不是本机重做该技能的双端验收。

`ApplyDefinition(false)`在PreInitializeComponents把DA送入Movement.ApplyConfiguration，后者仅初始化可用。10-F不得在GE开始／结束重新调用它：保持基础字段，`ApplyLongitudinalForces`只在实际消费时计算四个有效值，超速修正比较有效驾驶上限。`ApplySteering`继续读取原基础字段；Grip、WallEscape等不乘倍率。

球发射初速度原本包含实际车速，后坐也按实际车速计算，效果可以自然改变这些测量值；不再对球发射增量、后坐DeltaV、伤害或冲击档位额外乘运动倍率。

## 3. 引擎事实与选用路线

以本机`Engine/Source/Runtime/Engine/Public/Physics/NetworkPhysicsComponent.h`及`Private/PhysicsEngine/NetworkPhysicsComponent.cpp`核对，下列约束决定实现：

1. `GetUpcomingServerFrame_External`是GT的即将入队帧估计；实际动作在PT注册时可能顺延。不能把GT读到的LastPhysicsFrame当作已确认生效帧。
2. `OnPreProcessInputs_Internal`先恢复收到的服务器State／Input，随后`ApplyActionsForFrame`执行该精确帧的动作。Movement的AsyncPhysicsTick在后续物理模拟阶段消费。
3. `np2.Resim.RecordStatePostSolve`本机为1；OnPostSolve在求解帧F后采集State，标记为**F+1帧开始前的锚点**。不能在该State恢复时再次执行F帧变化。
4. 状态只在重模拟或允许的模拟代理运行路径恢复，且要求`bReceivedData`。最新Attribute复制、只改ApplyHistoryState或只存“当前值”均不构成完整时间线。
5. `TNetRewindHistory.ReceiveNewData`按TickOffset把ServerFrame映射到LocalFrame、与预测State比较后返回回滚建议。没有匹配预测帧时可以不返回回滚；已有收到的同帧记录不会被冲突重复覆盖，不能以重复发同帧来纠正它。
6. `FActionHistory`支持精确帧、可靠发送和回滚重放。**迟到动作不会自动挪到当前帧，动作到达本身也不能当作已触发重模拟。** 已消费动作在回滚时可重用，不能用全局LastAppliedSequence挡掉合法重放。
7. 当前网络DA显式启用CompareState/InputToTriggerRewind；全局CompareState CVar虽为0，不能据此声称该车没有比较。DA的EnableReliableFlow为False，但引擎`AsyncPhysicsTickComponent`中的Reliable Actions分支明确独立于该开关，允许可靠动作，不需改DA开实验性状态可靠流。

选用**可靠Authority参数动作＋带有界时间线的权威State检查点**，复用当前Network Physics，不增加可信客户端Buff输入、不建立另一套车辆物理复制。动作提供明确边界与及时传输，持续State提供晚加入／丢失事件／窗口恢复；两者均携带绝对有效数值，重复应用不累乘。

## 4. 新数据与线程交接契约（待10-F-2实现）

| 数据 | 最少内容与职责 |
|---|---|
| GT待发布值 | 两个最终倍率、AvatarEpoch、Revision；只从服务器ASC采样，不携带UObject到PT |
| 参数变更动作 | Authority作者、Epoch／Revision、绝对AccelScale／SpeedScale；实际生效ServerFrame以PT执行帧为准，SourceId沿用Owner语义0 |
| 物理帧快照 | Epoch、Revision、EffectiveServerFrame、两个倍率及Ready标记；同一字段用于驱动、BuildState和诊断 |
| 权威State扩展 | 格式版本1、当前快照、CoverageStartServerFrame、覆盖起点的参数锚点、之后的有界变更列表；包含无Buff／结束后的恢复事件 |
| 本地帧历史 | 每次真正使用的倍率／身份／Revision／帧，区分预测与已确认；回滚不读取当前ASC |

GT订阅两个Attribute变化与Avatar绑定／清理，初次绑定主动采样。一次GE可能先后通知两个Attribute，回调只标记Dirty；在该World的`OnWorldPostActorTick`统一刷新一次，读取最终配对值，数值未变不发布新参数Revision。同类GE只延长时间而倍率未变时，不制造虚假运动边界；实际到期再发布恢复值。

5秒由GAS的游戏世界时间计时，运动系统不另建倒计时，也不在开始时预定“300个物理帧后必定结束”。刷新／驱散／自然到期引起的真实Attribute变化，经同一GT刷新和下一安全物理边界进入时间线；日志区分GE游戏时间、GT采样与实际物理生效帧，测试允许正常采样量化延迟，不能用墙钟或预先移除GE冒充自然到期。

GT把最终纯值作为`EnqueueImmediateAction_External(...,0,true)`交给既有历史组件；不预填猜测的ServerFrame。多个GT刷新若落入同一物理帧，Revision最大者为该帧最终值，动作逐条可见但只保留一个最终帧事件。PT Handler只复制值／记帧，不访问ASC、GE句柄、Tag或Actor来源，也不施加一次性加速冲量。

Epoch由服务器每次有效Avatar绑定生成并在该Pawn可靠身份状态中复制；相同Avatar重复初始化不重开Epoch。旧Epoch的数据不得影响新Avatar。Revision从1开始，0为未知；排序限定同Epoch，溢出前换Epoch，不把uint32截断顺序当永久全序。

Standalone复用相同Dirty采样和纯值交接，在下一安全AsyncPhysicsTick边界消费，不需要虚构网络时间。模拟代理仍走当前PredictiveInterpolation，不新增驾驶施力；其最新Attribute／快照用于显示，不能让远端车重复加速。

## 5. 每帧、回滚与迟到处理

定义S(F)为F帧开始、尚未执行F帧参数动作前的状态。F帧采用顺序：**恢复S(F) → 按Revision处理F帧权威变更 → 锁存该帧最终倍率 → 驾驶／碰撞 → 记录S(F+1)**。回滚重新走同一顺序，只重放参数与物理，不重施GE／推进GAS时钟／伤害／脱球。

State的参数时间线是分段常量，绝不Lerp倍率或在两个端点中按0.5选版本。序列化和InterpolateData必须保留覆盖锚点及离散变更；查询某帧时选该帧有效的最后事件。对于端点间没有可信覆盖的帧，明确Unknown，不用最新倍率反填过去。

服务器时间线至少覆盖引擎当前State／Chaos回滚窗口并保留窗口之前的一个有效参数锚点；每帧合并后最多一条变更。容量按实际历史帧数+2建立，首轮硬保护上限256条，超过254历史帧的配置明确报不支持并禁用该桥接，不能静默截断仍可回滚的变更。本机约60Hz、MaxSupportedLatencyPrediction=1000ms；实际容量须在Solver建立历史后记录，不能仅靠这两个保存值推定。

序列化先验证格式版本和数量上限，再分配有界列表；拒绝非有限／超范围倍率、无效身份、倒序／重复冲突边界和越出覆盖范围的帧。客户端输入及其插值／合并／Decay均不携带权威参数。预留格式版本不表示允许旧接收端猜测缺字段；双方须使用同一构建。

拥有客户端在既有AsyncPhysicsTick中，从缓存的NetworkPhysics组件取得`GetStateHistory_Internal()`，有界扫描精确且`bReceivedData`的State，合并此前未见的Epoch／Revision／覆盖区间；不能在GT读History_External冒充物理真实数据，也不使用非精确Extract的插值记录作权威锚点。权威时间线归PT所有；收到较旧区间时允许补齐旧帧事实，但不能改写更晚已确认边界。

| 情形 | 规定处理 |
|---|---|
| 已知未来／当前帧动作 | 精确帧应用绝对值；同Epoch同Revision重复幂等，同Revision异值拒绝并记日志 |
| 窗口内迟到动作／检查点 | 比较历史实际倍率与新权威覆盖；对受影响帧调用RewindData.RequestResimulation。先检查IsFrameWithinRewindHistory和对应输入／状态／参数覆盖；在下一正常Solver周期处理，禁止在Handler递归推进Solver |
| 引擎自动State回滚 | CompareData包含Epoch／Revision／倍率及已知覆盖内的参数语义差异；纯窗口平移或完全等值的覆盖补充不独自触发回滚。即使当前倍率又回1，也能发现错过的开始／结束。不假设引擎建议一定是最早差异帧，自己的覆盖比较负责更早的有效回滚请求 |
| 回滚到Buff有效旧帧 | 以权威时间线或该帧已存快照恢复，再重放后续动作；不能因当前GE已到期而全部用1，也不能被未来Revision高水位挡掉旧帧合法值 |
| 超出窗口／缺锚点 | 不把旧动作移到现在。进入Recovery，等待持续State中的新检查点；只有自定义历史、输入和Chaos均有合法锚点时才请求恢复。实际回滚帧不同于请求帧时必须再次检查覆盖，不能盲用FindValidResimFrame的钳制结果 |
| 初次复制／TickOffset未就绪 | 当前帧可保留标记为预测的本地基准／最后可信值；权威帧对齐前不宣称确认、不补造过去。就绪后从新检查点校正 |
| Recovery期间历史重放缺值 | 该帧不追加主动驱动力，保留Chaos碰撞／已有速度并记录缺帧；等待有效检查点恢复，不能把最新Buff套到未知旧帧。正常前向驾驶继续用最后可信值，不能用客户端未知值修改服务端 |
| 结束／更换Epoch | 移除订阅、丢弃旧Epoch待发与恢复请求；新绑定重新采样。保留PlayerState的HP／其他冷却 |

连续State每次包含完整当前锚点和窗口内变更，因此丢失一次开始／结束动作不会永久失配，不依赖实验性状态可靠流或每次重发旧同帧记录。Recovery用后续正常复制检查点恢复，无新增客户端任意“设置倍率／生效帧”RPC；若F2实测未能恢复，须修复该恢复路径，不能降为复制最新属性后宣称完成。

## 6. 实施文件、诊断与出口

10-F-2新增最小Movement AttributeSet／纯值类型，在PlayerState注册；Pawn组装／绑定窄桥接，复用ASC／Avatar清理；Movement和VehicleNetPhysicsData实现动作、时间线／比较／序列化与消费。桥接可独立为窄组件，但不建通用Buff管理器，Movement不识别具体GE名字。现有输入Cmd不增加倍率，原发射／冲击动作语义保持。

10-F-3新增普通UGameplayEffect派生的首例加速GE，Additive贡献为+0.5／+0.25，Avatar作用域、AggregateByTarget一层刷新；不用免疫GE作父类。服务器调试入口复用该GE、校验调用者和比赛阶段，客户端不能提交任意倍率／时长；保留现有免疫主技能。届时提供团队Editor步骤，不直接编辑资产。

新增可关闭诊断`hal.MovementEffectLog`（计划名）：PlayerId、Epoch、Revision、ServerFrame／LocalFrame、来源Authoritative／Predicted／Replay／Recovery、两倍率、覆盖范围与回滚请求／采用帧。HALGASStatus／HUD显示有效倍率及是否Ready。数值序列化先使用有限float，比较容差1e-4；身份／Revision／帧精确比较，不模糊跨边界。

F2出口须有：无效果时原35项保持；倍率计算／叠加移除、分段历史／乱序／容量、真实Solver线程及双端先后获得／到期、强制迟到与历史不足恢复证据。真实PT测试检查内部帧和线程，不仅调用GT函数；双端测试不能由容器探针代替。F3使用自然GAS到期验证5游戏秒和刷新；F4补完整持球／漂移／发射／冲击／免疫／退出、100ms实测RTT／1%丢包／20ms抖动及四台有线PC，保留既有收敛与连续振荡门槛。

## 7. 本轮完成边界

本轮完成具体规则与技术路线、只读保存基线，以及两个直接调用UE真实历史容器的自动化探针。探针用已有WallEscapeBlend作不透明状态标记、已有Authority Impact载荷验证动作容器，**没有执行运动Buff、真实网络重放或修改生产施力**。UE5.8.3完整Editor／Game构建成功，最终全部35/35自动化0警告／0失败，包含原33项及新增2项；只读基线导出0错误／0警告，329资产整个施工／验证前后未变。原始日志和源码／引擎哈希见验证JSON；不把程序编译／探针成功写成10-F-2或10-F-4已通过。

首轮基线导出查询了不存在的p.RewindCaptureNumFrames，产生1条警告；删除该查询并改为记录实际PhysicsSettings后，最终导出0错误／0警告。一次Game构建与UE平台检查启动的UBT争用公共Trace.uba，在编译前中断；改为顺序执行并独立记录最终构建，不掩盖早期失败。既有引擎／Network Physics兼容层弃用警告与MSVC14.51较推荐14.50更新的提示保留，不在此扩展为引擎API迁移。
