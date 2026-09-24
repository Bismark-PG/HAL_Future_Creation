# MVP-B 第一阶段实现与长期配置约定 v1.0

日期：2026-09-18；模块：HAL_Future_Creation；验证引擎：UE 5.8.2。用户已授权施工，以下类型和职责计划继续沿用于正式开发。当前代码就绪，蓝图/配置资产切换和真实 PIE 手感验收由团队按根目录 `MVP_B_Phase1_Editor_Migration_CN_v1.0.md` 执行，尚未记为完成。

后续进展（2026-09-18）：团队已配置 Data Asset，用户反馈“初步测试暂时没有功能问题”。工程已有 `/Game/Data/MVP_B` 下五份配置，DefaultGame.ini 的全局来源为 Definition 并引用该目录的 Combat DA。日常调参见 [Data Asset 配置速查](MVPB_DataAsset_QuickGuide_CN_v1.0.md)。以下施工期快照/验证记录保留原时点，不以本次反馈补写未提供的专项验收或逐资产运行值。

## 1. 配置类型与职责

| 类型/文件 | 消费者与边界 |
|---|---|
| VehicleConfigurationTypes.h/.cpp | 分组的驾驶、抓地、转向、离墙、接地、控球、发射/后坐、脱球、球玩法、初始HP及调试参数；显式运行期 Validate |
| VehicleDefinition.h/.cpp：UVehicleDefinition | 现有玩家车或木桩的几何引用/Box尺寸、物理模板、外观；玩家额外消费 Movement、BallControl、LocalConfig、控球点和箭头 |
| 同文件：UBallDefinition | 球半径、物理模板、状态阈值/锁定、基础伤害/击退倍率与外观；仍为轻量物理 Actor |
| 同文件：UVehicleLocalConfig | 六个 Enhanced Input 引用、当前固定跟随相机；独立于权威玩法参数 |
| 同文件：UCombatRulesDefinition | 既有球命中的公共档位阈值、结果和稳定限制；未建设其他来源的规则框架 |
| VehicleKnockbackTypes.h | 原有 EVehicleKnockbackTier / FVehicleKnockbackTierDefinition 仅移动声明文件，反射名称与模块包不变 |
| VehicleConfigurationApplication.h/.cpp | 初始物理模板、现有视觉和相机组件的应用；配置来源/有效性窄查询 |
| VehicleKnockbackSettings.h/.cpp | Legacy 数值或 CombatRules 软引用；初始化加载/校验/缓存，命中路径只读取运行配置 |

使用普通 UDataAsset，未建立 PrimaryAssetId、Asset Manager、运行期类型注册或大型迁移框架。新增 Data Asset 的 ReadyForUse=False，人工填写并逐项比较后才能启用；新类型默认值只是种子，不能当成实际蓝图基线。

## 2. 数据分组、单位与版本

Vehicle.Movement 分为 Drive、Grip、Steering、WallEscape、Ground、Debug；Vehicle.BallControl 分为 Acquisition、Control、Launch、ForcedRelease、Debug；Vehicle.Health 分为 Health、Debug；Ball.Gameplay 分为 State、Acquisition、Damage、Debug。

全部原生玩法字段的迁移映射见 `NativeFieldMapping_v1.json`（61项），实际值与其他组件/输入/物理字段见 Editor 操作文档。映射中的 ArcadeVehicleConfig、BallControlConfig、BallGameplayConfig、VehicleHealthConfig 是结构名称；资产中的对应入口分别为 Movement、BallControl、Gameplay、Health，不是嵌套资产名称。

UPROPERTY 保留原字段 ClampMin/ClampMax/Units 元数据；运行校验显式拒绝 NaN、Infinity、越界、非法通道、错误离墙迟滞/后坐上下限和档位阈值。不能只依赖编辑器滑条；保留的旧 Legacy 路径仍沿用旧行为及原有运行 clamp。

距离为 cm、速度为 cm/s、线加速度为 cm/s²、时间为 s、质量 kg。角速度/角加速度、物理引擎高级字段以各字段注释/元数据与 UE 原始定义为准：例如 Body.MaxAngularVelocity 属于引擎字段，不能与 Combat.MaxAirborneAngularSpeed（rad/s）混淆。力度/倍率和 HP 沿用原规则，不改公式。

文档/快照/映射 schema 当前为 v1。反射字段重命名或删除必须新增兼容决定、旧→新映射及新基线；不能静默破坏已有正式资产。现有配置类型不挂靠地图名或 MVP 专用模块，测试配置实例名称允许标注 TestBaseline，后续正式实例继续使用同一类型。Legacy 是迁移兼容期入口，逐资产验收后再单独评估退出，不承诺永远维护双来源。

## 3. 来源与生效顺序

所有既有资产默认 EVehicleConfigurationSource::Legacy，完整保留序列化的组件字段；没有自动覆盖保存资产。Definition 要求引用存在、Ready、完整校验、正确根类型（玩家凸包网格、木桩Box、球Sphere）。失败明确记录 Error，关闭该实例物理碰撞/模拟，玩家移动 Tick/Input 入口停止，控球和伤害也检查有效性；不静默回退、不部分套用本地配置。

运行入口是 Actor.PreInitializeComponents：蓝图构造已结束；本地配置校验/应用及全局规则初始化在 Super 之前完成，之后才进入组件 BeginPlay / Pawn 自动占有等消费者。Health.BeginPlay 因此看到新的 MaxHP；BallControl 的获取定时器使用配置后的间隔。编辑器 OnConstruction 仅在非游戏 World 预览已有物理/视觉/相机，不写运行玩法状态。

Movement / BallControl / Health 的 ApplyConfiguration 是窄 C++ 初始化接口，不能在组件 BeginPlay 后重套基础值。当前不提供运行期重载或切换几何/质量的功能。修改 DA 后重启 PIE 应用，不能把修改共享模板当作 Buff 或动态 HP 变化。

## 4. 物理模板与实例隔离

Physics.Body 使用 FBodyInstance 作为 defaults-only 模板，覆盖原有高级物理配置而不丢弃重心、惯性、材质、锁轴、碰撞响应等。应用于初始化阶段：设置几何/缩放，销毁尚未进入玩法的 Physics State，CopyBodyInstancePropertiesFrom，再重建实际 Chaos 刚体。源模板不得包含活跃 Body/Owner/BodySetup 引用；不是从运行中刚体复制数据。不新增物理 Tick，不修改根类型或组件子对象名称/层级。

Shared Data Asset 在玩法中只读。HP、输入命令、线/角速度、持有者、发射者、球状态、锁定截止、定时器等留在各实例；组件保留字段是实例的已解析参数缓存和 Legacy 序列化兼容，不成为第二个 Definition 编辑来源。Definition 下原生旧玩法/输入入口变灰；引擎标准组件 Details 无法统一隐藏，团队以 DA 为本文迁移字段的唯一调参入口。

Visual 只管理现有 Mesh、材质覆盖槽/Overlay、相对变换和可见/阴影开关，仍不碰撞/不模拟；其他引擎渲染细节继续保留原组件。LocalConfig 管当前透视固定相机参数，未实现动态相机策略。没有以迁移为由重做 Rig、悬挂或网络物理。

## 5. 全局规则与伤害入口

Vehicle Knockback Project Settings 增加显式 Source / CombatRules。Definition 模式在 Actor 初始化时同步加载一次，验证后按当前 World 缓存 FVehicleKnockbackConfig；GetRuntimeRules 不加载资产，既有 CombatResolver 在修改 HP 前要求配置有效。旧的 Settings.SelectTier / GetTierDefinition C++ 辅助函数被合并到有效规则结构，避免 Definition 结果意外读取 Legacy 数值。

当前仍沿用已存在的简单 Health/CombatResolver 和 Actor 复制声明；未接入 GAS 或验证联机。此项目级缓存只服务当前测试阶段，同 World 重复初始化不重复加载。未来同时运行多 World/正式 Listen Server 时，应在相应权威 World 生命周期持有规则缓存并验证复制/客户端边界，而不是假定一个 DeveloperSettings 缓存即已完成联机设计。本阶段不为此提前建设比赛子系统。

三个原生 Actor 构造函数的质量赋值改为 BodyInstance.SetMassOverride（本机引擎此 setter 只写模板字段），避免构造期 SetMassOverrideInKg 立即触发材质/质量重算。实际模拟质量在组件物理状态创建时生效；只读加载与自动化中已验证，未将调用时机推断当作修复证据。

## 6. 本次工程核对与保存证据

基准 HEAD=d5d48c9fb50e7565fdff1226f15eb10e38c1908b，源控显示 main 落后 origin/main 1；本次没有拉取、提交或替用户合并。既有 Config/DefaultEditor.ini、Content/Maps/Test.umap、.uproject 改动保留。交接的历史 HEAD=08890c4 并不代表当前资产版本。

原 .gitignore 末尾重复的 *.md 规则会覆盖前面的文档白名单；本次修正该顺序，并允许 AGENTS、既有 MVP-A 文档、MVP-B 文档和配置留档进入版本控制。当前仍为未提交文件，由团队连同源码/基线一起保存到源控；没有以留档为由替用户提交现有关卡或配置改动。

初始有效快照：`Docs/Baselines/MVP_A_Saved_Test_20260918T030251Z.json`；编译后只读快照：`Docs/Baselines/MVP_A_Saved_Test_20260918T032357Z.json`。Tools/InspectMVPBaseline.py 不调用编辑/Compile/Save API，使用进程级 PythonScriptPlugin，不更改插件依赖。

比较结果：模板69个、Test实例10个旧原生 EditDefaultsOnly 字段，共79次比较一致；198次相关组件字段比较一致，输入及原有全局配置也一致。Source 有预期代码变化；23个已存 Content/Config/.uproject 文件对初始基线哈希无变化，未新增此范围文件。采样自身前后哈希也一致。比较工具为 Tools/CompareMVPBaseline.py，证据摘要见 `Phase1Verification_v1.json`。

只读加载当前成功退出0，Commandlet 汇总0错误、56警告（主要弃用/受保护读取及系统环境）；历史3条构造期 GetSimplePhysicalMaterial / GEngine not initialized 不再出现。仍不能声称引擎日志无警告或已自动读取全部引擎高级属性。Materials 覆盖槽、Blueprint 图表和 PIE 有效值需团队核对。

## 7. 可复用验证命令与范围

```powershell
& 'D:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' HAL_Future_CreationEditor Win64 Development '-Project=D:/UnrealProjects/HAL_Future_Creation/HAL_Future_Creation.uproject' -WaitMutex -NoHotReload
& 'D:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' HAL_Future_Creation Win64 Development '-Project=D:/UnrealProjects/HAL_Future_Creation/HAL_Future_Creation.uproject' -WaitMutex -NoHotReload
& 'D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UnrealProjects/HAL_Future_Creation/HAL_Future_Creation.uproject' '-ExecCmds=Automation RunTests HAL.FutureCreation' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/UnrealProjects/HAL_Future_Creation/Saved/Automation/MVPB_Phase1' '-DDC=(InstalledEnginePak,Local)' '-LocalDataCachePath=D:/UnrealProjects/HAL_Future_Creation/DerivedDataCache/Inspection' -Unattended -NullRHI -NoSound -NoSplash -NoLiveCoding -NoP4 -stdout
```

最终 Editor / Game Target 均编译成功、退出0。10项自动化全部通过、进程退出0：原有5项 WallEscape，新增 Configuration.Validation、InitializationAndIsolation、NoFallback、CombatRulesSource、PlayerAndBallInitialization。新增测试覆盖未审核种子/非法数值、全量校验前不改几何、缺失引用阻止玩法/伤害、真实刚体质量、组件 BeginPlay 初始 HP、共享配置隔离、禁止活跃重套、玩家输入/相机/移动/控球参数以及球控制/发射状态。

本地日志在 Saved/Logs/MVPB_Phase1_Build.log、MVPB_Phase1_GameBuild.log、MVPB_Phase1_Tests.log、MVPB_Phase1_BaselineInspection.log；详细测试报告在 Saved/Automation/MVPB_Phase1。这些是本地诊断产物，不提交 Binaries/Intermediate/Saved/DDC。持久化结果摘要在 Docs/Configuration 下。

首次沙箱内完整 Editor 测试在 TurnkeySupport 模块加载处停滞，结束本次创建的测试进程后使用正常本机工具权限重跑成功；不是玩法测试失败。首次新增测试编译报反射查询辅助函数/头文件顺序错误，已修正并重新完整编译，不把中间失败当成最终通过记录。

## 8. 交付边界与待人工结果

本次未编辑或保存任何 .uasset/.umap，也未启用实际资产 Definition。根目录 Editor 操作文档提供五个 DA 的创建、真实数值/引用、逐资产开关、关卡覆盖检查、Project Settings 默认配置保存和回退流程。遵守 AGENTS.md §8.3，由团队手动完成资产配置并记录。

未执行真实 PIE 的输入/占有、全部实例覆盖、手感、材质槽、低高速/侧撞/墙角/腾空/连续碰撞、持有者销毁/多候选球及完整三档命中回归。这些是第一阶段验收剩余项，不需要再次确认开工偏好，也不能用自动化替代。操作文档有验收记录表。

后续只在第一阶段参数/手感验收后进入已有路线，危险区、模型/Rig、联机、GAS、技能、持续状态和比赛框架未在本次实现。
