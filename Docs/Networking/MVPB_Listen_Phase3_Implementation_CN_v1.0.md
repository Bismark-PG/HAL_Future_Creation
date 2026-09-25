# MVP-B Listen Server 阶段 3：服务器权威输入基线实施记录

日期：2026-09-25  
依据：`Docs/MVP_B_Listen_Server_Plan_CN_v0.2.md` 第 7、12、19 节  
状态：C++ 路径已实现并编译；客户端持续驾驶初测符合阶段 3 预期，发射及重复事件仍待验证后验收

## 基线与范围

继续使用 `/Game/Maps/Test` 做单机回归，`/Game/Maps/MVPB_NetTest` 做联机验证。阶段 2 的 `Docs/Networking/MVPB_Phase2_PreCode_AssetSnapshot_20260925.json` 记录了 9 个地图、蓝图和 Data Asset 的 SHA-256；本次施工前复核一致。没有编辑 `.uasset`、`.umap`，也没有重填已调车辆参数。

本阶段接通客户端持续驾驶输入、服务器物理执行、客户端发射请求及发射序号幂等。自动控球、脱球、球命中与 HP 的现有代码已经检查服务器权限；阶段 3 沿用该权威入口。本阶段不实现客户端物理预测、Network Physics 回滚、远端车辆插值或球的原子复制状态。

## 实现与协议

- 本地 Enhanced Input 仍写入 `FVehicleInputCmd`。主机玩家直接向同一移动组件提供输入；客户端不再对自己的车辆施加本地驾驶力，每秒发送 30 次 `FVehicleNetInputData` 快照，使用 Pawn 所有权保护的 **Unreliable Server RPC**。该频率只影响传输，不改变 60 Hz 物理步或 Data Asset 中的驾驶参数。
- 服务器拒绝非有限值、越界值、零序号、重复或乱序快照，也拒绝把发射标志塞进持续输入。接收合法输入后，物理步读取该快照并由服务器施力；连续 0.25 秒没有新快照时清零输入，防止断线或失焦后一直加速。输入快照与待执行反作用力的跨线程读写受到锁保护；跨线程物理帧号使用原子读取。
- 发射键上升沿生成 `FVehicleLaunchRequest`，包含单调 `LaunchSequence`、客户端物理帧占位和预期球状态序号占位，经 **Reliable Server RPC** 到服务器。服务器先检查 Pawn 当前归属与序号，再通过原有 `BallControl->LaunchHeldBall()` 验证真实球权；同一或更旧序号不会再次发射或施加反作用力。主机也使用同一处理函数。
- `ExpectedBallStateSequence=0` 在此阶段表示尚不可用。阶段 6 建立原子 `FBallRepState` 后才可用真实球状态序号拒绝过期请求；客户端物理帧与服务器历史的对应也要在阶段 4 完成。因此当前已经具备基本发射幂等，但不宣称完成晚到球权请求或预测回退的全部协议。
- `hal.VehicleNetLog 0/1/2` 控制非 Shipping 诊断：`0` 关闭，`1` 记录发射接受/拒绝、输入拒绝/超时，`2` 另记录每个接受的连续输入快照及油门、刹车、转向、手刹和服务器物理帧。默认关闭。

v0.2 第 7.1 节的正式目标是 Network Physics 输入历史，阶段 4 才接入历史和 Resimulation。本阶段的 30 Hz Unreliable RPC 是服务器权威基线的**阶段性传输适配层**，没有发送逐帧 Reliable RPC；后续须替换传输而保留 `FVehicleNetInputData` 的语义、序号和统一物理执行函数。当前客户端驾驶会等待服务器位置复制，不能以即时响应或平滑度作为阶段 3 验收标准。

## 已执行验证

- `HAL_Future_CreationEditor Win64 Development` 与 `HAL_Future_Creation Win64 Development` 最终代码均编译通过。
- `HAL.FutureCreation` 最终代码自动化测试 11/11 通过，报告位于本机 `Saved/Automation/MVPB_Phase3_Final/index.json`；输入校验测试覆盖非法数值、范围、发射位隔离、重复/乱序和序号回绕。
- 命令行无输入烟测：`Test` 与 `MVPB_NetTest?listen` 均加载正确 GameMode，玩家车辆首个物理步为 `0.016667 s`；检索不到 `Handled ensure`、`IsInGameThreadContext`、Fatal 或 Assertion。日志位于 `Saved/Logs/MVPB_Phase3_Test_Smoke.log`、`MVPB_Phase3_NetTest_Smoke.log`。
- 同机两个独立无界面进程：主机在 `MVPB_NetTest?listen` 启动，客户端通过 `127.0.0.1:7794` 加入；主机生成第二辆车并记录 `Join succeeded`。再次运行并开启 `hal.VehicleNetLog 2` 后，主机连续记录客户端 `Vehicle input accepted ... Seq=1,2,...`，证明定时快照经 Pawn RPC 到达服务器。此测试未注入驾驶按键或发射请求，不能证明车辆实际位移、控球和发射的双端表现。
- 阶段 2 的 9 个资产 SHA-256 在施工后复核，差异为 0；`git diff --check` 通过。

## 2026-09-25 双窗口实测与复测结论

团队确认主机日志显示客户端输入值按预期变化，但客户端车辆呈“进一步退半步”，主机看到该车与球一卡一卡地缓慢运动。此次主机位于 Editor 进程、客户端位于独立游戏进程。主机日志 `Saved/Logs/HAL_Future_Creation.log` 中，客户端油门持续为 `1.00`，输入序号连续到达；然而服务器物理帧从 `03:32:49.845` 的 `2400` 到 `03:33:11.512` 的 `2530`，约 21.7 秒仅前进 130 帧，约 6 帧/秒。多条输入也在同一时间戳和同一服务器物理帧成批处理。没有发现输入拒绝或超时。这证明当前的“龟速”不能只归因于阶段 4 尚未实现预测，服务器测试进程本身明显没有按实时 60 Hz 推进。

UE 5.8 的 `UEditorPerformanceSettings` 默认启用 **Use Less CPU when in Background**，并在 Editor 非前台时节流。团队关闭该设置后反馈：主机不再出现龟速，客户端能够持续驾驶，仅剩较轻微的卡顿，手感不及主机顺滑。这与 Editor 后台节流的诊断一致；目前是团队现象反馈，尚无关闭设置后的量化帧率日志。客户端尚无本地预测和重模拟，仍需等待服务器位置复制；轻微延迟与校正可以作为阶段 3 基线现象记录，顺滑度留给阶段 4 验证。`hal.VehicleNetLog 2` 应只短时开启，然后改回 `0`，避免逐包日志干扰性能。发射、重复请求幂等、断线后的输入清零仍需单独回归，不因驾驶改善而将整个阶段标记为验收完成。

## 团队在 Unreal Editor 的回归步骤

1. 关闭 Live Coding，使用新 C++ 构建打开项目。先在 `/Game/Maps/Test` 以 `Number of Players=1`、`Net Mode=Play Standalone` 的 Standalone Game 做油门、刹车、转向、漂移、控球和发射回归；这里不输入 `HALStartMatch`。
2. 在 Editor Preferences 的 Performance 中关闭 **Use Less CPU when in Background**。打开 `/Game/Maps/MVPB_NetTest`，设置 `Number of Players=2`、`Net Mode=Play As Listen Server`、`Run Under One Process=Off`，启动两个窗口。两端输入 `HALMatchStatus`，核对主机 `Authority=1`、客户端 `Authority=0`、各自 Pawn 与镜头。主机输入 `HALStartMatch`，两端阶段为 `Playing`。
3. 主机控制台输入 `hal.VehicleNetLog 2`，客户端持续按油门 3 秒、松开，再依次测转向、刹车和手刹。查看**主机进程日志**中该客户端车辆的 `Vehicle input accepted`：按住时相应值改变，松开后归零；确认主机和客户端最终都看见客户端车辆持续位移，而不是一直回到出生点。当前允许输入延迟和可见校正。测试后在主机输入 `hal.VehicleNetLog 0`。
4. 由客户端接近并获取球，确认主机看到球权；客户端按一次发射键，确认球进入发射状态且车辆只受一次反作用力。主机可暂时输入 `hal.VehicleNetLog 1` 查 `Vehicle launch accepted/rejected`，观察球命中和 HP 在两端最终一致。主机玩家也执行同一流程。不要用连续输入快照中的 `bLaunch` 判断发射次数。
5. 断开客户端或关闭其窗口，确认服务器在 0.25 秒后不会持续应用旧油门。重连、重复按发射及高延迟/丢包条件下的事件幂等仍需专项测试；仅靠一次普通按键不能证明重复包已处理。
6. 留档测试日期、引擎补丁版本、地图、两端日志、输入/发射结果和异常复现步骤。阶段 3 的实际行为与事件幂等通过后才标记验收完成；当前不能进入阶段 4 的手感结论。
