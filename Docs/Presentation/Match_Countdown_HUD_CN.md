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
