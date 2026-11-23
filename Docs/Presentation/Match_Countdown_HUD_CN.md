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

## 2026-11-21：MatchFlow 决定内部页面迁移

本轮从 `main/b414f06` 接续。用户已有 `WBP_GF_MatchFlow` 未提交修改及未跟踪的 `WBP_GF_Results`；开工时共 22 份 UI 资产，均不编辑。代码修改不编辑任何 Blueprint，不新增源码文件或生产管理类，不修改 PreMatchFlow 逻辑；Room 资产和原有 `GetMatchHUD → RegisterRoomScreen(Match)` 接线保持。以下是本次新入口，前文逐页由 Room 选择的描述保留为历史。

### C++ 职责与兼容

- 公共父类增加 `bMatchFlowContainer`（Designer 中显示 Match Flow Container，默认 false）。Room 在既有 Designer 扫描中识别开启配置的容器，设置 Room 引用，不把它登记为 Transition 页面。
- 在 Loading／Countdown／Playing／Results 期间，Room 将同一份完整 View 下发到该容器，只管理整个容器的激活。容器没有启用时，所有现有逐页登记、显示和独立 HUD 回退路径保留。原有 MatchHUD 登记仍可保留，且不会覆盖容器入口；容器也能独立声明 Room 已接管 HUD，避免额外生成独立 HUD。
- MatchFlow 的 Apply View 接线选择已有页面，调用新增 `ShowFlowScreen(Screen)`。这个函数只允许当前容器内直接放在 WidgetSwitcher 下的 `HALGameFlowScreenWidget` 子实例，统一完成旧页面停用、Switcher 切页、新页面 View 下发／激活、Room 引用注入和默认焦点更新。传入空引用可清空页面。它不创建控件、不重新读取 GameState、不决定比赛规则。
- 容器跳过普通页面的标题、按钮、预览和嵌套 HUD 广播；只有选中的子页面接收 View 与激活。比赛中的内部切页不反复激活容器，也不抢占已打开的 LocalMenu；离开比赛或销毁 Room 时停用当前子页。现有 Return Action 仍通过注入的 Room 提交 Host 返房请求。
- **本轮没有改动服务器的结算超时自动返房规则。** Results 按钮仍可由 Host 提前请求返房；“必须按按钮才返房”尚不成立，须另行修改 `HALPracticeRoomMatch.cpp` 的自动返房路径，不能用 UI 隐藏或自行切回 Selection 代替服务器地图迁移。

### 团队只在 MatchFlow 中接线

1. 完整构建后重启 Editor，在 `WBP_GF_MatchFlow` 将父类由 UserWidget 改为 `HALGameFlowScreenWidget`，Class Defaults 勾选 **Match Flow Container**。其他页面保持关闭此配置；整个 Room 只启用一个 MatchFlow 容器。保留 GetMatchHUD／GetResults 现有返回类型和实例引用。
2. MatchHUD、ResultsWidget 放在 `MatchPageSwitcher` 的直接子级；两者都是公共父类的派生页面。Results 的 Panel 设为 Results，Actions 保留 `ReturnButton → Return`；不要再手动绑定第二条 OnClicked 返房路径。
3. 在 MatchFlow 增加继承的 **Event Apply View**，先 **Add Call to Parent Function**，然后拆出 View.Phase，根据阶段调用 **Show Flow Screen**（Target=Self，Screen=对应的已有控件）。Playing 接原 MatchHUD，Results 接 ResultsWidget；按现有功能需要处理 Loading、Countdown 与观战分支。只有两个子页时，不存在的准备页分支传空引用，不临时创建新 WBP，也不要把准备阶段误当作 Playing。若需兼容 View.ShowResults 提示，可在 Switch 前先判断该字段并优先进入 Results。
4. 原生 helper 会完成切换、View 转发、激活和 Room 引用注入，不再额外接 Set Active Widget、Apply View 或 Set Screen Active，也不在 Results 内查询 GameState／Room。**不要往 WBP_GF_Room 增加 Results 登记，也不要在 MatchFlow 内向 Room 逐页登记。** 已经试接的 Results 单独登记可以撤去；Room 原有 Match 登记保留。
5. 手动验收：正式比赛 HUD → 比赛结束 Results → Host 按钮请求返房 → 车辆选择 → 第二局。再覆盖 LocalMenu 开关、观战、Host／Client 返回按钮权限、加载／倒计时阶段，以及目录延迟加载后的 HUD 数据。开关默认 false，因此 C++ 完成不等于用户资产已经接入新模式。

### 本轮验证

新增原生专项 `HAL.FutureCreation.GameFlow.UIIntegration.MatchFlowOwnsPages`，用内存控件模拟 ApplyView 接线，检查无需 Room Blueprint 改动的自动识别、原登记兼容、准备／HUD／Results 迁移、完整 View／能量／倒计时、返回按钮绑定与焦点、LocalMenu、错误子页面拒绝、由容器改变阶段映射、返房与下一局及清理。

本机 UE **5.8.3-58210709**，`HAL_Future_CreationEditor / Win64 / Development` 完整 R1 构建 **Succeeded，退出 0，53/53 步骤，432.04 秒**，两个正式 DLL 均完成链接。日志 `Saved/UIIntegration/MatchFlowBuild_20261121.log`。未改动文件的数值转换／弃用警告仍保留，不能描述成整个构建零警告。

首次 UIIntegration **4/5 通过、1 项失败**：新增测试的默认焦点断言失败，原因是内存夹具未构建按钮的 Slate 控件，UE `UWidget::IsVisible()` 在没有 Slate 时返回 false。测试随后通过 TakeWidget 持有真实 Slate 按钮；**没有为迁就夹具修改生产焦点规则**。R2 增量构建 **Succeeded，退出 0，4/4 步骤，19.19 秒**；日志 `Saved/UIIntegration/MatchFlowBuild_R2_20261121.log`。首轮失败报告保留在 `Saved/UIIntegration/MatchFlowTests_20261121/index.json`，日志同名 `.log`。自动化进程退出码可能为 0，结果以 JSON 的用例状态为准。

最终使用 R2 正式 DLL，在两个独立的 UnrealEditor-Cmd 无界面 NullRHI 进程中验证，均退出 0：

- `HAL.FutureCreation.GameFlow.UIIntegration` **5/5 成功**，含新增 MatchFlowOwnsPages 和既有 ChildLifecycle、MatchHUDRootLifecycle、MatchHUDView、PresentationPriority。报告 `Saved/UIIntegration/MatchFlowTests_R2_20261121/index.json`，日志 `Saved/UIIntegration/MatchFlowTests_R2_20261121.log`。
- `HAL.FutureCreation.GameFlow.GF7.MatchHUDServerDeadline` **1/1 成功**，独立 HUD 回退保持。报告 `Saved/UIIntegration/MatchFlowStandalone_20261121/index.json`，日志 `Saved/UIIntegration/MatchFlowStandalone_20261121.log`。

合计 **6/6 成功，0 测试事件警告、0 错误、0 失败、0 未运行**。验证前后 22 份 UI 资产 SHA-256 全部一致，保护清单 `Saved/UIIntegration/MatchFlowAssets_20261121.json`；用户已有的 MatchFlow 修改和 Results 新资产保持，Room／MatchHUD／PreMatchFlow 资产未编辑。`git diff --check` 通过，未新增工程源码／资产文件，未提交或推送。没有打开 Editor GUI、没有保存／修改 Blueprint，没有进行 PIE／真实地图迁移／多人或手柄视觉验收，Game Target 未单独构建；上述测试是原生控件合同验证，不能替代团队按上节完成资产接线后的实测。

## 2026-11-21：Results 独占显示，避免旧选车结果重叠

团队已接通 MatchFlow → Results，随后回报 Results 与车辆选择页面中的旧结果同时显示。原有管理只停用已登记的页面，未覆盖 Room Designer 中未登记的旧页面和直接放置的文字；保留的 ReceiveRoomView 仍可能显示旧选车／结算分支。因此切中 Switcher 的 Results 不等于整个 Room 只剩该页面。

本次在 Room 现有 C++ 分发层补上临时显示约束：启用 MatchFlow 容器且进入 Results 时，沿 Room 内通向 MatchFlow 的布局路径折叠其它同级分支，涵盖未登记的旧 WBP 和 Room 自身文字；路径上的布局祖先与 MatchFlow 保持可见，内部子页依旧由 MatchFlow 决定。没有按资产路径／控件名硬编码，也没有移除、销毁或重新创建旧页面。

每次新 View 下发前，先恢复这些分支原来的 Visibility，再执行旧 ReceiveRoomView 和现有原生分发，最后重新施加 Results 约束。这样旧蓝图反复显示结算内容也不会覆盖新页面；退出 Results、返房或清理时恢复原值，随后由旧选车／练习规则决定显示，原本 Hidden／Collapsed 的内容不会被强制设为 Visible。不增加 Tick，不改阶段、权限或自动返房规则，不修改任何 WBP 或 PreMatchFlow 逻辑，不新增工程文件。已正确接好 MatchFlow 的项目不需要新增接线，须完整构建后重启 Editor 加载新 DLL。

新增专项 `HAL.FutureCreation.GameFlow.UIIntegration.ResultsHideLegacyRoomContent`：用现有合同 Room 夹具复现“旧 ReceiveRoomView 在 Results 仍显示自身结果”的路径，检查旧内容继续收到数据但不可见、未登记旧页面／Room 文字和深层同级分支被折叠、Flow／Results 保持可见、反复 View 不复显、返回前先恢复再执行旧可见性规则、原 Hidden 保留、回选车、清理，以及未启用新容器时旧 Results 行为不变。

本机 UE **5.8.3-58210709**，本轮 `HAL_Future_CreationEditor / Win64 / Development` 完整构建 **Succeeded，退出 0，53/53 步骤，852.41 秒**，两个正式 DLL 均已链接。日志 `Saved/UIIntegration/ResultsExclusiveBuild_20261121.log`；未改动文件的既有数值转换和弃用警告保留。

随后使用本轮正式 DLL 在独立无界面 NullRHI 进程验证：

- UIIntegration **6/6 成功**，含本轮 ResultsHideLegacyRoomContent 及原有五项；报告 `Saved/UIIntegration/ResultsExclusiveTests_20261121/index.json`，日志 `Saved/UIIntegration/ResultsExclusiveTests_20261121.log`，进程退出 0。
- MatchHUDServerDeadline **1/1 成功**；报告 `Saved/UIIntegration/ResultsExclusiveStandalone_20261121/index.json`，日志 `Saved/UIIntegration/ResultsExclusiveStandalone_20261121.log`，进程退出 0。

合计 **7/7 成功，0 测试事件警告／错误／失败／未运行**。开工时保存的 22 份 UI 资产 SHA-256 全部相同，清单 `Saved/UIIntegration/ResultsExclusiveAssets_20261121.json`；Room 及 PreMatchFlow 未编辑，用户已完成的 MatchFlow／Results 接线和资产修改保留。`git diff --check` 通过；没有新增工程源码或资产文件，没有提交／推送。未打开 Editor GUI，未修改或保存 Blueprint，未单独构建 Game Target，未进行本次修复后的实际 PIE 画面、多人、手柄或地图迁移验收。团队下一步只需重启 Editor，以现有接线确认 Results 期间旧选车结果／Room 文字／练习页面不再出现，然后返回选车并开第二局；用户已确认的“能进入 Results”是修复前的流程验收，不能当作本次重叠修复的画面证据。

## 2026-11-21：Loading／Countdown／Results 独占显示约定

用户确认：**MatchFlow 的 Loading、Countdown、Results 三个页面最终均采用独占显示，但按占位 WBP 的准备进度分别启用。** 独占显示时只呈现当前页面，隐藏 GF_Room 的其它内容及其它 Screens 页面；Room 与 MatchFlow 容器保留，用于数据分发和页面管理，不因此销毁。

| 阶段 | 当前占位状态 | 当前独占显示 | 后续执行条件 |
|---|---|---|---|
| Loading | 尚未准备占位 WBP | 暂不启用，保持现状 | 后续准备并接入 Loading 占位 WBP 时，再单独启用并验证该阶段的独占显示 |
| Countdown | 尚未准备占位 WBP | 暂不启用，保持现状 | 后续准备并接入 Countdown 占位 WBP 时，再单独启用并验证该阶段的独占显示 |
| Results | 已有占位 WBP，已接通流程 | 已在 C++ 启用，实际画面待复测 | 保持现有独占规则，验证旧选车结算不重叠及返回后恢复 |

这是已确认的设计约定，**本轮只记录文档，不提前创建 Loading／Countdown 占位，也不现在扩展 C++ 的独占阶段判断**。当前代码仍仅对 Results 施加显示约束；后续有了占位文件不能视为独占已自动生效，届时须分别完成页面接线与相应独占规则的实现／配置，并检查进入、退出及切到下一阶段时的显示与恢复。避免在没有对应占位页面时提前隐藏旧 UI，造成空白界面。

Playing 的 HUD／观战／LocalMenu 行为、Room 的选车／练习迁移及服务器规则均保持现状。前一节的构建和 7/7 测试是 Results 实现的历史验证，不代表 Loading／Countdown 独占显示已经实现或测试；本轮文档更新不重新声明编译或自动化通过。

本表为 2026-11-21 当时的准备状态；2026-11-22 已按用户指令扩展 Loading／Countdown，见文末本轮记录。

## 2026-11-21：Results 等待 Return 按钮返房

用户要求关闭结算超时自动返房，并以最小修改保留 Return 按钮路径。本节覆盖前文“自动返房仍保留／必须按按钮返房尚未实现”的历史限制。

生产代码只修改 `HALPracticeRoomMatch.cpp`：`PollMatchRules` 在比赛完成后直接返回，不再比较 ReturnServerTime 并自动调用 ReturnActiveRoundToRoom，也不再走该自动调用失败后的返入口兜底。正常 Results 阶段一直保持，直到主机通过既有 Return 动作提交返房请求；Room／PlayerController／GameMode 的现有权限、请求参数和实际地图迁移路径保持。加载、开赛倒计时、比赛结束判定、正常故障处理和开发调试入口不改。

为保留蓝图、配置和结果快照兼容性，ResultsSeconds、ReturnServerTime、ResultsRemaining 字段及其现有显示数据暂不删改，但这些值到期不再触发返房；无需修改 DA 数值，也不要将 ResultsSeconds 设置为不符合既有校验的 0。当前 Results 占位只有 Return 按钮，按钮接线保持。本轮没有修改 Room、MatchFlow 或 PreMatchFlow 的生产代码，没有修改 WBP 或新增参数／数据结构。

对应测试在已有文件中维护：新增 ResultsWaitForHostReturn，强制原截止时间过期后重复评估，检查仍在 Results、结果冻结、Host 请求可用、非 Host／旧 Revision 被拒绝；旧 GF7 三局／四人夹具改为超过原结果期限后手动返房，合同 UI 模式通过实际 UButton.OnClicked 广播提交请求；重连夹具的 Host 返房及重连等待也同步适配，不再依赖服务器自动迁移。

本机 UE **5.8.3-58210709**。Editor Win64 Development R1 完整构建 **Succeeded，57/57 步骤，562.53 秒，退出 0**；R2 仅增加网络测试的控件诊断后增量构建 **Succeeded，4/4 步骤，29.25 秒，退出 0**，生产返房代码不再改变。日志 `Saved/UIIntegration/ManualResultsReturnBuild_20261121.log`、`ManualResultsReturnBuild_R2_20261121.log`。既有未改动文件的数值转换与弃用警告保留。

原生比赛规则及 UI 专项 **14/14 成功，0 测试事件警告／错误／失败／未运行，进程退出 0**，包括新增 ResultsWaitForHostReturn、既有结束判定／冻结结果／截止时间／返房准入及六项 UIIntegration。报告 `Saved/UIIntegration/ManualResultsReturnTests_20261121/index.json`，日志 `Saved/UIIntegration/ManualResultsReturnTests_20261121.log`。R2 只改网络测试诊断，未重新把这份报告描述为 R2 原生复跑。

实际双进程 Listen Server／Client 做了三轮尝试，每次都运行完整三局：

- R1／R2 启用合同 UI。Host 都是 **1/1 成功、测试事件零警告／错误、退出 0**；通过真实 UButton.OnClicked 提交既有 Return 动作，三局均超过旧的 3 秒期限后才按下，最后存活、超时并列和同批全灭结果及返房后下一局均可完成。
- R2 诊断显示 Client 已进入 Results，有当前世界的 Room 控件对象（All=1），但没有挂在视口的该类控件（Visible=0），因而旧客户端用例的“结果 UI 已观察”检查没有完成。R3 取消合同 UI，使用项目当前 UI 配置复测，现象相同。三轮均是 **Host 1/1 成功、Client 0/1 失败**；双方进程均退出 0，但启动器严格检查 JSON 后退出 1，不能把双端整项记为通过。报告分别在 `Saved/UIIntegration/ManualResultsReturnNetwork_20261121`、`ManualResultsReturnNetwork_R2_20261121`、`ManualResultsReturnNetwork_R3_20261121` 的 Host／Client／Summary.json，详细日志在对应目录。
- R3 Host 从进入 Results 到接受手动返房的三次间隔分别约 **4.521、4.531、4.543 秒**，均大于原 3 秒期限；Client 三次返房地图迁移发生在 Host 接受请求后的约 **26、39、43 毫秒**，不是客户端自行计时退出。客户端的 UI 视口检查仍未通过，不以地图迁移成功替代该检查。

为满足最小生产修改要求，本轮没有改 UI 工厂／客户端视口重挂代码，也没有把上述现象直接认定为有渲染客户端的表现；后续需独立验证客户端实际 Results 显示及控件重挂。同步适配的四人 Forfeit／重连夹具已编译，未在本轮重新运行四人／重连专项。未单独构建 Game Target、未打开 Editor GUI、未做实际手柄或打包验收。

22 份 UI 资产与开工快照 SHA-256 全同，清单 `Saved/UIIntegration/ManualResultsReturnAssets_20261121.json`；原有 WBP／配置／数据结构／Room／MatchFlow／PreMatchFlow 生产代码不改，未新增工程文件，Saved 下的启动器和报告仅为本轮验证产物。`git diff --check` 通过，未提交或推送。团队使用当前占位 Results 无需改变接线，重启 Editor 加载本轮 DLL 后，在正常结算停留超过原期限，再按 Host Return 验证返房。

## 2026-11-22：Loading／Countdown 启用独占显示

用户已准备两个占位并要求启用独占显示。本轮基线 `main/2482dbd`，开工时有未提交的 WBP_GF_Room／WBP_GF_MatchFlow 修改，以及未跟踪的 WBP_GF_Loading／WBP_GF_Countdown；以这些实际磁盘版本作为保护基线，共 24 份 UI 资产。本轮不编辑或保存任何 WBP，不修改 PreMatchFlow，不新增源码、资产或配置。

生产修改仅在 HALRoomWidgetInterface.cpp：将原 Results-only 条件扩为 Loading／Countdown／Results（含 bShowResults 提示），仍要求有效 Room View 和已启用的 MatchFlow 容器。每次 View 先恢复原 Visibility，再执行旧 ReceiveRoomView 与原生页面分发，最后折叠通向 MatchFlow 路径之外的其它分支，包含未登记的旧页面和 Room 自身文字；保留布局祖先及当前 Flow 子页。沿用原恢复缓存与清理函数，未增加 Tick、阶段状态或资产名称依赖。

Loading → Countdown 连续切换时仍保持独占；进入 Playing、返回 Open、退出或清理时恢复旧显示值，原 Hidden／Collapsed 不强制设为 Visible。Playing 的 HUD／观战／LocalMenu、选车／练习和服务器加载屏障／开赛倒计时／比赛规则／Host 手动 Return 均不改。页面选择仍由 MatchFlow 的 ApplyView → ShowFlowScreen 接线负责，已接好时不需新增 Blueprint 节点；本次 C++ 启用不代表已验证新资产的可视接线。

保留既有自动化名称 ResultsHideLegacyRoomContent，在同一夹具中补 Loading、Countdown 的连续切换、完整 View 转发、Pending 与重复 View 下不复显、准备阶段回选车、进入 Playing 恢复与 LocalMenu、Loading 中途清理，以及未启用 MatchFlow 时三阶段旧行为保持。原 Results 各项断言保留。

本机 UE **5.8.3-58210709**，`HAL_Future_CreationEditor / Win64 / Development` 完整构建 **Succeeded，49/49 步骤，752.15 秒，退出 0**，两个正式 DLL 均完成链接。日志 `Saved/UIIntegration/LoadingCountdownExclusiveBuild_20261122.log`。未修改代码的既有数值转换／弃用警告保留，本次两个修改 cpp 未报告编译错误／警告。

随后使用本轮正式 DLL，在两个独立的 UnrealEditor-Cmd 无界面 NullRHI 进程中执行：UIIntegration **6/6 成功**，报告 `Saved/UIIntegration/LoadingCountdownExclusiveTests_20261122/index.json`；独立 MatchHUDServerDeadline **1/1 成功**，报告 `Saved/UIIntegration/LoadingCountdownExclusiveStandalone_20261122/index.json`。合计 **7/7 成功，0 测试事件警告、0 错误、0 失败、0 未运行**，两个进程均退出 0；按 JSON 状态核对，不仅依赖进程退出码。扩展后的 ResultsHideLegacyRoomContent 覆盖本轮 Loading／Countdown 各分支，其余既有页面优先级、生命周期、MatchFlow、HUD 缓存和独立倒计时回退用例维持通过。

编译与测试后，开工时的 **24 份 UI 资产 SHA-256 全部一致**，含用户未提交的 Room／MatchFlow 和未跟踪的两个新占位；清单 `Saved/UIIntegration/LoadingCountdownExclusiveAssets_20261122.json`。`git diff --check` 通过；本轮仅修改一个生产 cpp、现有测试 cpp 和两份既有交接文档，不新增工程文件、不编辑任何 WBP／PreMatchFlow，不提交／推送。用户已有四项资产改动保持。

Editor GUI／PIE／有渲染客户端与真人联机验收未执行，Game Target 未单独构建；上一轮已知的 Client Results 视口观察失败不在本轮扩大修复。团队下一步保存工作并重启 Editor 加载本轮 DLL，以已接好的占位验证 Loading → Countdown → Playing、Results → Host Return → 选车，以及下一局：三个独占阶段不残留旧 Room／Screens 内容，离开后原可见性和菜单恢复。无界面内存控件合同测试不等同于新 Blueprint 接线或实际画面已通过。

## 2026-11-22：正式比赛转场使用新版 Loading

用户实际确认 Countdown 画面正确，但进入比赛的 Loading 仍显示旧画面。`HALNetworkPresentation.cpp` 中的 `TRAVELLING TO THE ARENA` 是 GameInstance 直接添加到视口的 ZOrder=1000 Slate 层，既用于 Create Room 也用于地图转场，不在 Room 的 WidgetTree 内；前节 Room 内部独占约束不能隐藏这一层。这是本轮待替换的显示入口，不删除旧画面。

本轮保持 `main/2482dbd` 及之前未提交工作。生产侧只改现有四个文件：`HALNetworkGameInstance.h/.cpp`、`HALNetworkPresentation.cpp`、`HALPlayerControllerRound.cpp`。新增可选 MatchTravelLoadingClass 和一份 Transient 临时控件引用；ShowPracticeRoom 在实际 Open 房间记录规范化后的地图名，PreClientTravel 提供目标 URL 和无缝标记，以源地图为房间地图且目标为另一非入口地图识别进入比赛，不要求客户端事先收到 Locked／Loading 的复制阶段。PIE 前缀、URL 参数不影响匹配，反向返房保持旧显示；没有新增 RPC、地图配置或硬编码比赛资产路径。

进入比赛时，先沿用原路径移除离开的 Room，再创建由 GameInstance 所属的新占位实例作为原转场层的内容；清除 PlayerContext，整页不可命中，不设置 RoomRoot、不传旧 FHALRoomView、不查询旧世界 Actor。加载完成后仍由既有 ShowPracticeRoom／EndTravelPresentation 移除转场层并释放临时实例，新地图的 Room → MatchFlow 正常显示自己的 Loading → Countdown。返回入口、网络／转场失败及 Shutdown 继续复用现有清理入口。类为空、抽象／过期或创建失败时使用旧 Slate 遮罩，不留加载空档。当前仅支持用户的静态占位用途，不对阻塞加载期间的动画或进度刷新作承诺。

编辑器侧由团队手动设置，不添加节点或新资产：

1. 保存工作并重启 Editor，加载本轮正式 DLL。
2. 打开项目当前 `GameInstanceClass` 指向的 `/Game/Blueprints/Network/BP_MVPB_NetworkGameInstance`，进入 Class Defaults。
3. 在 Front End → Screens 下，将新增 **Match Travel Loading Class** 指定为已有 **WBP_GF_Loading**；Compile／Save。保持原有 Room Screen Class 与 MatchFlow 页面接线。
4. 从入口 Create Room，确认仍用旧遮罩进入选车；选车／练习后开始比赛，确认转场使用新占位，随后接上 MatchFlow 的 Loading／Countdown；Results 的 Host Return 仍返回选车，下一局重复通过。

原生专项在现有 WidgetExtensionTests／TestAccess 中补转场方向与临时控件生命周期用例，覆盖首次创建／加入、PIE 参数、返房、返回入口、缺失目标、重复 Begin、清理／Shutdown、未配置及抽象类回退。生命周期用例通过真实 HideTravelWidgets 入口替换已有旧遮罩，检查只挂一层、临时实例 Outer 为 GameInstance、没有旧 PlayerController／Room 数据且不捕获输入。无窗口测试夹具在本机 5.8.3 仍需使用 SetViewportOverlayWidget；该版本尚未公开弃用提示所指的 Bind 替代，兼容宏仅限该行，生产代码不使用这一接口。

本机 UE **5.8.3-58210709**，Editor Win64 Development：R1 完整构建 **52/52、624.33 秒、Succeeded、退出 0**（MatchTravelLoadingBuild_20261122.log）；R2 测试夹具增量构建 **4/4、17.77 秒、退出 0**（MatchTravelLoadingBuild_R2_20261122.log）；R3 修正测试收尾后的增量构建 **4/4、13.44 秒、退出 0**（MatchTravelLoadingBuild_R3_20261122.log）。日志均在 Saved/UIIntegration。生产代码首次构建即编译／链接通过，后两次没有修改生产逻辑；既有未改文件的数值转换与弃用警告保留。

首轮无界面 UIIntegration 没有完成：六项既有 UI 用例已成功，新增 TravelLoadingLifecycle 在测试收尾时，于 Shutdown 已清空 GameInstance.WorldContext 后再次取用而崩溃，进程退出 1、没有完整 JSON 报告；保留 MatchTravelLoadingTests_20261122.log，不记为通过。R3 只在测试中保存有效 Context 指针再清理，生产逻辑不变。

使用 R3 正式 DLL 重新执行整组 UIIntegration，**8/8 成功**，报告 Saved/UIIntegration/MatchTravelLoadingTests_R2_20261122/index.json；另一个独立进程执行 MatchHUDServerDeadline，**1/1 成功**，报告 Saved/UIIntegration/MatchTravelLoadingStandalone_20261122/index.json。合计 **9/9 成功，0 测试事件警告／错误／失败／未运行**，两进程退出 0，均逐项核对 JSON 状态；这些是本轮新报告，不复用前节 7/7。

不编辑／保存 WBP 或 PreMatchFlow，不新增工程文件，不修改服务器阶段、加载屏障、倒计时、比赛和手动 Return 规则。开工／编译／自动化后的 **24 份 UI 资产 SHA-256 全同**，清单 Saved/UIIntegration/MatchTravelLoadingAssets_20261122.json；保留原有 Room／MatchFlow 修改和两个未跟踪的占位资产。git diff --check 通过，未提交／推送。本轮 Editor GUI／实际 PIE／多人画面未执行，Game Target 未单独构建；上述新类字段仍待团队配置，既有 Client Results 视口观察问题不在本轮扩大修复。完成类配置及实际画面验收前，只声明代码／编译／原生专项完成。

## 2026-11-22：Loading 内部分为地图与人数两部分

用户明确要求在同一 Loading WBP 内分开设计地图加载 UI 与加载结束后的人数 UI。此前 PREPARING MATCH／PLAYERS LOADED 来源于 2026-11-19 d615f46 的父类默认填充，不是本轮转场替换新增的页面或阶段；这次仅对现有页面内的两个容器控制可见性，保留加载屏障与倒计时规则。

生产仅修改既有 HALGameFlowWidgets.cpp（12 行）：当 Panel=Loading、不是 MatchFlow 容器且采用普通 Panel 表现时，在 NativeConstruct 和 FinishView 按缓存 FHALRoomView.bRoomAvailable 切换两个同级容器。没有 Room View 的临时转场实例只显示 MapLoadingPanel；新地图 Room 的有效 View 到达后显示 PlayersLoadingPanel，人数继续来自 Room View，旧默认 TitleText／DetailText 填充仍保留于该部分。两个命名控件均存在才启用，不改变旧 Loading 布局，Countdown／HUD／Results 不适用该规则；不新增 Tick、枚举、RPC 或数据字段，不调用旧世界 Actor，也不改变地图何时加载结束的定义。

团队在 Editor 手动完成以下设置；代码代理不编辑 WBP：

1. 打开现有 WBP_GF_Loading，确认父类 HALGameFlowScreenWidget，Class Defaults 的 Panel=Loading、Match Flow Container 不勾选。
2. Designer 根 Canvas Panel 下保留全屏背景 Img_Background，在其上放两个**同级**容器，精确命名 MapLoadingPanel 和 PlayersLoadingPanel。可用 Overlay 或 Vertical Box；两个容器的 Canvas Slot 都全屏 Stretch，四个 Offset=0，ZOrder 高于背景。
3. MapLoadingPanel 内放地图加载期间的文字／图片，文字用自由名称，例如 MapLoadingText；Designer 文字可为 LOADING MAP…。不要在此部分使用 TitleText 或 DetailText。
4. PlayersLoadingPanel 内放现有 TitleText 与 DetailText。TitleText 接收 PREPARING MATCH，DetailText 接收 PLAYERS LOADED x / y；从根移入容器后保持控件名，不加新的蓝图读数或定时器。
5. 默认 MapLoadingPanel 可见，PlayersLoadingPanel=Collapsed。两个容器建议勾选 Is Variable 方便在 Designer 识别；C++ 按精确名称查找，无需事件图接线。
6. Compile／Save。GameInstance 的 Match Travel Loading Class 和 MatchFlow 的 Loading 页仍引用同一 WBP_GF_Loading。保存并重启 Editor 加载新 DLL，再验收进入比赛时地图部分 → Room 数据就绪后人数部分 → Countdown；转场实例与 MatchFlow 实例仍是两个对象，不保留旧 Room 跨图。

新增原生 LoadingParts 用例放在既有测试文件，检查初次 Construct 显示地图且不伪造人数、Room View 切换／人数更新、地图自定义文字保持、其它 Panel 和缺少容器的兼容、Room 不可用时隐藏旧人数。

本机 UE **5.8.3-58210709**，Editor Win64 Development 首轮构建未通过：生产 cpp 编译成功，新增测试用 auto* 推断 TObjectPtr<UWidgetTree> 失败，日志 Saved/UIIntegration/LoadingPartsBuild_20261122.log，436.24 秒、退出 1。仅将测试读取改为 WidgetTree.Get() 后，R2 完成 **5/5 步骤、20.49 秒、Succeeded、退出 0**，两个正式 DLL 均链接成功，日志 LoadingPartsBuild_R2_20261122.log；生产切换逻辑没有为测试另行改动，既有其它文件警告保留。

使用本轮 R2 正式 DLL 的 UIIntegration **9/9 成功**，报告 Saved/UIIntegration/LoadingPartsTests_20261122/index.json；独立进程的 MatchHUDServerDeadline **1/1 成功**，报告 Saved/UIIntegration/LoadingPartsStandalone_20261122/index.json。合计 **10/10 成功，0 测试事件警告／错误／失败／未运行**，两进程退出 0，逐项核对 JSON；均为本轮新报告，不复用前节 9/9。原生夹具没有加载或保存用户 WBP，不能作为实际资产接线与渲染画面通过的证据。

本轮保护快照包括 24 份 UI 资产及用户已修改的 BP_MVPB_NetworkGameInstance，共 **25 份 SHA-256 开工后全同**，清单 Saved/UIIntegration/LoadingPartsAssets_20261122.json；未编辑任何资产、PreMatchFlow 或配置，不新增工程文件，git diff --check 通过，未提交／推送。未打开 Editor GUI、未单独构建 Game Target；实际布局和 PIE／多人画面待团队完成，既有 Client Results 视口观察问题未在此扩大修复。
