# MVP-B 第一阶段：Unreal Editor 手动迁移操作 v1.0

日期：2026-09-18。执行人员：团队成员。当前状态：C++ 类型和迁移入口已实现，实际资产仍保持 Legacy；本文不代表已完成蓝图切换或 PIE 验收。

后续更新（2026-09-18）：上句为施工交付时点。团队随后配置 DA，并反馈初步测试暂未发现功能问题；实际五份资产位于 `/Game/Data/MVP_B`，与本文建议目录不同。团队日常调参使用 [Data Asset 配置速查](Docs/Configuration/MVPB_DataAsset_QuickGuide_CN_v1.0.md)，本文保留为详细迁移与专项验收操作参考。

## 1. 依据与准备

实际保存基线是 `Docs/Baselines/MVP_A_Saved_Test_20260918T030251Z.json`，采样引擎 UE 5.8.2。下表来自该文件，不能用新建资产默认值或旧交接恢复表替代。参数只是测试基线，后续可继续调节。

1. 保存当前工作并关闭 Unreal Editor。使用完整 Editor Target 编译后的模块重新启动项目；新增反射类型、结构和组件字段不使用 Live Coding 迁移。
2. 在源控或副本中保留原始 BP_RoundedVehiclePawn、BP_BasicBall、BP_PassiveTestVehicle 和 Test 地图。代理没有改动这些资产。Definition 的编辑器预览会改变瞬态组件显示，团队 Compile/Save 后可能保存相应模板；回退需还原已留档的组件设置，不能只切回 Legacy 就假定已经恢复。
3. 打开 `/Game/Maps/Test`。允许团队自行复制到 MVPB-BTEST，但先记录新地图完整路径。World Settings → GameMode Override 应为 BP_MVPA_GameMode；打开该 GameMode 的 Class Defaults，Default Pawn Class 应为 BP_RoundedVehiclePawn。
4. 已保存 Test 有一个普通球和一个木桩车，没有预放置圆头车。第一次 PIE 按 F8，检查实际生成玩家车的 Class、控制器/占有和根组件，确认没有多生成车辆。
5. 先用 Legacy 运行一遍并记录手感及有效值，作为本次迁移的比较对象。快照不包含 PIE 最终有效值，也不读取 Event Graph；若构造脚本修改了这些设置，应先记录差异及其处理决定。

## 2. 创建五个配置实例

在 Content Browser 新建 `/Game/Data/MVPB/Baseline` 文件夹。右键空白 → Miscellaneous → Data Asset，搜索下列原生类。创建普通 Data Asset 实例，不是 Blueprint Class，不更换现有蓝图父类。

| 实例建议名 | 原生类型 | 用途 |
|---|---|---|
| DA_Local_TestBaseline_v1 | VehicleLocalConfig | 当前输入、固定跟随相机 |
| DA_Vehicle_Player_TestBaseline_v1 | VehicleDefinition | 圆头玩家车，ConvexMesh |
| DA_Vehicle_Passive_TestBaseline_v1 | VehicleDefinition | 木桩车，Box |
| DA_Ball_Normal_TestBaseline_v1 | BallDefinition | 普通球 |
| DA_Combat_TestBaseline_v1 | CombatRulesDefinition | 现有球命中击退规则 |

新实例的 Ready For Use 默认为 False。先填完并核对基线，再勾选；该标记是人工已比较的声明，不能代替数值/引用校验。Details 可能以带空格的友好名称显示属性，本文用 C++ 名方便搜索。Definition 引用的 Details 编辑入口在 Definition 模式才启用，因此先完成资产，再在蓝图一次性切换及指定引用，未指定前不要运行 PIE。

## 3. 输入与相机

打开 DA_Local_TestBaseline_v1，填写 Input 六项引用。保留现有 IMC，不重建 Input Action 或修改按键。

| 字段 | 已保存引用 |
|---|---|
| DefaultMappingContext | /Game/Input/Vehicle/IMC_Vehicle.IMC_Vehicle |
| SteeringAction | /Game/Input/Vehicle/IA_VehicleSteering.IA_VehicleSteering |
| ThrottleAction | /Game/Input/Vehicle/IA_VehicleThrottle.IA_VehicleThrottle |
| BrakeAction | /Game/Input/Vehicle/IA_VehicleBrake.IA_VehicleBrake |
| HandbrakeAction | /Game/Input/Vehicle/IA_VehicleHandbrake.IA_VehicleHandbrake |
| LaunchAction | /Game/Input/Vehicle/IA_VehicleLaunch.IA_VehicleLaunch |

实际发射键为 LB（Gamepad_LeftShoulder）/鼠标左键；转向 LeftX / A、D，油门 B / W，刹车 A / S，手刹 RB / Space。这里只迁移资产引用，实际键位仍由上述 IMC 决定。

Camera 从旧蓝图 CameraBoom / TopDownCamera 迁移。RelativeTransform 展开 Location、Rotation、Scale 按表填写；Rotator 的 Pitch 是俯仰，勿把 -60 填到 Yaw。

| 新 Camera 字段 | 旧组件字段 | 保存值 |
|---|---|---|
| BoomTransform.Location | CameraBoom.relative_location | (X=0.000000,Y=0.000000,Z=0.000000) |
| BoomTransform.Rotation | CameraBoom.relative_rotation | (Pitch=-60.000000,Yaw=0.000000,Roll=-0.000000) |
| BoomTransform.Scale | CameraBoom.relative_scale3d | (X=1.000000,Y=1.000000,Z=1.000000) |
| bAbsoluteLocation | CameraBoom.absolute_location | False |
| bAbsoluteRotation | CameraBoom.absolute_rotation | True |
| bAbsoluteScale | CameraBoom.absolute_scale | False |
| TargetArmLength | CameraBoom.target_arm_length | 1200 |
| SocketOffset | CameraBoom.socket_offset | (X=0.000000,Y=0.000000,Z=0.000000) |
| TargetOffset | CameraBoom.target_offset | (X=0.000000,Y=0.000000,Z=0.000000) |
| bDoCollisionTest | CameraBoom.do_collision_test | False |
| ProbeSize | CameraBoom.probe_size | 12 |
| ProbeChannel | CameraBoom.probe_channel | <CollisionChannel.ECC_CAMERA: 4> |
| bUsePawnControlRotation | CameraBoom.use_pawn_control_rotation | False |
| bInheritPitch | CameraBoom.inherit_pitch | True |
| bInheritYaw | CameraBoom.inherit_yaw | True |
| bInheritRoll | CameraBoom.inherit_roll | True |
| bEnableCameraLag | CameraBoom.enable_camera_lag | True |
| bEnableCameraRotationLag | CameraBoom.enable_camera_rotation_lag | False |
| CameraLagSpeed | CameraBoom.camera_lag_speed | 8 |
| CameraRotationLagSpeed | CameraBoom.camera_rotation_lag_speed | 10 |
| CameraLagMaxDistance | CameraBoom.camera_lag_max_distance | 0 |
| bUseCameraLagSubstepping | CameraBoom.use_camera_lag_substepping | True |
| CameraLagMaxTimeStep | CameraBoom.camera_lag_max_time_step | 0.0166666675 |
| bClampToMaxPhysicsDeltaTime | CameraBoom.clamp_to_max_physics_delta_time | False |
| bDrawDebugLagMarkers | CameraBoom.draw_debug_lag_markers | False |
| CameraTransform.Location | TopDownCamera.relative_location | (X=0.000000,Y=0.000000,Z=0.000000) |
| CameraTransform.Rotation | TopDownCamera.relative_rotation | (Pitch=0.000000,Yaw=0.000000,Roll=0.000000) |
| CameraTransform.Scale | TopDownCamera.relative_scale3d | (X=1.000000,Y=1.000000,Z=1.000000) |
| FieldOfView | TopDownCamera.field_of_view | 90 |
| AspectRatio | TopDownCamera.aspect_ratio | 1.77777803 |
| bConstrainAspectRatio | TopDownCamera.constrain_aspect_ratio | False |
| PostProcessBlendWeight | TopDownCamera.post_process_blend_weight | 1 |

Camera.PostProcess 对应 TopDownCamera.PostProcessSettings：展开核对全部 override 开关和 Weighted Blendables。快照导出文本保存在 JSON；当前没有启用的 override 或 Blendables，不自行新增后期效果。Camera 保持透视，Use Pawn Control Rotation=False；未迁移的引擎高级表现属性仍保留在原组件，不声称所有引擎字段已抽离。

填完勾选 Local 的 Ready For Use，Save。后续先将玩家 VehicleDefinition.LocalConfig 指向此资产再勾选玩家的 Ready。

## 4. 驾驶、控球、HP 和球玩法逐字段表

打开圆头车蓝图，仅查看 Components 的 ArcadeMovement、BallControl、Health 和 Class Defaults；对照填写玩家 DA。下表给出全部现有原生可调玩法字段的迁移位置。球字段从 BP_BasicBall 的 Class Defaults 填入球 DA；木桩只消费 Health，不添加驾驶/控球/LocalConfig。

| 旧对象.字段 | 新 DA 字段 | 实际保存值 |
|---|---|---|
| ArcadeVehicleMovementComponent.ForwardAcceleration | VehicleDefinition.Movement.Drive.ForwardAcceleration | 1800 |
| ArcadeVehicleMovementComponent.ReverseAcceleration | VehicleDefinition.Movement.Drive.ReverseAcceleration | 1200 |
| ArcadeVehicleMovementComponent.BrakeDeceleration | VehicleDefinition.Movement.Drive.BrakeDeceleration | 2800 |
| ArcadeVehicleMovementComponent.MaxForwardSpeed | VehicleDefinition.Movement.Drive.MaxForwardSpeed | 2400 |
| ArcadeVehicleMovementComponent.MaxReverseSpeed | VehicleDefinition.Movement.Drive.MaxReverseSpeed | 900 |
| ArcadeVehicleMovementComponent.ReverseEngageSpeed | VehicleDefinition.Movement.Drive.ReverseEngageSpeed | 80 |
| ArcadeVehicleMovementComponent.CoastingDragRate | VehicleDefinition.Movement.Drive.CoastingDragRate | 0.349999994 |
| ArcadeVehicleMovementComponent.OverspeedCorrectionRate | VehicleDefinition.Movement.Drive.OverspeedCorrectionRate | 3 |
| ArcadeVehicleMovementComponent.LateralGripRate | VehicleDefinition.Movement.Grip.LateralGripRate | 12 |
| ArcadeVehicleMovementComponent.HandbrakeGripRate | VehicleDefinition.Movement.Grip.HandbrakeGripRate | 0.200000003 |
| ArcadeVehicleMovementComponent.MaxLateralGripAcceleration | VehicleDefinition.Movement.Grip.MaxLateralGripAcceleration | 2800 |
| ArcadeVehicleMovementComponent.SteeringAngularAcceleration | VehicleDefinition.Movement.Steering.SteeringAngularAcceleration | 12 |
| ArcadeVehicleMovementComponent.FullSteeringSpeed | VehicleDefinition.Movement.Steering.FullSteeringSpeed | 400 |
| ArcadeVehicleMovementComponent.HighSpeedSteeringStart | VehicleDefinition.Movement.Steering.HighSpeedSteeringStart | 1600 |
| ArcadeVehicleMovementComponent.HighSpeedSteeringScale | VehicleDefinition.Movement.Steering.HighSpeedSteeringScale | 0.800000012 |
| ArcadeVehicleMovementComponent.HandbrakeSteeringMultiplier | VehicleDefinition.Movement.Steering.HandbrakeSteeringMultiplier | 1.79999995 |
| ArcadeVehicleMovementComponent.HandbrakeMinimumSteeringAuthority | VehicleDefinition.Movement.Steering.HandbrakeMinimumSteeringAuthority | 0.5 |
| ArcadeVehicleMovementComponent.HandbrakeReverseSteeringSpeed | VehicleDefinition.Movement.Steering.HandbrakeReverseSteeringSpeed | 150 |
| ArcadeVehicleMovementComponent.YawDampingRate | VehicleDefinition.Movement.Steering.YawDampingRate | 1.60000002 |
| ArcadeVehicleMovementComponent.HandbrakeYawDampingRate | VehicleDefinition.Movement.Steering.HandbrakeYawDampingRate | 0.300000012 |
| ArcadeVehicleMovementComponent.bEnableWallEscape | VehicleDefinition.Movement.WallEscape.bEnableWallEscape | True |
| ArcadeVehicleMovementComponent.WallEscapeEnterSpeed | VehicleDefinition.Movement.WallEscape.WallEscapeEnterSpeed | 300 |
| ArcadeVehicleMovementComponent.WallEscapeExitSpeed | VehicleDefinition.Movement.WallEscape.WallEscapeExitSpeed | 450 |
| ArcadeVehicleMovementComponent.WallEscapeProbeDistance | VehicleDefinition.Movement.WallEscape.WallEscapeProbeDistance | 15 |
| ArcadeVehicleMovementComponent.WallEscapeMinimumSteering | VehicleDefinition.Movement.WallEscape.WallEscapeMinimumSteering | 0.200000003 |
| ArcadeVehicleMovementComponent.WallEscapeIntoWallThrottleScale | VehicleDefinition.Movement.WallEscape.WallEscapeIntoWallThrottleScale | 0.0500000007 |
| ArcadeVehicleMovementComponent.WallEscapeTargetYawRate | VehicleDefinition.Movement.WallEscape.WallEscapeTargetYawRate | 2.5999999 |
| ArcadeVehicleMovementComponent.WallEscapeYawResponseRate | VehicleDefinition.Movement.WallEscape.WallEscapeYawResponseRate | 10 |
| ArcadeVehicleMovementComponent.WallEscapeMaxYawAcceleration | VehicleDefinition.Movement.WallEscape.WallEscapeMaxYawAcceleration | 24 |
| ArcadeVehicleMovementComponent.WallEscapeBlendInTime | VehicleDefinition.Movement.WallEscape.WallEscapeBlendInTime | 0.0799999982 |
| ArcadeVehicleMovementComponent.WallEscapeBlendOutTime | VehicleDefinition.Movement.WallEscape.WallEscapeBlendOutTime | 0.200000003 |
| ArcadeVehicleMovementComponent.bDrawWallEscapeDebug | VehicleDefinition.Movement.Debug.bDrawWallEscapeDebug | False |
| ArcadeVehicleMovementComponent.GroundTraceExtraDistance | VehicleDefinition.Movement.Ground.GroundTraceExtraDistance | 30 |
| ArcadeVehicleMovementComponent.GroundTraceChannel | VehicleDefinition.Movement.Ground.GroundTraceChannel | <CollisionChannel.ECC_VISIBILITY: 3> |
| ArcadeVehicleMovementComponent.bDrawGroundDebug | VehicleDefinition.Movement.Debug.bDrawGroundDebug | True |
| BallControlComponent.AcquisitionRadius | VehicleDefinition.BallControl.Acquisition.AcquisitionRadius | 350 |
| BallControlComponent.AcquisitionCheckInterval | VehicleDefinition.BallControl.Acquisition.AcquisitionCheckInterval | 0.0500000007 |
| BallControlComponent.LineOfSightTraceChannel | VehicleDefinition.BallControl.Acquisition.LineOfSightTraceChannel | <CollisionChannel.ECC_VISIBILITY: 3> |
| BallControlComponent.MaxControlledDistance | VehicleDefinition.BallControl.Control.MaxControlledDistance | 300 |
| BallControlComponent.ControlPositionStrength | VehicleDefinition.BallControl.Control.ControlPositionStrength | 35 |
| BallControlComponent.ControlVelocityStrength | VehicleDefinition.BallControl.Control.ControlVelocityStrength | 8 |
| BallControlComponent.ControlMaxForce | VehicleDefinition.BallControl.Control.ControlMaxForce | 40000 |
| BallControlComponent.LaunchSpeedIncrement | VehicleDefinition.BallControl.Launch.LaunchSpeedIncrement | 2200 |
| BallControlComponent.BaseRecoilDeltaSpeed | VehicleDefinition.BallControl.Launch.BaseRecoilDeltaSpeed | 180 |
| BallControlComponent.MovingRecoilFraction | VehicleDefinition.BallControl.Launch.MovingRecoilFraction | 0.150000006 |
| BallControlComponent.MaxRecoilDeltaSpeed | VehicleDefinition.BallControl.Launch.MaxRecoilDeltaSpeed | 420 |
| BallControlComponent.DropCollisionImpulseThreshold | VehicleDefinition.BallControl.ForcedRelease.DropCollisionImpulseThreshold | 100000 |
| BallControlComponent.DropReacquireLockDuration | VehicleDefinition.BallControl.ForcedRelease.DropReacquireLockDuration | 0.75 |
| BallControlComponent.DropBallImpulse | VehicleDefinition.BallControl.ForcedRelease.DropBallImpulse | 18000 |
| BallControlComponent.DropBallUpwardImpulse | VehicleDefinition.BallControl.ForcedRelease.DropBallUpwardImpulse | 6000 |
| BallControlComponent.bDrawAcquisitionDebug | VehicleDefinition.BallControl.Debug.bDrawAcquisitionDebug | False |
| BasicBallActor.LowSpeedThreshold | BallDefinition.Gameplay.State.LowSpeedThreshold | 250 |
| BasicBallActor.LowSpeedRequiredDuration | BallDefinition.Gameplay.State.LowSpeedRequiredDuration | 0.5 |
| BasicBallActor.LowSpeedCheckInterval | BallDefinition.Gameplay.State.LowSpeedCheckInterval | 0.0500000007 |
| BasicBallActor.PostLaunchPickupLockDuration | BallDefinition.Gameplay.Acquisition.PostLaunchPickupLockDuration | 0.200000003 |
| BasicBallActor.VehicleHitDamage | BallDefinition.Gameplay.Damage.VehicleHitDamage | 25 |
| BasicBallActor.KnockbackStrengthMultiplier | BallDefinition.Gameplay.Damage.KnockbackStrengthMultiplier | 5 |
| BasicBallActor.bLogDamageHits | BallDefinition.Gameplay.Debug.bLogDamageHits | True |
| BasicBallActor.bLogStateChanges | BallDefinition.Gameplay.Debug.bLogStateChanges | True |
| VehicleHealthComponent.MaxHP | VehicleDefinition.Health.Health.MaxHP | 100 |
| VehicleHealthComponent.bLogHealthChanges | VehicleDefinition.Health.Debug.bLogHealthChanges | False |

木桩 HP：

| 字段 | 保存值 |
|---|---|
| MaxHP | 100 |
| bLogHealthChanges | True |

单位和范围以 Details 元数据及 C++ Validate 为准：距离 cm、线速度 cm/s、加速度 cm/s²、时间 s、角速度 rad/s、质量 kg，伤害/HP 为玩法数值。各旧字段注释与范围保留在配置结构中，不凭字段名猜测单位；没有 Units 元数据的倍率、强度保持原定义。

## 5. 物理根、外观、控球点与箭头

玩家 CollisionShape=ConvexMesh，CollisionMesh 指向下表网格；木桩 CollisionShape=Box，BoxExtent=(100,70,35)，仍使用 UBoxComponent；球 Radius=50，仍使用 USphereComponent。根组件不能更名、删除或重建。

每个 DA 展开 Physics → Body，这是 FBodyInstance 的设置模板。逐项核对原 PhysicsRoot/CollisionRoot 的 Physics、Collision、高级物理属性；若界面不显示某字段，右键显示高级属性，或以 JSON 的 body_instance.export_text 对照。不导入运行中的刚体数据。

三者碰撞预设 PhysicsActor、QueryAndPhysics、ObjectType=PhysicsBody；Simulate Physics/Gravity/CCD/Mass Override=True。物理材质 None、COM Nudge=(0,0,0)、Inertia Tensor Scale=(1,1,1)、Mass Scale=1。玩家和球 Simulation Generates Hit Events=True；木桩的实际值 False，必须手动保留。各方向平移/旋转锁均 False，DOFMode=Default。bLockTranslation/bLockRotation 是引擎内部聚合标志，不能误认为三个轴已锁死。

| 类型 | 根网格/形状 | 质量 kg | Linear Damping | Angular Damping | 根缩放 | Hidden In Game | Overlap |
|---|---|---|---|---|---|---|---|
| player | /Game/Meshes/SM_CarCollisionProxy.SM_CarCollisionProxy | 800.000000 | 0.150000 | 0.800000 | (X=1.000000,Y=1.000000,Z=1.000000) | True | True |
| passive | (X=100.000000,Y=70.000000,Z=35.000000) | 800.000000 | 0.150000 | 0.800000 | (X=1.000000,Y=1.000000,Z=1.000000) | True | True |
| ball | 50 | 35.000000 | 1.000000 | 0.100000 | (X=1.000000,Y=1.000000,Z=1.000000) | True | True |

其他 Body 字段全文附在文末；这些也是已保存基线，不将资产种子默认值当成自动迁移。当前 Position/Velocity/Projection Solver Iterations=8/2/1、OverrideIterationCounts=False、MaxAngularVelocity=3600（Override=False）、MaxDepenetrationVelocity=0（Override=False）、StartAwake/AutoWeld/UpdateMassWhenScaleChanges=True、GyroscopicTorque/GenerateWakeEvents=False。

Visual.Mesh/RelativeTransform/可见性按下表填。Materials 是覆盖槽数组：原组件没有覆盖时留空，使用网格默认材质；有覆盖时按原槽号添加，保留空槽位置。Python 快照未可靠暴露所有 OverrideMaterials，必须在旧 VisualMesh Details → Materials 手动逐槽核对，不声称这一项已自动采完。OverlayMaterial=None。表现根始终 NoCollision、Simulate Physics=False。

| DA 类型 | Visual 字段 | 保存值 |
|---|---|---|
| player | Mesh | /Game/Meshes/SM_CarCollisionProxy.SM_CarCollisionProxy |
| player | RelativeTransform.Location | (X=0.000000,Y=0.000000,Z=0.000000) |
| player | RelativeTransform.Rotation | (Pitch=0.000000,Yaw=0.000000,Roll=0.000000) |
| player | RelativeTransform.Scale | (X=1.000000,Y=1.000000,Z=1.000000) |
| player | bVisible | True |
| player | bHiddenInGame | False |
| player | bCastShadow | True |
| player | OverlayMaterial | None |
| passive | Mesh | /Engine/BasicShapes/Cube.Cube |
| passive | RelativeTransform.Location | (X=0.000000,Y=0.000000,Z=0.000000) |
| passive | RelativeTransform.Rotation | (Pitch=0.000000,Yaw=0.000000,Roll=0.000000) |
| passive | RelativeTransform.Scale | (X=1.000000,Y=1.000000,Z=1.000000) |
| passive | bVisible | True |
| passive | bHiddenInGame | False |
| passive | bCastShadow | True |
| passive | OverlayMaterial | None |
| ball | Mesh | /Engine/BasicShapes/Sphere.Sphere |
| ball | RelativeTransform.Location | (X=0.000000,Y=0.000000,Z=0.000000) |
| ball | RelativeTransform.Rotation | (Pitch=0.000000,Yaw=0.000000,Roll=0.000000) |
| ball | RelativeTransform.Scale | (X=1.000000,Y=1.000000,Z=1.000000) |
| ball | bVisible | True |
| ball | bHiddenInGame | False |
| ball | bCastShadow | True |
| ball | OverlayMaterial | None |

玩家 BallControlPoint.Location=(170,0,20)，Rotation=(0,0,0)、Scale=(1,1,1)。ForwardArrowTransform.Location=(110,0,0)，Rotation=(0,0,0)、Scale=(1,1,1)；Color=(R=0,G=255,B=255,A=255)、Size=1.5、Length=80、Visible=True、HiddenInGame=False。其余箭头/视觉高级表现选项保留在现有组件。

## 6. 全局球命中规则

打开 DA_Combat_TestBaseline_v1，展开 BallImpacts；该资产当前仅管理既有球命中，不预建危险区/技能/比赛规则。

| BallImpacts 字段 | 当前有效值 |
|---|---|
| GroundProbeExtraDistance | 20 |
| HeavyImpactSpeedThreshold | 2600 |
| HeavyTier | (HorizontalDeltaSpeed=900.000000,TargetVerticalSpeed=750.000000,AirborneFlipTurns=1.250000) |
| LightTier | (HorizontalDeltaSpeed=250.000000,TargetVerticalSpeed=0.000000,AirborneFlipTurns=0.000000) |
| MaxAirborneAngularSpeed | 8 |
| MediumImpactSpeedThreshold | 1000 |
| MediumTier | (HorizontalDeltaSpeed=600.000000,TargetVerticalSpeed=500.000000,AirborneFlipTurns=1.000000) |

逐项比较后勾选 Combat 的 Ready For Use 并 Save。Physics/玩法/引用全部填完且 Local 已有效后，同样勾选玩家、木桩、球的 Ready For Use，保存五个配置资产。在 Content Browser 选中五个 → 右键 Asset Actions → Validate Assets（也可搜索 Validate），查看 Message Log 的 Data Validation；全部通过才切换。

## 7. 分步绑定、切换与实例检查

1. BP_RoundedVehiclePawn → Class Defaults → Vehicle / Configuration：Configuration Source=Definition，Definition=玩家 DA；Compile、Save。不要修改 Event Graph，不把规则迁入图表。
2. World Settings 及 BP_MVPA_GameMode 的 Default Pawn Class 保持原引用。PIE → F8 选择实际玩家车，Configuration Valid=True；核对根网格/实际质量 800、阻尼 .15/.8、61 项中玩家有效字段、控球点、相机及输入。DA 中未消费的木桩驾驶字段无需填写为正式类型。
3. 本步做驾驶、刹车/倒车、低高速手刹、侧撞/墙角、腾空/连续碰撞、控球/发射/后坐/脱球/锁定回归。球和全局规则此时仍为 Legacy，有助于定位差异。结果记录后再下一步。
4. BP_BasicBall → Class Defaults → Ball / Configuration：Source=Definition，Definition=球 DA，Compile/Save。Test 的该球实例 Details 搜索 Configuration Source / Definition，检查是否有实例覆盖；明确清除覆盖或设为相同 DA 后保存地图。PIE 核对 Radius=50、质量35、LinearDamping=1、击退倍率5及球状态转换。
5. BP_PassiveTestVehicle 同样绑定木桩 DA 并切换 Definition，Compile/Save；检查 Test 该实例覆盖。确认 Box 根、HP、质量/阻尼和外观不变，仍不会主动驾驶或控球。
6. Project Settings → Game → Vehicle Knockback：Source=Definition，Combat Rules=Combat DA。将设置保存到项目默认配置；本步骤将产生团队授权的 DefaultGame.ini 差异。关闭后重开确认引用确实已持久化，不能只留在 Saved/Config。旧阈值/档位入口应变灰且不参与 Definition 结果。
7. 再次完整 PIE 比较三档命中、HP、位移/翻转；各蓝图原生玩法字段在 Definition 下变灰。引擎组件 Physics/Mesh 的标准 Details 仍可能可编辑，Definition 会在运行前接管本文覆盖的设置；以后在 DA 调节这些基础参数，不从旧模板形成另一来源。未抽离的渲染细节仍属于表现配置。
8. 将每个关卡实例的 Source、Definition、原覆盖和处理结果留档。快照能证明保存值，运行中有效值与手感必须人工记录。不能把当前“代理编译通过”记作“手动迁移已验收”。

## 8. 错误、回退与验收记录

缺失引用、Ready=False、NaN/越界、无简单凸包、根类型不匹配或全局 Combat 无效：Output Log 会出现 invalid ... / No default fallback。实例停止物理碰撞/模拟及相关输入/伤害，不回退种子值。解决引用/数值后退出并重开 PIE；本阶段不支持运行中重载 DA。

若需回退，团队恢复留档的蓝图/关卡组件及覆盖、Project Settings 的 Source=Legacy；重新完整启动/PIE。资产切换与编辑器预览不能保证旧序列化模板自动回滚。

| 验收项目 | 记录 |
|---|---|
| 日期 / 引擎 / Git版本 / 执行人 | 待团队填写 |
| 地图完整路径 / GameMode / 实际Pawn / 占有 | 待团队填写 |
| 五个 DA 完整路径 / Ready / Validate结果 | 待团队填写 |
| 模板 / 场景实例差异及处理决定 | 待团队填写 |
| PIE 实际参数逐项比较 / Materials槽 | 待团队填写 |
| 驾驶/控球/发射/后坐/脱球/锁定/命中回归 | 待团队填写 |
| 低速/高速/侧撞/墙角/腾空/连续碰撞 | 待团队填写 |
| 持有者销毁 / 重复发射 / 多候选球确定性 | 待团队填写 |
| 实际结果 / 剩余问题 / 回退记录 | 待团队填写 |

历史普通漂移退出顿挫仍为搁置问题；危险区和模型/Rig 待后续阶段。本次不引入完整联机、GAS、技能或比赛框架。

## 附录：物理根 FBodyInstance 已保存导出文本

仅用于团队对照模板字段，不能向共享配置复制活跃刚体实例。内容来自原始已保存快照，非 C++ 默认值。

### player

```text
(PositionSolverIterationCount=8,VelocitySolverIterationCount=2,ProjectionSolverIterationCount=1,ObjectType=ECC_PhysicsBody,CollisionEnabled=QueryAndPhysics,SleepFamily=Normal,DOFMode=Default,bUseCCD=True,bUseMACD=False,bAllowPartialIslandSleep=True,bIgnoreAnalyticCollisions=False,bNotifyRigidBodyCollision=True,bSmoothEdgeCollisions=False,bLockTranslation=True,bLockRotation=True,bLockXTranslation=False,bLockYTranslation=False,bLockZTranslation=False,bLockXRotation=False,bLockYRotation=False,bLockZRotation=False,bOverrideMaxAngularVelocity=False,bOverrideMaxDepenetrationVelocity=False,bOverrideWalkableSlopeOnInstance=False,bOverrideIterationCounts=False,bInterpolateWhenSubStepping=True,bInertiaConditioning=True,bOneWayInteraction=False,bOverrideSolverAsyncDeltaTime=False,SolverAsyncDeltaTime=0.016667,CollisionProfileName="PhysicsActor",CollisionResponses=(ResponseArray=),MaxDepenetrationVelocity=0.000000,MassInKgOverride=800.000000,LinearDamping=0.150000,AngularDamping=0.800000,CustomDOFPlaneNormal=(X=0.000000,Y=0.000000,Z=0.000000),COMNudge=(X=0.000000,Y=0.000000,Z=0.000000),MassScale=1.000000,GravityGroupIndex=0,InertiaTensorScale=(X=1.000000,Y=1.000000,Z=1.000000),WalkableSlopeOverride=(WalkableSlopeBehavior=WalkableSlope_Default,WalkableSlopeAngle=0.000000),PhysMaterialOverride=None,MaxAngularVelocity=3600.000000,CustomSleepThresholdMultiplier=1.000000,StabilizationThresholdMultiplier=1.000000,PhysicsBlendWeight=1.000000,bSimulatePhysics=True,bOverrideMass=True,bEnableGravity=True,bUpdateKinematicFromSimulation=False,bGyroscopicTorqueEnabled=False,bAutoWeld=True,bStartAwake=True,bGenerateWakeEvents=False,bUpdateMassWhenScaleChanges=True)
```
### passive

```text
(PositionSolverIterationCount=8,VelocitySolverIterationCount=2,ProjectionSolverIterationCount=1,ObjectType=ECC_PhysicsBody,CollisionEnabled=QueryAndPhysics,SleepFamily=Normal,DOFMode=Default,bUseCCD=True,bUseMACD=False,bAllowPartialIslandSleep=True,bIgnoreAnalyticCollisions=False,bNotifyRigidBodyCollision=False,bSmoothEdgeCollisions=False,bLockTranslation=True,bLockRotation=True,bLockXTranslation=False,bLockYTranslation=False,bLockZTranslation=False,bLockXRotation=False,bLockYRotation=False,bLockZRotation=False,bOverrideMaxAngularVelocity=False,bOverrideMaxDepenetrationVelocity=False,bOverrideWalkableSlopeOnInstance=False,bOverrideIterationCounts=False,bInterpolateWhenSubStepping=True,bInertiaConditioning=True,bOneWayInteraction=False,bOverrideSolverAsyncDeltaTime=False,SolverAsyncDeltaTime=0.016667,CollisionProfileName="PhysicsActor",CollisionResponses=(ResponseArray=),MaxDepenetrationVelocity=0.000000,MassInKgOverride=800.000000,LinearDamping=0.150000,AngularDamping=0.800000,CustomDOFPlaneNormal=(X=0.000000,Y=0.000000,Z=0.000000),COMNudge=(X=0.000000,Y=0.000000,Z=0.000000),MassScale=1.000000,GravityGroupIndex=0,InertiaTensorScale=(X=1.000000,Y=1.000000,Z=1.000000),WalkableSlopeOverride=(WalkableSlopeBehavior=WalkableSlope_Default,WalkableSlopeAngle=0.000000),PhysMaterialOverride=None,MaxAngularVelocity=3600.000000,CustomSleepThresholdMultiplier=1.000000,StabilizationThresholdMultiplier=1.000000,PhysicsBlendWeight=1.000000,bSimulatePhysics=True,bOverrideMass=True,bEnableGravity=True,bUpdateKinematicFromSimulation=False,bGyroscopicTorqueEnabled=False,bAutoWeld=True,bStartAwake=True,bGenerateWakeEvents=False,bUpdateMassWhenScaleChanges=True)
```
### ball

```text
(PositionSolverIterationCount=8,VelocitySolverIterationCount=2,ProjectionSolverIterationCount=1,ObjectType=ECC_PhysicsBody,CollisionEnabled=QueryAndPhysics,SleepFamily=Normal,DOFMode=Default,bUseCCD=True,bUseMACD=False,bAllowPartialIslandSleep=True,bIgnoreAnalyticCollisions=False,bNotifyRigidBodyCollision=True,bSmoothEdgeCollisions=False,bLockTranslation=True,bLockRotation=True,bLockXTranslation=False,bLockYTranslation=False,bLockZTranslation=False,bLockXRotation=False,bLockYRotation=False,bLockZRotation=False,bOverrideMaxAngularVelocity=False,bOverrideMaxDepenetrationVelocity=False,bOverrideWalkableSlopeOnInstance=False,bOverrideIterationCounts=False,bInterpolateWhenSubStepping=True,bInertiaConditioning=True,bOneWayInteraction=False,bOverrideSolverAsyncDeltaTime=False,SolverAsyncDeltaTime=0.016667,CollisionProfileName="PhysicsActor",CollisionResponses=(ResponseArray=),MaxDepenetrationVelocity=0.000000,MassInKgOverride=35.000000,LinearDamping=1.000000,AngularDamping=0.100000,CustomDOFPlaneNormal=(X=0.000000,Y=0.000000,Z=0.000000),COMNudge=(X=0.000000,Y=0.000000,Z=0.000000),MassScale=1.000000,GravityGroupIndex=0,InertiaTensorScale=(X=1.000000,Y=1.000000,Z=1.000000),WalkableSlopeOverride=(WalkableSlopeBehavior=WalkableSlope_Default,WalkableSlopeAngle=0.000000),PhysMaterialOverride=None,MaxAngularVelocity=3600.000000,CustomSleepThresholdMultiplier=1.000000,StabilizationThresholdMultiplier=1.000000,PhysicsBlendWeight=1.000000,bSimulatePhysics=True,bOverrideMass=True,bEnableGravity=True,bUpdateKinematicFromSimulation=False,bGyroscopicTorqueEnabled=False,bAutoWeld=True,bStartAwake=True,bGenerateWakeEvents=False,bUpdateMassWhenScaleChanges=True)
```
