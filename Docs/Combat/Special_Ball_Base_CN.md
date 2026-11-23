# 一次性特殊球

## 所有球型共用的规则（已确认）

特殊球可以和唯一常驻主球同时上场。每辆车仍只带一颗球。每颗特殊球可以易手、多次发射，最多触发一次球型效果。主球保留自己的回场和相机规则，特殊球消耗、出界或结束本局后由场地回收。下一颗由投放规则决定。[比赛主球生命周期](../Networking/Match_Primary_Ball_Lifecycle_CN.md)

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

## 陨石球

在 Development 的场地中开车面向空地，打开控制台执行 `HALSpawnMeteorBall`，驶近拾球后按普通发射键。单人测试可在 `Fred_TestScene` 放一辆活着的 `BP_SkillTestDummy` 当对手。正式球型位于 `/Game/Data/Balls/DA_Ball_Meteor`。

| 时机 | 场上看到的表现与判定 |
| --- | --- |
| 生成、争夺 | 发亮的碎岩向中心聚成一颗带熔岩裂纹的球。显示直径、物理直径均为 2 m。持球暂停自由寿命，易手沿用剩余时间。 |
| 正式发射 | 球从原处竖直升空。服务器从有有效地面位置的存活对手中等概率选一辆，随后移除可争抢、可碰撞的球体。没有合格对手时保留球，可以稍后再发。 |
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
