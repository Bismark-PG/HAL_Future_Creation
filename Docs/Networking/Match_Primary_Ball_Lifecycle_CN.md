# 正式比赛主球的生命周期

正式地图 `MVPB_Match` 已配置一颗常驻主球。开赛时，服务器接管关卡预放的 `BP_BasicBall`，记下它的开球位置。玩家可以照常带球、发射、击飞或争抢这颗球。球停在场内、长时间被持有或被高高抛起，都可以继续参与比赛。

## A 款轨道仪联赛球（2026-11-25）

正式主球的可见直径和物理直径都是 **230 cm**。爆炸球、黑洞球、陨石球的实体和碰撞直径都是 **200 cm**，主球大 **15%**。爆炸球的日冕会延伸到球壳外，同场看时视觉占幅更大。主球沿用 35 kg 质量。开球和回场时，球底距行车面 8 cm，静止后贴地。[四种球同镜头实拍，从左至右依次为主球、爆炸球、黑洞球、陨石球](../Combat/Art/PrimaryBall/Orbital_FourBalls.jpg)

象牙白球壳上嵌有两道钴蓝轨道、黄铜包边和青绿色四芒星徽。短青色灯条沿其中一道轨道布置。图案固定在模型的局部坐标中，球体转动时可以看出滚动方向。冷暖金属反光来自场地灯光。

![正式比赛夜场里的轨道仪主球近景和 Car01 同框实拍](../Combat/Art/PrimaryBall/Orbital_InMatch.jpg)

| 想调整什么 | 在哪里调 |
| --- | --- |
| 调整球壳、轨道、包边、徽章、灯条 | `/Game/Assets/Balls/Primary/MI_PrimaryBall_Orbital` 的 `Orbital Finish` 组。依次对应 `Pearl`、`Cobalt`、`Brass`、`DialColor`、`Signal`，灯条亮度由 `SignalStrength` 控制。 |
| 确认比赛使用的球型 | `/Game/Data/MVP_B/DA_Ball_Normal_TestBaseline_v1` 的 `Radius` 为 115 cm，`Visual → Mesh` 引用专用网格 `SM_PrimaryBall_Orbital`，相对缩放为 1。正式图预放与回场均走 `BP_BasicBall`；练习房使用同一普通球配置。 |
| 复测碰撞与外观 | 在空闲 Unreal Editor 从 Python 执行 `Tools/PrimaryBall/VerifyOrbitalBall.py`。会打开正式图，检查预放球、同类新实例及三种特殊球。切线内外 2 cm 的射线分别应命中和落空。 |

本轮在 UE 5.8.3 验证了正式图、实际 PIE 主球和回场球类。球壳 LOD 0 的 2,143 个渲染顶点距物理球心均为 115 cm，六向射线也在 115 cm 命中。实测行车面 Z = −200.274 cm，球静止后的球心 Z = −85.274 cm。独立主机和客户端各 1/1 通过两局回场、平射滚地及解除攻击归属。练习房用 Car01 自动拾取并驾驶约 15.4 m，客户端同时读到 115 cm 的球壳和碰撞半径。客户端持球表现会做位置平滑，本次末尾 60 个样本的显示球心与本地物理球心相距约 2–21 cm。Windows 定向 Cook 完成正式图、普通球 DA 与三个新美术包，0 错误。地图有一条 `WAITING` 文本的 FText 保存警告。本机证据在 `Saved/PrimaryBallA`。四台设备的实际争球手感待团队试玩。

## 替换前的主球占位尺寸

制作 A 款前，正式图的预放球、同类新实例及 PIE 主球均使用 `DA_Ball_Normal_TestBaseline_v1` 里的引擎占位网格。下表保留当时的测量数据，单位为 cm。

| 测量对象 | 直径 | 测量依据 |
| --- | ---: | --- |
| 物理球 `PhysicsRoot` | 100 | 六条穿过球心的射线均在距球心 50 处首次命中 |
| 可见球壳 `VisualMesh` | 320 | 三个轴向均为 320，使用默认比例的引擎网格 `EditorSphere` |
| PIE 中临时等比缩小的球壳 | 100 | 将球壳相对缩放设为 `(0.3125, 0.3125, 0.3125)` 后重测，六个方向的几何边界与碰撞吻合 |

旧球壳比碰撞每侧多出 110 cm。距球心 48 cm 的平行射线会命中，挪到 52 cm 或 105 cm 都会穿过。原因是普通球 DA 分开配置碰撞半径和外观网格，当时的 `EditorSphere` 在单位缩放下比物理球大 3.2 倍。表中的 0.3125 是旧网格的临时诊断比例。原始测量见本机 `Saved/MainBallReview/Current.json` 和 `Saved/MainBallReview/PIE.json`。

正式图双端 PIE 在客户端自由球上也读到半径 160 / 50 cm、球心间距 0 cm。持球移动时另有位置差异。在开发练习房确认选车后，让主机的 Car01 自动拾取同款普通球，再驾车移动约 15.4 m，客户端记录了 183 次持球观测。末尾 60 次采样中，可见球心与客户端本地物理球心相距 21.11–24.80 cm，中位数 22.54 cm。开始拾取前为摆放球做了瞬移，统计排除了拾取过渡。这组数据来自单次本机观察，持球显示会相对车辆平滑，服务器依然以物理球判定命中。采样在 `Saved/MainBallReview/PracticeRemote.json`。

另从练习房进入正式比赛，以 80 ms 主机出站模拟延迟跑了两局回场和一次平射滚地，主客两端的回归均为 1/1 通过，报告在 `Saved/MainBallReview/MatchPair/Summary.json`。旧测试图 `MVPB_NetTest` 的 `BallRelativePresentation` 专项在建立持球约束时失败，未取得该用例的转向数据。报告在 `Saved/BallClientReview/Pair_20261125_095424`。

## 正式主球的外观提案

三张概念图均用夜间六边形金属地板作背景，A 已选为正式方案。左侧展示表面工艺，右侧展示与车辆同框的辨识度。上方实拍展示正式资产在比赛灯光下的效果。

| A 轨道仪联赛球 | B 极光舷窗球 | C 星际方旗球 |
| --- | --- | --- |
| ![象牙白球壳上的蓝金轨道与导航星](../Combat/Art/PrimaryBall/A_Orbital.png) | ![青紫极光透过未来运动球的舷窗](../Combat/Art/PrimaryBall/B_Aurora.png) | ![航星与珊瑚红赛车方旗环绕球面](../Combat/Art/PrimaryBall/C_Pennant.png) |
| 两道蓝色轨道沿球面交叉，滚动方向容易辨认。 | 银白框架圈住深色舷窗，极光集中在球壳内。 | 导航星接上方格旗，红色斜带随球转动。 |

<details><summary>ImageGen 概念图生成提示（中文）</summary>

三图使用内置 ImageGen 分别生成。共同提示为 3:2 横图，给星际碰碰车躲避球联赛设计常驻比赛用球。左边是球的四分之三俯视近景，右边是同一设计与一辆粉红、青绿色碰碰车同框的小型比赛视角。夜蓝色拉丝六边形钢板，少量暖色地灯，左上冷光，右侧暖色反弹光。使用柔和的三档卡通明暗、简洁大色块和清晰的接地影。球体为正球，装饰嵌入表面。排除文字、HUD、悬空环、尖刺、轨迹与球外粒子。

- A 使用象牙白陶瓷球壳、两道钴蓝大圆轨道、细黄铜包边与青色四芒星徽章。俯视缩小时仍能看见蓝带从球面滚过。
- B 使用深海青色球壳、银白 Y 形骨架和三块圆角烟色舷窗。窗内有青紫极光和寥寥几颗星，外表面保持完整。
- C 使用珍珠奶油色搪瓷球壳、宽珊瑚红斜带和海军蓝导航星。星尖接赛车方格旗，辅以流线弧带和少量细缝，传达星际赛事转播的气质。

</details>

## 发射后的贴地滚动与争抢

普通球贴着地板或斜坡滚动时，球速连续 **0.15 秒**处于 **10 m/s** 以内，服务器就结束这次攻击并切到 `Free`。再过 **0.15 秒**，进入车头控球范围的车辆可以拾取。侧墙和触发区无法单独提供地面支撑，球弹离地面时重新计时。高速贴地射门仍可命中，空中沿用原有的 **2.5 m/s 持续 0.5 秒**判定。

在 `/Game/Data/MVP_B/DA_Ball_Normal_TestBaseline_v1` 调整下面的参数。所有使用这份 DA 的普通球都采用相同手感，包括正式图回场后生成的球。

| 想调什么 | DA 参数 | 本版值及作用 |
| --- | --- | --- |
| 多快的滚地球可以失去攻击资格 | Gameplay → State → Ground Roll Release Speed | `1000 cm/s`。提高会更早转 `Free`，设为 `0` 会关闭这条贴地规则。 |
| 要连续贴地多久 | Gameplay → State → Ground Roll Required Duration | `0.15 s`。离地或重新加速超过门槛后重新计时。沿用现有的 `0.05 s` 服务器检查间隔。 |
| 转为自由球后多久能捡 | Gameplay → Acquisition → Ground Roll Pickup Lock Duration | `0.15 s`。直接命中车辆等其他结束方式使用原有的 `Post Launch Pickup Lock Duration`。 |
| 球实际还会滚多远 | Physics → Body → Angular Damping | `5.0`，原为 `0.1`。球滚动时，自转衰减经接地摩擦缩短滑行。此参数也会加快空中自转衰减。线性阻尼仍为 `0.25`，发射初速由车辆配置。 |

爆炸球使用独立 DA，贴地提前结束的开关默认关闭。它可以沿地面滚过并继续攻击，直到满足球型自己的命中或低速结束条件。

### 正式场地平射对照（2026-11-22）

在 `MVPB_Match` 使用回场后的正式主球，在同一处平地上方 40 cm 以 **22 m/s** 水平发射。两次均为本机 Development 一主一客，主机出站延迟模拟为 80 ms。下表按服务器游戏时间计量，每种配置各实射一次。

| 实测指标 | 调整前 | 调整后 |
| --- | ---: | ---: |
| 发射至首次贴地 | 0.30 s | 0.30 s |
| 首次贴地至 `Free` | 8.95 s | **1.00 s** |
| 发射至 `Free` 的水平距离 | 65.95 m | **19.22 m** |
| 进入 `Free` 至允许拾取 | 1.00 s | **0.15 s** |
| 发射至球速降到 1 m/s 以内 | 13.20 s | **2.67 s** |

两端都观测到真实球从 `Launched` 转为 `Free`，同时清除本次攻击和发射者。地面、30° 斜坡、空中、侧墙、查询触发区、短暂弹起、快速贴地球、拾取再发射均有原生专项覆盖。UE 5.8.3 CL58210709 的 Win64 Development Editor 和独立 Game 均构建成功。玩法自动化 277/277 通过，其中两项既有换车用例带警告。团队仍需用手柄和键盘实际感受这版节奏。

本机 Exhibition 一主三客再测两局，主球首次贴地到 `Free` 同样为 1.00 秒，四端均观测到攻击资格清除。首局爆炸球命中后目标从 100 降到 75 HP，次局自然消散时目标保持 100 HP。四个进程均退出 0，测试事件无警告，报告在 `Saved/MainBallGround/FourPlayers/Summary.json`。

可在工程根目录运行下列命令重测该段射程。主机日志中的 `HAL main ball roll` 列出首触地时间、转为 `Free` 的时间、距离和拾取间隔。本机前后对照在 `Saved/MainBallGround/Before/Host.log` 和 `Saved/MainBallGround/After/Host.log`，全量自动化在 `Saved/MainBallGround/Full/index.json`。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\TestGameFlowLoop.ps1 -EngineRoot 'D:\Program Files\Epic Games\UE_5.8' -Profile Development -VerifyGroundRoll
```

Editor 使用统一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup`。为验证可执行游戏也能载入新增配置，另外构建了独立目标，使用的完整命令如下。

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' HAL_Future_Creation Win64 Development '-Project=D:\HAL_Future_Creation\HAL_Future_Creation.uproject' -WaitMutex -NoHotReload -NoLiveCoding '-Log=D:\HAL_Future_Creation\Saved\MainBallGround\Game.log'
```

## 球什么时候回场

| 比赛中发生的事 | 玩家看到的结果 |
| --- | --- |
| 球短暂越过边线又弹回 | 继续使用原球，保留原有球权和攻击归属。 |
| 球心持续越过圆形边界，或跌到地板以下超过容许深度 | 超过宽限时间后回收旧球。如果当时仍有持球车，同时解除它与旧球之间的物理约束。 |
| 主球 Actor 被销毁，或开赛时缺少预放主球 | 自动寻找安全落点。优先使用原开球位置，缺少预放球时从场心开始寻找。 |
| 找到安全落点 | 主机和客户端都先显示青色空心地环。默认预告 1 秒，随后在环内放入一颗可立即争夺的自由球。新球恢复主球和相机标记，攻击归属从空开始。 |
| 车辆或机关占住落点 | 服务器另选位置，重新给足预告时间。开球点附近被大面积占用时，会扩大到场内其他位置。所有候选点都被占用时，每 0.5 秒重试。 |
| 比赛结算或离开地图 | 停止补球，撤下预告。下一局重新接管该局的开球点。 |

常驻指整场比赛都有一个主球名额。回场预告和等待安全落点期间会有短暂空档。场上的额外用球可以独立存在，回场流程只补主球。

## 场地与调参

在 **Project Settings → Game → HAL Match Ball → Arenas** 展开 `MVPB_Match`。数值写入 [DefaultGame.ini](../../Config/DefaultGame.ini)，下局加载地图时生效。

| 参数 | 正式图当前值 | 调整含义 |
| --- | --- | --- |
| Match Map / Primary Ball Class | `MVPB_Match` / `BP_BasicBall` | 配对比赛地图和补球用的普通球蓝图。球的物理、尺寸和外观沿用这份蓝图的球 DA。 |
| Floor Center | `(-58.74, 173.81, -200.27) cm` | 填实际行车面的中心，Z 用地板碰撞的表面高度。 |
| Playable Radius | `7000 cm` | 从场心到正式图行车面外缘的水平距离。物理护网贴着钢板、略靠内侧。[正式图边界实测](../Presentation/ArenaFloor_SatinMetal_CN.md#compact-rim-20261126) |
| Fall Depth | `300 cm` | 球心允许低于地板的距离。 |
| Outside Grace Seconds | `0.35 s` | 允许球自然弹回的时间，可调 0～5 秒。服务器每 0.15 秒检查一次。 |
| Return Warning Seconds | `1.0 s` | 落点显示到放球之间的最短时间，可调 0.25～5 秒。 |

**移动开球点**时，在关卡里移动那颗勾选 **Ball → Camera → Primary Camera Target** 的普通球即可。正式图目前开球点的 XY 是 `(320, 570) cm`。每张比赛地图只标记一颗，其他球将此项留空。新增比赛地图时，在 `Arenas` 中添加对应条目并填写实际地板范围。

**更换提示环材质**可在同一设置页调整 **Presentation → Return Ring Material**。材质可通过名为 `Color` 的向量参数接收青色。环在各端本地渲染，没有碰撞。

## 复测入口

在工程根目录的 PowerShell 中运行以下命令。它启动本机四个进程，连续完成两局。第一局令主球水平出界，第二局直接销毁主球，各端检查落点提示、唯一性和恢复后的球权。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\TestGameFlowLoop.ps1 -EngineRoot "D:\Program Files\Epic Games\UE_5.8" -Profile Exhibition -VerifyPrimaryBall
```

只需要一主一客时，使用 `-Profile Development`。运行结果写入脚本打印的报告目录。服务器日志类别为 `LogHALMatchBall`。团队实机验收时，可以在回场环上停车，观察预告换位，然后驶向新落点，确认可立即控球和发射。

### 2026-11-22 本机验证

| 项目 | 结果与本机记录 |
| --- | --- |
| Win64 Development 构建 | UE 5.8.3 CL58210709。Editor 统一入口退出 0，最终日志在 `Saved/BuildTool/BuildAndLaunch_20261122_142341_276_987176`。独立 Game 目标编译、链接通过，见 `Saved/MatchBall/Game_Final.log`。 |
| 玩法自动化 | 从项目默认地图运行 `HAL.FutureCreation`，273/273 通过。两项既有换车测试带警告。主球两项均无警告，覆盖正式图资产、控球／发射、短暂越界、跌落、占位换点、无可用地面时重试、缺少预放球、额外球独立和结算取消。见 `Saved/MatchBall/Native_Final/Report/index.json`。 |
| 提示环渲染 | D3D12 俯视相机在测试地板上读回 5,364 个青色环像素，环中心检测到 0 个。开启画面见 `Saved/MatchBall/GroundWarning_Lit.bmp`，报告见 `Saved/MatchBall/Render_Final2/Report/index.json`。 |
| 正式地图联机 | Exhibition 一主三客，连续两局，主机模拟 80 ms 出站延迟。四端逐局确认预告、回场位置和唯一自由球，均退出 0 且无警告。见 `Saved/MatchBall/Network_Final/Summary.json` 与各端日志。四台独立电脑的打包画面仍待团队验收。 |

为检查独立游戏可执行文件是否链接了该组件，本轮另外运行以下 Game 构建命令。日常 Editor 编译继续使用工程根目录的 `BuildAndLaunch.cmd -NoLaunch -NoPopup`。

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' HAL_Future_Creation Win64 Development '-Project=D:\HAL_Future_Creation\HAL_Future_Creation.uproject' -WaitMutex -NoHotReload -NoLiveCoding '-Log=D:\HAL_Future_Creation\Saved\MatchBall\Game_Final.log'
```
