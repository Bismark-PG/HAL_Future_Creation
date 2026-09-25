# MVP-B Listen Server 阶段 4：本地车辆 Resimulation 技术纵切（v0.1）

日期：2026-09-25。状态：**团队反馈约 100 ms 实测 RTT 下客户端空车即时驾驶、普通无碰撞驾驶无持续大幅修正；阶段 4 核心行为门槛已达到，详细测量／日志待归档。** 本记录对应 `Docs/MVP_B_Listen_Server_Plan_CN_v0.2.md` 的阶段 4，不扩展 GAS、技能、比赛框架或球的正式复制协议。

## 施工前基线

阶段 3 团队反馈：关闭 Editor 的 **Use Less CPU when in Background** 后，客户端持续驾驶不再龟速；客户端持球发射暂时正常，仍感到轻微卡顿。此反馈可作为进入阶段 4 的基线，**不是阶段 3 重复发射、断线输入清零等专项测试已经通过的证明**。阶段 2 留档的 9 个地图、蓝图和 Data Asset 在本次改动前重新计算 SHA-256，全部与 `MVPB_Phase2_PreCode_AssetSnapshot_20260925.json` 一致。本次没有直接编辑任何 `.uasset` 或 `.umap`，车辆、球、摄像机和操控数值未改。

## 实现边界与长期结构

- `Config/DefaultEngine.ini` 在原有 60 Hz Chaos async 固定步基础上启用 UE Physics Prediction。这是网络物理的项目级开关；没有给已调车辆补写 C++ 驾驶默认值。
- v0.2 第 8.3 节建议把“历史长度”也放进资产。UE 5.8.2 内建 Settings Data Asset 没有逐 Actor 的历史长度字段；历史容量由项目级 `PhysicsPrediction.MaxSupportedLatencyPrediction` 决定，本次在配置中保留 `1000 ms`。资产负责该车的冗余、比较、修正与渲染插值。后续如需不同车辆不同历史容量，先做引擎接口验证并修订 v0.2 规格，不能在 Data Asset 中放一个不起作用的假字段。
- UE 5.8.2 的 `UNetworkPhysicsSettingsDataAsset` 和 `UNetworkPhysicsSettingsComponent` 承载正式配置。车辆 C++ 基类新增 `VehicleNetworkPhysicsSettings` 组件；其 `Settings Data Asset` 为空时维持阶段 3 的 30 Hz Unreliable RPC 路径。只有为此组件指定资产，且处于联机世界时，车辆才创建 `UNetworkPhysicsComponent` 历史并切换到 `Resimulation`。因此 `/Game/Maps/Test` 的 Standalone 基线不受该资产开关影响。
- `FVehiclePhysicsHistoryInput` / `FVehiclePhysicsHistoryState` 集中提供 UE 5.8 `FNetworkPhysicsData` 的序列化、插值、合并、验证和比较。连续输入沿用 `FVehicleInputCmd` 与 `FVehicleNetInputData`，发射仍是带 `LaunchSequence` 的单独可靠请求，不把按键上升沿放进逐帧历史。启用历史后不再发送阶段 3 的持续输入 RPC，以免双重驱动。
- `UArcadeVehicleMovementComponent` 的同一 `SimulateVehicleStep` 消费服务器和本地预测输入。Network Physics 回调记录/重放输入，Chaos 回退刚体，状态历史恢复墙角脱困的临时记忆。状态包记录姿态和速度以便诊断；刚体的阈值比较及权威修正由 Chaos 与 Settings Data Asset 负责，不能在自定义 `ApplyData` 中再传送一次刚体。
- 历史输入实现引擎的衰减回调；车辆解除占有/结束播放后，物理线程消费侧阻断旧输入并清零油门、刹车、转向和手刹。阶段 3 的 RPC 超时仍只用于未接资产的回退路径。
- 状态历史目前没有把发射反作用力登记成可回放离散动作。这属于 v0.2 阶段 7；阶段 4 的手感门槛先用**不持球、普通驾驶**验证。球、对车碰撞和发射后修正须记录现象，但不能据此宣称阶段 5／7 已完成。
- UE 5.8.2 引擎源码仍在 ChaosMover 后端使用 `CreateDataHistory<Traits>(Component)` 形式；本项目采用相同、集中于两个 history struct 的适配接口，避免把历史协议散在 Pawn。该接口在引擎内部被称为 legacy data path。未来升级到 `FNetworkPhysicsPayload` / Internal 接口时，只替换此适配边界，车辆输入语义、移动执行函数和 Data Asset 所有权保持不变。

## 团队在 Unreal Editor 的资产接线

1. 关闭 Live Coding 后启动 UE 5.8.2，确认 C++ 编译已完成。打开 **Edit → Project Settings → Engine → Physics**，核对 **Tick Physics Async** 已启用、固定步约 `0.016666667 s`，**Physics Prediction → Enable Physics Prediction** 已启用。若 Editor 显示的值与 `Config/DefaultEngine.ini` 不符，先停止测试并记录配置覆盖来源。
2. Content Browser 在 `/Game/Data/Network` 建文件夹；右键 **Miscellaneous → Data Asset**，在 Pick Data Asset Class 中搜索并选择引擎类 **NetworkPhysicsSettingsDataAsset**；命名 `DA_NetworkPhysics_MVPB`，得到 `/Game/Data/Network/DA_NetworkPhysics_MVPB`。这是 UE 内建 Data Asset，不需要创建自定义蓝图类。
3. 打开资产，展开 `Settings`。阶段 4 先在 `General Settings` 勾选 `Override Sim Proxy Rep Mode`，将 `Sim Proxy Rep Mode` 设为 **Default**，保持远端车辆仍走原复制模式。到阶段 5 碰撞门槛验证时，再有记录地评估 **Predictive Interpolation**。
4. 在 `Network Physics Component Settings` 勾选 `Override Compare State To Trigger Rewind` 并启用 `Compare State To Trigger Rewind`；勾选 `Override Compare Input To Trigger Rewind` 并启用 `Compare Input To Trigger Rewind`。为固定该资产的传输意图，建议勾选左侧 `Enable Unreliable Flow` 覆盖框并保持右侧值为启用；勾选左侧 `Enable Reliable Flow` 覆盖框并保持右侧值为禁用。灰色的右侧值仅显示当前继承的全局默认，左侧未勾时并未由此资产固定。冗余输入、误差阈值和 `Resimulation Settings → Resimulation Error Correction Settings` 先采用资产创建时显示的 UE 5.8.2 值，并截图/抄录到下面的测试记录；项目级历史容量另记录为 `MaxSupportedLatencyPrediction=1000 ms`。不要先猜一个“最佳阈值”。如 UI 字段名称略有空格差异，以括号中的英文关键字搜索。
5. **Save** 该 Data Asset。打开 `/Game/Blueprints/Vehicles/BP_RoundedVehiclePawn`；在 Components 面板选中从 C++ 继承的 `VehicleNetworkPhysicsSettings`（搜索 `NetworkPhysics`）；在 Details → **Networked Physics Settings → Settings Data Asset** 指向 `DA_NetworkPhysics_MVPB`。只编辑这一处引用，**Compile → Save**。不要改 VehicleDefinition、Movement、BallControl 或 LocalConfig。`BP_PassiveTestVehicle` 暂不接此资产。
6. 运行前检查 `/Game/Maps/MVPB_NetTest` 仍使用原 `HALMatchGameMode` 子蓝图与 `BP_RoundedVehiclePawn`。如团队复制了单独的测试 Pawn 蓝图，只在该 Pawn 上接此资产，并确认 GameMode 的 Default Pawn 指向该测试 Pawn；不要同时悄悄改变正式测试版本引用。

## 双进程技术验证与记录

1. 先在 `/Game/Maps/Test` 做一名玩家 **Standalone Game** 回归：正常驾驶、转向、手刹、墙角、控球与发射。它应保持阶段 3／已保存测试版本手感；日志不应出现 `Network Physics vehicle history active`，因为 Standalone 不启用历史。
2. 关闭 Editor 的 **Use Less CPU when in Background**，在 `MVPB_NetTest` 按现有双进程 Listen Server 指南启动两人。用 `HALMatchStatus` 查两端归属，只在主机执行 `HALStartMatch`。两端车辆日志应有 `Network Physics vehicle history active`；各车首次 `Vehicle physics step` 仍只出现一次、`DeltaTime` 约 `0.016667`。确认客户端**按下油门立即运动**，主机看到同一车辆移动；再做刹车、倒车、转向、漂移、低速贴墙和墙角。若客户端不动或反复回原点，先撤掉 BP 上的 Settings Data Asset 引用恢复阶段 3 路径，不要修改已调驾驶参数补偿网络问题。
3. 在两端控制台临时输入 `p.Chaos.DebugDraw.Enabled 1`、`p.Net.DebugDraw.ShowRepMode 1`，核对本地拥有车辆显示 Resimulation 模式、远端显示 Default 模式；记录截图/日志后恢复为 `0`。可短时开启 `np2.Resim.DrawDebug 1` 观察修正，结束立即恢复为 `0`；调试绘制本身会增加开销。
4. 测试工具应记**实测 RTT**，不能把单向注入延迟直接写成 RTT。依次记录低延迟基线与约 `100 ms RTT / 0% loss`；每组至少完成直线加速、连续转向、急刹、手刹转向和贴墙转出。记录本地输入可见延迟、每分钟明显回退次数、最大位置修正量、旋转修正量、运行时物理帧率、主机/客户端日志及重现步骤。通过门槛是 v0.2 所述：100 ms RTT 下立即响应，普通驾驶没有持续大幅修正。阈值只在采样后调整资产并记录前后数据。
5. 记录球和另一车辆接触的异常，但阶段 5 才完成远端 PI 与正撞、侧撞、追尾、墙角连续接触门槛；阶段 7 才处理发射反作用力的历史动作。阶段 3 遗留的重复发射和断线输入清零也要单独补测。若发生 handled ensure、线程上下文警告、网络历史反复回退或性能下降，附两端日志，不标记通过。

建议每次测试留档：引擎补丁／代码版本、Data Asset 名称和关键字段截图、地图、运行方式、后台节流状态、单向注入参数、实测 RTT/loss/jitter、Physics FPS、两端车辆归属、修正次数/幅度、发射和接触现象、日志路径、结论。

## 本次已执行验证

- `HAL_Future_CreationEditor Win64 Development` 与 `HAL_Future_Creation Win64 Development` 均编译成功。
- 占有阻断与输入衰减补丁后，重新编译 Editor／Game Target 均成功；`UnrealEditor-Cmd` 再执行 `HAL.FutureCreation`：**12/12 通过**，其中新增历史输入/状态网络序列化往返与输入衰减断言；最终报告在 `Saved/Automation/MVPB_Phase4_Final`。
- 团队已在 `/Game/Data/Network/DA_NetworkPhysics_MVPB` 创建设置资产并接到 `BP_RoundedVehiclePawn`；只读日志确认两个车辆都激活了历史。修复版编译前已保存的蓝图、DA、测试地图 SHA-256 见 `MVPB_Phase4_AssetSnapshot_20260925.json`。双进程 100 ms RTT 测试结果仍未确认，因此**阶段 4 尚未验收通过**。

## 首次接线后的阻断问题（待修复版复测）

团队将该资产接到 `BP_RoundedVehiclePawn` 后，在 `MVPB_NetTest` 双人 Listen Server 中主机和客户端均无法驾驶，键盘与手柄相同。现场日志确认两个车辆的 Network Physics history 都已启用，物理固定步 `DeltaTime=0.016667`，主机执行 `HALStartMatch` 后比赛状态为 `Playing`；因此不是接线位置、比赛阶段或物理步未启动的问题。

检查 UE 5.8.2 `NetworkPhysicsComponent.cpp` 的输入流程发现：正常物理步中，本地拥有车辆执行 `BuildData` 记录输入，**不会再执行 `ApplyData`**；`ApplyData` 只在服务器远端输入、模拟代理或回退重模拟时运行。阶段 4 首版移动组件却让所有启用 history 的车辆只消费 `ApplyData` 缓存，导致主机与本地客户端都持续使用零输入。这是 C++ 适配错误，不需要修改车辆驾驶 Data Asset。

修复将输入来源明确分为：正常本地步消费 `PendingInputCommand`；服务器远端和本地重模拟消费 `ApplyData` 中的历史输入。补充了四种来源选择的自动化断言，并增加 `hal.VehicleHistoryLog 1` 的限频诊断日志（测试后恢复 `0`）。团队关闭 Editor 后，修复版 **Editor/Game Target 均重新编译成功**；`HAL.FutureCreation` **12/12 通过**，报告在 `Saved/Automation/MVPB_Phase4_InputFix`。这只能证明代码构建和数据/分支测试通过，仍需真人双窗口驾驶复测。

修复版复测时保留现有 DA/蓝图接线，不必重填任何车辆参数。重开 Editor，确认后台 CPU 节流关闭，在 `MVPB_NetTest` 启动两人 Listen Server；主机用 `HALMatchStatus` 确认两人后执行 `HALStartMatch`。先分别操作主机和客户端直线前进/转向。必要时在**两个窗口各自**输入 `hal.VehicleHistoryLog 1`：主机玩家车及客户端本地玩家车应周期性记录 `Source=Local` 且按油门时 `Throttle=1.00`；主机进程里的客户端车辆应记录 `Source=History` 且 `Throttle` 跟随客户端变化，`Blocked=0`。采样后两端各自执行 `hal.VehicleHistoryLog 0`。若本地为 `Source=Local` 且油门非零却仍不动，再检查 `bGrounded`、碰撞和 Chaos 力，而不是修改输入资产。保留两端日志和实际驾驶结果；成功后再进入 100 ms RTT 的阶段 4 手感与修正门槛。

## 第二轮双进程反馈与复制组件生命周期修正（已编译，待可见窗口驾驶复测）

团队反馈双端已能驾驶，但客户端观看主机车与球时仍一顿一顿；客户端持球后稍微移动即被拉回，几乎无法前进。进一步确认：**客户端空车也会被拉回或卡在原地**，因此不能把客户端驾驶阻断单独归因于持球约束。远端车辆当前被资产固定在 `Default` 复制模式，球也仍为 `SetReplicateMovement(true)` 默认模式；它们的远端平滑分别属于阶段 5 和 6，不能用调整已保存驾驶参数掩盖。持球车辆的服务器约束由 `BallControlComponent` 创建，客户端目前没有 `Controlled` 球的无权威表现路径；在阶段 6 完成前，持球不能作为阶段 4 普通驾驶门槛的通过证据。

同时，第二轮客户端日志在两辆车初始复制时各出现一次 `UActorChannel::ProcessBunch: ReadContentBlockPayload failed to find/create object. RepObj: NULL`，而此前 `UNetworkPhysicsComponent` 在 `BeginPlay` 才被 `NewObject` 动态创建。UE 5.8.2 的 `UActorChannel::ProcessBunchInternal` 在处理初始子对象内容块之后才调用 `PostNetInit`／`BeginPlay`；因此当时客户端组件尚不存在，是历史子对象未能解析的强烈嫌疑，足以影响输入/状态历史。C++ 已将历史组件改为车辆构造时创建的同名默认子对象，并在 `PostInitializeComponents` 建立 history，确保初始内容块到来前组件和历史都已存在；仍只在指定 Settings Data Asset 的联机世界启用，未改资产、车辆数值或球规则。**这只是组件接收问题的修正，不能据此宣称驾驶卡顿已解决。**

修正后 `HAL_Future_CreationEditor` 与 `HAL_Future_Creation` Development Target 均编译成功；`HAL.FutureCreation` 自动化测试 12/12 通过，报告位于 `Saved/Automation/MVPB_Phase4_RepComponentFix`。额外使用本机无界面 Listen Server 与 Client 短时连接 `/Game/Maps/MVPB_NetTest`：客户端日志确认两辆车均激活历史，未再出现 `ReadContentBlockPayload failed to find/create object`；服务器确认客户端 Join 和第二辆车生成。日志分别为 `Saved/Logs/Phase4RepServer.log`、`Saved/Logs/Phase4RepClient.log`。无界面测试没有操作输入，也未测可见帧、球或碰撞，因此仍需团队实测空车持续驾驶与回拉量，阶段 4 **未验收**。

下一轮先核对客户端日志不再出现上述 `ReadContentBlockPayload` 警告，再分别测试空车与持球：客户端空车连续直线/转向是否仍大幅回拉；主机进程对该车的 `hal.VehicleHistoryLog 1` 是否显示 `Source=History`、`Blocked=0`、按油门时 `Throttle` 非零。持球若仍卡住，记录是否只在 `Controlled` 状态发生，进入阶段 6 前需针对服务器约束反力和客户端球碰撞/表现路径做专项设计与验证。远端车和球的可见顿挫分别留给阶段 5/6，不作为阶段 4 空车预测验收项。

团队随后反馈：**客户端空车回拉已消失**，但客户端看到的主机车与球仍有远端顿挫；该轮仅测低延迟，**尚未测试约 100 ms RTT**。这说明本地驾驶阻断已解除，但阶段 4 的普通驾驶即时性与修正门槛仍待验收。远端车辆 PI 的下一步接线与测试方案见 `MVPB_Listen_Phase5_Implementation_CN_v0.1.md`；球仍保留给阶段 6。

团队进一步反馈：在约 **100 ms 实测 RTT** 下，客户端空车能即时驾驶，普通无碰撞驾驶没有持续大幅修正。按 v0.2 的阶段 4 核心行为门槛，**可以进入阶段 5 的远端车辆配置与碰撞验证**。此结论基于团队实测描述；实测 RTT 数值、网络工具设置、修正次数／幅度和两端日志尚未提交，须补入本记录后再将阶段 4 标为完整验收。客户端所见球仍有轻微顿挫：球目前使用默认运动复制，`Free`／`Launched` 的 PI 与 `Controlled` 的无权威客户端表现属于阶段 6；这不推翻空车预测的阶段 4 结果。若球导致持球车辆无法行驶、持续强回拉、错判球权或命中，则应单独记录为玩法阻断，不能以“阶段 6 才做平滑”为由忽略。
