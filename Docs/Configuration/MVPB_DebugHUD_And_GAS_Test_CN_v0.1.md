# MVP-B 调试 HUD 与 10-B 双端 HP 检查（v0.1）

日期：2026-10-13。范围：只读诊断与测试操作；不改车辆／球 DA、输入、物理、伤害规则或蓝图资产。适用当前 UE 5.8.2 工程。10-B 真人验收仍须按 [实施文档](../Combat/MVPB_Phase10B_GAS_Foundation_CN_v0.1.md)完成，状态入口见 [Project_Status](../Project_Status_CN.md)。

## 1. 在一台电脑启动两个独立游戏进程

先关闭旧游戏／PIE 窗口，使用最新 C++ 编译后的工程。保持 **Use Less CPU when in Background** 关闭。

打开两个 PowerShell 窗口，分别执行下面的命令。以下路径是当前开发机路径；其他电脑替换引擎和工程位置。`-game` 表示直接运行游戏，虽使用 UnrealEditor.exe，但不打开完整编辑器；无需打包。这些命令不使用 PIE 的 Number of Players 或 Net Mode 设置，也不会自动连接两端。

**第一窗口：准备作为主机**

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' 'D:\UnrealProjects\HAL_Future_Creation\HAL_Future_Creation.uproject' '/Game/Maps/MVPB_NetEntry' -game -log -windowed -ResX=960 -ResY=540 '-abslog=D:\UnrealProjects\HAL_Future_Creation\Saved\Logs\MVPB_GAS_Host.log' '-ExecCmds=hal.DebugHUD 1,hal.GASLog 1,hal.BallNetLog 1'
```

**第二窗口：准备作为客户端**

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' 'D:\UnrealProjects\HAL_Future_Creation\HAL_Future_Creation.uproject' '/Game/Maps/MVPB_NetEntry' -game -log -windowed -ResX=960 -ResY=540 '-abslog=D:\UnrealProjects\HAL_Future_Creation\Saved\Logs\MVPB_GAS_Client.log' '-ExecCmds=hal.DebugHUD 1,hal.GASLog 1'
```

1. 两个游戏窗口都先出现 Entry 菜单。第一端点 **Host**，进入 `MVPB_NetTest`，成为 Listen Server。
2. 第二端 Join 输入 **`127.0.0.1`**，点 **Join**。不要加 `:7777`；菜单会附加端口。两台真实电脑时改填主机有线网卡 IPv4，不能继续用 `127.0.0.1`。
3. 确认双方都进入测试场景，等待人数为 2。**由主机点 Start**；如需控制台调用则主机输入 `HALStartMatch`。每局只需开始一次，Playing 后不重复开始。
4. 用窗口标题／HUD 的 **Listen Host / Client** 区分两端。只有一套键鼠时点击游戏画面切换焦点，一辆作为静止靶车，另一辆驾驶和发射，之后交换。切换焦点引起输入释放是正常行为。

## 2. Ready 与 HP 怎么检查

点击要检查的游戏窗口，按键盘 **Esc 下方的 `~ / 反引号` 键**打开 Unreal 控制台，输入命令并回车，再按同一键收起控制台；使用英文输入法更方便。

```text
HALMatchStatus
HALGASStatus
```

- `HALMatchStatus` 显示本端 PlayerId、Authority、Pawn 与 Phase；开始后应是 `Playing`。
- `HALGASStatus` 列出本机可见的所有 Health 目标，而非仅本车；既在屏幕短暂显示，也写入该进程的日志。玩家车应有正确的 Owner／Avatar、`Ready=1`、`Mode=Mixed`，初始 `HP=100 MaxHP=100`。刚加载时先给 PlayerState 和属性复制一点时间。
- 当前输出含 **PlayerId**。比较两端同一 PlayerId，不依靠 `BP_RoundedVehiclePawn_C_0` 等本地 Actor 后缀。HUD 的 `[YOU]` 指本端控制车辆，在两端的位置不同。
- 客户端车辆 `Authority=0` 是正常的；Ready 指 GAS 正确绑定并取得初始属性，不代表客户端有扣血权限。

按以下顺序测试单次有效球命中，先不要加入网络模拟或连续乱射：

| 时点 | 主机玩家 A 的 HP | 客户端玩家 B 的 HP |
|---|---:|---:|
| 新开一局，未被命中 | 100 | 100 |
| A 持球主动发射，命中 B 一次 | 100 | 75 |
| B 持球主动发射，命中 A 一次 | 75 | 75 |

每次命中后，观察 **两个窗口的玩家列表**，等复制更新后应得到表中相同结果。也可双方再次执行 `HALGASStatus` 将当时数值落日志。如果 B 第一次直接降到 50、未受击的 A 也掉血、HP 自动恢复 100，或两端持续不同，都需要记录并反馈。客户端显示可以晚于服务器；HUD 每 0.1 秒刷新，本次要求最终收敛，不要求同一渲染帧变更。

继续有效命中会得到 `75→50→25→0`；需要恢复初始 100 时结束并新开一局，重新 Host／Join。**不要将 HP=0 后冲击验收混入本步**：旧命中入口在 HP=0 时跳过额外冲击的已知限制留待 10-C／10-D。

## 3. 日志在哪里

使用本页命令时，文件路径固定为工程下：

- `Saved/Logs/MVPB_GAS_Host.log`
- `Saved/Logs/MVPB_GAS_Client.log`

这是两个独立文件；同名文件下一次启动会更新，重要结果在下次运行前另存。`-log` 打开的日志窗口也可实时查看。双进程 `-game` 的全部日志不会集中到完整 Editor 的 Output Log。

搜索 **`HAL GAS:`** 可找到手动状态快照，搜索 **`GAS HP:`** 可找到 `hal.GASLog 1` 启用后的属性变化，例如 `100.0 -> 75.0`；可同时搜索 `Ready=`、`PlayerId=`。主机 `hal.BallNetLog 1` 用于补充权威球事件。客户端日志的复制通知与主机数值初始化／扣血记录不要求相同条数，以目标 HP 单次变化和最终一致为准。

## 4. HUD 开关、内容和边界

不需要新建 Widget Blueprint、设置 HUD Class 或连接 Event Graph。`AHALPlayerController` 为本地玩家自动建立 C++ `UHALDebugHUDWidget`，默认关闭；Entry 使用不同 Controller，所以菜单入口本身没有此面板，进入 NetTest 后显示。

```text
hal.DebugHUD 1
hal.DebugHUD 0
```

开关在每个进程分别设置。启动参数 `-ExecCmds=hal.DebugHUD 1` 可自动开启；退出后不会改写项目配置。单进程多窗口 PIE 共用一个 CVar，验收优先使用上面的独立进程。

左上角深色半透明面板显示：本端 Host／Client／Standalone、比赛阶段、本地 PlayerId、Ready／HP／MaxHP、水平速度（cm/s 与 km/h）、沿车头方向速度、垂直速度、油门／刹车／转向／手刹／发射输入、物理帧和重模拟运行次数，以及所有玩家车的 Ready／HP。速度读取游戏线程刚体快照，输入是本地输入采样；它们不是服务端输入包到达的证明。`Resim runs` 不等于可见位置修正次数。更多 ASC 归属和复制模式仍用 `HALGASStatus`。

面板只读，使用 0.1 秒定时刷新，关闭时跳过车辆采样；不拦截鼠标、不改变输入模式，不写 HP，不施力，不增加 RPC。Controller 退出时移除面板，Widget 释放时清理定时器。非 Shipping 构建才创建此面板。它依赖 `AHALPlayerController`，`Test` 旧 GameMode 若使用引擎 PlayerController，不会自动得到本 HUD；本次不替换单机地图的 Controller 资产。

## 5. 验证记录

- 最终代码的 Editor/Game Development Target 均编译通过；既有 `HAL.FutureCreation` 自动化 16/16 通过。自动化在初版 HUD 接入后执行；之后仅调整字号／DPI 与换行，重新编译两个 Target 并做实际渲染检查，没有把此前测试伪装成最终显示微调后的新一轮测试。
- 960×540 隐藏离屏 Listen Server／客户端截图已人工检查：半透明面板可读，客户端列出 PlayerId 256／257 的两车，均 Ready=1、HP=100、MaxHP=100；主机日志 Join succeeded。等待 UI 可与调试面板重叠，面板不参与鼠标命中；正式 HP／驾驶检查建议点 Start 后观察，此时原等待 UI 收起。
- 一次额外截图辅助脚本错误调用了 `-game` 下不可用的 EditorLevelLibrary 接口，导致 EditorScriptingUtilities 崩溃；最终主机改用直接截图命令、客户端改用已加载 World 查询后截图。此为截图工具失误，最终两端未见相同异常；不将失败运行作为通过证据。没有为它修改玩法或 HUD 规则。
- 详细摘要见 [HUD Verification JSON](MVPB_DebugHUD_Verification_20261013.json)，截图／日志位于 `Saved/`，不入 Git。测试进程已清理，没有修改资产；`git diff -- Content` 为空。

后续团队测试更新：用户已完成本地双向命中 HP 检查，双方日志确认每次减少 25 且收敛，见 [HP 日志复核](../Combat/MVPB_Phase10B_ManualHP_LogReview_20261013.json)。真实双端驾驶的完整矩阵、模拟延迟和团队 UI／输入观感仍待确认；不将本地 HP 子项通过扩大为 10-B 全部验收。
