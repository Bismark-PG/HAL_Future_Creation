# MVP-A → MVP-B 开发迁移留档 v1.0

更新：2026-09-18。用途：在同一项目的新对话中接续开发，不依赖旧聊天记录自动共享。

本文区分已实现、用户测试反馈、历史自动验证与后续目标。初次生成时 MVP-B 尚未施工；当前进展以接续更新及新的阶段文档为准。

接续更新（2026-09-18）：用户认可第一阶段配置迁移方案并授权施工，明确数据结构/系统持续用于正式开发；测试场景继续使用当前 Test，现有临时调参完整留存。C++ 配置类型/初始化入口已实现，Editor 与 Game Target 编译成功，10项自动化通过。决策与基线见 `MVP_B_Phase1_DataAsset_Migration_CN_v1.0.md`，长期约定/验证见 `Docs/Configuration/MVPB_Phase1_Implementation_CN_v1.0.md`，详细手动步骤见 `MVP_B_Phase1_Editor_Migration_CN_v1.0.md`。实际 IMC 发射为 LB，球击退倍率5、线阻尼1，历史默认值不得替代新快照。所有实际资产仍保持 Legacy，未由代理编辑或保存，团队资产切换/PIE验收待执行。

## 1. 接手时先读

项目根目录：`D:\UnrealProjects\HAL_Future_Creation`。

1. `AGENTS.md`：开发约束，必须先读，并检查相关目录是否有更具体的 AGENTS。
2. 本文件：截至上述日期的阶段决策、当前状态与交接注意事项。
3. `Codex_Project_Migration_Context_CN_v1.0.md`：游戏整体企画与早期技术讨论。
4. `MVP_A_Vehicle_Ball_Rules_CN.md`：基础操作和球规则；部分早期车辆描述已被后续迭代替代，见本文第 3 节。
5. `MVP_A_Tiered_Knockback_Design_CN.md`：现有档位制击退方案。
6. `MVP_A_Rounded_Vehicle_Setup_CN.md`：圆角车迁移、历史参数核对、离墙辅助配置和测试记录。

最新用户确认的阶段调整优先于旧文档的阶段范围。没有变更的玩法和技术约束继续遵守。出现新的冲突需说明，不要静默扩大范围。

## 2. 游戏和不可改变的技术边界

- 四人自由混战、俯视角未来体育竞技，核心是“碰碰车 × 躲避球 × 动态竞技场”。
- 寻找/争夺球 → 带球与车头瞄准 → 发射攻击 → 碰撞及机关改变位置 → 淘汰对手。
- 球与机关是主要伤害来源；普通车辆碰撞只推动、改变姿态与球权，不直接扣血。
- 正式目标为四人 Listen Server，每位玩家拥有独立的本地跟随摄像机。
- C++ 实现核心逻辑；蓝图仅配置资产、组件和表现连接。
- 车辆继续使用 Chaos 模拟刚体＋自定义力/扭矩，不改成逐帧 SetActorLocation 驱动，不采用完整 Chaos Vehicles 作为核心框架。
- 视觉车身与物理根分离，车轮/倾斜/Rig 表现不能反向改变权威物理。
- GAS 管属性、技能许可、伤害、状态和表现事件；普通 Actor/Component 管实际移动与碰撞。GAS 预测不能替代网络物理预测。
- 涉及蓝图资产或 Event Graph 编辑时，代理提供详细的 Editor 操作步骤，由用户手动完成；不要直接编辑、保存或重写蓝图。
- 不手工编辑 .uasset/.umap，不覆盖用户已有改动，不提交 Binaries/Intermediate/Saved/DerivedDataCache/.vs，不擅自引入第三方插件。

## 3. MVP-A 开发状态与阶段结论

用户已确认：基础球物理测试功能暂时无问题；摄像机和离墙辅助完成初步调试；目前基础玩法结果足以进入 MVP-B。

阶段结论是“按调整后的范围通过基础玩法验证”，不是原始 MVP-A 清单逐项全部完成。固定危险区与基础模型表现经用户确认转入 MVP-B。

| 内容 | 当前状态 |
|---|---|
| 地面驾驶、抓地、转向、漂移/手刹、刹车/倒车 | C++ 已实现，用户已多轮调参测试 |
| 玩家圆头车、简单凸包碰撞、视觉/物理分离 | 已接入用户制作的占位资产；未接入正式赛车模型 |
| 低速手刹转向与顶墙甩尾辅助 | 已实现；用户确认初步调试完成 |
| 一辆不主动驾驶的物理测试车 | 已实现，可推动、受击；不是主动 AI |
| 自动控球、软约束带球、发射、脱球、重新捕获锁定 | 已实现；用户明确报告基础球物理功能测试暂时无问题 |
| 发射后坐力 | 已实现速度相关的质量无关速度变化；不是固定物理冲量旧方案 |
| 简单 HP、球命中伤害、档位制击退 | 已实现最小伤害纵切；尚非 GAS 正式链路 |
| 俯视跟随摄像机 | 已实现，用户已初步调试 |
| 完整控球/发射/命中视听表现与 UI | 未完成，基础表现转入 MVP-B；现有日志/调试状态不等于正式反馈 |
| 固定危险区 | 未实现/未验收，明确转入 MVP-B |
| Data Asset 配置体系 | 未实现，MVP-B 优先施工 |
| 四点悬挂、空中/落地辅助、基础 Rig | 未实现，模型接入后验证 |
| 两人联机、GAS、技能与持续状态 | 未贯通；现有复制声明不代表网络玩法已完成 |

### 3.1 当前操作

Xbox：左摇杆仅转向；B 油门；A 刹车，近乎静止后继续按住倒车；RB 漂移/手刹；X 发射。键盘为调试后备，以现有 Input Mapping Context 为准。

输入统一通过 Enhanced Input → FVehicleInputCmd → Movement，发射请求由 Pawn 转交控球组件，不在组件内分散读输入。

### 3.2 车辆、圆头碰撞与离墙规则

- 驱动力使用 AddForce（质量无关加速度），转向使用 AddTorqueInRadians；不是发动机驱动车轮，也不是用车轮碰撞实现驾驶。
- 玩家根为 Static Mesh Component：属性 `CollisionRoot`，默认子对象名 `VehiclePhysicsBody`。组件树可显示 `Collision Root` 或 `Root Component (VehiclePhysicsBody)`。
- 根隐藏但参与物理；VisualMesh 仅显示，不模拟、不碰撞。本地 +X 是实际车头、驾驶与发射方向。
- 需要低矮、平底、圆润的简单凸包；不要让多余 Box/胶囊碰撞破坏轮廓，不用 Complex As Simple 模拟刚体。
- 根网格未设置、没有简单碰撞或使用 Complex As Simple，会报错并禁用物理；未模拟物理的车不会自动控球。
- 地面 Visibility 必须 Block，当前接地依赖向下 Trace；这不是悬挂系统。
- 普通手刹具有最低转向权限，允许接地低速/静止甩尾；小幅向后反弹不会马上反转方向。
- 顶墙辅助仅在接地、非严重侧翻、低速、油门＋手刹＋有效方向输入且探测到 WorldStatic 墙时启用。
- 用物理根简单几何前向 Sweep，过滤地面/坡面/动态车辆/球；最多 4 次查询、2 个墙面。
- 削弱油门的顶墙分量，保留沿墙分量；有限 Yaw 角加速度追踪目标转速，平滑进入/退出。
- 不施加额外离墙推力，不瞬移、不锁姿态、不自动翻身。凸包转动仍需要空间，不保证狭窄夹角必定脱困。

旧规则文档中的“车辆不能原地旋转”和“方块占位”已被后续低速手刹/圆头车迭代替代。旧文档中“松漂移逐渐恢复抓地”是设计意图；当前代码直接在正常/手刹 GripRate 间切换，并未实现抓地插值。用户之前报告过松漂移到重新加速时的顿挫，明确暂时搁置，不要宣称已经修复；离墙 Blend 不是普通漂移抓地过渡。

### 3.3 基础球规则

- 明确状态 Free / Controlled / Launched，有持有者和有效发射者记录。
- 自动获取车头前方近距 180° 范围内的 Free 球，不能隔墙；候选按距控球点距离、夹角、稳定 Actor 标识排序，每车最多一球。
- 带球使用有限驱动力的软物理约束，不 Attach/瞬移；只忽略与持有车辆的碰撞，仍碰场地及其他车。
- 其他车不能轻碰直接抢球，需先把持有车撞到脱球。持有者失效或球远离控制点也会脱球。
- 发射初速 = 车辆当前线速度＋实际车头方向×固定发射增量；不蓄力。
- 发射后车辆反向后坐，使用质量无关速度变化：Clamp(Max(基础后坐速度变化, Max(当前前进速度,0)×移动后坐比例), 基础下限, 后坐上限)。不承诺静止和移动时主观感知完全相同。
- Launched 在低速持续达到指定时间，或完成一次有效车辆伤害结算后返回 Free；反弹墙面不会单独结束攻击状态。
- 发射者免疫这次球的碰撞伤害，包括反弹球；免疫不取消普通物理碰撞。中立伤害无视敌我仅为后续扩展。
- 脱球后仅原持有车短暂禁止重捕，其他车可争夺；发射结束后的全局拾取锁定是另一项配置。

### 3.4 伤害与击退现状

- 球 OnHit → FVehicleHitContext → FCombatResolver → HP 适配＋档位物理结果；OnHit 不直接修改 HP。
- 当前是 UVehicleHealthComponent 的简单 HP（变化事件＋CurrentHP 复制），尚未接入 ASC/AttributeSet/Gameplay Effect。
- 等效撞击速度 = Max(Abs(Dot(球速度－目标命中点速度,击退方向)), NormalImpulse.Size/球质量) × 击退倍率。
- Light 地面击退、不主动击飞；Medium 更远击退＋腾空翻转；Heavy 更高更远。质心施加质量无关速度变化，旋转结果独立控制并保留 Yaw，不用命中点冲量制造主要击飞力矩。
- 完整一圈只在理想无二次碰撞弹道下近似成立；墙、其他车辆、提前触地仍会改变结果，没有完成辅助落地。
- 当前全局阈值/档位位于 Project Settings → Game → Vehicle Knockback，球只保存基础伤害和击退倍率。
- 当前结算器先要求成功扣血，再执行档位击退；零伤害、无 HP 组件或 HP 已为零的目标不能通过此入口得到档位结果。MVP-B 扩展车撞车时必须分离伤害资格与位移结果，不能通过给车撞车加伤害来绕过。
- 当前球命中使用回调时速度并结合法向冲量补充；未来网络强度评估应审查采样时机，不当成已经验证的碰撞前速度。

## 4. 参数基线：防止迁移后回滚

当前最终调参保存在用户的蓝图/组件/关卡与项目配置中。本文没有重新只读提取全部已保存 CDO，因此不能把下列历史数值当成最终值。

迁移前必须先读取/人工记录当前圆头车、球、物理根、CameraBoom/Camera 和全局规则的实际值及资产引用；保存基线并对比。不要直接拿 C++ 默认值创建 Data Asset 然后覆盖已有调参。

历史迁移检查发现旧车以下 9 项与新车默认值不同，曾要求手动恢复：

| 参数 | 历史应恢复值 |
|---|---:|
| Steering Angular Acceleration | 8 rad/s² |
| Full Steering Speed | 500 cm/s |
| High Speed Steering Scale | 0.8 |
| Handbrake Steering Multiplier | 1.8 |
| Yaw Damping Rate | 1.6 s⁻¹ |
| Handbrake Yaw Damping Rate | 0.35 s⁻¹ |
| Lateral Grip Rate | 10 s⁻¹ |
| Handbrake Grip Rate | 0.2 s⁻¹ |
| Max Lateral Grip Acceleration | 2800 cm/s² |

历史最低手刹转向权限 0.5、手刹倒车转向阈值 150 cm/s；圆头车当前是否再次调整需读取资产。

以下仅是 C++ 默认/首轮基准（单位 cm、s、kg），不是当前蓝图验收快照：

- 车辆质量 800、线阻尼 0.15、角阻尼 0.8；球半径 50、质量 35、线阻尼 0.25、角阻尼 0.1。
- 获取半径 350、持球最大距离 300；发射速度增量 2200；后坐基础 180、比例 0.15、上限 420。
- 球低速阈值 250、持续 0.5、检查间隔 0.05；攻击结束锁定 0.2；原持有车脱球重捕锁定 0.75。
- 球基础伤害 25、击退倍率 1；HP 100。
- Medium/Heavy 强度阈值 1000/2600；Light 水平 250；Medium 水平 600/垂直 500/1 圈；Heavy 水平 900/垂直 750/1.25 圈；Pitch/Roll 档位角速度上限 8 rad/s。
- 离墙进入/退出速度 300/450、探测 15、方向门槛 0.2、顶墙动力比例 0.05、目标 Yaw 2.6 rad/s、响应 10 s⁻¹、最大角加速度 18 rad/s²、进入/退出混合 0.08/0.2。
- 摄像机最终距离/俯角/跟随参数已由用户调整，本文未采样，不以原生相机默认值替代。

## 5. MVP-B 已确认目标

定位：网络与规则纵向切片，同时建立可复用配置与基础赛车表现。先证明链路成立，再大量生产车辆、属性球、Buff、技能和机关。

### 5.1 Data Asset 管理：优先必做

用户明确要求未来多种车/球的可调参数集中到 Data Asset，精简冗长蓝图；此方向已同意。

| 配置职责（名称尚未固定） | 内容 |
|---|---|
| Vehicle Definition | 驾驶/漂移/离墙、物理参数、控球/发射/后坐、初始属性、模型/动画等引用 |
| Ball Definition | 球物理、状态/锁定、基础伤害、击退倍率、模型/表现等引用 |
| Combat Rules | 全局规则及不同命中来源的档位阈值/结果，不能复制到每个球里 |
| 共用配置 | 输入、摄像机等共用设置，可由类型配置引用；只按实际需要拆分 |

- Data Asset 内按配置结构分组折叠，不为每个参数或组件单独建一个资产。
- 全部人为编辑的玩法/类型/表现基础调参集中管理；组件结构和必要表现连接仍由 C++/轻量蓝图承担。
- Data Asset 是共享的只读基础模板；当前 HP、速度、球权、冷却进度等是实例状态，不能写入共享资产。
- GAS 在实例上施加属性/倍率/状态，不修改模板；基础物理配置不一股脑做成 GAS Attribute。
- 凸包、骨骼、动画内容仍留在相应资产中，Data Asset 管引用和设置，不替代资产制作。
- 数值/模型不同但行为相同的类型，原则上新增配置即可，不必复制一个新蓝图。
- 如将全局击退迁入 Combat Rules，Project Settings 只保留统一资产引用入口，不保留两套独立数值来源。
- 迁移后旧蓝图调参入口退出使用；过渡期必须明确优先级、校验和迁移状态，避免静默覆盖/双重应用。
- 先保存实际基线，再迁移和验证手感；蓝图资产引用由用户按操作步骤手动设置。
- 资产类型、加载策略、是否需要 Primary Data Asset、具体拆分和生效生命周期尚待第一阶段设计；不提前上复杂 Asset Manager 框架。类型选择由权威逻辑决定，不将客户端自由配置作为规则来源。

### 5.2 网络与 GAS 核心纵切

- 两名玩家＋Listen Server，优先 PIE 多客户端与局域网。
- 服务端权威驾驶/球与球权；客户端输入请求、本地允许的反馈、插值与校正。每位玩家独立摄像机。
- 一颗普通球，完整争球、带球、发射、后坐、脱球、攻击归属和命中链路。
- GAS 最小接入：推荐 PlayerState 持有 ASC、VehiclePawn 为 Avatar、玩家 Mixed Replication；迁移 HP，服务端应用 Gameplay Effect 伤害，客户端收到 HP/Tag/表现。
- 球/机关默认不持有 ASC；需要效果时采用轻量可复制 Payload，不把球变成技能系统拥有者。
- 一个主动技能：输入、许可、冷却、执行、同步及拒绝恢复。具体技能尚未选定，短冲刺只是讨论示例，不是既定方案。
- 一个持续状态：施加、持续、生效/失效与同步。减速只是示例，尚未定案。
- 禁止客户端直接上报命中目标或伤害数值；现有 ReplicateMovement 不等于已解决 Chaos 网络预测。

### 5.3 击退小型重构与危险区域

- 保留已验证的档位思路，不重新建设巨大通用战斗框架。
- 球击车、车撞车、机关采用各自配置与阈值，共用必要的结果执行；伤害与位移资格独立。
- 普通车撞车继续不扣血；不能为了复用旧结算入口添加隐式伤害。
- 固定危险区域作为一个最小机关：服务端触发、GAS 扣血＋可读冲量/位移，客户端同步表现。
- 不自动复位、不直接秒杀；不在本阶段追加大量动态机关生成或完整比赛节奏。

### 5.4 模型、基础 Rig、物理辅助与摄像机

- 用户要求 MVP-B 导入赛车模型并验证最基础 Rig 动画；先轮转、转向、车身倾斜和基础受击/发射反馈，保持视觉/物理分离。
- 模型接入后验证四点悬挂、有限空中/落地辅助。落地辅助主要修 Pitch/Roll、尽量保留 Yaw，不抹除所有碰撞后果。
- 自然落地与倒扣紧急翻身是独立系统；不把紧急翻身默认追加为已确认必做项。
- 动态相机距离：按玩家分布实时拉远/拉近，分散时扩大视野、聚集时靠近；已同意留到 MVP-B 后半段体验验证。
- 相机具体分布指标、平滑、范围和信息量目标尚未定案；先多玩家同步稳定，再做各自本地相机，不改成四人共用一台摄像机。
- 不做最终美术、复杂动画系统和完整视听包装。

## 6. 已同意的施工顺序与验收

1. 固定 MVP-A 基线：实际参数/引用、模型碰撞、输入、相机、球/击退与当前问题记录。
2. Data Asset 配置迁移：C++ 配置类型与应用入口、用户手动建资产/设置引用、逐项对照，不改变手感。
3. 两人联机驾驶＋一颗球：尽早暴露物理同步、输入、球权与争夺风险。
4. GAS 伤害＋击退规则整理：HP 权威一致、同一命中不重复扣血、不重复位移。
5. 一个主动技能、一个持续状态、固定危险区：最小完整规则链路。
6. 赛车模型与基础 Rig，之后悬挂/有限落地辅助；回归碰撞、球、击飞与网络。
7. 动态摄像机、多玩家体验及四人基础压力测试。

两人纵切为核心验收；四人测试用于发现问题，不意味着要做完整五分钟比赛。完整淘汰/观战/结算、大厅/匹配/在线服务、Dedicated Server、主动 AI、多球/属性球/大量车辆技能与 Buff 仍不作为最小必做范围。

验收必须覆盖：主机/客户端球权一致、实际车头瞄准一致、伤害/状态结束一致、技能拒绝恢复、离墙/低高速/侧撞/墙角/腾空/连续碰撞、基础动画不影响权威碰撞。额外模拟 50/100/150 ms 延迟与 1%–3% 丢包，观察修正跳变、球瞬移/回弹、重复命中/Gameplay Cue 和机关时序。网络物理策略与通过标准需实测确认，不能只以零延迟下“能连接”验收。

## 7. 代码与资产定位

代码目录：`Source/HAL_Future_Creation/`。

| 代码 | 责任/接手重点 |
|---|---|
| TestVehiclePawn.h/.cpp | 组件组装、输入资产、控球入口、相机、物理根配置检查；不是正式轮胎驱动 |
| VehicleInputCmd.h | 统一油门/方向/刹车/手刹/发射命令 |
| ArcadeVehicleMovementComponent.h/.cpp | 地面力、抓地、转向、离墙辅助；现有 Tick 为 PrePhysics |
| VehicleWallEscapeMath.h | 离墙资格、动力投影、Blend 与转速控制窄计算 |
| Tests/VehicleWallEscapeTests.cpp | 5 项离墙自动化测试 |
| BallControlComponent.h/.cpp | 定时获取、软约束、发射、后坐、碰撞脱球 |
| BasicBallActor.h/.cpp | 球状态、归属、低速监测、有效命中入口与复制声明 |
| VehicleHitContext.h、CombatResolver.h/.cpp | 标准化命中、权威伤害/档位结果；MVP-B 窄重构入口 |
| VehicleHealthComponent.h/.cpp | 当前 HP 适配，未来替换/对接 GAS，不同时保留两套可写 HP |
| VehicleKnockbackSettings.h/.cpp | 当前全局 Project Settings 数值来源 |
| PassiveTestVehicle.h/.cpp | 物理木桩，不主动驾驶 |

现有资产路径（仅确认文件存在，未在本次读出所有资产内部配置）：

- `Content/Blueprints/Vehicles/BP_RoundedVehiclePawn.uasset`：当前圆头车。
- `Content/Blueprints/Vehicles/BP_TestVehiclePawn.uasset`：旧车，历史出现 CollisionRoot=None/根 Details 空白；保留作参数参考，不当作正常运行基线。
- `Content/Blueprints/Vehicles/BP_PassiveTestVehicle.uasset`、`Content/Blueprints/Balls/BP_BasicBall.uasset`。
- `Content/Meshes/SM_CarCollisionProxy.uasset`：圆头碰撞代理；仍需检查当前车辆实际引用。
- `Content/Blueprints/Gamemode/BP_MVPA_GameMode.uasset`、`Content/Input/Vehicle/IMC_Vehicle.uasset` 和 5 个 IA_Vehicle 输入资产。
- `Content/Maps/Test.umap`：近期测试关卡；项目还有其他同名/不同测试关卡，不凭文件名或当前 GameDefaultMap 认定实际 PIE 关卡。

切换玩家类型改 GameMode 的 Default Pawn Class 或预放置车辆占有，不是通过 Player Start 选择蓝图。Player Start 只管理出生位置/朝向；避免场景车辆和 GameMode 同时生成/占有玩家。

## 8. 验证记录、已知风险与环境

- Windows；`.uproject` EngineAssociation 为 5.8，历史测试日志实际引擎 5.8.2。
- 引擎位置：`D:\Program Files\Epic Games\UE_5.8`。
- 模块：HAL_Future_Creation；当前依赖 Core/CoreUObject/Engine/InputCore/EnhancedInput/DeveloperSettings 和 PhysicsCore；尚无 GameplayAbilities/GameplayTags/GameplayTasks 依赖。
- 项目已启用内置 ModelingToolsEditorMode（Editor），无新增第三方插件。
- 2026-09-17 离墙开发：Editor 与 Game Target 完整编译成功；测试组 `HAL.FutureCreation.Vehicle.WallEscape` 两轮各 5 项通过、退出码 0。
- 自动化包含资格/速度迟滞、单/双墙投影、混合、Yaw 限幅、临时 Static Mesh 物理场景探测、地面/动态物体排除、开阔地面、空中/倒扣禁用和开关。
- 自动测试物理根为只读引擎 Cube，代理未进行玩家实际圆头凸包的人工驾驶 PIE。之后用户报告摄像机和离墙初步调试完成；不能把这句话扩展为全部网络/物理边缘用例通过。
- 已有原生 CDO 构造期 `FBodyInstance::GetSimplePhysicalMaterial: GEngine not initialized` 3 条启动错误，尚未修复；历史测试成功不等于启动日志无错误。正式迁移物理初始化时应审查，不盲目搬到更晚生命周期破坏配置生效顺序。
- 普通漂移退出顿挫尚搁置；根组件类型改动曾导致旧蓝图模板失效，不能再要求复制旧根组件解决。
- 历史文档提到的 `Tools/InspectVehicleTuning.py` 在 2026-09-18 工作区未找到；不假定脚本可用，应选择合规只读检查或用户手动记录。
- `Config/DefaultEngine.ini` 当前 GameDefaultMap 为引擎 OpenWorld 模板，且 RayTracing=True，与最初推荐设置不完全一致；本次未修改，正式运行/打包前需要单独核对，不擅自改用户设置。
- 全部最新相机/球衰减/冲击/离墙调参尚未在本文采样；实际 .uasset 和配置是必须保留的基线。

增加反射参数、配置类型、组件布局等改动时优先保存并关闭 Editor，完整编译 Editor Target。不要用 Ctrl+Alt+F11 的 Live Coding 结果证明新设置注册、原生组件模板或资产迁移成功；普通函数体修改与结构性修改区别处理。

历史可用的 PowerShell 验证命令（未来按实际环境检查后执行）：

```powershell
& 'D:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' HAL_Future_CreationEditor Win64 Development '-Project=D:/UnrealProjects/HAL_Future_Creation/HAL_Future_Creation.uproject' -WaitMutex -NoHotReload
& 'D:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' HAL_Future_Creation Win64 Development '-Project=D:/UnrealProjects/HAL_Future_Creation/HAL_Future_Creation.uproject' -WaitMutex -NoHotReload
& 'D:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/UnrealProjects/HAL_Future_Creation/HAL_Future_Creation.uproject' /Engine/Maps/Entry -Unattended -NullRHI -NoSound -NoSplash -NoLiveCoding -NoP4 '-ExecCmds=Automation RunTests HAL.FutureCreation.Vehicle.WallEscape' '-TestExit=Automation Test Queue Empty' -stdout
```

## 9. Git 与交接完整性

本次文档生成前：分支 `main`，HEAD `08890c4`（`[mainDev] add wall escape`）；源码已有离墙实现，工作区源码无未提交修改。

当时已有用户改动：`Config/DefaultEditor.ini`、`Content/Maps/Test.umap`、`HAL_Future_Creation.uproject`；另有 3 份未跟踪的 MVP-A 文档。必须保存/保留，不擅自覆盖或删除。本文件生成时仅新增迁移文档并为它添加 .gitignore 白名单，不提交 Git。

同机同目录新对话可以读取这些本地文件，但不能假定它自动获得旧聊天记录。换机器/工作区还需要同步 Source、Config、Content（包含已调好的蓝图/网格/关卡）、.uproject、AGENTS 和企画文档；仅复制本文不能重建最终资产调参。原 .gitignore 广泛忽略 Markdown，其他被忽略的说明文件也要另行确认保存/同步。

## 10. 新对话启动文本

可复制以下内容作为新对话第一条消息：

> 这是同一个 Unreal 项目的 MVP-B 接续开发。请先完整阅读根目录 AGENTS.md 和 MVP_A_to_MVP_B_Handoff_CN_v1.0.md，再按文档读取关联说明和相关源码/配置。MVP-A 已通过调整后的基础玩法验证；危险区、模型/Rig 表现延期到 MVP-B。MVP-B 第一阶段是保存当前实际调参基线并进行车辆/球 Data Asset 配置迁移，不能用 C++ 默认值覆盖已调好的蓝图。涉及蓝图编辑请提供详细引擎操作步骤，不直接修改资产。请先核对当前工程与留档是否有变化，列出第一阶段的具体迁移方案及必要待确认项；不要直接扩展到完整联机、GAS、技能或比赛框架。

尚需后续明确：实际基线快照、Data Asset 拆分/生效/缺失策略、主动技能与持续状态选型、模型/骨骼规范、网络物理策略、危险区重复伤害规则、动态相机指标。上述未定案事项不能自行当成已经确认的设计。
