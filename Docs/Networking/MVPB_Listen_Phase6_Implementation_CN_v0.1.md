# MVP-B Listen Server 阶段 6：球状态与控球表现（v0.1）

**历史版本：持球约束 Frame1／Frame2 接反，导致实测持球车辆被静止球限制在原地。请使用同目录 v0.2 的修正记录和复测步骤；本版仅用于追溯首次施工。**

日期：2026-09-26。状态：**C++ 施工完成，Editor/Game Target 与现有自动化测试通过；双进程可见行为和网络条件尚待团队实测，因此阶段 6 未验收。** 依据为 `Docs/MVP_B_Listen_Server_Plan_CN_v0.2.md` 第 10、11、19 节。沿用到正式版的边界是：服务器决定球权、物理、命中；客户端按一份离散状态快照呈现球，球的运动状态另由物理复制处理。

## 实现与协议

- `ABasicBallActor` 用单个 `ReplicatedUsing` 的 `FBallRepState` 替代原来分开的 State、Holder、Launcher。快照包含状态、Holder、发射者 Pawn、局内稳定 `PlayerId`、`uint32 StateSequence`、可取得时的服务器车辆物理帧、定向重捕获锁定及全局拾取保护的服务器结束时间。发射者 Pawn 在退出后可以失效，稳定 ID 在该次 `Launched` 期间保留。未由持球车辆触发的转换，其 `ServerPhysicsFrame` 为 `INDEX_NONE`，不伪称为准确帧。
- 仅服务器的 `BeginControl`、`LaunchFromControl`、`ReleaseFromControl`、低速回到 `Free` 会提交新快照。每次成功转换只递增一次序号并 `ForceNetUpdate`；拒绝争球、重复发射不改变序号。客户端只读 RepNotify，按序号忽略旧离散表现；晚到 Holder 指针由后续显示 Tick 自然重试，不靠位置猜球权。
- `Free`／`Launched` 保持服务器 Chaos 和物理 Transform 复制；球的网络模拟代理以 Predictive Interpolation 显示。C++ 给球设置 PI 基线，联机代理预初始化再次确认，以免旧蓝图序列化了原 Default；球上的 `BallNetworkPhysicsSettings` 组件可挂独立的 Network Physics DA 来覆盖和调节，不复用车辆 DA 的调参。
- `Controlled` 仍由服务器车辆 `BallConstraint` 约束真实球。约束设置 `Parent Dominates`，球的不可预测约束反力不再反馈到要重模拟的车体。客户端收到 `Controlled` 后，关闭本地球刚体模拟和碰撞，只在 PostPhysics Tick 将**视觉 Mesh** 跟随复制 Holder 的 `BallControlPoint`；不创建本地物理约束、不向车辆施力。离开 `Controlled` 时先从最后显示位置恢复本地球体，再交回 PI。Tick 只在客户端持球表现期间启用。
- `hal.BallNetLog 1` 在非 Shipping 版本记录服务器提交与客户端收到的 State、Sequence、Holder、Launcher、PlayerId、ServerFrame；采样后设回 `0`。这是诊断开关，不改变规则。
- 阶段 7 才会把发射请求的预期球序号与物理帧对齐并处理预测反作用力；当前客户端发射请求仍沿用阶段 4 的 `ExpectedBallStateSequence=0` 占位。不要把本阶段称作已完成发射预测。

## 团队在 Unreal Editor 中的资产接线

1. 关闭所有旧 PIE／Standalone 窗口并重开已编译的项目。Content Browser 打开 `/Game/Blueprints/Balls/BP_BasicBall`，在 **Components** 查找继承组件 `BallNetworkPhysicsSettings`。**不要**改动现有 `DA_Ball_Normal_TestBaseline_v1`、球半径、质量、阻尼、视觉 Mesh 或车辆 Data Asset。若旧蓝图打不开或提示组件冲突，先记录完整编译/加载日志，不要删除并重建球蓝图。
2. 在 `/Game/Data/Network` 右键 **Miscellaneous → Data Asset**，选择 **Network Physics Settings Data Asset**（搜索 `NetworkPhysicsSettingsDataAsset`），命名 `DA_NetworkPhysics_Ball_MVPB`。打开它，在 `Settings → General Settings → Sim Proxy Rep Mode` 勾选字段左侧 **Override**，右侧选择 **Predictive Interpolation**。其余 Override 保持未勾选的默认值；先不要改车辆网络资产。保存。
3. 回到 `BP_BasicBall`，选中 `BallNetworkPhysicsSettings` 组件，Details 中搜索 `Settings Data Asset`，指定刚创建的 `/Game/Data/Network/DA_NetworkPhysics_Ball_MVPB`。点击 **Compile**、**Save**。如蓝图因父类新增组件显示 Dirty，可先确认引用正确再保存。无需 Blueprint Event Graph 接线，也不要给球加 Network Physics History：它没有本地预测输入。
4. 在 `/Game/Maps/MVPB_NetTest` 确认测试球仍是 `BP_BasicBall`，地图的 GameMode Override、车辆和球 DA 引用仍为之前版本；不要把地图测试改动保存为正式配置。阶段 6 的 PI C++ 基线在不接资产时也能运行，但团队接好独立资产后才能按同一配置复测并留档。

## 双进程验收步骤

保持 **Use Less CPU when in Background** 关闭，用已沿用的双进程 Listen Server 配置进入 `MVPB_NetTest`。主机执行 `HALMatchStatus`，确认两名玩家后执行 `HALStartMatch`。两端分别输入 `hal.BallNetLog 1`，按以下顺序观察并保存日志/视频，结束后都执行 `hal.BallNetLog 0`。

1. `Free`：两端看到同一自由球。客户端用 `p.Chaos.DebugDraw.Enabled 1`、`p.Net.DebugDraw.ShowRepMode 1` 临时确认球为 PI；录下远端滚动、弹墙及静止收敛，再把命令设回 `0`。
2. 主机先持球、客户端观察；随后交换。两端日志最终应为同一 `Controlled`、同一 Holder、同一 Sequence。客户端球跟随车头，没有球体把本地车拖回原地，也没有由显示球产生的伤害或碰撞冲量。服务器球仍可与另一辆车产生真实物理交互。
3. 两车同时接近同一 `Free` 球，重复至少 10 次。每轮服务器只能产生一次成功获取；两端最终 Holder 相同。不同网络窗口可短暂异步显示，但不能永久分叉或两个 Holder 同时成立。
4. 持球车辆被撞到脱球阈值：两端最终回到 `Free`、Holder 清空；原车在锁定期内不能立即吸回，其他车可以按常规条件争取。测试车主离开球附近后再回来，以及脱球后立刻发射请求；不得重复发射或造成额外权威命中。
5. 持球发射：两端最终为相同 `Launched`、Holder 空、Launcher 和 `PlayerId` 相同；球在客户端用 PI 显示。球低速结束后两端最终回到 `Free`。重复点击只能由服务器状态决定一次发射，现阶段发射响应手感与拒绝回滚仍留给阶段 7。
6. 用低延迟先跑完整流程，再在约 **100 ms 实测 RTT、1% 丢包、20 ms 抖动** 下复测持球驾驶、发射和碰撞。记录客户端球可见顿挫、持球时车辆回拉、StateSequence 是否最终一致。若球变 `Free` 的瞬间发生明显传送，优先对照服务器与客户端位置/速度日志及视频，不先调整基础球/车调参。

## 验证和未关闭事项

- 最终代码的 `HAL_Future_CreationEditor Win64 Development`、`HAL_Future_Creation Win64 Development` 均编译通过；`HAL.FutureCreation` 自动化 **12/12** 通过，最终报告在 `Saved/Automation/MVPB_Phase6_Final`。现有配置测试增加服务器争球、重复发射、原子序号及定向锁定断言。
- 最终构建无界面双进程加载 `/Game/Maps/MVPB_NetTest`：服务器记录客户端 Join succeeded，客户端被引导到同一地图与 `BP_MVPB_ListenGameMode`；两端日志未出现 `ReadContentBlockPayload`、组件创建失败、Fatal、Assertion 或 Handled ensure。原始日志在 `Saved/Logs/MVPB_Phase6_FinalSmokeServer.log` 与 `MVPB_Phase6_FinalSmokeClient.log`。该烟测没有执行驾驶、控球或发射输入。
- 双进程真实操控、球 PI 可见效果、服务器单向约束的实际手感、可见脱球瞬间，以及网络故障场景仍待团队验证。无界面加载或自动化测试不能替代这些实测。阶段 6 暂不标记验收完成。
- 本轮没有修改 `.uasset`、`.umap`。施工前只读 SHA-256：`BP_BasicBall` `1DAD678583FF7C61978EBD490B7D43FC9445C73FDC2544BCCB00B6FA1FB3C08E`；`DA_Ball_Normal_TestBaseline_v1` `07FFEA5C14E069EB300BFFD2A15405E4C6EF5EFDFD9EB3A861CAACC244142F32`；车辆 `DA_NetworkPhysics_MVPB` `48E78ECA68F7249D20153D67821D912FB077E0509D1400719F1C86F0866F4F17`；`MVPB_NetTest` `E4D5A1A3F947F4D98AEC9100C22A78E05EE0905DBA3E44E209228022AAEA567D`。团队接线后应另存资产路径、字段截图、日期和新哈希。
