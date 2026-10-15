# MVP-B 10-C：统一命中／攻击快照／伤害公式实施与接线（v0.1）

日期：2026-10-14（Asia/Tokyo）。施工基点 `main / 272ba8717af3d8e04dad9ff33d7e3115fc02ea72`，实际引擎 UE 5.8.3。依据 [已确认方案](MVPB_Phase10C_Combat_Plan_CN_v0.1.md)。**C++、Editor/Game Development 编译、自动化22/22、保存资产只读检查和本机双进程连接／初始HP复制通过；团队 DA 迁移与可见单机／联机验收尚未执行。** 不将 10-B 的旧报告当作本轮复测。

**2026-10-15 接续更新：上述是10月14日施工结束时的历史状态。用户现已报告另一设备完成相关资产更新和测试，DA迁移已在 `1eb02c4` 提交；代码／文档接续核对完成。** 详情见第6节及 [接续核对记录](MVPB_Phase10C_Handoff_20261015.json)，不覆盖原自动化／Legacy只读快照，也不把未细分的扰动工况视为已有实测证据。

## 1. 行为与职责

- `FHALCombatAction` 是来源持有的攻击生命周期，服务器生成 GUID，当前脉冲序号从 1 开始。`FHALCombatHitRequest` 提供目标、可选物理数据和独立伤害／冲击开关；`ResolveCombatHit` 返回接受／拒绝原因、原始伤害、结算伤害、实际扣血及独立冲击结果。
- 共用入口不要求来源 Actor／ASC 在命中时仍存活；来源快照保留稳定 PlayerId、动作 ID、捕获数值及标签。伤害可以没有物理刚体、命中法线或来源 ASC。目标通过已有 AbilitySystemInterface 取得玩家 PlayerState ASC 或被动车 Actor ASC；需要伤害时要求对应属性已初始化、Avatar 和目标匹配。
- 去重按动作＋当前脉冲＋目标执行，在通知／物理结果之前登记，防止重入。一个脉冲可命中多个目标；`AdvancePulse` 只接受更大的序号。每脉冲最多256个不同目标，超额明确拒绝，不驱逐旧记录。`Close` 后旧请求不能重开；新攻击使用新 GUID。记录由来源对象释放，不建全局缓存或超时放行机制，生命周期不可复制。
- `UHALCombatAttributeSet` 持有 AttackPower、OutputMultiplierDelta 和 DamageTakenMultiplier，随 ASC 复制。配置只在 ASC 首次初始化时提供种子，重复 Possess／绑定不重置生效数值。它不保存冲击抗性、抓地或物理质量。
- 球在权威发射时生成快照，放入原子 `FBallRepState`；飞行过程中来源 Buff 移除或发射者退出不回算。首次接受命中后回 Free 并关闭该次 Action；免疫、零伤害或目标 HP=0 也会消费这次球攻击。稳定归属同时进入可复制的 `FHALCombatEffectContext`。
- 固定机关来源和车辆攻击来源共用目标伤害规则。车辆攻击为 `CapturedAttackPower × max(0, ActionMultiplier + MultiplierDeltaSum)`；命中时再乘 `max(0, DamageTakenMultiplier)`，`State.Combat.DamageImmune` 使伤害为0。负值不作为恢复，非有限／溢出数值不能进入扣血。
- 伤害和冲击独立：伤害免疫、0 HP、0伤害均不阻止已有物理冲击；无刚体、无方向或非法物理输入只使冲击分支返回明确结果，不取消合法伤害。Health 组件继续提供读取／通知，扣血统一在 Resolver 经 Instant GE 处理。

本环节保留三档物理选档、速度变化、翻转和既有脱球阈值，不修改驾驶、控球、反作用力或 Network Physics 历史。显式档位、目标冲击抗性／免疫与最低脱球档位留给 10-D。标签载荷本轮仅保存元数据，尚不表示已接入状态 GE、技能或 Cue。弹簧拳保持另一成员的独立模块，等阶段10完成后再接伤害／冲击。

## 2. 显式资产迁移与旧25伤害保护

新增版本枚举 `EHALCombatConfigVersion`：`LegacyFixedDamage=0`，`AttackSnapshotV1=1`。旧资产没有新字段时保持0，不因 C++ 新默认值自动启用公式。

| 配置 | LegacyFixedDamage | AttackSnapshotV1 |
|---|---|---|
| Vehicle DA → Combat | 战斗属性明确初始化 AttackPower=0、输出增量0、受伤倍率1；新种子字段不启用 | 从已填写并校验的 Combat 字段初始化 |
| Ball DA → Gameplay.Damage | 保存的 VehicleHitDamage 在发射时捕获为固定伤害，当前仍25 | 发射时读取车辆有效攻击力与倍率；要求发射车辆已迁到V1，否则拒绝发射并保留持球 |

旧 `VehicleHitDamage` 字段保留作兼容／对照，V1 下不再作为公式输入。`KnockbackStrengthMultiplier` 始终只参与原物理选档，不能当成伤害倍率。冲击是否执行由请求的 `bApplyImpact` 决定，倍率0仍按旧规则选Light，不表示没有冲击。

### 团队在 Unreal Editor 手工操作

1. 保存工作并退出 Editor／游戏，关闭 Live Coding，完整编译两个 Development Target 后重新打开工程。代理不编辑／保存 `.uasset` 或 `.umap`。
2. 打开 `/Game/Data/MVP_B/DA_Vehicle_Player_TestBaseline_v1`，展开 **Combat**：先填写 Attack Power=`25`、Output Multiplier Delta=`0`、Damage Taken Multiplier=`1`，最后将 **Version** 设为 **Attack Snapshot V1**。保持其他物理、Movement、BallControl、Health、Visual、Local字段原值。保存并 Validate Assets。
3. 打开被动车 DA `DA_Vehicle_Passive_TestBaseline_v1`，同样填写25／0／1并设V1，保存并验证。被动车没有主动移动，使用相同目标属性；它的 AttackPower 不会令普通碰撞产生伤害。
4. **先迁车辆，再迁球**。打开 `DA_Ball_Normal_TestBaseline_v1` → **Gameplay → Damage**：Action Multiplier=`1`、Action Multiplier Delta=`0`、Effect Tags 为空，最后将 **Version** 设为 **Attack Snapshot V1**。旧 Vehicle Hit Damage 保持`25`，Knockback Strength Multiplier 保持`1`，其他球字段不动。保存并验证。
5. Test／NetTest 的 GameMode、PlayerStateClass、DefaultPawn 和既有 DA 引用不变，无需新增 Blueprint Event Graph。检查地图实例没有指向别的 DA；重启玩法应用配置，不在运行中热切版本。
6. 在使用 HALPlayerController 的 NetTest 双端执行 `HALGASStatus`：玩家应为 CombatReady=1、CombatVersion=1、Attack=25、OutDelta=0、Taken=1；初始 HP仍100。`hal.DebugHUD 1` 可看本车 Combat V1。Test 旧 Controller 没有该 Exec 时用 `showdebug abilitysystem` 和已有 Health 显示检查。

车辆迁到V1但球仍Legacy时，球继续固定25，便于逐步对照；球先迁V1而发射车未迁会明确拒绝，不用猜测的攻击默认值补齐。配置变化需要新开玩法／新局，ASC 已初始化后不会因换 Avatar 自动重置战斗数值。

## 3. 团队验收

1. **Test 单机**：玩家与被动车初始HP100；正常球命中每次25，自动控球、带球、发射后坐、受撞脱球、锁定、三档击退、墙角及腾空保持原基线。
2. **0 HP／0伤害**：把目标打到0后再次击中，额外档位冲击仍生效，球结束该次攻击回Free。另复制测试DA，将玩家攻击设0，仍应有有效命中／冲击；不得修改保存基线来长期替代正常资产。
3. **伤害抵抗**：复制目标车辆DA，将受伤倍率设0.5，每次原始25只扣12.5，冲击选档不随伤害减半。伤害免疫由未来GE／状态来源提供；本轮自动化已用ASC标签验证，不需要团队为测试写临时蓝图扣血或另建HP。
4. **双进程**：从Entry运行原Host／Join／Start流程，双向球命中均只扣25，HP与球状态最终一致；两端 `HALGASStatus` 的战斗版本和属性一致。保持后台CPU节流关闭。
5. **网络扰动**：在约100ms实测RTT、1%丢包、20ms抖动下重测驾驶／持球／命中／快速连按，确认无重复扣血和永久状态分歧；发射者退出后已发射攻击仍有效。记录实际提交、引擎补丁、DA保存值、双端日志及结果。

非球来源、多目标／多段、Buff移除和来源销毁用C++自动化验证入口；不代表真实范围技能或机关已开发。完整冲击抗性／免疫在10-D再做验收，不把此处的伤害免疫测试扩大为全部抗性系统已完成。

## 4. 日志和未来来源接入

`hal.CombatLog 1/0` 记录一次接受结果的 Attack GUID、Pulse、PlayerId、来源标签、数值模式、Raw／Resolved／Actual和冲击结果。继续使用 `hal.BallNetLog`、`hal.VehicleNetLog` 与 `HALGASStatus` 对照权威发射、状态序号及复制结果。HP变化通知不作为命中次数；后续演出从权威命中结果产生。

新来源在服务器动作成立时构造快照并调用 `Action.Begin(*this, Snapshot)`；每个目标请求携带 `Action.GetSnapshot().AttackId` 与 `Action.GetPulseSequence()`，交给 `ResolveCombatHit`。下一段攻击显式 AdvancePulse，动作结束 Close。固定机关使用SourceFixedDamage且不需要ASC；检测方式和机关时序继续由机关自身管理。持续GE生命周期、运动参数桥接及弹簧拳实际接入留给对应后续环节。

`ResolveVehicleHit` 仅保留单次即时兼容适配，现有旧测试仍使用；运行时球已用持久Action。重复物理回调／持续伤害来源必须使用共享Action，不能每个回调新建动作来绕过去重。

## 5. 本轮验证记录

- 最终 `HAL_Future_CreationEditor Win64 Development` 与 `HAL_Future_Creation Win64 Development` 均成功，使用 UE 5.8.3。初次完整 Editor 构建被活动 Live Coding 阻止，用户保存并关闭后完成重编。引擎 `Build.bat` 的路径检查在当前执行环境未通过，改为使用引擎自带 dotnet／UnrealBuildTool；未更改引擎文件、工具链或项目配置。编译带现有引擎／Network Physics 弃用警告，MSVC14.51较推荐14.50更新，本轮不扩展到物理接口迁移。
- 全部 `HAL.FutureCreation` 自动化最终 **22/22通过、0警告、0失败**，既有16项与新增6项均成功；报告 `Saved/Phase10C/Automation_Final/index.json`。首轮有两项带警告的成功：来源销毁夹具未注册引擎World上下文，以及故意的迁移拒绝警告。最终夹具创建／清理独立上下文，明确断言来源Actor／Avatar销毁成功、弱引用失效，并登记预期迁移拒绝消息后重编复测；不能用首轮替代最终来源销毁证据。
- 只读 Unreal 检查确认玩家／被动车／球的新版本字段均为 **LegacyFixedDamage**，两个地图仍引用正确PlayerState／Pawn；320个项目资产在整个源码施工／验证前后和只读采样前后均未变。没有调用资产Compile／Save，种子字段显示25／1并不表示团队已迁移。
- 隐藏 Listen Server／Client 使用现有 NetTest 和Legacy资产，主机监听7786、客户端Join succeeded；客户端收到两辆车HP100／MaxHP100。主机状态为CombatReady=1、CombatVersion=0、Attack=0、Taken=1，符合兼容策略。本轮没有真人输入、动态命中、V1保存资产迁移或网络扰动；不能据此宣布V1攻击快照／伤害复制验收完成。两个测试进程已清理。
- 独立共享摘要及当次源码／证据哈希见 [10-C Verification JSON](MVPB_Phase10C_Verification_20261014.json)，不回写10-A／10-B历史JSON。`Saved/`日志和报告不随Git同步，接收设备使用摘要并执行必要验证。

团队尚未按第2节保存V1资产或完成第3节真人验收，当前是 **10-C代码与自动验证完成，资产迁移／团队验收待完成**；通过后再进入10-D。

## 6. 跨设备接续与团队完成反馈（2026-10-15）

- 用户报告已在另一电脑完成10-C、相关蓝图／资产更新和测试。当前 `main / 1eb02c4c8cae47cd010337912bbd165abdfce256`，开工核对时工作区干净；最近提交包含玩家／被动车／普通球三个DA的迁移，包内均含AttackSnapshotV1名称。接续记录保存三份资产当前哈希；本轮未通过Unreal重新导出保存字段，不能把名称检查当完整数值采样。
- 复核原验证清单73份源码：29份原始SHA一致，44份仅LF／CRLF差异，归一化后全部一致，没有发现清单之外的源码内容漂移。已读取Action／快照／请求结果、Resolver、战斗Attribute、Effect Context、球发射／消费／退出和新增六项自动化。
- 团队功能完成反馈已接受，不继续将迁移标为未开始；详细地图、每个测试子项、版本状态截图和RTT／丢包／抖动条件没有在本条反馈中逐项列出，保留该证据边界。
- 此接收设备仍安装UE5.8.2，与原交付验证的5.8.3不同。项目Editor DLL仍为10月13日版本，缺少10-C Combat结果／CombatReady诊断字符串；当前10月15日本地双端日志虽有25HP扣血，不构成新版V1验证。后续在本机运行前应先统一到团队验证补丁并完整编译当前Target，本轮没有替用户安装引擎或复测。
- 本轮只更新交接文档；未改源码／参数／资产，未新编译或执行自动化。下一环节为10-D规则确认：显式／物理冲击选档、目标冲击抗性／免疫与最低脱球档位。没有启动10-D或接入弹簧拳。
