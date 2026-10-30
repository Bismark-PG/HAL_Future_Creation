# 生成车辆蓝图的输入引用显示

## 配置链与显示方式

生成器直接以原生 `ATestVehiclePawn` 创建玩家蓝图，连接本车型独立 Definition、Presentation、AnimBP 和所选共享 Network Physics。车辆 Definition 的 Local Config 继续引用共享 `DA_Local_TestBaseline_v1`，各车型无需保存一组重复输入配置。

原来 Class Defaults → Vehicle → Input 显示的是 Pawn 保存的旧默认值。Car_01 的旧蓝图保存过输入引用，生成器创建的 Car_02 没有这些副本；运行初始化虽会读取 Local Config，原编辑器界面缺少对应显示。

本轮在现有 `HAL_Future_CreationEditor` 模块注册车辆 Details 定制。单个 Definition 模式的蓝图 Class Defaults／编辑器预览对象，会在 Input 中直接显示 Local Config 来源，以及 Mapping Context 和全部九个 Input Action。数据来自当前 `Definition.LocalConfig`，修改／替换／清空共享引用后再次读取当前值；草稿 DA 未 Ready 也可以查看。缺失来源或可选 Action 显示 None。

输入行只读，保留浏览资产入口；编辑仍发生在 Local Config 及其输入映射资产中。显示过程不复制输入到 Pawn，不保存 Blueprint／DA，不更改输入消费、物理或权威规则。切换 Configuration Source 会刷新布局；Legacy、运行中的游戏世界及多对象选择继续使用原有 Details 行。

现有 `BP_VehiclePawn_Car_02` 和之后生成的车型都继承该显示逻辑，现有资产无需重新生成。

## 生成器与运行初始化

生成器原有接线保持：

| 引用位置 | 来源 |
|---|---|
| 车辆蓝图 Definition | 本车型 `DA_Vehicle_<车型>` |
| Definition.LocalConfig | 共享 `DA_Local_TestBaseline_v1` |
| Definition.Presentation | 本车型 `DA_<车型>_Presentation` |
| Presentation.Mesh／AnimClass | 所选模型／生成的 AnimBP 类 |
| 网络物理 SettingsDataAsset | 工具所选共享 Network Physics DA |

运行时仍由 `ATestVehiclePawn::ApplyDefinition(false)` 将同一 Local Config 的引用初始化到车辆，再由 Enhanced Input 管线消费。新增回归检查新生成蓝图的预览来源，以及在临时有效配置和世界中生成该蓝图、执行初始化后十项输入引用的一致性。

测试只在临时生成配置中填写有效凸包、选择静态表现并启用 Ready，以隔离空 AnimGraph／轮组校准。正常生成结果继续保持两份 DA 的 Ready=false；车型碰撞、表现／骨骼接线及校准仍由团队按原流程完成。引用显示正确不等于未配置的车型已通过玩法校验。

运行初始化测试沿用项目既有临时Legacy规则夹具，验证范围为生成类的输入初始化；当前保存车型的TieredV1迁移、共享规则校验及完整玩法继续独立验收。

## 使用与验证

1. 当前统一入口为根目录 `BuildAndLaunch.cmd`。保存退出 Editor／游戏／Live Coding 后运行，编译成功后会启动工程；只编译可用 `-NoLaunch`。已删除的 `Build_Editor.cmd`无需恢复。
2. 在已有或新生成车辆蓝图的 Class Defaults → Vehicle → Input 核对 Local Config、Mapping Context、驾驶／发射、技能、自救和相机 Action。字段保持只读。
3. 共享引用在 Local Config 中配置，具体按键在所引用 IMC 中配置。修改共享配置会影响使用它的车型；输入显示不会向蓝图保存重复值。
4. 完成车型配置并按原流程启用 Ready／Validate 后进行驾驶初测；当前正确引用还需要有效配置才能进入玩法。

本轮自动化入口为 `HAL.Editor.VehicleInputPreview` 和 `HAL.Editor.VehicleAssetGenerator`。前者检查实时来源、实际 Details 资产控件、空引用、Legacy 与无副作用；后者检查四份资产接线和生成蓝图的运行输入初始化。真实编译／运行结果以最新项目状态及本轮验证留档为准，不引用旧测试作为本次通过证据。

### 2026-10-30验证记录

在工程内独立快照中使用本机UE5.8.3-58210709完整构建Editor Target成功，包含Runtime和Editor两个模块；同步最终两份测试源后，显式刷新快照源时间戳再次编译并链接成功。主工程218份Source文件与最终快照逐文件SHA-256一致；1172份原始保存资产相对本轮开工均未变化。没有替换正在运行的主工程DLL或保存原始Blueprint／DA／地图。原相机测试C4305及UBA结果存储提示保留，不宣称零编译警告。

新增两项输入预览测试和一项生成蓝图运行初始化测试已编译，原三项生成器回归仍保留，合计六项待运行验证。尝试UnrealEditor-Cmd及普通Editor离屏方式均没有生成Unreal日志或自动化报告，标准输出／错误为空；直接Cmd方式退出码0也不作为执行通过证据。**本轮未确认任何UE测试执行，实际Details显示、保存车型的玩法校验和真人驾驶仍待团队验证。** 编译通过与测试通过分别记录。

现有Car_02无需重新生成。团队保存退出Editor后运行统一入口，再按本节步骤检查Input，并在Session Frontend运行上述两个过滤组。详细证据见[本轮验证](Vehicle_Input_Preview_Verification_20261030.json)。

### 2026-10-30主工程收尾核对

统一入口补齐`-NoHotReload -NoLiveCoding`，并在脚本夹具中断言正常构建和官方Rebuild都包含这些参数；最终13项隔离检查全部通过。原Editor模块已包含生成器、引用预览及六项相关测试，新增源文件会随该Target编译，旧`Build_Editor.cmd`保持删除。

本轮主工程使用本机UE5.8.3完成29项编译／链接／元数据任务，生成新版Runtime和Editor DLL。UBA启用的主工程构建在收尾阶段未能退出，相关三个专用会话已结束，不能记作统一入口完整运行成功；随后使用同一Target和完整构建参数加`-NoUBA`调用原生Build.bat，返回`Result: Succeeded`、退出码0，确认最终Target最新。原日志保留既有C4305及UBA缓存提示。临时环境变量只作用于验证进程，未改变保存的构建配置。

主工程218份Source与先前成功构建的隔离快照逐文件相同；1172份保存资产相对原基线逐份SHA-256全同。再次直接启动隔离UnrealEditor-Cmd仍未产生日志或报告，不能记录六项UE测试执行或通过；实际Details显示、当前保存车型校验和真人驾驶仍待验收。当前新版主工程DLL已经生成，下一入口为重新打开Editor检查Car_02的Class Defaults → Vehicle → Input，再运行本页列出的两组自动化。两份新DA的Ready与车型配置要求继续适用。

编译、脚本夹具、资产完整性与未完成检查见[结构化证据](Vehicle_Input_Preview_Verification_20261030.json)中的`completion_verification`。
