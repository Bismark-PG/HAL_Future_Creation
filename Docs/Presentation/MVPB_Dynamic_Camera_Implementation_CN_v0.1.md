# 动态相机 0.1：实现与配置

日期：2026-10-28（Asia/Tokyo）。范围经用户授权，FOV 与基础距离同样集中在共用 Camera DA。沿用[讨论方案](MVPB_Dynamic_Camera_Plan_CN_v0.1.md)；参数是初测起点，可以继续调整。本文件的配置步骤由团队在 Editor 操作，代码代理没有编辑或保存任何 BP、DA、IA、IMC 或地图资产。

## 1. 接入方式与实际迁移基线

`UHALDynamicCameraComponent` 由车辆组装，`ATestVehiclePawn::CalcCamera` 调用它输出本地视图；组件不新增 Tick。相机只读取当前显示位置，不修改物理、驾驶、球权、技能或网络物理历史。鼠标和摇杆输入独立于 `FVehicleInputCmd`，不发送相机 RPC。每个本地玩家各有运行状态，共享 DA 只保存参数。

新配置为 `UHALCameraConfig`。`VehicleDefinition → LocalConfig → CameraProfile` 引用它，不放进车型 Presentation DA。使用有效 Profile 时，唯一生效来源是 **Profile 的 Base Camera.Field Of View 与 Target Arm Length**；原 Local Config.Camera 是旧配置迁移源及回退，编辑器会将该组置灰。Profile 未配置、未 Ready 或无效时，使用原 Camera／Spring Arm。CameraProfile 属于可选表现配置，错误不会阻止驾驶。

本次读取团队保存资产得到：`DA_Local_TestBaseline_v1` 的基础距离 **1200 cm**、FOV **90°**、Boom Pitch 约 **−75°**，Camera 相对变换为单位。原生构造默认 Pitch −60°，不能直接用它覆盖现有镜头。

动态模式的姿态和 FOV 固定，FOV 可以在 DA 中手调；自动取景只改焦点和距离。动态平滑由求解器统一执行，实际输出不会再次叠加 Spring Arm Lag。静态 A/B 模式保持同一 Base Camera 的固定跟随、FOV 与距离，模拟其位置 Lag，但不是 Spring Arm 旧实现的逐帧完全复刻；彻底移除 CameraProfile 才会回到旧组件实现。

## 2. 首次配置：共用 Camera DA

1. 完整构建后启动 Editor，在 Content Browser 建议新建 `/Game/Data/Camera`。
2. 右键 **Miscellaneous → Data Asset**，选择 **HALCameraConfig**，命名例如 `DA_Camera_Default`。不需要创建蓝图子类。
3. 打开 DA，在 **Migration → Legacy Source** 选择当前车辆实际使用的 Local Config，本工程现行共享资产是 `DA_Local_TestBaseline_v1`。
4. 点击 Details 中的 **Copy Legacy Camera** 按钮。它复制整份旧 Camera 配置，扩展距离范围以容纳旧基础距离，并勾选 Ready For Use；不会改写来源资产。此操作支持 Undo，仍需手动保存新 DA。
5. 检查 **Base Camera → Boom → Target Arm Length = 1200**、Boom Transform 的 Pitch 约 −75°，以及 **Base Camera → Camera → Field Of View = 90**。不要仅勾 Ready 后使用构造默认值。以后修改这两个字段即可调整基础距离／FOV。
6. 保持 **Dynamic Enabled** 打开。初次可以先使用其余默认值；在 Content Browser 对新 DA 执行 Validate Assets，处理报错后保存。
7. 打开当前 `VehicleDefinition` 引用的 **Local Config**，把 **Camera → Camera Profile** 指向新 DA，保存。所有引用这份 Local Config 的玩家车辆共用这份参数，各自独立求解视图；其他 Local Config 要单独引用。

0.1 支持固定向下、世界绝对旋转镜头：Absolute Rotation 开，Absolute Location 关，Use Pawn Control Rotation 与 Do Collision Test 关，Boom／Camera 缩放为 1。不能把普通摄像机碰撞／自动旋转开关当作已实现功能。镜头至少有向下分量，Camera 相对旋转不能大幅偏离 Boom；不符合时会给出可追踪的回退日志。

Editor 中 Pawn 的 Camera 预览仍是旧组件配置；进入游戏后 `CalcCamera` 才输出新 Profile。用实际游戏画面和调试面板判断是否生效。

## 3. 输入资产与映射

两项均为可选输入，未配也可使用自动取景。沿用当前共享 Local Config 的 Default Mapping Context（目前为 `IMC_Vehicle`）。不增加 Event Graph 或 Level Blueprint 逻辑。

### 鼠标

1. 新建 Input Action，命名例如 `IA_Camera_Mouse`，Value Type 选 **Axis2D**。不添加 Hold／Pressed 等离散 Trigger，不加 Delta Time／时间缩放 Modifier。
2. 在 `IMC_Vehicle` 为该 Action 添加 **Mouse X**，默认进入 X。
3. 为同一 Action 再加 **Mouse Y**，添加 **Swizzle Input Axis Values**，Order 选 **YXZ**，将单轴值放入 Y。
4. 默认不加 Negate。代码把正 Y 当作向画面上方观察；若设备方向与预期不同，调整 Camera DA 的 **Invert Mouse Y**，避免同时在映射和 DA 双重反转。
5. Local Config 的 **Input → Camera → Camera Mouse Action** 填入此 IA，保存。

鼠标位移累积为相对观察偏移，停止后等待再回中；不是将屏幕鼠标绝对位置作为目标。游戏视口需要捕获鼠标，先点击视口，确保处于游戏输入状态。PIE 用 Shift+F1 释放鼠标时应停止观察并清掉偏移。

### 右摇杆

1. 新建 `IA_Camera_Stick`，Value Type **Axis2D**，不添加 Hold Trigger。
2. 在同一个 IMC 映射 **Gamepad Right Thumbstick 2D-Axis**。若使用分轴，则 Right Thumbstick X 原样进入 X，Right Thumbstick Y 配 YXZ 进入 Y；不要同时重复配 2D 与两条单轴。
3. 首轮移除相机映射上额外的 Dead Zone／指数 Modifier，由 Camera DA 的 Stick Dead Zone／Exponent 统一处理，便于调参和定位问题。
4. Local Config 的 **Camera Stick Action** 填入此 IA。两项相机 IA 必须彼此独立，不能复用车辆转向／油门／制动／技能／自救 IA。

摇杆控制偏移变化速度，推得越大移动越快，松开沿同一延时回中。鼠标和摇杆同时出现有效输入时本帧优先鼠标；死区内摇杆噪声不会阻止回中。当前不支持镜头旋转。

## 4. 主球标记

1. 在关卡 World Outliner 选中实际主球的**关卡实例**。
2. Details 搜索 **Primary Camera Target**，位于 **Ball → Camera**，只为这一颗勾选。
3. 确保其他球实例未勾选，保存地图。此字段是实例专用，避免在球 BP 类默认值勾选后，让所有调试补球都成为主球。

该标记由服务器复制。运行时生成主球时，由服务器生成流程调用 C++ `SetPrimaryCameraTarget(true)`；当前不为相机新建多球管理器，也不在 DA 引用关卡 Actor。现有调试补球默认不带标记。

本地有且仅有一个有效标记球才参与取景。0 个、复制尚未到达、球被销毁时退回本车＋其他玩家；多个标记时忽略所有球并在调试面板显示候选数量，不能依靠遍历顺序任选一颗。替换主球需取消旧标记，再标记新球。

## 5. DA 参数调试顺序

PIE／Standalone 开始后直接编辑**被运行车辆 Local Config 引用的那份 Camera DA**，数值会在下一次相机求值读取。外部独立运行的 Host／Client 各自加载资产，Editor 内修改不会跨进程热同步；保存后重新启动对应进程。资产引用／输入绑定／IMC结构变更建议重开本轮测试。新 DA 校验无效时会回退旧镜头，Output Log 会显示原因。

| 分组／字段 | 含义及建议 |
|---|---|
| Base Camera.Field Of View | 水平 FOV，5～150°；保持固定，不随自动缩放呼吸。先定 FOV，再调距离 |
| Base Camera.Target Arm Length | 基础距离，cm，必须位于 Minimum／Maximum Distance 内。对象聚集时自动距离回到这里；0.1 不主动缩到基础距离以内 |
| Minimum／Maximum Distance | 全局距离护栏，cm；超出最大值时允许远处目标离屏，优先保持自车 |
| Own Weight | 本车权重，至少 0.1；默认 3 |
| Main Ball／Other Players | 各自 Weight、Full Weight Distance、Zero Weight Distance；范围按与本车的水平距离计算。主球默认 3／1800／6000cm，其他玩家默认 1／1200／4000cm |
| Distance Curve | 可选 CurveFloat；横轴为完整权重距离到零权重距离的归一化 0～1，纵轴为权重比例 0～1；未配置用平滑下降曲线 |
| Zoom Influence | 目标对拉远的影响上限。实际贡献同时乘有效权重／Own Weight（上限 1）；低权重远目标不会独自把镜头拉至最大 |
| Weight Response Seconds | 有效权重变化响应，默认 0.2s；较大更柔和 |
| Target Frame Size | 软目标取景区域占有效画面的宽高比例，默认 0.8／0.8；较小留白更多，较早拉远 |
| Soft／Hard Frame Size | 自车自动跟随框／最终观察护栏，默认 0.4／0.6；X/Y 独立。0.4 指中央40%，坐标约0.3～0.7，不是“中心向两侧各40%” |
| Own Screen Padding | 车辆中心额外留白，默认0.02；实际允许中心范围从框边向内缩2%有效画面。它是中心点留白，不是完整车身像素包围盒保证 |
| Focus Response Seconds | 自动焦点响应，默认0.25s；更小跟随更快。0表示立即响应 |
| Zoom Out／Zoom In Response Seconds | 拉远／拉近独立响应，默认0.15／0.7s，先快拉远、慢拉近 |
| Distance Dead Zone | 距离差小于此值时暂不追踪，默认25cm；过大会留下可见距离残差 |
| Height Response／Maximum Height Lag | 本车高度单独平滑，默认0.3s／150cm；还受当前镜头高度安全限制，避免腾空时车在镜头后方 |
| Target Height Limit | 其他目标相对本车的取景高度限幅，默认800cm；不改变实际物体位置 |
| Mouse Sensitivity／Noise Threshold | 默认0.0008画面比例／原始Delta，噪声阈值0.05；鼠标不乘DeltaTime |
| Stick Dead Zone／Exponent／Speed | 默认0.2／1.5／每秒0.35画面宽高，使用径向死区和时间积分 |
| Maximum Manual Offset | 额外手动观察上限，默认X/Y各0.35画面；仍受本车硬框限制，实际可能更小 |
| Return Delay／Response Seconds | 默认停止0.6s后开始、0.3s指数响应回中；响应不是严格到位时长 |
| Maximum Sample Delta／Teleport Reset Distance | 默认0.25s／1000cm；长帧或真正大幅跳变重种状态并清掉旧观察，防止陈旧积分甩镜头 |

先用纯自动观察聚集／分散，调整 FOV、基础／最大距离、目标权重和范围；再调框与响应，最后调鼠标／摇杆。默认值仅为测试方案。任何平滑之后都会再次校验硬框，达到边界后将修正反馈到实际手动偏移，回拖不必抵消隐藏累计量。观察本身不另行拉远镜头。

## 6. 运行调试与回退

游戏控制台命令：

```text
hal.CameraDebug 1
HALCameraStatus
hal.CameraStatic 1
hal.CameraStatic 0
HALCameraReset
hal.CameraDebug 0
```

Debug 显示蓝色软框、橙色硬框、有效目标及权重、Goal／Auto／Final焦点、实际／目标距离、自车归一化坐标、手动偏移、设备（0无／1鼠标／2摇杆）、Idle时间及限制／回中状态。Own Screen Padding 是框内额外安全余量，画出的框是原始尺寸。固定宽高比开启时，所有比例均指去掉黑边后的有效画面。首版调试面板只在非Shipping本地玩家创建，不影响输入。

`hal.CameraStatic 1` 将当前有效 DA 切为静态基准；`0`恢复 DA 模式。`HALCameraReset` 清掉焦点平滑／权重和观察积分，适合切参数后对照。`HALCameraStatus` 将当前值打印到本地日志。要完全撤回新相机，可清空 Local Config.CameraProfile，旧 Camera 配置仍保留。

失控、眩晕、翻车、自救期间仍可观察，不读驾驶许可作为相机开关。等待开赛、暂停、UI显示鼠标／忽略观察输入、窗口失焦时清掉手动输入和偏移；自动镜头仍可跟随。失去Pawn、重新Possess和结束游戏会重置。菜单接入方应使用正常输入模式／显示鼠标或 `SetIgnoreLookInput`，不能只盖一层不接管输入的装饰Widget。

## 7. 团队实测清单与已知边界

- 先不配输入，仅检查新 DA 迁移、FOV／基础距离与旧镜头接近；有意勾错Ready或距离范围时确认回退日志。
- 两车聚集／分开、主球靠近／远离／持球／发射；检查低权重远车不会拉到最大，无主球／重复主球不随机换球。
- 开 Debug，持续向四角观察至边界，随即反向／沿边拖动；边移动边观察，缩放时检查自车始终处于硬框内部。
- 鼠标停止／摇杆松开后的延时及回中，30／60／120fps，横屏／窄屏／窗口缩放，游戏内实时改FOV、基础距离和权重。
- 高速、斜坡、腾空、侧翻、眩晕、自救；不要把新相机当作车辆扶正或落地辅助。
- Shift+F1、Alt+Tab、暂停／菜单、关卡切换／重新Possess；返回后无旧偏移继续累积。
- 真实Host／Client分别操作观察，确认对方镜头／车头／发射方向不被改变。现有F4完整扰动和四台有线PC验收继续后置。

0.1 不做遮挡避让、自动旋转、镜头震动、观战或多球优先级。以物理碰撞体中心作为本车锚点；视觉骨骼夸张偏移未计入安全框。其他玩家依当前有效PlayerState筛选，当前尚无淘汰系统，未把HP=0当作已实现的观战状态。网络位置小幅校正由已有焦点响应吸收，超过跳变阈值按重种处理。硬框优先级可能造成极端工况的镜头快速纠正，需要真人调参。

## 8. 本轮验证

基点 `main/36ed9b4db0d313794e5e6b3fa12cc5d075d6cd8a`，本轮代码／留档尚未提交或 Push。本机 UE **5.8.3-58210709**，最终结果与可跨设备保存的基线见[验证摘要](MVPB_Dynamic_Camera_Verification_20261028.json)。

- 完整 **Editor／Game Development Win64 构建成功**；引擎头文件既有 C4996 弃用警告保留，不将“0测试警告”写成“编译0警告”。
- 最终普通回归 **93/93**，0测试警告／失败／未运行，含新增5项相机测试：配置与实际旧资产复制、引擎矩阵投影一致性、最终硬框与远目标贡献、鼠标／摇杆帧率与回中／抗累计、本地生命周期／主球异常／数值热读。
- 真实双进程 **D3D12 SM6、960×540离屏视口**，Listen Host／Client各 **1/1**，均0测试警告／失败。不同本地临时Profile输出FOV65／100，CameraManager缓存各为65／100；同一服务器主球标记两端均可读，对方Pawn无本地相机状态。截图核对软／硬框、有效权重及状态文字已经渲染。
- **1169份保存资产逐份SHA-256与用户保存关闭后的本轮基线全同**；原Car_01表现DA、NetTest地图及BuildAndLaunch修改保留。210份最终Source／Config／uproject哈希及完整保护资产清单已入共享摘要。原生运动历史仍为格式3。
- 早期两次编译失败分别为调试Widget的TObjectPtr类型推导、测试夹具旧CameraCachePOV API；均修复。首轮真实双端Host因原生测试球缺少TieredV1 Definition而失败，Client虽报告成功也有测试开始前错误日志，不能算完整干净通过；改用项目已迁移的BP_BasicBall后重新双端通过。原始报告分开保留，没有忽略错误或削弱断言。

最终普通报告：`Saved/DynamicCamera/20261028/Final/Regression/index.json`；双端报告：同目录的`Network_Host/index.json`、`Network_Client/index.json`。截图在`Saved/DynamicCamera/Camera_Host.png`、`Camera_Client.png`。这些原始运行产物不提交；必要结果、旧镜头值、原始Local配置导出和哈希在共享JSON中留档。最终复验后没有继续改C++，专用进程均已退出，未自动打开普通Editor。

**仍待团队操作第2～4节并执行第7节真人初测。** 自动化输入样本不等于真实鼠标／手柄捕获、Alt+Tab／菜单或驾驶手感验收；临时Profile数值热读不替代Editor中修改DA的人工流程。双端基本验证不替代完整扰动、四台有线PC或长时间高速驾驶／机关／斜坡画面。保存资产尚未启用新相机，直接进入原地图仍会使用旧镜头。

## 9. 主球勾选置灰：原因与修复（2026-10-29）

团队在World Outliner选中`BP_BasicBall`关卡实例，`Ball → Camera → Primary Camera Target`仍不能勾选。这个选择位置正确，问题在C++：`ABasicBallActor::CanEditChange`为防止Data Asset迁移后继续编辑旧参数，在Definition模式锁定了球类的可编辑字段，遗漏了后来新增的实例主球身份标记。

修复仅将`bPrimaryCameraTarget`加入该保护的例外，与`ConfigurationSource`／`Definition`一起允许编辑。字段继续为`EditInstanceOnly`和服务器复制：不能在球蓝图Class Defaults上给全部球默认勾选，也不会让调试生成的球自动变成主球。旧伤害／物理等迁移参数仍由原Data Asset管理；不需要切回Legacy或重建球蓝图。

修复版完整编译Editor Target后，团队执行：

1. 重开项目并进入实际使用的`MVPB_NetTest`，保持停止PIE／Simulate。
2. 从**World Outliner**选择场景中的`BP_BasicBall`实例，Details顶部选`BP_BasicBall (Self)`，不要进入该蓝图的Class Defaults。
3. 搜索`Primary Camera Target`，展开`Ball → Camera`并勾选。只标记这一颗主球，其他实例不勾选。
4. **保存地图**。主球身份属于关卡实例，不是球Definition／Camera DA中的配置。
5. 开始测试，执行`hal.CameraDebug 1`或`HALCameraStatus`；在有效Camera Profile生效时，确认面板`Primary candidates 1`／日志`PrimaryCandidates=1`。0表示该端还没有有效标记球，多个标记时全部球都会被忽略。若显示legacy fallback，先完成第2节Camera DA引用／Ready配置。

本修复不修改运行时取景／物理／网络协议或资产。原第8节的UE5.8.3、93项与真实视口双端结果保留历史含义；本设备实际UE5.8.2-56702186，新修复构建／回归及资产保护另见[本次记录](MVPB_Dynamic_Camera_Primary_Edit_Verification_20261029.json)。真人勾选与保存、鼠标／手柄及驾驶／双端画面仍由团队确认。

本次实际完成Editor Target构建（166.95秒）及`HAL.FutureCreation` **94/94**，相机6项，报告0警告／失败／未运行；新增主球编辑回归通过。1172份资产相对用户保存关闭后的基线逐份SHA-256一致，原Local／IMC修改和新Camera DA／IA保留。生产改动仅编辑器保护，未另构建Game或重跑双端；没有把原生属性检查写成实际点击／保存地图已验收。本次C++／测试／文档未提交／Push。

## 10. Client反复撞电墙后短暂缩近（2026-11-05，仅诊断）

团队确认：经Entry创建／加入房间，在练习或比赛中反复撞电墙，Client镜头容易突然zoom-in再恢复；Host没有相同现象，缩近没有明显伴随卡顿。当前检查从干净`main/10f19ed`开始，期间团队本地更新至`ee17c2b`，后者仅电流／电墙表现与留档改动，本文引用的相机、输入切换和移动桥接代码未变。见[本轮诊断证据](MVPB_Client_ElectricWall_Camera_Diagnosis_20261105.json)。

只读UE5.8.2导出确认两车型都引用同一Local／`DA_Camera_Default`：动态基础距离1600cm，旧Local相机1200cm，两者FOV均90°，Profile有效且Dynamic开启，Spring Arm碰撞关闭。长帧阈值0.25s，位置跳变阈值1000cm，拉远／拉近响应0.15／0.7s。以上是当前保存值，不以第1节历史迁移1200cm覆盖。

**首要候选是房间输入切换与相机完整重置的耦合，尚未取得事故当帧证据：**

1. 电墙Stun增减触发Movement Bridge发布新规则快照。Client需要合并权威时间线／回滚，`MovementStatus.bReady`还依赖有效快照、物理帧偏移和非Recovery；不能把Stun本身直接等同于Ready=false，也不是每次回滚都会失去Ready。
2. 房间Widget每0.1秒刷新，将该临时`bReady`作为输入模式就绪条件。若它短暂变false，`Menu = MenuVisible() || !Ready`使其切入UIOnly；恢复true时再切回GameOnly，即使玩家没有打开菜单。
3. 两次切换都调用`SetMenuInputActive → NeutralizeInputForMenu → DynamicCamera.ResetCamera`，清除的不仅是观察输入，还包括当前取景距离／焦点。
4. 下次相机求解直接把距离设为基础1600cm，当帧Step=0，绕过缩放响应；再按动态目标拉远。**仅当此前实际距离大于1600cm时，这条路径才直接产生距离缩近。** 即使没有长帧或10米位置校正也能发生，符合已报告的Entry流程、Client特有恢复与无明显卡顿，但本轮未证明实际那一帧Ready确实翻转。

另有两条保留候选：长帧超过0.25秒／锚点跳变超过1000cm也会执行相同全量重种；动态视图无效时`CalcCamera`回退旧1200cm相机，恢复后再回动态取景。后者要看到调试HUD的legacy fallback或相机无效记录才能确认。普通目标权重变化则应走0.7s拉近平滑，不能仅凭画面说是FOV变化。电墙／shocked代码没有直接改相机FOV或距离；Niagara默认NoCollision，本车相机采集的是碰撞组件Bounds，不能在无证据时把电弧包围盒判为根因。

**建议后续修复顺序：** 先在现有回调／求解入口记录状态变化：Reset原因、Frame、重置前后Distance／GoalDistance、Ready／Recovery／Replay、UI切换及fallback，取得一次完整关联；随后分离初始Avatar／Loading就绪保护与运行中的临时物理恢复，保留驾驶与载入安全检查，临时恢复不重建菜单模式；把观察输入清理与完整相机重置分开，短暂状态波动保持有效缩放，真正换车／跨图／瞬移仍有明确重种策略。保持已调DA、眩晕／回弹／服务器规则不变。

回归必须经Entry流程：低延迟及受控延迟下反复撞电墙，检查Host／Client的距离连续性、UI输入不乱切；再验证真实开关菜单、失焦／回焦、重新Possess、换图及真实瞬移。现有相机测试检查长帧后清手动偏移，但没有断言距离连续性，也未覆盖房间HUD＋反复电墙＋移动恢复组合，旧通过不代表本场景通过。

无需改资产即可先辅助观察：Client执行`hal.CameraDebug 1`、`hal.MovementEffectLog 1`、`hal.ElectricWallLog 1`，关注缩近时D是否直落1600、Goal仍更远，或出现legacy fallback；后两项会增加日志，可在复现后关闭。`hal.CameraStatic 1`固定在Profile基础距离作A/B，完成后`hal.CameraStatic 0`恢复；A/B消失只能缩小范围，不能单独证明Ready链路。调大Zoom In Response Seconds不能阻止直接赋值重置，不建议先改阈值遮蔽问题。

本轮仅源码／引擎实现核对与只读配置导出，**没有修改C++、配置或资产，没有新构建或真实联机复现，不宣称根因已动态确认／修复。** 1183份保存资产前后逐份SHA256全同，第二轮正确路径导出退出0、0错误／警告；第一轮Car02路径假设错误的部分导出／日志保留，不当作工程资源缺失。原始记录位于`Saved/Camera/Diagnosis_20261105_171110/`，不提交Saved，必要事实与哈希已存上述JSON。
