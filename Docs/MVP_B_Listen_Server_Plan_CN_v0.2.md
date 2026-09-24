# MVP-B Listen Server 施工规格

版本：`v0.2`  
状态：已确认，可作为 MVP-B Listen Server 第一里程碑施工依据  
对应引擎：Unreal Engine `5.8.x`  
测试地图：`/Game/Maps/MVPB_NetTest`  
前一版本：`Docs/MVP_B_Listen_Server_Plan_CN_v0.1.md`

## 1. 文档目的

本文件将 MVP-B Listen Server 设计案整理为可施工、可验证、可继续用于正式开发的技术规格。

本里程碑的目标是完成两名玩家的联机物理纵向切片，并扩展到四名玩家的基础压力测试。施工重点是车辆、球、控球、发射、碰撞和命中事件在网络环境中的权威性与一致性。

本文件不把当前实现写成一次性联机演示。输入数据、球状态、事件序号、权威边界和测试方法都应能继续沿用到正式开发。

## 2. v0.2 已确认决定

| 项目 | 决定 |
|---|---|
| 联机形式 | 四人 Listen Server 自由混战 |
| 第一纵向切片 | 一名主机玩家 + 一名客户端玩家 |
| 联机测试地图 | `/Game/Maps/MVPB_NetTest` |
| MVP-A 回归地图 | 保留 `/Game/Maps/Test`，不得以联机施工覆盖其现有调参基线 |
| 主机优势 | 接受，不人为增加主机延迟 |
| 固定物理频率 | 第一版使用 `60 Hz` |
| 本地玩家车辆 | `Resimulation` |
| 远端玩家车辆 | `Predictive Interpolation` |
| 球 | 服务器权威；`Free`、`Launched` 使用 `Predictive Interpolation` |
| `Controlled` 球表现 | 服务器保持权威控制；客户端使用无权威、无玩法冲量的本地表现约束或目标跟随 |
| 发射预测 | 本地即时表现与反作用力预测；球状态、速度和命中由服务器确认 |
| 玩家加入阶段 | 仅 `WaitingForPlayers` 允许加入；`Playing` 拒绝新连接 |
| 比赛阶段 | 当前仅保留 `WaitingForPlayers` 与 `Playing` 两个最小状态 |
| 玩家退出后的已发射球 | 继续有效，并保留稳定的发射来源标识 |
| 复制基础设施 | 使用 Unreal 默认复制；当前不接入 Iris 或 Replication Graph |
| GAS | 作为第二里程碑，不阻塞 Listen Server／Network Physics 第一里程碑验收 |
| 混合物理模式 | 先做技术验证；目标条件不稳定时允许依据记录调整复制模式 |
| 中途加入 | 不支持 |
| 断线重连 | 不支持 |
| 主机迁移 | 不支持 |
| 最终展示 | 四台电脑通过同一交换机或路由器有线连接 |

## 3. 里程碑边界

### 3.1 第一里程碑：Listen Server 与 Network Physics

包含：

- Host／Join 的底层连接能力。
- 两名玩家生成、Possess、输入和本地摄像机。
- 最小比赛阶段 `WaitingForPlayers`、`Playing`。
- 服务器权威的车辆、球权、脱球、发射、碰撞和命中事件。
- 本地车辆 Resimulation。
- 远端车辆 Predictive Interpolation。
- 服务器权威球及客户端平滑表现。
- 发射事件幂等、反作用力预测和服务器修正。
- 玩家退出、持球清理和发射来源保留。
- 两人网络测试与四人基础压力测试。

第一里程碑允许继续使用现有生命值实现验证一次权威伤害，但 Combat Resolver 的事件接口必须保持可由第二里程碑接入 GAS。

### 3.2 第二里程碑：最小 GAS 纵向切片

在第一里程碑稳定后再接入：

- 一次 GAS 伤害结算。
- 一个持续状态。
- 一个主动技能。
- 对应的 Gameplay Tag、Gameplay Effect 和最小表现事件。

第二里程碑不扩展为完整技能库、完整角色成长或完整比赛框架。

## 4. 网络权威与对象职责

服务器保存唯一可信的玩法状态。客户端提交输入和离散动作请求，运行允许的本地预测，并显示复制结果。

| 对象 | 主要职责 | 网络边界 |
|---|---|---|
| `GameMode` | 登录检查、玩家生成、出生位置、开始比赛、退出清理 | 仅服务器存在 |
| `GameState` | 最小比赛阶段和公共状态 | 服务器写入，客户端只读 |
| `PlayerController` | 本地输入入口、Host Start 请求、本地 UI 和摄像机入口 | 服务器与拥有它的客户端 |
| `PlayerState` | 稳定玩家身份；未来 ASC Owner | 服务器权威并复制 |
| `Vehicle Pawn` | 物理 Avatar、移动组件、控球组件、承受碰撞 | 服务器权威；拥有客户端预测自己的车辆 |
| `Ball Actor` | 球状态、物理、持有者、发射来源和命中来源 | 服务器权威；客户端不得拥有球 |
| `Combat Resolver` | 统一接收命中上下文并给出规则结果 | 仅服务器结算 |
| Camera／UI | 本地显示、输入反馈和连接界面 | 只在本地运行 |

`CurrentHolder` 是球的玩法关系，不用于修改 Ball Actor 的网络 Owner。客户端不得因持球而获得向球直接发送权威 RPC 的权限。

## 5. 连接、比赛阶段与生成

### 5.1 测试地图

- 联机施工和测试统一使用 `/Game/Maps/MVPB_NetTest`。
- `/Game/Maps/Test` 保留为 MVP-A 单机基线回归地图。
- 不在 Level Blueprint 中实现连接、阶段切换、玩家生成或球权规则。

开发阶段的底层命令等价于：

```text
Host: open MVPB_NetTest?listen
Join: open <HostIPv4>:7777
```

最终最小 UI 仅封装相同能力，不引入账号、好友、房间列表或自动 LAN 搜索。

### 5.2 最小比赛阶段

使用可复制枚举表达：

```text
WaitingForPlayers
Playing
```

规则：

- Listen Server 建立后进入 `WaitingForPlayers`。
- 该阶段允许玩家加入、生成和 Possess。
- 主机可以触发 Start；服务器验证后切换到 `Playing`。
- `Playing` 阶段拒绝新的连接。
- 当前不增加倒计时、结束、观战或结算阶段。
- 主机退出时本局结束。

登录拒绝应在服务器登录流程中完成，并返回可读原因。仅隐藏新玩家 Pawn 不算拒绝连接。

### 5.3 玩家生成

- 每台电脑只支持一名本地玩家。
- GameMode 在服务器选择唯一出生槽并生成车辆。
- 出生槽必须稳定且不重叠，不依赖 Actor 遍历顺序。
- PlayerController 只 Possess 自己的车辆。
- 输入和摄像机必须受 `IsLocallyControlled()` 等本地控制条件保护。
- 四个出生槽不足时拒绝额外连接。

## 6. 固定物理执行规格

### 6.1 第一版频率

- Network Physics 第一版固定物理频率为 `60 Hz`。
- 所有需要预测或重模拟的车辆力、扭矩和移动状态更新都以物理步执行。
- 渲染 Tick 只承担输入采样、视觉插值、调试显示等不改变权威物理结果的工作。
- 不允许同一移动规则同时存在于普通 Tick 和物理重模拟路径中。

### 6.2 现有移动逻辑迁移要求

`ArcadeVehicleMovementComponent` 中以下会影响最终刚体状态的逻辑必须进入统一物理执行函数：

- 油门、刹车和反向力。
- 转向扭矩。
- 漂移／手刹影响。
- 悬挂和贴地力。
- 空中角速度限制和落地辅助。
- 墙角脱困或接触面辅助。
- 发射反作用力。

该函数应能够由服务器正常物理帧、本地预测帧和重模拟帧调用。不得在执行函数内直接读取 PlayerController、Input Action、UI 或本地摄像机。

### 6.3 必须记录的可重模拟状态

除刚体 Transform、线速度和角速度外，任何会影响下一物理帧结果的瞬态状态都必须满足以下条件之一：

1. 写入网络物理状态历史；或
2. 能够完全由当前物理状态和当前输入确定地重新计算。

需要逐项审计：

- 是否接地及有效支撑信息。
- 墙角脱困混合值、剩余时间和接触法线。
- 空中／落地辅助阶段。
- 影响力或扭矩的计时器。
- 已应用的离散物理动作序号。

纯视觉悬挂压缩、车轮转动和车身表现不进入权威状态历史。

### 6.4 多世界安全

Multi-PIE 会同时存在多个 World。任何保存在全局对象或配置 CDO 上的运行时 World 缓存都必须按 World 隔离，或改为无可变 World 状态的只读配置访问。

施工前需要审计现有 `VehicleKnockbackSettings` 的 `CachedWorld` 路径，避免一个 PIE World 覆盖另一个 World 的运行时状态。

## 7. 车辆输入与网络物理数据

### 7.1 连续输入

输入管线保持：

```text
Enhanced Input
→ FVehicleInputCmd
→ FVehicleNetInputData
→ Network Physics 输入历史
→ ArcadeVehicleMovementComponent
→ Chaos 刚体
```

`FVehicleInputCmd` 表达玩法语义：

```text
Throttle
Steering
Brake / Reverse
Handbrake
```

网络包装 `FVehicleNetInputData` 至少包含：

```text
FVehicleInputCmd Cmd
uint32 InputSequence
PhysicsFrame
```

要求：

- 输入值在进入历史前完成 Clamp 和有限值检查。
- 服务器拒绝 NaN、Inf 和超范围数值。
- 持续输入使用 Network Physics 输入历史，不发送逐帧 Reliable RPC。
- 主机玩家也使用同一输入结构和移动执行函数。

### 7.2 离散动作

发射不作为持续输入位重复消费。它是带唯一序号的上升沿离散动作：

```text
LaunchSequence
ClientPhysicsFrame
ExpectedBallStateSequence
```

第一阶段服务器权威基线可以使用限频 Reliable Server RPC。接入 Resimulation 后，预测反作用力应迁入与物理帧对齐的 Network Physics Action 或等价历史动作，但继续沿用同一个 `LaunchSequence` 幂等键。

### 7.3 网络物理状态

`FVehicleNetStateData` 至少能比较和恢复：

```text
Position / Rotation
LinearVelocity / AngularVelocity
必要的移动瞬态状态
LastAppliedDiscreteActionSequence
ServerPhysicsFrame
```

具体序列化接口应遵循项目实际使用的 UE 5.8 Network Physics API。不得把输入历史或状态比较散落到 Pawn、Controller 和移动组件多个位置。

## 8. 车辆复制模式

### 8.1 本地拥有车辆

- 客户端立即执行本地输入。
- 服务器使用收到的输入历史执行权威模拟。
- 客户端收到权威历史状态后比较误差。
- 超过配置阈值时回退到对应物理帧，并重模拟至当前帧。
- 物理根接受权威修正；视觉根平滑消化修正偏移。

主机玩家没有 RTT，但不得拥有另一套移动实现。

### 8.2 远端车辆

- 客户端不预测远端玩家输入。
- 远端车辆使用服务器状态和 Predictive Interpolation 显示。
- 远端动画只接收必要的驾驶表现状态，不复制完整本地输入历史供表现使用。
- 车轮、车身倾斜和保险杠动画不改变权威碰撞。

### 8.3 Network Physics 配置资产

创建一个正式的 Network Physics Settings Data Asset，建议命名：

```text
/Game/Data/Network/DA_NetworkPhysics_MVPB
```

该资产至少集中保存：

- Autonomous Proxy 的 Resimulation 设置。
- Simulated Proxy 的 Predictive Interpolation 设置。
- 历史长度和冗余策略。
- 误差比较和修正参数。
- 视觉平滑参数。

资产引用通过可配置字段设置，不在 C++ 中硬编码 Content 路径，也不以新的 C++ 默认值覆盖已经调好的蓝图或 Data Asset 参数。

## 9. 混合复制模式技术门槛

在迁移完整球玩法前，必须先完成以下技术验证：

```text
一辆本地 Resimulation 车辆
+ 一辆远端 Predictive Interpolation 车辆或服务器权威物理对象
+ 连续正撞、侧撞、追尾和墙角接触
```

主要门槛：

- `100 ms RTT + 1% 丢包 + 20 ms Jitter` 下不会永久分叉。
- 硬碰撞后的修正不会持续振荡超过 `1 秒`。
- 普通无碰撞驾驶不会周期性出现大幅瞬移。
- 服务器、主机和客户端最终位置关系一致。

若不满足：

1. 保留日志和录像，记录发生条件与修正量。
2. 先检查固定物理步、历史状态遗漏和碰撞配置。
3. 再调整 Network Physics Data Asset 参数。
4. 仍不稳定时，允许评估统一关键交互物体复制模式等替代方案。
5. 复制模式变更必须形成新的设计记录，不静默改变架构。

## 10. 球的权威状态

### 10.1 原子复制状态

球的离散玩法状态使用单个原子复制结构，建议为 `FBallRepState`：

```text
EBallState State
Vehicle / Pawn Holder
PlayerState or stable PlayerId LastLauncher
uint32 StateSequence
ServerPhysicsFrame
ReacquireLockedVehicle
ReacquireLockEndServerTime
```

规则：

- 所有状态转换只在服务器函数中发生。
- 每次有效状态转换递增 `StateSequence`。
- 客户端通过一个 RepNotify 应用整组状态，不分别推断 Holder、Launcher 和 State。
- 客户端忽略比当前已应用序号更旧的离散表现事件。
- 物理 Transform 复制与离散球状态复制职责分离。
- 球权不得通过 Attach、位置接近或场景层级推断。

### 10.2 `Free` 与 `Launched`

- 服务器运行球的 Chaos 物理和碰撞。
- 客户端使用 Predictive Interpolation 显示服务器状态。
- 客户端碰撞只可用于表现，不能产生伤害、球权或发射结果。
- 命中资格、发射者免疫和回到 `Free` 的条件都由服务器判断。

### 10.3 `Controlled`

- 服务器创建并维护真实控球约束或权威驱动。
- 客户端依据 `FBallRepState` 创建本地表现约束或目标跟随。
- 客户端表现路径不得向车辆施加玩法冲量，不得参与权威命中。
- 服务器控球约束对车辆产生的反作用必须能够被车辆重模拟路径一致重现；第一版优先采用不会把不可预测反力反馈给车辆的控制方式。
- 状态离开 `Controlled` 时，服务器和客户端都必须清理各自约束。
- RepNotify 必须支持重复调用和晚到状态，不得重复创建约束。

## 11. 自动控球

服务器根据以下信息决定控球：

- 球是否为 `Free`。
- 车辆是否已经持球。
- 距离和车头前方范围。
- 必要的遮挡结果。
- 重新捕获锁定状态。
- 多候选球的确定性排序。

客户端可以显示范围提示，但不得声明获取成功。多辆车同帧争夺一颗球时，最终只允许一次服务器状态转换，并以 `StateSequence` 复制结果。

## 12. 发射协议与反作用力

### 12.1 客户端请求

本地按下发射键后：

1. 生成单调递增的 `LaunchSequence`。
2. 记录当前本地物理帧和预期球状态序号。
3. 立即播放按键、音效、轻量特效和允许的预测反作用力。
4. 向服务器发送一次发射动作。

### 12.2 服务器验证

服务器至少验证：

- 请求来自该 Pawn 的合法拥有客户端。
- Pawn 当前确实持有目标球。
- 球仍处于 `Controlled`。
- 预期状态序号没有表明请求基于失效球权。
- `LaunchSequence` 尚未处理。
- 发射频率和输入状态合法。

验证成功后，服务器只执行一次：

- 清除 Holder。
- 写入稳定发射来源。
- 球切换为 `Launched` 并递增状态序号。
- 设置权威发射速度。
- 对车辆应用一次权威反作用力。

服务器拒绝请求时，不生成球状态转换；拥有客户端通过权威状态和车辆重模拟撤销预测结果。

### 12.3 幂等规则

- 同一 `LaunchSequence` 不得重复改变球状态。
- 同一 `LaunchSequence` 不得重复施加反作用力。
- 伤害事件拥有独立的服务器事件序号，不以发射 RPC 到达次数为依据。
- Reliable 只保证传递，不替代幂等检查。

## 13. 碰撞、命中与伤害边界

统一权威管线：

```text
服务器物理碰撞
→ FVehicleHitContext
→ Combat Resolver
→ 物理位移／脱球结果
→ 当前生命值适配层或第二里程碑 GAS
```

客户端不得上报：

- 命中了谁。
- 造成多少伤害。
- 成功获得球。
- 目标应被击飞到哪里。

普通车辆碰撞仍只产生物理推动、姿态变化和可能的脱球，不直接造成伤害。球与场地机关保留主要伤害来源定位。

第一里程碑不得让当前 Health 实现绕过 Combat Resolver。第二里程碑替换结算后，碰撞检测和物理位移代码不应重写。

## 14. 玩家退出与引用清理

### 14.1 持球玩家退出

服务器按顺序执行：

1. 验证该车辆是否为 Holder。
2. 解除服务器控球约束。
3. 球切换为 `Free` 并递增 `StateSequence`。
4. 清除 Holder。
5. 清理重新捕获锁定中的失效车辆引用，并设置必要的短暂全局拾取保护。
6. 清除车辆控球组件中的 HeldBall 和候选引用。
7. 移除对应 Pawn。

### 14.2 发射后退出

- 已经进入 `Launched` 的球继续运动并保持伤害资格。
- 发射来源不能只依赖即将销毁的 Pawn 指针。
- `FBallRepState` 或命中载荷保存该局内稳定的 `PlayerId`，并可在有效时同时保存 PlayerState。
- 发射者 Controller 已不存在时，命中仍可结算；Instigator Controller 可以为空，但 Source PlayerId 必须保留到该次 `Launched` 状态结束。

### 14.3 主机退出

主机退出时本局立即结束。本阶段不尝试迁移服务器或恢复客户端状态。

## 15. 摄像机、UI 与本地表现

- 每名玩家的摄像机 Transform 只在本地计算，不复制。
- 摄像机只能跟随本地 Possess 的车辆。
- 屏幕震动、镜头距离和 HUD 不得反向修改权威玩法状态。
- 球权、发射确认、伤害和比赛阶段 UI 只读取复制状态或服务器确认事件。
- Host／Join UI 仅封装连接命令和错误提示。

## 16. 默认复制策略

第一里程碑使用 Unreal 默认复制：

- 不启用 Iris 作为本阶段依赖。
- 不增加 Replication Graph。
- 四辆玩家车和关键球在小型测试场地中保持对所有玩家可见；具体 Relevancy 和更新频率在采样后调整。
- 不复制摄像机、纯视觉组件和可由本地状态推导的逐帧表现数据。
- 不在 C++ 中用高频 Reliable Multicast 广播物理过程。

如果四人压力测试证明默认复制不能满足带宽或相关性要求，再单独立项调整，不提前建设大型复制框架。

## 17. 测试方法与验收指标

### 17.1 测试层级

按以下顺序测试：

1. 单机 Standalone 固定物理步回归。
2. Multi-PIE 多进程 Listen Server + Client。
3. 同机两个 Standalone／打包进程。
4. 两台电脑局域网。
5. 四台电脑有线压力测试。

不能只使用单进程 PIE 作为联机验收依据。

### 17.2 网络条件

记录工具配置时必须区分单向模拟延迟和实测 RTT。

| 条件 | 目标 |
|---|---|
| `0～20 ms RTT`、`0% loss` | 局域网和最终展示基准 |
| `50 ms RTT`、`0～1% loss` | 常规网络测试 |
| `100 ms RTT`、`1% loss`、`20 ms jitter` | 第一里程碑主要验收条件 |
| `150 ms RTT`、`3% loss`、`50 ms jitter` | 压力条件，允许明显修正 |

### 17.3 功能测试场景

- 玩家连接、生成、Possess 和主机 Start。
- `Playing` 阶段拒绝新连接。
- 持续油门、转向、刹车、倒车和漂移。
- 低速、高速、正撞、侧撞、墙角和连续碰撞。
- 腾空、落地和碰撞后的姿态恢复。
- 两辆车同时争夺一颗球。
- 客户端发射、反作用力预测和服务器确认／拒绝。
- 球反弹后命中车辆。
- 发射者免疫自己的球伤害。
- 持球客户端退出。
- 发射后客户端退出。
- 主机退出。
- 重复包、丢包和乱序下的离散事件幂等。

### 17.4 第一版量化门槛

- `100 ms RTT` 下本地驾驶输入由预测立即响应，不等待一个 RTT 才开始移动。
- 普通无碰撞驾驶不出现周期性明显瞬移。
- 硬碰撞允许可见修正，但修正振荡不得持续超过 `1 秒`。
- 球的 State、Holder 和 Launcher 在网络扰动停止后，应在 `2 × RTT + 100 ms` 内收敛到服务器状态。
- 任意 `LaunchSequence` 最多产生一次球状态转换和一次权威反作用力。
- 任意服务器命中事件最多结算一次伤害。
- `1%～3%` 丢包不会形成永久球权错误、重复发射或重复伤害。
- 两人测试连续运行至少 `10 分钟`，不得出现永久不同步或无法继续控球。
- 四台有线电脑能够完成基础争球、发射和碰撞压力测试。

位置和角度的最终修正阈值应在车辆 Network Physics 技术验证中采样后写入 Data Asset，并把测试数据补充到实施记录中；不得凭未验证的固定数值覆盖调好的驾驶参数。

## 18. 调试与记录要求

所有调试输出必须可关闭。第一里程碑至少提供：

- 本机角色：Authority、LocalRole、RemoteRole、Owning Connection、Locally Controlled。
- 输入序号、物理帧和最近确认帧。
- Resimulation 次数、回退帧数、位置和旋转误差。
- 球状态、Holder、Launcher、StateSequence。
- LaunchSequence 的接收、接受、拒绝和重复丢弃原因。
- 命中事件序号和重复丢弃记录。
- 玩家退出时的约束和引用清理记录。

日志不得在 Shipping 默认开启，也不得每帧无条件刷屏。

## 19. 施工顺序

### 阶段 0：基线与施工前检查

- 保存并核对现有 Data Asset、蓝图和地图基线。
- 使用 `/Game/Maps/MVPB_NetTest`，保留 `/Game/Maps/Test` 回归能力。
- 审计 Multi-PIE 下的 World 缓存和全局可变状态。
- 建立 Network Physics 配置资产及参数记录位置。

通过条件：现有单机驾驶、控球和发射在固定测试地图中仍可复现，且未被新的 C++ 默认值覆盖。

### 阶段 1：最小联机外壳

- 建立窄职责 C++ GameMode、GameState、PlayerController、PlayerState。
- 完成 Listen Host、Join、两名玩家 Spawn 和 Possess。
- 实现 `WaitingForPlayers`、`Playing` 和主机 Start。
- 验证输入与摄像机仅作用于本地车辆。

通过条件：两名玩家连接后各自只控制自己的车辆，比赛开始后拒绝第三个新连接测试客户端。

### 阶段 2：固定物理执行路径

- 将影响车辆刚体结果的移动逻辑迁入统一物理步。
- 定义 `FVehicleNetInputData` 和 `FVehicleNetStateData`。
- 审计墙角脱困、悬挂、落地辅助等瞬态状态。
- 在不开启网络预测时完成单机驾驶回归。

通过条件：`60 Hz` 固定物理路径下基础手感和现有玩法功能无阻断性回归问题。

### 阶段 3：服务器权威基线

- 客户端提交连续输入和带序号发射动作。
- 服务器执行车辆、控球、脱球、发射、命中和反作用力。
- 建立事件幂等检查。
- 此阶段只验证规则正确性，不作为最终客户端手感标准。

通过条件：客户端不能决定球权、命中或伤害；重复发射请求不产生重复结果。

### 阶段 4：本地车辆 Resimulation 技术纵切

- 接入 Network Physics 输入和状态历史。
- 使用正式 Network Physics Data Asset。
- 完成权威状态比较、回退、重模拟和视觉平滑。
- 记录不同网络条件下的修正数据。

通过条件：一名客户端在 `100 ms RTT` 下可即时驾驶，普通驾驶无持续大幅修正。

### 阶段 5：远端车辆与混合模式门槛

- 远端车辆接入 Predictive Interpolation。
- 测试两车正撞、侧撞、追尾、墙角和连续接触。
- 在 `100 ms + 1% loss + 20 ms jitter` 下执行门槛测试。

通过条件：满足第 9 节门槛；否则先完成原因记录和复制模式评估，不继续迁移完整球玩法。

### 阶段 6：球状态与控球表现

- 建立原子 `FBallRepState`。
- 接入 Free／Launched 的服务器物理和客户端 PI。
- 接入 Controlled 的服务器约束和客户端非权威表现。
- 处理同时争球、脱球、重捕获锁定和晚到状态。

通过条件：所有客户端最终看到相同 State、Holder 和 Launcher；客户端表现约束不产生权威结果。

### 阶段 7：发射预测与反作用力

- 将 LaunchSequence 与物理帧动作对齐。
- 接入本地表现和预测反作用力。
- 处理服务器接受、拒绝、重复和晚到请求。

通过条件：一次按键最多产生一次权威发射和反作用力，拒绝请求能够通过重模拟恢复。

### 阶段 8：退出清理与故障测试

- 完成持球退出、发射后退出和主机退出。
- 执行 RTT、Loss、Jitter、重复包和乱序测试。
- 验证球权、命中和伤害事件不会永久错误或重复。

通过条件：达到第 17 节两人测试标准。

### 阶段 9：最小 UI 与四人压力测试

- 完成 Host、Join、错误提示和主机 Start 的最小 UI。
- 四辆车与至少一颗球同时运行。
- 记录服务器帧率、物理帧稳定性、带宽和修正次数。

通过条件：四台有线电脑完成基础驾驶、争球、发射和碰撞测试。

### 阶段 10：第二里程碑 GAS 纵向切片

- Combat Resolver 将标准化结果交给 GAS。
- 验证一次伤害、一个持续状态和一个主动技能。
- 不改变已经验证的 Network Physics 和球状态协议。

## 20. 第一里程碑完成标准

第一里程碑只有同时满足以下条件才算完成：

- 主机和客户端稳定加入并只控制各自车辆。
- 最小比赛阶段和加入闸门有效。
- 本地车辆使用 Resimulation，远端车辆使用经验证的平滑方案。
- 服务器独立决定车辆最终物理、球权、发射、命中和伤害事件。
- 所有客户端最终看到一致的球状态、Holder 和 Launcher。
- Controlled 球在客户端显示稳定，且表现逻辑不改变权威结果。
- 发射、反作用力和命中不会因重发而重复执行。
- 玩家退出不会留下约束、球权或失效 Pawn 引用。
- 已发射球在发射者退出后继续有效并保留稳定来源标识。
- 通过 `100 ms RTT` 主要验收条件和 `150 ms RTT` 压力测试。
- 通过两人连续测试和四台有线电脑基础压力测试。
- 所有关键设置、测试结果、已知限制和回退决定均已留档。

## 21. 本阶段不包含

- Dedicated Server。
- Iris 或 Replication Graph 迁移。
- 在线账号、Steam／EOS、平台邀请和匹配。
- 自动 LAN 房间列表。
- 比赛中途加入。
- 断线重连。
- 主机迁移。
- 反作弊系统。
- 完整比赛、淘汰、观战和结算流程。
- 完整 GAS、技能库或角色成长系统。
- 多球和属性球的正式玩法扩展。

## 22. 变更控制

- v0.1 保留为原始设计记录，不覆盖。
- v0.2 是第一里程碑施工依据。
- Network Physics 复制模式、固定物理率、球权协议或权威边界发生变化时，必须更新文档版本和变更原因。
- 蓝图、地图和 Data Asset 的具体编辑由团队成员在 Unreal Editor 中执行，代码代理只提供明确操作步骤，不直接修改 `.uasset` 或 `.umap`。
- 新的 C++ 默认值不得覆盖已经保存的蓝图和 Data Asset 调参基线。

## 23. 参考资料

- [Unreal Engine 5.8 Networking Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine)
- [Unreal Engine 5.8 Networked Physics Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/networked-physics-overview)

