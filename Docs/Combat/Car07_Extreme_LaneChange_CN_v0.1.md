# Car07 主动技能：极限换线 v0.1

2026-11-16，接续 `main/9addbe3`，UE **5.8.3-58210709**。用户已授权技能逻辑与初版特效；本文用于团队接线、调参和后续维护。工程证据见文末，真人手感、排气孔精确贴合和打包验收单独记录。

## 已确认规则

- 初始满 **4 次**，每次成功发动消耗 **1 次**；未满时每 **5 秒恢复 1 次**，依次恢复。再次使用不重置正在进行的恢复计时。没有次数或发动条件不满足时不扣次。
- 按住左／右方向并按原主技能键，沿发动时车辆的左／右方向侧移。无有效方向输入则拒绝，不猜方向、不消耗。允许静止发动。
- 默认侧移 **300 cm / 0.8 s**。起手方向锁定为水平世界方向，期间转车头不改变这次横移方向。保留原前进惯性、重力与真实接触，墙和其他阻挡能缩短行程，不穿墙、不瞬移。
- 漂移强化与侧移同时开始，持续 **1.0 s**。仅在按住漂移／手刹时启用强化转头，默认最大目标转速 **240°/s**；松方向会按响应参数收敛角速度。最后 **0.2 s** 可继续修正车头，时长合理，属于首测值。
- 上一次 **0.8 s** 侧移结束后可消费下一次，新的动作替换旧动作剩余的漂移尾段；不是等到资源恢复才能连续使用。
- 不新增伤害、无敌或冲击免疫。持球／发射规则沿用，技能不主动脱球，发射仍沿实际车头，普通发射反作用力保留。
- 腾空、翻覆、眩晕、自救执行、碰撞反弹控制阶段及比赛规则禁用时不能发动；动作中失去许可时停止额外驱动和强化，保留已经形成的物理动量，不退还次数。眩晕不暂停资源恢复。
- 淘汰、换车、失去 Avatar、移除技能或离场清理动作、恢复计时及该 Avatar 的次数效果。新 Avatar 重新初始化，重复绑定同一 Avatar 不补满。

## GAS 与物理职责

`UHALSideShiftAbility` 接入已有 Primary 技能授予，使用 ServerOnly GAS 激活和条件检查。次数是 Avatar 范围的 GAS 持续效果层数，短期状态授予 `State.Movement.SideShift`；未新增免疫标签。GA 不 Tick，恢复用服务端计时器。

`UHALSideShiftComponent` 管方向请求、次数、动作窗口和清理。客户端只提交左／右意图与递增序号，服务器用自己的车辆姿态计算世界方向，并走已有 ASC 激活路径；不接受客户端指定距离、速度或命中结果。

`FHALSideShiftMotion` 作为纯数据进入既有移动快照／网络物理历史，`ArcadeVehicleMovementComponent` 在正常 Chaos 物理步施加有加速度上限的横移驱动和漂移转向力矩。没有逐帧设置位置，也不在物理线程读取 UObject／GAS。平滑位置曲线理论积分为配置距离，实际地面摩擦、碰撞、减速区域和驱动上限可改变行程；这是物理目标距离，不是无条件保证距离。既有移动加速度／速度倍率继续作用，油膜的摆动保留。

本次移动输入格式 **7 → 8**，Host 和 Client 必须使用同版构建。GAS 激活仍需服务器确认，未另建客户端预测技能启动；网络物理历史接续和 GAS 启动延迟是两件事。

## 已制作的资源

| Content Browser 路径 | 用途 |
| --- | --- |
| `/Game/Data/Abilities/DA_Ability_Car07_SideShift` | `HALSideShiftConfig`，玩法、次数、漂移和所有技能特效参数 |
| `/Game/Assets/FX/SideShift/NS_SideShift_ToonSparks` | 独立世界空间卡通火花配方，复用既有柔和火花母材质 |
| `/Game/Assets/FX/SideShift/NS_SideShift_ToonSmoke` | 世界空间立体烟团，柔和暗／中／亮三阶 |
| `/Game/Assets/FX/SideShift/M_SideShift_ToonSmoke` | 烟团母材质，Unlit，圆润色阶过渡 |
| `/Game/Assets/FX/NS_TireSkid` | 复用现有轮胎痕迹及卡通烟尘，不重存、不复制 |

Car07 既有 BP、Definition、Presentation DA、模型、材质和房间目录未改写。当前排气挂点是按模型范围生成的初始偏移，**尚未声称精确对准模型排气孔**，请按下节完成贴合。原生资源制作工具 `HALBuildSideShiftFXCommandlet` 默认拒绝覆盖已存在资产，日常调参不必重跑。

## Editor 配置：团队手动执行

1. 完整重开本轮编译后的 Editor。打开已有 `DA_Ability_Car07_SideShift`，确认 **Charges** 为 4／5 s，**Motion** 为 300 cm／0.8 s，**Drift Seconds** 为 1.0 s；**Presentation → FX → Enabled** 已勾选，Sparks／Smoke 已引用上述新资源。
2. 在 `/Game/Blueprints/GA` 新建 **Blueprint Class → All Classes → HALSideShiftAbility**，命名 `GA_Car07_SideShift`。打开 **Class Defaults → Side Shift → Skill Config**，选择上述技能 DA，Compile、Save。Event Graph 保持为空；不另建次数 GE、恢复 Tick 或侧移节点。
3. 打开 `/Game/Data/MVP_B/DA_Vehicle_Car_07`。沿用已有 **Combat → Attack Snapshot V1**、**Impact → Tiered V1**；在 **Abilities → Version = Vehicle Definition V1 → Grants** 中，将唯一 **Input Tag = Input.Ability.Primary** 的 **Ability** 替换为 `GA_Car07_SideShift`，Level 为 1。不要同时授予旧 Primary 和新 Primary，其他 Grant 保留。
4. 继续使用 `/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_07`，确认其 Definition 指向上述 Car07 DA。房间目录 `DA_VehicleCatalog_Default` 已登记 Car07 时沿用原条目。不需增加车 BP Event Graph、Input Action 或手工添加 Side Shift 组件。
5. 进入允许技能的开发／比赛流程，先在开阔平地停车，按住左或右再按主技能。用现有漂移键测试强化转头；向左换线应从右侧排气孔喷出，向右换线则从左侧喷出。
6. 完成排气挂点贴合及下述真人验收，Host／Client 均重启到同版工程后复测。

**技能参数继续只放在技能 DA**。普通驾驶的漂移配方仍在 Presentation DA；这次技能触发时，用技能 DA 的 Tire Marks 配方临时提出轮胎痕迹需求，结束后返回普通驾驶配置。

## 排气孔挂点贴合

在技能 DA 的 **Presentation → FX → Side Jets** 配置：

- **Left Mounts**：车辆左侧排气孔，**向右侧移时**使用；**Right Mounts**：右侧孔，向左侧移时使用。每侧最多 4 个挂点，首版每侧 1 个。
- **Part Id = Body**：沿用已注册的车身表现 Mesh；不允许挂碰撞根。当前 Car07 骨架有车身／轮组骨骼，没有专用排气骨骼，因此默认 **Socket Or Bone = None**，用 **Offset → Location** 贴合。
- Offset 相对于所选 Mesh 原点；指定 Socket／骨骼时则相对于该 Socket／骨骼。**这是模型局部坐标，不能直接把车辆世界坐标填进去**，也不必再叠加 Presentation DA 的变换。可在 Skeletal Mesh Editor 查看轴向，先小量改一个轴，重新释放验证。
- 推荐美术成员给 Skeleton 添加 `FX_SideJet_L`／`FX_SideJet_R` 两个 Socket，再在对应数组填写名称，Offset 留小量微调。此操作由团队手动完成；有 Socket 后车身动画会自然带动挂点。
- 显式指定不存在的 Socket 时默认不播放并输出一次缺失提示；只有确实需要时才勾 **Fallback To Mesh Origin**。Part Id 不存在时始终跳过，避免火花跑到物理根。
- 喷射方向由代码设为这次横移世界方向的反向，**旋转 Offset 不用于改变喷射方向**。世界空间粒子出生后不粘车，后续转头不会把已喷出的火花带着旋转。

初始位置由左右外包范围估算，可能位于侧厢外侧；应以模型的真实孔位为准。多孔可增加数组，但每个挂点需要两层粒子，会受到全局每车粒子层预算限制（本技能最多使用 8 层）。

## 调参表

以下均在 **DA_Ability_Car07_SideShift**，范围由 Editor 限制和 C++ 校验共同约束。修改后重新释放；资源上限／恢复周期等初始化参数建议退出 PIE 后重开测试。

| 分组／字段 | 默认 | 允许范围／含义 |
| --- | --- | --- |
| Charges → Maximum Charges | 4 | 1～8 |
| Charges → Recharge Seconds | 5 s | 0.2～60 s，逐次恢复 |
| Input → Direction Dead Zone | 0.2 | 0.05～1，低于门槛不消费 |
| Motion → Distance | 300 cm | 10～2000 cm，物理目标行程 |
| Duration Seconds | 0.8 s | 0.1～5 s，位移窗口／最早重用时间 |
| Maximum Acceleration | 6000 cm/s² | 100～30000，太低可能达不到行程；调大不能穿墙 |
| Drift Seconds | 1 s | 0.05～5 s，从发动时计，不是额外追加 |
| Drift Yaw Rate | 240°/s | 30～720，按方向时的目标转速 |
| Drift Response | 10 /s | 1～30，越高越快到达／收敛目标转速 |
| Maximum Yaw Acceleration | 40 rad/s² | 1～100，强化转向力矩上限 |
| FX → Spark Rate | 200 /s/孔 | 0～600，增加火花数量 |
| Spark Speed | 2400 cm/s | 100～6000，喷射速度 |
| Spark Lifetime | 0.16 s | 0.05～1，和速度共同影响喷射距离 |
| Spark Size | 12 × 34 cm | 各轴 1～100 |
| Spread Degrees | 16° | 1～60，火花／烟尘共享散射幅度，随机速度包围盒近似 |
| Smoke Rate | 35 /s/孔 | 0～150 |
| Smoke Speed | 600 cm/s | 10～2000，额外带轻微向上速度 |
| Smoke Lifetime | 0.4 s | 0.1～2 |
| Smoke Size | 34 cm | 1～200，基准球体直径 |
| Spark Color / Smoke Color | 暖色／灰白 | 线性 RGB 0～10，Alpha 0～1 |
| Glow | 1 | 0～3，火花发光强度 |
| Fade Seconds | 0.06 s | 0.01～0.3，侧喷首尾渐变 |

横移曲线在当前 60 Hz 物理历史中按整步离散，最后有一帧收掉额外横移速度，保留原有横向惯性。扩大距离／缩短时间时请同步检查 Maximum Acceleration；所有可配数值并不保证任意组合都有相同手感。

**Tire Marks** 默认引用原 `NS_TireSkid`，四轮均可发痕。它绕过普通“已在漂移／急刹”的触发门槛，包括静止起手，但仍要求实际轮位地面支撑、正常驾驶许可与最大贴地间距；腾空不会在空中画痕。继续复用原四个轮胎组件槽，避免普通漂移与技能重复播放两套。技能侧喷在 0.8 s 结束，轮胎痕迹按原配方自然淡出，漂移强化尾段仍可继续正常漂移。

## UI、日志与后续特效制作约束

现有原生调试 HUD 增加剩余／最大次数和下次恢复秒数。自制 Widget Blueprint 可从车 Pawn 的 `HALSideShiftComponent` 调用只读 **Get Charge View / Get Recharge Remaining** 绘制正式次数 UI，保留项目的“原生回退＋WBP 可替换”方式，不把计时或消费写进 Widget。

控制台开启 `hal.AbilityLog 1` 查看成功发动的方向、次数、距离、恢复时刻与停止位置；`hal.SideShiftFXLog 1` 查看实际挂点／喷射侧；轮胎排查沿用 `hal.VehicleTireSkidDebug 1`。完成测试后关闭。缺失挂点会有一次 Warning，不会改变权威技能。

Niagara 必须使用 **World Space**；C++ 在激活前设置 `User.SpawnRate`、`LifetimeMin/Max`、`VelocityMin/Max`、`SizeMin/Max`、`ScaleMin/Max`、`SparkColor`、`HighlightColor` 和 `StyleParameters`。火花用 Sprite、烟尘用立体 Mesh；保留圆润卡通三阶和柔和过渡，不添加写实尖锐火星或硬黑边。停止用 Deactivate 让短寿命粒子收尾，并有 2.5 s 保底清理。粒子与轮胎痕迹均不能驱动碰撞、球权、伤害或位移。

## 验收与接续

本轮最终 **Editor R8／Game R3** 构建成功；真实 D3D12 下相关回归 **Final_R2 122/122**（120 无警告、2 带警告，失败／未运行 0）。两项警告分别为累计 D3D12 预留虚拟资源 258 GB 超出 256 GB 预算，以及既有无效／阻塞换车负例提示，不写成零警告。覆盖次数耗尽、恢复不重置、眩晕中恢复、球发射事务、动作历史、真实 Chaos 地面／墙阻挡、轮胎、既有技能／受击与清理。

真实 Solver 的左右横移均为 **300.000 cm**，原前进速度 **600.000 cm/s** 保留；实际 Car07 模型挂接图中火花／烟团均可见，结束恢复基准图。最终 **Network_R4** 为实际 D3D12 Listen 双进程、Host 出站 **80 ms**，双方各 **1/1**，零测试警告／失败、退出 0。隔离平地上 Host／Client 首次实际横移 **289.932／289.943 cm**，符合物理目标允许摩擦影响的规则；两端确认独立次数、原 5 s 恢复时钟、旁观阶段、反向喷口和真实轮位痕迹。

早期原生夹具的未持续提供 PT 输入及未模拟 Chaos 却读取球 GT 速度已修正；渲染夹具补齐火花母材质预编译后保留像素断言通过。首轮宽回归暴露新组件在 World 释放后的计时器清理崩溃，已增加空 World 保护并复验原清理测试及全部相关项目。最初网络测试混入旧场景机关作用且测量跨越第二次动作，现改为一次性布置远处隔离平地、只测第一次动作；测试不在技能执行中改位置、速度或物理结果。完整失败记录保留，不以最终通过抹去。

开工 **1337/1337** 份既有资产／Config／uproject／启动入口 SHA256 不变，新增四份上述资源。团队原 Car07 BP／Definition／Presentation 与所有既有参数保留。

工程验证、失败后修正过程及保存资产核对记录在 [本轮验证摘要](Car07_SideShift_Verification_20261116.json)。原始构建日志、自动化报告和实际 D3D12 图像位于本机 `Saved/Car07SideShift/20261116`，不提交 Saved。

团队真人至少复核：静止左右发动；四次连续消费和 5 s 恢复；无方向／腾空／眩晕／自救拒绝；移动中保留前进、按漂移转头而横移方向锁定；持球侧切后按实际车头发射；墙、墙角、侧撞和连续碰撞；慢速／油膜区域；双端次数独立、旁观喷射方向及结束无残留。最后以比赛相机调数量、孔位、色彩与烟尘遮挡。

GA BP 与 Definition 由团队手动接线，真人手感、模型排气孔精确贴合、真实飞行球联动、四机、真实晚加入、完整房间和 Cook／发布包均不包含在原生自动化通过结论里。本轮不提交／Push。
