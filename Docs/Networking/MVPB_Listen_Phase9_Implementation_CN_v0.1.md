# MVP-B Listen Server 阶段 9：最小 UI 与四机验收（v0.1）

日期：2026-09-28。状态：C++ 最小入口和诊断已施工；**Unreal Editor 资产接线、打包启动、双机及四台有线电脑的真人测试尚待团队完成，阶段 9 未验收。** 依据：`Docs/MVP_B_Listen_Server_Plan_CN_v0.2.md` 第 5、15、17、19、20 节。阶段 8 团队报告暂未发现问题，见 `MVPB_Listen_Phase8_Implementation_CN_v0.1.md`。

## 已施工的窄职责

- `UHALNetworkGameInstance` 在本地发起 Host／Join，保存网络和地图加载错误，跨地图返回入口时仍能显示原因。Host 用 UE 默认 Listen 端口 `7777` 打开配置中的地图，Join 只接受数字 IPv4 并连接同一固定端口。地图路径可在 GameInstance 派生蓝图的 Class Defaults 中设置；本阶段不提供自定义端口、账号、Online Session、房间搜索或自动重连。
- `AHALNetworkMenuGameMode` 用于独立入口地图，不生成车辆。`UHALNetworkMenuWidget` 只提供按钮可调用的 Host／Join 和只读状态文案；`UHALMatchLobbyWidget` 只读 GameState 阶段及当前 PlayerState 数量，并复用 `AHALPlayerController::RequestStartMatch()`。服务器原有 `MinimumPlayersToStart=2`、`MaximumPlayers=4`、开局后拒绝加入规则不变。四机正式测试由主机等四人到齐再开始。
- 菜单与等待界面的创建由 GameInstance 和本地 PlayerController 执行。等待阶段鼠标可操作 Start；进入 `Playing` 后恢复游戏输入与隐藏鼠标。UMG 蓝图只排版、绑定只读显示和按钮调用，不拥有阶段状态或网络规则。
- `HALNetMetrics` 可在各机本地控制台输出本车物理帧和 **ResimRuns（重模拟连续运行次数）**。这只是车辆回退诊断，不等于所有可见位置修正次数，也不记录远端 PI 平滑次数。现有 `hal.VehicleNetLog`、`hal.BallNetLog` 保留为离散事件诊断。
- `Config/DefaultEngine.ini` 先启用 C++ GameInstance。团队创建下面的派生蓝图后，还须把 Game Instance Class 改为该蓝图以加载两个 Widget 类。当前 `GameDefaultMap` 仍是引擎 OpenWorld 模板，须在入口地图创建后由团队手动改为新地图；`EditorStartupMap` 保持 `MVPB_NetTest`，方便现有回归。

## 团队在 Unreal Editor 中接线（不得直接手改 `.uasset`／`.umap`）

1. 保存需要保留的工作，关闭 Editor 和所有游戏进程，用新编译的 C++ 打开工程。先不要改车辆、球或 Network Physics Data Asset 的值。
2. 在 `/Game/Maps` 新建空关卡，命名 **`MVPB_NetEntry`**。打开该关卡，在 **World Settings → GameMode Override** 选择 C++ `HALNetworkMenuGameMode`。此关卡不要放车辆或球，也不要在 Level Blueprint 写 Host／Join 逻辑。保存地图。
3. 在 `/Game/UI/Network` 新建 **Widget Blueprint**，父类选择 C++ `HALNetworkMenuWidget`，命名 **`WBP_MVPB_NetMenu`**。Designer 放 Host 按钮、Join 按钮、一个输入 IPv4 的 Editable Text Box、一个显示连接状态的 Text Block；可加清楚的 `192.168.x.x` 输入提示。Graph 只做四处轻接线：Host `OnClicked → Host Match`；Join `OnClicked → IPv4 输入框 Get Text → To String → Join Match`；状态文字绑定 `Get Connection Message`；错误／连接中状态都由该方法显示。不要在蓝图自行执行 Open Level 或构造网络 URL。
4. 同一文件夹新建 Widget Blueprint，父类选择 C++ `HALMatchLobbyWidget`，命名 **`WBP_MVPB_MatchLobby`**。显示 `WaitingForPlayers` 和人数（`Get Connected Player Count`）；主机 Start 按钮的 `OnClicked → Start Match`，按钮 `Is Enabled` 绑定 `Can Start Match`，主机专用区域 Visibility 由 `Is Local Host` 决定。等待面板 Visibility 由 `Is Waiting For Players` 决定，进入 `Playing` 后收起。客户端显示“等待主机开始”，不提供有效 Start 按钮。建议显示“主机可在 2～4 人时开始；四机压力测试请等到 4 人”。不要在 Widget 保存第二份阶段或人数变量作为规则来源。
5. 新建 **Blueprint Class → All Classes → HALNetworkGameInstance** 的子蓝图 **`BP_MVPB_NetworkGameInstance`**。在 Class Defaults 设置 `Menu Widget Class = WBP_MVPB_NetMenu`、`Match Lobby Widget Class = WBP_MVPB_MatchLobby`；核对 `Entry Map Path=/Game/Maps/MVPB_NetEntry`、`Match Map Path=/Game/Maps/MVPB_NetTest`。本阶段端口固定为 `7777`，无需填端口字段。在 **Project Settings → Maps & Modes → Game Instance Class** 选择该蓝图并保存项目设置。
6. 在 **Project Settings → Maps & Modes → Default Maps → Game Default Map** 选择 `MVPB_NetEntry`，保持 **Editor Startup Map** 当前 `MVPB_NetTest`。在 **Project Settings → Packaging → List of maps to include in a packaged build** 至少加入 `MVPB_NetEntry`、`MVPB_NetTest`；如打包版也要做单机基线回归，再加入 `Test`。现有 `MVPB_NetTest` 的 GameMode Override、四个 `HALPlayerStart`、默认车辆蓝图和车辆／球 DA 引用保持原样。保存 Widget、GameInstance 蓝图、地图和项目配置。
7. 先用独立进程运行，不用单进程 PIE 来验收入口流。主机从入口点 Host，客户端从入口输入主机的局域网 IPv4 点 Join。两端等待面板应显示同一阶段和玩家数；2 人即可启用主机 Start，客户端始终不能 Start。主机点 Start 后两端面板消失，驾驶输入恢复；新客户端应被服务器拒绝并能在入口看到可读原因。错误地址、主机未启动和主机退出也分别测试错误提示。若 Error 文案未能回到入口，先记录两端日志与进入的地图，不通过蓝图复制第二套连接流程修补。

## 四机验收顺序和记录

1. 先完成单机 `/Game/Maps/Test` 回归，以及两台机器通过独立进程 Host／Join 的接线和断线复测。关闭 Editor 的 **Use Less CPU when in Background**，不要将窗口后台 CPU 节流误记为网络延迟。
2. 四台 Windows 电脑使用相同 UE 5.8 补丁、代码提交、已保存资产与打包构建；同一有线交换机／路由器，记录主机 IPv4、端口、各机名称和构建版本。放行测试程序所需的 UDP 7777。每台电脑仅一名本地玩家，不在同一电脑模拟两名展示玩家。
3. 四人都进入 `WaitingForPlayers` 后主机点 Start。运行至少 `10 分钟`：持续驾驶、刹车、漂移、四车争一球、两车碰撞、墙角、持球、发射、命中、客户端持球退出与发射后退出。结束后再试主机退出及开局后第五个连接被拒。检查四端相机独立、球 State／Holder／Launcher 收敛、发射／后坐／伤害无重复，任何掉线不留下永久约束。
4. 每台机记录运行时 FPS／Game／Draw／GPU、物理帧稳定性和 `stat net` 的带宽／包情况；可用 `stat unit`、`stat physics` 和 Unreal Insights 做短时采样。每分钟在各机执行 `HALNetMetrics` 并保留日志，计算 ResimRuns 增量；同时人工记录可见大幅修正次数和发生场景，两种数据不可混用。采集主机与至少一台客户端日志、视频，记录实际 RTT、loss、jitter。低延迟有线基准通过后，再按 v0.2 第 17 节复测 `100 ms RTT + 1% loss + 20 ms jitter` 及 `150 ms RTT + 3% loss + 50 ms jitter` 压力条件；单向注入延迟不能当 RTT。
5. 如性能、流量或同步不达标，先依时间戳、车辆 PhysicsFrame／ResimRuns、球 StateSequence、发射 LaunchSequence、命中 EventSeq 定位原因，再决定是否采样调整复制频率或设置。不得为隐藏问题直接覆盖现有车辆和球调参；不在阶段 9 扩展 Iris、Replication Graph 或 GAS。

## 验收状态与限制

- `HAL_Future_CreationEditor Win64 Development` 和 `HAL_Future_Creation Win64 Development` 均已编译通过；最终代码的 `HAL.FutureCreation` 现有自动化测试 **14/14** 通过，报告位于 `Saved/Automation/MVPB_Phase9_Final/index.json`。这些自动化测试不创建 UMG 蓝图、入口地图，也不验证真人四机操作。Editor Widget、入口地图、GameInstance 蓝图及打包配置需要团队手动接线并验证；代码代理没有直接编辑这些资产。
- 在团队提供双机入口 UI 与四台真实有线电脑测试结果前，阶段 9 和 Listen Server 第一里程碑均不能标记完成。
- 当前等待阶段没有冻结车辆／球；UI 的“等待”仅表达加入闸门和开始权限，不应误当成正式比赛准备状态。没有比赛结束、重连、主机迁移或完整结算。
