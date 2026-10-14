# MVP-B 第一阶段：配置迁移设计与决策留档 v1.0

日期：2026-09-18。

状态：用户已认可第一阶段迁移方案；尚未实现 C++ 配置类型、采样完整实际参数或迁移资产。本文记录已确认方向与施工门槛，不代表已完成验证。

## 1. 文档关系与长期定位

先读根目录 AGENTS.md、MVP_A_to_MVP_B_Handoff_CN_v1.0.md 及其关联说明。本文补充第一阶段已认可的方案，不替代项目约束和历史验证记录。

用户明确：从 MVP-B 开始使用的数据结构与系统设计将持续用于未来正式开发。实现应采用稳定职责、明确配置归属和可演进的入口，不能以一次性原型为理由另建将来必须废弃的配置链路。这不意味着提前实现完整联机、GAS、技能或比赛框架，也不意味着冻结字段、不再重构。

后续开发必须维护：设计决策、字段映射及单位、资产/配置来源、生效生命周期、迁移步骤、验证结果和已知问题。结构或规则变更时记录原因、兼容处理和回归范围，避免只在聊天记录中保存结论。

## 2. 接手核对结果

- 留档 Git 基点为 main / 08890c4；接手时为 main / d5d48c9。
- 新提交仅修改 Content/Blueprints/Vehicles/BP_RoundedVehiclePawn.uasset，说明为调整离墙转向；未读取资产内部具体数值差异。
- Source/ 与 Config/ 相对该基点没有新的已提交变化，源码没有未提交修改。
- 已有工作区改动：.gitignore、Config/DefaultEditor.ini、Content/Maps/Test.umap、HAL_Future_Creation.uproject；四份未跟踪 MVP-A 说明/交接文档。不得覆盖或删除。
- 本机引擎 Build.version 为 5.8.2，项目 EngineAssociation 为 5.8。
- 未发现子目录 AGENTS.md。Tools/InspectVehicleTuning.py 不存在。
- 当前配置体系未迁移为 Data Asset；现有复制声明不等于已完成联机。
- Config/DefaultEngine.ini 仍指定引擎 OpenWorld 默认地图，并启用 RayTracing；不在本阶段擅自调整。
- 实际蓝图模板、关卡覆盖及运行时有效值尚未完整采样。历史数值、C++ 默认值和文件哈希不能替代实际基线。

## 3. 第一阶段范围与配置职责

只保存实际基线、迁移已有配置、验证参数与手感一致。危险区、赛车模型/Rig、悬挂/落地辅助、联机、GAS、技能、状态及比赛流程仍按交接顺序后续实施。

名称为设计名，最终 C++ 命名沿用模块风格。

| 配置 | 职责 |
|---|---|
| VehicleDefinition | 驾驶、抓地、转向、手刹、离墙、接地检测；物理根；控球点、获取、约束、发射、后坐、脱球；初始 MaxHP；现有视觉引用及相对变换 |
| BallDefinition | 半径、质量、阻尼、物理材质、CCD 等物理设置；低速结束与攻击结束锁定；基础伤害、击退倍率；现有视觉引用及变换 |
| CombatRules | 现有球命中的全局档位阈值、三档结果、角速度限制和接地探测距离 |
| VehicleLocalConfig | 输入映射及五个 Input Action 引用；当前固定跟随相机参数，由车辆配置引用 |

- 配置按结构分组折叠，不为每个组件或字段创建一个资产。
- 先使用普通 UDataAsset。类型配置使用明确引用；Project Settings 的全局规则入口可使用软引用，在玩法开始前加载、校验并缓存，不在命中回调中重复加载。
- 不为当前迁移引入 Primary Asset 注册、复杂 Asset Manager 或动态类型选择框架。
- 木桩车保留 Box 根及被动行为，独立配置实例；不添加驾驶或控球能力，不更换其根类型。
- 凸包内容保存在网格资产，配置只管理引用和设置。
- 同行为、不同数值/外观的类型通过不同配置实例表达。
- 模型、动画等尚无实际消费者的内容不提前建立复杂层次；本阶段覆盖现有人工调参及资产引用。
- 调试开关需记录基线，实施时明确其入口；运行时状态不混入基础配置。

## 4. 基线与兼容策略

基线记录格式：资产/实例完整路径、组件、属性名、值、单位、引用路径、是否存在覆盖、采样层级及版本信息。

采样至少区分蓝图模板、关卡实例和 PIE 最终有效值。文件哈希仅识别保存的文件版本。不得拿历史九项恢复值或原生 CDO 默认值批量覆盖当前蓝图。

迁移模式：

- Legacy：完整沿用现有蓝图/组件参数，不应用 Definition。现有资产初始保持此模式。
- Definition：配置校验通过后，Data Asset 成为唯一基础配置来源，不叠加旧参数。
- Definition 模式缺失引用或配置非法时明确报错并阻止该实例正常进入玩法，不静默回退原生默认值，也不部分应用配置。

旧属性首轮保留序列化兼容，不能直接删除或随意重命名。逐资产完成核对及验收后退出旧编辑入口。Legacy 属于迁移兼容路径，不是正式版本长期保留的第二套调参来源。

全局击退迁移后，Project Settings 仅保留 CombatRules 资产引用入口。旧配置数值在兼容期间只服务 Legacy 路径，完成切换后退出有效来源。

字段演进应保留版本记录和迁移映射；需要重命名/删除反射字段时先评估现有资产兼容。不要为了版本化提前建设大型通用迁移框架。

## 5. 生效生命周期与实例边界

- 配置在蓝图构造结束后、输入占有及玩法消费者启动前统一解析、校验和应用。
- 建议审查 PreInitializeComponents 入口及其与 Super 调用的顺序；实施前以本机引擎和现有组件生命周期为准验证。
- BallControl 的 BeginPlay 会启动获取定时器，Health 的 BeginPlay 会初始化 HP；不得在 Pawn 的 Super::BeginPlay 之后才写入基础配置。
- 物理参数需同步实际刚体，不能只更新缓存。配置应用保持幂等，不新增逐帧配置 Tick，不在运行中切换几何、质量或重置状态。
- 保留现有原生组件类型、子对象名称及层级，避免再次损坏蓝图组件模板。
- 编辑器预览与运行时应用要分别验证；代理不得通过预览逻辑修改或保存蓝图资产。
- CurrentHP、线速度、角速度、球状态、持有者、发射者、锁定截止时间等仍属于实例。
- 共享配置只读；未来 GAS 或其他效果通过实例结果/倍率生效，不修改模板。
- 构造期 GetSimplePhysicalMaterial / GEngine not initialized 为历史风险，迁移时审查；单纯更改调用时机不能视为已修复。

## 6. 团队手动操作与采样清单

1. 打开实际验收关卡，记录完整路径、World Settings 的 GameMode Override、GameMode 的 Default Pawn Class；预放置玩家另记录 Auto Possess。
2. 核对实际被占有的玩家车，避免 GameMode 生成车与预放置车混用。
3. 打开 BP_RoundedVehiclePawn，逐组件记录 ArcadeMovement、BallControl、Health 的所有人工可调参数。
4. 记录物理根网格、缩放、质量覆盖、阻尼、重心/惯性覆盖、物理材质、碰撞响应、重力、CCD、命中事件及其他已有自定义设置。
5. 记录 VisualMesh、BallControlPoint、ForwardArrow 的资产引用、材质和相对变换；记录 CameraBoom / TopDownCamera 的实际设置及六个输入引用。
6. 同样记录 BP_BasicBall 的 Actor / PhysicsRoot / VisualMesh 与 BP_PassiveTestVehicle 的根体、外观和 HP。
7. 在关卡逐个检查车辆/球实例覆盖；PIE 用 F8 检查实际生成实例的有效配置。当前球权和 HP 变化不能写入模板。
8. Project Settings → Game → Vehicle Knockback 记录所有有效数值。当前未发现项目 ini 中该类数值覆盖，仍需核对加载后的设置对象。
9. C++ 类型完成后，团队保存并关闭 Editor，完整编译 Editor Target，再启动项目；不以 Live Coding 代替结构性迁移验证。
10. Content Browser → Miscellaneous → Data Asset 创建配置实例，逐项填写真实基线，不直接接受默认数值作为验收值。
11. 团队手动给现有蓝图指定配置引用，核对后切换 Definition。若实例覆盖代表正式差异，使用单独配置实例；若是临时测试，先记录处理决定再清理。
12. 团队 Compile / Save 后重新 PIE。顺序为圆头车、普通球、木桩车、全局击退；每步核对后再进入下一步。

本节是施工操作顺序，不代表当前已存在配置类型或迁移开关。字段完成后应补充与真实引擎 Details 一致的逐字段映射。

## 7. 验证与后续记录要求

实施后编译 Editor / Game Target，运行 HAL.FutureCreation.Vehicle.WallEscape 五项已有测试；如迁移改变测试访问的字段位置，调整测试读取入口并保留原有行为断言。

补充针对配置来源、缺失/非法配置、生命周期和避免重复应用的必要验证。团队 PIE 回归正常驾驶、刹车倒车、低高速手刹、侧撞、墙角、腾空、连续碰撞、获取/带球/发射/后坐/脱球、重捕锁定、持有者销毁及三档命中结果。

验收要求：参数逐项一致，资产引用及实例差异明确，实际手感无迁移引入的改变，旧入口不再形成第二数值来源。历史普通漂移退出顿挫仍搁置，不把它写成已修复。

每次施工更新涉及文件、关键决定、编译/测试命令与结果、人工 PIE 结果及未执行项目。历史通过记录不能当作本次通过记录。

## 8. 开工条件与用户补充确认

用户已明确：

1. 功能测试暂时继续使用当前 Test 场景；用户也可能手动复制新的 MVPB-BTEST 测试场景。目前不要求代理创建或保存新关卡。
2. 全部调参均已保存，可以开始读取已保存基线。
3. 现有参数均为临时测试调参，先完整保存，后续允许继续调整。临时性质不意味着可以在迁移时抹除或改用 C++ 默认值。

已消除开工前的偏好问题，不重复要求用户确认。实例覆盖先作为基线保留和记录，不视为已经确定的正式车辆/球类型。迁移配置可以使用测试基线命名，后续调参更新配置及基线版本即可。

尚需只读核对的工程事实：关卡完整路径、GameMode Override / Default Pawn Class / Auto Possess 与实际玩家、球、木桩引用。Saved/Config 的 LastLevel 和最近关卡指向 /Game/Maps/Test，这只是选择只读检查目标的依据，不替代关卡内部检查或 PIE 验证。

联机策略、技能/状态、模型规范和动态相机指标不阻塞本阶段。完整实际基线记录并校验前，不启用 Definition 覆盖。

## 9. 已保存基线采样记录（2026-09-18）

- 工具：Tools/InspectMVPBaseline.py。使用本机内置 PythonScriptPlugin，仅通过该次命令行启用，不更改 .uproject、不作为玩法依赖。
- 有效快照：Docs/Baselines/MVP_A_Saved_Test_20260918T030251Z.json；范围为已保存蓝图 CDO、编辑器关卡实例、输入资产和全局击退设置，不包含 PIE。
- 已读取 /Game/Maps/Test，其 GameMode Override 为 BP_MVPA_GameMode；对应 Default Pawn Class 为 BP_RoundedVehiclePawn。已读取场景中的普通球和木桩车，没有发现该圆头车的预放置实例。实际 PIE 生成/占有仍需后续运行核对。
- 圆头车已保存值包括：SteeringAngularAcceleration=12、FullSteeringSpeed=400、LateralGripRate=12、HandbrakeYawDampingRate=0.3、WallEscapeMaxYawAcceleration=24。不能沿用历史恢复表或原生默认值。
- 普通球已保存值包括 KnockbackStrengthMultiplier=5、LinearDamping=1；其余配置详见快照。
- 当前 IMC 发射为 Gamepad_LeftShoulder（LB），键盘为 LeftMouseButton；旧交接/操作文档的 X 不再代表已保存输入。保留实际资产绑定，不自动改键。
- 原生配置字段读取校验通过；全局击退配置完整读出。快照保留其他引擎字段的 read_errors，主要为受保护字段、事件委托与弃用字段，不宣称已采集所有引擎属性或蓝图图表。
- 采样前后以及进程退出后，Source/、Config/、Content/ 与 .uproject 的文件哈希一致；没有调用资产编辑、Compile 或 Save API。
- Python 脚本执行成功，Commandlet Main 返回 0；整个进程退出码为 1，原因包括历史原生 CDO 构造期三条物理材质初始化错误。另有弃用属性读取与系统证书警告。因此不得表述为无错误启动或完整环境验证通过。
- 首次启动因 Zen 缓存目录不可用中止；后续通过命令行使用项目 DerivedDataCache/Inspection 文件系统缓存。该缓存及 Saved/Logs 仅为本地诊断产物，不纳入正式配置或提交。

可复用采样命令（每次输出新文件，不覆盖既有有效基线）：

```powershell
& 'D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UnrealProjects/HAL_Future_Creation/HAL_Future_Creation.uproject' -EnablePlugins=PythonScriptPlugin -run=pythonscript '-Script=D:/UnrealProjects/HAL_Future_Creation/Tools/InspectMVPBaseline.py' '-DDC=(InstalledEnginePak,Local)' '-LocalDataCachePath=D:/UnrealProjects/HAL_Future_Creation/DerivedDataCache/Inspection' -Unattended -NullRHI -NoSound -NoSplash -NoLiveCoding -NoP4 -stdout '-abslog=D:/UnrealProjects/HAL_Future_Creation/Saved/Logs/MVPBaselineInspection.log'
```

注意 Script 参数大小写：本机 Commandlet 对 -Script= 进行手动查找，使用上述拼写。下一施工步为配置字段映射与 C++ 类型/应用入口，现有资产仍保持 Legacy。

## 10. 本阶段施工记录（2026-09-18）

以上设计/采样记录为对应时点；用户随后授权施工。本次已实现可持续用于正式开发的四类普通 UDataAsset：VehicleDefinition、BallDefinition、VehicleLocalConfig、CombatRulesDefinition，以及现有组件的初始化配置入口、显式 Legacy/Definition 来源、运行校验与错误处理。旧序列化字段和全部组件名称/类型/层级保留，既有资产仍保持 Legacy，没有用新类型种子覆盖调参。

详细长期约定、涉及文件和命令/结果见 `Docs/Configuration/MVPB_Phase1_Implementation_CN_v1.0.md`；61项原生玩法字段映射见同目录 `NativeFieldMapping_v1.json`；五个 DA 的创建、真实调参表、物理/视觉/输入/相机、逐资产切换、实例覆盖及回退步骤见 `MVP_B_Phase1_Editor_Migration_CN_v1.0.md`。

验证结果：Editor / Game Target 成功、退出0；原有5项离墙和新增5项配置自动化均通过、退出0。编译后只读快照为 `Docs/Baselines/MVP_A_Saved_Test_20260918T032357Z.json`：模板/场景实例79次旧原生配置与198次相关组件字段比较一致，输入/全局击退规则一致，23个 Content/Config/.uproject 保存文件哈希未变。只读 Commandlet 退出0、0错误/56警告，历史三条构造期物理材质初始化错误未再出现；不声称所有引擎属性均已采完。

留档摘要：`Docs/Configuration/Phase1Verification_v1.json`。尚未完成团队手动 DA/蓝图配置、实际玩家占有/PIE有效值、Materials槽与手感验收；因此第一阶段不记为全部完成。蓝图/关卡资产没有被代理修改或保存。未来危险区、模型/Rig、联机/GAS/技能/比赛系统没有在本次扩展。
