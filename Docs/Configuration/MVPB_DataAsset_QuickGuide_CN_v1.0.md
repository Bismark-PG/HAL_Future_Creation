# MVP-B Data Asset 配置速查 v1.0

适用：当前车辆、普通球与测试场景。更新：2026-09-18。团队已配置 DA，用户反馈初步测试暂未发现功能问题；现有数值仍是可继续调整的测试参数。

## 1. 去哪里改

当前五份资产位于 Content Browser 的 **Content → Data → MVP_B**（`/Game/Data/MVP_B`）。双击资产，在 Details 中修改参数。

| 资产 | 类型 | 配置内容 |
|---|---|---|
| DA_Vehicle_Player_TestBaseline_v1 | VehicleDefinition | 玩家驾驶、漂移、控球、发射/后坐、脱球、初始 HP、物理与外观 |
| DA_Vehicle_Passive_TestBaseline_v1 | VehicleDefinition | 木桩车的 Box 尺寸、物理、初始 HP 与外观；不消费驾驶、控球或 Local 配置 |
| DA_Ball_Normal_TestBaseline_v1 | BallDefinition | 球半径、物理、攻击结束条件、拾取锁定、伤害、击退倍率与外观 |
| DA_Local_TestBaseline_v1 | VehicleLocalConfig | 输入资产引用与跟随相机；驾驶手感参数在玩家车辆 DA 中 |
| DA_Combat_TestBaseline_v1 | CombatRulesDefinition | 所有普通球共用的击退分档阈值和各档结果 |

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
| 撞击脱球、重捕等待 | 玩家 → BallControl.ForcedRelease | 脱球碰撞阈值、锁定时间、脱球水平/向上冲量 |
| 初始生命值 | 玩家或木桩 → Health.Health.MaxHP | 下次启动玩法的初始/最大 HP，不直接编辑当前 HP |
| 球大小、质量与阻尼 | 球 → Radius / Physics.Body | Radius 为碰撞半径；质量使用 Mass Override，阻尼影响物理运动 |
| 球何时结束攻击、恢复可拾取 | 球 → Gameplay.State / Acquisition | 低速阈值/持续时间/检查间隔，以及结束攻击后的拾取锁定 |
| 球伤害、击退倍率 | 球 → Gameplay.Damage | VehicleHitDamage 管伤害；KnockbackStrengthMultiplier 缩放击退强度后参与分档，不是伤害倍率 |
| 三档击退结果 | Combat → BallImpacts | Medium/HeavyImpactSpeedThreshold 与 Light/Medium/HeavyTier；Heavy 阈值必须 ≥ Medium |
| 相机距离、角度、跟随延迟 | Local → Camera | TargetArmLength、BoomTransform.Rotation.Pitch、CameraLagSpeed / bEnableCameraLag |
| 输入映射、按键 | Local → Input 引用；打开所引用的 IMC | Local 指定 IMC/五个 Input Action；具体键位在 IMC 中修改 |
| 模型、材质与位置 | 车辆或球 → Visual | Mesh、Materials、RelativeTransform；外观尺寸与物理碰撞尺寸分别配置 |

距离通常为 **cm**，速度 **cm/s**，时间 **s**，质量 **kg**；角度、角速度及高级物理字段以各字段单位提示为准。调参前保留已有值，每次优先改一组参数以方便比较。

## 3. 引用怎么接

**Local Config 在玩家车辆 DA 内，不在 RoundedVehicle 蓝图内。**

1. 打开玩家车辆 DA → Details → **Local → Local Config**，选择 Local DA。
2. 打开 BP_RoundedVehiclePawn → **Class Defaults → Vehicle / Configuration**：Configuration Source=`Definition`，Definition=玩家车辆 DA。
3. BP_PassiveTestVehicle 同样绑定木桩 DA；BP_BasicBall 在 **Ball / Configuration** 中绑定球 DA。团队手动 Compile / Save 蓝图。
4. **Project Settings → Game → Vehicle Knockback**：Source=`Definition`，Combat Rules=Combat DA。当前引用已保存于项目 DefaultGame.ini。
5. 地图中的实例可能有 Source/Definition 覆盖；更换资产后检查实例是否仍指向原 DA。共享同一 DA 的对象会一起使用修改后的基础参数，独立测试差异请复制 DA 并绑定到相应对象。

## 4. 修改后怎么生效

填写并核对完整配置后勾选每份 DA 的 **Ready For Use**，保存；该标记不替代校验。右键资产执行 **Validate Assets**，确认有效，再退出并重新启动 PIE。当前不支持运行中热重载配置。

Definition 模式下，本文迁移参数统一在 DA 调节；蓝图旧原生字段变灰，标准组件 Details 中仍能编辑的同名设置也可能在运行前被 DA 覆盖。Legacy 仅供迁移兼容，不能假定切回它就自动恢复原模板。

缺失引用、Ready 未勾选或参数非法时，会阻止相关实例正常参与玩法并在 Output Log 报错，不回退默认值。先检查引用、Ready、Validate 结果与关卡实例覆盖。

调参后记录 **DA 名称、字段、修改前后值、测试场景与结果**。提交配置时同步相关 DA、手动改过的蓝图/地图及项目设置；基础配置与本局 HP、球权、速度等运行状态分开保存。
