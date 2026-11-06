# 车辆通用状态 FX：FX-2 共用配置、预加载与 Cook v0.1

日期：2026-11-06；开工基点为干净 `main/4e5d586`，已包含 FX-1；本机 UE5.8.2-56702186。

本环节交付共享配置和资源准备入口。总体顺序见[六环节方案](MVPB_Vehicle_Status_FX_Discussion_CN_v0.1.md)。**FX-3运行实例、FX-4材质／粒子适配、FX-5电击迁移、FX-6组合验收尚未完成。** 本环节没有将新DA接到现有电击渲染，因此配置后电击外观仍由旧组件控制。

## 1. 各层的职责

| 类型／位置 | 内容 | 维护方式 |
|---|---|---|
| Project Settings → Game → Vehicle Status FX | Default Catalog，一份项目默认目录 | 团队在Editor设置一次，落到DefaultGame.ini |
| `UHALVehicleStatusFXCatalog` | 精确视觉Tag → 共用效果定义DA | 共用配置维护者；不复制每车运行状态 |
| `UHALVehicleStatusFXDefinition` | Overlay材质、参数／曲线、Niagara模板、表现淡入淡出／残留及表面优先级 | 美术／特效维护，每效果一份；支持后续C++派生扩展与校验 |
| `UVehicleDefinition.StatusFXCatalogOverride` | 可选的例外Tag目录 | 普通车型留None；仅列特殊Tag，未列Tag仍继承默认目录 |
| `UHALVehicleStatusFXSubsystem` | 每GameInstance资源缓存、异步加载、配置验证与就绪通知 | 原生自动创建；不是每车的MID／粒子或Buff管理器 |

目录Tag必须是 `State.Visual` 的有效子Tag，例如当前已有的 `State.Visual.Shocked`。采用精确匹配，不按父Tag或数组首项猜效果；重复Tag使该目录无效。不同Tag可以引用同一份定义，层ID和参数名在各自定义中唯一。

目录内定义、定义内材质／Niagara／外部曲线及车型的例外目录均为**硬引用**，因此加载目录会带入其资源依赖。项目默认引用为软引用，由异步入口加载，并由Cook收集入口显式加入打包根。正常新车不需要额外填写状态FX引用。

这些DA不存HP、冲击档位、Buff剩余时间、MID、Niagara组件或任何车辆运行时状态。Fade／Residue是视觉尾迹，GAS仍决定实际状态何时结束。规则不依赖本地资源是否就绪。

## 2. 效果定义字段

| 字段 | 配置语义／校验 |
|---|---|
| Overlay Material | 可选共享表面材质；至少具备该材质或一个完整Niagara层 |
| Surface Priority | -1000～1000；为FX-4不兼容表面样式的选择保留，不影响伤害与GAS |
| Scalars／Colors | 材质参数名和值；数值须有限，名称不能空或跨列表重复 |
| Curves | 材质标量参数随视觉实例开始后的秒数变化；至少一个有效关键帧，时间非负；支持内嵌或外部CurveFloat |
| Niagara Layers | 各层唯一Layer Id；共享模板引用，不是运行组件 |
| Binding = Bounds | 近似包围模式，只填Bounds System，不同时填写两个Surface模板 |
| Binding = Mesh Surface | 真实表面模式的数据入口；须分别填Static Surface System和Skeletal Surface System，不填Bounds System |
| Padding Fraction | 0～1，相对Mesh Extent的包围余量；不改变车辆物理或碰撞尺寸 |
| Fade In／Fade Out Seconds | 各0～10秒的视觉过渡配置 |
| Residue Seconds | 0～30秒的可选视觉残留，不能延长实际Debuff |

本环节只检验数据结构、有限值、Tag和资源引用。它**没有验证材质一定存在同名Shader参数、Niagara采样接口／CPU访问／LOD正确，或实际创建上述表现**；这些在FX-4实现适配、FX-5迁移时验证。可填写这些配置不代表当前已支持播放所有样式，也不能将加载Ready当成GPU／Shader预热完成。

没有新增Ready For Use勾选。配置检查直接依据实际数据；项目未配置目录时为Unconfigured，保持可驾驶、可进入完整流程。

## 3. 团队Editor操作

本节由团队手动完成；代码代理没有创建或修改`.uasset`／`.umap`。可现在建立最小目录，或在FX-5迁移时补齐实际资源；暂时留None不阻塞后续C++建设。

### 3.1 建立一份最小电击定义

1. 关闭Live Coding，使用本次编译后的Editor。Content Browser进入或新建 `/Game/Data/StatusFX`。
2. 右键 → Miscellaneous → Data Asset，在父类列表搜索并选择 **HALVehicleStatusFXDefinition**，命名 `DA_StatusFX_Shocked`。
3. 打开DA，先只填写 **Surface → Overlay Material**。从目前已调好的 `BP_RoundedVehiclePawn` 中查看 `ShockedPresentation` 组件的Overlay Material资源，使用同一材质引用即可。这里只读取旧配置，**不修改旧BP、不用原生初值覆盖旧参数／曲线**。
4. 当前可保持Scalars、Colors、Curves、Niagara Layers为空，Timing维持新DA默认。这样得到的是可验证的最小配置，并非已复刻当前电击外观；FX-5施工前会另行保存并迁移实际基线。
5. Save。不要为了本环节复制车辆BP、改PlayerState／GA／GE或替换旧电击组件。

### 3.2 建立项目共用目录

1. 同文件夹右键 → Miscellaneous → Data Asset，选择 **HALVehicleStatusFXCatalog**，命名 `DA_VehicleStatusFX_Default`。
2. 打开DA，在 **Effects** 添加一项。
3. Tag选择 **State.Visual.Shocked**，Definition指向上一步的 `DA_StatusFX_Shocked`。不要重复添加相同Tag。
4. 保存两个DA。选中它们，右键 → Asset Actions → Validate Assets（验证资产，名称随Editor语言略有不同），检查Message Log中没有本资产的配置错误。
5. 空目录、缺定义、无材质且无Niagara层、重复Tag／层ID／参数名等会明确报错。根据条目序号、Tag和定义路径定位后修正；不通过勾选Ready绕过。

### 3.3 指定一次项目默认

1. Edit → Project Settings，在 **Game → Vehicle Status FX** 找到 **Shared Configuration → Default Catalog**。
2. 指向 `/Game/Data/StatusFX/DA_VehicleStatusFX_Default`。确认配置写入工程 `Config/DefaultGame.ini`，而非仅留本机未保存改动。
3. 普通车型DA，包括当前玩家及Car_02／Car_03，在 **Visual → Status FX → Status FX Catalog Override** 保持 **None**。无需逐辆重复引用默认目录。
4. 停止并重新开始游戏进程。GameInstance初始化会异步准备默认目录，车辆BeginPlay也会确保本车默认／例外目录已请求；不会在命中回调中同步加载。
5. 在运行中的游戏控制台输入 `hal.StatusFXStatus`，在Output Log或该进程Saved/Logs日志中搜索 `StatusFX`／`Status FX catalog`。
6. 预期看到DefaultCatalog为所选目录、State从Loading到 **Ready**、Effects=1；普通车辆的Catalog路径相同。None显示 **Unconfigured**；Failed附带路径及原因；NotRequested说明该目录尚未进入准备入口。
7. 此时可见电击仍按旧组件播放。仅修改新DA暂时不改变画面，是阶段边界的预期结果。

### 3.4 真正有例外车型时

1. 创建该车型专用的 **HALVehicleStatusFXCatalog** DA，只列需要替换的Tag，并引用对应的共用或例外效果定义。
2. 在该车型VehicleDefinition的 **Status FX Catalog Override** 指向它。未列出的Tag自动查询项目默认目录，默认目录以后新增效果时不需同步复制整表。
3. 同一个Tag在本目录仍只能有一项。显式例外目录无效或未就绪时，本车查询返回无表现，不悄悄换成另一套样式；不会禁用驾驶／伤害／开局。
4. 例外目录及其定义也需验证和保存。没有具体需求时继续留None。

## 4. 生命周期、加载与Cook约定

共享子系统以GameInstance为边界，跨练习／比赛地图保留已准备的只读目录；每个Host／Client进程独立加载。相同路径重复请求不重复发起加载，完成后保留目录硬引用并释放streaming handle；退出时取消在途请求、释放缓存、清理委托，延迟回调不能重新填回已清空状态。

`FindEffect`只读就绪缓存，不启动加载。加载失败只记录一次可追踪诊断；修改配置后重启当前游戏实例再试。当前没有新增复制字段、RPC、状态粒子Tick或新的玩法许可条件。

Cook时，项目模块通过引擎 `FGameDelegates::GetModifyCookDelegate` 收集Default Catalog所在包；Cook-only入口同步检查该目录和定义有效，再沿普通硬依赖包含资源。无需另配AssetManager或把整个资源目录Always Cook。车型例外沿原有车型Definition／BP的引用进入Cook，前提仍是该车型本身已被正式流程纳入打包。

若显式配置的默认目录缺失／无效或同时标为NeverCook，Cook输出Error，要求修复已配置资源；这不是游戏开局门槛。特殊命令行若关闭GameAlwaysCookPackages会绕过引擎此委托，不能用该裁剪方式验证此功能。正式打包后仍须检查资源实际存在、首次效果性能及画面，不能以原生引用测试代替发布包验收。

团队同步时提交新DA、所有新增依赖和`Config/DefaultGame.ini`，各测试电脑使用相同版本。这些本地资源不会通过Host网络复制到其他电脑。

## 5. 验证记录

代码、构建与下述程序验证完成；团队创建目录后的Ready检查、实际Cook和可见画面验收尚未执行。详细构建／报告路径、哈希保护清单与失败记录见[本次验证记录](MVPB_Vehicle_Status_FX2_Verification_20261106.json)。

- UE5.8.2 Editor／Game构建成功；最终Editor为R3、Game为R1。此前完整Editor构建输出原有`Tests/HALCameraTests.cpp`的C4305精度转换警告，该文件没有修改。
- FX-2配置／资源／生命周期测试 **5/5**，全量`HAL.FutureCreation` **153/153**；两份报告均0测试警告／失败／未运行／进行中。
- 五项覆盖配置有限值／重复名、精确Tag与嵌套诊断、默认／局部覆盖及失败不阻断车辆配置、目录至材质／Niagara／外部曲线硬引用、Cook根收集，以及真实Streamable请求的重复／失败／取消／延迟回调。Cook根使用内存包检验，没有保存测试资产，也未据此声称正式资源已打包。
- 实际双进程Listen Server重跑原GF7三局流程，Host／Client各 **1/1**，均0测试警告／失败／未运行／进行中、退出码0。两端启动日志均确认`StatusFX DefaultCatalog= State=Unconfigured Catalogs=0`，表明本地子系统实际存在，未配置新目录不妨碍练习／比赛／返房。夹具直接从PracticeRoom启动，使用同机NullRHI，不代表Entry界面、已配置新FX的画面或校园四机验收。
- 首轮Config_R1在重复Tag夹具中因`TArray.Add(自身元素引用)`触发重叠断言，退出码3、未生成最终报告；改为独立副本后严格重跑。异步及Cook引用两项此前已过，后续仅测试文件改变；失败日志保留，不以放宽生产校验处理。
- 开工前后 **1190份保存资产逐份SHA256相同**，最终 **312份Source／Config／uproject**与Editor R3构建前冻结一致；Config／Build.cs／uproject未改，未直接编辑／创建BP、DA或地图，未提交／Push，专用测试进程已退出。

新默认目录目前仍为None。第3节的团队接线、保存DA真实加载为Ready、实际发布包依赖检查、首次效果／GPU性能及四机画面均保持待验；当前电击没有接入新目录。

## 6. 下一入口

FX-3：每车运行实例和生命周期，消费视觉进入／更新／退出，聚合多来源，创建并清理本车资源，处理外观重绑与换车／跨图。FX-4接材质协调与通用粒子适配，FX-5才将已调电击资源／曲线迁入，保留当前本地回弹提前反馈及权威接续。
