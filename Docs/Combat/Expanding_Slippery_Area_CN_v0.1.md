# 可生成的扩散打滑区域

## 2026-11-24 柱子模型造成調試圓盤傾斜

用戶在Match柱子加回兩個Static Mesh後，回報圓形範圍像斜放的盤子。已保存場景的唯讀取樣確認兩個模型仍掛GeneratorBase、Actor旋轉為0、兩模型都開QueryAndPhysics且不模擬物理；生成中心由位置＋世界Offset計算，生成器仍以Identity旋轉／單位Scale產生獨立打滑Actor。這次針對`DrawGroundDebug`的貼地射線：原本掃WorldStatic／WorldDynamic，只排除打滑Actor與Instigator，未排除Owner生成柱。柱子新增的固定碰撞可被選成地面，圓盤就依其斜面法線顯示，與車輛判定的世界XY圓柱不同。

修正僅在調試貼地Query加入忽略Owner，保留真實地板／斜坡取樣，不移動生成位置、不改打滑／高度／碰撞／動畫規則。沿用`MovementAreas.DebugGroundMeshCheckboxAndColors`加入可重現的25度柱子Box與平地：未排除柱子的對照射線應先撞柱子斜面，修正後整圈所有頂點應貼平地Z=3；再旋轉真正地板10度，確認仍跟隨地板斜度、世界XY半徑維持500cm，Area與Owner姿態保持。

本輪`main/84a748b`、UE **5.8.2-56702186**。統一入口`BuildAndLaunch.cmd -NoLaunch -NoPopup`最終Editor R2編譯成功(exit0，38.95秒／9actions)；首輪僅因Editor／Live Coding占用被前置檢查阻止，未停止用戶程序，用戶保存關閉後重試成功。Native R1 **12/12、0警告／失敗／未執行／進行中**，為MovementAreas 7項＋ArenaGenerator 5項。新增對照射線確實先撞25度柱子斜面；修正後全部圓盤頂點貼平地Z=3，真正10度斜坡與世界XY半徑500cm均通過，原位置與動畫回歸亦通過。只改`HALExpandingSlipperyArea.cpp`調試Query與既有測試，未改BP／地圖／配置／車操作；用戶保存關閉後的**3/3份Match／生成器BP／打滑BP雜湊保持**，保留本輪起始既有Match修改。未提交／Push；未新跑Game Target、全量、新雙端或真人畫面，不沿用11/19或遠端報告。下一步重開Editor，在加了兩模型的柱子生成打滑區時檢查圓盤是否貼齊地板。

證據`Saved/ExpandingSlipperyArea/20261124/GeneratorGround`（Build_Editor_R1／R2、Native_R1／log、ProtectedBeforeTests／AfterTests），詳細構建`Saved/BuildTool/BuildAndLaunch_20261124_153607_298_b61faa`。唯讀場景取樣及初版Python射線無命中回傳None的診斷留在`Saved/GitSync/20261124/InspectTilt*`；該失敗查詢不作實際場景射線命中證據，重現結論來自上述原生幾何測試。

## 2026-11-17 Match 无效果的高度诊断

用户报告柱子生成与关卡预放两种圆形区域均无效果。本轮只读检查，未修改 C++、BP、地图或配置：`main/793f619`，UE5.8.2-56702186，保留全部用户未提交资产。读取保存 BP：Expansion=2s、Hold=5s、Radius=1000cm、HalfHeight=200cm、Grip=0.3、Debug=true；Match 预放区域 Z=-80，实例 Hold=50s；柱子已由用户移动至 Z=-200（此前校准记录的 -190 不再代表当前位置）。

实际游戏日志 `Saved/Logs/HAL_Future_Creation.log` 的 2026.11.17-08.31.46:404 `HALSpawnSlipperyArea` 记录生成位置 `(4172.72,910.56,136.15)`；该命令保持车辆物理根 Z，所以该次车根高度为136.15。预放区域的有效高度为[-280,120]，这个车根高度在范围外；按Base Z=0、Slippery Offset Z=110计算，当前柱子生成中心=-90，上界110，也低于该次车根。**这解释该高度下的漏判，但尚未采样车在两处圆心正上方的落地高度，不宣称两处已完成现场复测。** 调试地面网格只是显示投影，不会移动权威判定中心。

给团队的初次调参：预放区域中心改约Z=136；若柱子Z=-200且Base Z=0，Slippery Area Offset Z改约336（精确对齐日志值336.15）。若柱底Z=-190则约326。其他Base值按`目标Z－柱底Z－Base Z`换算；保留各效果独立设置。可按实际地面高度进一步调HalfHeight，不能把这次136.15当作全地图固定车高。需团队手动改属性／保存并在区域内驾驶确认抓地和摆动、出区恢复；本轮未写入资产，未编译或执行新自动化测试。

只读证据：`Saved/SlipperyArea/20261117/Scene.json`、`Inspect.log`、`Live_R1.json`／`Live_R2.json`（本机Editor当时无PIE世界，未取得游戏快照）。若高度修正后仍无效果，下一步在实际游戏中采样Receiver来源、GAS Tag／Grip和物理消费状态，继续排查共用链路。

## 规则与入口

`AHALExpandingSlipperyArea` 是独立的服务器权威 Actor，不依赖车辆技能或打滑带。生成位置是圆柱范围中心，使用世界单位，不随 Actor 缩放或旋转改变范围。服务器每 0.02 游戏秒检查车辆中心；生成时立即检查一次。

- `Expansion Seconds`：默认 2 秒，从 1 cm 种子半径线性扩大到最大半径。0 秒表示立即达到最大范围。扩张期间已生效，没有预警无效阶段。
- `Duration Seconds`：默认 5 秒，**从扩张完成后起算**。总寿命是扩张时间加持续时间，到期销毁。
- `Maximum Radius`：默认 500 cm，指半径。
- `Half Height`：默认 200 cm，中心上下的高度容差，避免无限高度影响。
- `Handling` 使用与 Slippery Trail 相同的结构和原生默认配方：抓地倍率 0.30、恢复 0.35 秒；Sway 幅值 5 rad/s²、周期 0.9 秒、700 cm/s 满幅、渐入 0.2 秒。

通过 `UHALMovementAreaReceiver::RegisterArea/UnregisterArea` 复用原生 `UHALSlipperyAreaEffect` 及运动桥接，和 Trail 一样有随车速缩放的左右摆动。静止时没有摆动力，离区停止摆动、抓地按既有曲线恢复。不降低加速度或速度上限，不造成伤害；**按用户明确要求，生成者也受影响，不添加免疫**。多来源取最低抓地倍率和单个最强摆动，不叠加摆动。

旧的平铺抓地／恢复字段改为共享 `Handling` 分组，使已有 BP 不继续沿用原来的 0.2／0.4 默认值。范围、时间与 Debug Draw 字段保持。团队无需修改 Event Graph；需要调参时在 BP 的 Settings → Handling 展开 Grip／Sway。默认值取原生 Trail Config；以后若单独改 Trail DA，圆形 BP 的 Handling 不会自动追随，须保持两份配方一致。

权威 C++ 可以直接生成默认 Actor；自定义参数用 deferred spawn，在 `FinishSpawning` 前调用 `ConfigureArea`。客户端不自行生成权威区域。范围参数和开始时间复制，客户端用服务器时钟显示阶段。

## 调试显示与下一步

在 BP 的 **Class Defaults → Slippery Area → Debug → Debug Draw** 打勾并 Compile／Save，生成后显示地面圆盘网格：扩张红色、维持蓝色。默认不勾，关闭时效果仍生效。该勾选随服务器复制到客户端，不需要各端再输显示命令。旧 `hal.SlipperyAreaDebug` 已由此勾选取代。Shipping 无调试绘制。

网格使用引擎 DrawDebugMesh 和圆内网格线，无需材质或额外资产。显示半径取当前实际判定半径；中心向上下 Half Height 内查找地面，显示位于地面上方 3 cm，并跟随该处地面法线。没有命中可支撑地面时回退到 Actor 中心平面，不改变权威范围位置。整片显示参考中心切平面，尚不逐顶点贴合崎岖地形。

### Debug 生成命令

游戏控制台输入 `HALSpawnSlipperyArea`，在自己当前车辆前方生成一个区域。默认前方 350 cm，中心高度与车辆物理根相同。生成后立刻开始扩张和生效。要显示范围，先在服务器所用 BP 勾选 **Debug Draw**。单独勾选不会生成物件；单独生成而未勾选则只有效果、没有网格。

使用自己建立的 BP：在 **Project Settings → Game → HAL Debug → Slippery Area → Default Slippery Area Class** 选择该 BP，保存设置。`Slippery Area Forward Distance` 控制前方距离，设 0 可直接在车中心生成。BP Class Defaults 中的范围、时间和抓地设置直接保留，命令不会用原生默认值覆盖它们。监听服务器使用服务器保存的类和距离，客户端不能提交任意类或生成位置；更改后重启测试进程，以确保两端使用正确配置。

默认类为原生 `HALExpandingSlipperyArea`，不需先建 BP。显式清空类或配置非法时拒绝生成，不静默回退。只接受当前拥有有效车辆的控制器请求；客户端请求经可靠 RPC 交由服务器生成并复制结果。成功／失败反馈显示在屏幕与日志。Shipping 构建禁用该命令。不需要修改技能授权或 Event Graph。

## 验证与边界

### 2026-11-10 操控效果对齐

基点 `main/ae389ba`；UE5.8.2 Editor／Game Development 均成功。Debug＋MovementAreas＋SlipperyTrail **11/11**，0 测试警告／失败／未运行／进行中。新增断言只读加载保存的 Trail DA 和圆形 BP，抓地、恢复及完整 Sway 参数均一致；将区域 Owner／Instigator 设为目标车辆，确认仍登记来源、收到摆动且离区移除。两份保存资产前后 SHA256 全同。

报告 `Saved/ExpandingSlipperyArea/TrailParity/index.json`，日志 `TrailParity.log`。NullRHI 单进程，不冒充新双端或真人验收。未改保存 BP／DA／Config，未提交／Push。团队重开 Editor 后用原 HALSpawnSlipperyArea 测试，BP 的 Settings → Handling 可看到新抓地／恢复和 Sway 分组。

新增 `HAL.FutureCreation.MovementAreas.ExpandingLifecycle`，检查默认参数、时间边界、零扩张、非法参数、中心立即注册、离区、扩张覆盖与销毁清理。既有 MovementAreas 测试覆盖共享 GE、重叠选择与恢复。

手工验收：生成在车辆中心，确认扩张阶段已低抓地；停车在外侧等待范围扩至车辆；穿越与退出；多个区域交叠并先后销毁；Listen Server/Client 对照红蓝切换与到期恢复。尚未进行真人、双端和画面验收。

判定使用车辆中心，边缘接触车身不算进入；每 0.02 秒采样，不提供高速穿过极小区域的连续扫掠保证。范围不投影地形；在斜坡和多层地图应配置合适中心高度与 Half Height。

## 2026-11-10 本机结果

基点 `main/b11f305`，UE 5.8.2-56702186。Editor 和 Game Development 编译通过。最终 `HAL.FutureCreation.MovementAreas` **5/5**，0 测试警告／失败／未运行／进行中，NullRHI 单进程；未进行全量原生、新双端或画面验收。

第一次编译发现 DOREPLIFETIME 要求形参名 OutLifetimeProps，已修复。首次区域报告 4 成功／1 失败：新增夹具没有初始化世界游戏生命周期，Destroy 不派发 EndPlay；补齐与既有真实重叠测试一致的世界初始化后 R2 全通过。首轮完整构建还显示原有 HALCameraTests.cpp C4305 警告，未改无关相机测试。

首轮报告 `Saved/ExpandingSlipperyArea/Tests/index.json`，最终报告 `Saved/ExpandingSlipperyArea/Tests_R2/index.json`，日志分别为同目录 `Tests.log`／`Tests_R2.log`。这些 Saved 输出不入库；本节保留跨设备结论。没有修改 uasset／umap／保存配置，没有提交或 Push。

## Debug 命令验证

### 地面网格更新（2026-11-10，编译及专项已通过）

用户保存关闭 Editor 后重新链接成功，Editor／Game Development 均通过。本次 Debug＋MovementAreas **8/8**，0 测试警告／失败／未运行／进行中；新增 `DebugGroundMeshCheckboxAndColors` 验证通过。报告 `Saved/ExpandingSlipperyArea/GroundDebug/index.json`，日志 `GroundDebug.log`。NullRHI 验证实际提交的 debug 网格三角形、线框、颜色、半径、地面高度与勾选门槛；没有执行真实画面像素或新双端验收。下段待验证描述保留首次链接受阻时点，当前已解除。

新增 BP 的 Debug Draw 勾选及其复制、地面圆盘网格、红蓝阶段和圆内线框。Game Development 构建通过。Editor C++ 编译完成，但链接阶段被正在运行的 UnrealEditor 占用两份 DLL 阻止（LNK1104）；等待团队保存关闭后重新构建。新增 `DebugGroundMeshCheckboxAndColors` 尚未运行，不沿用下方原 7/7 结论作为本次证据。该测试检查未勾选无提交、填充网格及网格线、地面高度、红／蓝切换、最终半径、取消勾选和到期不再提交。

没有修改团队保存的 BP、比赛地图或 DefaultGame.ini。团队需在更新 Editor 后打开 BP，Class Defaults 搜索 Debug Draw，勾选并 Compile／Save，再进游戏输入 HALSpawnSlipperyArea。当前日记只找到旧显示开关执行，未找到成功生成消息，因此此前“未显示”尚不能判定为生成失败或单纯显示未开启。

### 此前生成命令验证（地面网格改动前）

此前生成命令的 Editor／Game Development 两 Target 均构建成功。保留既有相机测试的 C4305 编译警告。

2026-11-10 新增 `HALSpawnSlipperyArea`、服务器无参数请求、客户端结果反馈和 HAL Debug 类／距离设置。新增测试覆盖无车拒绝、一次请求一次生成、按权威车头定位、复制标记与原生类默认值保留、空类不回退、非法距离和非车辆拒绝。测试不会保存 Project Settings 或修改 BP。

UE 5.8.2 Editor 构建通过，`HAL.FutureCreation.Debug` 与 `HAL.FutureCreation.MovementAreas` 合计 **7/7**，0 测试警告／失败／未运行／进行中；报告 `Saved/ExpandingSlipperyArea/DebugCommand/index.json`，日志 `DebugCommand.log`。这是 NullRHI 单进程直接命令入口测试，不等于真实客户端 RPC 或已保存 BP 的画面验收。手测：将自己的 BP 选入 HAL Debug，Host 和 Client 分别执行一次命令，核对成功信息的 Class 与 BP 一致、红蓝阶段和到期恢复；在各窗口单独开启显示开关。前述未实现命令的状态为此前交付时点。
