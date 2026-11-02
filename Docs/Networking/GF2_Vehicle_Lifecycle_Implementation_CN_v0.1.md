# GF-2：换车／新局GAS生命周期与练习伤害策略

日期：2026-11-02，Asia/Tokyo。依据[分段施工v0.2](Room_VehicleSelection_Practice_Match_Construction_CN_v0.2.md)。本轮接续`main/653320d27a66fe50135eef320dd36dec6fa7e863`，GF-1已经提交，新增三份Gameflow DA为团队未跟踪资产，原样保留。

## 1. 团队保存配置核对

已通过只读加载／T3D导出核对：两份Room DA都引用`/Game/Data/Gameflow/DA_VehicleCatalog_Default`；目录含`Car_01 → BP_RoundedVehiclePawn`和`Car_02 → BP_VehiclePawn_Car_02`。新增原生测试检查两份车型BP／Definition结构及迁移有效性。

**本轮开工时磁盘上的`DA_Room_Development.Rules.Profile`仍是Exhibition（0）**，Exhibition资产同样为Exhibition。前者请团队打开DA，将Rules → Profile改为**Development**并保存；这可能是尚未保存的编辑，截图中的资产图标本身无法证明保存值。目录引用正确不代表开发单人门槛已经配置正确。代理没有修改或保存这三份资产，也不以原生临时Development夹具通过替代该人工配置项。

本阶段没有新的GA／GE／输入接线，不启用当前回归GameMode的Room Config。可先修正Profile并保存；实际选车界面与生车流程在GF-3接入。

## 2. 生命周期入口与归属

新增PlayerState服务器C++接口：

```cpp
bool ChangeVehicleAvatar(ATestVehiclePawn* FreshPawn, uint32 ChangeSerial);
bool BeginNewRoundAvatar(ATestVehiclePawn* FreshPawn, uint32 RoundId, uint32 ChangeSerial);
```

两者均非玩家RPC、非蓝图随意调用的重置按钮。Controller、PlayerState、ASC及新Pawn须属于本次权威World；新Pawn已完成有效配置，但尚未被任何Controller控制。Room启用时还检查本人的选车确认、服务器目录精确类以及Open／Loading阶段和冻结名单。运行中的同一Pawn不能作为新操作的目标。

`ChangeSerial`由服务器流程分配，在同一PlayerState生命周期内单调递增、非0。相同Serial＋Pawn＋操作类型／Round的成功重试只返回已有结果，不再次回血、清效果或授予能力；旧Serial、同Serial换目标、重复Round换目标均拒绝。无效配置在清理旧车前拒绝；若提交后Possess失败，保持未就绪并记录失败，该Serial不被当作成功重做，GF-3应清理失败候选并以新Serial重试。

`RoundId`同样非0且递增。新局必须使用重新生成的Pawn，借此隔离旧的Chaos状态、运动历史、反作用力、冲击／回弹／自救缓存和表现状态。GF-4本来就要求车辆／球重新生成；本轮没有在仍运行的刚体上直接清空物理内部历史。新局幂等调用仍允许指向已成功初始化的当前Pawn，但不能用新RoundId反复重置同一实例。

PlayerState复制`FHALVehicleLifecycleState`：Revision、ChangeSerial、RoundId、当前Avatar。客户端按该身份重新绑定ASC，旧Pawn的迟到OnRep不能抢回新Avatar；客户端不执行属性重置。普通Possess／OnRep／ActorInfo重复绑定继续使用原来的初始化保护，不会自动回血或清冷却。

属性仍由GAS复制。生命周期身份快照与各Attribute的复制不是一个原子事务，GF-4仍需对本次Round、Avatar及必要客户端属性／输入就绪做载入屏障；不能只因Revision到达就开放比赛。跨图保留／复制PlayerState身份、单调计数与服务器开局标识由GF-4明确承接，不能将每个新World都无条件从RoundId=1重新开始。

## 3. 一次转换实际处理

1. 从新Pawn已经应用的配置读取MaxHP、Combat和Impact初始种子，不从旧ASC的当前值反推。完整验证后才开始提交。
2. 撤销Room的旧Avatar就绪，保留选车版本／车型／席位／颜色，防止把旧的Ready复制给新车。
3. 旧Pawn进入Retired：释放持球约束，调用原球退出处理，清理区域来源／自身GE、自救、旧输入与定时器、输入上下文及相机积分，解除旧ASC表现／运动绑定。旧车不能再次通过普通绑定取得ASC。已发射球沿原退出规则处理；实际新局旧World的球由GF-4统一重建，不宣称本轮已经跨图销毁。
4. 撤销旧Pawn来源的Ability Specs，保留PlayerState来源的共享Specs；取消进行中的Ability。现有`PlayerState.DefaultAbilities`仍是共享玩家装配，重复授予保持幂等。本轮不增加车型技能DA字段或第二技能槽；以后车型授予须使用具体Avatar作为Source，遵守输入唯一性契约。
5. 显式换车清除Avatar作用域GE及`Cooldown`根下的冷却GE，其他非Avatar玩家作用域GE保留。显式新局清除全部当前Active GE，包括未标Avatar的练习／调试效果；两者都保留共享Ability Specs。
6. 直接应用新种子到ASC：MaxHP／CurrentHP、AttackPower／输出增量／受伤倍率、冲击降档，以及六项运动／区域基础增量。设置已有初始化结果标记，不来回清零一次初始化保护。物理基础参数已经来自新Pawn组件，普通Buff仍由GAS Modifier叠加。
7. 绑定并Possess新Pawn，创建新的MovementEffectBridge Epoch与实际运动快照，复用既有物理协议格式3。新Pawn完成常规GAS、Health、表现和输入绑定；此时仍需服务器`Room.CompleteAvatar`授予就绪资格。

旧Pawn实际Destroy由生成流程在转换成功后执行，便于GF-3统一处理失败、视觉和出生流程。旧Pawn延迟Destroy／解绑不能清掉新车效果。不得在蓝图切Pawn后临时修改HP来掩盖生命周期问题。

### GF-3／GF-4调用顺序

GF-3：服务器验证目录ID及确认版本 → 生成并完成配置的新Pawn → `ChangeVehicleAvatar` → 检查结果／销毁旧车 → `CompleteAvatar(Controller, SelectionRevision)`。若存在异步加载，完成时仍须匹配同一确认版本，已取消的请求不能继续生车。GF-2提供同步转换入口，尚未实现异步生成及UI。

GF-4：锁定名单 → 真实转场并重建身份／车型 → 为每人生成新Pawn → `BeginNewRoundAvatar(Fresh, RoundId, ChangeSerial)` → `CompleteAvatar(Controller, SelectionRevision, RoundId)` → 实际客户端载入屏障 → 倒计时 → Playing。Loading下的Complete还检查Lifecycle.RoundId与具体Pawn，过期Round或尚未按新局初始化会拒绝。该服务器就绪不是客户端“Ready”声明，也没有新增第二个准备按钮。

## 4. 练习伤害策略

统一`FCombatResolver`消费GF-1的`bDamage`权限。Room Open期间仍完整验证攻击／目标，计算RawDamage，记录`bDamageSuppressedByFlow=true`，将ResolvedDamage和实际扣血置0；真实DamageImmune标签单独记录，不把练习规则变成永久免疫GE。

命中随后仍消费目标、执行独立冲击／免疫降档、脱球和表现通知。HP为0扣减不等于整次命中失败。Locked／Loading／Countdown继续由世界交互权限拒绝规则命中；Playing恢复正常伤害。Room关闭时保留旧NetTest伤害行为，现有伤害抗性与冲击抗性继续解耦。

此策略覆盖走标准Resolver的球、拳、爆炸及危险区；后续Ability／机关也必须走该管线，不能直接从蓝图或碰撞回调改HP。并未增加淘汰、观战、比赛计时或胜负结算。

## 5. 验证记录与边界

本机UE **5.8.3-58210709**，最终Editor／Game Development Win64均构建成功；最终构建后没有继续修改C++。共享证据及保护哈希见[GF-2验证摘要](GF2_Verification_20261102.json)，原始报告与日志在`Saved/GameFlow/GF2_20261102`，Saved不入库。

- 最终普通回归 **118/118**，0测试警告／失败／未运行。新增4项覆盖A→B→A种子及实际组件参数、来源能力／共享能力、区域／球权／冷却清理、普通重绑不回血、显式转换幂等、过期操作拒绝、新局重建、练习伤害与冲击／脱球／命中消费，以及团队保存目录中的两份真实车型配置。原生夹具使用临时Definition：A的HP100／攻击20／降档0／加速度1800，B的HP220／攻击75／降档1／加速度3600，没有修改保存车型。
- 最终真实独立进程Listen Host／Client的GF-2测试各 **1/1**，0测试警告／失败。两端完成A→B→A→Round1新Avatar，客户端确认复制Avatar／ASC绑定、属性、效果清理和新运动Epoch；服务器确认4次提交及成功重试不重复初始化。使用现有NetTest实际默认Car_02类、临时GAS种子及原物理调参，不把该夹具写成玩家操作选车界面或两种模型切换验收。
- 在同一最终源码上重跑既有GF-1真实双端兼容测试，Host／Client也各 **1/1**，0测试警告／失败。默认Room Config为空时旧NetTest流程继续可用。
- **1180份保存资产逐份SHA-256全同**，包括用户新增的三份Gameflow DA；Config、Build.cs及uproject未改。最终254份Source／Config／uproject哈希留档。三份团队DA仍未跟踪，本轮源码／文档尚未提交／Push，跨设备前须正常纳入Git。
- 最后一次只读资产核对仍显示`DA_Room_Development.Rules.Profile=Exhibition`，该人工配置项待团队改为Development并保存；两份Room的Catalog及Car_01／Car_02引用有效。原生Development夹具通过不抵消此项。

首轮Editor的日志格式及测试字段引用编译错误已修正；首轮普通回归117/118，唯一失败为练习夹具未显式设置`bApplyImpact=true`，其默认值为false。补齐夹具请求后专项4/4及最终完整118/118通过，没有降低冲击／脱球断言或改变生产规则来适配测试。首轮与最终结果分别保留。现有引擎弃用、相机测试及工具链建议警告保持；测试0警告不代表编译0警告。

没有进行本轮真人UI／手柄操作、Niagara画面、真实跨图、打包、完整网络扰动或四台电脑验收；新局测试只验证当前World中的阶段／身份契约。GF-3仍须实现生产选车／生成编排和基本UI，GF-4才接真实ServerTravel与客户端载入屏障。既有F4／VP-4C和FX欠项不因本轮回归通过而关闭。

下一环节为 **GF-3：灰盒房间、选车与练习**。团队目前无需重配Primary GA／GE；修正Development Profile并保留两份Room配置与目录即可。GF-3再提供专用房间地图／GameMode／UI和出生点的手工接线步骤，继续保留现用NetTest回归入口。
