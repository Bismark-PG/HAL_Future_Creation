# MVP-B Listen Server 阶段 0/1 实施与编辑器配置

日期：2026-09-24  
依据：`Docs/MVP_B_Listen_Server_Plan_CN_v0.2.md`  
范围：施工前基线检查、最小联机外壳、两人连接与归属验证

## 1. 当前交付状态

已完成 C++ 编译的联机基础类：

| 类 | 职责 |
|---|---|
| `AHALMatchGameMode` | 服务器登录、容量、出生槽、退出和开始规则 |
| `AHALMatchGameState` | 复制 `WaitingForPlayers`／`Playing`；为 UI 提供阶段变化事件 |
| `AHALPlayerController` | 本地相机入口、主机开始请求、临时控制台诊断 |
| `AHALPlayerState` | 稳定玩家身份；第二里程碑的 ASC 承载位置 |
| `AHALPlayerStart` | 显式编号的出生槽 `0～3` |

此处使用 `AGameModeBase`／`AGameStateBase` 派生类，因为本阶段需要玩家在 `WaitingForPlayers` 时就生成并 Possess 车辆。正式比赛计时、淘汰和结算以后在这些项目类中扩展，不在蓝图 Event Graph 中实现。

阶段 1 只验证连接、归属、生成和本地镜头。当前车辆输入仍由 `ATestVehiclePawn` 本地采集，尚未接入客户端到服务器的持续输入传输；客户端驾驶的权威性和最终手感属于阶段 3～5 的验收内容。

## 2. 施工前基线与代码决定

- 保留 `/Game/Maps/Test` 作为单机回归地图；联机使用 `/Game/Maps/MVPB_NetTest`。
- 编辑器资产尚未由代码代理修改。当前两张地图、玩家车／球蓝图和主要 Data Asset 的只读哈希在 `MVPB_Phase1_PreEditor_AssetSnapshot_20260924.json`。
- `BP_RoundedVehiclePawn` 已引用保存的玩家车辆和 Local Data Asset；新联机 GameMode 应引用这个蓝图类，不应把其配置复制到 C++ 默认值。
- 原 `UVehicleKnockbackSettings` 在配置 CDO 上保存单个 `CachedWorld`，不适合多 World PIE。规则实际只依赖同一份全局 CombatRules Data Asset，现改为与 World 无关的只读配置快照；`InitializeRules()` 在首次加载时验证并缓存，命中路径仍不懒加载。
- Network Physics 配置资产预留路径为 `/Game/Data/Network/DA_NetworkPhysics_MVPB`。本阶段尚不创建该资产，也不启用物理预测；创建和填值属于阶段 4。

2026-09-18 的历史 Test 快照与当前 Test 文件哈希不同，这是两个时间点的工程状态；当前文件不按旧快照自动回退。

## 3. Unreal Editor 手动配置步骤

### 3.1 创建联机 GameMode 蓝图

1. 关闭正在运行的 PIE，完成 C++ 编译后重启 Unreal Editor，使新 UCLASS 正常出现在类选择器。
2. Content Browser 打开 `/Game/Blueprints/Gamemode`。
3. 右键 **Blueprint Class** → **All Classes**，搜索 `HALMatchGameMode`，创建子蓝图 `BP_MVPB_ListenGameMode`。
4. 打开新蓝图 → **Class Defaults**，核对：
   - **Default Pawn Class** = `/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn`。
   - **Game State Class** = `HALMatchGameState`。
   - **Player Controller Class** = `HALPlayerController`。
   - **Player State Class** = `HALPlayerState`。
   - **Match | Players → Maximum Players** = `4`。
   - **Match | Players → Minimum Players To Start** = `2`。
5. Compile、Save。Event Graph 保持空白；加入、开始和生成规则已在 C++。

不要重新指定车辆的 `Definition`、`LocalConfig`、输入 Action、网格或碰撞参数。当前已调 Data Asset 继续由 `BP_RoundedVehiclePawn` 使用。

### 3.2 设置联机地图

1. 打开 `/Game/Maps/MVPB_NetTest`。
2. 打开 **World Settings**，将 **GameMode Override** 设为 `BP_MVPB_ListenGameMode`。不要修改 `/Game/Maps/Test` 的 GameMode。
3. 在 World Outliner 检查是否已有预放置的**玩家车辆**。若有，先确认它不是被动测试车，再仅从 `MVPB_NetTest` 中移除；否则 GameMode 还会额外生成玩家车辆，场上出现重复物理车。保留球、被动测试车和场地对象。同时核对 `BP_RoundedVehiclePawn` 的 **Pawn → Auto Possess Player** 为 `Disabled`。若原蓝图默认值并非 `Disabled`，请新建其联机用子蓝图、只在子蓝图关闭 Auto Possess，并让新 GameMode 引用这个子蓝图；保留原蓝图的调参基线。
4. 在 **Place Actors** 搜索 `HALPlayerStart`，添加四个实例，分别设置 **Match | Spawn → Spawn Slot** 为 `0`、`1`、`2`、`3`。
5. 将四个出生点放到安全、互不重叠的位置；以玩家车碰撞体外缘为准留出车辆间和墙体间的间隙，确认朝向不会立刻撞墙。
6. 如果复制地图时带有普通 `PlayerStart`，可在新地图中移除它；新 GameMode 只使用 `HALPlayerStart`。
7. 检查 `BP_RoundedVehiclePawn` 的 Class Defaults：`Vehicle | Configuration` 仍为 `Definition`，引用原玩家车辆 Data Asset；只核对，不重填数值。
8. Save `BP_MVPB_ListenGameMode` 与 `MVPB_NetTest`。不要保存对 `Test`、原车辆蓝图或既有 Data Asset 的意外修改。

至少需要两个不同编号的 `HALPlayerStart` 才允许进入阶段 1 测试。建议一次布置四个，以便之后直接验证容量限制。重复编号或缺少足够槽位会在登录时给出错误，不会随机选择普通 PlayerStart。

## 4. 两人 Multi-PIE 验证

1. 在 `MVPB_NetTest` 打开时进入 **Play** 下拉设置：**Number of Players = 2**、**Net Mode = Play As Listen Server**、**Run Under One Process = Off**，使用独立窗口。不要选择 Dedicated Server。
2. 开始 PIE。主机和客户端窗口应各自获得一辆 `BP_RoundedVehiclePawn`，且出生位置不同。
3. 在主机窗口打开控制台，输入 `HALMatchStatus`。期望显示 `Local=1 Authority=1`、非空 Pawn、`Phase=WaitingForPlayers`。
4. 在客户端窗口输入 `HALMatchStatus`。期望显示 `Local=1 Authority=0`、另一辆 Pawn、`Phase=WaitingForPlayers`。
5. 分别观察两窗镜头是否以各自 Pawn 为中心；轻按本地驾驶输入可辅助检查镜头跟随。若客户端本地驾驶出现服务器拉回，本阶段先记录现象；持续输入权威传输尚未接入。
6. 在客户端先输入 `HALStartMatch`，主机日志应记录拒绝，双方阶段仍为 `WaitingForPlayers`。
7. 在主机输入 `HALStartMatch`，双方再次输入 `HALMatchStatus`；应显示 `Phase=Playing`。
8. 停止 PIE，检查 Output Log 中 `LogHALMatch` 的进入、出生、拒绝和阶段切换记录；不得出现“joined without a vehicle Pawn”。

`HALMatchStatus` 与 `HALStartMatch` 是 `AHALPlayerController` 上的控制台命令，只在调用时输出，不增加 Tick。后续 Host UI 将调用同一 `RequestStartMatch()` 接口。

## 5. 独立进程与加入闸门验证

在同一台电脑或局域网两台电脑上分别运行编辑器 Standalone／打包构建时：

1. 主机打开 `MVPB_NetTest?listen`；确认主机进入 `WaitingForPlayers`。
2. 客户端通过 `open <主机 IPv4>:7777` 加入。若显式配置了其他端口，使用实际端口；PIE 端口可能不同，以日志为准。
3. 两名玩家在主机输入 `HALStartMatch` 后，另起第三个客户端尝试连接，应被拒绝并看到 `The match has already started.`。
4. 在未开始的第二局中尝试第五名玩家，应收到 `The match is full.`；本项可等到四人测试时执行。
5. 断开一个客户端，服务器应销毁该玩家 Pawn，其他玩家仍能看到空出的出生槽。当前球权及发射归属的完整断线处理属于阶段 8，不能据此宣布已通过。

双人 Multi-PIE 会在按 Play 时同时启动两名玩家，不能据此验证“开局后第三人加入”。同机验证可打开三个独立 PowerShell 窗口，先在第一窗口启动主机：

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' 'D:\UnrealProjects\HAL_Future_Creation\HAL_Future_Creation.uproject' '/Game/Maps/MVPB_NetTest?listen' -game -log -port=7777 -windowed -ResX=960 -ResY=540
```

第二窗口启动客户端，进入地图后在游戏控制台输入 `open 127.0.0.1:7777`：

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' 'D:\UnrealProjects\HAL_Future_Creation\HAL_Future_Creation.uproject' '/Game/Maps/MVPB_NetTest' -game -log -windowed -ResX=960 -ResY=540
```

主机执行 `HALStartMatch` 后，再在第三窗口重复客户端启动命令并尝试 `open 127.0.0.1:7777`。局域网电脑将 `127.0.0.1` 换为主机 IPv4，并确认防火墙允许该程序的 UDP `7777` 端口。以上命令是待团队执行的操作示例，尚未由本次施工实际运行。

当前 `WaitingForPlayers`／`Playing` 只管理加入闸门和公共阶段；等待阶段没有冻结车辆、球或伤害，不应按完整赛前准备期验收。

联机结束条件应以独立进程和实机测试为准；单进程 PIE 可能掩盖本地控制与 World 隔离问题。

## 6. 验收和下一阶段

当前自动验证：`HAL_Future_CreationEditor Win64 Development` 和 `HAL_Future_Creation Win64 Development` 均编译通过。`HAL.FutureCreation` 的 10 项自动化测试全部通过，报告在本地 `Saved/Automation/MVPB_Listen_Phase1`。`git diff --check` 通过；快照中 9 个地图／蓝图／Data Asset 文件的哈希在编译和测试后保持不变。

首次运行时 `CombatRulesSource` 测试失败，因为测试夹具依赖旧的 Legacy 项目默认值，而当前项目已配置 Definition 和 CombatRules。测试已显式设置被验证的 Legacy 模式及空规则引用；重新编译、重跑后 10 项全部通过。未修改实际 CombatRules 资产或运行时数值。

实际两人 PIE、蓝图引用、地图出生位置和加入拒绝仍必须由团队在编辑器中完成，结果尚未预填为通过。

阶段 1 的可验收条件：

- 两个独立进程加入同一 Listen Server。
- GameMode 为每名玩家生成并 Possess 各自的已配置车辆蓝图。
- 两个本地镜头只跟随各自车辆。
- 仅主机能把阶段从 `WaitingForPlayers` 改为 `Playing`。
- `Playing` 拒绝后续连接；容量和无效出生槽也能给出明确拒绝原因。
- `/Game/Maps/Test` 与现有 Data Asset 保持可回归。

完成上述编辑器与两人验证后进入阶段 2：`60 Hz` 固定物理执行路径、网络输入／状态数据结构和单机手感回归。不要把阶段 1 的客户端本地移动现象当作已完成车辆网络预测。

### 2026-09-25 团队实测反馈

团队反馈命令行测试暂时没有发现问题。此反馈记为阶段 1 独立进程命令行路径的初步实测结果；目前尚无逐项结果或日志留档可核对两端 `HALMatchStatus`、客户端开始比赛被拒、主机开始比赛成功及开局后第三人加入被拒。因此阶段 1 暂不标记为正式验收完成，待这些结果确认后收口。阶段 2 的代码审计和单机基线采样可先行开展。
