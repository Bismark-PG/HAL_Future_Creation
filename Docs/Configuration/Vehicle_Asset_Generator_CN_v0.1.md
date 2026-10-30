# 车辆资产生成工具

更新：2026-10-23。工具属于现有 `HAL_Future_CreationEditor` 模块，通过 Editor 的 **Tools → HAL Tools → HAL Vehicle Asset Generator** 启动。公开接口位于 `Source/HAL_Future_CreationEditor/Public/VehicleAssetGenerator`，实现位于对应 Private 目录。

2026-10-30输入引用显示补充：已在现有Editor模块加入车辆Details只读预览，生成蓝图及已有Car_02的Input直接显示当前Definition.LocalConfig中的十项输入引用和来源。共享Local不变，显示不复制Pawn旧默认值；构建统一由BuildAndLaunch.cmd负责。具体读取位置、运行初始化及验证边界见[输入引用显示](Vehicle_Input_Preview_CN_v0.1.md)。

2026-10-23车型独立配置修正：面板仅选择 Skeletal Mesh 和共享车辆 Network Physics DA，已删除 Shared Vehicle Defaults 输入、参数和复制来源。每次直接新建独立 `UVehicleDefinition`，使用 C++ 初始参数，并自动引用固定共享资产 `/Game/Data/MVP_B/DA_Local_TestBaseline_v1`。生成器直接从原生 `ATestVehiclePawn` 创建玩家蓝图，连接本车型的新 Definition／Presentation；生产实现与核心测试均无需现有车辆 Definition 或 Blueprint。最终只生成两个 DA、一个 BP 和一个空 AnimGraph 的 ABP。

2026-10-22菜单恢复历史基点为 `main/5f6c9ef`；本轮接收云端 `main/ad3e4d4`，保留 VP-3 实现及原有碰撞、视觉高度校准入口。Build.cs 已具备所需依赖及平铺头文件目录，本轮保持内容。HAL Tools 是 Tools 菜单内的分组标题，界面中可能直接列出三个工具。

## 1. 生成内容

以 `SKM_Car_02` 为例，工具固定生成四份资产：

| 资产 | Content 路径 | 初始设置 |
|---|---|---|
| `ABP_Car_02` | `Blueprints/AnimBP` | ParentClass 为 `HALVehicleAnimInstance`，TargetSkeleton 为所选 Mesh 的实际 Skeleton，PreviewSkeletalMesh 为所选 Mesh |
| `DA_Car_02_Presentation` | `Data/Car_Presentation` | 引用所选 Mesh 和新 AnimBP 的生成类；其余表现字段使用原生初始值，Ready For Use=false |
| `DA_Vehicle_Car_02` | `Data/MVP_B` | 独立新建 VehicleDefinition，使用 C++ 初始参数；LocalConfig 引用共享 DA_Local_TestBaseline_v1，Presentation 引用本次新表现 DA，Ready For Use=false |
| `BP_VehiclePawn_Car_02` | `Blueprints/Vehicles` | 直接继承原生 TestVehiclePawn，ConfigurationSource=Definition，Definition 引用新车辆 DA，网络组件引用所选共享 Network Physics DA |

车型取 Mesh 名称中 `SKM_` 后的完整内容，保留下划线和大小写。工具使用 `/Game` 虚拟包路径，输出目录固定。任一目标名称已存在，包括尚未保存的内存包，整组创建都会被阻止；没有覆盖或自动编号。

AnimBP 由引擎工厂创建，仅保留默认空 AnimGraph 和默认输出节点。工具不增加 Modify Bone、状态机或动画接线。创建期间进行必要编译，以建立可供表现 DA 引用的生成类。

## 2. 面板使用

1. Skeletal Mesh 选择名称为 `SKM_<车型>` 且具有 Skeleton 的模型。
2. Shared Vehicle Network Physics 选择已有车辆网络物理 DA，当前为 `DA_NetworkPhysics_MVPB`，车辆应使用车辆配置，球的网络物理 DA 保留给球。当前项目已启用 Physics Prediction；生成器不会修改该项目开关。
3. 面板显示固定共享 LocalConfig 的完整资产路径。该资产由工具自动加载，需存在且通过原有配置校验；没有车辆参数模板输入。
4. 面板显示四个完整输出路径及检查结果。Create Asset Set 创建四份尚未保存的资产，并在 Content Browser 定位它们。
5. Save Generated Assets 仅保存本组四份资产。保存结果逐项处理，失败项显示路径，已成功项保留，可再次保存。源模型与共享 Local／Network DA 不会被该按钮保存。
6. Locate Generated Assets 可重新定位结果。上一组通过工具保存后，可继续选择另一个模型创建下一组。

Use Selected Sources 从 Content Browser 读取选择，最多接受一个 Skeletal Mesh 和一个 Network Physics DA；同类型多选会明确拒绝，避免依赖选择顺序。VehicleDefinition 与车辆 Blueprint 均不作为输入。

PIE 或 Simulate 期间禁止创建与保存。关闭工具面板不会自动保存资产，创建后的资产遵循 Editor 常规未保存资产管理。

### 2.1 从生成到接入测试

| 阶段 | 配置位置与结果 |
|---|---|
| 加载工具 | 完整构建 Editor Target 后重新打开项目；Tools 菜单的 HAL Tools 分组提供 HAL Vehicle Asset Generator |
| 选择输入 | Mesh 选择新车型，Shared Vehicle Network Physics选择现有车辆网络DA；共享Local自动连接 |
| 创建并保存 | 核对输出路径，Create Asset Set → Save Generated Assets；结果应为4/4，Locate Generated Assets可再次定位 |
| 配置动画 | 新 ABP 的父类和 Skeleton 已设置；团队手动配置轮转／转向及需要的 Chassis 节点，生成器保持 AnimGraph 默认内容 |
| 校准车型 | 新表现 DA 的四轮骨骼、轴向、方向、实际半径及模型变换由团队配置；新车辆 DA 的Collision Mesh必须配置，骨骼Presentation有效时Visual.Mesh为可选静态回退，物理、驾驶、控球点等初值需要校准 |
| 高度校准 | 完成表现数据后，Content Browser选中新 VehicleDefinition，使用现有 HAL Vehicle Visual Alignment；具体限制与保存流程见[校准工具说明](Vehicle_Visual_Alignment_Tool_CN_v0.1.md) |
| 启用配置 | 校准完成后，两份新 DA 分别设置 Ready For Use，保存并执行 Validate Assets；输入和摄像机仍使用原 LocalConfig |
| 接入测试 | 新 BP 的 Definition 已绑定。团队在测试关卡或 GameMode 中手动选用新 BP，重新 Play，检查驾驶、动画、持球与发射；主机和客户端需要同一套已保存资产 |

工具不自动替换当前 GameMode 的默认 Pawn，不放置关卡实例。已放置实例可能保存 Definition 覆盖，应核对其实际引用。相同 Mesh 再次生成会因固定名称冲突被拒绝，已生成资产的后续配置直接在对应编辑器完成。

## 3. 引用和待配置事项

引用位置沿用 `BP_RoundedVehiclePawn` 的结构：

| 属性位置 | 生成后的引用或值 |
|---|---|
| 新 Pawn → ConfigurationSource | Definition |
| 新 Pawn → Definition | 本车型独立 DA_Vehicle_<车型> |
| 新车辆 DA → LocalConfig | 共享 DA_Local_TestBaseline_v1 |
| 新车辆 DA → Presentation | 本车型独立 DA_<车型>_Presentation |
| 新表现 DA → Mesh／AnimClass | 所选 SKM_<车型>／新 ABP_<车型> 的生成类 |
| 新 Pawn → VehicleNetworkPhysicsSettings → SettingsDataAsset | 面板所选共享 Network Physics DA |

Network Physics 保持原引用方式并配置 Resimulation。Combat 与 Stability 继续由项目设置提供；Local／Network／Combat／Stability 均不新建或复制。工具不修改 GameMode、PlayerState、地图或输入映射。

驾驶、控球、战斗、物理和 Marker 使用 `UVehicleDefinition` 的 C++ 初始值，各车型独立持有配置。碰撞 Static Mesh、回退 Visual.Mesh 和车型材质没有从 Car01 复制，也不根据 Skeletal Mesh 推断玩法碰撞；Collision Mesh初始未指定，仍须配置。2026-10-29起驾驶车按可用表现路径校验：有完整有效且Ready的骨骼Presentation时，Visual.Mesh可以保留None；否则必须配置有效静态Visual。生成器继续保持空静态引用，不自动创建占位网格。两份车型 DA 的后续参数编辑互不影响，共享 Local／Network 的调整则继续影响引用它们的车型。

新Pawn与 `BP_RoundedVehiclePawn` 共享同一个原生父类。物理根、Static/Skeletal外观、移动、姿态与自救、网络物理Settings／History、运动效果桥接、控球点／约束／控球、Health／ImpactReceiver、方向标记及相机共17个组件由原生构造函数创建，组件连接及输入／GAS Avatar／控球／发射逻辑沿用原生实现。新Pawn没有对 `BP_RoundedVehiclePawn` 资产的继承或引用；选择的模型经新Definition／Presentation接入。ASC仍由PlayerState持有，GameMode的PlayerState／Controller配置继续使用项目现有设置。

新表现 DA 不继承 Car01 的轮轴、半径、模型缩放和俯仰调参。其 Root／Chassis 和四轮名称来自当前 C++ 默认约定，半径初值为0。缺少控制骨骼不会阻止创建配置草稿，但会阻止表现配置通过运行时校验。

两份新 DA 均保持 Ready For Use=false。团队在 AnimBP 完成所需骨骼接线，填写Collision Mesh，校准四轮轴向、方向、实际半径、模型对齐、物理与控球点后，再启用Ready并执行资产校验及单机／双端基本测试；需要静态回退时另填Visual.Mesh。车辆DA未Ready或两条表现路径都不可用时会拒绝参与玩法，此状态不能作为可直接驾驶的交付。具体路径规则与手工步骤见[DA说明第5～6节](MVPB_DataAsset_QuickGuide_CN_v1.0.md#5-骨骼车型仍需填写-visual-吗2026-10-29源码核对)。当前工具只负责四份资产的创建与连接。

## 4. 首次实现验证记录

第4～6节保留各轮实现与验证历史，其中基础Blueprint输入及Shared Vehicle Defaults复制方案已经由第1～3节的当前方案替代。

本机 UE5.8.3-58210709，Editor Win64 Development 编译通过。最终构建日志为 `Saved/BuildTool/VehicleAssetGenerator_Editor_Final.log`。首次尝试遇到 UBA 全局缓存写入权限及 Live Coding 全局互斥对象访问问题，随后使用本次进程可写缓存和完整非热重载构建；没有修改引擎或项目构建配置。编译中发现的头文件路径、局部变量遮蔽及过期遍历 API 均已修正，最终构建通过；UBA action-result 缓存提示保持，不作为失败。

新增三项核心自动化：`HAL.Editor.VehicleAssetGenerator.Naming`、`ReferenceChain`、`Preflight`，覆盖精确输出路径／非法名称、四份资产引用／共享配置／网络设置继承／空 AnimGraph、缺失输入／名称冲突／无部分创建。另有实际渲染面板测试 `HAL.EditorUI.VehicleAssetGenerator.Panel`，检查 Tools 菜单、新旧工具入口、窗口和截图。

本轮尝试通过 UnrealEditor-Cmd 及 UnrealEditor 启动自动化，进程没有产生 Unreal 日志、报告或截图，不能证明 Editor 完成启动或任何测试执行。**上述四项新增测试均只完成编译，自动化、实际面板操作和渲染尚未验证。** 第一次调用使用常规渲染和 HAL.Editor 过滤，随后采用 NullRHI 核心过滤复查，均未取得执行证据。不以退出码0记为通过。

团队可在重新启动的 Editor 中打开 Session Frontend 的 Automation，运行 `HAL.Editor.VehicleAssetGenerator`，随后运行 `HAL.EditorUI.VehicleAssetGenerator.Panel`。手工检查可使用独立新车型名称，确认菜单能打开、四个路径正确、基础蓝图选中后自动读取 Definition、创建后引用完整、同名重复创建被拒绝、保存结果明确。保存并重启 Editor 后再次检查引用与空 AnimGraph。

Git 检查中 Content 仍仅显示开工时已有的 `SKM_Car_01.uasset` 未提交修改，本轮未保存或生成 Content 资产；也未修改配置、运行时源码及现有蓝图。开工的完整资产清单曾写入进程临时目录，后续工具进程无法访问该目录，因此不宣称完成全资产前后哈希比对。核对摘要见 [本次验证记录](Vehicle_Asset_Generator_Verification_20261022.json)。

真人新车型 AnimGraph 接线、完整保存后重启 Editor、基本驾驶／双端和四机测试尚未执行。工具本轮交付为代码完成、编译通过，人工与自动化执行验证待完成；不将其写成可驾驶车型验收。

## 5. 同步后菜单恢复验证

2026-10-22基于已拉新的 `main/5f6c9ef`，在最新模块入口上恢复资产生成工具的窗口注册、Tools菜单动作与关闭注销。碰撞生成和视觉高度校准入口保持，Build.cs与云端提交一致，没有增加模块或依赖。

本轮 UE5.8.3-58210709 的 Editor Win64 Development 完整构建成功，包括拉新后所需的运行时模块和Editor模块；日志 `Saved/VehicleAssetGenerator/Reconnect_20261022/Editor_Build.log`。现有UI测试补充检查视觉高度校准入口保留，测试源已编译。`git diff --check`通过。

本轮用 `UnrealEditor-Cmd`、RenderOffscreen和 `HAL.Editor` 过滤尝试自动化，进程退出码0，但标准输出／错误为空，没有 Unreal 日志、自动化报告或截图；本次仍无法证明测试实际执行。三项核心测试、面板测试、手工生成／保存后重启检查继续待验证，不引用以前的构建作为本次运行结果。

本次将资产基线放在项目 Saved 目录，开工和收尾的667份 `.uasset`／`.umap` 数量一致，逐文件 SHA-256 全同。原有 `SKM_Car_01` 的未提交修改相对本轮开工保持，未新增保存资产。详细见[菜单恢复验证摘要](Vehicle_Asset_Generator_Reconnect_Verification_20261022.json)，首次实现记录仍保留。

## 6. 取消基础蓝图输入后的验证

2026-10-22基点 `main/faf5d06`，开工工作区干净。用户要求以Mesh和现有共享DA为输入，生成独立玩家Pawn。现已移除Base Vehicle Blueprint字段及生成器对基础蓝图的所有读取／继承。Shared Vehicle Defaults提供已有驾驶参数和Local等外部引用；Shared Vehicle Network Physics直接连接原生网络Settings组件。四个输出目录和名称保持。

原生父类固定为 `ATestVehiclePawn`，创建时设置Definition模式及新DA、共享网络DA和Resimulation；编译后复核父类及Definition／网络引用，失败时不公布部分结果。回归夹具只读取Mesh、参数DA与网络DA，不加载已有车辆Blueprint；断言17个原生玩家组件、复制模式、共享配置、Native CDO不受修改、空AnimGraph及缺失输入／同名冲突保护。

UE5.8.3 Editor Win64 Development最终编译成功，日志 `Saved/VehicleAssetGenerator/NativePawn_20261022/Editor_Final.log`。首次测试编译遇到引擎GetPhysicsReplicationMode非const接口问题，修正测试对象限定后重新构建通过；首次失败日志保留。本轮未改运行时模块、Build.cs、菜单入口、项目配置或现有资产。

本轮尝试运行 `HAL.Editor.VehicleAssetGenerator` 三项核心测试，UnrealEditor-Cmd仍未产生日志／报告，不能确认测试实际执行；UI／真人功能与保存重启检查保持待测。667份资产前后SHA-256全同。此轮仅证明代码完成与编译通过；与BP_RoundedVehiclePawn相同的原生逻辑及配置入口已连接，驾驶／GAS／球／双端实际行为尚无本轮新验收。两份新DA的Ready及AnimGraph手工配置要求仍适用。详细见[原生玩家Pawn修正摘要](Vehicle_Asset_Generator_NativePawn_Verification_20261022.json)。

## 7. 车型独立Definition修正验证

2026-10-23基点 `main/ad3e4d4`，保留云端VP-3和前轮工具未提交修改。Shared Vehicle Defaults已从面板、服务参数和选择逻辑移除；生产实现及测试不再加载已有车型Definition。创建时使用 `NewObject<UVehicleDefinition>`，连接固定共享Local和本次新Presentation，再将其绑定到原生玩家Pawn。所选Network Physics DA继续直接连接Pawn组件。共享Local的存在及原有Ready／输入／相机校验在创建前检查；工具不增加共享DA。

本轮 UE5.8.3-58210709 的 Editor Win64 Development 构建成功，日志 `Saved/VehicleAssetGenerator/IndependentDefinition_20261023/Editor_Build.log`。构建包含接收VP-3后需要重编的运行时模块和本次Editor代码；UBA部分action-result缓存任务未成功，Target最终结果为Succeeded。运行时源码及配置没有修改。

三项核心回归测试已编译。本次ReferenceChain增加两个车型独立生成及参数编辑隔离检查，核对固定共享Local、新Presentation／Pawn引用、原生初值、17组件、共享网络、空AnimGraph和共享资产保护。UnrealEditor-Cmd以NullRHI启动 `HAL.Editor.VehicleAssetGenerator` 后退出码0，但标准输出／错误均为空且没有日志／报告，不能确认测试执行或任何通过数量。没有本轮面板操作、渲染截图或保存后重启证据。

本轮667份 `.uasset`／`.umap` 和151份运行时源码／配置逐文件SHA-256全同；未生成或保存Content资产，没有增加模块或修改菜单入口。详细见[本轮验证摘要](Vehicle_Asset_Generator_IndependentDefinition_Verification_20261023.json)。

团队验收范围为：Editor Automation中的三项核心测试，以及面板选择新车型Mesh／共享Network DA后生成并保存4/4；重启后核对第3节全部引用，确认两个车型DA分别独立、Local／Network继续共享，ABP只有默认输出节点。车型必填模型、动画与参数校准完成后，再进行驾驶、控球、发射及Listen Server测试。现有Car01和共享DA保持原有内容。
