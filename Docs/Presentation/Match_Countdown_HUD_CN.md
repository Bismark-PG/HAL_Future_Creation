# 正式比赛倒计时 HUD

<a id="results-loading-panel-20261126"></a>

## 2026-11-26：Results 前仅显示专用 Loading 文案（代码／蓝图已修改，C++ 待构建）

用户批准最小方案：保留 MatchFlow 现有 Match → Loading（0.3 秒）→ Results，过渡 Loading 只显示 ResultLoadingPanel 中的 ResultLoadingText，地图与开赛人数两部分折叠。基线仍为 main/9e55a7f，实际引擎 UE 5.8.3 CL58210709；保留前轮资源预载和 Countdown DetailText 清理。

### 本轮实际修改

| 文件 | 改动 |
|---|---|
| Source/HAL_Future_Creation/HALGameFlowWidgets.cpp | 仅扩展 HALFlowDisplay::LoadingParts：结果阶段或 bShowResults 提示时显示可选 ResultLoadingPanel，折叠 MapLoadingPanel／PlayersLoadingPanel；地图加载与开赛等待时折叠结果部分。没有结果 Panel 的旧布局继续使用原 Map 面板回退。C++ 不查找／写入 ResultLoadingText。 |
| Content/UI/GameFlow/Screens/MatchFlow/WBP_GF_Loading.uasset | 只把已有 ResultLoadingPanel.Visibility 从 SelfHitTestInvisible 改为 Collapsed，作为默认状态；通过当前正在运行的 Editor Python 接口修改、编译并保存。 |
| Source/HAL_Future_Creation/Tests/HALWidgetExtensionTests.cpp | 扩展已有 LoadingParts 断言，覆盖三种显示、Results 阶段／提示、旧布局回退、下一局恢复和 Designer 文案保留；实际 ResultsRankingBlueprints 夹具补充专用 Panel 显示与地图／人数折叠检查。断言已写入，未编译／运行。 |
| 本文、Docs/Project_Status_CN.md | 记录实际资产差异、验证和编译限制。 |

Editor 只读检查确认：WBP_GF_Loading 开工时没有未提交或未保存修改；CanvasPanel_14 下已有 MapLoadingPanel、PlayersLoadingPanel、ResultLoadingPanel 三个同级 Overlay。ResultLoadingPanel 内原本就只有 ResultLoadingText，文字 **WAITING FOR RESULT**，因此没有新增或移动控件，没有改文案、字体、动画或 Event Graph。公共 Img_Background 保留，阶段内的地图／人数文字由对应 Panel 隐藏。现有 0.3 秒计时、MatchFlow／Results 蓝图、资源预载及服务器阶段均未修改。

### 已完成验证与明确限制

- 当前运行 Editor 中该 WBP **编译／保存成功**；独立 UnrealEditor-Cmd Python 进程从磁盘重新加载，检查控件树、所有已有文字／显隐，确认只发生上述默认 Visibility 差异，蓝图再次编译成功、0 编译错误，退出 **0**。此检查仅验证保存后的布局资产，不验证新 C++ 显隐逻辑。首轮读回因 Python 命令行脚本路径反斜线转义失败，已用正斜线路径重跑成功，失败日志保留。
- 56 份 UI／GameInstance 资产 SHA256 保护清单中，**仅 WBP_GF_Loading 变化，其余 55/55 保持**；包括用户 Countdown 与背景纹理。git diff --check 通过。
- 统一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup` 被占用检查阻止，退出 **1**，检测到 UnrealEditor PID **17872**、LiveCodingConsole PID **5324**，未进入 C++ 编译。用户明确选择继续保持 Editor 打开，因此未终止用户进程、未用 Live Coding 或另一个 Editor 构建入口绕过。
- **本轮 C++ 编译、新增断言、完整 UIIntegration、PIE／双端画面均未验证。当前 Editor 仍运行上轮 DLL，新分流要在下一次完整构建后生效。** 上方／下方此前 13/13 等通过记录属于上一轮，不代表本次断言通过。

本机证据：`Saved/ResultLoading/20261126/{BeforeAssets.json,LoadingBefore.json,LoadingEdit.json,LoadingVerified.json,LoadingReloaded.json,ReadBack_R2.log}`；脚本、备份和报告均仅在 Saved，不是产品依赖。未提交／推送。

下一入口：用户保存关闭 Editor／Live Coding 后，执行统一入口增量构建，再运行 `HAL.FutureCreation.GameFlow.UIIntegration`。用实际 MatchFlow 检查 Results 前显示 WAITING FOR RESULT、地图／人数文字隐藏、0.3 秒后切结果；下一局 Map Loading 与玩家人数显示恢复，旧布局回退保持。原生夹具不能替代实际画面验收。

<a id="match-resource-scope-revision-20261126"></a>

## 2026-11-26：Match 专属资源预载（已实施，复用既有 GameInstance）

本节取代下方已撤回的独立类／全局开关方案。用户保存并关闭 Editor 后实施；基线 `main / origin/main = 9e55a7f`，实际引擎 **UE 5.8.3 CL58210709**。本次预载覆盖 **Map Loading 后的人数 Loading、Countdown、正式比赛／观战、参赛车辆与技能、球／机关表现、Results**。PreMatchFlow 的入口、选车、练习及独立预览不加入预载，也不修改其资产。

### 实际行为与复用

1. 既有 Practice → Match 的 PreClientTravel 开始本局异步资源请求，同时复用 GameInstance 的 Map Loading 遮罩。引擎照常加载目标地图；加载完成后先不撤遮罩。
2. 复用 Controller 每 0.1 秒的 PollRoundReadiness：等待资源包、目标地图当前子关卡／World Partition 请求、普通 Texture2D 初始化及可用的最高常规 Mip 就绪。所有纹理请求并行推进，不用固定等待时间冒充加载完成。
3. 本地完成才撤 Map Loading，交给现有玩家人数 Loading，并允许原有 ServerRoundLoaded 上报；服务器仍等所有参与端就绪，再进入 Countdown → Playing。没有新增 RPC、Tick 或比赛阶段。
4. GameInstance 持有本局资源强引用、纹理常驻／忽略流式 Mip Bias 请求，直到 Results 结束后的返房旅行、离开／失败恢复或 Shutdown。退出时撤销自己的请求并恢复原有标志；同进程多个 PIE GameInstance 对共享纹理的请求按计数释放。
5. 必需资源缺失时记录具体路径／车型错误，保持 Map Loading，不上报就绪；恢复仍使用原有 LoadTimeoutSeconds（默认 45 秒）及旅行失败路径。

资源根只取已配置 Room 模板中的 **bMatchFlowContainer 子容器**、既有转场 Loading、本局成员的 PlayerState 类／参赛车型和 HUD 图片、目标地图 Actor／组件，以及现有球与表现配置。沿这些根读取已加载的硬引用、蓝图 CDO／组件模板／WidgetTree 和脚本中的资产引用；没有 Asset Registry 全目录扫描，也没有以整个 GameInstance、Room 或全部 Catalog 作为递归根。Catalog 只用于查本局 VehicleId；未参赛车型和纯预览不加载。**若某参赛车型没有 Icon，现有 Match HUD 会使用 PreviewImage，因此仅这一实际用于比赛的备用图片会被纳入。** HUDColorSource 仅用于已烘焙颜色元数据判断，不为此加载源模型。

### 实际文件范围

| 既有文件 | 本轮改动与必要性 | 复用点 |
|---|---|---|
| Source/HAL_Future_Creation/HALNetworkGameInstance.h | 本局资源列表、异步句柄、引用／纹理常驻状态及窄 C++ 就绪接口；跨图与 Results 期间必须有持有者 | 既有 GameInstance 生命周期；不增加 Blueprint API |
| Source/HAL_Future_Creation/HALNetworkPresentation.cpp | 开始／收集／检查／释放本局资源，保持 Map Loading 到就绪 | HideTravelWidgets、BeginTravelPresentation、Engine AssetManager／StreamableHandle／纹理 API |
| Source/HAL_Future_Creation/HALNetworkGameInstance.cpp | 地图完成后按资源状态交接遮罩，离开及 Shutdown 清理 | ShowPracticeRoom、ReturnToEntry、LoadComplete、Shutdown |
| Source/HAL_Future_Creation/HALPlayerControllerRound.cpp | 既有轮询增加一处就绪检查，不满足时不发原有就绪 RPC | RoundReadyTimer、ServerRoundLoaded 和全员屏障 |
| Source/HAL_Future_Creation/Tests/HALWidgetExtensionTestAccess.h | 测试读取／配置本局资源状态 | 既有 friend 访问器 |
| Source/HAL_Future_Creation/Tests/HALWidgetExtensionTests.cpp | 新增 3 项资源生命周期／范围／常驻测试 | 原有 UIIntegration 夹具；从实际 MatchFlow 验证 Countdown 背景可达 |
| 本文、Docs/Project_Status_CN.md | 实施、证据与边界 | 既有交接文档 |

**本次预载：4 个既有生产文件、2 个既有测试文件、2 个既有文档；0 个新生产源码文件、0 个独立类、0 个模块依赖。** 必要扩展是本局资源的持有、检查与释放；已有 Loading 显示和全员屏障不能替代这些状态，因此把它们放入已有跨图生命周期，而不另建管理器。

上一版未提交的 HALMatchResourcePreloader.h/.cpp 已删除。以下 **5 份文件保持 origin/main**：Config/DefaultEngine.ini、HALPlayerController.cpp/.h、HAL_Future_Creation.Build.cs、Tests/HALRoundTravelTests.cpp。HALPlayerControllerRound.cpp 撤回旧版后仅重做上述一处接入。PreMatchFlow 资产与云端无差异；此前授权的 DetailText 清理仍在 HALGameFlowWidgets.cpp、Editor/HALGameFlowUIMigration.cpp，另计。用户已修改的 Countdown、新建的 T_Countdown_BG 均保留；本轮未通过 Editor 修改、编译或保存任何资产。

### 本轮验证证据

- 统一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup`：最终 Editor R6 **成功，exit 0，4 actions，UBT 20.04 秒／调用 20.39 秒**；证据 `Saved/BuildTool/BuildAndLaunch_20261126_041938_528_5cd5e9`。R2 曾因 const API 调用失败、R5 曾因迭代器标志名失败，均已修正；其他轮次及日志保留。关联文件重编仍有既有数值转换／弃用警告。
- 最终 NativeR3 **UIIntegration 13/13，0 警告／失败／未运行**：含原有界面回归及新增的纹理共享持有／恢复、仅参赛车型和 HUD 回退图、缺失车型不放行、Map Loading 持续／交接、取消及第二局。从真实 MatchFlow 类出发即可找到并持有 T_Countdown_BG，没有把它另列为测试预载根。首轮测试夹具的 viewport 生命周期问题已修复；NativeR1 的 12/13 失败报告保留。
- Game Target R1 **成功，exit 0，18 actions，UBT 275.43 秒**；为验证 WITH_EDITOR=0 下的运行时依赖，额外直接构建 `HAL_Future_Creation Win64 Development`，没有另建 Editor 编译入口。完整命令：`& 'D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' HAL_Future_Creation Win64 Development '-Project=D:\UE Project\HAL_Future_Creation\HAL_Future_Creation.uproject' -WaitMutex -NoHotReload -NoLiveCoding '-Log=D:\UE Project\HAL_Future_Creation\Saved\MatchPreload\20261126\GameBuildR1.log'`。未进行 Game 二进制启动或打包验证。
- NativeGF4_R1 **3/3，0 警告／失败／未运行**，保留原有旅行配置、身份／重复上报、全员屏障与倒计时测试。最终 RHI_UI_R2 **D3D12 1/1，0 警告／失败／未运行**：从 MatchFlow 发现实际 Countdown 背景，两局均通过纹理初始化／Mip 检查并完成遮罩交接（3 张纹理，RenderChecks=1）。
- 最终 **NetworkRHI_R2 主客各 1/1、exit 0，零测试警告／失败／未运行／进行中**，连续两局经过 Map Loading、Countdown、Playing、技能检查、Results／返房，再退出入口。两端每局 25 个显式根、68 张纹理（RenderChecks=1），日志中两端 ready 均早于服务器 all loaded 和 Playing；第 1 局主机／客户端约 6.80／5.48 秒、第 2 局约 6.34／4.50 秒，从发起预载到本地 ready，包含旅行和资源检查，不代表纯 IO 耗时。客户端保留既有 2 秒就绪延迟，断言与 180／240 秒截止不变。为适配本机双进程 iGPU，采用 D3D12 离屏 640×360、ScreenPercentage 50、ViewDistance／Shadow／PostProcess／Effects 质量 0，并关闭动态 GI／反射方法；没有修改 TextureQuality 或项目配置。本结果证明该环境下的加载屏障与往返，不作为默认画质性能或真人视觉验收。
- 中间版 NetworkNullR1：主客各 **1/1**，连续两局身份／技能／往返通过；各端每局 25 个显式根、66 张纹理，资源 ready 后才出现 GF4 all loaded／Playing。NullRHI 不检查渲染资源；最终版本已由上面的 NetworkRHI_R2 复测。
- 中间版 RHI_UI_R1：D3D12 单项 **1/1**，两局 Texture2D 初始化／Mip 检查通过（RenderChecks=1）。NetworkRHI_R1 在 1280×720 双进程时客户端等待 ≥10 FPS 达 117 秒，主机完成第一局后达到原有 180 秒测试截止，脚本随后 240 秒超时清理自己的进程；主机报告失败，不能作为双端通过证据。未放宽断言／超时，最终结果以上述降低离屏渲染负担的 NetworkRHI_R2 为准。

本轮证据目录为 `Saved/MatchPreload/20261126`（不纳入 Git）。资产保护清单为 BeforeAssets.json，交付核对 56/56 份 UI／GameInstance 资产 SHA256 保持，云端保留文件与 PreMatchFlow 差异为空，git diff --check 通过。未提交或推送。

复测入口：统一编译后，在 Automation 中运行 `HAL.FutureCreation.GameFlow.UIIntegration` 和 `HAL.FutureCreation.GameFlow.GF4`。原有双端 NullRHI 入口是 `Tools/TestGameFlowLoop.ps1 -Profile Development -EngineRoot 'D:\Program Files\Epic Games\UE_5.8' -Port <空闲端口> -ReportDirectory <新的绝对目录>`。本轮 D3D12 复用同一脚本逻辑，仅在 Saved 中生成启动副本：将 `-nullrhi` 替换为 `-RenderOffscreen -d3d12 -windowed -ResX=640 -ResY=360`，在双方原 ExecCmds 的 Automation 前加入 `sg.ViewDistanceQuality 0,sg.ShadowQuality 0,sg.PostProcessQuality 0,sg.EffectsQuality 0,r.DynamicGlobalIlluminationMethod 0,r.ReflectionMethod 0,r.ScreenPercentage 50`，其余测试与退出／清理逻辑保持。Saved 启动器不属于交付源码。

### 验收边界与下一入口

包加载和普通 Texture2D 的常规 Mip 就绪，**不等于 Shader／PSO、Niagara 模拟、Nanite／虚拟纹理所有分页均已预热**。目标 World 当前已经请求的流式单元被等待，未来尚未请求的分区不因此全量载入。以后代码拼出的新路径仍须接入现有配置根；Cook 剔除的分辨率无法恢复。保持本局纹理到 Results 会增加显存，真实设备仍需记录加载时间和峰值；本轮不更改全局 r.TextureStreaming。

团队下一步：从 NetEntry 一主一客冷启动，检查 Map Loading → 人数 Loading → 3、2、1 的首次背景与放大动画、技能／特殊球首次使用、观战、Results 长时间停留、手动返房和第二局；检查较慢客户端与缺失资源的超时恢复。四机真人、目标设备画面／显存、Cook／打包与 PSO／虚拟几何专项均须分别验收，不能把离屏自动化写成人工画面通过。

<a id="match-resource-loading-20261126"></a>

## 2026-11-26：Loading 等待比赛资源的第一版（历史，已全部撤回）

**本节仅保留撤回前的实现和验证历史；当前状态以上方修正版范围为准，下面的独立类、全局纹理开关及配套代码已不存在。**

用户确认 Countdown 背景只有首次出现模糊，随后变清晰，并要求资源未准备好时继续显示 Loading，避免进入比赛后才看到美术资源逐渐清晰。接续 `main/9e55a7f`；保留此前 DetailText 清理、团队已修改的 Countdown 及新建的 T_Countdown_BG。本轮不编辑或保存任何蓝图／纹理／地图。

### 已撤回的实现（历史）

- `Config/DefaultEngine.ini` 在 RendererSettings 设置 `r.TextureStreaming=False`，关闭整个项目的普通贴图 Mip 流式加载；需要重新启动进程。按平台 LOD／Cook 设置加载完整可用分辨率，不会恢复已经被 Cook 剔除的分辨率。
- 新增窄本地对象 `UHALMatchResourcePreloader`，使用现有 Controller 的 0.1 秒就绪轮询，不新增 Tick、比赛阶段、RPC、插件或新的服务器规则。
- `PrepareRoundResources` 先等待当前子关卡／World Partition 请求及可见性变更完成；以当前地图、已加载的持久化子关卡、Controller／Pawn／GameInstance 类和车型目录为根，补入现有 MatchBall、BallComicFX、ImpactBurst、VehicleDoodleStatus 和 VehicleStatusFX 配置中的表现资源。
- 使用 Asset Registry 的 Game 依赖闭包收集运行时硬／软引用，异步加载非地图资产。跳过未参与当前加载的其它地图和 EditorOnly 依赖；不是预载整个 Content 目录。配置缺失、Cook／注册表中缺失或加载失败时，不上报就绪并输出一次具体资源错误，随后沿用服务器现有 LoadTimeoutSeconds 返房恢复。
- 加载完成后保持资产强引用和加载句柄到旅行／退出；等待后续包加载、Editor 中尚未完成的资产构建、相关 Texture2D 的资源初始化及 RHI 线程命令栅栏，再调用原有 ServerRoundLoaded。LoadToken 改变、PreClientTravel 和 EndPlay 清理旧请求及本地缓存，下一局重新准备。
- 服务器继续通过原有 AllClientsLoaded → Countdown → Playing 路径判定；一端资源未就绪，全员继续 Loading。加载超时仍包含从发起旅行以来的总时间，没有用固定延迟假装加载完成。
- Runtime 模块增加标准引擎 AssetRegistry／RenderCore 依赖，Editor 专用 RHI 测试依赖保留。新增 `GF4.ResourcePreloadLifecycle` 和 `GF4.ResourcePreloadMissingPackage`，覆盖实际纹理加载、重置／重入与缺失包不放行；当前仅写入测试源码。

### 本轮证据与限制

实际引擎 UE **5.8.3 CL58210709**。统一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup` 被前置占用检查阻止，检测到 UnrealEditor PID **47000** 和 LiveCodingConsole PID **26948**，退出 **1**，未进入 C++ 编译。用户明确选择「暂时保持 Editor 打开」，因此未再尝试构建或终止这些进程。已经核对新增使用的 AssetRegistry、StreamableHandle、Texture2D、WorldPartition 和 RenderCommandFence API 声明，`git diff --check` 通过；静态核对不能替代 UHT、编译／链接和运行测试。

**本次未完成 Editor／Game 编译、自动化、Cook、冷启动视觉、双端或显存验证。当前正在运行的 Editor 尚未加载新代码与配置，不得将本记录写作实测通过。** 全分辨率常驻及预载目录内车型会增加显存和加载耗时，是否适合学校设备必须测量。

这个屏障覆盖已保存资产的运行时依赖及上述现有 Config 根。以后由代码临时拼出的路径、未登记在依赖中的新资源需要补到根集合；未进入加载范围的未来 World Partition 单元不属于「整个世界已全部载入」。RHI 栅栏确认初始化命令顺序完成，**不等于**全部 Shader／PSO、Niagara 模拟、Nanite／虚拟纹理分页和所有首帧渲染缓存都已预热；不能据此承诺「任何美术资源都绝无加载过程」。普通贴图流式加载问题应与这些独立机制分别验收。

### 接续验证入口

1. 用户保存并关闭 Editor／Live Coding 后，执行 `BuildAndLaunch.cmd -NoLaunch -NoPopup`，先解决实际 UHT／编译／链接结果，再用同一入口启动 Editor，确认控制台 `r.TextureStreaming` 为 **0**。
2. 执行现有自动化前缀 `HAL.FutureCreation.GameFlow.GF4` 与 `HAL.FutureCreation.GameFlow.UIIntegration`。NullRHI 结果只能证明包加载／流程，不能证明图片第一次显示清晰；真实 RHI 需另验。
3. 用正式地图一主一客冷启动。日志应先出现各端 `Match resources loading`、`Match resources ready`，再出现服务器 `GF4 all loaded` 和 `GF4 Playing`。重点测试较慢客户端、Countdown 背景首次出现、技能／特殊球首次使用、手动返房与第二局。
4. 在目标设备记录加载时间和显存峰值，执行 Windows Cook／打包后的依赖检查；缺失资源或无法完成加载应停留 Loading 后恢复返房，不得提前放行。另列 Nanite／虚拟纹理／PSO 的检查，未测项目保持待验收。

<a id="countdown-detailtext-removal-20261126"></a>

## 2026-11-26：移除 Countdown 对 DetailText 的依赖

从 `main/9e55a7f` 接续，用户要求删除 Countdown 页中控制 DetailText 的全部 C++ 内容，再由用户删除 UI 组件。开工时 WBP_GF_Countdown 已有用户未提交改动，本轮不编辑、编译或保存任何 WBP。

- `HALGameFlowWidgets.cpp`：Countdown 分支不再生成 GET READY 文案；`FinishView` 在 Countdown 下只写入 TitleText，跳过 DetailText 的控件查找及文本更新。TitleText 的向上取整、View 数据链、服务器起止时间及其它页面的 DetailText 行为保持。
- `HALGameFlowUIMigration.cpp`：旧 UI 生成工具的 Countdown 定义不再提供副标题，不再生成／设置 DetailText，也不再将其作为 Countdown 的必要控件；其它页面继续使用原有副标题。
- 代码已修改；本机实际引擎为 UE **5.8.3-58210709**。执行统一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup`：沙箱首轮停在 Build.bat 临时锁阶段，已停止该次尝试；随后沙箱外重试被前置检查阻止，检测到 UnrealEditor PID 16120 与 LiveCodingConsole PID 22860，退出 1，未进入 C++ 编译，未关闭用户进程。
- 上述是清理发生时的验证限制；接续本次资源预载施工，Editor R6 与 Game Target 均已成功编译，UIIntegration 13/13 通过。未由代理编辑／保存 WBP，数字放大与实际布局仍由团队在 UI 删除 DetailText 后编译保存并检查，另确认 Loading 人数文字正常。

**最新实现（2026-11-25，本机验证通过）**：已实现完整排名与并列 WINNER、单人测试结束必胜，以及 MatchFlow 现有 Loading 独占 0.3 秒后切 Results。Editor 编译通过，原生／实际 WBP 自动化 19/19、有 RHI 离屏检查 1/1；80 ms 注入的双端三局复测主客各 1/1，通过最小重新挂载分支修复了客户端旅行后 Room 脱离视口的问题。实际改动 13 个现有文件，保留团队布局、Loading 和 ResultRow。详见[实施、蓝图差异和证据](#results-ranking-implementation-20261125)；[施工前方案](#results-ranking-transition-20261125)作为历史保留。四机真人混战尚待团队验收。

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

<a id="results-ranking-transition-20261125"></a>

## 2026-11-25：胜者规则、单人测试特例与 Results 前 0.3 秒过渡

本节为用户授权施工前确认的规则和最小范围估计，覆盖此前关于“无胜者”的建议。接续基线为本机 `main/abaa591`、UE 5.8.3-58210709。当时仅留档，随后用户授权按方案施工并再次明确允许修改、编译保存 MatchFlow 和 Results；实际完成程度及新验证以[实施记录](#results-ranking-implementation-20261125)为准。

### 用户已确认的行为

1. 正常多人比赛不提供“无胜者”结算。时间尚未结束而只剩一名玩家存活时，该玩家获胜；时间结束仍有多名玩家存活时，按存活玩家剩余绝对 HP 排名，HP 更多者获胜，最高 HP 相同者并列第一且全部显示 WINNER。若截止时只剩一人，该玩家仍为胜者。
2. 开局只有一名参赛玩家的模式是临时单人测试模式；在该局结束时，无论该玩家存活还是死亡，都显示其为胜者。该测试模式／入口后期关闭，不作为正式多人规则。实现时按服务器开局锁定人数及现有 Development 配置识别，不能把正常多人局只剩一名玩家的状态误认成单人测试模式。本条不额外规定单人死亡后是否继续等待计时截止。
3. 结算过渡复用 `WBP_GF_MatchFlow` 中现有的 `WBP_GF_Loading` 实例，独占显示 0.3 秒，然后切到 `WBP_GF_Results`。这里不使用 GameInstance 的地图转场实例，也不创建新的 Loading WBP。
4. 服务器先完成排名和不可变结果快照，随后沿现有复制／View 链路下发。各端收到完整结果后本地显示上述过渡；0.3 秒不作为服务器继续计算、延迟判胜或等待所有客户端确认的时间。规则阶段保持 Playing → Results，界面内部为 MatchHUD → Loading（0.3 秒）→ Results。
5. Results 只接收和展示快照；Host 手动 Return 路径继续复用。任何过渡中的返房、离开、销毁或跨局都应取消旧回调，重复 View 不重复启动该局过渡。
6. 多人同时归零允许并列名次，包括并列第二、第三或第一。若最后一批仍存活玩家同时归零而结束比赛，该批玩家并列第一并共同显示 WINNER，可有 2、3 或 4 名 WINNER；更早淘汰者不因最终都为 0 HP 而一并成为胜者。比赛中途某批玩家同时归零但仍有其他存活者时，该批玩家并列相应的非第一名，不提前结束比赛或授予 WINNER。

### 并列显示的最小实现约定

- 同一名次允许多条结果行；按排序列表的位置分配四个控件，行内文字读取 FinalRank，不能按 FinalRank 作为控件下标，否则并列玩家会互相覆盖。
- 本方案采用标准竞赛名次编号：两人并列第一时为 1、1、3、4，两人并列第二时为 1、2、2、4；这是本方案的显示编号约定。相同名次内可按已有 Number 稳定排列，Number 不作为打破并列的胜负条件。
- 时间结束 HP 比较沿用现有绝对 HP 与最高值的相等容差口径；淘汰并列由服务器同一评估批次记录判断，不根据客户端收到通知的先后排序。
- 行状态与共用结果摘要都应优先展示 bWinner；即使 Life 为 Eliminated，最终同批归零的并列胜者仍显示 WINNER。单人测试死亡后获胜也适用此优先级。不会为了产生胜者而复活玩家或改写冻结 HP。

### 最小修改估计

更新后的最低范围为 **12 个现有文件：5 个生产 C++、2 个 WBP、3 个测试 C++、2 个文档**。前提是 `ResultRow.SetResultData` 可以复用，且短过渡计时保持为 MatchFlow 的轻量本地表现逻辑；若行接口需要补充，预计再改 1 个 WBP，合计 13 个。相比前一轮 11 个文件，多计入 HALPracticeRoomWidget.cpp：当前共用结果摘要先把 Eliminated 玩家标成 OUT，再判断 bWinner，会误标同时归零的共同胜者及死亡单人测试胜者，需要一并修正。此处是文件范围估计，不代表已经施工。

| 类别 | 文件 | 预计改动 |
|---|---|---|
| 生产 C++ | Source/HAL_Future_Creation/HALRoundMatchTypes.h | 增加最终名次、淘汰批次、单人测试标识与排序结果快照等必要字段；保留原 Players 顺序。若区分同时结束的结算原因，兼容扩展现有枚举，不重排旧值。 |
| 生产 C++ | Source/HAL_Future_Creation/HALRoundMatchTypes.cpp | 在现有 Decide 路径计算完整排名，支持最高 HP 同分与最后同批归零的并列胜者；实现单人测试结束必胜；正常结算不再产出无胜者结果。 |
| 生产 C++ | Source/HAL_Future_Creation/HALPracticeRoomMatch.cpp | 按开局配置识别单人测试，记录淘汰／退出批次，首次发布完整结果前计算并冻结快照，每局重新初始化。 |
| 生产 C++ | Source/HAL_Future_Creation/HALGameFlowWidgets.cpp | Loading 在 Results 过渡期间隐藏开赛人数显示，复用通用 Loading 视觉；保留开赛地图／人数显示。 |
| 生产 C++ | Source/HAL_Future_Creation/HALPracticeRoomWidget.cpp | MatchResultText 优先展示胜者身份，正确汇总多个 WINNER，并调整同时结束等结果文案，避免将已淘汰的共同胜者或死亡测试胜者标成 OUT。 |
| WBP | Content/UI/GameFlow/Screens/MatchFlow/WBP_GF_MatchFlow.uasset | 在现有 ApplyView／ShowFlowScreen 路径加入按局一次的 0.3 秒本地表现定时器、阶段检查和取消处理。 |
| WBP | Content/UI/GameFlow/Screens/MatchFlow/WBP_GF_Results.uasset | 从完整排序结果快照更新四行，名次与玩家编号分开，清空未参赛行。 |
| 测试 C++ | Source/HAL_Future_Creation/Tests/HALMatchLoopTests.cpp | 更新旧无胜者断言，覆盖 HP 并列、最后同批共同胜者、中途同批非胜者、单人存活和死亡、排名冻结及跨局清理。 |
| 测试 C++ | Source/HAL_Future_Creation/Tests/HALMatchLoopNetworkTests.cpp | 更新旧全灭无胜者场景，验证主客端完整排名、多个胜者及冻结快照一致。 |
| 测试 C++ | Source/HAL_Future_Creation/Tests/HALWidgetExtensionTests.cpp | 扩展结算用途 Loading 与正常 Loading 的显示恢复、共用结果摘要 WINNER 优先级验证；实际 WBP 计时、独占显示及跨局取消另做有渲染主客端验证。 |
| 文档 | Docs/Presentation/Match_Countdown_HUD_CN.md | 保存当前规则、临时单人模式关闭事项、施工与验证记录。 |
| 文档 | Docs/Project_Status_CN.md | 更新接续入口、完成程度、验证证据和未定边界。 |

按需扩展的行控件为 `Content/UI/GameFlow/Components/WBP_GF_ResultRow.uasset`。最低方案不新建文件、管理类、RPC 或比赛阶段；沿用 Room.Data.Match 复制、View.Match、ShowFlowScreen、现有 Results 独占显示及 Return 路径。历史枚举／兼容分支的清理不作为本次最低范围的前提，不通过重排旧枚举值改变已保存资产含义。

### 施工前差异与估计边界（历史，后续实施覆盖）

- 当前 `HALRoundMatch::Decide()` 仍在全部淘汰时输出无胜者，现有原生／联机测试也要求该行为；单人死亡局当前也不满足“结束必胜”，共用摘要还会把死亡胜者标成 OUT。这些是本次计划要修改的旧行为，不能把当前代码或历史测试描述成已经符合新规则。
- 原最高 HP 同分、最后多名玩家同批归零的决胜问题已由本次用户指令解决：允许并列第一和多个 WINNER，不再作为待定项。中途同批淘汰可并列第二或第三。
- 不同淘汰批次之间的完整名次及退赛排序沿用此前的方案建议，尚未被本次并列指令单独定稿；不能将建议擅自标成用户已确认的正式规则。
- 最低文件数量按在上述现有结算入口内完成决胜与排名估算。若后续规则要求改造伤害执行或增加网络确认，应重新评估范围。
- 本轮只完成规则留档与范围估算。施工前复核 Blueprint 实际接口；当前 Loading、Results 有用户未提交修改，ResultRow 为未跟踪资产，保留这些内容。后续编辑已有改动 Blueprint 时按当前 AGENTS.md 第 8.3 节先确认具体改动，再通过运行中的 Editor 修改、编译和保存。

<a id="results-ranking-implementation-20261125"></a>

## 2026-11-25：排名快照与 0.3 秒过渡实施完成

本轮从 `main/abaa591` 接续。开工已有团队修改的 Loading、Results、两份方案文档及未跟踪的 ResultRow。用户批准按方案施工，并在读取现有图表后明确允许修改、编译保存 MatchFlow 和 Results。所有 WBP 修改均通过运行中的 Unreal Editor Python／BlueprintGraphEditor API 完成，没有直接改写 uasset 二进制。

### 已实现的规则与数据链路

- FHALRoundParticipant 增加 EliminationBatch、FinalRank；FHALRoundMatchData 增加 LastEliminationBatch、bSinglePlayerTest、RankedPlayers。服务器同一次评估中归零的玩家记录相同淘汰批次；退赛单独记录并排在正常存活／淘汰者之后。
- 正常胜负继续在 HALRoundMatch::Decide 判断：唯一存活者获胜；超时按存活者绝对 HP 排名，沿用 0.001 HP 的同分容差；最后同批淘汰者共同获胜，更早淘汰者不因此获胜。排名采用 1、1、3、4 等竞赛编号，后淘汰者在前，相同名次内以玩家编号稳定显示。比较器先严格排序，容差按组首比较，避免将非传递的近似相等直接作为排序比较器。
- 单人特例仅在服务器开局确认为 Development 且锁定人数为 1 时启用。存活的单人不在开局立即获胜，仍可等到时间结束；死亡时沿原测试结束路径结算为 WINNER，保持 HP=0、Life=Eliminated，不复活或改血。后期必须关闭此临时单人测试入口／特例。
- SharedVictory、SoloTest 原因追加在旧枚举末尾，保留旧序号。正常完成不再生成旧的无胜者结果；全部退出且无正常参赛结果的异常情形使用已有 Aborted 取消语义，不向退赛者授予胜者。
- 原 Players 保持开局身份／座位顺序；只在首次完成结算时生成 RankedPlayers，并在发布 Results 前随同结果一次性冻结。原有 Room.Data.Match 复制、View.Match 转发、结束后阻止伤害／输入及 Host 手动 Return 路径继续复用。
- 共用 MatchResultText 优先检查 bWinner，支持多个 WINNER；死亡的共同胜者／单人测试胜者不会再被摘要标成 OUT。

### 两份 WBP 的实际差异

| 资产 | 修改前 | 修改后 |
|---|---|---|
| WBP_GF_Results | 既有四行循环读取 Match.Players；SetResultData 的 Rank 输入默认 0。 | 保留原循环、Is Valid Index、SetResultData、ClearResultData；将两处数组数据源改为 RankedPlayers，并连接 Participant.FinalRank → Rank。布局和行控件接口不变。 |
| WBP_GF_MatchFlow | Results 阶段立即 ShowFlowScreen(ResultsWidget)。 | Results 首次到达时显示现有 LoadingWidget，单次非循环定时 0.3 秒，再显示现有 ResultsWidget；重复 View 不重新计时，同局完成后不重播。 |

MatchFlow 新增本地表现用 ResultTransitionRound、ResultTransitionPending、ResultTransitionTimer，以及 ApplyResultsTransition、FinishResultsTransition、CancelResultsTransition、ResetResultsTransitionForView 辅助函数。结束回调检查当前阶段、局号和完成快照；非 Results、SetScreenActive(false)、Destruct 取消挂起回调。继续调用原 ShowFlowScreen 处理 View 转发、激活与焦点。界面计时不控制服务器何时结束／结算比赛，不添加 Tick 或 RPC。

Loading 在 Results 过渡时复用原 MapLoadingPanel，隐藏开赛人数部分；普通地图／人数用途及团队的现有文字、美术保持。当前通用部分的团队文案仍为 MAP LOADING，未修改 Loading 资产。ResultRow 已包含名称、车型、图标、胜者优先级与清空逻辑，本轮直接复用；图标 Match Size=false，未改尺寸或字体。

两份资产均编译、保存成功，并在独立 Editor 进程中重新加载编译、检查连线。以开工磁盘版本为基线，30 份 UI 资产仅 MatchFlow、Results 两份变化；其余 28 份（包括团队已改 Loading、未跟踪 ResultRow）SHA-256 一致。报告为本机 Saved/ResultsRanking/20261125/{BeforeAssets.json,BlueprintBefore.json,BlueprintAfter.json,BlueprintVerification.json}。

### 联机复测带出的最小挂载修复

首轮同机双端三局：Host 1/1 成功，Client 0/1 失败。客户端已收到 Results、拥有当前世界 Room 对象，但 Visible=0，复现了 2026-11-21 的旧视口观察问题，首轮不记为通过。

定位到 HALNetworkGameInstance::ShowPracticeRoom：同类、同世界、同拥有者时直接复用 VisibleWidget，没有检查其视口插槽是否被旅行清理。增加唯一分支：控件仍可复用但 !IsInViewport 时 AddToViewport，再沿用现有 RefreshFromRoomState。未新增页面、管理器、计时器、RPC 或 Room Blueprint 改动。客户端复测出现 Mounted=1，三个 Results 均 Visible=1，第二、第三局及返房继续通过。

因此最终修改 **13 个现有文件：6 个生产 C++、2 个 WBP、3 个测试 C++、2 个文档**。相比施工前最低估计仅多计入 HALNetworkGameInstance.cpp；ResultRow 无需修改，没有新建交付文件。

### 本次编译、自动化与画面证据

- 统一入口均为 `BuildAndLaunch.cmd -NoLaunch -NoPopup`，UE 5.8.3-58210709，HAL_Future_CreationEditor／Win64／Development，保留 -WaitMutex -NoHotReload -NoLiveCoding。首轮完整增量 24/24 成功；最后包含挂载修复的增量 5/5 成功，100.41 秒、退出 0，记录 Saved/BuildTool/BuildAndLaunch_20261125_230223_532_4b98d8/Result.json。未改文件的既有 C4305 数值转换警告保留，不描述为整个构建零警告。
- 最终 DLL 的 HAL.FutureCreation.GameFlow.GF7＋UIIntegration：**19/19 成功，0 测试警告、0 失败／未运行／进行中**。报告 Saved/ResultsRanking/20261125/NativeR2/index.json。覆盖纯规则排名、真实四人内存局的同批淘汰、HP 容差、单人存活／死亡、摘要 WINNER、退赛、冻结、现有 Return、加载与页面生命周期。
- 新增 ResultsRankingBlueprints 用例加载实际 MatchFlow／Results／ResultRow，验证 0.29 秒仍显示 Loading、0.32 秒显示 Results，重复 View 不重启；离开 Results 和停用容器会取消回调；下一局单人死亡为 WINNER，旧四行其余位置被清空。上述时间为可控游戏定时器推进边界，不宣称显示器精确刷新到毫秒。
- 同一实际 WBP 用例在有 RHI 离屏渲染下另跑 **1/1 成功、0 测试警告**，报告 Saved/ResultsRanking/20261125/NativeRender/index.json。实拍 Loading.png、SharedWinners.png、SoloWinner.png 已检查，分别显示独立加载页、TOP 1／1／3／4 与两个 WINNER、死亡单人 TOP 1／WINNER。此夹具没有注入 Vehicles 目录，图标为原行控件的白色占位、车型为破折号，不当作真实车型图标加载验证；不等同完整多人比赛渲染。
- 80 ms PktLag 注入的真实 Listen Server／Client 三局最终复测：**Host 1/1、Client 1/1 成功，均 0 测试警告／失败／未运行／进行中，进程退出 0**。三局分别为唯一存活者、时间结束 HP 并列、最后同批归零共同获胜；比较 RankedPlayers 与原身份快照、顺序／名次／胜者、权限、超出旧期限后的 Host 手动 Return 和下一局。日志确认双方 PktLag=80，这是模拟注入值，不是实测 RTT。最终报告 Saved/ResultsRanking/20261125/NetworkR2/{Summary.json,Host/index.json,Client/index.json}；NetworkR1 保留首轮失败证据，未削弱既有视口断言。
- 首次图表脚本在保存前遇到接口覆写不生成 Parent 节点的 API 差异，修正后才编译保存；失败日志 Wire_R1_Failed.log 保留。客户端／主机和 Editor 测试进程均已结束。Saved 下脚本和报告是本机测试留档，不是运行时或跨设备交付依赖。

### 团队接续与限制

本轮未提交或推送。保留团队的 Loading 修改和未跟踪 ResultRow；后续正常提交时需一并纳入团队打算交付的资产。未单独构建 Game Target、Cook 或打包，未做本次四台实机／真实碰撞手感／手柄验收。

下一步团队以同一引擎补丁版本在四台机器连跑两局，观察每端 0.3 秒过渡、并列结果、真实玩家名与车型图标、Host Return 后重新开赛及本地菜单焦点。临时单人测试模式的后期关闭事项继续保留。

<a id="playerstatus-client-catalog-20261126"></a>
## 2026-11-26：PlayerStatus 仅 Host 显示的诊断与最小修复

诊断起点为干净 `main/45c7de6`，本机 UE 5.8.3-58210709。通过 Unreal Python 只读导出实际 MatchHUD、MatchFlow、PlayerStatusRow 和 Room，再以正式 BP_MVPB_PracticeRoomGameMode 运行本机 Listen Server／Client 三局并采样实际 Widget。下列先保留修复前诊断；该阶段没有保存或修改蓝图、修改生产源码或新编译。用户随后授权的最小修复与本次新验证见本节末尾。

### 已观察到的直接原因

- 每局 Playing 时双方 `Match.Players.Num()` 都是 2，实际 WBP_GF_MatchHUD 均为 HitTestInvisible，因此玩家数据已到客户端、HUD 本体也已创建。
- Host 的 Room／MatchHUD `LoadedCatalog` 有效，PlayerRow01／02 为 Visible，未参赛两行为 Collapsed。
- Client 的 Room／MatchHUD `LoadedCatalog` 为 null，四行全部 Collapsed；同一客户端 Room 实例在旅行前 Locked 时目录有效，旅行后的 Loading／Countdown／Playing 均为空。
- 实际 `RefreshPlayers` 先把四行设为 Collapsed，再经 `Is Valid(LoadedCatalog)` 才执行 Players 遍历和 SetPlayerData。图标目录的缺失因此阻断整行编号／HP／图标显示。没有发现 Host／Authority 条件限制该刷新链。

### 源码中的缓存恢复缺口

`HALPracticeRoomPresentation.cpp:95` 的 NativeDestruct 会取消目录／图片加载并清空 LoadedCatalog、LoadedPreview，但保留 CatalogPath、ImagePath。客户端无缝旅行后，同一 Room 可能重新 AddToViewport；NativeConstruct 虽再次 RefreshPresentation，但该函数第 48 行只在路径变化时启动目录加载。路径相同而缓存已清空时不再加载，后续定时刷新仍走相同路径，空目录持续传给 HUD。图片路径缓存存在同类生命周期缺口。

诊断确定应在 C++ 中恢复重构／重挂载后的目录和图片缓存，避免只依赖路径改变。玩家编号和 HP 是否需要摆脱图标目录门槛可作为独立表现改进；本次授权只实施缓存生命周期修复，保留蓝图现有刷新链。

### 修复前诊断证据

- 当前只读导出与采样目录：`Saved/MatchHUD/Review_20261126_Client`。BlueprintCurrent.json 和四份 T3D 为本次实际资产接线；四份蓝图导出前后及最终运行后 SHA-256 均保持。
- 第一轮动态探针误用 Python 类名 WidgetBlueprintLibrary，读取失败；改为引擎声明的 ScriptName WidgetLibrary 后，R2 成功采样。第一轮规则测试通过不作为玩家行证据。
- R2 使用正式蓝图 GameMode，`HAL.NetworkSmoke.GameFlow.GF7.ThreeRoundMatch` 主客各 1/1 通过，报告 `R2/{HostReport,ClientReport}/index.json`，两个专用进程退出 0。此测试覆盖三局规则／结果／返房，**没有验证 PlayerStatus 行显隐，故通过与本缺陷并不矛盾**。
- R2/Client.log 的 1525 行显示 Room_C_1 在旅行前 LoadedCatalog 有效，1579／1585／1588 行显示同实例旅行后为空且 Playing 已有 2 名玩家；1777／1965 行显示第二、第三局仍复现。R2/Host.log 的 1603 行为首局 Host 两行正常的对应样本。探针按状态变化记录，未保存游戏状态或写回资产。
- 诊断阶段为 NullRHI 运行时属性检查，没有渲染截图、四台实机、Game Target／Cook／打包或修复后验收。所有诊断进程已结束；Saved 脚本和日志只作本机证据，不作为交付依赖。

### 已实施的最小修复

用户授权后，只在 `HALPracticeRoomPresentation.cpp:102` 的 NativeDestruct 缓存清理后增加一条说明注释及 `CatalogPath.Reset(); ImagePath.Reset();`。路径与资源同时失效，下一次 NativeConstruct／RefreshPresentation 会重新请求同一路径的目录和图片。现有加载句柄取消、异步回调、Room 视图分发和 HUD 刷新逻辑保持。

最终交付仅一个生产 C++ 文件及本说明、Project_Status 两份文档。不改头文件、MatchHUD／MatchFlow／PlayerStatusRow／Room 蓝图、GameInstance 或资产引用，不新增生产 Tick、RPC、类或依赖。保留此前诊断的两份未提交文档改动，未提交／推送。

### 修复后的本次验证

- 统一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup`，UE 5.8.3 CL58210709，HAL_Future_CreationEditor／Win64／Development，包含 `-WaitMutex -NoHotReload -NoLiveCoding`。沙盒内首次尝试停在构建锁阶段，已停止；沙盒外首次尝试因 UnrealEditor 3076／LiveCodingConsole 40632 占用而退出 1，未编译、未停止用户进程。用户保存关闭后正式增量构建 **5 actions、63.16 秒、退出 0**，记录 `Saved/BuildTool/BuildAndLaunch_20261126_171054_478_0cd5cd/Result.json`。包含既有其他文件 C4305 数值转换警告，不描述为整个构建零警告。
- 新 DLL 的 `HAL.FutureCreation.GameFlow.UIIntegration` **13/13 成功、0 测试警告／失败／未运行／进行中**，进程退出 0。报告 `Saved/MatchHUD/Fix_20261126/Native/Report/index.json`。
- 正式 BP_MVPB_PracticeRoomGameMode 的本机 Listen Server／Client 连续三局：既有 `HAL.NetworkSmoke.GameFlow.GF7.ThreeRoundMatch` **Host 1/1、Client 1/1 成功，均 0 测试警告／失败／未运行／进行中、进程退出 0**。保留唯一胜者、HP 并列、同批归零、Host Return 与下一局的原测试断言。两端日志确认 PktLag=80，是模拟注入值，不是实测 RTT。
- 在同一运行中另以只读 Python 探针采样实际 Room、MatchHUD 和四个玩家行，并通过外部断言确认：**双方第 1／2／3 局均有目录有效且 HUD 已展开的完整正确样本**；Player 01／02 正确，HP 百分比与该 HUD 的玩家快照一致，覆盖 1.0／0.5；两行车型图标资源有效，未参赛 03／04 两行保持 Collapsed。双方返房后的 Room 预览也恢复。未放宽蓝图目录门槛或向 UI 注入替代数据。
- 报告、命令、专用进程 ID、日志、匹配样本和校验结果在 `Saved/MatchHUD/Fix_20261126/{Summary.json,Host,Client}`；三局玩家行证据在 Summary 的 network.Host／Client.matched_samples。采样脚本没有编译或保存蓝图，不作为交付依赖；所有专用测试进程已退出。
- `Content/UI` 与 `Content/Blueprints/Network` 共 **56/56 份资产 SHA-256 保持**，前后清单 BeforeAssets.json／AfterAssets.json，git diff --check 通过。
- 本次仍为 NullRHI 属性／生命周期验证，未做渲染截图或四台真人画面验收，未单独构建 Game Target、Cook 或打包。团队下一步在正常画面下确认客户端实际布局、图标、受伤血条及返房再开赛；原未完成的其他 UI／玩法验收保持原状态。

<a id="input-hints-20261127"></a>
## 2026-11-27：键鼠／Xbox 操作提示 C++ 支持

### 本次范围与职责

从本机 `main/a58bb32`、UE 5.8.3 CL58210709 接续。团队已创建未提交的 `Content/UI/GameFlow/Components/WBP_InputHints.uasset` 与键盘占位贴图；本轮只实现并编译 C++，保留这些资产原样。

- 新增 `UHALInputHintSubsystem`（LocalPlayerSubsystem）：每个本地玩家独立记录最近有效输入，跨旅行保留，通过原生事件通知变化。所有标准游戏手柄暂统一使用 Xbox 图标，不识别其他厂商外观。
- 新增 `UHALInputHintsWidget`：PreMatchFlow 与 MatchFlow 可以分别持有同一 WBP 类的不同实例，共享该玩家状态。构建时立即同步、隐藏时仍更新、销毁时解除订阅；显示面板为 `HitTestInvisible`，另一面板为 `Collapsed`，不取得输入焦点。
- 输入预处理器在 UMG／GameOnly 路由前观察事件，所有处理函数均返回 `false`。仅观察属于本地玩家且焦点／鼠标命中路径位于活动游戏视口中的事件，不消费确认、返回、驾驶等原输入。
- 非重复的键盘／鼠标按钮／滚轮、有效鼠标位移选择键鼠；手柄按钮、有效摇杆／扳机动作选择 Xbox。模拟轴具有激活／释放滞回：同方向持续按住不会在键鼠操作后立即抢回提示，释放后再次推动或反向推动才重新选择 Xbox。
- 使用已有 Slate／UMG 依赖，未启用新插件、修改输入映射、增加服务器复制或 RPC。无新增逐帧轮询；IInputProcessor 必需的 Tick 为空。

### 团队接入与调参

1. 打开 Editor，在 `WBP_InputHints` 的 Class Settings 将 Parent Class 改为 `HALInputHintsWidget`。
2. Designer 保留 `KeyboardPanel`、`XboxPanel` 两个 Panel Widget，名称大小写一致并勾选 Is Variable。C++ 使用 `BindWidget`，缺少任意面板时蓝图编译会提示。当前只读检查确认二者均为 HorizontalBox，前者 Visible、后者 Collapsed；Python 未能读取 Is Variable 标记，仍需团队在 Designer 确认。
3. 编译并保存 WBP，在目标 PreMatchFlow／MatchFlow 页面中嵌入控件，由宿主提供 Owning Local Player。无需 Event Tick、输入监听、手动设备切换接线或单独 AddToViewport。
4. `Input Hints > Preview > Preview Input Device` 只影响 Designer 预览；运行时使用输入服务。没有本地玩家时回退键鼠布局。
5. 图标、文字和布局仍由 Designer 配置。同一 WBP 类的不同实例具有相同默认内容；需要不同内容时可制作轻量 Designer 派生布局。当前不包含运行时 HintContext、改键后自动更新提示、长按进度或动态生成操作列表。

可按需在 DefaultGame.ini 覆盖（本次未修改配置）：

```ini
[/Script/HAL_Future_Creation.HALInputHintSubsystem]
AnalogActivationThreshold=0.25
AnalogReleaseThreshold=0.15
MinimumMouseMovement=2.0
```

前两项为归一化模拟轴幅值（0～1），释放阈值应小于激活阈值；鼠标阈值单位为每次事件的 Slate 屏幕像素。初始显示键鼠；连接手柄本身不切换。手柄拔出后的主动回退未纳入本版，下一次键鼠操作会正常选择键鼠。日志类别 `LogHALInputHints` 仅在 Verbose 等级输出设备变化。

### 本次验证与限制

- 统一 `BuildAndLaunch.cmd -NoLaunch -NoPopup`，Editor／Win64／Development，保持 `-WaitMutex -NoHotReload -NoLiveCoding`。最终增量构建 **17 actions，501.56 秒，退出 0**；记录 `Saved/BuildTool/BuildAndLaunch_20261127_151312_445_b0c0e5/Result.json`。包含既有其他玩法与测试文件的 C4305／C4996 警告，新文件无编译警告。首次沙箱构建停在构建锁阶段后已停止；首次沙箱外构建因 UHT 不接受 `px` 单位标记失败，改为明确显示名称后才成功，失败日志保留。
- 新 DLL、NullRHI、独立测试进程：`HAL.FutureCreation.GameFlow.InputHints` **3/3 成功**，覆盖信号过滤（含持续按住、滞回、反向拨杆、重复按键／鼠标微动）、本地玩家状态隔离、PreMatch／Match 双实例同步、隐藏恢复、重建和销毁解绑。报告 `Saved/InputHints/20261127/InputHints/index.json`。
- 同一 DLL 的既有 `HAL.FutureCreation.GameFlow.UIIntegration` **13/13 成功**，报告 `Saved/InputHints/20261127/UIIntegration/index.json`。两组均 **0 测试警告／错误／失败／未运行／进行中，进程退出 0**，两个进程串行运行。
- Editor Python 只读检查 R3 退出 0，确认两个新原生类可加载、占位 WBP 父类仍为 UserWidget、两套面板名称／类型及默认显隐符合约定。没有重设父类、编译或保存蓝图。报告 `Saved/InputHints/20261127/PlaceholderInspection.json`；R1 路径转义、R2 未暴露的 ParentClass 属性读取失败，均不作为成功证据。
- `Content/UI` **100/100 份资产 SHA-256 保持**（包含用户新建占位 WBP、键盘图标与现有 Xbox 素材），清单为同目录 UIHashesBefore.json／UIHashesAfter.json；git diff --check 通过。
- 本轮自动化是 C++ 状态与控件生命周期夹具，未把用户 WBP 改父类或挂到真实页面；真实 Slate 输入路由、UIOnly／GameOnly 下的键鼠／手柄实际操作、PIE／真人联机、Game Target、Cook 和打包未验收。团队下一步按上段接入，在两种输入模式下确认首次按键仍执行原操作、摇杆静止不闪换、返房／重建后状态正确、主客端各自独立。
- 最终改动为四个生产 `.h/.cpp`、一个测试 `.cpp` 与本说明／Project_Status。未提交或推送；构建、两组测试及只读检查进程均已结束。
