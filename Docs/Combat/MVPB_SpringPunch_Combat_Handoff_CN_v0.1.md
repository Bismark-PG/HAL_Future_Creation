# 弹簧拳：战斗与联机接入交接注意事项（v0.1）

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
