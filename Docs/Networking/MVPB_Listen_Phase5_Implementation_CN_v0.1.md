# MVP-B Listen Server 阶段 5：远端车辆 Predictive Interpolation 接线与验证（v0.1）

日期：2026-09-25。状态：**团队报告阶段 5 核心行为门槛通过：PI 模式显示正确，低延迟及约 100 ms RTT／1% loss／20 ms jitter 下两车接触收敛、明显振荡不超过 1 秒；量化日志和网络工具设置待补档。** 本文按 `Docs/MVP_B_Listen_Server_Plan_CN_v0.2.md` 第 8、9、17 节及阶段 5 编写，沿用到正式开发的车辆复制边界：本地拥有车辆用 Resimulation，远端车辆从服务器状态使用 Predictive Interpolation。服务器仍负责最终物理与碰撞结果。

## 进入本阶段的依据与未关闭的门槛

团队复测确认：上轮修复初始复制组件时序后，**客户端空车回拉已消失**。客户端仍能看到主机车辆与球的远端顿挫。阶段 4 留档时 `/Game/Data/Network/DA_NetworkPhysics_MVPB` 的模拟代理 `Sim Proxy Rep Mode` 显式设为 `Default`。进入本阶段后团队确认已在 Unreal Editor 把它改成 `Predictive Interpolation` 并保存；球仍使用默认 `ReplicateMovement`。远端车辆是本阶段目标；球的 `Free`／`Launched` PI 与 `Controlled` 客户端表现属于阶段 6，不能通过此处的车辆资产开关替代。

阶段 4 的核心行为条件是约 **100 ms 实测 RTT** 下客户端空车立即响应、普通无碰撞驾驶无持续大幅修正。团队已经反馈这两项在该条件下成立，可以切换远端车辆 PI。网络工具设置、实测 RTT 数值、修正次数／幅度和两端日志仍需补归档；不能把球的远端顿挫当作阶段 4 空车预测失败。

## 现有接线与配置边界

- `ATestVehiclePawn` 在配有 Settings Data Asset 的联机世界启用 UE Network Physics history，并为本地拥有车辆设置 Resimulation。
- UE 5.8.2 的 `UNetworkPhysicsSettingsComponent::BeginPlay` 会读取 Settings Data Asset 的 `General Settings → Sim Proxy Rep Mode`，仅对 `ROLE_SimulatedProxy` 应用指定复制模式。因此阶段 5 的首次切换可由同一资产完成，不需要新增车辆蓝图脚本或改写输入管线。
- 保留 `60 Hz` 固定物理步、现有驾驶／球 Data Asset 数值、输入冗余、比较、纠错设置及现有 `BP_RoundedVehiclePawn` 引用。先隔离复制模式效果，不同时调多个参数。
- 阶段 4 留档的车辆蓝图、Network Physics DA、联机测试地图 SHA-256 见 `MVPB_Phase4_AssetSnapshot_20260925.json`；团队保存 PI 后的只读快照见 `MVPB_Phase5_PI_AssetSnapshot_20260925.json`。蓝图和地图的 hash 未变，只有 Network Physics DA 变化。代码代理没有修改任何 `.uasset`、`.umap`。

## 团队在 Unreal Editor 的操作

1. 先留存阶段 4 的约 `100 ms` 实测 RTT 测试设置、两端日志和空车驾驶观察记录；该行为门槛已由团队确认，不需要重复测试才能开始本阶段配置。
2. 保存当前需要保留的工作。打开项目，在 Content Browser 找到 `/Game/Data/Network/DA_NetworkPhysics_MVPB`，先记录 `General Settings` 截图；无需修改车辆蓝图引用。
3. 双击资产，展开 `Settings → General Settings`。确认 `Sim Proxy Rep Mode` 左侧 **Override** 勾选；把右侧从 **Default** 改成 **Predictive Interpolation**。若界面字段文字有空格差异，搜索 `Sim Proxy Rep Mode`。
4. 保持 `Focal Particle in Physics Replication LOD`、`Default Replication Settings`、`Predictive Interpolation Settings`、`Resimulation Settings`、`Network Physics Component Settings` 的其余字段为当前值。点击 **Save**。记录变更前后截图、资产路径、引擎版本及日期。
5. 完全结束原 PIE／Standalone 会话后，在 `/Game/Maps/MVPB_NetTest` 重新启动双人多进程 Listen Server。保持 **Use Less CPU when in Background** 关闭。主机用 `HALMatchStatus` 核对两人后执行 `HALStartMatch`。
6. 在客户端窗口短时输入 `p.Chaos.DebugDraw.Enabled 1` 与 `p.Net.DebugDraw.ShowRepMode 1`，确认**客户端自己控制的车**仍为 **Resimulation**、**客户端看到的主机车**为 **Predictive Interpolation**；截图后将两个命令都恢复为 `0`。主机窗口中的客户端车是服务器权威实体，不属于 `ROLE_SimulatedProxy`，不能要求它显示 PI。若客户端看到的主机车仍为 Default，先核对资产是否保存、蓝图的 `VehicleNetworkPhysicsSettings.Settings Data Asset` 是否仍指向本资产，再重新启动会话；不要通过修改驾驶参数补偿。

## 当前施工记录

- 团队确认已把 `Sim Proxy Rep Mode` 改为 `Predictive Interpolation` 并保存。只读文件快照记录 DA 为 2550 bytes，SHA-256 为 `48e78eca68f7249d20153d67821d912fb077e0509d1400719f1c86f0866f4f17`；这个 hash 只证明资产版本变化，字段值依据团队的 Editor 确认。
- 当前双进程日志显示服务器收到客户端 Join，两端的两辆车均激活 Network Physics history，客户端固定物理步为 `0.016667 s`；客户端日志未见此前的 `ReadContentBlockPayload` 组件解析警告。客户端日志也显示已启用 `p.Chaos.DebugDraw.Enabled` 与 `p.Net.DebugDraw.ShowRepMode`，但纯文本日志不能证明实际绘制颜色或观感；需团队反馈截图／观察。
- 团队随后观察到：客户端本地车辆的复制模式为**红色 Resimulation**，客户端所见主机车为**黄色 Predictive Interpolation**；远端主机车“确实更顺滑一些，程度没有特别明显”。这确认 PI 已作用于目标模拟代理；低延迟下改善幅度不大本身不是失败，尚需比较扰动条件及接触稳定性。
- 团队低延迟接触测试反馈：两车位置会收敛，碰撞后无超过 1 秒的持续振荡。团队按“一车停住、另一车正面／侧面／追尾撞击及墙角连续接触”的问题反馈为已测；尚无各子项的逐项录像或最大误差数值，留待补档。记录此条时，扰动门槛尚未测试；后续结果见下一条。
- 团队进一步反馈：已在约 `100 ms` 实测 RTT、`1%` 丢包、`20 ms` 抖动下复测正撞、侧撞、追尾、墙角与连续接触，**两端最终位置均收敛，未出现超过 1 秒的明显振荡**。按 v0.2 第 9 节及阶段 5 的核心行为条件，可以进入阶段 6；本记录明确标注结论来自团队观察，未取得逐项录像、网络工具的单向配置、精确实测 RTT 数值、修正次数／最大位移和两端完整日志。现有本机日志能确认车辆 history 启用、固定步约 `0.016667 s` 且未见旧组件解析警告，但不能代替上述量化证据。
- 本阶段没有 C++ 规则或车辆调参改动。内建 UE 5.8.2 设置组件会在模拟代理 BeginPlay 读取 PI 模式；模式颜色只证明接线正确，行为门槛依据后续两组接触测试反馈判定。

## 对照测试与判定

先在低延迟、无丢包下让主机直行、转向、急刹，客户端观察主机车，这是 PI 的直接观感测试。交换驾驶方后，主机窗口观察到的是服务器权威模拟的客户端车，应记录输入历史和服务器物理是否稳定，但不能把这一路当作 PI 观感。分别记录客户端远端视觉顿挫、各拥有者自己的即时响应，以及车辆是否出现位置回拉。优先使用**空车、无碰撞**区分远端显示与本地预测；测试球时单独注明球尚未迁入阶段 6，不能把球的顿挫计入本阶段车辆 PI 判定。

随后依次做两车正撞、侧撞、追尾、墙角和连续接触；记录服务器、主机和客户端最终位置关系。按 v0.2 第 9 节，在约 `100 ms` **实测 RTT**、`1% loss`、`20 ms jitter` 下重测，不能把单向注入延迟直接记为 RTT。硬碰撞后的可见修正不得持续振荡超过 1 秒，普通无碰撞驾驶不得周期性大幅瞬移，也不得永久分叉。保留网络工具设置、实测 RTT、两端日志、录像或截图，以及触发动作和持续时间。

若使用 UE 5.8.2 内建 `NetEmulation` 而非外部工具，**连接并开始比赛之后**，可先在主机和客户端控制台各执行以下命令作为接近目标网络条件的起点：

```text
NetEmulation.Off
NetEmulation.PktLagMin 40
NetEmulation.PktJitter 20
NetEmulation.PktLoss 1
```

UE 5.8.2 的 `NetConnection.cpp` 在 `PktJitter` 非零时交替使用 `PktLagMin` 和 `PktLagMin + PktJitter` 作为**各端发包的单向延迟**；两端上述设置的平均 RTT 约 100 ms，但瞬时值会变化。`PktLag` 在此分支不叠加，不能再把 `PktLag 50` 当作有效基础延迟。必须核对并记录**实测 RTT**，必要时调整起始值。测试结束后在两端执行 `NetEmulation.Off`，确认回到低延迟基线。若使用外部网络工具，则记录该工具的单向设置并同样实测 RTT，不要与 UE 内建模拟叠加。

若 PI 反而使远端或两车接触更差，先记录现象并检查固定步、历史数据、碰撞和模式显示；再一次只改一个相关 Settings DA 参数并记录前后结果。达不到技术门槛时恢复 `Sim Proxy Rep Mode = Default`，依据 v0.2 第 9 节评估替代复制模式并修订设计记录；不能静默把混合模式视为已验证。阶段 5 通过前不开始完整球状态／控球表现迁移。

## 待团队填写的测试记录

| 项目 | 记录 |
|---|---|
| UE 版本、代码版本、测试日期 | 待填 |
| Settings DA 路径与切换前／后截图 | 待填 |
| 后台 CPU 节流状态、运行方式 | 待填 |
| 注入方式、单向延迟、实测 RTT／loss／jitter | 团队报告约 100 ms 实测 RTT／1% loss／20 ms jitter；工具和单向设置待填 |
| 客户端本地车／所见主机车的 Rep Mode 显示 | 红色 Resimulation／黄色 PI（团队观察） |
| 低延迟空车远端观感与本地驾驶回拉 | 远端略更顺滑，改善不明显；本轮本地回拉待记录 |
| 五类碰撞与 100 ms 条件下最大／持续修正 | 团队报告低延迟及扰动条件下均收敛，无超过 1 秒振荡；最大修正量待填 |
| 主机／客户端日志与录像路径 | 待填 |
| 阶段 4 门槛、阶段 5 门槛结论 | 核心行为门槛均由团队反馈通过；量化日志待补档 |
