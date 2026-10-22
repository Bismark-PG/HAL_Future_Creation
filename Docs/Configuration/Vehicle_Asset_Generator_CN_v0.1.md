# 车辆资产生成工具

日期：2026-10-22。工具属于现有 `HAL_Future_CreationEditor` 模块，通过 Editor 的 **Tools → HAL Tools → HAL Vehicle Asset Generator** 启动。公开接口位于 `Source/HAL_Future_CreationEditor/Public/VehicleAssetGenerator`，实现位于对应 Private 目录。

2026-10-22同步后恢复：当前接续基点为 `main/5f6c9ef`。资产生成工具的窗口注册、Tools菜单和关闭时注销已在云端最新模块入口上补回；现有碰撞生成、视觉高度校准入口同时保留。最新 Build.cs 已具备运行时模块依赖及平铺头文件目录，本次保持它的云端内容。HAL Tools 是 Tools 菜单内的分组标题，界面中可能直接列出三个工具。

## 1. 生成内容

以 `SKM_Car_02` 为例，工具固定生成四份资产：

| 资产 | Content 路径 | 初始设置 |
|---|---|---|
| `ABP_Car_02` | `Blueprints/AnimBP` | ParentClass 为 `HALVehicleAnimInstance`，TargetSkeleton 为所选 Mesh 的实际 Skeleton，PreviewSkeletalMesh 为所选 Mesh |
| `DA_Car_02_Presentation` | `Data/Car_Presentation` | 引用所选 Mesh 和新 AnimBP 的生成类；其余表现字段使用原生初始值，Ready For Use=false |
| `DA_Vehicle_Car_02` | `Data/MVP_B` | 复制所选基础 VehicleDefinition，将 Presentation 替换为新表现 DA，Ready For Use=false |
| `BP_VehiclePawn_Car_02` | `Blueprints/Vehicles` | 继承所选基础车辆 Blueprint，ConfigurationSource=Definition，Definition 引用新车辆 DA |

车型取 Mesh 名称中 `SKM_` 后的完整内容，保留下划线和大小写。工具使用 `/Game` 虚拟包路径，输出目录固定。任一目标名称已存在，包括尚未保存的内存包，整组创建都会被阻止；没有覆盖或自动编号。

AnimBP 由引擎工厂创建，仅保留默认空 AnimGraph 和默认输出节点。工具不增加 Modify Bone、状态机或动画接线。创建期间进行必要编译，以建立可供表现 DA 引用的生成类。

## 2. 面板使用

1. Skeletal Mesh 选择名称为 `SKM_<车型>` 且具有 Skeleton 的模型。
2. Base Vehicle Blueprint 选择已经编译的玩家车辆蓝图，例如现有 `BP_RoundedVehiclePawn`。新车型作为其子类，继承组件和网络物理设置；基础蓝图本身保持原样。
3. 选择基础蓝图时，Base Vehicle Definition 自动读取其 Class Defaults 中的 Definition，也可手动选择另一份有效的驾驶车辆 DA。
4. 面板显示四个完整输出路径及检查结果。Create Asset Set 创建四份尚未保存的资产，并在 Content Browser 定位它们。
5. Save Generated Assets 仅保存本组四份资产。保存结果逐项处理，失败项显示路径，已成功项保留，可再次保存。源模型、基础蓝图、基础 DA 和共享资产不会被该按钮保存。
6. Locate Generated Assets 可重新定位结果。上一组通过工具保存后，可继续选择另一个模型创建下一组。

Use Selected Sources 从 Content Browser 读取选择，最多接受一个 Skeletal Mesh、一个 Blueprint 和一个 VehicleDefinition；同类型多选会明确拒绝，避免依赖选择顺序。

PIE 或 Simulate 期间禁止创建与保存。关闭工具面板不会自动保存资产，创建后的资产遵循 Editor 常规未保存资产管理。

### 2.1 从生成到接入测试

| 阶段 | 配置位置与结果 |
|---|---|
| 加载工具 | 完整构建 Editor Target 后重新打开项目；Tools 菜单的 HAL Tools 分组提供 HAL Vehicle Asset Generator |
| 选择输入 | Mesh 选择新车型；基础蓝图通常选择 BP_RoundedVehiclePawn，面板自动读取其 Definition；也可单独更换基础 Definition |
| 创建并保存 | 核对输出路径，Create Asset Set → Save Generated Assets；结果应为4/4，Locate Generated Assets可再次定位 |
| 配置动画 | 新 ABP 的父类和 Skeleton 已设置；团队手动配置轮转／转向及需要的 Chassis 节点，生成器保持 AnimGraph 默认内容 |
| 校准车型 | 新表现 DA 的四轮骨骼、轴向、方向、实际半径及模型变换由团队配置；车辆 DA 的碰撞、物理、控球点及回退外观也需核对 |
| 高度校准 | 完成表现数据后，Content Browser选中新 VehicleDefinition，使用现有 HAL Vehicle Visual Alignment；具体限制与保存流程见[校准工具说明](Vehicle_Visual_Alignment_Tool_CN_v0.1.md) |
| 启用配置 | 校准完成后，两份新 DA 分别设置 Ready For Use，保存并执行 Validate Assets；输入和摄像机仍使用原 LocalConfig |
| 接入测试 | 新 BP 的 Definition 已绑定。团队在测试关卡或 GameMode 中手动选用新 BP，重新 Play，检查驾驶、动画、持球与发射；主机和客户端需要同一套已保存资产 |

工具不自动替换当前 GameMode 的默认 Pawn，不放置关卡实例。已放置实例可能保存 Definition 覆盖，应核对其实际引用。相同 Mesh 再次生成会因固定名称冲突被拒绝，已生成资产的后续配置直接在对应编辑器完成。

## 3. 引用和待配置事项

引用为 `BP_VehiclePawn_<车型> → DA_Vehicle_<车型> → DA_<车型>_Presentation → Mesh / AnimBP`。

LocalConfig、碰撞 Static Mesh、物理材质及回退外观等外部引用保留为共享引用。Combat 与 Stability 继续由项目设置提供，工具不复制这些 DA，也不修改 GameMode、PlayerState、地图或输入映射。

基础车辆 DA 的驾驶、控球、战斗、物理和 Marker 数值会被复制。新车型的碰撞大小、质量适配、模型变换及控球点仍须检查，工具不根据视觉包围盒自动推断玩法物理。

新表现 DA 不继承 Car01 的轮轴、半径、模型缩放和俯仰调参。其 Root／Chassis 和四轮名称来自当前 C++ 默认约定，半径初值为0。缺少控制骨骼不会阻止创建配置草稿，但会阻止表现配置通过运行时校验。

两份新 DA 均保持 Ready For Use=false。团队在 AnimBP 完成所需骨骼接线，校准四轮轴向、方向、实际半径和模型对齐，检查碰撞与控球点后，再启用 Ready 并执行资产校验及单机／双端基本测试。车辆 DA 未 Ready 时会拒绝参与玩法，此状态不能作为可直接驾驶的交付。

## 4. 首次实现验证记录

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
