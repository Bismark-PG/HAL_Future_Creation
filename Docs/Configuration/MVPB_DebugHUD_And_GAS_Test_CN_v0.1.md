# MVP-B 调试 HUD 与 10-B 双端 HP 检查（v0.1）

初版日期：2026-10-13；生成球与技能操作／启动日志更新：2026-10-16。范围：调试显示与测试操作；`HALSpawnBall`会临时生成一个服务器球，不改保存的DA或蓝图资产。当前10-D迁移／验收见[冲击施工记录](../Combat/MVPB_Phase10D_Implementation_CN_v0.1.md)，10-B历史HP子项按本文保留，最新状态见 [Project_Status](../Project_Status_CN.md)。

## 1. 在一台电脑启动两个独立游戏进程

先关闭旧游戏／PIE 窗口，使用最新 C++ 编译后的工程。保持 **Use Less CPU when in Background** 关闭。

打开两个 PowerShell 窗口，分别执行下面的命令。以下路径是当前开发机路径；其他电脑替换引擎和工程位置。`-game` 表示直接运行游戏，虽使用 UnrealEditor.exe，但不打开完整编辑器；无需打包。这些命令不使用 PIE 的 Number of Players 或 Net Mode 设置，也不会自动连接两端。

**第一窗口：准备作为主机**

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' 'D:\UnrealProjects\HAL_Future_Creation\HAL_Future_Creation.uproject' '/Game/Maps/MVPB_NetEntry' -game -log -windowed -ResX=960 -ResY=540 '-abslog=D:\UnrealProjects\HAL_Future_Creation\Saved\Logs\MVPB_GAS_Host.log' '-ExecCmds=hal.DebugHUD 1,hal.GASLog 1,hal.BallNetLog 1,hal.CombatLog 1,hal.VehicleNetLog 1,hal.VehicleHistoryLog 1,hal.ImpactPhysicsLog 1,hal.AbilityLog 1'
```

**第二窗口：准备作为客户端**

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' 'D:\UnrealProjects\HAL_Future_Creation\HAL_Future_Creation.uproject' '/Game/Maps/MVPB_NetEntry' -game -log -windowed -ResX=960 -ResY=540 '-abslog=D:\UnrealProjects\HAL_Future_Creation\Saved\Logs\MVPB_GAS_Client.log' '-ExecCmds=hal.DebugHUD 1,hal.GASLog 1,hal.BallNetLog 1,hal.CombatLog 1,hal.VehicleNetLog 1,hal.VehicleHistoryLog 1,hal.ImpactPhysicsLog 1,hal.AbilityLog 1'
```

两端的开关完全相同，Host／Join换图后仍保留在各自进程内；退出后不改写项目配置。它们分别开启文字HUD、GAS属性、球状态／命中、权威战斗结算、车辆网络输入／发射和物理帧历史日志。`hal.CombatLog`的结算内容来自服务器，客户端开启后也不会自行结算伤害／冲击，所以两端日志条数和内容无需完全相同。此版本不自动开启碰撞球形Gizmo，也不注入延迟／丢包。

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

继续有效命中会得到 `75→50→25→0`；需要恢复初始 100 时结束并新开一局，重新 Host／Join。本文表格验证单次HP变化；旧入口HP=0后跳过额外冲击的限制已在10-C解除，当前有效冲击与HP独立，完整档位／免疫／脱球仍按10-D矩阵验收。

## 3. 日志在哪里

使用本页命令时，文件路径固定为工程下：

- `Saved/Logs/MVPB_GAS_Host.log`
- `Saved/Logs/MVPB_GAS_Client.log`

这是两个独立文件；同名文件下一次启动会更新，重要结果在下次运行前另存。`-log` 打开的日志窗口也可实时查看。双进程 `-game` 的全部日志不会集中到完整 Editor 的 Output Log。

搜索 **`HAL GAS:`** 可找到手动状态快照，搜索 **`GAS HP:`** 可找到 `hal.GASLog 1` 启用后的属性变化，例如 `100.0 -> 75.0`；可同时搜索 `Ready=`、`PlayerId=`。主机 `hal.BallNetLog 1` 用于补充权威球事件。客户端日志的复制通知与主机数值初始化／扣血记录不要求相同条数，以目标 HP 单次变化和最终一致为准。

10-D额外搜索主机日志中的`HAL Combat:`，核对`SourceTier`、`EffectiveTier`、`ImpactImmune`和`Drop`。需要当时所有车辆状态时，在进入测试地图、双方完成加入后再手动执行`HALGASStatus`；不把一次性状态命令放在Entry启动参数里冒充持续日志。`HALImpactDebug <PlayerId> <TierReduction> <Immune>`按目标实际编号由主机执行，不自动给任何玩家加免疫或降档。

物理线程修复后的新开关`hal.ImpactPhysicsLog 1`输出`Vehicle impact physics:`：含Event、实际物理Frame、Internal BeforeV／BeforeW和QueuedDV／QueuedDW。它证明动作已经被物理回调消费，而不只是在游戏线程排队；Queued是将积分的变化量，不是碰撞／阻尼之后的最终速度。设为0关闭，Shipping不输出。上面的两条启动命令已加入此开关。

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

2026-10-14 接续更新：用户已确认 10-B 驾驶／持球／冲击测试完成并授权进入 10-C，测试日志可能未上传。上述回归改为团队完成反馈；具体网络工况、引擎版本及 HUD／UI 观感未单独提供，不追加未经核对的量化结果。详见 [10-B 团队反馈](../Combat/MVPB_Phase10B_GAS_Foundation_CN_v0.1.md)。

## 6. 在当前车辆前方补一个默认球

使用本次完整重编的Development版本进入NetTest，控制自己的车辆后，在**主机或客户端自己的游戏控制台**输入：

```text
HALSpawnBall
```

无需参数、PlayerId或额外蓝图接线。服务器按调用者当前所控制车辆的水平车头方向，在根位置前方350cm、向上100cm处生成一个Free球。客户端只请求生成，不传目标／位置／球类别；服务器生成后正常复制给其他玩家。生成时没有攻击归属或发射冲量，随后落地、自动控球和争夺沿用原有规则，因此靠近车辆后可能正常被捕获。

“默认球”引用配置`[/Script/HAL_Future_Creation.HALDebugSettings]`中的`DefaultBallClass`，当前指向已有`BP_BasicBall`；继续使用它引用的普通球DA和已调数值，包括当前Heavy档位，不强制改成Medium或原生默认值。不复制场上某颗球的实时持有者／攻击状态，也不要求场上预先有球。

空间被墙／其他球占据时拒绝生成并提示，不把球推到墙内或改到车辆背后。需要车辆离墙远一点再执行。Entry尚未进入车辆或控制非车辆Pawn时也会拒绝。球没有自动销毁时限，反复执行会增加球数；结束当前测试局可清理，不保存进地图。

主机日志及请求端日志搜索`HALSpawnBall:`可见成功／失败、调用者PlayerId及生成位置；请求端同时有短暂屏幕提示。Shipping版本命令不生效。项目设置中搜索`HAL Debug`可由团队调整默认类别、距离和高度；C++不硬编码资产路径，默认引用保存在`Config/DefaultGame.ini`，不修改现有资产。

本轮UE5.8.2的Editor／Game编译成功，29/29自动化0警告／0失败；新测试覆盖已配置球、车头相对位置和阻挡／缺失配置等拒绝。实际NetTest隐藏主机执行命令成功，客户端Join后收到对应动态球；324资产前后未变、专用进程已清理。客户端亲自输入命令发起RPC、真人捕获／发射和网络扰动尚未实测。详见[生成球命令验证摘要](../Combat/MVPB_DebugBall_Verification_20261016.json)。

## 7. 10-E主动技能与免疫／冷却检查

使用最新完整构建进入NetTest，两端加入并由主机Start后，在想发动的一端执行：

```text
HALUseSkill
HALGASStatus
```

原生PlayerState已授予首个GA，无需先创建GA／GE蓝图。服务器接受后自身免疫规则冲击8秒，冷却从发动起20秒；不免伤、不清除已有速度。冷却内重复输入拒绝，不能连按刷新。客户端的`Requested=1`只是提交请求，以复制后的状态判断是否发动成功。

本地HUD新增`Primary skill: Granted / Immune / CD`；`HALGASStatus`新增`PrimarySkillGranted`、`ImmunityRemaining`、`CooldownRemaining`。默认约t=8免疫归0、冷却剩12，t=20冷却归0可再发动。其他客户端不一定能看到远端Spec与GE时长，远端Granted=0或时长=-1不代表技能失效；用`ImpactImmune`与本地Owner状态核对。旧Test地图没有匹配GameState时技能拒绝，先用联机测试图。

上方两条启动命令已默认开启`hal.AbilityLog 1`。日志搜索`Ability accepted`、`Ability rejected`、`Effect added`／`Effect removed`；服务器才记录权威接受。测试技能前清除此前`HALImpactDebug`的免疫贡献，避免独立无限免疫掩盖8秒到期；该命令现在使用独立调试GE，关闭不会移除技能GE。

按键／手柄接线、数值配置和双向受击验收步骤见[10-E操作说明第3～5节](../Combat/MVPB_Phase10E_Implementation_CN_v0.1.md)。当前已完成33项普通回归与真实双端技能请求／复制测试；真人按键、持球受击和网络扰动仍待团队反馈，见[10-E验证摘要](../Combat/MVPB_Phase10E_Verification_20261016.json)。
