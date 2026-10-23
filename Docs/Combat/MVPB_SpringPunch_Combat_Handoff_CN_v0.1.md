# 弹簧拳：战斗与联机接入交接注意事项（v0.1）

## 2026-10-23：独立收拳曲线

用户追加要求收拳也可配置曲线。新增同一Movement分类下的`Retraction Curve`，与`Extension Curve`分开指定Curve Float；横轴仍为该阶段归一化时间0～1，纵轴为**收回完成比例**：0代表仍完全伸出，1代表完全回到原位，不能把纵轴误当成剩余伸出长度。总时间由Retract Duration控制。未指定/空曲线仍线性；范围/端点与非法值回退共用出拳策略。

服务器进入Retracting时捕获并复制本次Retraction Curve引用，客户端按服务器时间计算同一回收进度，不复用出拳曲线。更换BP配置引用在下一阶段生效；修改资产本身仍应停止并重新Play且各PC保持同版资产。收拳仍不攻击，阶段结束严格回原点。

团队手动建立第二个Curve Float（例如CF_SpringPunch_Retract），从(0,0)到(1,1)绘制单调曲线，保存后在BP_SpringPunch Class Defaults → Spring Punch → Movement → Retraction Curve引用，Compile/Save。代理不编辑Curve/BP资产。UE5.8.2 Editor/Game Win64 Development均Succeeded，弹簧拳专项1/1、0测试警告/失败；覆盖独立阶段引用、半程25%回收位置、精确原点、无攻击及未选曲线线性回退。新保存资产的画面/真实双端引用仍待团队，见[收拳曲线摘要](SpringPunch_RetractionCurve_Verification_20261023.json)。

## 2026-10-23：可调出拳Curve Float

新增`BP_SpringPunch → Class Defaults → Spring Punch → Movement → Extension Curve`，引用Unreal Curve Float资产。横轴为归一化时间0～1，纵轴为伸出进度0～1；曲线斜率决定速度，不是直接输入cm/s或动画序列。Extend Duration仍控制总时长，Punch Distance控制总距离。未指定/空曲线保持旧线性伸出；输出限制0～1，起止端点严格0/1，非有限结果回退线性。只改Extending，Retracting维持线性。

团队手工配置：
1. Content Browser右键 → Miscellaneous → Curve，选择CurveFloat，命名例如`CF_SpringPunch_Extend`。
2. 双击曲线添加关键点。例如(时间,伸出比例)：(0,0)、(0.10,0.02)、(0.35,0.85)、(0.60,0.98)、(1,1)，先用Linear连接，再按手感调切线。此例前段蓄势、中段快速打出、尾段缓停；各点是建议，不是最终默认资产。推荐保持单调递增；下降段会实际回退拳位置。
3. 保存Curve，在BP的Extension Curve引用它，Compile/Save后重新Play。示例中一半时间不等于一半距离，勿将纵轴误填速度。
4. Curve随Extending阶段捕获资产引用并复制给客户端，双方按相同服务器进度求值；不要只在客户端视觉上改曲线。各PC需相同已保存资产；改曲线后停止并重开测试。
5. 快速曲线仍保留前后位置Sweep、最终伸到底的补扫及每拳去重；不引入新的Tick或客户端结算。低帧率下非单调往返曲线可能在两次采样间遗漏折返轨迹，应优先用单调曲线；本轮没有增加子步扫描框架。

本轮不自动创建/编辑Curve或蓝图资产，团队现有Imported素材改动保留。用户关闭Editor后，UE5.8.2 Editor/Game Win64 Development均Succeeded，弹簧拳专项1/1、0测试警告/失败，含阶段曲线引用、半程90%位置、端点/范围/空曲线回退及既有战斗回归。没有渲染观感、新保存Curve的真实双端引用或全套/扰动复测；详见[曲线验证摘要](SpringPunch_ExtensionCurve_Verification_20261023.json)。

## 2026-10-20：第二步阶段同步

用户已授权继续本会话施工。机关默认复制，并在服务器PostInitializeComponents纠正旧蓝图保存的非复制标记；不编辑蓝图资产。每次阶段切换原子更新一个RepNotify结构，含阶段、服务器起始游戏时间、本阶段时长、伸出距离、收回原点与拳次编号，ForceNetUpdate；客户端以GameState服务器时间计算伸缩位置，只在伸缩阶段Tick。迟到资料直接恢复当前进度，已过期阶段夹到端点，新的休息阶段恢复原点并停Tick。不逐帧复制拳位置、不用可靠位置RPC、不在客户端检测/补算伤害；原BeginPlay循环与比赛阶段规则保持，未加入Waiting暂停。

源码与测试：SpringPunchGimmick.h/.cpp新增阶段复制；HALImpactTests补迟到快照/端点/原点及不创建攻击测试；新增HALSpringPunchNetworkTests.cpp，仅显式`HAL.NetworkSmoke.SpringPunch`运行。测试在NetTest运行时生成一个原生拳头和一个现有BP_SpringPunch，模拟旧非复制默认值，验证主机及客户端均收到两个拳头Extending/Retracting/Cooldown、第一拳号、两台玩家HP100→75、Medium档位，且客户端无活动Action。没有修改保存地图或机关资产。

最终正常时间双进程（t.MaxFPS 60，端口7799）Host/Client各1/1、0测试警告/失败；专项1/1通过。全套普通回归49/50，唯一失败仍是Car_01已接线后旧OptionalRigPreservesGameplay假设“未选Rig”，与本轮同步无关。本轮无画面渲染/人工体验、新网络扰动/四机或真实晚加入测试；晚到快照由普通测试模拟。最初原生双端通过后，严格混合测试在加速游戏时间下超时；保留失败记录，改为真实60FPS后相同阶段断言通过，没有减弱断言。首轮构建复制宏参数名错误已修正。

团队下一步：将已有BP_SpringPunch手动放进MVPB_NetTest（如果尚未放置），用双人Listen Server正常Play，两台车轮流受击，核对拳的时序/击退/持球脱球及免疫。原屏幕扣血Debug仍仅显示单人/主机，不因本轮阶段复制扩展到客户端。完整扰动/四机继续后置。详细本机证据见[阶段同步摘要](SpringPunch_Replication_Verification_20261020.json)。

## 2026-10-20：第一步战斗接入

同日屏幕调试补充：SpringPunch蓝图Class Defaults → Spring Punch → Debug新增`Debug Damage On Screen`（默认关闭），实际接受命中且ActualHPRemoved>0时显示黄色文字6秒：拳头/车辆名称、HP前后值/实际扣血、抵抗后的Impact档位0～3及Applied是否成功。与原Debug Punch Hits范围/日志开关独立，免疫伤害、HP0或重复拒绝不刷扣血文字。仅非Shipping、Standalone/Listen主机本地屏幕；不额外复制到远端客户端，不代表阶段表现同步已实现。代理未修改蓝图，开关由团队手动勾选并保存。

用户现已授权本会话实施第一步统一战斗结算；下方10月19日的原型说明与分工是历史交接基点。此次保持原BeginPlay循环、仅Extending攻击、车辆目标及原伸缩时长；联机阶段表现复制仍未实现，不把第一步称为完整机关交付。

- 暂用可编辑测试默认值：`Spring Punch > Combat > Base Damage = 25`（0～10000）、`Impact Tier = Medium`（None/Light/Medium/Heavy）。用户未指定最终平衡数值，此处不是最终玩法定案。
- 固定沿SceneRoot局部+X出拳方向推动车辆，抬升/翻转沿用公共档位响应。机关使用`Combat.Source.SpringPunch`和SourceFixedDamage，不借玩家攻击力，不新增ASC/GA。
- 每次进入Extending创建一个Action并快照伤害/档位；每个目标最多接受一次，拒绝不提前消耗。同拳多目标各自结算；0伤害、免疫与HP0沿用共用独立分支。
- 所有命中经CombatResolver；只对合法接受的目标添加本拳缓存。阶段转换与EndPlay关闭Action；保留伸到终点的最后一段Sweep。客户端显式不启动权威阶段、不检测、不结算。
- 公共Combat配置必须为TieredV1；非法伤害/档位或未迁移规则不开启攻击，Debug Punch Hits可查看原因。目标仍需现有有效GAS和Impact配置；没有恢复旧炸弹的预放玩家车临时初始化。
- 团队操作：编译后手动打开既有SpringPunch蓝图的Class Defaults，检查上述Combat参数和Debug Punch Hits；保存后用具备GAS的玩家/被动靶车测试。未自动编辑蓝图、地图或DA。

本轮验证：UE5.8.2 Editor/Game Win64 Development均成功（Editor首次被占用，用户关闭后重链成功）。普通自动化49/50，新增SpringPunch.CombatLifecycle通过；唯一失败为旧Presentation.OptionalRigPreservesGameplay对“未选骨架Rig”的断言，与团队已接入Car_01的资产不符，本轮未改表现测试/资产。没有真人或Listen Server双端受击验收；不得把此结果视作完整机关联机交付。共享证据见[验证摘要](SpringPunch_Combat_Verification_20261020.json)。

日期：2026-10-19（Asia/Tokyo）。检查基点`main / c825d8d2516c56da3b71029562344ea68f975101`，本机UE`5.8.3-58210709+++UE5+Release-5.8`。用户指定本部分交由另一位成员开发；本文提供接口与约束，不代表已完成接入或已确认新伤害数值。本会话不修改机关源码／资产，不代替负责成员施工。

先完整读取根目录[AGENTS.md](../../AGENTS.md)与[项目状态](../Project_Status_CN.md)，核对接收设备Git改动、引擎补丁、已保存资产。10-F-4整套集成／扰动／四机测试已由用户后置至MVP-B收尾；本模块仍需编译、针对性自动化和基本双端检查。原45项／F3双端报告是历史底座证据，不能冒充新机关已通过。

## 1. 当前原型与建议边界

`Source/HAL_Future_Creation/SpringPunchGimmick.*`已有Idle／Windup／Extending／Retracting／Cooldown、服务器Sweep检测及每拳目标集合；目前检测到车辆后仅记录日志。原生类没有完整阶段／起点复制，客户端不能正确复现拳的时序。

保留现有动作与配置，补服务器战斗适配、必要的阶段同步及验证即可。机关默认不拥有ASC，也不必为了造成一次伤害新增GA；玩家ASC仍在PlayerState、车辆为Avatar。不要另建通用机关框架或修改车辆／普通球／加速技能的已调DA与协议。

## 2. 战斗只能接统一入口

建议路径：

```text
服务器进入本拳有效攻击阶段
 → 创建机关自身的来源快照并Begin一次FHALCombatAction
 → Sweep得到候选目标
 → 填写FHALCombatHitRequest
 → FCombatResolver::ResolveCombatHit
 → GAS目标伤害、ImpactReceiver／Movement动作、统一脱球
 → 阶段结束Close本次Action
```

主要接口在`HALCombatTypes.h/.cpp`、`CombatResolver.h/.cpp`、`HALImpactTypes.h`、`HALImpactReceiverComponent.cpp`。普通球`BasicBallActor.cpp`的请求组装与`Tests/HALCombatTests.cpp`的固定来源／多目标去重可作参考，不照搬球的伤害归属或命中后回Free规则。

| 接入项 | 当前接口与易漏点 |
|---|---|
| 机关伤害来源 | `FHALAttackSourceSnapshot.DamageMode = SourceFixedDamage`，`FixedDamage`使用机关自己的可配置基础值，`SourceActor = this`。不调用车辆攻击捕获函数来借用附近玩家攻击力 |
| 来源身份 | 普通环境机关无玩家归属时PlayerId保持`INDEX_NONE`。如新增机关SourceTag，应在项目原生Tag定义集中增加；当前只有球／碰撞来源Tag，不伪装成普通球 |
| 动作生命周期 | 每拳一个持久`FHALCombatAction`，在服务器有效阶段开始`Begin(*this, Snapshot)`，检查成功；不能每次Sweep或每个目标都重新Begin，不逐帧推进Pulse |
| 请求身份 | 使用`Action.GetSnapshot().AttackId`、`Action.GetPulseSequence()`填写请求，不能假设Begin前临时快照里的ID就是最终ID |
| 目标与接触 | 填`Hit.TargetActor`、实际目标物理根／合法组件、命中点及有效法线；源组件／自然冲量等按真实检测填写，不伪造质量／冲量来凑档位 |
| 冲击开关 | **`FHALCombatHitRequest.bApplyImpact`默认false**，需要冲击时显式设true；有无伤害由独立`bApplyDamage`表达 |
| 冲击模式 | **`Request.Impact.Mode`默认LegacyMeasurement**。若采用设计指定档位，显式使用`ExplicitTier`及`EHALImpactTier`，不要依赖旧速度换算路径 |
| 冲击配置版本 | 如果复用`FHALAuthoredImpactConfig`，其Version默认Legacy，需要明确迁移到TieredV1，不能只填Tier。目标及公共规则也应沿用已迁移的V1配置 |
| 结果消费 | 用`Result.bHitAccepted`判断是否已经接受本目标；不能用`ActualHPRemoved > 0`或`bImpactApplied`代替 |

伤害与冲击的抵抗／免疫完全分离：0伤害仍可有冲击，冲击免疫仍可扣血；HP=0当前也不自动等于淘汰，不能让机关提前退出整个命中处理。不要在机关中直接改HP、直接给车辆AddImpulse／SetVelocity或再次手动脱球；统一入口会执行已有规则与可重放物理动作。

`ExplicitTier`方向当前来自`-Hit.ImpactNormal`的水平投影，零方向才回退源到目标方向。若玩法要求始终沿拳朝向推出，先明确规范，再按这个接口约定填写有效法线，避免依赖回退造成侧向推开。当前`FHALImpactProposal`没有Direction字段；垂直抬升／翻转来自公共档位表，不为单个机关擅自改公共表。

## 3. 一拳去重与检测边界

- 每拳同一目标最多接受一次，下一拳才可再次命中；同一拳可分别命中多个目标，不能首个命中后关闭整个Action。
- 原型当前会在检测到目标时立即`HitActorsThisPunch.Add`。接入后应让`FHALCombatAction`作为结算去重依据；若保留原集合，只在`bHitAccepted`后登记，避免暂时未就绪／被拒目标被提前永久跳过。
- 伤害为0、目标免疫或HP不变的合法命中仍被消费；不能因未扣血重复请求。Resolver本身会在通知／施力前登记接受目标，保留其重入保护。
- 保留前一位置到当前位置的Sweep及Extending结束时的最后一段Sweep，防止低帧率／短ExtendDuration漏判。多组件返回同一Actor不应多次扣血。
- 当前仅Extending有效；是否让Retracting也能伤害尚未确认，不能顺便扩展。阶段结束／禁用／销毁应关闭Action、清理计时器和集合，避免晚回调延续旧拳。
- FistMesh目前无物理碰撞，HitBox用于显式Sweep。若增加物理阻挡，必须检查自然Chaos碰撞与显式冲击的合并，不能同时额外推两次；优先保持已确认的检测／表现边界。

## 4. 服务器时序与客户端表现

服务器负责状态切换、目标检测、命中及伤害／冲击；客户端不执行结算，不提交“命中谁”。`HasAuthority`与客户端NetMode边界均应检查，不能让非复制的场景机关在客户端自行攻击。

建议把阶段、服务器阶段起点、状态／拳序号及必要时长放入一致的RepNotify状态中，配置Actor复制并声明Replication。客户端按同步服务器时间计算拳位置，收到中途阶段也可重建表现；不在客户端从自己的BeginPlay时间独立跑一套循环，不逐帧可靠RPC广播拳位置，也不在客户端补发漏掉的伤害。网络下可以跳过已过时的表现阶段，当前状态仍须正确。

Tick如保留，只在需要运动／扫掠的阶段启用；不在Blueprint Tick管理伤害、Buff或阶段规则。当前服务器检测借用了PunchPivot位置，接入时要确保权威Sweep位置来自服务器阶段进度，视觉修正不能反过来移动权威命中区。BeginPlay／EndPlay／禁用／重新启用的状态初始化与旧计时器清理都要处理。

比赛何时允许启动机关需要先确定：当前原型从BeginPlay就开始循环，不能默认为已经与Playing阶段绑定；不要因此扩建整个竞技场调度系统。

## 5. 施工前要定的规则

以下尚未由用户确认，不在交接中擅自给数值：

| 项目 | 需明确的内容 |
|---|---|
| 基础伤害 | 每拳伤害值、是否允许配置成0；不能因为普通球为25就把它当成机关已确认数值 |
| 冲击 | 指定哪个档位、固定拳朝向还是按接触方向；抬升／翻转是否直接沿用公共档位响应 |
| 有效阶段 | 保持仅伸出判定，还是回收也有效；若两个阶段都有效，是否仍整拳只命中一次 |
| 时序与比赛阶段 | Waiting是否暂停、Playing何时启动／重置；保持还是调整当前预警与伸缩时长 |
| 作用对象 | 仅车辆还是也影响球；当前共享战斗目标是车辆，击球需要单独定义，不能当作给球加ASC |

已确认的总原则不重开讨论：机关自己的基础伤害、伤害／冲击分离、服务器权威、每拳每目标去重、客户端表现同步、保留现有车辆与球参数。

## 6. 最小交付与验证

1. 完整Editor／Game Target构建，记录实际UE补丁；关闭旧Editor／Live Coding后构建，不把热更新成功替代完整编译。
2. 自动化覆盖一拳重复回调／多组件只一次、同拳两个目标各一次、下一拳可再命中、未就绪拒绝后不误消费、两类免疫分离、0伤害冲击／HP0、结束后的旧请求拒绝。
3. 基本Listen Server双端：主机车辆和客户端车辆分别受击；双方看见一致预警／拳阶段；HP与有效档位、冲击／脱球对应，不能只有主机有效或客户端额外施力。
4. 回归目标持球、加速中受击、冲击免疫、机关销毁／玩家退出；异常下不残留Action／引用。针对实际网络物理路径检查动作不因重模拟重复执行。
5. 日志可关闭，至少能关联机关／拳序号／AttackId／Pulse／目标PlayerId／接受与拒绝原因／伤害／有效档位／脱球／物理帧。`hal.CombatLog 1`可看已接受的公共结算；拒绝原因如需复核，由机关自己的受控诊断补充。
6. 交付源码、配置／资产操作步骤、编译／测试结果和限制；人手保存的新蓝图与接线正常提交。若负责者为代码代理，遵循AGENTS不直接修改uasset／umap。不要提交Saved／Binaries／Intermediate或重置其他模块改动。

完整网络扰动、长时压力与四机并入MVP-B末尾[F4统一验收](MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)，不影响本模块的最小验证要求。公共CombatResolver／Movement协议如确需改动，先说明缺口与影响，采用窄改动并补原回归；不要另开一条机关专用扣血／推车路径。
