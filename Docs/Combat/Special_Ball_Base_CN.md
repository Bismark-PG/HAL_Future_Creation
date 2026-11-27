# 一次性特殊球

## 所有球型共用的规则（已确认）

特殊球可以和唯一常驻主球同时上场。每辆车仍只带一颗球，每颗特殊球最多触发一次终结效果。下表是可发射球型的基础规则。定时炸弹球通过接触点火和撞车易手，点火后使用独立引信。主球保留自己的回场和相机规则，特殊球消耗、出界或结束本局后由场地回收。[比赛主球生命周期](../Networking/Match_Primary_Ball_Lifecycle_CN.md) · [定时炸弹球](#timed-bomb-concepts)

| 球的状态 | 剩余寿命 | 争夺与攻击 |
| --- | --- | --- |
| 自由球 `Free` | 消耗剩余时间，到零自然消散 | 可以争抢、滚动或被推开。自由球碰车只产生普通物理接触。 |
| 持球 `Controlled` | 暂停 | 持球被撞掉后转回自由球，保留特性和剩余时间。原持球者沿用普通球的短暂回收限制，其他车可以抢球。 |
| 已正式发射 `Launched` | 暂停 | 第一次满足球型触发条件时结算并消耗。打空后按普通球的反弹规则运动。持续低速或被技能解除攻击后转回自由球，接着倒数。 |

普通墙面反弹后，仍在攻击状态的球可以继续命中。球型可以明确指定额外的消耗条件，例如爆炸球可以在撞墙时引爆。服务器确认一次触发后，即使同帧收到多条碰撞回调，也只结算一次。目标免疫由现有战斗规则处理，一次有效命中即使被抵抗也会消耗球。

**攻击归属跟着最近一次正式发射。** 球自身保留球型特性。本次攻击结束或被防御解除时清除这次发射的攻击资格，下一次正式发射建立新的归属。例如 A 射出的球被 Car05 龙卷风解除攻击，B 拾取后射中对手，这次命中归 B。水炮等能正式重新发射来球的技能，也按新发射者结算。

首版累计自由时间为 20 秒，最后 3 秒给出即将消散的视觉提示，两项均可调。例如 A 在球自由存在 8 秒时拾取，持球一段时间后被撞掉，球滚了 2 秒才被 B 抢到。B 此时还剩 10 秒自由时间可以使用。临期提示也随倒计时暂停和恢复。

## 所有球型的视觉要求（已确认）

- **生成。** 每次投放都有醒目的出场特效。四人俯视镜头下应能看清新球出现的位置、球型轮廓以及何时可以争抢。
- **自然消散。** 寿命耗尽时有清晰的退场特效，能看出剩余能量已耗尽。自然消散不造成伤害或冲击。
- **触发消耗。** 由球型的命中特效承接球体退场。生成、自然消散和命中应当一眼可区分。主机和客户端依据同一次服务器事件呈现结果。

## 正式赛场的特殊球生成盘

`MVPB_Match` 的中圈两侧各有一个六孔盘。在 World Outliner 展开 **Gimmick → Special Ball Pads** 找到 `SpecialBallPad_NE` 和 `SpecialBallPad_SW`。爆炸球投放前亮起橙金色脉冲，黑洞球是紫色旋纹，陨石球是红橙色流光，定时炸弹球是红色双弧和琥珀刻度，跟踪导弹球是青绿与橙色的箭纹。球从盘心出现，播放各自的生成特效。

全场共用 **Content → Data → Balls → DA_Arena_SpecialBallSupply**。修改后重新开一局即可试玩。

| 参数 | 首版值 | 调整效果 |
| --- | --- | --- |
| First Drop Seconds | 15 秒 | 从比赛进入 Playing 到首次出球，含预告时间 |
| Drop Interval | 25 秒 | 全场两次成功投放之间的间隔。两盘轮流优先，同一轮只投一颗 |
| Warning Seconds | 1.5 秒 | 提前亮灯并向盘心汇聚。首轮默认在开赛后 13.5 秒亮灯 |
| Blocked Grace | 2 秒 | 盘心被占时最多等待多久，每 0.25 秒复查一次 |
| Ball Pool | Explosive、Black Hole、Meteor、Timed Bomb、Homing | 每组五种各出一次，顺序随机。只保留一种即可定向测试 |
| Spawn Clearance | 8 cm | 球底相对实际行车面的初始间隙。球心高度另加该球型的实际碰撞半径 |
| Pad Diameter / Pad Thickness | 500 / 3 cm（当前正式 DA） | 六孔盘的可见大小和厚度 |

预告前，优先盘被车辆、球或实体障碍占用时，服务器可以选另一个空盘。亮灯后位置锁定，若车辆驶入，会在原盘等它离开；持续占用则跳过本轮，待投球型留给下一轮。等待开赛和结算期间停发，下一局重新计时、洗牌。

**场上特殊球数量没有硬上限。** 玩家把球带离盘心后，后续仍能继续投放。想让多球同场更常见，可以缩短共享 DA 的 `Drop Interval`，或在对应球型 DA 延长 `Free Life Seconds`。持球、正式飞行继续沿用各球自己的暂停计时规则。

**生成盘可以直接放置。** Outliner 里的 `Open HALSpecial Ball Pad` 指向可直接放进关卡的 C++ Actor。每个盘记录位置、轮转顺序和钢板的装饰高度。GameState 上的供球组件在开赛时收集全场点位，由服务器统一计时并选球，客户端显示同一次亮灯预告。制作同款盘只需布点，修改共享 DA 即可统一调节整场投放。

**在正式图加第三个盘**

1. 打开 `MVPB_Match`，在 **Gimmick → Special Ball Pads** 选中 `SpecialBallPad_SW`，按 **Ctrl+W** 复制，改成便于识别的名字。也可以在 **Place Actors** 搜索 **Special Ball Pad**，将原生类 `HALSpecialBallPad` 拖入关卡。
2. 将盘心移到空旷位置。Actor 原点放在车辆实际行驶的碰撞面。现有两处内场的世界 **Z 为 −200.274 cm**。同款钢板保留 **Deck Finish Offset = 3.75 cm**，它只抬升可见盘面。若在其他高度布置，把原点重新对齐当地行车面；高度相差超过 8 cm 的点位会被跳过。
3. 在 Details 把 **Rotation Order** 设为 **2**（东北为 0，西南为 1），保存关卡，重新开一场比赛。新盘会自动进入轮转。默认全场仍每 25 秒投一颗，遇到被占用的盘会按规则选择空位。

如果把第三个盘纳入正式地图基线，同时更新 `HAL.NetworkSmoke.SpecialBallSupply.FormalMatch` 对正式点位数量的断言。目前该回归用例按两盘验收。

2026-11-25 在正式图的临时 PIE 副本放入同类 Actor，设为顺序 2，以原有投放参数、三倍测试时间流速实际开赛。服务器依次在 0、1、2 号盘投出爆炸球、黑洞球、陨石球；客户端在同样三个盘心看到相同球型。这是当次洗牌的结果。记录在本机 `Saved/PrimaryBallA/ThirdPad.json`。

生成盘的六孔模型和三个材质实例位于 `/Game/Assets/ArenaMechanics/SpecialBallPad`。单独拖入网格只会摆放模型，要参加轮转应拖入上述 Actor 类。

**2026-11-24 实测。** UE 5.8.3 的 Development Editor 与 Game 均已编译。连续投放六颗，其中一颗被持有，验证了多球共存、两轮洗牌、盘心阻挡重试、跳过和重新开赛。正式图的两个盘都通过盘心和两侧的地面射线检查，主机车辆横穿空盘期间测得车身高度变化小于 0.01 cm。主机出站模拟 60 ms 延迟，主机与独立客户端均看到东北盘在约第 15 秒生成陨石球、西南盘在约第 40 秒生成爆炸球。以下三帧来自同一客户端机位，依次为待机、盘心预告和球体生成。[游戏内三帧实拍](Art/SpecialBallPad/SupplyPad_InGame.jpg)

<a id="homing-ball"></a>

## 跟踪导弹球

持球时，橙色弹头与车头的瞄准箭头同向。停车、倒车或横向漂移时，看弹头就能判断起射方向。发射后，导弹沿车头起飞，锁定距发射者最近的存活对手，弹头随航向转动。对手可以在车后方。初速低时能转紧弯，提速后转弯逐渐变宽。高速掠过目标、目标落在弹后且距离拉开时，本次发射永久失锁。发射时没有对手、或被锁车辆中途淘汰，导弹也会沿当前方向飞完余程。Car08 烟幕中仍可锁定和追踪。甩开后的导弹仍能意外击中其他车辆。

车辆、实体墙、阻挡机关和通电电幕能引爆来球。服务器按接触点对范围内对手逐车结算一次伤害与冲击，实体遮挡可挡爆风。普通行车地面允许贴地飞行。自由、持有沿用特殊球基础寿命，正式发射另起飞行时钟，飞行到期在原地散为冷色碎光。

| 调节项目 | `DA_Ball_Homing` 首测设置 |
| --- | --- |
| 起步与极速 | `Initial Speed / Cruise Speed` 为 1600 / 4000 cm/s，`Acceleration Seconds` 为 1.8 秒 |
| 转弯与甩脱 | `Initial Turn Rate / Cruise Turn Rate` 为 180 / 35°/s。完成提速幅度的 72% 后，7 米内的近距离错车可触发甩脱 |
| 两套寿命 | `Free Life Seconds / Warning Seconds` 为 20 / 3 秒，`Flight Seconds` 为 4 秒 |
| 爆炸 | `Explosion Radius` 为 250 cm。`Gameplay → Damage / Impact` 首测为 25 HP、Heavy |
| 飞行与特效 | `Exhaust Length` 控制高速尾焰长度。三种颜色、发光强度及生成、消散和命中特效时长均在本 DA |
| 锁定预警 | `Warning Screen Fraction` 为视口高度的 4.7%，`Warning Approach Seconds` 为 1.6 秒。预警指向屏外导弹，接近时加快闪动，入镜或失锁时收起。`Warning Bottom Inset` 给底部中央的能量条预留 10% 高度 |

开发版在控制台输入 `HALSpawnHomingBall`，靠近球后用通常的发射键即可试射。两人可在中场拉开距离，让被锁一方看到屏边提示，再横向漂移试甩脱。单人无对手时可以检验直线发射和撞墙引爆。

**2026-11-27 实测。** UE 5.8.3 CL58210709，球、生成器、烟幕与 HUD 共 29 项自动化通过。专项测试覆盖无对手前射、后来入场不补锁、高速错车后保持原航向、目标淘汰、烟幕中继续锁定、飞行到期无伤害及旁车只结算一次。

正式 `MVPB_Match` 本机一主一客，主机出站模拟 80 ms 延迟。发射车自动拾取导弹，用普通持球发射流程打出。服务器记录极速 4000 cm/s，两端均记录半径 250 cm 的爆炸、目标 HP 从 100 降至 75，以及 Heavy 冲击。来球从屏幕下方飞入时，客户端的提示出现在能量条上方，入镜后收起。另一颗从东北盘投放，自由存在 20 秒后播放消散。测试只临时指定了首轮投放球种，磁盘重载确认正式供球池共有五种。四人真人对抗中的甩脱手感待试玩。[生成盘实拍](Art/HomingBall/Match_Arrival.png) · [被锁车辆视角](Art/HomingBall/Match_Offscreen.png)

Editor 使用项目统一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup`。独立游戏目标另用以下命令验证 Win64 Development。六份球型、网格和材质资源的 Windows 定向 Cook 为零错误、零警告。本机报告位于 `Saved/HomingBall` 的 `RulesAccepted`、`NetAccepted` 和 `CookAccepted.log`。

```powershell
& 'D:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' HAL_Future_Creation Win64 Development '-Project=D:/HAL_Future_Creation/HAL_Future_Creation.uproject' -WaitMutex -NoHotReload -NoLiveCoding '-Log=D:/HAL_Future_Creation/Saved/HomingBall/GameFinal.log'
```

<a id="homing-held-facing"></a>

**持球朝向复测（2026-11-27）。** 之前持球时弹壳沿用球形刚体的旋转。现在持球模型在位置平滑后对齐车头，沿用正常起射的水平方向。`HAL.FutureCreation.SpecialBall.Homing.HeldFacing` 验证停车转向、掉球、换手及出手，持球点和碰撞体在转动模型时保持稳定。相关自动化通过 30/30。

`MVPB_NetTest` 一主一客各自自动拾取一颗初始朝向偏转 90° 的导弹，主机出站模拟 80 ms 延迟。两车向相反方向转弯并拉手刹，主机车头转过 178°，客户端车头转过 118°。两端逐帧检查自己和对手的弹头，最大水平偏差均小于 0.01°。`MVPB_Match` 双端另测了正式拾取、发射与命中。报告在 `Saved/HomingBall/HeldFacing_RulesFinal`、`HeldFacing_AutoPickup` 和 `HeldFacing_Match`。复现弯道用例时，两端启动参数均加 `-HALHomingFacing`，运行 `HAL.NetworkSmoke.BallRelativePresentation`。[客户端漂移实拍](Art/HomingBall/HeldTurn.png)

<a id="timed-bomb-concepts"></a>

## 定时炸弹球 · 磁扣与分段表盘（2026-11-25）

炸弹在场上巡游，首次碰车时强制占用持球位并点火。持球期间发射键锁定，持有者可以主动撞车转交炸弹。无论是首次碰车、已点火后重新碰车，还是持球者撞车转交，接手车辆原本带着的球都会掉回地面，炸弹立即上车。引信在换手和掉地时继续走时。

引信归零时，两种爆炸都使用服务器的实际球心。持球引爆只对当前持有者造成伤害和冲击，水平冲击从车头持球位置指向车身。落地引爆对范围内的每辆车造成伤害并向外推，包括仍在范围内的上一任持有者。两种爆炸都遵循车辆的冲击抗性和免疫规则。尚未点火的巡游球耗尽自由寿命时，沿用无伤害的自然消散。

正式外观采用 C 款笼式护壳与 3 号磁扣崩裂。四根带倒角的钢肋包住暗红内胆，球顶保留首轮选定的分段表盘。巡游时表盘满格，点火后指针扫动、亮格逐个熄灭，最后三秒转红脉动。转交时，两半钢笼从原车头张开，划过短弧，再扣到接手车头。

<a id="timed-bomb-pad-arrival"></a>

### 从正式生成盘出场

`DA_Arena_SpecialBallSupply → Ball Pool` 中的 `Timed Bomb` 对应定时炸弹球，`Explosive` 对应暗太阳爆炸球。东北、西南两盘共用这个池，每组随机投放四种各一颗。

轮到定时炸弹时，盘面先亮红色双弧。出球瞬间，六道短光向盘心汇聚，琥珀刻度依次点满。球已经开始巡游，两半钢笼在行进中合扣，表盘短亮一次。刻度全亮表示装配完成，第一次碰到车辆后才开始走引信。

| 在 `DA_Ball_TimedBomb` 中调整 | 正式值 | 用途 |
| --- | --- | --- |
| `Effects → Arrival Seconds` | 0.58 秒 | 同时缩放盘面投影和球壳合扣的播放时间 |
| `Arrival → Appearance → Arrival Jaw Opening` | 0.60 | 出场时每半片钢笼额外张开多少个球半径。设为 0 可只看盘面投影 |
| `Effects → Corona Size` | 2.6 | 投影大小，单位为球半径。此项也影响自然消散等沿用 Corona Size 的表现 |
| `Arrival → Appearance → Clasp Arrival Material` | `M_TimedBomb_ClaspArrival` | 出场投影材质。投影沿用球型 DA 的钢色、磁极红、灯色和表盘格数 |

**2026-11-26 正式赛场实测。** 首次联机测试中，远侧盘投放的球有时在短动画结束后才出现在客户端。现由 GameState 广播定时炸弹的出场时刻，并在新球建立网络通道和出场期间提高同步优先级。在 `MVPB_Match` 的 D3D12 一主一客 PIE 中，主机出站模拟 80 ms 延迟，使用正式四球池完整投放一轮。两端依次收到陨石、暗太阳、定时炸弹和黑洞。定时炸弹在东北盘出现时，主机为第 65.10 秒，客户端为第 65.18 秒。另用临时单球池逐盘复测，东北与西南两盘的生成动画在主客两端均只播放一次，钢笼合拢后投影淡出，引信仍未点燃。[正式四球轮转中的客户端实拍](Art/TimedBombBall/ClaspArrival_Match.webp)

UE 5.8.3 CL58210709 的 Win64 Development Editor 与 Game 均已编译，相关自动化 30/30 通过。球型 DA、生成池 DA 和新材质的 Windows 定向 Cook 为 3/3，零错误、零警告。双端记录位于 `Saved/TimedBomb/PadArrivalVerified/Result.json` 和 `Saved/TimedBomb/PadArrivalBoth/Result.json`，规则回归位于 `Saved/TimedBomb/ArrivalFinalRules/index.json`。

<a id="timed-bomb-appearance"></a>

### 钢笼、引爆与持球位置

引信归零时，球壳沿钢肋和环轨断开。两段弧形钢肋、两枚带底轨的红色磁扣和一截上箍环从各自在球壳上的位置飞出。碎片沿不同高度的短弧翻转，倒角和断面随翻转露出，随后错开收小退场。中央短闪照亮膨起的烟云。挂车引爆集中在车头，落地引爆多一道扩到 `Ground Blast Radius` 的压力波。

三种碎片模型沿用完整球壳的截面、曲率和钢材质。主机和客户端各自按同一个引爆时刻播放，五片均为短时无碰撞模型，伤害与冲击由引爆时的球心结算。

**金属碎片实测（2026-11-25）。** 正式图 `MVPB_Match`、D3D12 一主一客，主机出站模拟延迟 80 ms。挂车与落地两种引爆中，两端都读到五片带同一钢材质的无碰撞模型。地爆时，客户端跟踪的一段钢肋距球心水平距离从 1.65 米增至 2.91 米，随后翻过侧面并缩小。持球引爆只扣接手车 25 HP，地爆时圈内两辆车各扣 25 HP，均有水平击退和上抛。未点火球在 20.10 秒时自然消散。[挂车引爆实拍](Art/TimedBombBall/CageFragments_Mounted.png) · [客户端落地引爆回放](Art/TimedBombBall/CageFragments_Match.webp)

UE 5.8.3 CL58210709 的 Editor 使用 `BuildAndLaunch.cmd -NoLaunch -NoPopup` 编译。独立游戏目标使用 `Build.bat HAL_Future_Creation Win64 Development -Project=D:/HAL_Future_Creation/HAL_Future_Creation.uproject -WaitMutex -NoHotReload -NoLiveCoding -Log=D:/HAL_Future_Creation/Saved/TimedBomb/FragmentsGame.log` 编译通过。球与冲击相关自动化 30/30 通过，三种碎片及对应材质、纹理、球型 DA 共七个资源的 Windows 定向 Cook 为零错误、零警告。记录见 `Saved/TimedBomb/MatchLifeReplayFragments/Result.json`、`Saved/TimedBomb/FragmentsRules/index.json` 和 `Saved/TimedBomb/FragmentsCook.log`。

**C＋3 实机验证。** 在正式图 `MVPB_Match` 的 D3D12 一主一客测试中，主机出站延迟设为 80 ms。转交后，持有者扣 25 HP，服务器在归零后的 0.8 秒内测到后退 5.87 米、上抛 2.97 米。落地引爆时，圈内两辆车各扣 25 HP，两端都观察到水平击退和上抛。稳定持球的 201 个客户端采样中，球壳与通用球心的偏差均小于 0.01 cm。另一次无人碰球时，20.10 秒后自然消散。回放取自客户端实际渲染帧，0.73 秒引爆按游戏时间播放，首尾稍作停留。[巡游实拍](Art/TimedBombBall/Cage_Match.png) · [落地引爆回放](Art/TimedBombBall/ClaspSnap_Match.webp)

UE 5.8.3 CL58210709 的 Win64 Development Editor 与 Game 已编译。球和冲击相关自动化通过 30/30，九个球型与美术资源的 Windows 定向 Cook 为零错误、零警告。本次报告为 `Saved/TimedBomb/MatchLifeReplay/Result.json`、`CageRules/index.json` 和 `CageCook.log`（后两项也在 `Saved/TimedBomb/`）。烟云源图来自内置 ImageGen，游戏导入时缩至 512 像素。[RGBA 源图](../../Tools/SpecialBall/Art/T_TimedBomb_ClaspSmoke.png) · [绘制提示](Art/TimedBombBall/AppearancePrompts.txt)

旧球型把红色球形碰撞体显示在游戏中，又从车身碰撞外沿另算球壳的位置。现在 `DA_Ball_TimedBomb` 的 `Physics → Hidden In Game` 已勾选，`Presentation → Controlled → Enabled` 已开启。物理约束使用车辆的 `BallControlPoint`，球壳和表盘继承通用球网格的位置，客户端同时使用持球插值。磁扣的独立位移只发生在交接动作中。巡游专用尾迹已移除。

以下六张是本轮比稿留档。最终选用第三行的球壳和爆炸。球壳图右下角给出缩小后的辨识效果，爆炸分镜从左到右展示起爆、膨胀和消散。

| 球壳方向 | 爆炸方向 |
| --- | --- |
| [A · 重装磁扣](Art/TimedBombBall/Body_A_ClaspArmor.png)　石墨色厚夹爪框住琥珀表盘，黄色短杠提示危险。 | [1 · 撕裂闪爆](Art/TimedBombBall/Burst_1_Shear.png)　三道短促的亮色笔锋切开红黑烟团。 |
| [B · 热裂外壳](Art/TimedBombBall/Body_B_ThermalShell.png)　一条橙红热缝穿过暗壳，磁扣位于两侧。 | [2 · 闷燃烟冠](Art/TimedBombBall/Burst_2_SmokeCrown.png)　火光先贴地闪过，再从下面照亮鼓起的厚烟。 |
| [C · 笼式护壳](Art/TimedBombBall/Body_C_Cage.png)　宽钢肋之间露出暗红内胆。 | [3 · 磁扣崩裂](Art/TimedBombBall/Burst_3_ClaspSnap.png)　两道扣形剪影向外弹开，烟云从中间涌出。 |

六款图使用同一块夜场钢板作背景。[绘制提示](Art/TimedBombBall/AppearancePrompts.txt)

上一版在 `Fred_TestScene` 中单测过通用持球位置。Car07 和 Car12 静止持球时，球壳中心与通用球心重合，两车各自的球心距离车头持球点均小于 0.01 cm。[旧版 Car12 持球实拍](Art/TimedBombBall/Clasp_Car12_CommonHold.png)

在 `MVPB_Match` 的 D3D12 一主一客实测中，主机出站模拟延迟为 80 ms。两端在巡游、转交和稳定持球阶段测得的球壳中心偏差均为 0.000 cm（保留三位小数），表盘中心的水平偏差同样为零。客户端持球段采样 195 帧。转交后的持有者扣 25 HP，服务器记录向后 5.89 米、上抛 2.97 米。地爆命中两车各一次，未点火球在 20.13 秒时自然消散。原始数据为 `Saved/TimedBomb/AppearanceMatch.json`。

本次 UE 5.8.3 CL58210709 的 Editor 构建使用 `BuildAndLaunch.cmd -NoLaunch -NoPopup`。为验证独立游戏代码，另运行 `Build.bat HAL_Future_Creation Win64 Development -Project=D:/HAL_Future_Creation/HAL_Future_Creation.uproject -WaitMutex -NoHotReload -NoLiveCoding -Log=D:/HAL_Future_Creation/Saved/TimedBomb/AppearanceGame.log`。两个目标均通过，球与冲击相关自动化通过 30/30。报告位于 `Saved/TimedBomb/AppearanceRules/index.json`。

在 Development 场景中，把车头朝向一块空地，在控制台输入 `HALSpawnTimedBombBall` 即可试球。正式比赛由现有生成盘从四种特殊球中轮换投放。

| 在 `/Game/Data/Balls/DA_Ball_TimedBomb` 中调什么 | 初始值 | 手感或画面 |
| --- | --- | --- |
| `Free Life Seconds`、`Patrol Speed`、`Patrol Turn Rate` | 20 秒、650 cm/s、26°/s | 未点火时的巡游寿命、前进速度和摆向幅度。碰到实体墙会转向。 |
| `Fuse Seconds`、`Urgent Seconds`、`Dial Segments` | 8 秒、3 秒、12 格 | 从首次上车开始计时，最后阶段改变表盘颜色和脉动。 |
| `Minimum Ram Speed`、`Minimum Closing Speed` | 220、140 cm/s | 持有者须有驾驶输入，沿目标方向有足够速度，且正追近对方。贴车停着或被别人撞不构成主动转交。 |
| `Return Lock Seconds`、`Clasp Travel Seconds`、`Jaw Opening` | 0.7 秒、0.42 秒、0.32 倍球半径 | 两车分开且锁定时间结束后才允许回传。后两项调整松扣到合扣的时长、单侧开口幅度。 |
| `Gameplay → Damage → Vehicle Hit Damage`、`Ground Blast Radius` | 25 HP、500 cm | 两种爆炸共用基础伤害。地爆半径按车辆碰撞体判定，只影响落地引爆。 |
| `Gameplay → Impact → Tier`、`Push Preset` | Heavy、Standard | 两种爆炸共用冲击档位和水平推力预设。档位决定基础平推与上抛，推力预设只调整水平分量。 |
| `Cage → Cage Steel Color`、`Inner Core Radius Ratio` | 枪灰钢、0.77 | 钢肋的主色；内胆半径占物理球半径的比例，可调 0.52 至 0.88。 |
| `Core Color`、`Current Color`、`Pole Tip Color`、`Corona Color`、`Glow` | 暗红内胆、红扣、暖色扣尖高光、琥珀表盘 | 调整内胆、扣件、表盘与引爆闪芯。`Arrival Seconds`、`Expiration Seconds`、`Impact Seconds` 分别控制对应演出的时长。 |
| `Detonation → Appearance → Clasp Burst Reach`、`Clasp Smoke Size` | 0.62、0.54 | 分别控制碎片飞散的基准距离和烟云的半宽，单位为本次演出半径的倍数。落地伤害范围由 `Ground Blast Radius` 决定。 |
| `Detonation → Appearance → Rib / Lock / Collar Fragment Mesh` | 钢肋、磁扣、上箍环 | 替换对应零件。模型按参考球半径 50 cm 制作，枢轴在该截金属上，运行时按实际球径缩放。色彩共用 `Cage Steel Color`、`Current Color` 和 `Pole Tip Color`。 |

强制替换适用于主球和特殊球，也适用于另一颗已点火的炸弹。被换下的炸弹落地后继续走自己的引信。眩晕按通用规则掉球。服务器把伤害记给最后一次主动转交成功的车辆。从未转交过的炸弹由场地结算。

首版在正式图的双端实测中，第一辆车先持主球，第二辆车先持爆炸球。定时炸弹从侧面碰上第一辆车并顶掉主球，按发射键后仍挂在车头。持有者踩油门撞上第二辆车，磁扣张开，跨到对方车头合拢。接手车的爆炸球掉地。主机出站模拟 80 ms 延迟复测时，双方读到同一个引信截止时间。持球引爆后，两车血量为 100、75。下一轮通过眩晕落球，地爆后血量为 75、50。两次引爆的网格在两端都实际出现。[首版巡游](Art/TimedBombBall/Clasp_Patrol.jpg) · [首版磁扣换手](Art/TimedBombBall/Clasp_Handoff.webp) · [地面引爆实拍](Art/TimedBombBall/Clasp_GroundBurst.jpg)

无人碰球的一轮中，两端都看到了生成。20 秒自由寿命结束后，在投放后 20.09 秒观察到两端的消散演出，两车血量均保持原值。测试时把车辆停在球的巡游路线外，使用正式 DA 的速度、墙面碰撞与寿命。

验证环境为 UE 5.8.3 CL58210709。Win64 Development Editor 与独立 Game 已构建，球相关自动化 18/18 通过，10 个正式资源包的 Windows 定向 Cook 为零错误、零警告。原球换下时的物理线程等待已修复，独立主机和客户端在 80 ms 延迟下也各自通过了既有的主球持球漂移与发射测试。本机报告位于 `Saved/TimedBomb/`，正式图复测脚本为 `Tools/SpecialBall/VerifyTimedBombMatch.py`。添加 `-ClaspLag` 可开启 80 ms 出站延迟，`-ClaspExpiry` 增测 20 秒未点火巡游。四人实机的接力手感待团队试玩。

<a id="timed-bomb-impact"></a>

### 挂车引爆的冲击修复与实测

旧版会在引爆时读到 `Heavy / Standard`，但给持有者的单体命中请求只启用了伤害。正式图复现时实际扣了 25 HP，主机和客户端在引爆后 0.8 秒内测得的向后位移与上升高度都是 0 cm。此前的双端用例只检查血量和爆炸画面，因此能在这个问题存在时通过。

挂车命中现在与地爆共用同一段伤害与冲击结算。自动化用另一组 `Medium / Long` 和 1 cm 地爆半径检验挂车命中，确认档位和推力预设按 DA 生效，挂车命中只作用于当前持有者。使用正式 DA 的物理复测结果如下。距离取车辆碰撞体在归零后 0.8 秒采样窗内，沿受力方向及竖直方向的最大位移。

| 场景 | 实测对象 | 水平位移 | 上升高度 |
| --- | --- | ---: | ---: |
| Fred 测试场，自持引爆 | Car07 | 向后 5.23 米 | 2.74 米 |
| Fred 测试场，自持引爆 | 技能木桩 | 向后 5.30 米 | 4.32 米 |
| 正式图，转交后持球引爆 | 接手车，主机／客户端 | 向后 5.84／6.61 米 | 2.97／2.97 米 |
| 正式图，眩晕掉球后地爆 | 爆心两侧的车辆，主机 | 向外 6.13／5.65 米 | 2.97／2.83 米 |

正式图在主机出站 80 ms 延迟下运行，客户端也观测到地爆两车向外分开。两端的血量变化一致，每辆受影响车辆只扣 25 HP。项目统一入口的本次 Editor 构建记录为 `Saved/BuildTool/BuildAndLaunch_20261125_152228_427_60b928/Result.json`。为独立验证游戏代码，另在 Win64 Development 构建 Game Target。命令为 `Build.bat HAL_Future_Creation Win64 Development -Project=D:/HAL_Future_Creation/HAL_Future_Creation.uproject -WaitMutex -NoHotReload -NoLiveCoding -Log=D:/HAL_Future_Creation/Saved/TimedBomb/ImpactGame.log`。球、爆炸和物理冲击相关自动化 30/30 通过。本机记录在 `Saved/TimedBomb/` 下的 `ImpactBeforeMotion.json`、`FredImpact.json`、`ImpactMatchMotion.json` 和 `ImpactRules/index.json`。

以上位移来自 PIE。额外生成的独立 Development 包在进入地图前以 `777006 / CrashDuringStaticInit` 退出，打包版实玩验证待补。

以下保留四款概念图，便于和实机对照。

| 概念 | 俯视时的辨认方式 |
| --- | --- |
| [A · 赛道秒表](Art/TimedBombBall/A_Chronograph.png) | 粗红指针扫过琥珀色刻度。爆炸时，表盘刻度在地面展开一圈。 |
| [B · 磁吸扣球](Art/TimedBombBall/B_MagneticClasp.png) | 两片红白磁扣合到车头。两扣之间的灯格逐格熄灭，接触点短闪一次。 |
| [C · 翻牌计时球](Art/TimedBombBall/C_FlipScore.png) | 黄壳上的大号翻牌直接显示秒数。地面归零时，几片方形计分牌随圆形冲击波翻起。 |
| [D · 倒数河豚](Art/TimedBombBall/D_Puffer.png) | 星际吉祥物鼓起脸颊、眯紧眼睛，颊灯转红。归零时，圆鼓的烟团从地面亮圈中央绽开。 |

四款均采用三档柔和卡渲和同类夜场钢板。[完整绘制提示](Art/TimedBombBall/Prompts.md)

## 陨石球

在 Development 的场地中开车面向空地，打开控制台执行 `HALSpawnMeteorBall`，驶近拾球后按普通发射键。单人空场可测试定点落击，在 `Fred_TestScene` 放一辆活着的 `BP_SkillTestDummy` 可测试追踪。正式球型位于 `/Game/Data/Balls/DA_Ball_Meteor`。

| 时机 | 场上看到的表现与判定 |
| --- | --- |
| 生成、争夺 | 发亮的碎岩向中心聚成一颗带熔岩裂纹的球。显示直径、物理直径均为 2 m。持球暂停自由寿命，易手沿用剩余时间。 |
| 有合格对手时发射 | 球从原处竖直升空。服务器从有有效地面位置的存活对手中等概率选一辆，随后移除可争抢、可碰撞的球体。 |
| 无合格对手时发射 | 服务器沿发射瞬间的车头方向选择固定地面落点，默认车前 12 米。球升空时即显示地面预警圈，随后按同样节奏下坠。转向、驶离或新对手入场都不会移动落点。 |
| 朝场外或缺少地面 | 沿同一方向尝试更近的场内地面，落点与发射时的车身至少相隔一个爆炸半径。前方空间不足时，球当场播放消散并释放持球位，无伤害或冲击。 |
| 追踪 | 被选车辆上方显示 B 款三瓣锁环，中心陨石固定，三片外环随追踪进度向内收。目标在此期间被淘汰或离场，陨石沿用最后的有效地面位置。 |
| 下坠 | 车顶锁环退场，地上出现固定大小的伤害边界，环内的暗影逐渐扩大。陨石拖着火焰和烟从上方落下。锁定后，目标可以开出范围，其他车辆也可能驶入。 |
| 落地 | 以落点为圆心结算一次伤害和向外的冲击，范围包括发射者。三股橙金焰云从爆心向外鼓起，一道贴地音爆沿钢板扩至预警边界，余烟随后上浮消散。各车沿用现有减伤、抗冲击规则。 |
| 自然到期 | 球化为向外飘散的余烬。场上车辆可以从旁经过，没有伤害或落击。 |

**首测调参**。下列字段均在同一份球型 DA。

| 在 DA 中查找 | 首版值 | 改动后的感觉 |
| --- | --- | --- |
| `Meteor → Tracking Seconds` | 1.6 s | 三瓣环从张开到收拢的时间，也是落点还能跟车移动的时间。 |
| `Meteor → Fall Seconds` | 1.0 s | 落点固定后至命中的躲避窗口，阴影、烟焰与陨石共同使用。 |
| `Meteor → Damage Radius` | 500 cm 半径 | 地面外圈和实际伤害取同一个半径。车身碰撞体触及范围即可受击。 |
| `Meteor → Untargeted Distance` | 1200 cm | 无合格对手时，从发射车辆中心沿车头水平朝向量出的首选距离，可填 100–10000 cm。有效距离还会为爆炸半径和车身留出空间。发射后自行驶入预警圈仍会受击。 |
| `Gameplay → Damage → Vehicle Hit Damage` | 25 HP | 正式资产使用固定伤害。 |
| `Gameplay → Impact → Tier / Push Preset` | Heavy / Standard | 分别控制冲击档位和横向推开距离。 |
| `Meteor → Tracking Marker → Marker Size / Marker Closed Scale / Marker Lift` | 96 / 0.78 / 38 | 图标展开时为 96 × 96 个屏幕逻辑单位。追踪结束时外环收至约 75，中心陨石大小固定。第三项控制车顶标识上方的间距。 |
| `Meteor → Flight FX → Sky Height / Falling Rock Radius / Trail Length` | 6000 cm / 110 cm / 3.2 | 调上下飞行的高度、下落岩体的尺寸和烟焰尾长。另有 `Rise Seconds` 和 `Smoke Color`。 |
| `Meteor → Flight FX → Smoke Lifetime / Smoke Spread` | 0.46 s / 1.15 | 前者控制一团烟从排出到散尽的时间，后者控制向两侧翻卷、散开的幅度，以岩体半径为单位。`Trail Length` 限制可见烟尾长度。 |
| `Meteor → Flight FX → Rock Spin Rate` | 320°/s | 调整下坠岩体绕斜轴翻转的速度。设为 0 可以单独观察烟和气流。 |
| `Meteor → Flight FX → Airflow Strength / Airflow Speed` | 0.72 / 2.4 周期每秒 | 前者调迎风面两侧风痕的可见度，后者调亮纹和扰动向尾部推进的速度。 |
| `Meteor → Impact FX → Cloud Coverage / Cloud Lift` | 0.88 / 0.42 | 调云团的横向覆盖和向上鼓起的高度，均相对伤害半径。范围分别为 0.35–0.94 和 0–0.65。 |
| `Meteor → Impact FX → Cloud Expansion Fraction` | 0.62 | 主体膨胀占 `Impact Seconds` 的比例，可填 0.20–0.95。值越大，越能看清火团从小到大。当前为约 0.50 秒。 |
| `Meteor → Impact FX → Cloud Tail Seconds` | 0.44 s | 外圈演出结束后，余烟额外上浮和变薄的时间，可填 0–1.5 秒。 |
| `Meteor → Impact FX → Cloud Curl` | 1.0 | 0–2，控制云团在向外膨胀时向前翻起的角度。设为 0 可单看膨胀。 |
| `Meteor → Impact FX → Cloud Tint / Cooled Smoke Color` | 白色乘色 / 暗紫灰 | 前者给受光火面染色，后者调火焰冷却后的烟色。 |
| `Meteor → Impact FX → Shock Ring Width / Shock Ring Intensity` | 23 cm / 1.25 | 调音爆的基础笔触宽度与亮度。宽度可填 2–100 cm，从外沿向内加粗。基础宽度会限制在半径的 0.3%–24%，再沿圆周收尖和展宽。亮度 0–4，设为 0 可单看焰云。 |
| `Meteor → Impact FX → Shock Accent Color` | 浅青 | 控制外环迎光弧段的冷色，暖色取球型的 `Corona Color`。 |
| `Lifetime → Free Life Seconds / Warning Seconds` | 20 s / 3 s | 累计可争抢时间，以及自然消散前的提醒。 |
| `Effects → Arrival Seconds / Expiration Seconds / Impact Seconds` | 0.62 s / 0.65 s / 0.80 s | 分别调生成聚合、余烬消散和命中收尾。球面颜色、流动速度与发光也在此组。 |

美术资产在 `/Game/Assets/FX/SpecialBalls/Meteor`。原生控件会绘制 `Marker Core` 与 `Marker Jaws` 两层纹理。若团队改做 Widget Blueprint，可以在 `Marker Widget Class` 引用派生自 `HALMeteorMarkerWidget` 的控件，读取 `Ball Type`、`Tracking Progress` 或响应 `Receive Tracking Progress`。进度由同一次服务器发射的起始时间计算。

四人联机回归入口为 `Tools/TestGameFlowLoop.ps1 -Profile Exhibition -VerifyMeteorBall`。第一局验证锁定、实际落地伤害与主球共存，第二局验证自然到期。

<a id="meteor-untargeted"></a>

**11 月 24 日空场发射复测。** 在 `MVPB_Match` 单人测试模式使用正常拾球和发射按键。正对空地时，预警圈从起飞起可见，实测落点距车约 12.0 米，随后完成下坠和爆炸。贴边朝外发射时球无伤害消散，同一辆车随即成功拾取常驻主球。两次发射后车辆均为 100 HP。首选距离在 `DA_Ball_Meteor → Meteor → Untargeted Distance` 调节。

特殊球自动化 9 项通过。三个新增用例覆盖空地、前方地面较短和无法安全落击，并验证后来驶入预警圈的车辆可受到一次正常伤害。另在本机一主三客、主机出站模拟 80 ms 延迟下复测原有追踪，目标从 100 降至 75 HP，第二局自由到期，主球两局均在场。单车实测和联机报告在 `Saved/MeteorBall/NoTarget_20261124/{Solo.json,SpecialBall/index.json,Network/Summary.json}`。

为验证打包所用代码，还单独构建了 Win64 Development 游戏目标。构建命令及结果记录在 `Saved/MeteorBall/NoTarget_20261124/Game.log`。

```powershell
& 'D:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' HAL_Future_Creation Win64 Development '-Project=D:/HAL_Future_Creation/HAL_Future_Creation.uproject' -WaitMutex -NoHotReload -NoLiveCoding '-Log=D:/HAL_Future_Creation/Saved/MeteorBall/NoTarget_20261124/Game.log'
```

下坠烟团留在陨石经过的位置，在那里翻卷、膨胀、飘散。岩体绕斜轴自转，迎风面两侧有细亮风痕向上掠过。要让烟留久一点，先调 `Smoke Lifetime`。若尾端已经达到可见长度，再增大 `Trail Length`。这次实拍中，岩体在约 0.1 秒里下降了 1.52 m，同一团烟在这段时间上浮了 0.18 m。[下坠实拍动图](Art/MeteorBall/MeteorBall_Descent.webp) · [落地爆炸选型草图](Art/MeteorBall/ImpactConcepts/README.md)

在 `MVPB_Match` 的 D3D12 实射中，木桩从 1000 降至 975 HP，独立生成的自由球在 20.0 秒自然消散。四进程两局的记录在 `Saved/MeteorBall/FinalNetwork`，画面原帧在 `Saved/MeteorBall/ReleaseView`。[实拍对照](Art/MeteorBall/MeteorBall_InMatch.webp)

### 逆卷焰云与外围音爆

落地先闪出金白爆心，三股火团向外撑开并向上鼓起，较小的烟团从侧面接续展开。每团的凸面陆续鼓起一次，三档卡通明暗沿圆弧过渡。云顶先冷却，靠近爆心和底部的余火稍后转暗。烟团继续上浮，轮廓薄处先露出地面，厚处稍后散尽。最外侧的一道贴地音爆在约 0.35 秒后扫到 5 米边界，地面演出长 0.80 秒。余烟可延续至落地后 1.24 秒。伤害在落地瞬间结算一次。

要放慢膨胀，先增大 `Cloud Expansion Fraction`。要单独延长烟尾，增大 `Cloud Tail Seconds`。宽度调 `Cloud Coverage`，高度调 `Cloud Lift`。手工制作其它陨石球型时，可以在 `Impact Volume Mesh` 引用同一个闭合网格 `SM_Meteor_ImpactVolume`，槽 0 使用 `M_Meteor_ImpactVolume`。[新旧焰云实射对照](Art/MeteorBall/MeteorBall_CloudExpansion.webp) · [B、C 原草图](Art/MeteorBall/ImpactConcepts/README.md)

11 月 23 日在 `MVPB_Match` 从拾球开始实射，驾驶、固定近景和斜侧机位各录了 73 帧。三个机位的木桩均从 1000 降至 975 HP，只扣一次。另在 13 个时刻暂停比赛，对照同一帧开关焰云后的像素差。落地 0.54 秒时差值总量达到采样峰值，0.84 秒剩约 39%，1.05 秒剩约 6%，1.12 秒降到与背景波动相当的 1% 以内。原始结果在 `Saved/MeteorBall/ReleaseCloud/{Close,Side,Driving,ClosePairs,VisibleCloud.json}`。[驾驶与斜侧实拍](Art/MeteorBall/MeteorBall_CloudInMatch.webp)

11 月 22 日首版记录了驾驶镜头与近景各 53 帧，木桩均从 1000 降至 975 HP。当时另用正交镜头对照开关音爆的画面。正式 500 cm 与对照 300 cm 两档各读 48 个方向，外沿半径中位数分别为 499.5 cm、299.7 cm。原始数据在 `Saved/MeteorBall/RollingImpact`。[首版实射](Art/MeteorBall/MeteorBall_RollingImpact.webp)

## 黑洞球

在 Development 的场地中驾驶车辆，朝空地执行控制台命令 `HALSpawnBlackHoleBall`。也可以从 Place Actors 将原生 `HALBlackHoleBall` 放进测试图。选用的球型为 `/Game/Data/Balls/DA_Ball_BlackHole`。

| 接触或退场原因 | 结果 |
| --- | --- |
| 正式发射后直击存活的对手车辆 | 按最近一次发射归属结算一次直接伤害，首测 25 HP。在命中点下方的行车面展开黑洞。 |
| 正式发射后撞到实心墙、机关实体或通电电幕 | 球消耗一次，黑洞出现在来球侧地面。附近车辆只受黑洞牵引。 |
| 普通地板、可行驶斜坡、其他球或发射者 | 继续通用球状态流程。 |
| 自由寿命到期 | 播放紫金粒子向中心收束的消散。已经开始的比赛结束时会一起回收场上剩余的球和球生成的力场。 |

力场直接生成 `BP_HALBlackHole`，沿用场地机关的向心吸引、绕转、抓地力变化、转向对抗和周期强拉。进入范围的发射者也会受到牵引。吸引只作用于车辆，伤害只来自最初的直接命中。场地机关使用平面范围和高度容差，墙边同样沿用这套判定。

| 在球型 DA 中调整 | 首测值 | 在当前场地黑洞上的实际效果 |
| --- | --- | --- |
| **Black Hole → Radius Scale** | 0.50 | 水平半径 350 cm，软化中心按比例缩小。 |
| **Black Hole → Strength Scale** | 0.45 | 向心加速度 1350 cm/s²、切向加速度 270 cm/s²、强拉 315 cm/s。进场减速、抓地损失与油门偏向同步减弱。 |
| **Black Hole → Duration Scale** | 0.40 | 扩张 0.8 秒，维持 4 秒，从命中算起约 2.8 秒触发一次强拉。 |
| **Effects → Surface Flow Speed、Glow、Core / Current / Corona Color** | 1.15、1.12 和紫色系 | 调球面旋速、发光强度、暗核和旋臂配色。球面旋向读取同一场地黑洞的旋向。 |

球体显示直径和碰撞直径均为 200 cm，与爆炸球一致。直击伤害调 `Gameplay → Damage → Vehicle Hit Damage`。`Free Life Seconds` 为 20 秒，最后 3 秒提示，自由、持球和有效飞行的计时见上表。球型引用原机关蓝图与退场 Niagara，场地黑洞调整后，球生成的力场会自动套用上面的倍率。联网复测可向 `Tools/TestGameFlowLoop.ps1` 传入 `-Profile Exhibition -VerifyBlackHoleBall`。

实射中，`MVPB_Match` 木桩从 1000 降至 975 HP，力场落点 Z 为 −198.27 cm，距行车面 2 cm。第二颗在自由状态经过 20 秒后自然收束，生成的力场数为零。`Fred_TestScene` 实墙测试生成一个黑洞，中心距墙面朝来球侧 15 cm。查看服务器的 `LogHALBlackHoleBall` 可以核对发射者、实际接触对象、命中位置、落地位置和扣血量。[比赛图实拍](Art/BlackHoleBall/BlackHoleBall_InMatch.webp)

## 首颗球型 · 爆炸球

**球型方向。** 外形像一颗偏暗的小太阳，表面有缓慢流动的日纹。正式发射后命中对手车辆、墙体或机关时引爆，即使爆心附近没有车辆，这颗球也已用完。范围内的对手车辆各受到一次伤害和一次从爆心向外的冲击。特效的爆心和半径与玩法一致。

### 已确认的接触规则

| 场上情形 | 判定结果 |
| --- | --- |
| 贴地滚动、滑过坡面或接触另一颗球 | 延续飞行。对手车、普通实心墙、弹簧拳实体、电墙立柱都可引爆已发射的球。通电的电幕同样可以引爆，服务器沿球的运动路径检查高速穿越。减速区、机关预警区和未通电的电场允许球通过。 |
| 贴身发射、直击一车、同时擦到墙 | 仅在已发射状态引爆，忽略本次发射者自身的接触。以首次有效接触点为爆心只结算一场爆炸。直击车辆也只计一次爆炸伤害与冲击。 |
| 多辆车贴近爆心，或在空中擦过边界 | 爆炸瞬间按三维球形范围检查车身碰撞体的最近点。每辆有效对手车沿各自的“爆心到车辆”水平方向被推出。发射者免受自身爆炸，免疫、冲击降档与持球脱落沿用通用结算。首版范围内固定伤害、固定冲击档位。 |
| 车与爆心之间隔着实心墙 | 实心墙挡住爆风。球直接撞墙时，从接触面的来球一侧向车身最近点检查遮挡，因此墙前可以溅射，墙后可以躲避。通电电幕本身只负责触发，电墙的实体立柱可以提供掩护。 |

### 游戏中的视觉演出

实心球直径为 2 米，车、墙和机关按同尺寸的球形碰撞体判定。暗红球面上有流动的橙红日纹，外侧是常驻的橙金日冕。日冕随球移动，在自由、持有、飞行时都可见，只参与显示。爆炸另按爆心周围 5 米半径结算。

| 时刻 | 玩家看到的动作 |
| --- | --- |
| 生成 | 几道弧形火流从外向内汇聚，勾出球体轮廓，表面日纹开始流动。 |
| 自然消散 | 临期时日纹间歇变暗，到期后日冕向内收缩，留下少量熄灭的余烬。 |
| 命中爆炸 | 爆心冒出一小团橙红烟云，三道长短不同的日珥向外拱起，在末端卷回短弧形火纹。最外圈在结算瞬间短暂亮起，随后一道断续的冲击波沿地板展开。演出使用服务器给出的爆心和半径，遇实心墙时沿迎球侧展开。 |

### 投放与调试

| 控制台命令 | 生成物 | 如何看见命中效果 |
| --- | --- | --- |
| `HALSpawnExplosiveBall` | 可持有、可发射的暗太阳爆炸球。成功日志含 `Class=HALExplosiveBall` | 空手用车头靠近拾球，朝木桩或实心墙按发射键。命中时橙金色冲击波向外展开。一直放在地上则累计 20 秒后自然消散。 |
| `HALSpawnBomb` | 定时场地炸弹。成功日志含 `Spawned=HALBombActor` 和 `Fuse=3.00s` | 当前默认配置生成即开始倒数，届时以调试线框显示爆炸范围。 |

1. 在 `Fred_TestScene` 或已经开赛的 `MVPB_Match`，控制车辆后打开控制台，执行 `HALSpawnExplosiveBall`。Development 版的主机或客户端均可请求，服务器在这辆车的前方放一颗球。前方空间被占时，换个位置再试。
2. 要固定放置，在 **Place Actors** 搜索 **HAL Explosive Ball** 并拖入关卡，使球底略高于实际行车面。正式比赛开局倒计时期间，预置球隐在摆放点，车辆可以穿过。进入 Playing 时才播放生成特效、恢复碰撞并开始计时。每放一颗可使用一次，后续投放次数和时机由关卡决定。
3. 打开内容浏览器中的 `/Game/Data/Balls/DA_Ball_Explosive`。第一次手测可以让两辆目标车分别停在同一堵墙两侧，再从墙前发射。实际球体、命中后最外圈、被推出的车辆和墙后的车辆可以在同一镜头里比对。

| 想调什么 | 同一份球型 DA 中的位置 | 正式球型的设置 |
| --- | --- | --- |
| 球本体的命中尺寸和可见尺寸 | Physics → Radius、Visual → Relative Transform → Scale | 碰撞半径 100 cm，100 cm 直径的 Sphere 使用等比缩放 (2, 2, 2) |
| 自由球能留多久、提前多久提示 | Lifetime → Free Life Seconds / Warning Seconds | 20 秒 / 3 秒 |
| 爆炸可打到多远 | Explosion → Explosion Radius | 500 cm，三维半径，同时控制画面外缘 |
| 每车扣血和冲击 | Gameplay → Damage → Vehicle Hit Damage，以及 Gameplay → Impact → Tier / Push Preset | 25 HP、Heavy、Standard |
| 生成、自然消散、命中闪光的时长 | Effects → Arrival Seconds / Expiration Seconds / Impact Seconds | 0.52 / 0.48 / 0.54 秒。命中火弧有独立的余焰时间 |
| 球面流动和色阶 | Effects → Surface Flow Speed / Glow / Core Color / Current Color / Corona Color | 流速 0.65，亮度 1.65，暗红、橙红、金色 |
| 常驻日冕的宽度和亮度 | Effects → Body Corona → Body Corona Width / Body Corona Glow | 0.30 / 2.4。宽度按球半径的比例计算，0.30 对应最多 30 cm。两项的编辑范围分别为 0～1、0～8。 |
| 生成和消散的外圈大小 | Effects → Corona Size | 球半径的 2.6 倍 |
| 日珥的拱高 | Explosion → Effects → Prominence Height | 0.50，可调 0.15～0.90，按溅射半径的比例设置 |
| 最宽一道火弧的宽度 | Explosion → Effects → Prominence Width | 0.29，可调 0.04～0.35，按溅射半径的比例设置 |
| 日珥内部火纹的流速 | Explosion → Effects → Prominence Flow Speed | 1.35 条弧长/秒，可调 0～5 |
| 日珥内部火纹的对比度 | Explosion → Effects → Prominence Flow Strength | 0.70，可调 0～1 |
| 火弧在命中闪光结束后还能留多久 | Explosion → Effects → Prominence Tail Seconds | 额外 0.26 秒，可调 0～1.5 秒。与 0.54 秒的闪光合计 0.80 秒 |
| 爆心烟团的大小 | Explosion → Effects → Core Smoke Size | 0.27，可调 0～0.45。设为 0 可单看三道日珥 |
| 日珥外围的短火星数量 | Explosion → Effects → Flare Count | 12，可调 4～20。三道主火弧独立绘制 |

爆炸球使用 `/Game/Assets/FX/SpecialBalls/M_ExplosiveBall_DarkSun` 绘制实心球面。同目录的 `M_ExplosiveBall_BodyCorona` 绘制常驻日冕，`M_SpecialBall_Corona` 绘制生成、自然消散、冲击波和小火星，`M_ExplosiveBall_Eruption` 负责命中时的日珥、烟云和落点火纹。球型 DA 的 `Explosion → Effects → Eruption Material` 引用最后这份材质。已有自定义球型可在这里选用它。

### 命中爆炸的美术候选

下面四张是以正式场地相同机位制作的卡渲概念图。用于挑选命中瞬间的轮廓和分色，实际演出仍以服务器给出的爆心、半径和墙体遮挡为准。共同的绘制要求是保留地面与车辆的辨识度，使用暗红、橙红、奶金三档主色。[四款并排对照](Art/ExplosiveBall/Comparison.jpg) · [原图与绘制提示](Art/ExplosiveBall/Prompts.md)

| 候选 | 命中瞬间 |
| --- | --- |
| [A · 日珥爆发](Art/ExplosiveBall/A-SolarLoops.png) | 三道橙金日珥从亮核翻卷，拱形之间可以看到地面，低位冲击波扫出范围。 |
| [B · 漫画爆云](Art/ExplosiveBall/B-ComicPuffs.png) | 圆润的烟团互相挤开，亮面朝外，底部留暗。四个方案中团块感最强。 |
| [C · 日蚀冲击](Art/ExplosiveBall/C-EclipseShock.png) | 暗色太阳留一弯亮边，宽扁的金色冲击波贴地展开，中央有大片留白。 |
| [D · 彗焰甩尾](Art/ExplosiveBall/D-CometBrush.png) | 几道宽笔触从中心向外甩，弧线有相同旋向，长短错开，形成明显的速度感。 |

### 火弧回卷与消散

命中约 0.26 秒后，三道火弧陆续向内回卷。暗面先透出地板，弧面上弯曲的亮纹继续向落点流动，逐渐转成铜橙色余焰。旧版在约 0.40 秒时能看见贯穿弧身的截面。新版同一时刻仍能看清完整的弧形，之后各段先后熄灭。[正式赛场同机位实射对照](Art/ExplosiveBall/SolarProminence_TailComparison.webp)

想让火弧多留一会儿，调球型 DA 的 **Explosion → Effects → Prominence Tail Seconds**。它给回卷与消散额外留出时间。设为 0 时，火弧会在基础的 0.54 秒内完成消散。只想让弧面笔触走得更快或更慢，调 **Prominence Flow Speed**。命中伤害在接触瞬间结算。

D3D12 在冲击环结束后单独采样火弧。0.67 秒时，外围火弧的暖色信号约为 0.54 秒时的 8%。到 0.77 秒，余焰已低于检测门槛，0.80 秒关闭演出时没有可见跳变。固定姿态的弧面流纹、5 米外缘与实心墙遮挡检查也通过。正式场地的同机位实射中，木桩从 1000 降至 975 HP。

本轮使用 UE 5.8.3。Editor 统一构建记录为 `Saved/BuildTool/BuildAndLaunch_20261122_194355_222_4f41f4/Result.json`。独立 Game、D3D12、近景实射、玩法和一主三客两局的记录分别在 `Saved/ExplosiveBall/ProminenceFade/` 下的 `Game.log`、`VisualFinal/index.json`、`AfterFinal/MVPB_Match/result.json`、`Gameplay/index.json` 与 `FourPlayers/Summary.json`。玩法共 277 项通过，其中两项既有换车用例带警告。四个联机进程均通过且无测试警告。

### 近景拱高与日珥内部流纹

日珥是从爆心向上拱起的三维火弧。靠近弧顶时，透视会使弧顶相对地面的落点显得更大。正式场地的近景对照沿用了驾驶镜头的 90° FOV 和 −80° 俯角，两次拍摄距离都是 10.5 米。将球型的 `Prominence Height` 从 0.67 调至 0.50 后，在 5 米溅射半径下，名义拱高从约 3.35 米降至 2.5 米。三道弧沿用原先的伸展、回卷与消散时序，贴地的落点和冲击波仍按原先的半径展开。[同机位实射对照](Art/ExplosiveBall/SolarProminence_FlowComparison.webp)

流动绘制在火弧表面。暗面有橙金色火纹，亮面有铜橙色暗纹。两者都从爆心沿弧身向落点移动。以下两个控件位于同一份球型 DA 的 `Explosion → Effects` 下。

- `Prominence Flow Speed` 控制火纹沿弧身前进的速度。调到 0 可以定住流纹，单看弧线如何展开与回卷。
- `Prominence Flow Strength` 控制流纹与底色的对比。调到 0 可看到原来的三色笔触。想调暗太阳球表面的日纹速度，使用 `Effects → Surface Flow Speed`。

2026-11-22 已通过 Unreal 保存 `M_ExplosiveBall_Eruption` 和 `DA_Ball_Explosive`。重新启动的 UE 5.8.3 进程分别以固定近景和原车驾驶镜头进入 `MVPB_Match`，均读回上述正式值，实射命中木桩后 HP 从 1000 降至 975。近景对照动图取自实际录帧，按游戏时间配对。

D3D12 专项固定火弧展开姿态，只改变流纹相位。在 320 × 320 的同机位读回中，爆心以外有 4,891 个像素明显变色。独立的深度图记录了 13,079 个前景像素，两次的覆盖差为 0。专项同时复核了爆心烟团、实心墙遮挡和 5 米外缘。本机报告为 `Saved/ExplosiveBall/FlowTune/VisualFinal/index.json`，两次实射记录在同目录的 `After/MVPB_Match/result.json` 与 `GameView/MVPB_Match/result.json`。

Editor 统一构建记录为 `Saved/BuildTool/BuildAndLaunch_20261122_192818_117_71f2d9/Result.json`。玩法自动化结果在 `Saved/ExplosiveBall/FlowTune/Gameplay/index.json`，共 275 项通过、另 2 项既有换车用例带警告通过。

为验证两项球型参数可在独立 Win64 Development 游戏中链接，已在工程根目录运行以下命令，构建通过。

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' HAL_Future_Creation Win64 Development '-Project=D:/HAL_Future_Creation/HAL_Future_Creation.uproject' -WaitMutex -NoHotReload -NoLiveCoding '-Log=D:/HAL_Future_Creation/Saved/ExplosiveBall/FlowTune/Game.log'
```

### 日珥爆发与爆心烟云

选用 A 的三道日珥。两道宽弧向两侧翻卷，中间一道较短，弧面使用暗红内侧、橙红中间色和奶金亮缘。末端稍后抵达，在各自的落点扫出两笔火纹。B 的四瓣烟云挤在爆心，在火弧回收时碎散。开阔地可以透过火弧看见地板，靠实心墙时，三道弧朝可以受击的一侧张开。

美术在 `DA_Ball_Explosive → Explosion → Effects` 调试。可以在同一堵墙前试两次，一次把 `Core Smoke Size` 设为 0 看火弧，再恢复 0.27 看混合效果。两次均可用同一发射位和 5 米溅射半径比较轮廓。

比赛伤害由服务器在命中瞬间结算一次。正式联机比赛通过 GameState 上常驻的 `UHALMatchBallLifecycle` 可靠广播这次命中的球型、爆心、表面法线、半径和服务器开始时间。每端生成一个本地演出 Actor，在球体销毁后继续播放。日珥、落点与烟团网格构造一次，按服务器时间在材质中展开和收束，实心遮挡使用球爆风的障碍查询。

在 `MVPB_Match` 未保存的单人驾驶预览中，测试车用车头拾球，正式发射命中技能木桩，HP 从 1000 降至 975。回放按记录的游戏时间合成，末帧停留以便循环。另在 `Fred_TestScene` 近射实心墙核对迎球侧的火弧。两次实射的物理半径为 100 cm，爆炸半径为 500 cm。[正式比赛场地回放](Art/ExplosiveBall/SolarEruption_Gameplay.webp) · [日珥展开时的原始帧](Art/ExplosiveBall/SolarEruption_Gameplay.png) · [墙边命中原始帧](Art/ExplosiveBall/SolarEruption_Wall.png)

本轮验证使用 UE 5.8.3 CL58210709。统一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup` 成功，记录为 `Saved/BuildTool/BuildAndLaunch_20261122_183736_714_b0e194/Result.json`。独立 Win64 Development 游戏目标也已编译，以确认球型资产用到的运行时模块可链接。在工程根目录复测的命令如下。

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' HAL_Future_Creation Win64 Development '-Project=D:/HAL_Future_Creation/HAL_Future_Creation.uproject' -WaitMutex -NoHotReload -NoLiveCoding '-Log=D:/HAL_Future_Creation/Saved/ExplosiveBall/Eruption/GameVerified.log'
```

本机报告在 `Saved/ExplosiveBall/Eruption/`。`GameplayVerified/index.json` 记录 277 项玩法自动化全部通过，其中两项既有换车测试带警告。`VisualVerified/index.json` 的 D3D12 专项通过，验证了实心墙遮挡和 5 米半径外缘。将烟量从 0.27 切到 0 再读回同一帧，爆心区有 2,424 个像素变化，外围火弧轮廓有 0 个像素变化。

旧的短时特效 Actor 在本机四进程复测中两次出现个别客户端已收到扣血、却未观察到爆炸。改用 GameState 的可靠广播后，独立重启两次进行一主三客复测，每次连续开两局，主机出站模拟延迟 80 ms。首局四端均记录一次本地命中演出，目标从 100 降至 75 HP。次局自然消散，目标保持 100 HP。两局各有一颗主球。两次的四端均通过且无警告，记录分别为 `ReliableRepeat/Summary.json` 和 `FourPlayersVerified/Summary.json`。校园四台实机的打包画面待团队验收。

### 撞墙修复与两米球复测

在 `Fred_TestScene` 的 `Wall` 上，旧版实射的物理接触点为 X = 2306.29 cm，迎球面在 X = 2290 cm。以墙内的点为爆心检查遮挡时，所有方向都在起点被挡住，当帧冲击波网格的包围盒为零。此前的直击木桩、靠墙引爆和理想墙面接触样例均未覆盖这次穿入墙面的回调。

服务器现在在实际撞到的阻挡组件上，从来球侧查找表面。墙前溅射、墙后遮挡以及各端播放的特效使用同一个校准后的爆心。两次测试场实射命中 `Wall` 和 `Wall4`，后者的回调点为 Y = 3104.93 cm，最终爆心为墙面 Y = 3090.00 cm。正式场 `MVPB_Match` 也完成了拾球、发射及撞击场边 `Path` 的实射。[墙边实射画面](Art/ExplosiveBall/WallImpact_Gameplay.png)

正式球型 DA 载入后实测物理半径 100 cm，球体缩放 (2, 2, 2)，溅射半径 500 cm。D3D12 关闭 Bloom 后，在 320 × 320 的测试帧中仍读到球面轮廓以外 15,153 个暖色日冕像素。生成、自然消散、命中、实心墙遮挡和爆风半径外裁剪的渲染测试均通过。[正式赛场球体画面](Art/ExplosiveBall/Ball_Gameplay.png)

本轮在 UE 5.8.3 CL58210709 上完成 Win64 Development 的 Editor 和独立 Game 编译。Editor 记录为 `Saved/BuildTool/BuildAndLaunch_20261122_172556_865_59f237/Result.json`。其余证据在 `Saved/ExplosiveBall/WallReview/`，其中 `Game.log` 为独立构建日志，`Before/`、`Final/`、`Close/` 保存实射截图与物理坐标，`Gameplay/index.json` 记录 277 项通过（两项既有换车用例带警告），`Visual/index.json` 为渲染报告。`FourPlayers/Summary.json` 记录正式图一主三客两局通过，四端均观测到首局命中后 75 HP、次局自然消散后 100 HP。两局常驻主球数量均为 1。

为核对独立游戏可执行文件，在项目根目录使用了以下命令。

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' HAL_Future_Creation Win64 Development '-Project=D:\HAL_Future_Creation\HAL_Future_Creation.uproject' -WaitMutex -NoHotReload -NoLiveCoding '-Log=D:\HAL_Future_Creation\Saved\ExplosiveBall\WallReview\Game.log'
```

### 程序与特效交接

- 新球型继承 `AHALSpecialBall` 与 `UHALSpecialBallDefinition`。基类处理自由寿命、状态切换、生成、临期提示与回收。注册的比赛圆场沿用主球的边界和越界宽限。正式发射使用同一份攻击快照。球型只需要定义自己的有效接触和消耗结果。
- 爆炸球在服务器锁定第一次有效接触，然后用这一发的攻击 ID 对每辆车结算一次。伤害和抗性经过 CombatResolver / GAS，冲击交给车辆的物理组件。普通炸弹和爆炸球共用三维范围及径向冲击查询，各自保留引爆、自伤与掩体规则。[既有炸弹规则](MVPB_Bomb_Rebuild_CN_v0.1.md)
- 生成与自然消散使用短时复制 Actor。正式比赛中的命中由常驻比赛组件可靠广播，各端生成本地演出 Actor。球消失后，三种演出都可以独立播完。美术可在同一个 DA 调整配色和节奏。新资产的 Editor 生成入口在 `Tools/SpecialBall/CreateExplosiveBallAssets.py`，重复运行会保留已保存的调参。

### 2026-11-22 验证与复测

- 在 UE 5.8.3 CL58210709、Win64 Development 下，项目统一入口 `BuildAndLaunch.cmd -NoLaunch -NoPopup` 编译 Editor 成功，记录在 `Saved/BuildTool/BuildAndLaunch_20261122_152915_040_6f1b18/Result.json`。为同时验证独立游戏可执行文件，另编译 Game，退出码为 0。其命令见下方，日志在 `Saved/ExplosiveBall/Game_StagingFinal.log`。
- `HAL.FutureCreation` 共 276 项通过，其中两项既有换车用例带警告。特殊球三项均无警告，覆盖持球、掉球、换手、发射、去重、直击与溅射、墙体与通电电幕先后顺序、自然到期、开赛等待、越界及结算回收。普通球和爆炸球的开发控制台指令也通过了前方被占及成功生成的检查。报告位于 `Saved/ExplosiveBall/Gameplay_Final/index.json`。
- D3D12 从俯视相机分别读回球体、生成、自然消散、无遮挡爆炸和有墙爆炸。测试确认完整外缘到达 DA 半径，有墙的一侧被截断，半径外及墙后测试区域均为 0 个特效像素。五张图在 `Saved/ExplosiveBall/Visual/`，报告在 `Saved/ExplosiveBall/Visual_Final/index.json`。
- 正式图 `MVPB_Match` 的本机一主三客在主机模拟 80 ms 出站延迟下连续完成两局。首局真实发射击中目标，四端观察到生成、爆炸及目标从 100 降到 75 HP。第二局四端观察到生成、自然消散和目标保持 100 HP。两局主球数量均为 1，四个进程均退出 0，测试事件无警告。报告在 `Saved/ExplosiveBall/FourPlayers_Final/Summary.json`。校园四机打包游玩及正常比赛镜头下的美术验收留待团队进行。
- 另以 D3D12 的玩家行车镜头核对了控制台实射。分别在 `Fred_TestScene` 和 `MVPB_Match` 的单人驾驶预览中执行 `HALSpawnExplosiveBall`，用车头拾球、发射击中技能木桩，两次均由 1000 降至 975 HP。钢板地面上能看到橙红色自由球，命中时可见橙金色日焰和外环。两组原始截图与 `result.json` 在 `Saved/ExplosiveBall/CommandView/<地图名>/`。

独立 Game 构建命令在项目根目录执行。

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' HAL_Future_Creation Win64 Development '-Project=D:\HAL_Future_Creation\HAL_Future_Creation.uproject' -WaitMutex -NoHotReload -NoLiveCoding '-Log=D:\HAL_Future_Creation\Saved\ExplosiveBall\Game_StagingFinal.log'
```

用以下命令复测正式图上的两个回合。首轮实际发射命中，第二轮等自由球自然消散。开关可与主球回场检查一起使用。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\TestGameFlowLoop.ps1 -EngineRoot 'D:\Program Files\Epic Games\UE_5.8' -Profile Exhibition -Port 7959 -VerifyPrimaryBall -VerifyExplosiveBall
```
