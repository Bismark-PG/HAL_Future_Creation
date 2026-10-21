# 车辆表现 VP-0 代码核对／VP-1 底座实施记录（v0.1）

日期：2026-10-19（Asia/Tokyo）。基点 `main / ca90ffb9b85c1fef20bdde81fa562ace8882b170`，UE `5.8.3-58210709+++UE5+Release-5.8`。起始保留上一轮未提交的方案、截图、状态文档与 Git 白名单，不覆盖它们。

用户已授权开发代码侧准备与对应接口，并要求完成后提供模型导入配置文档。本轮实现 VP-0 的代码接口核对和 VP-1 的原生底座；实际素材导入、局部轴／半径校准和 AnimBP 资产接线仍由团队完成。VP-2 车身动态、VP-3 视觉悬挂、VP-4 受击／发射回弹未施工。

**2026-10-21进度补充：用户确认VP-1结束，Car_01素材及ABP／DA／玩家引用已由团队完成并提交，下一入口VP-2具体方案／施工准备。** 上文及第6节保留10月19日代码交付时点，当前结论与未关闭的测试夹具问题见第8节。

## 1. 交付入口

- [模型导入与 AnimBP 配置流程](MVPB_Vehicle_Model_Import_And_AnimBP_Setup_CN_v0.1.md)：供导入／配置成员直接执行。
- [总体方案](MVPB_Vehicle_Presentation_Rig_Plan_CN_v0.1.md)：后续范围、骨骼截图、线程／联网原则。
- [项目状态](../Project_Status_CN.md)：当前进度与接续入口。
- [验证摘要](MVPB_Vehicle_Presentation_VP1_Verification_20261019.json)记录本轮实际构建、普通自动化、基本双端和资产保护结果；没有实际模型的原生测试不替代骨骼画面验收。

## 2. 实际源码与职责

| 文件 | 本轮内容 |
|---|---|
| [HALVehiclePresentationConfig](../../Source/HAL_Future_Creation/HALVehiclePresentationConfig.h) | 可选车型 Mesh／AnimClass、相对变换、四轮骨骼／半径／轴／符号、转向及采样参数，运行／Editor 校验 |
| [HALVehiclePresentationTypes](../../Source/HAL_Future_Creation/HALVehiclePresentationTypes.h) | 明确 FL／FR／BL／BR 轮位、局部轴、轮转／转向姿态帧、独立数学函数 |
| [HALVehiclePresentationComponent](../../Source/HAL_Future_Creation/HALVehiclePresentationComponent.cpp) | 游戏线程取数、分轮点速度、轮角积分、转向平滑、异常间隔／瞬移／控制者切换重置，以及远端转向复制 |
| [HALVehicleAnimInstance](../../Source/HAL_Future_Creation/HALVehicleAnimInstance.cpp) | 原生游戏线程更新拉取帧，AnimGraph 只读继承的 Presentation 数据 |
| [TestVehiclePawn](../../Source/HAL_Future_Creation/TestVehiclePawn.cpp) | 新增 SkeletalVisual 与 VehiclePresentation 默认子对象；按 Definition 选择可选表现 |
| [VehicleDefinition](../../Source/HAL_Future_Creation/VehicleDefinition.h) | 新增可空 Presentation 引用；原 Visual 继续作回退，原玩法校验不依赖可选配置有效性 |
| [ArcadeVehicleMovementComponent](../../Source/HAL_Future_Creation/ArcadeVehicleMovementComponent.cpp) | 发布在物理步中实际消费的转向／帧／接地纯值，使用现有锁复制；没有新增施力或修改历史协议 |
| [表现普通测试](../../Source/HAL_Future_Creation/Tests/HALVehiclePresentationTests.cpp) | 轮转数学、帧率、配置／骨骼约定、旧车回退、AnimInstance 快照与重置 |
| [表现网络测试](../../Source/HAL_Future_Creation/Tests/HALVehiclePresentationNetworkTests.cpp) | 真实双进程中本地、服务器远端与客户端观察的转向路径，活动与归零 |

## 3. 初始化与回退

1. 现有物理根和 Static Mesh 默认子对象名称不变，新增独立无碰撞／不模拟物理的 SkeletalVisual。
2. Legacy 模式或 Definition.Presentation 为空时沿用原视觉；不启用表现计算 Tick，不发送新增表现转向变化。
3. 选中配置后，校验 Ready、资产、Skeleton、骨骼链、四轮身份、实测半径、轴／符号、数值与相对变换。
4. 配置无效时输出可定位的 fallback 警告，保留旧 Static Mesh；不因此将车辆 bConfigurationValid 设为 false，不禁用驾驶／球／ASC。
5. 配置有效时保存原 Static Mesh 可见性，隐藏它，只显示骨骼路径；退出／重新配置时恢复原可见性并清理快照。

新增引用为驾驶车辆专用可选项，没有把球／被动车的共享 FVehicleVisualConfig 改成骨骼配置。没有更换权威碰撞根、质量、惯性、尺寸、控球点、发射方向、驾驶参数、伤害／GAS 数值或网络物理状态序列化。

配置默认 Ready=false、四轮 RadiusCm=0。默认骨骼名来自用户截图，但 Z 转向轴／Y 滚动轴和 +1 符号仅是待核对种子；实际模型导入后必须校准，不能直接把默认配置当成已验证模型。

## 4. 数据、更新与联网

### 4.1 连续表现

- 游戏线程通过外部物理 API 获取线速度、角速度、质心，按缓存的参考轮心计算 `V + W × R`。
- 轮心速度投影到随视觉转向改变的滚动方向，再除以实测世界轮半径积分角度；支持倒车、镜像轴符号、低速死区和角度环绕。
- 前轮用指数平滑跟随视觉转角，默认后轮不转向。它不修改现有车辆转向力矩。
- 该轮转为装饰估算，不计算真实轮胎牵引／打滑。本轮腾空时仍按运动状态估算轮转；空中保持／衰减策略留给后续 VP-2 体验讨论。
- 参考轮心从参考骨骼姿态计算并缓存，不从本帧已经动画变换的轮子反读，避免反馈漂移。

### 4.2 线程与时序

- 新增的物理输入快照只在已有物理步写入，借用现有 PendingPhysicsDataLock；游戏线程 getter 返回值拷贝，物理停用时返回无效，避免读到陈旧输入。
- 表现组件在 PrePhysics 更新，Mesh 动画 Tick 对它建立前置依赖；采样的是安全的外部状态／已完成物理输入，不承诺其与下一次异步物理步同帧。
- 原生 NativeUpdateAnimation 拉取一次帧拷贝，求值阶段不读 Pawn／ASC／UWorld／物理内部对象，也不在 AnimGraph 积分。
- 大间隔、无效数值、位置突变、控制者变化会重置采样与轮角；恢复首样本只播种，不补算整段停更时间。
- 目前四车规模使用 AlwaysTickPoseAndRefreshBones 保持快照与可见恢复简单；实际资产接入后测量开销，再决定 LOD／可见性优化。

### 4.3 网络角色

| 角色 | 转向来源 |
|---|---|
| 本地拥有车（含 Listen Server 本人） | Pawn 已有的本地 FVehicleInputCmd，立即反馈 |
| 服务器上的客户端车 | Movement 在物理步实际消费并安全发布的输入，不读取服务器上通常为空的本地命令 |
| 客户端上的其他车辆 | 服务器复制的 ObserverSteering，量化为 [-127,127]，按 127 还原 |

ObserverSteering 使用默认子对象组件复制，SkipOwner；服务器最多 15 Hz 更新变化并请求 Actor 更新，不新增客户端控制 RPC，不高频可靠广播，不复制每轮角度。实际送达频率受连接／Actor 复制调度限制，15 Hz 是发布上限，不是保证网络频率。

本轮没有离散发射／冲击回弹，因此也没有新增命中演出事件。后续 VP-4 仍需独立处理预测确认、去重、迟到与事件传输；不能因本轮有转向复制就认定该部分已完成。

## 5. 实际 AnimBP 接口

继承 UHALVehicleAnimInstance，读取 Presentation：

- `FL/FR/BL/BR.SteeringRotation`、`FL/FR/BL/BR.SpinRotation`：直接连接 Modify Bone Rotation。
- `bValid`、`SteeringDegrees`、`ForwardSpeedCmPerSecond`：诊断数据。

没有 Chassis／Suspension 偏移输出；它们是后续范围。首轮图为 Reference Pose→Local To Component→前轮 steer／四轮 spin→Component To Local→Output。手工选择目标骨骼，旋转 Add to Existing／Bone Space，平移与缩放 Ignore，Event Graph 无计算。

## 6. 验证记录

最终结果见[验证摘要](MVPB_Vehicle_Presentation_VP1_Verification_20261019.json)。本轮源码完成后实际执行如下检查，不用构建成功代替实际模型接线验收：

| 检查 | 本轮实际结果 | 证据与边界 |
|---|---|---|
| Editor Win64 Development | 成功 | `Saved/VehiclePresentation/Editor_TestFixtureFixed.log`，完整Target、无Live Coding |
| Game Win64 Development | 成功 | `Saved/VehiclePresentation/Game_Final.log`，与最终源码一致 |
| 普通自动化 HAL.FutureCreation | **49/49，0测试警告／0失败** | 原45项＋新增4项；`Automation_Final/index.json` |
| 真实Listen Server／客户端基本数据检查 | **Host 1/1、Client 1/1，均0测试警告／0失败** | 独立两个`-game`进程，NetTest／7798／loopback，未主动注入扰动，未测RTT；非实际模型画面验收 |
| 资产保护 | **331→331，全部哈希一致** | 没有保存／修改uasset或umap；清单及哈希随摘要归档 |
| 进程清理 | Host PID3196／Client PID29172均正常退出0 | 检查结束后无Editor／Cmd／LiveCoding进程；本次专用进程不残留 |

双端活动时主机本车／远端车约为`+16.250°/-15.485°`，客户端约为`-15.670°/+15.756°`，观察转向量化值为`±0.654`。归零时各角色均收敛至约`±0.013°`以内。服务器测试确认负向转向来自实际消费的客户端物理输入；客户端确认正向观察值来自复制状态。上述为采样点，不能据此推导网络RTT或宣称所有视觉帧完全一致。

首轮 Editor 构建发现复制宏依赖形参名 OutLifetimeProps，已修正；随后补充物理停用时拒绝陈旧快照的保护与配置契约检查。构建保留引擎 C4996 弃用提示和 MSVC 14.51 新于推荐 14.50 的既有警告，不宣称零编译警告。

普通自动化过滤器为 HAL.FutureCreation，本轮新增 4 项表现测试。网络过滤器为 HAL.NetworkSmoke.VehiclePresentation；它需要匹配的 Listen Server／客户端进程。

首次普通回归47/49成功、2项夹具失败：合成骨架在Modifier重建前误用最终骨骼数量，以及本地测试Controller缺少HALPlayerState／世界上下文。修正夹具后重新完整构建并运行普通回归，最终49/49成功。首次报告保留为`Automation_Initial/index.json`，不覆盖成通过记录；生产逻辑未因夹具错误改动。

网络测试使用仅开发自动化可见的运行实例探针，绕过“实际模型尚未导入”这一素材条件来检验原生数据计算和复制；不保存、生成或修改 Blueprint／模型资产。测试只证明相应数据链路，不能证明真实 Mesh／AnimGraph 轴向、蒙皮、LOD、渲染或玩家观感。

## 7. 代码交付时的限制与接续（2026-10-19历史）

1. 团队尚未导入／交付本次骨骼模型；实际骨骼空间、轴心、符号、半径与材质待核验。
2. AnimBP 及表现 Data Asset 尚未由团队创建，当前已保存车型仍走原 Static Mesh；代理未编辑 uasset／umap。
3. VP-0 代码接口侧已准备，素材侧未完成；VP-1 应分别记录原生实现／编译／原生测试与团队接线／观感。
4. 模型到位后按导入流程先完成逐轴校准和六节点接线，再做主客互换基本验证；发现真实接线问题及时修复。
5. 其后推进 VP-2 车身驾驶表现、VP-3 视觉悬挂、VP-4 回弹；物理悬挂／落地辅助另行设计。
6. 弹簧拳仍由另一成员实现，本轮未修改该模块。10-F-4 完整组合／扰动／四机仍后置到 MVP-B 收尾，没有提前执行或标记最终通过。

交接前将源码、方案、操作文档与验证摘要正常提交 Git。当前工作区修改尚未提交／Push，不依赖 Saved 报告目录在其他设备自动存在。

## 8. VP-1收尾与进度同步（2026-10-21）

用户本次确认VP-1结束，作为本步团队收尾结论；此前“尚未导入／待创建AnimBP／DA”和前轮侧倾排查不再作为当前阻塞。实际素材／接线已提交：2e4176d包含Car_01模型／材质／贴图，a11228a包含ABP_Car01、DA_Car01_Presentation、玩家BP及VehicleDefinition引用，fd41bbb包含后续碰撞代理／地图／Definition调整。同步时HEAD为干净main/21d29b08c17f015cd4e4568779ee441608a149cf，本机UE5.8.3。

| 证据类型 | 当前结论与范围 |
|---|---|
| 规格／代码／历史构建 | VP-1原生实现已完成；10月19日Editor／Game成功及49/49是当时基线证据，第6节和原JSON不改写 |
| 素材／Editor接线／团队收尾 | 素材和ABP／DA／玩家引用已提交，用户此前报告模型显示，本次确认VP-1结束；本轮未重新导出最终轴向／半径／缩放或读取图 |
| 双端／完整验收 | 原数据Host／Client各1/1仍为无模型探针历史证据；用户本次未提供新的逐项双端、网络扰动、性能或四机记录，不追加这些通过结论 |
| 自动化旧欠项已修复 | 10月20日另一设备49/50中，OptionalRigPreservesGameplay依赖保存玩家“未选Rig”的旧假设。10月21日反弹诊断小步已改为运行实例Definition副本，保留原玩法／物理回退断言，本机5.8.3最终普通50/50通过；见[本轮证据](../Movement/MVPB_Vehicle_Stability_Diagnostic_Verification_20261021.json)。不改写旧验证JSON |

下一步为VP-2具体方案／施工准备：加减速俯仰、转弯侧倾、限幅／阻尼、空中衰减与异常重置，计算仍在C++、AnimBP只输出Chassis姿态。VP-3视觉悬挂、VP-4离散回弹和四点物理悬挂／落地辅助继续分开；本次未启动这些实现。10-F-4仍统一放在MVP-B收尾，其他成员的弹簧拳与碰撞工具记录见项目状态。

同日后续调整：用户提出侧翻仍能驾驶与持续弹跳问题，并选择低速、持续侧翻后长按自救。现先按[独立物理计划](../Movement/MVPB_Vehicle_Stability_And_SelfRighting_Plan_CN_v0.1.md)完成反弹诊断／修正、姿态门控及自救，再返回VP-2；VP-1结束结论保持，不把本次新增物理需求记成VP-1未完成。

该插入小步的反弹诊断现已完成，见[诊断结果](../Movement/MVPB_Vehicle_Stability_Diagnostic_CN_v0.1.md)：16组真实Solver对照、当前Editor／Game成功及普通50/50，361资产未改；物理修正和自救仍未实现。新基线发现保存半径10cm及模型／物理支撑参考高度需复核，保留配置推导与未做本轮画面验证的边界。

本次仅更新文档，未修改源码／Unreal资产、运行Editor、构建或测试，未提交／Push。
