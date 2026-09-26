# MVP-B Listen Server 阶段 6：球状态与控球表现修正（v0.2）

**后续客户端持球滚动表现修正见同目录 `MVPB_Phase6_ControlledBall_Rolling_Fix_CN_v0.1.md`；本版记录约束卡车回归的原因与修复。**

日期：2026-09-26。状态：**团队实测发现 v0.1 持球车辆被卡在原地；C++ 原因已定位并修正，Editor/Game Target 与自动化测试通过。修正版的双进程可见行为尚待团队复测，阶段 6 未验收。** 依据为 `Docs/MVP_B_Listen_Server_Plan_CN_v0.2.md` 第 10、11、19 节。沿用到正式版的边界是：服务器决定球权、物理、命中；客户端按一份离散状态快照呈现球，球的运动状态另由物理复制处理。

## 实现与协议

- `ABasicBallActor` 用单个 `ReplicatedUsing` 的 `FBallRepState` 替代原来分开的 State、Holder、Launcher。快照包含状态、Holder、发射者 Pawn、局内稳定 `PlayerId`、`uint32 StateSequence`、可取得时的服务器车辆物理帧、定向重捕获锁定及全局拾取保护的服务器结束时间。发射者 Pawn 在退出后可以失效，稳定 ID 在该次 `Launched` 期间保留。未由持球车辆触发的转换，其 `ServerPhysicsFrame` 为 `INDEX_NONE`，不伪称为准确帧。
- 仅服务器的 `BeginControl`、`LaunchFromControl`、`ReleaseFromControl`、低速回到 `Free` 会提交新快照。每次成功转换只递增一次序号并 `ForceNetUpdate`；拒绝争球、重复发射不改变序号。客户端只读 RepNotify，按序号忽略旧离散表现；晚到 Holder 指针由后续显示 Tick 自然重试，不靠位置猜球权。
- `Free`／`Launched` 保持服务器 Chaos 和物理 Transform 复制；球的网络模拟代理以 Predictive Interpolation 显示。C++ 给球设置 PI 基线，联机代理预初始化再次确认，以免旧蓝图序列化了原 Default；球上的 `BallNetworkPhysicsSettings` 组件可挂独立的 Network Physics DA 来覆盖和调节，不复用车辆 DA 的调参。
- `Controlled` 仍由服务器车辆 `BallConstraint` 约束真实球。UE 5.8 的约束 **Frame1 是 Child、Frame2 是 Parent**；修正版把球放在 Frame1、车辆放在 Frame2，再设置 `Parent Dominates`，使球的不可预测约束反力不反馈到要重模拟的车体。初始化后运行时检查两端及约束有效性，接线错误则立即解除并拒绝控球。客户端收到 `Controlled` 后，关闭本地球刚体模拟和碰撞，在 PostPhysics Tick 将无碰撞的球根组件随 Holder 的 `BallControlPoint` 移动；附着其上的视觉 Mesh 和调试球形位置也随之更新。不创建本地物理约束、不向车辆施力。离开 `Controlled` 后在最后显示位置恢复模拟，交回 PI。Tick 只在客户端持球表现期间启用。
- `hal.BallNetLog 1` 在非 Shipping 版本记录服务器提交与客户端收到的 State、Sequence、Holder、Launcher、PlayerId、ServerFrame；采样后设回 `0`。这是诊断开关，不改变规则。
- 阶段 7 才会把发射请求的预期球序号与物理帧对齐并处理预测反作用力；当前客户端发射请求仍沿用阶段 4 的 `ExpectedBallStateSequence=0` 占位。不要把本阶段称作已完成发射预测。

## 事故、原因与修正边界

- 团队按 v0.1 接好 `/Game/Data/Network/DA_NetworkPhysics_Ball_MVPB` 和 `BP_BasicBall.BallNetworkPhysicsSettings.Settings Data Asset` 后实测：主机、客户端的车辆一旦持球就被限制在原地；球碰撞调试球形停在原地；发射后车辆恢复移动。
- v0.1 代码将车辆放在约束 Frame1、球放在 Frame2，却设置了 `Parent Dominates`。UE 5.8 的 Chaos 将 Frame2 作为 Parent；因此**球**成为不受约束反力影响的一端，车辆被拉向停在原地的球。问题由约束端点顺序造成，不能靠增加马力、调整 DA 或更改 PI 参数修复。
- 客户端红色调试球形停在原地是第二个独立的显示问题：v0.1 只移动视觉 Mesh，禁用碰撞的本地根组件仍留在原处。v0.2 让禁用模拟和碰撞的根组件跟随目标，因此调试形状也移动；客户端根组件仍不具有权威碰撞。
- 修正只在 C++ 约束构造、客户端持球显示和必要回归断言中，未改车辆/球的数值 Data Asset，也未扩大到阶段 7 的发射预测。

## 团队在 Unreal Editor 中的接线核对

1. 完全退出之前的 PIE／Standalone 窗口，关闭 Editor 后以新编译的 C++ 重新打开项目。无需重建或重填已保存的球 DA、车辆 DA 或 `BP_BasicBall`。
2. 在 `/Game/Blueprints/Balls/BP_BasicBall` 选中 `BallNetworkPhysicsSettings`，确认 `Settings Data Asset` 仍指向团队已创建的 `/Game/Data/Network/DA_NetworkPhysics_Ball_MVPB`。打开该 DA，确认 `Settings → General Settings → Sim Proxy Rep Mode` 的左侧 Override 勾选、右侧是 **Predictive Interpolation**。若已正确，直接关闭资产，不必再次保存。
3. 在 `/Game/Maps/MVPB_NetTest` 确认测试球仍是 `BP_BasicBall`，地图 GameMode Override 和车辆／球 Gameplay DA 引用不变。不要为修复本次问题改动球质量、阻尼、约束力度、车辆动力或网络 PI 参数。

## 双进程验收步骤

保持 **Use Less CPU when in Background** 关闭，用已沿用的双进程 Listen Server 配置进入 `MVPB_NetTest`。主机执行 `HALMatchStatus`，确认两名玩家后执行 `HALStartMatch`。两端分别输入 `hal.BallNetLog 1`，按以下顺序观察并保存日志/视频，结束后都执行 `hal.BallNetLog 0`。

1. `Free`：两端看到同一自由球。客户端用 `p.Chaos.DebugDraw.Enabled 1`、`p.Net.DebugDraw.ShowRepMode 1` 临时确认球为 PI；录下远端滚动、弹墙及静止收敛，再把命令设回 `0`。
2. **首先只做本次回归复测**：主机持球后连续直行、转向，再让客户端持球连续直行、转向。两辆车持球时都应能正常移动；球的可见模型和临时打开的红色调试球形均应随车前进，不能停留在拾取点。发射后也应继续正常移动。两端日志最终应为同一 `Controlled`、同一 Holder、同一 Sequence；如任一端仍卡住，暂停后续网络压力测试，保存两端日志和短视频。
3. 两车同时接近同一 `Free` 球，重复至少 10 次。每轮服务器只能产生一次成功获取；两端最终 Holder 相同。不同网络窗口可短暂异步显示，但不能永久分叉或两个 Holder 同时成立。
4. 持球车辆被撞到脱球阈值：两端最终回到 `Free`、Holder 清空；原车在锁定期内不能立即吸回，其他车可以按常规条件争取。测试车主离开球附近后再回来，以及脱球后立刻发射请求；不得重复发射或造成额外权威命中。
5. 持球发射：两端最终为相同 `Launched`、Holder 空、Launcher 和 `PlayerId` 相同；球在客户端用 PI 显示。球低速结束后两端最终回到 `Free`。重复点击只能由服务器状态决定一次发射，现阶段发射响应手感与拒绝回滚仍留给阶段 7。
6. 用低延迟先跑完整流程，再在约 **100 ms 实测 RTT、1% 丢包、20 ms 抖动** 下复测持球驾驶、发射和碰撞。记录客户端球可见顿挫、持球时车辆回拉、StateSequence 是否最终一致。若球变 `Free` 的瞬间发生明显传送，优先对照服务器与客户端位置/速度日志及视频，不先调整基础球/车调参。

## 验证和未关闭事项

- 修正版 `HAL_Future_CreationEditor Win64 Development`、`HAL_Future_Creation Win64 Development` 均编译通过；`HAL.FutureCreation` 自动化 **12/12** 通过，报告在 `Saved/Automation/MVPB_Phase6_ConstraintFix`。测试新增对真实 `BallConstraint` 的端点顺序及 `Parent Dominates` 断言；这直接覆盖本次接反原因，但不能代替动态驾驶测试。
- 修正版无界面双进程加载 `/Game/Maps/MVPB_NetTest`：服务器记录 Join succeeded，客户端 Welcome 并加载同一地图；两端日志未出现 `ReadContentBlockPayload`、组件创建失败、Fatal、Assertion、Handled ensure 或约束端点检查失败。日志在 `Saved/Logs/MVPB_Phase6_ConstraintFixServer.log` 与 `MVPB_Phase6_ConstraintFixClient.log`。v0.1 也曾通过同类无输入烟测，却未发现持球卡住；因此本项仅证明联网加载正常，不代表动态持球行为通过。
- 修正版双进程真实操控、球 PI 可见效果、服务器单向约束的实际手感、可见脱球瞬间，以及网络故障场景仍待团队验证。阶段 6 暂不标记验收完成。
- 本次 C++ 修复没有修改 `.uasset`、`.umap`。团队已在 Editor 中保存的当前只读 SHA-256：`BP_BasicBall` `B47D684494BC0F2326C36D4D2CFA585F84AA0A1FAA704200B70AF6B6891ECD72`；新球网络 DA `DA_NetworkPhysics_Ball_MVPB` `FBB7124256E5F5D797E086A54E55335CC867A010FA8479500743032F815D7A16`。基础球 DA 仍为 `07FFEA5C14E069EB300BFFD2A15405E4C6EF5EFDFD9EB3A861CAACC244142F32`，车辆网络 DA 仍为 `48E78ECA68F7249D20153D67821D912FB077E0509D1400719F1C86F0866F4F17`，`MVPB_NetTest` 仍为 `E4D5A1A3F947F4D98AEC9100C22A78E05EE0905DBA3E44E209228022AAEA567D`。这些哈希只证明资产文件版本；PI 字段与组件引用依据团队 Editor 操作记录核对。
