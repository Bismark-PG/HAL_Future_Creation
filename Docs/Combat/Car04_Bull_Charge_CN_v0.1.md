# Car04 主动技能「蛮牛冲撞」v0.1

2026-11-12，从 `main/3cb1386` 接续。规则已经用户确认，本文同时记录可调测试默认值、接线和验收。构建与测试状态见末节及项目状态入口。

## 规则

- 成功发动后自动向物理车头提供驱动力。总持续时间包含加速阶段，驱动力和前进上限逐渐提高，玩家保留削弱后的转向与普通刹车，期间禁用倒车和手刹漂移。驱动继续受地面、墙体和实际碰撞影响。
- 正立接地、存活、非眩晕、非自救执行且不在电墙回弹中时可以发动。技能期间腾空或翻倒会暂停实际驱动和接触攻击，计时继续，恢复可驾驶姿态后继续。
- 冲击免疫通过独立限时 GE 提供，免疫规则击退／击飞／脱球，伤害与眩晕仍按各自规则结算。眩晕不结束技能，进入眩晕的物理帧清零水平速度并清理当时的脚本回弹，期间停止主动推进。垂直速度、角速度与后续自然推挤继续由物理处理。解除后按本次技能原有进度恢复，不重新计时。
- 仅车头前方锥角内、向前速度达到门槛、且沿碰撞法线正在接近的接触可造成技能伤害。使用 Chaos 碰撞事件的 `Velocity - DeltaVelocity` 重建碰撞前速度，排除下落速度。每目标独立内置 CD，重复碰撞回调共享该 CD，满足条件后允许再次命中。两方的攻击资格先采集，再结算。
- 发动时已有持球立即沿物理车头发射。技能期间符合车头接触条件的 Free 球会被发射，普通捕获让位，两条技能发射均跳过附加后坐力。对手持球仍先走脱球规则，刚脱落当帧禁止立即发射；Launched 球保持原攻击归属。
- 到期结束伤害与免疫，转向在短时恢复段回到正常。接地时加快超过普通有效前进上限部分的衰减，目标包含其他仍有效的 Buff／区域影响。腾空期间沿用正常空中物理。淘汰、换车、离场清理旧 Avatar 的状态及命中 CD。

伤害沿用通用入口。攻击力和常规倍率在发动时保存，速度对应的动作倍率在每次碰撞时确定，目标防御／免疫在命中时判定。

```text
速度比例 = Clamp((碰撞前向前速度 - 最低速度) / (满伤害速度 - 最低速度), 0, 1)
动作倍率 = 可选速度曲线(速度比例)，或最低倍率与最高倍率之间的线性插值
伤害 = 发动时攻击力 × Max(0, 动作倍率 + 发动时其他倍率增量)
```

动作倍率有上限。伤害和冲击各自结算，技能冲击使用明确档位和水平推送预设。普通车身碰撞规则与本次已接受的技能碰撞在入口处去重。练习阶段扣血继续由 Game Flow 抑制。

## 配置与职责

GA 校验许可与冷却，GE／Tag 管持续状态及免疫。`HALBullChargeComponent` 管服务器接触、速度采样、攻击快照与每目标 CD。`HALMovementEffectBridge` 发布不可变运动参数，移动组件在物理步执行与重演。球沿现有状态机提交发射。

`FHALOverdriveMotion` 保存离散起止帧和加速／恢复参数，与原区域参数一并进入运动历史，运行时不在 GA 中 Tick。冲锋带来的加速度和速度增量与既有运动贡献先求和再钳制到 0～3，基础车辆 DA 保留原调参。运动历史升级到格式 **6**，联机两端需要同版构建。

## 生产特效讨论草案（2026-11-14）

用户已接受 Car03 特效作为初版，并于本轮整体同意 Car04 的三层表现方案、授权施工：加速时车头强气流、撞到其他车辆时更强的卡通火花并叠加闪光／冲击、车尾推进火星。延续柔和卡通风格。以下保留讨论入口，落地接口、资源、挂点配置与验证见[生产特效文档](../Presentation/Car04_BullCharge_FX_CN_v0.1.md)。

- 车头气流建议仅在主动技能驱动期间出现，以无明显色相的近白透明风带表现。形状从车头向两侧及后方掠过，随实际前进速度与技能进度变化，避免停车时仍显示满强度破风。
- 撞车大效果建议由服务器现有冲锋接触资格与每目标 CD 确认后触发，在世界接触点播放短促卡通火花、局部闪光与定向冲击波。撞墙／撞球不自动套用同级效果；伤害／冲击免疫各自结算，接触表现不应以 HP 是否扣除作为唯一依据。使用 Car04 独立 Niagara 配方，保留 Car03 已接受的资源。
- 尾部推进火星建议随技能有效驱动出现，可在速度上限阶段持续发射。发射器跟随挂点，已喷出粒子留在世界空间。眩晕、腾空、翻倒等实际驱动暂停阶段停止喷射，恢复后按原技能进度继续；结束、淘汰、换车与离场清理。
- 在独立技能 DA 中配置 Niagara 引用、目标表现 Mesh、Socket／骨骼名称、局部变换及强度参数。提供窄挂接结构，支持多个尾喷点；优先使用挂在现有骨骼上的 Socket，无需专为特效新增骨骼。挂点缺失时明确诊断并按显式配置回退或跳过该层，不影响技能规则。
- 讨论时状态 FX 支持 Bounds／MeshSurface，Car04 组件仅复制活跃标志。本轮新增挂点、表现生命周期／阶段同步与服务器接触事件出口；不改变驾驶、球发射、伤害或冲击判定，不新增逐帧 GA 或客户端命中上报。

## 首测参数

以下均为首测默认值，团队可以在独立技能 DA 调整。

| 分组／参数 | 默认值 | 用途 |
| --- | --- | --- |
| Charge → Motion → Timing | Duration 5 s、Ramp 2 s、Recovery 0.5 s | 总时长含加速，加速后维持过载，结束收速与转向恢复 |
| Charge → Motion → Drive | Initial Acceleration Scale 0.4、Final 1.5、Final Speed Scale 1.7 | 以车辆基础参数为参照的本技能倍率 |
| Charge → Motion → Steering | Steering Scale 0.35 | 额外削弱主动转向，原高速转向衰减继续生效 |
| Timing | Cooldown Seconds 20 s | 从成功发动起算，活跃中重按不刷新 |
| Contact | Target Cooldown Seconds 0.8 s | 每目标独立命中 CD |
| Contact | Front Half Angle 45° | 车头左右各 45° |
| Contact | Minimum Forward Speed 400、Minimum Closing Speed 50 cm/s | 排除低速与离开的接触 |
| Damage | Full Damage Speed 3500 cm/s、Minimum Multiplier 0.4、Maximum 1.8 | 速度到动作倍率，超过最高值继续封顶 |
| Damage | Damage Multiplier Curve 可选 | Float Curve 的 X 为速度比例 0～1，Y 为动作倍率，输出钳制到 0～Maximum |
| Impact | Heavy、Long | 明确冲击档位和水平推送 |
| Ball | Base Ball Speed 3000 cm/s、Inherited Fraction 0.75、Maximum 6000 cm/s | 出球速度等于基础值加正向车速贡献，并封顶 |

## Editor 接线

1. 关闭 Live Coding，使用本轮完整编译的 Editor。Content Browser 在 `/Game/Data/Abilities` 创建类型为 **HALBullChargeConfig** 的 Data Asset，建议命名 `DA_Ability_Car04_BullCharge`，先保留上述测试值。
2. 在 `/Game/Blueprints/GA` 新建父类为 **HALBullChargeAbility** 的 Blueprint，建议命名 `GA_Car04_BullCharge`。打开 Class Defaults，在 **Bull Charge → Skill Config** 选择上一步 DA，然后 Compile、Save。
3. 打开 Car04 实际使用的 Vehicle Definition，确认 **Combat → Version** 为 **Attack Snapshot V1**、**Impact → Version** 为 **Tiered V1**，核对该车的 **Attack Power** 和倍率增量。在 **Abilities → Version** 选择 **Vehicle Definition V1**。将现有唯一 `Input.Ability.Primary` Grant 的 Ability 改为该 GA，Level 为 1，Save。车型已有主技能时直接替换该条目，保留唯一 Primary。
4. 使用已有 Car04 车辆 BP。基类自动组装 `BullCharge` 组件，持续 GE 和冷却 GE 均由原生技能提供。通过房间选车时确认测试目录已登记 Car04。
5. 运行已允许单人测试的开发流程，或双进程 Listen Server。按原主技能键，观察渐进加速、转向、撞击、出球和结束收速。联机双方完整重启到同版代码及资产。
6. 排查时打开 `hal.AbilityLog 1`、`hal.CombatLog 1`、`hal.MovementEffectLog 1`。前者包含 `BullCharge started/contact/stopped`、撞前速度和倍率，Combat 日志包含实际伤害／档位，Movement 日志包含生效帧、倍率、Ready 和回滚信息。

## 验收

- 从静止、已有前进速度、刹车、转向和贴墙开始。检查总时长包含爬升、实际速度逐步提高、手刹／倒车受限、结束后恢复。分别测其他加速 Buff、减速区域和打滑区域重叠。
- 正撞、侧擦、追尾、迎面及目标远离，比较同速结果。对同一目标在 CD 内重复接触、CD 后重新撞击，再测试不同目标。检查 0 伤害与冲击免疫各自独立。双冲锋和冲锋对陀螺时双方结算一致。
- 持球起手、撞 Free 球、来袭 Launched 球、对手持球及刚脱落的球。检查归属、速度封顶、伤害消费、拾取锁和无附加后坐力。
- 冲锋期间电墙眩晕、空中／侧翻、自救、到期、淘汰、换车和返房。确认眩晕清零水平速度、状态仍计时、解除可继续，退场无残留。
- 同版 Host／Client 在低延迟和网络扰动下测试以上流程，重点看车主即时驾驶、权威命中、HP 一致和收速边界。校园四机与正式发布包另验收。

## 球速计算与接触推球排查（2026-11-12）

当前技能给球设置的是世界初速度，不会在公式外再加一次完整车速：

```text
出球速度 = Min(Maximum Ball Speed,
               Base Ball Speed + Max(0, 碰撞前向前车速) × Ball Inherited Speed Fraction)
```

原生单位为 cm/s，Editor 可自动显示为 m/s。只读载入本机已保存的 `DA_Ability_Car04_BullCharge`，确认 Base=9000 cm/s（90 m/s）、Maximum=9000 cm/s（90 m/s）、Inherited=0.75，GA 引用此资产；因此本配置会将车速贡献全部封顶，最终初速度固定为 90 m/s。已保存 Car04 的基础 Max Forward Speed=2400 cm/s，技能 Final Speed Scale=1.5，不计其他效果时目标上限约为 36 m/s。这些是本次观察的测试值，不替换上方首测默认值或推定运行时一定达到该速度。

因此，“持续被车推着走”不能仅凭该现象判为车速超过出球速度。还需确认接触是否真的提交了技能发射：只处理满足正立接地、车头锥角、最低前进／接近速度、有效同帧碰撞采样及拾取锁等条件的 Free 球；Controlled／Launched 球不通过此入口。在 Host 开启 `hal.BallNetLog 1`，以对应球的 `Free -> Launched` 记录为依据。没有转换时加球速不会解决接触资格问题；发生转换仍迟缓时，再检查实际刚体速度、碰撞减速和客户端表现。

可独立测试发动前持球的立即发射，与技能中正面碰到新生成 Free 球的接触发射，区分共用出球速度配置与接触判定链路。当前公式也没有保证球始终快于车辆；若日后需要稳定的领先速度，应另行确认相对车速增量／最低领先量与世界速度上限冲突时的规则。本次仅只读检查与说明，没有修改公式／保存资产或新增玩法复现通过结论。只读输出在 `Saved/Car04Charge/20261112/BallSpeedInspection.json`。

后续团队日志检查：`Saved/Logs/HAL_Future_Creation.log`（本地 2026-11-12 17:18:47 结束，SHA256 `507F436B3AF25C637AE747CEE1A397BF2635227EDBC842869CC7A7A7295BB8F9`）确认 NetTest Listen Host、Car04、单人 Playing；17:18:15.768 开启 `hal.BallNetLog 1`，17:18:40.840 关闭，期间没有任何球状态转换。用户确认此次仅测试冲锋中接触球，未测持球起手。该证据支持“接触发射未提交成功”，不支持将现象解释为发射后 90 m/s 仍慢于车；日志未包含接触拒绝分支，尚不能确定究竟是姿态／运动就绪、同帧 Chaos 采样、几何／接近门槛还是球状态／锁拒绝。代码存在首次碰撞未接受后球被自然推到近似同速、后续接近门槛继续不满足的可能，需要真实碰撞诊断验证。当前原生用例为人工构造碰撞样本，不能替代此次场景接触复现；下一步应为该链路增加可关闭拒绝原因／实际球速诊断和真实物理接触回归，再据证据修正。此次仅日志分析／留档，未改 C++、资产、速度公式或重跑测试。

## 接触发射修复与复测（2026-11-12）

已确认是 C++ 回调时序问题。UE 5.8.2 的 `FPhysScene_Chaos::HandleCollisionEvents` 在自己的事件处理器内直接调用 `DispatchPendingCollisionNotifies`，并不等待其他 Chaos 处理器完成；处理器遍历顺序也没有保证。原实现立即处理组件碰撞，并要求技能采样属于当前游戏帧。真实测试连续出现 `NoCollisionSample`／`StaleCollisionSample`，球保持 Free、峰值仅约 1589.68 cm/s。原测试预先注入采样，未覆盖这个顺序。

修复仅预约涉及冲锋的车／球、车／车接触，在同一 World 的 `OnWorldPostActorTick` 中回到原权威接触入口结算，此时当前帧的 Chaos 处理器已经全部执行。每组件每帧按对方刚体去重，最多保留 64 个接触，保留同帧最早撞前采样；旧帧、销毁对象和退场队列不再消费。重新检查状态、角度、最低车速、接近条件和球锁，拒绝时仍走原普通冲击处理。没有新增组件 Tick、RPC、复制字段或物理历史格式，也没有改球速公式、DA 或蓝图。

新增默认关闭的 `hal.BullChargeContactLog 1`：打印球名、拒绝原因、采样帧／当前帧、撞前速度、请求球速与刚体实际球速。可能结果为 `StateNotReady`、`BallNotFree`、`NoCollisionSample`、`StaleCollisionSample`、`InvalidObservation`、`BelowMinimumForwardSpeed`、`OutsideFrontCone`、`BelowMinimumClosingSpeed`、`LaunchCommitRejected` 或 `Launched`。同类拒绝限频；用 `hal.BullChargeContactLog 0` 关闭。配合 `hal.BallNetLog 1` 查看状态提交。

本机 UE **5.8.2-56702186** 最终 Editor R5／Game R2 构建成功（日志前缀 `ContactFix_`）。原生专项最终 **11/11**，覆盖碰撞通知在采样之前／之后、重复通知只发射一次、陀螺与冲锋既有规则、物理线程和历史。保存 Car04／技能 DA／球 BP 的真实单 Host 测试由修复前 **0/1** 转为修复后 **1/1**，成功 `Free -> Launched`，同帧采样，实际球速 **9000 cm/s**。保存配方的双进程真实碰撞在低延迟及 Host 出站 **80 ms** 注入下，Host／Client 均各 **1/1**，每轮两辆车均通过本人的普通 Primary 请求发动、经实际 Chaos 接触发球，服务器确认两球发射速度均 9000 cm/s；客户端确认两球状态和归属，未将其 PI 插值中间速度当作服务器发射速度。原眩晕续行／恢复／冷却双端回归也各 **1/1**。这些是同机 NullRHI 程序验证，真人画面／手感、校园四机和打包仍待团队复测。

完整原生检查为 **199 通过、1 带告警通过、4 失败**，没有记为全量通过。四项失败都仍引用已重命名的 `/Game/Data/MVP_B/DA_Vehicle_Player_TestBaseline_v1`，分别属于 Configuration、Health、Recovery 和 StatusFX 的资产测试；此次未改这些测试／引用。带告警项为原无效车型／出生点阻挡用例。原本 11 项旧配方失败是前轮时点，不代替此轮结果。1251 份保存资产及原 Config／uproject 共 **1256** 项与开工逐份 SHA256 一致，用户技能接线和调参保持。详见[本次验证摘要](Car04_Bull_Charge_ContactFix_Verification_20261112.json)，原始报告在 `Saved/Car04ContactFix/20261112` 和 `Saved/Car04Charge/20261112/ContactFix_*`。

中间失败保留：诊断版缺完整球组件类型头文件、测试缺接收器类型头文件已补齐；Editor 占用 DLL 导致链接失败，用户保存关闭后重新构建通过。双端夹具最初错误要求客户端读远端 Ability Spec，随后又使用了不会跨端保持的动态 Actor 名称；最终改为服务器检查技能授予、两端按复制的 Owner 识别测试球，低延迟 R3／延迟 R1 均重新验证通过。

团队无需新增接线。重开已编译的 Editor，用当前保存的 Car04 配方，先让车与 Free 球保持距离，再发动技能正面接触；低速低于 4 m/s 或侧面接触仍按原门槛拒绝，不是所有碰撞都应发射。分别测试 Host 和 Client，以及持球起手、Free 球、来袭 Launched 球和刚脱落／仍锁定的球。成功时日志应看到 `Result=Launched`、`RequestedSpeed=9000`、`BallSpeed=9000`；当前 90 m/s 已较强，可在功能复测后再调回合适手感。

## 首轮技能施工验证（历史）

- 本机 UE **5.8.2-56702186**，最终 Editor R16 与 Game R9 构建通过。最终 `Native_Release` **11/11**，0 测试警告／失败，包含 Car04 三项、Car03 两项、运动历史／真实物理线程与车型授予回归。检查速度门槛、倍率封顶、包序列化、取消后不得误发动、同目标 CD、眩晕续行、持球／自由球发射及无后坐力；未来发动／眩晕帧元数据被拒绝，失效持球导致发射失败时撤销状态和冷却且不提交运动／恢复段，零长度取消也不会收速。真实 TaskGraph 物理线程验证冲锋施力、结束制动、眩晕进入帧及其后自然运动、同帧回弹停止但保留确认序号和历史重演。
- 实际 `BP_VehiclePawn_Car_04` 的最终 `Network_Release_Low` 与 Host 出站 **80 ms** 延迟注入 `Network_Release_Lag`，两轮 Host／Client 均各 **1/1**，0 测试警告／失败／未运行／进行中，四个进程均退出 0。测试通过正常 Primary 请求，验证客户端请求、重复请求拒绝、双方状态、实际自动前进、上限提高、眩晕后继续、恢复与 20 游戏秒冷却。使用 Editor `-game`、NullRHI 和瞬态测试配方，尚待团队保存技能配置并验收真实画面／手感、校园四机与发布包。
- 全量原生报告为 **192 通过、1 项带告警通过、11 失败**。失败集中在旧免疫／加速 Primary 默认配方和 GF2 授予冲突，首个错误与当前已保存车型技能不匹配，符合此前状态记录中的旧夹具限制。此结果保留为全量检查结论。带告警项为 VehicleTest 的无效车型／PlayerStart 阻挡分支，未扩大为 Car04 专项告警。
- 开工已有油膜实例、玩家 Definition 和项目状态文档改动。验证前后 **1246** 份保存资产／Config／uproject 的 SHA256 一致，代理提供手动接线步骤。可同步摘要见[本轮验证 JSON](Car04_Bull_Charge_Verification_20261112.json)，原始日志和报告位于 `Saved/Car04Charge/20261112`。

中间失败均保留。早期构建遇到沙箱目录权限、复制宏形参名及测试代理类型头文件问题；原生夹具先后遗漏球初始化、物理回放后的当前帧确认、跨引擎帧的计时推进，已修正。双端 R2 暴露客户端预检误用了服务器权限条件，现已将权限检查留在提交和结算；R3 客户端先退出使主机夹具等待超时，R4 修正收尾，R5／R6 增加实际运动断言后通过，最终 Release 在边界修正后重新跑完上述全部专项及全量检查。直接 Game 可执行文件的早期启动退出 777006 且未产生日志，最终双端采用工程已有的 Editor `-game` 路径。
