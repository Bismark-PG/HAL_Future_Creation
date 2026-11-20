# Car09 消防车 · 水炮

Car09 按一次主技能键便向车头持续喷水。按住油门可顶着后坐力缓慢追击，松油门可边喷边退，倒车输入和水炮后坐同向。主动转动实际车头可以扫射。水柱内部的亮纹、命中端的水花和车上的消防双音警报共同提示喷射状态。

## 车辆配置

在 Content Browser 打开 `/Game/Data/Abilities/DA_Ability_Car09_WaterCannon`。这一个 DA 集中控制喷射、伤害、冲击、球速、后坐、水流和声音。`DA_Vehicle_Car_09 → Abilities → Grants[0]` 已绑定原生 `HALWaterCannonAbility`、`Input.Ability.Primary` 和这份 DA。每次成功施放花费 100 点能量，车型当前自然回满时间为 20 秒。

### 在 Fred_TestScene 试车

`Fred_TestScene` 使用 `BP_FredSkillTestGameMode`。进入测试场后，在测试面板选择 Car09，点击 **Apply selected vehicle**，再用主技能键对场内木桩喷水。

2026-11-20 在 UE 5.8.2 接续机读回的 `Test Vehicle Classes` 已包含 `BP_VehiclePawn_Car_09`，本机可直接试车。这份 GameMode 蓝图仍是本地改动。只同步到提交 `342b361` 的设备，如果测试面板里没有 Car09，打开 `BP_FredSkillTestGameMode → Class Defaults → Vehicle Test → Test Vehicle Classes`，添加 `BP_VehiclePawn_Car_09`，然后 **Compile → Save** 并重新进场。

如果用 `SingleVehicleTest`，在 `BP_VehicleTestGameMode` 的同名数组添加这辆车即可。正式房间读取 `DA_VehicleCatalog_Default`，本次核对到正式目录为 Car01、Car02、Car03。

| 想调整的手感或表现 | 技能 DA 参数 | 首测值 |
|---|---|---|
| 持续、射程、水平扇面 | Jet → Duration / Range / Fan Angle | 3 秒 / 800 cm / **总角度** 12° |
| 水柱的垂直命中容差、出水口半宽 | Jet → Half Height / Nozzle Radius | 24 cm / 7 cm |
| 喷口位置与俯角 | Nozzle → Mount Bone / Nozzle Offset / Aim Pitch | Car09 以 `Chassis_jnt` 为基点，偏移 `(211, 0, 95)` cm，分别沿车身前、右、上三轴，喷口朝下 `5°`（填 `-5`） |
| 对同一辆车的脉冲间隔与伤害 | Hits → Hit Interval / Damage Multiplier | 0.5 秒 / 0.20 倍施法者攻击，命中后沿水流方向推开 |
| 目标冲击档位与水平击退 | Hits → Impact Tier / Push Distance | Light / Short，冲击抗性、免疫、受击掉球由通用规则结算 |
| 水柱命中球后的发射速度 | Hits → Ball Launch Speed | 2400 cm/s，水平向当前车头方向发射 |
| 喷空时的后退力度 | Recoil → Recoil Acceleration | 1050 cm/s²，作用在质心 |
| 顶着水压前进时的速度上限 | Recoil → Forward Limit Scale | 常规前进限速的 55%，使用车辆的超速修正逐步减速 |
| 后坐力可把车加速到的倒车速度 | Recoil → Reverse Limit | 1400 cm/s，到达后水炮停止继续增加后退速度 |
| 水柱分色与纹理速度 | Water → Shadow Color / Mid Color / Light Color / Tone Softness / Flow Speed | 深青蓝、亮蓝、近白，过渡 0.11，亮纹 1700 cm/s |
| 开喷到完整射程、停喷到余水消失 | Water → Spray In Seconds / Spray Out Seconds | 0.19 秒 / 0.26 秒，可分别调整 |
| 端部飞溅大小 | Water → Splash Size | 34 cm，设 0 可关闭 |
| 消防警报 | Fire Siren → Siren / Siren Volume / Siren Inner Radius / Siren Falloff | 单声道高低双音循环，音量 0.65，内半径 450 cm，再衰减 2700 cm。更换音效时，Sound Wave 需勾选 Looping，Sound Cue 需配置循环 |

## 命中和状态

- 每辆车首次暴露立即结算。此后按目标独立计时，离开后再进入沿用上次命中的时间。默认 3 秒持续中单车最多吃到 6 次。车身和普通墙体只遮挡自己覆盖的水束，障碍后方没有暴露部位的车可借此躲避。
- 自由球和已经发射的球被喷中后都会得到新的正式发射攻击记录，发射者为消防车。同一颗球持续留在水柱里只触发一次。自己的持球实际被水柱覆盖时，通过原控球组件解除约束并发射。对手的持球继续归对手，喷到持球车时由其实际承受的冲击档位和脱球门槛决定球权。刚掉的球遵循原有的当帧保护和拾取锁。
- 车辆稳定着地时可发动。已发动后，眩晕会封住驾驶输入，水柱、命中、后坐和消防警报继续。短暂腾空仍按车头瞄准，严重翻侧或执行自救时暂停水流与后坐，警报和总计时继续。扶正后恢复剩余时间。淘汰、换车、离开或回合结束会收掉本次喷射。
- 服务器判定扇面的暴露部分、每目标间隔和球权，同时发送各束长度及实际受阻面的接触点和法线。两端按本地画面上的喷口绘制水柱。水边波动，水头分成不同长度的水舌和飞滴。扫过墙角时，被挡的一侧贴着表面溅开，敞开的一侧继续前进。后坐和前进限速通过车辆物理历史传入 Chaos 子步，能随同一帧的眩晕状态回放。
- 开喷时水头从喷口向前推进，停喷时近端先断开，空中的余水继续向前并淡出。翻侧停喷和扶正恢复也使用这组过渡。伤害资格、击球和后坐使用原技能时间，残余水花只参与本地渲染。

## 实车调试

检查水花时，在测试场先对空开喷，再慢慢扫过墙角，最后整股对准木桩或另一辆车。对空能看到参差的水头和漂离的水滴。贴着墙角时，两边的可见长度会分开，亮色水花留在实际受阻面上。若只想加强飞溅，在技能 DA 调 `Water → Splash Size`。要改变开喷或收回的节奏，分别调 `Spray In Seconds` 和 `Spray Out Seconds`。

1. 在练习场选择 Car09，分别在静止、踩油门和倒车时开喷。先独立调 `Recoil Acceleration` 和 `Forward Limit Scale`，再测试倒车退出距离。
2. 对准技能木桩，保持命中，再快速扫开、扫回。用半面墙遮住目标，观察露出部位、水花位置和伤害间隔。用电墙截停消防车，观察眩晕中剩余喷射时间。
3. 用主机和一个客户端分别准备自由球、敌方已发射球和对手持球。观察前两种被水流改成消防车的发射，对手持球按冲击门槛结算。在来车视角与俯视相机同时检查喷口位置、三档水色以及警报距离。

## UE 5.8.3 双端实测

2026-11-20 在 `MVPB_NetTest` 启动两个独立游戏进程，主机出站加入 80 ms 延迟。`HAL.NetworkSmoke.WaterCannon.ClientCastStunRecoilAndBall` 和 `HAL.NetworkSmoke.WaterCannon.DriveDirections` 在 Host 和 Client 各通过 2/2，零测试警告。第一项由客户端施放，两端均观测到新的发射归属、水柱的出现与收回，以及喷射结束后车辆后坐状态清除。最终复测中，主机在施法车眩晕期间记录了 104 帧后坐，最大后退速度 5.57 m/s。

第二项用同一辆车在同一平坦出生点静止起步，每次给足油门或倒车输入 0.85 秒后测量沿车头轴的实际速度。此处是起步手感的对照，极速由车辆、后坐力和地面共同决定。

| 0.85 秒时的速度 | 普通驾驶 | 喷水中 |
|---|---:|---:|
| 全油门向前 | 7.54 m/s | 0.67 m/s |
| 全输入倒车 | 3.48 m/s | 9.05 m/s |

最终双端报告在 `Saved/WaterCannon/Network_20261120_113417`。复测时主机打开 `/Game/Maps/MVPB_NetTest?listen`，客户端连接主机，两端启动参数均加 `-game -HALWaterJetFixture`。两端执行 `Automation RunTests HAL.NetworkSmoke.WaterCannon.`，可以在主机先执行 `NetEmulation.PktLag 80` 复现这组延迟条件。警报音量、水流风格和四人驾驶中的实际辨识度留给团队实玩确认。

### 编译、物理与渲染回归

- UE 5.8.3（CL 58210709）完成 `HAL_Future_CreationEditor` 和 `HAL_Future_Creation` 的 Win64 Development 编译与链接。
- `HAL.FutureCreation.` 在 NullRHI 下通过 263/263。261 项干净通过，另两项既有换车测试记录了车型未配置或出生点被挡的提示。报告在 `Saved/WaterCannon/Final_Native`。
- D3D12 水炮、球权、车辆冲击、移动与相关技能专项通过 27/27，零测试警告。渲染用例对照了不同喷射时刻，以及同一机位下的全长和受阻水柱。三档蓝色均可见，两次取样之间有 8,710 个水柱像素亮度发生变化。报告在 `Saved/WaterCannon/Final_RenderAndCombat`，画面在 `Saved/WaterCannon/Render`。
- 另一次将所有玩法用例和渲染用例串在同一 D3D12 进程时，驱动在 `SkillDummy.SingleVehicleSwapPreservesOnlyNewDummy` 期间报告 `DXGI_ERROR_DRIVER_INTERNAL_ERROR` 并退出。上述玩法全量和真实渲染专项分别复跑通过；该次中断记录在 `Saved/WaterCannon/Full_01.log`。

## UE 5.8.2 接续复测

2026-11-20，接续机从磁盘读回了 Car09 的主技能输入、技能 DA、水柱材质和警报引用。独立 D3D12 进程加载当天已成功构建的 Editor 模块（CL 56702186），通过 `PhysicsHistory`、`ProductionGrantHitsBallsAndOcclusion`、`FlowTonesAndContact` 三项水炮测试，零测试警告。覆盖后坐状态回放、正式车型施放、对车与球的命中、墙体遮挡和水柱画面变化。本次另一次构建在链接时遇到已打开的 Editor 占用 DLL。

本机验证目录为 `Saved/WaterCannon/Handoff_20261120/`。`Focused/index.json` 记录测试结果，`assets.json` 记录资产与选车列表。联机数据见上面的 UE 5.8.3 双进程记录，四台实机的视听验收仍待团队完成。

### 2026-11-20 水流与接触水花重制

UE 5.8.2 的 D3D12 实拍覆盖开喷、全长、收束、完全消失和喷口堵住。相对空场，四个喷射阶段分别改变 5,745、25,464、10,999 和 0 个像素。保持机位、水束长度、材质和时刻相同，开启接触水花后另有 1,703 个像素出现可见差异。测试还把真实可阻挡的网格移过扇面，分别拍摄了只有一侧被截断和全部被截断的画面，并验证实际命中车辆也会产生表面接触点。本机报告为 `Saved/SkillFXReview/Verified/index.json`，画面对照为 `Saved/SkillFXReview/WaterComparison.png`。

双进程用例在主机创建了一个仅主机可查询的阻挡面。设置主机出站延迟 80 ms 后，施法客户端和观察主机均收到了水花位置与朝外的表面法线。击球归属、眩晕中持续后坐和不同踏板输入的回归也通过。两端各 2/2，报告在 `Saved/SkillFXReview/Net_Water_Verified`。

正式水流材质为 `M_WaterCannon_ToonFlow`。如果编辑了内部水纹的着色器源码，通过工程的资产生成脚本传入 Editor 参数 `-RefreshWaterMaterial`，即可单独更新水流材质。[水纹着色器源码](../../Tools/WaterCannon/ToonWater.hlsl) · [资产生成脚本](../../Tools/WaterCannon/CreateWaterCannonAssets.py)
