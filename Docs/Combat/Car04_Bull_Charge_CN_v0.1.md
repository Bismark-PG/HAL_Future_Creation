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

## 本轮验证

- 本机 UE **5.8.2-56702186**，最终 Editor R16 与 Game R9 构建通过。最终 `Native_Release` **11/11**，0 测试警告／失败，包含 Car04 三项、Car03 两项、运动历史／真实物理线程与车型授予回归。检查速度门槛、倍率封顶、包序列化、取消后不得误发动、同目标 CD、眩晕续行、持球／自由球发射及无后坐力；未来发动／眩晕帧元数据被拒绝，失效持球导致发射失败时撤销状态和冷却且不提交运动／恢复段，零长度取消也不会收速。真实 TaskGraph 物理线程验证冲锋施力、结束制动、眩晕进入帧及其后自然运动、同帧回弹停止但保留确认序号和历史重演。
- 实际 `BP_VehiclePawn_Car_04` 的最终 `Network_Release_Low` 与 Host 出站 **80 ms** 延迟注入 `Network_Release_Lag`，两轮 Host／Client 均各 **1/1**，0 测试警告／失败／未运行／进行中，四个进程均退出 0。测试通过正常 Primary 请求，验证客户端请求、重复请求拒绝、双方状态、实际自动前进、上限提高、眩晕后继续、恢复与 20 游戏秒冷却。使用 Editor `-game`、NullRHI 和瞬态测试配方，尚待团队保存技能配置并验收真实画面／手感、校园四机与发布包。
- 全量原生报告为 **192 通过、1 项带告警通过、11 失败**。失败集中在旧免疫／加速 Primary 默认配方和 GF2 授予冲突，首个错误与当前已保存车型技能不匹配，符合此前状态记录中的旧夹具限制。此结果保留为全量检查结论。带告警项为 VehicleTest 的无效车型／PlayerStart 阻挡分支，未扩大为 Car04 专项告警。
- 开工已有油膜实例、玩家 Definition 和项目状态文档改动。验证前后 **1246** 份保存资产／Config／uproject 的 SHA256 一致，代理提供手动接线步骤。可同步摘要见[本轮验证 JSON](Car04_Bull_Charge_Verification_20261112.json)，原始日志和报告位于 `Saved/Car04Charge/20261112`。

中间失败均保留。早期构建遇到沙箱目录权限、复制宏形参名及测试代理类型头文件问题；原生夹具先后遗漏球初始化、物理回放后的当前帧确认、跨引擎帧的计时推进，已修正。双端 R2 暴露客户端预检误用了服务器权限条件，现已将权限检查留在提交和结算；R3 客户端先退出使主机夹具等待超时，R4 修正收尾，R5／R6 增加实际运动断言后通过，最终 Release 在边界修正后重新跑完上述全部专项及全量检查。直接 Game 可执行文件的早期启动退出 777006 且未产生日志，最终双端采用工程已有的 Editor `-game` 路径。
