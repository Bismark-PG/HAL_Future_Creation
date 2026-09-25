# MVP-B Listen Server 阶段 2：固定物理执行路径实施记录

日期：2026-09-25  
依据：`Docs/MVP_B_Listen_Server_Plan_CN_v0.2.md` 第 6、7、19 节  
状态：C++ 物理线程访问已修正并复测；需团队完成单机手感回归后验收

## 已保存的测试参数基线

团队确认当前车辆参数可作为已调好的测试版本，后续允许继续调整。实施前对 `Test`、`MVPB_NetTest`、相关车辆蓝图及 MVP-B Data Asset 做了只读 SHA-256 快照：`Docs/Networking/MVPB_Phase2_PreCode_AssetSnapshot_20260925.json`。本阶段没有编辑 `.uasset` 或 `.umap`，没有用 C++ 默认值重填 Data Asset。

## 实施内容与数据边界

- `Config/DefaultEngine.ini` 将 Chaos 异步物理设置为固定 `1/60 s`。这是项目级物理设置，也会作用于球和其他刚体；不改变车辆调参资产。
- `ArcadeVehicleMovementComponent` 停用普通组件 Tick，改由 UE 5.8 的 `AsyncPhysicsTickComponent` 每个 Chaos 物理步调用统一的 `SimulateVehicleStep`。物理步从最新输入快照读取油门、刹车、转向和手刹，执行接地检测、墙角脱困、纵向力、侧向抓地和转向扭矩。此回调处于物理线程上下文；刚体姿态、速度与受力必须通过 PhysicsThread handle／适用的 Chaos API 访问，不能假定普通游戏线程组件 API 可用。场景查询的同步成本与四人压力需在后续测量。
- `FVehicleNetInputData` 保存连续输入、输入序号和本地 Chaos 帧号。进入物理步前将非有限输入归零并限制范围，`bLaunch` 不作为连续输入重放。
- `FVehicleNetStateData` 保存刚体位置、旋转、线速度、角速度，以及墙角脱困的启用状态、混合值、最多两条接触法线和待执行的反作用力。移动组件提供 C++ Capture/Restore 窄接口；当前没有启用 Network Physics 历史或实际回滚。
- 发射仍由现有服务器权威控球逻辑判定；成功发射时只把一次反作用速度变化交给移动组件，下一物理步应用，不再由控球回调直接对车辆施加冲量。阶段 3/7 将给此离散动作加入序号、网络确认与重模拟历史。
- 运行烟测暴露了已调车辆蓝图的移动组件可能未自动激活：Chaos 注册异步物理回调后要求组件保持 Active。移动组件现在在 BeginPlay 激活并注册，在 Deactivate/EndPlay 注销，避免首帧断言及退出时残留回调。
- 当前工程的悬挂和落地辅助尚未实现。接地标记和前向速度每步可重算；墙角脱困混合与法线必须保存。以后增加悬挂、落地状态时按相同规则扩充状态结构。

`AsyncPhysicsTickComponent` 是阶段 2 的固定步入口，但 UE 5.8 此入口本身并不提供完整的 Network Physics 重模拟回调。阶段 4 接入 Resimulation 时必须将同一移动规则接到可重放的输入/状态历史，并处理接地与墙面场景查询在回滚帧的可用性；不得把目前的 Capture/Restore 接口误认为预测已经完成。

2026-09-25 团队在 Editor 启动的独立 Standalone Game 日志发现 `IsInGameThreadContext()` handled ensure，位置在移动组件的刚体 Transform 读取和 `AddForce`。之前的无输入烟测只筛查了 Fatal/Assertion，漏掉 handled ensure，不能作为运行无错证明。现已将固定步中的刚体姿态、速度和受力改为 PhysicsThread handle／内部 Chaos API；Editor/Game Target 均重新编译通过，`Test` 独立进程复测没有 `Handled ensure` 或线程上下文错误。墙角脱困接触测试改为直接验证共用的接触查询，避免在不推进 Chaos 的测试 World 中手动调用 physics-thread 回调。

## 已执行的验证

- `HAL_Future_CreationEditor Win64 Development` 与 `HAL_Future_Creation Win64 Development`：物理线程修正后均重新编译通过。
- `HAL.FutureCreation` 自动化测试：修正后 11 项全部通过，包含 60 Hz 配置、输入有限值/范围校验及墙角脱困接触与状态 Capture/Restore 验证。报告保存在本机 `Saved/Automation/MVPB_Phase2_ThreadFix_Final`。
- 编译和自动化测试后重新核对快照内 9 个地图、蓝图和 Data Asset 文件，SHA-256 均未变化；`git diff --check` 通过。
- 修正后，`UnrealEditor-Cmd -game` 加载 `/Game/Maps/Test`，玩家车输出首个 `Vehicle physics step ... DeltaTime=0.016667 Frame=0`；专门检索 `Handled ensure`、`IsInGameThreadContext`、Fatal 和 Assertion 均为 0。日志在本机 `Saved/Logs/MVPB_Phase2_ThreadFix_Test.log`。此无输入烟测仍不能替代实际驾驶、控球和发射手感验收。
- 尚未由本次命令行测试验证 Editor 内车辆手感、球发射反馈、实际双进程行为或性能，不将阶段 2 标为验收通过。
- 2026-09-25 团队双窗口 Listen 测试反馈：主机车辆可正常移动，客户端窗口可看见主机移动；两端相机独立，`GamePhase=Playing`。客户端车辆短距离移动后被拉回原位置。源码中车辆开启位置复制，但本地输入目前只写入本地移动组件，尚无客户端连续输入提交至服务器的 RPC，因此该现象符合阶段 3 尚未施工的边界；`Playing` 阶段本身不会启用输入传输。此处记录为团队观察，尚未收到该次运行日志，也不将其视为客户端驾驶已通过。

## 团队在 Unreal Editor 的回归步骤

1. 关闭 Live Coding，打开项目并编译 C++。先打开 `/Game/Maps/Test`，将 **Number of Players = 1**、**Net Mode = Play Standalone**、Play 模式设为 **Standalone Game**，并关闭额外服务器进程选项（若已开启）。Standalone Game 使用独立游戏进程；查看项目 `Saved/Logs` 中最新的 `HAL_Future_Creation_2.log` 等独立进程日志，而不是只查 Editor 的 Output Log。该车辆每次运行应只出现一次 `Vehicle physics step`，首个 `DeltaTime` 约为 `0.016667`。`Test` 使用 `BP_MVPA_GameMode`，无需输入任何比赛开始命令。不要保存对地图、车辆蓝图或 Data Asset 的意外改动。
2. 在 `Test` 按现有输入完成持续油门、刹车至倒车、普通转向、手刹漂移、低速贴墙转出、墙角、侧撞、腾空及落地。与已保存测试版本比较方向、加速、刹车和漂移手感，记录明显差异及复现步骤。
3. 自动控球后按一次发射，确认球只发射一次、车辆只受到一次反作用力；重复控球/发射，并验证受撞脱球及重新捕获锁定。反作用力现在在下一物理步生效，允许不超过一个物理步的时序变化。
4. 分别用控制台 `t.MaxFPS 30`、`t.MaxFPS 60`、`t.MaxFPS 120` 重做直线加速、急刹和漂移。固定物理步下不应出现因渲染帧率变化而导致的阻断性驾驶差异。测试后用 `t.MaxFPS 0` 恢复限制。
5. 在 `/Game/Maps/MVPB_NetTest` 做同样的单机回归，再运行双进程 Listen Server 烟测，确认各自出生、镜头与阶段 1 加入闸门没有退化。只有该联机地图的主机在两人加入后才执行 `HALStartMatch`；项目没有 `HALStartGame` 命令。阶段 2 不以客户端驾驶预测或平滑程度验收。
6. 将测试日期、引擎补丁版本、地图、渲染帧率、通过/失败项、异常日志和主观手感差异补入本记录。全部无阻断问题后才能标记阶段 2 验收通过，再进入阶段 3。

## 验收与后续

阶段 2 的门槛是 60 Hz 物理路径真实运行、现有单机驾驶和球玩法无阻断性回归、资产哈希不变。阶段 3 才接入持续输入的客户端提交与服务器权威执行；阶段 4 才接入 Network Physics 历史和 Resimulation。GAS 不在本阶段。
