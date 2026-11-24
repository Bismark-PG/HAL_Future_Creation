# Car12 震荡钉镖

Car12 的主技能有两次操作。第一次按键从车头掷出飞镖，第二次按键引爆已经钉在对手车身上的飞镖。可以等对手转向，让推力对准电墙或争夺中的球再引爆。

## 驾驶与战斗规则

- 满 100 点能量、车辆稳定着地且能施放技能时可以出镖。起手按左转向左回旋，中立或右转向右回旋。飞镖沿出手时的车头方向向前画宽弧，绕到原出手位置的侧后方。施放者可以继续变向驾驶。
- 首个有效车体接触挂镖，挂上时不扣血。飞镖可以命中去程或回程中的车辆，沿途穿过球、施放者和重叠触发区。实体墙截住飞镖，飞空或撞墙均结束本次投掷，能量照常恢复。
- 挂镖后可再次按同一个主技能键引爆。第二次按键不耗能。挂镖满 **5 秒**自动引爆。已腾空的施放者可以手动引爆，眩晕期间只能等待自动引爆或眩晕结束。淘汰、换车、离场和回合结束清除未结算的镖。
- 每名施放者同时拥有一枚飞镖。多人可以钉住同一个目标，每枚分别倒计时、结算一次。爆炸只作用于被钉住的目标。
- 普通命中结算 **10 点基础伤害、Medium 冲击、Long 水平推动**。攻击力与倍率在出镖时锁定，承伤倍率、伤害免疫、冲击抗性和脱球门槛在引爆时由现有 Combat Resolver 判定。练习房沿用房间的免扣血规则。

| 钉入的位置 | 引爆时的水平推力 |
|---|---|
| 前部 | 向车尾 |
| 后部 | 向车头 |
| 左侧 | 向车右侧 |
| 右侧 | 向车左侧 |

命中位置在目标车自身坐标中记录。车辆转过 90°，底盘箭头和引爆推力也随之转过 90°。箭头表示技能希望施加的水平方向，墙、车辆原速度和冲击免疫都可能改变实际位移。

## 初版表现

飞行中的切面双翼会自旋。钉住后，镖身贴在目标中镖的一侧，亮纹按施放者身份色闪烁。引爆时从钉入点向推力方向展开断环和冲击碎线。

施放者听到双音锁定提示。前 0.35 秒会闪过分段扫描线，两翼向目标合拢。目标在屏内时，翼标标注目标编号，底盘空心箭头预告推动方向。目标出屏后，淡线从本车轮廓外延伸到屏边翼标。HUD 显示编号、方位、中镖侧、操作状态和剩余时间。目标被烟幕遮蔽时只保留无方位的挂镖提示，仍可手动引爆。

## 配置与试车

车辆入口是 `/Game/Data/MVP_B/DA_Vehicle_Car_12 → Abilities`。主技能引用原生 `HALBatarangAbility` 和 `/Game/Data/Abilities/DA_Ability_Car12_Batarang`。UE 读回 Car12 的攻击力为 25，动作倍率为 0.4，基础伤害为 10。车辆 DA 的 `Full Recharge Seconds` 为 20 秒。

| 技能 DA 参数 | 初版值或用途 |
|---|---|
| Flight → Forward Reach / Side Reach | 前伸 1050 cm、侧向约 480 cm，以出手点为原点 |
| Flight → Flight Seconds / Sweep Radius | 完整飞行 1.45 秒，接触半径 18 cm |
| Flight → Launch Offset | 相对物理车头的局部偏移，默认向前 12 cm、向上 3 cm |
| Combat → Fuse Seconds | 从钉住起计时，默认 5 秒 |
| Combat → Damage Multiplier / Impact Tier / Push Distance | 动作倍率与独立冲击配置 |
| Presentation → Wing Span / Dart Material / Pin Signal | 默认翼展 68 cm，可换双面材质和锁定音 |

1. 打开 `/Game/Maps/SingleVehicleTest/SingleVehicleTest`，用 **Play Standalone** 单人进入。这张图的 `BP_VehicleTestGameMode` 车型名单已包含 Car12。
2. 在测试面板选 `BP_VehiclePawn_Car_12`，点击 **Apply selected vehicle**。在 **OPPONENT CAR** 选一辆车，点击 **Spawn / replace opponent**。对手默认出现在正前方 700 cm。稍微调整车身，让对手落在车头前方偏右的弧线上，松开转向并出镖。挂镖后观察中镖侧和箭头，短按同一键引爆。第二次可以等 5 秒，对比自动引爆。
3. 双人时，让目标挂镖后原地转向，再驶出画面、进入烟幕。施放者检查屏内箭头、屏外淡线和烟内保密提示。目标玩家应看到插在自己车上的实体镖，追踪 HUD 只出现在施放者屏幕上。在正式比赛中核对单次扣血和击退。

`hal.AbilityLog 1` 可在主机输出日志中查看 `Batarang release`、`Batarang pin`、`Batarang burst`。引爆日志记录实际扣血、有效冲击档和水平推力方向。

若团队习惯用 `Fred_TestScene`，这张图另有自己的名单。在 `BP_FredSkillTestGameMode → Class Defaults → Vehicle Test → Test Vehicle Classes` 点击 **+**，选择 `BP_VehiclePawn_Car_12`，编译并保存即可加入。

## 本机验证 · 2026-11-24

- UE 5.8.2 的 Win64 Development Editor 通过 `BuildAndLaunch.cmd -NoLaunch -NoPopup` 增量构建。单独启动 Editor 再次加载正式资产，从磁盘读回攻击力 25、倍率 0.4、挂镖 5 秒、能量回满 20 秒，以及两张试车图的车型名单。
- 两项 Car12 专项自动化通过，覆盖左右镜像、四个钉入面、撞墙、穿过球与触发区、挂上时零伤害、手动和到期引爆、眩晕、多施放者与回合清理。按项目默认启动图运行 `HAL.FutureCreation.`，285/285 通过，其中两项既有换车用例带预期警告。另一次从引擎空图禁用资产注册缓存启动时，既有地面稳定性用例报告地面无物理体，使用项目默认入口的全量复测通过。报告在 `Saved/Batarang/GameplayFinal/index.json`。
- 本机独立 Listen Server 与客户端进程分别通过 `HAL.NetworkSmoke.Batarang.ClientPinsTurnsAndDetonates`。主机启用 60 ms 出站模拟延迟。客户端钉住主机车辆，主机转向约 90°，进入 Car08 烟幕后客户端再次按键。两端最终 HP 均从 100 降到 90，服务器只结算一次，推力和引爆前的箭头方向一致。两端报告在 `Saved/Batarang/NetworkFinal/`。
- 已检查客户端屏内挂镖、目标出屏、目标进烟三张 1280×720 实拍，分别保存在 `Saved/Screenshots/WindowsEditor/Car12_Pinned.png`、`Car12_Edge.png` 和 `Car12_Smoke.png`。四台真实电脑混战时提示是否足够醒目、锁定音在场地音效中的音量，待团队试玩。
