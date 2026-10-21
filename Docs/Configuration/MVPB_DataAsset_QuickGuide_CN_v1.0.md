# MVP-B Data Asset 配置速查（现行10-E）

2026-10-21输入接线补充：Definition模式下，BP的旧Input字段只读；自救输入填写在 **玩家Vehicle Definition → Local Config → Input → Self Right Action**。现已补齐可选Digital (bool)字段及初始化复制，具体按键仍在IMC；不要切Legacy绕过。详细步骤见[自救配置第4节](../Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md#4-独立长按输入editor手工步骤)。

初版：2026-09-18；更新：2026-10-16。适用：当前车辆、普通球与测试场景，按已迁移的10-C AttackSnapshotV1／10-D TieredV1说明字段用途。本次只按现有源码核对职责，没有重新导出DA保存值或修改资产，以下不作为新数值基线。旧文件名保留便于已有链接继续使用；接续入口见[Project_Status](../Project_Status_CN.md)。

## 1. 去哪里改

当前五份资产位于 Content Browser 的 **Content → Data → MVP_B**（`/Game/Data/MVP_B`）。双击资产，在 Details 中修改参数。

| 资产 | 类型 | 配置内容 |
|---|---|---|
| DA_Vehicle_Player_TestBaseline_v1 | VehicleDefinition | 玩家车型的基础配置：驾驶／漂移、控球、发射／后坐、脱球、初始HP／攻击力／受伤倍率、冲击降档／脱球资格、物理与外观；还引用Local DA |
| DA_Vehicle_Passive_TestBaseline_v1 | VehicleDefinition | 被动物理测试车：Box尺寸、物理、初始HP／战斗与冲击配置、外观；不消费驾驶、控球或Local配置，不是第二名联机玩家的专属DA |
| DA_Ball_Normal_TestBaseline_v1 | BallDefinition | 这种球的半径／物理／外观、攻击结束条件、拾取锁、伤害动作倍率与主动攻击冲击档位；球发射速度／后坐在玩家车辆DA |
| DA_Local_TestBaseline_v1 | VehicleLocalConfig | 输入资产引用与跟随相机，含可选Primary Skill Action、Self Right Action；具体按键在引用的IMC，驾驶施力参数在玩家车辆DA |
| DA_Combat_TestBaseline_v1 | CombatRulesDefinition | 全场公共冲击标尺与三档物理响应：Light／Medium／Heavy各自推多远、抬多高／翻转，以及自然碰撞如何选档 |

这些都是共享的配置资产，不是玩家运行时状态或存档。四名玩家可共用同一份玩家车辆DA／Local DA，各自的当前HP、Buff、冷却仍在各自ASC／PlayerState中；速度／球权在各自Actor／组件中。以后新增不同车型通常复制或新建车型DA并绑定对应Pawn；Local和公共Combat可继续共享，不按玩家人数复制一套DA。

## 2. 常用参数

以下为 Details 的字段路径，界面可能显示带空格的名称。只列常用入口，完整字段与原始数值见[详细迁移文档](../../MVP_B_Phase1_Editor_Migration_CN_v1.0.md)。

| 想调整 | DA / 字段 | 含义 |
|---|---|---|
| 加速、刹车、速度上限 | 玩家 → Movement.Drive | Forward/ReverseAcceleration、BrakeDeceleration、MaxForward/ReverseSpeed |
| 侧滑与漂移抓地 | 玩家 → Movement.Grip | LateralGripRate 为正常侧向抓地；HandbrakeGripRate 为手刹侧向抓地 |
| 转向与手刹旋转 | 玩家 → Movement.Steering | SteeringAngularAcceleration、FullSteeringSpeed、手刹转向倍率与 Yaw 阻尼 |
| 顶墙时转出 | 玩家 → Movement.WallEscape | 低速组合输入下的离墙辅助；ExitSpeed 必须 ≥ EnterSpeed |
| 控球范围、跟随稳定性 | 玩家 → BallControl.Acquisition / Control | 获取半径/检查间隔、控球距离、位置/速度约束强度与最大力 |
| 球的发射速度、车辆后坐 | 玩家 → BallControl.Launch | LaunchSpeedIncrement 叠加于车辆速度；Base/MaxRecoilDeltaSpeed 管后坐范围，Max 必须 ≥ Base |
| 多强的冲击会脱球、基础冲击抗性 | 玩家／木桩 → Impact | TieredV1下用MinimumDropTier／DropOnImpact与TierReduction；规则脱球比较实际档位，不使用旧原始冲量阈值 |
| 脱球后的重捕等待／球弹出 | 玩家 → BallControl.ForcedRelease | DropReacquireLockDuration、DropBallImpulse／DropBallUpwardImpulse；旧DropCollisionImpulseThreshold只供Legacy路径 |
| 初始生命值 | 玩家或木桩 → Health.Health.MaxHP | 下次启动玩法的初始/最大 HP，不直接编辑当前 HP |
| 车辆基础攻击力、输出增量、受伤倍率 | 玩家／木桩 → Combat | AttackPower、OutputMultiplierDelta、DamageTakenMultiplier；V1初始化对应GAS属性，局内GE修改有效属性，不改写DA |
| 球大小、质量与阻尼 | 球 → Radius / Physics.Body | Radius 为碰撞半径；质量使用 Mass Override，阻尼影响物理运动 |
| 球何时结束攻击、恢复可拾取 | 球 → Gameplay.State / Acquisition | 低速阈值/持续时间/检查间隔，以及结束攻击后的拾取锁定 |
| 球攻击的伤害倍率 | 球 → Gameplay.Damage | AttackSnapshotV1用ActionMultiplier／ActionMultiplierDelta，发射时结合车辆有效攻击力／输出增量快照；VehicleHitDamage只供旧固定伤害版本 |
| 球主动攻击的冲击档位 | 球 → Gameplay.Impact.Tier | TieredV1明确指定None／Light／Medium／Heavy，伤害与冲击独立；旧KnockbackStrengthMultiplier不参与V1冲击 |
| 每个档位实际击飞效果 | Combat → BallImpacts.LightTier / MediumTier / HeavyTier | HorizontalDeltaSpeed、TargetVerticalSpeed、AirborneFlipTurns为公共物理响应；虽然分类仍叫BallImpacts，V1已供不同来源共用 |
| 自然碰撞选档／来源上限／连续接触 | Combat → CollisionImpacts | Light／Medium／HeavyThreshold、Environment／FreeBallTierCap、ContactRearmSeconds、MaximumRuleRestitution；玩家车型自身CollisionTierCap在车辆Impact中 |
| 相机距离、角度、跟随延迟 | Local → Camera | TargetArmLength、BoomTransform.Rotation.Pitch、CameraLagSpeed / bEnableCameraLag |
| 输入映射、按键 | Local → Input 引用；打开所引用的 IMC | Local指定IMC与转向／油门／刹车／手刹／发射Action，另有可选PrimarySkillAction及独立SelfRightAction；具体键位在IMC中修改，Definition模式下不改BP旧Input字段 |
| 模型、材质与位置 | 车辆或球 → Visual | Mesh、Materials、RelativeTransform；外观尺寸与物理碰撞尺寸分别配置 |

距离通常为 **cm**，速度 **cm/s**，时间 **s**，质量 **kg**；角度、角速度及高级物理字段以各字段单位提示为准。调参前保留已有值，每次优先改一组参数以方便比较。

V1伤害可理解为：发射时锁定的车辆攻击力 × max(0, 球动作倍率 + 车辆输出增量 + 球动作增量)，命中时再按目标受伤倍率／伤害免疫结算。球指定的冲击档位再按目标降档／冲击免疫结算，两条分支独立。旧BallImpacts的Medium／HeavyImpactSpeedThreshold用于Legacy球速度选档，不是新版CollisionImpacts的自然接触标尺；不要用旧1000／2600一类数值覆盖新标尺。

## 3. 引用怎么接

**Local Config 在玩家车辆 DA 内，不在 RoundedVehicle 蓝图内。**

1. 打开玩家车辆 DA → Details → **Local → Local Config**，选择 Local DA。
2. 打开 BP_RoundedVehiclePawn → **Class Defaults → Vehicle / Configuration**：Configuration Source=`Definition`，Definition=玩家车辆 DA。
3. BP_PassiveTestVehicle 同样绑定木桩 DA；BP_BasicBall 在 **Ball / Configuration** 中绑定球 DA。团队手动 Compile / Save 蓝图。
4. **Project Settings → Game → Vehicle Knockback**：Source=`Definition`，Combat Rules=Combat DA。当前引用已保存于项目 DefaultGame.ini。
5. 地图中的实例可能有 Source/Definition 覆盖；更换资产后检查实例是否仍指向原 DA。共享同一 DA 的对象会一起使用修改后的基础参数，独立测试差异请复制 DA 并绑定到相应对象。

本次8秒免疫／20秒冷却属于GE配置，不在这五份DA内：GA引用免疫GE和冷却GE，PlayerState.DefaultAbilities授予GA，GameMode选择PlayerState类。Local.PrimarySkillAction只连接输入动作；它不存技能持续时间或选择GA。详细手工步骤见[10-E第5节](../Combat/MVPB_Phase10E_Implementation_CN_v0.1.md#5-以后改时长换技能时怎么配置)。

## 4. 修改后怎么生效

填写并核对完整配置后勾选每份 DA 的 **Ready For Use**，保存；该标记不替代校验。右键资产执行 **Validate Assets**，确认有效，再退出并重新启动 PIE。当前不支持运行中热重载配置。

Definition 模式下，本文迁移参数统一在 DA 调节；蓝图旧原生字段变灰，标准组件 Details 中仍能编辑的同名设置也可能在运行前被 DA 覆盖。Legacy 仅供迁移兼容，不能假定切回它就自动恢复原模板。

缺失引用、Ready 未勾选或参数非法时，会阻止相关实例正常参与玩法并在 Output Log 报错，不回退默认值。先检查引用、Ready、Validate 结果与关卡实例覆盖。

调参后记录 **DA 名称、字段、修改前后值、测试场景与结果**。提交配置时同步相关 DA、手动改过的蓝图/地图及项目设置；基础配置与本局 HP、球权、速度等运行状态分开保存。
