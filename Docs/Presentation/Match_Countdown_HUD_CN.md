# 正式比赛倒计时 HUD

2026-11-16；起点 main / 821034f；引擎 UE 5.8.3-58210709。

## 当前范围

新增可继承的 C++ `UHALMatchHUDWidget : UUserWidget`，默认原生布局仅显示 `MM:SS`。位于玩家屏幕顶部水平居中，上边距 24 个 Slate 逻辑单位，黑色 32 号字，白色矩形背景 `FLinearColor(1,1,1,0.5)`，内边距左右 24、上下 8。无标签、动画、技能、血量或其他新增效果。

读取 `AHALMatchGameState → RoomComponent → Match.EndServerTime`，减去 `GetServerWorldTimeSeconds()`，剩余秒数向上取整、最低为零。沿用现有比赛时间上限 3600 秒；每 0.1 秒刷新显示，不本地累计减秒，不控制结束比赛。只有 Playing、Match.bActive 且尚未完成时显示，赛前／结算／无有效数据时隐藏，不显示占位提示。

本地 `AHALPlayerController` 自动创建并添加到本地玩家屏幕；复用已有就绪轮询处理晚到的 LocalPlayer。旅行和 EndPlay 清理实例，Widget Destruct 清理刷新定时器。HUD 不可聚焦且 HitTestInvisible，不切换 InputMode、不修改 IMC 或驾驶状态。观战时仍显示同一局时间。

原生 PracticeRoom 的顶部综合驾驶条仅在 Open 阶段且菜单关闭时显示，即选车确认后的 Practice 驾驶。Host Start 请求被服务器接受、离开 Open 后，Locked／Loading／Countdown／Playing／Results／Returning 均不显示旧顶栏。新 MatchHUD 在正式 Playing 时显示本局倒计时；加载与开赛倒计时阶段沿用现有流程页面，不提前显示比赛时间。其他原有菜单、观战、结算与场景表现不改。自定义 Room/Pause Widget Blueprint 不会被代理改写：如其中已绘制比赛时间或同位置的顶部条，团队需自行将旧顶栏限定为 Open 且菜单关闭时显示。

## 应用与继承

当前 Match 使用 `AHALPlayerController` 派生 Controller 的既有流程时，无需增加 Widget Blueprint、修改地图或 Level Blueprint，完整编译后从 NetEntry 进入比赛即可自动显示。

可选扩展入口是 Controller 的 Class Defaults → Match → UI → Match HUD Class，留空使用原生 HUD。若以后需要 Designer 自定义布局，团队手动新建父类 `HALMatchHUDWidget` 的 Widget Blueprint，并配置到该字段。存在 Designer 根控件时使用其布局；可用继承的 `Get Countdown Text` 只读接口绑定文本。当前需求无需此操作。不要配置到 GameInstance 的 Room Screen Class，它们属于不同基类与生命周期。

直接打开 Match 可以创建 HUD 实例，但没有有效 Playing 比赛数据就保持隐藏；本轮没有建立开发用假比赛或绕过房间流程。

## 验证与团队复核

最终 UE5.8.3 Editor R3 构建成功；用户保存关闭 Editor 后，两个项目 DLL 链接成功，退出码 0。使用新 DLL 在 NullRHI 下执行相关自动化，报告确认 **9/9 成功、0 测试警告、0 失败、0 未运行**：GF5 只读视图／未绑定操作、Widget 类回退；GF7 批量淘汰与不可变结果、截止时间与单人局、离场与主机淘汰、新增 MatchHUDServerDeadline、回房间普通加入与延迟携带、预留席位／准入以及确定性胜负规则。原计划按本文件估计 7 项，过滤器还匹配两项既有回房间测试，实际以报告 9 项为准。

新增专项覆盖无 World 不制造倒计时、服务器截止时间向上取整、整分钟边界、过期显示 00:00、显示读取不结束权威比赛及完成后隐藏。首轮 Slate 括号错误已修复；第二轮编译通过但 Editor 占用 DLL 导致 LNK1104，第三轮关闭 Editor 后成功。保留全部阶段证据，不把失败轮次写成通过。重编存在既有浮点精度／弃用 API 警告；自动化启动日志另有 Editor 布局版本警告，测试事件本身无警告。没有修改无关玩法代码或 Blueprint／地图资产。

本机证据：`Saved/MatchHUD/20261116/Editor_R1.log`、`Editor_R2_LinkBlocked.log`、`Editor_R3_Success.log`、`Tests.log`、`Tests/index.json`。Saved 不纳入 Git，以上结论随本文档同步。实际渲染、完整 NetEntry→Match 双端真人输入、Game Target 与打包未验证；NullRHI 自动化不等同于透明度／位置的画面验收。

团队从 NetEntry 建房并开始 Match，检查：顶部仅新倒计时；背景为半透明白、文字黑色；从本局配置时间递减；主机与客户端时间基本一致；驾驶／漂移／发射／技能输入不被抢占；菜单开关后计时继续；淘汰观战仍计时；比赛结束后隐藏；返回房间再开一局无重复控件。直接打开未初始化 Match 应不显示虚假时间。检查 1280×720 与 1920×1080 的顶部位置及自定义 Room 页有无重复顶栏。

2026-11-16 显示范围补充：将旧顶栏条件从排除 Playing 改为明确仅允许 Open；本次 Editor R4 构建成功（构建系统触发 51 项关联重编），新 DLL 的 Tests_R4 回归 9/9 成功、0 测试警告／失败／未运行，进程退出 0。构建既有警告保留；本次仍未进行实际画面与手柄输入验收。证据：Saved/MatchHUD/20261116/Editor_R4_PracticeOnly.log、Tests_R4.log、Tests_R4/index.json。此前 R3／Tests 记录保留。

## 2026-11-20：Room 向 MatchHUD 的最小数据分发

基于干净的 `main/b97ca6c`，实际本机引擎为 UE 5.8.3-58210709；项目状态中的其他设备 5.8.2 构建记录不是本轮证据。没有新增源码、资产或数据结构。

当前资产已由团队移动到 `Screens/PreMatchFlow` 和 `Screens/MatchFlow`。Room 已在 Construct 中调用 `MatchFlowWidget.GetMatchHUD()`，将返回的 HUD 注册为 Match 页面。`WBP_GF_MatchFlow` 当前为 UserWidget 容器，只有 GetMatchHUD，没有 Switcher 或加载／结果页面；没有独立的 PreMatchFlow 管理 WBP。本轮不扩展这些页面职责。

本轮 C++ 仅在已有 `FHALRoomView` 末尾追加四个只读字段：

- `LoadedCatalog`：复用 Room 根页面原有的异步目录加载。
- `LocalUltimateEnergy`：当前本地 Avatar 的既有能量显示结构。
- `bShowUltimateEnergy`：沿用原来 HUD 使用的 Cost 与动作权限判断，由 Room 采样。
- `ServerWorldTimeSeconds`：由 Room 提供的服务器时间，用于保持能量条原有的显示插值。

`GetRoomView()` 只在 Playing 采样后三项，Open 选车／练习阶段不增加周期时间或能量采样，不改变其刷新条件。已有 Match.Players 的内容、顺序和编号，以及旧 View 字段、页面解析、输入、按钮与 Preview 逻辑均不改。

Room 接管后的原生 MatchHUD 从 View 读取能量、显示许可和服务器时间，不再独立查询 Pawn／ASC／GameState。比赛时间继续使用现有 MatchRemaining 和 GetCountdownText。独立测试场未被 Room 接管的 HUD 保留原读取及定时回退路径。ReceiveMatchView、GetMatchView 等既有接口不改名、不改参数；MatchHUD 父类不变。

所有 uasset／umap 均不编辑，包括 WBP_GF_Room、MatchFlow、MatchHUD、PlayerStatusRow、Network 参考画面和 PreMatchFlow 文件夹。没有调用 Editor、修改默认配置、重排玩家或修改选车／练习逻辑。

### 团队随后只需在 WBP_GF_MatchHUD 手动完成

1. 在 Receive Match View 中，从事件 View 拆出新增 LoadedCatalog，赋给现有 LoadedCatalog 变量，然后执行原来的 RefreshPlayers。初始为 null 时也赋值，避免保留旧目录；Room 完成加载后会通过同一事件更新。
2. RefreshPlayers 继续使用 GetMatchView → Match.Players 和现有 SetPlayerData，保留现有行、HP、编号与车型表现。
3. 确认上述路径生效后，移除运行时 Construct 中自行 GetGameState／GetRoom、绑定 RoomChanged、读取 GetData.Catalog 和异步加载目录的旧链路。仅清理依赖该旧链路的变量／事件；保留 Designer 预览所需的表现分支。
4. GetText → GetCountdownText 及能量显示的既有继承接口保持不变。无需修改 Room 注册接线、MatchFlow 或 PreMatchFlow。

这些 Blueprint 接线未在本轮执行，因此不能将当前资产描述为“已全部改由 Room 供数”。团队接线后仍需确认目录延迟加载、血量／车型显示、能量恢复、技能消耗、观战、返回房间及第二局；至少覆盖一主一客。

已有 `HAL.FutureCreation.GameFlow.UIIntegration.MatchHUDView` 已扩展并随 Target 编译：无 Pawn 时使用供应的能量、目录更新／清空、失活不复显，以及由 Room 决定能量显示许可。

本次 `HAL_Future_CreationEditor / Win64 / Development` 完整构建 **Succeeded，退出 0，50/50 构建步骤完成，394.48 秒**；运行时与 Editor 模块均已链接，生成的反射代码确认四个新字段存在。日志为 `Saved/UIIntegration/RoomViewBuild_20261120.log`。未改动代码仍有既有数值转换和弃用警告，本次修改文件未报告编译错误／警告。首次沙箱调用停在脚本临时锁阶段，已停止；上述成功结果来自获准的沙箱外构建。Game Target 未单独构建。

本次 `git diff --check` 通过，Content／Config 和 PreMatchFlow 相关页面逻辑无修改。检查前后 Content/UI 下 21 份 uasset 的 SHA-256 全部一致，含 Room、MatchFlow、MatchHUD 和 PreMatchFlow 两个 WBP；没有新增工程文件。按用户要求没有启动 Editor，自动化／PIE／联机实测未执行，回归用例只能记为“已编译、未运行”。本轮未提交或推送。

## 2026-11-20：MatchHUD 公共职责上移到 GameFlowScreenWidget

本轮根据用户新指令整合两个现有类，起点为 `main/1983179`。开始时 `WBP_GF_MatchHUD.uasset` 已有用户刚完成的未提交接线，本轮以该磁盘版本为保护基线，不修改任何 UI 资产，不修改 PreMatchFlow、Room 的阶段规则或 Results 自动返房规则，不新增文件。

继承链调整为 `UserWidget → HALGameFlowScreenWidget → HALMatchHUDWidget → WBP_GF_MatchHUD`。Blueprint 的直接父类依然是 HALMatchHUDWidget，团队不需要重新指定 WBP 父类。原有 ReceiveMatchView、GetMatchView、GetCountdownText、GetLocalUltimateEnergy 的名称、归属和参数保留。

- 通用父类统一保存 LatestView、Room 接管状态与页面激活状态，并在销毁时清理。新增只读 GetScreenView；GetMatchView 直接返回同一份父类缓存，不再有 HUD 自己的 RoomView 副本。
- ApplyView、SetScreenActive 的公共实现由父类提供。MatchHUD 仅通过 OnRoomViewApplied／OnScreenActiveChanged 两个 C++ 回调更新倒计时、能量及既有 ReceiveMatchView 事件。首次以 inactive 接管也会触发一次通知，以停止独立定时器；相同激活状态不重复通知。
- MatchHUD 仍保留 MM:SS 格式、默认原生时钟、能量条、专用 Slate／定时器清理和未接入 Room 的独立测试回退。
- MatchHUD 关闭通用 Panel 表现，防止新的继承关系使 Room 在 PrepareView／FinishView 时覆盖 HUD 自己的 TitleText／StatusText、生成选车预览或给被动 HUD 设置按钮焦点。普通页面的既有按钮、预览和显示适配仍保留。
- Room 的 RegisterRoomScreen、阶段解析和 MatchFlow 容器职责不改；本轮不另外建设页面管理器。

现有 MatchHUDView 回归增加父类／旧接口快照一致性检查；新增 MatchHUDRootLifecycle 覆盖 Room 注册、显示／隐藏、跨局再激活、原有标题不被覆盖，以及 HUD 不提供菜单焦点。既有 MatchHUDServerDeadline 专项用于验证独立测试场回退。

早期验证：实际本机 UE 5.8.3-58210709。首轮编译发现继承后的局部变量 bUsesRoomView 与父类成员重名（C4458），已改名为 bRoomManaged，并停止首轮构建后重编。R2 中四个修改文件、回归用例和相关模块源码编译完成，反射代码确认 HALMatchHUDWidget 的父类为 HALGameFlowScreenWidget。但运行时与 Editor 两个正式 DLL 都因 Windows 文件占用错误 32 发生 LNK1104，Target 构建失败，耗时 428.58 秒。以下 R3 已完成正式链接，保留本段作为失败轮次记录。

早期日志为 `Saved/UIIntegration/SharedScreenBuild_20261120.log`、`SharedScreenBuild_R2_20261120.log`。R2 阶段未执行自动化、PIE、联机或 Game Target，未强制关闭用户的 Editor。用户随后已保存文件并关闭 Editor，首次局部命名错误和 DLL 占用均已解除。

本轮 `git diff --check` 通过。以开始时含用户接线的磁盘版本为基线，Content/UI 中 21 份 uasset 的 SHA-256 均未变化；用户已有 MatchHUD 资产改动保留，Room、MatchFlow、PlayerStatusRow、PreMatchFlow 和所有其他 UI 资产均未编辑。没有新增工程文件，未提交或推送。

团队接续时保存已有资产并用本轮完整构建重启 Editor，检查上述继承链和原有事件／取值节点后验收同一局 HUD；不要通过 Live Coding 替代原生反射父类变更的完整构建。Blueprint 的可视接线与多人视觉效果不属于本轮自动通过范围。

### R3 正式链接与无界面专项结果

用户关闭 Editor 后，`HAL_Future_CreationEditor / Win64 / Development` R3 构建 **Succeeded，退出 0，4.59 秒**。运行时与 Editor 两个正式 DLL 均已链接，Target 元数据已写入；不是重定向输出的独立链接校验。日志：`Saved/UIIntegration/SharedScreenBuild_R3_20261120.log`。

随后在两个独立的 UnrealEditor-Cmd 无界面进程中，使用本轮正式 DLL、`/Engine/Maps/Entry` 和 `-nullrhi` 验证：

- `HAL.FutureCreation.GameFlow.UIIntegration` **4/4 成功，0 警告、0 错误、0 失败、0 未运行**：ChildLifecycle、PresentationPriority、MatchHUDView、MatchHUDRootLifecycle，含本轮共享缓存、Room 注册和激活、标题不覆盖、被动 HUD 焦点及跨局再激活检查。进程退出 0。报告 `Saved/UIIntegration/SharedScreenTests_20261120/index.json`，日志同目录的 `SharedScreenTests_20261120.log`。
- `HAL.FutureCreation.GameFlow.GF7.MatchHUDServerDeadline` **1/1 成功，0 警告、0 错误、0 失败、0 未运行**，独立 HUD 的无 World、服务器截止时间、取整边界和比赛结束隐藏回退维持。进程退出 0。报告 `Saved/UIIntegration/SharedScreenStandalone_20261120/index.json`，日志同目录的 `SharedScreenStandalone_20261120.log`。

合计 **5/5 专项通过**；上述零警告指自动化测试事件，不将引擎启动日志或 R2 中既有编译警告描述为零。没有打开 Editor GUI、没有运行 PIE 或多人实测、没有单独构建 Game Target，也没有编辑／编译保存任何 Blueprint 资产。验证前后再次对保存后的 21 份 UI 资产做 SHA-256 比较，确认测试未改动用户接线；PreMatchFlow 与 Room／MatchFlow 资产不变。`git diff --check` 通过，未新增工程文件，未提交或推送。
