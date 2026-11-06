# 相机输入、运行恢复与生命周期边界 v0.1

**2026-11-06接续：** 本实现已随`328b78e`提交。当前源码与下方299份成功构建输入无实质差异，155份字节一致、144份仅换行符不同。下方“本轮未提交／Push”为原施工时点；原构建及9份报告为UE5.8.3历史证据，本机实际UE5.8.2，本次未重跑。保留当前NetTest地图未提交修改，原场景／实际保存Camera DA真人复测仍待完成，详见[接续核对](MVPB_Camera_ProgressSync_20261106.json)。

日期：2026-11-05。基点`main/e4f0b09`，本机UE5.8.3-58210709；保留开工前进度同步的文档修改。用户同意独立判断接口并授权施工。代码、Editor／Game及本机程序验证完成，原事故场景真人验收仍待团队；本轮未提交／Push。

## 1. 修正范围

原问题为Client经Entry进入练习／比赛，反复撞电墙后镜头短暂缩近再恢复，Host无明显同类现象。前轮已确认`Movement Ready → 房间输入模式 → NeutralizeInputForMenu → ResetCamera`存在耦合；事故当帧尚无关联日志，诊断不能冒充动态根因确认。见[原诊断第10节](MVPB_Dynamic_Camera_Implementation_CN_v0.1.md#10-client反复撞电墙后短暂缩近2026-11-05仅诊断)。

本次按职责拆分输入中和、首次Avatar就绪、运行中的物理恢复及相机重新初始化。保留车辆／电墙的实际物理、眩晕／回弹、GAS与网络物理协议。全部UI仍为原生回退＋Widget Blueprint可替换，C++父类处理同一输入策略。

## 2. 独立接口与边界

| 接口／条件 | 实际职责 |
|---|---|
| `ATestVehiclePawn::IsLocalInputAvatarLinked()` | 检查本地控制权、当前PS／Pawn／ASC、房间ReadyAvatar、配置与技能等稳定身份关系；不把瞬时Physics Ready当成生命周期 |
| `IsLocalInputInitialized()` | 每组Controller／PS／Movement Epoch必须至少观察到一次匹配Epoch的Ready样本；初始化后，同一绑定的临时Recovery不撤销此状态。失去绑定、换Epoch、重新接管会重新验证 |
| `UHALPracticeRoomWidget::ShouldUseMenuInput()` | 由真实菜单／房间阶段与首次初始化决定UI输入归属，临时物理恢复不再自行打开菜单或拆装IMC；Loading及Client Loaded屏障保持原逻辑 |
| `NeutralizeInputForMenu()` | 中和驾驶命令，并调用观察输入清理，不清相机自动取景历史 |
| `UHALDynamicCameraComponent::ClearObservationInput()` | 清鼠标增量、摇杆输入、累计手动偏移及回中计时，保留自动聚焦、距离、权重与初始化状态 |
| `ResetCamera(Reason)` | 真正换配置／控制者、退场／跨图、无效配置等明确原因执行全量重置；带原因与序号便于定位 |
| `HALCamera::GetReseedReason()` | 首次样本或锚点跳变超过DA阈值时重种；长帧独立处理 |

相机失焦／菜单期间仍可按场上目标自然更新自动取景，并非暂停相机；清除累计手动偏移可能使观察位置回到自动聚焦，这是输入清理的既定含义。自动距离不会因此跳回基础值。

长帧超过`Maximum Sample Delta`时清残留观察输入，积分步长上限取该参数，保留自动距离和焦点历史。真实位置跳变仍按`Teleport Reset Distance`触发重种；大于阈值的网络位置修正也可能进入此边界，日志可辨识，并不保证任意大跳变均平滑。已有有效视图但视口暂时为0时保留上一视图，恢复后继续；首次无有效视口或失去本地控制权仍安全回退。

## 3. 配置与团队复测

不要求重建BP、DA、IA／IMC或Widget。保存相机数值保持；完整编译后重开Editor，新开Host／Client会话即可。两端须使用同一套新代码／包。

Client控制台输入`hal.CameraLog 1`，可配合`hal.CameraDebug 1`；复现后以0关闭。日志按状态转换或明确事件输出，重点字段：

- `CameraAvatarReady`：稳定绑定／首次初始化变化、Physics Ready／Recovery与Epoch。
- `CameraUIInput`：菜单输入归属和Ready／Recovery／Replay变化，应能看到临时恢复而Menu保持0。
- `CameraInputClear`：清理观察输入时原有Distance／GoalDistance。
- `CameraReset`：全量重置的Reason、Frame、重置前距离／目标与Serial。
- `CameraSample`：首次／瞬移Seed原因，或Long Sample与实际步长输入。

验收顺序：

1. 经Entry正常Host／Join、选车确认。让球／其他玩家使取景距离明显大于1600cm，再在练习房间反复撞电墙；检查Client距离不突然回基础值、无自行弹出的菜单／光标，Host同步观察。
2. 进入正式Playing重复。低延迟和受控延迟分别测，不用旧NetTest直入代替房间UI组合路径。
3. 拉远后开／关菜单，Alt-Tab失焦／回焦，确认油门／转向与手动观察无残留，自动缩放保持连续；鼠标与手柄都测。
4. 实际取消换车、重新接管、开赛／返房及新局，验证首次就绪和加载屏障仍生效，旧观察输入不带入新车／新图。
5. 实际瞬移超过当前DA阈值，确认明确重种、自车仍在允许画面范围。不要用调大阈值或改基础距离遮蔽问题。
6. 对自定义Room／Pause WBP复测同样流程，Designer布局不叠加原生界面，原UI语义动作与Receive Room View继续有效。

## 4. 验证状态

完整记录见[验证保护清单](MVPB_Camera_Input_Boundaries_Verification_20261105.json)，原始证据位于`Saved/Camera/InputBoundaries_20261105/`。

| 检查 | 实际结果 |
|---|---|
| 完整构建 | Editor_R5／Game_R3成功；已有MSVC非首选工具链、引擎C4996及旧相机测试C4305警告保留，不把0测试警告写成编译零警告 |
| 原生Regression_R2 | **144/144**，0测试警告／失败／未运行／进行中；含新加的输入清理保留取景、首次Avatar就绪／临时恢复、长帧／瞬移三项 |
| Room_NoLag_R2 | 实际从Entry Host／Join，双端各1/1、退出0；练习及Playing各三轮接触，两车每阶段电墙计数6，真实开关菜单后取景连续 |
| Room_Lag80_Custom_R1 | Host出站注入80ms；原生Host＋内存UMG替换Client各1/1、退出0；同样两阶段重复接触与菜单回归通过，Designer接口保持 |
| LegacyTwoRounds_R1 | 原GF4.TwoRounds保存BP／地图双端各1/1、退出0；身份／新局清理／手动返房兼容 |
| GF7ThreeRounds_R1 | 原GF7三局双端各1/1、退出0；最后存活／超时并列／同批全灭、观战与自动返房通过 |

合计9份最终自动化报告均0测试警告／失败／未运行／进行中。两组相机双端均D3D12离屏1280×720、60FPS；实际读取Pawn正常CalcCamera的有效输出，跟踪期间要求距离大于1700cm、全量重置序号不变，实际菜单操作独立验证。Client日志实测出现`Menu=0 PhysicsReady=0 Recovery=1`，同时距离保持连续。Probe的`RecoveryObserved`为会话累计标记，不单独证明每个阶段均发生Recovery；详细状态日志另行留档。

联机夹具只调整运行时生成的Actor：在高处隔离电墙、每阶段一次性关闭测试车辆重力、驱动物理速度／位置，用瞬态相机Profile（FOV60、基础1600cm）和正式保存的BP_BasicBall建立远处主目标。初始大跳变在跟踪前完成并允许重种，重复接触期间不屏蔽重置或放宽断言。替换Client为内存UMG夹具，不冒充已制作／保存的生产WBP；这些检查不代表已验收真实手柄、鼠标、最终UI美术或团队保存DA画面。80ms为同机Host出站注入，不是实测RTT或校园四机。

首轮Editor缺HALCameraMath显式include已修正；首轮原生143/144因旧相机夹具未执行BeginPlay、Health未初始化，补完整生命周期后144/144。首轮房间双端虽通过全部相机断言，但默认原生球未按TieredV1迁移，产生错误，故两端均按失败保留；改用现有正式球类后完整重验通过，未屏蔽错误或降低生产检查。原生最终报告之后仅改联机夹具球类，生产与原生测试源码逐份未变；最终两Target及全部联机包含此修正。

**1190保存资产前后逐份SHA-256全同，299最终源码／配置／uproject冻结，7份Config／Build.cs／uproject保持。** 无BP／DA／地图保存、无依赖或网络物理格式修改，专用进程已退出。本轮代码／文档未提交／Push。团队按第3节用实际保存的Camera DA复测原事故画面；独立电墙高速轨迹问题、鼠标／手柄、发布包／校园四机及F4／VP-4C继续单独验收。
