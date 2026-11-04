# GF-6 工程收尾与后续验收

日期：2026-11-03，Asia/Tokyo。接续 `main/125395cb4464654f9f6374f7f1b9e70e2e2ae50e`，UE **5.8.3-58210709**。

用户授权补齐 GF-6 工程收尾。本步补返回接口、展示测试结束入口、打包根清单和可复跑检查。正式比赛计时、淘汰、观战、胜负与结算继续独立规划。实际打包运行、四台真实电脑及完整异常组合仍是后续验收。

## 1. 返回接口与权限

`AHALPracticeRoomGameMode::ReturnActiveRoundToRoom()` 是服务器 C++ 接口。未来服务器比赛规则可以调用；当前没有接入自动结束条件，也没有向蓝图或客户端暴露任意结束结果接口。

它只接受真实正式场地的 `Playing`，拒绝加载、倒计时、练习、已经离房或已经开始返回的重复调用。失败保留当前阶段；成功复用已有 `StartRoundTravel(true, ...)`，生成一次性交接并销毁旧 Avatar。返回后保留身份／席位／颜色／车型偏好、清除确认，玩家可以重新选车确认。新开赛增加 RoundId，并重新建立 Pawn／ASC Avatar／物理历史。

手动入口继续使用既有 `HALReturnToRoom` 和暂停页 `RETURN TO ROOM`，经已有可靠请求管线处理：

| 服务器房间配置 | 手动返回权限 |
|---|---|
| Development | 保留已有本地主机入口 |
| Exhibition，普通启动 | 默认拒绝，隐藏按钮 |
| Exhibition，非 Shipping，主机启动带 `-HALAllowTestRoundEnd` | 仅本地主机可用 |
| Exhibition，Shipping | 忽略测试开关 |

主机必须匹配原 Host PlayerState、为本地 Controller，并携带当前选车版本；拒绝附加 VehicleId。Client 单独添加命令行参数、在控制台输入命令或修改自己的 Widget 都不能授权服务器结束。`CanRequestRoundReturn()` 是服务器能力查询，自定义 Widget 继续消费已有 `RoomView.bCanReturnToRoom`／`ReturnToRoom()`，无需新增 WBP 字段或 RPC。

返回状态立即关闭操作，重复请求不再触发第二次跨图。加载超时、参与者离开和 Host 离房沿用 GF-4 原恢复路径，本步没有改变这些规则或物理协议 **3**。

## 2. 团队操作

### 2.1 常规开发房

编译本轮代码后，按现有入口 Host。保存的 Room GameMode 引用 Development 时，无需新开关；进入正式场地的 Playing 后，Host 按菜单键，选择 `RETURN TO ROOM`，或在主机控制台执行：

```text
HALReturnToRoom
```

### 2.2 展示配置测试

由团队在 Unreal Editor 中将 PracticeRoom GameMode 的 `Room Config` 手动切到 `DA_Room_Exhibition` 并保存，再用带开关的主机进程从入口创建房间。保持四人门槛，不能通过人数不足自动降为开发配置。

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'D:\HAL_Future_Creation\HAL_Future_Creation.uproject' `
  '/Game/Maps/MVPB_NetEntry' -game -log -windowed -ResX=1280 -ResY=720 `
  -HALAllowTestRoundEnd
```

主机在 UI 选择 Host，其他三人 Join。四人全部确认后才能开始；Playing 后由 Host 使用同一返回按钮或命令。测试完可手动恢复 Development。此处只提供操作步骤，代理没有切换或保存 BP／DA。

UI 的 Host／Join 使用固定 **7777**。同机其他进程填 `127.0.0.1`；其他电脑填 Host 局域网 IPv4。下节自动化工具使用独立端口，不经过 UI Join。

## 3. 打包准备与只读检查

`Config/DefaultGame.ini` 新增显式地图：

- `/Game/Maps/MVPB_PracticeRoom`
- `/Game/Maps/MVPB_Match`

保留原 NetEntry／NetTest。新增窄目录 `/Game/Data/Gameflow` 为 AlwaysCook 根，以包含 Development／Exhibition 两份房间 DA 及车辆目录；即使当前地图只引用其中一种配置，另一份也进入 Cook 根。车辆 BP／Definition／表现／输入／相机／GA／GE／已配置 UI 通过保存的硬、软引用依赖收集，不启用 CookAll，也不将整个素材库设为必打包。

当前没有命名的 TransitionMap，UE 使用默认瞬态过渡世界；以后配置独立 TransitionMap 时，须显式加入 MapsToCook。

新增 `Tools/InspectGameFlowPackaging.py`，读取实际合并后的 Packaging Settings、GI 默认入口／Host 地图／页面类、两份 Room DA、车型 Definition、地图 GameMode／PlayerState 技能和四个出生点；遍历项目包硬／软依赖并报告缺失项目包。全程不编译蓝图、不保存资产，前后校验保存资产 SHA-256。

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\HAL_Future_Creation\HAL_Future_Creation.uproject' `
  -nullrhi -unattended -nop4 -nosound `
  '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' `
  -run=pythonscript `
  '-script=D:\HAL_Future_Creation\Tools\InspectGameFlowPackaging.py --output D:\HAL_Future_Creation\Saved\GameFlow\Packaging_Preflight_New.json'
```

报告路径必须不存在，保留旧证据。成功日志标记 `HAL_GF6_PACKAGING_PREFLIGHT_PASSED`，同时核查 JSON 的 `passed=true`、`errors=[]`。这项检查不能证明实际 Cook 成功、插件内容全部齐全或发布包运行正常；配置新 WBP／车型后应重新执行。

## 4. 可复跑的同机循环测试

`Tools/TestGameFlowLoop.ps1` 创建独立进程和独立报告，检查进程退出码以及 Automation JSON，失败／超时只清理工具自己启动的进程。默认 Exhibition 四进程：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'D:\HAL_Future_Creation\Tools\TestGameFlowLoop.ps1' `
  -EngineRoot 'D:\Program Files\Epic Games\UE_5.8' -Profile Exhibition
```

Development 兼容：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'D:\HAL_Future_Creation\Tools\TestGameFlowLoop.ps1' `
  -EngineRoot 'D:\Program Files\Epic Games\UE_5.8' -Profile Development -Port 7910
```

默认报告位于新建 `Saved/GameFlow/Loop_日期时间`，启动时会输出完整目录。上述`-ExecutionPolicy Bypass`仅作用于本次子PowerShell进程，不修改系统持久策略，适用于Windows默认禁止直接运行ps1的情况。需要使用已编译的本轮 Editor DLL；该工具不自动关闭用户 Editor，不负责构建。

Exhibition 夹具在启动 URL 指定原生 PracticeRoom GameMode 与保存的 Exhibition DA，避免修改团队 BP。使用真实保存的 PracticeRoom／Match、目录、两类车辆及输入／物理初始化，四端验证全员屏障、共同倒计时、Round1→返回→重新确认→Round2→Host离房→全员入口，并检查新 Epoch、身份保持、满 HP、旧 Buff／Stun／冷却清理。四端均成功才通过。原生 GameMode 使用原生 PlayerState，**不会冒充团队 BP PlayerState 初始技能或实际自定义 WBP 接线验证**；Development 兼容组走保存的地图 GameMode。

测试使用 NullRHI，不验证画面／真实手柄；关闭测试进程 XInput 以排除额外控制器事件，不修改项目插件配置。同机独立进程也不能代替四台真实电脑。

### 4.1 Windows PowerShell 5.1误报修正（2026-11-04）

团队使用Windows PowerShell 5.1运行时，旧脚本输出`Unclean result: Host`。已读取原目录`Saved/GameFlow/Loop_20261104_101003`：Host／Client报告均1/1通过，0测试警告／失败，日志均完成两局并回入口。原错误来自工具退出码读取，不将其误记为游戏流程失败。

用真实重定向子进程复现：5.1不提前保留`Process.Handle`时，进程已退出但`ExitCode`为null，即使事后WaitForExit也仍为null；保留句柄后得到真实退出码7。旧验证使用PowerShell 7.6.5，未覆盖这一5.1差异。

工具现会在Start-Process后立即保留句柄，结束后等待输出排空；继续严格校验真实退出码与全部Automation计数，空退出码仍报错。报告检查失败也会保存Host／Client全部`Summary.json`结果，打印具体原因及目录，不再仅给出`Unclean result`。成功时每端应为`ExitCode=0, Passed=1, Warnings=0, Failed=0`。

新增`Tools/Tests/TestGameFlowLoop.ps1`：以真实重定向子进程和合成报告验证成功、非零退出、测试失败、警告、缺报告、坏JSON六种情况；夹具不启动Unreal。后续验证及本次真实双端重跑见[修正证据](GF6_LoopTool_Compatibility_Verification_20261104.json)。本次不改C++／Config／保存资产，无需因本脚本修正重新编译。

2026-11-04团队独立复跑：`Loop_20261104_103440`已逐项核查，Host17076／Client11876均`ExitCode=0, Passed=1, Warnings=0, Failed=0`，未运行／进行中均0；两端日志确认两局、新状态及回入口。本次为修正后的团队同机Development自动化证据，四机／发布包仍待后续。

## 5. 本轮验证

UE5.8.3最终结果：

| 项目 | 本轮结果 |
|---|---|
| Editor／Game Win64 Development | 均成功，41.74s／40.78s；保留已有引擎C4996弃用警告 |
| 普通原生回归 | 130/130，0测试警告／失败；含新增返回权限保护 |
| Exhibition四独立进程连续两局 | Host及3个Client均1/1，0测试警告／失败 |
| Development保存BP双端连续两局 | Host／Client均1/1，0测试警告／失败 |
| D3D12原生UI 1280×720 | 1/1，0测试警告／失败 |
| 最终只读打包预检 | 成功，86个项目包依赖存在，Commandlet 0错误／警告 |
| 资产保护 | 1183份uasset／umap逐份SHA-256相同；无代理保存资产 |
| 源码／配置保护 | 285份留档；仅6份相关源码和DefaultGame.ini变化，uproject／Build.cs未改 |

最终8份Automation JSON均无未运行或进行中项，所有专用进程退出。具体日志路径、资源清单与保护哈希见 [GF-6 验证摘要](GF6_Engineering_Verification_20261103.json)。前三轮预检曾暴露检查脚本的UE5.8类型读取及入口默认GameMode判断问题，修正脚本后最终成功；早期失败日志保留，没有为通过检查而修改保存资产或车辆规则。

## 6. 下一步验收

1. 团队确认实际 Room Profile 和 WBP 接线，运行上述预检。
2. Editor 关闭后生成 Win64 Development 包，核对 Cook／Stage 错误与文件清单。建议输出到新的忽略目录，命令示例：

   ```powershell
   & 'D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun `
     '-project=D:\HAL_Future_Creation\HAL_Future_Creation.uproject' `
     -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore `
     -archive '-archivedirectory=D:\HAL_Future_Creation\Saved\Packages\GF6_Development_New' -utf8output
   ```

   不传 `-map` 子集，以使用项目完整 MapsToCook。命令仅为后续可执行步骤，不代表本轮已经 Cook／打包。
3. 从独立发布目录运行 Host／Client，覆盖入口、两类车、技能、正式图、返回与第二局，检查图片／材质／动画／特效／输入完整。
4. 四台有线 PC：记录提交、引擎补丁、包版本和 Host IPv4，测试四人门槛／同车型／慢机加载／玩家离开／Host突然退出／错误地址／连接取消及重建房间，保留各端日志和视频。
5. 原 F4、VP-4C、FX 及真实硬件反馈继续独立记账。未执行项不标通过；本步不宣布整个 GF-6／MVP-B 验收结束。
