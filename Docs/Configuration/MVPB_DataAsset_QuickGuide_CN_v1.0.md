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
| 静态外观、材质与位置 | 车辆或球 → Visual | 车辆为静态回退外观，球／被动测试车仍作为主要外观；骨骼车型正常显示时该静态组件隐藏，详见第5节 |

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

## 5. 骨骼车型仍需填写 Visual 吗？（2026-10-29源码核对）

核对基点`main/98b1a8d`。`DA_Vehicle_Player_TestBaseline_v1.Visual`仍会应用到Pawn的静态`VisualMesh`，作为骨骼表现不可用时的回退。正常可用的`Presentation`由车型表现DA提供Skeletal Mesh／AnimBP／模型变换，表现组件会隐藏静态Visual；结束／重配时恢复原可见性。Visual的材质、变换和显隐不是骨骼模型的配置入口。

本节最初检查发现Definition无条件要求静态Visual，用户随后同意调整，现已写入按可用表现路径校验的实现：**驾驶车辆有有效且Ready的骨骼Presentation时，可以将Visual.Mesh留空；骨骼配置不可用时，必须有有效静态Visual。** 两条都不可用时明确拒绝初始化，错误同时说明静态与骨骼原因。只填写Presentation引用或勾Ready不能代替Mesh、Skeleton／AnimClass、骨骼层级、四轮半径等完整校验。被动Box测试车与球仍需要各自的静态Visual，不因填了一个它们不会消费的骨骼配置而放行。

有效静态回退仍完整应用其材质、变换及显隐。无效／缺失静态回退但骨骼路径有效时，Pawn只在运行／预览组件上清除旧静态网格、重置该无用组件变换并隐藏，不将无效参数送入组件，不修改保存的DA，也不改变碰撞体或骨骼模型变换。Ready、物理、战斗、Local等其他门槛保持。

| 检查的入口 | 当前依赖 |
|---|---|
| Validate Assets／车辆运行初始化 | 驾驶车要求有效骨骼或有效静态之一；被动Box／球保持静态要求 |
| 正常骨骼显示／轮转／倾斜／视觉悬挂 | 使用Presentation DA；有效时隐藏静态Visual |
| 实际Chaos碰撞 | 使用独立的CollisionMesh／Physics；VisualMesh始终NoCollision、不模拟 |
| 视觉高度校准工具 | 使用CollisionMesh凸包与Presentation骨骼几何，不以Visual.Mesh作高度或轮位基准 |
| 碰撞生成工具 | 使用生成形状参数及输出校验，不要求Visual.Mesh |
| 车型资产生成工具 | 可先创建／保存空静态Visual、Ready=false的资产；正式启用须有可用表现路径，骨骼有效时不必补占位Static Mesh |
| BuildAndLaunch编译入口 | 没有读取Visual.Mesh或按其名称验收；不能将编译等待归因于这个字段 |

不要求Visual.Mesh必须等于`SM_CarCollisionProxy`，任意适当且有效的Static Mesh均可；Materials可为空，Overlay Material可为None。更换这个字段不等于更换碰撞体。另有旧健康／配置测试直接把`/Game/Meshes/SM_CarCollisionProxy`作为碰撞夹具加载，因此调整DA字段与删除该资源文件是两件事；本次未做资产引用导出，不能据此宣称删除资源安全。

本次构建／回归与手工步骤见下节。上一次“仅检查、未改代码”保留在项目状态的历史记录；本节规则以当前实现为准，不把旧静态强制依赖继续作为验收门槛。

## 6. 静态回退可选：团队配置与验证（2026-10-29）

1. 同步修改后的C++，完整编译Editor Target，再打开项目。
2. 打开实际车型Vehicle Definition，沿`Visual → Presentation`进入车型表现DA。完成Ready、Mesh／Skeleton、匹配AnimBP、四轮骨骼／实际半径及模型对齐，先对该表现DA执行Validate Assets。
3. 回到Vehicle Definition，若不需要静态备用外观，将`Visual → Visual → Mesh`清空为None；其余未使用的静态字段不用重新填写。**保留独立的Collision Mesh和Physics配置**。需要备用外观时可继续保留原有效静态Visual。
4. 保持已核对的车辆Ready、Local、Movement／Combat等配置，保存并Validate Assets。骨骼有效的驾驶车应可通过；没有有效骨骼时不能同时删掉静态路径。不要用切Legacy来绕过检查。
5. 重新启动测试，确认实际车模与动画、驾驶／碰撞、控球／发射正常。两端使用同一代码与保存资产；车型生成器不再要求为骨骼车型额外填占位Static Mesh。

本轮不由代理清空任何保存DA或编辑BP／地图。校验和组件处理仅在既有配置应用／初始化时生效，不新增局内配置热重载；骨骼运行中人为停用且未配置回退，不承诺自动生成替代模型。

本机UE5.8.2 Editor／Game完整构建成功，普通**96/96**、车型生成器**3/3**，真实NetTest表现回归Host／Client各**1/1**，均0测试警告／失败；1172保存资产与用户关闭后的基线逐份哈希一致。无静态路径与失败门槛由原生临时配置／真实保存骨骼初始化验证；双端使用现有保存车辆，不代表已保存None配置的真人画面验收。详细日志／报告与210份源码／配置哈希见[验证摘要](MVPB_Optional_Static_Visual_Verification_20261029.json)。原94项保持前步历史；本轮未提交／Push，等待团队按上述步骤实配／初测。
