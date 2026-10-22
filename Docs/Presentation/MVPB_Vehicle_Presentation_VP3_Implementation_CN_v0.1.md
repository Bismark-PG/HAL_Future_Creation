# MVP-B VP-3：视觉悬挂与配置（v0.1）

日期：2026-10-22（Asia/Tokyo）。施工基点：`main/faf5d067b85140ddd108969f6b2c48a0ac63b8b9`，开工工作区干净，667份资产。实际使用本机UE **5.8.2-56702186**；项目状态顶部资产生成器的5.8.3构建属于另一设备历史，不冒充本轮验证。

## 1. 目的与范围

沿用既有`UHALVehiclePresentationComponent → FHALVehiclePresentationFrame → UHALVehicleAnimInstance → AnimGraph`链路，为四个悬挂骨骼输出独立平移。平地基准为零，局部高差产生压缩／伸长，空中平滑伸长，侧翻及主动自救时归中。车轮的转向／滚动和VP-2车身倾斜继续独立工作。

本步只做视觉探测，没有物理弹簧、支撑力、自然落地辅助、VP-4受击／发射回弹或额外复制协议。碰撞根、质量、速度、驾驶接地／姿态许可、球控制点、发射方向和GAS结算不消费本步输出。

默认新通道关闭，不修改已保存DA／AnimBP。团队先保留已完成的静态轮底校准，再启用动态视觉偏移；不要通过加大伸长行程掩盖整车模型悬空或碰撞体尺寸错误。

## 2. 探测、坐标与后续接口

1. `Configure`缓存实际Mesh参考姿态的四个spin轮心和suspension组件空间变换，按显式FL／FR／BL／BR定位。运行中不读取动画后的轮心，所以不会因上帧压缩再次抬高探测起点而漂移。
2. 每个正常更新最多四次游戏线程简单Line Trace，沿当前**车辆物理根的-Up**方向。起点在参考轮心之上，覆盖最大压缩；终点覆盖半径、允许坡面的修正、伸长及余量。轮半径与行程均为**世界cm**，沿用已校准`Radius Cm`。
3. 支撑对象只查询`WorldStatic`，与组件Mobility无关：当前Movable／WorldStatic地板有效。忽略自身Actor和自身持球；PhysicsBody车辆／球、WorldDynamic机关／道具不作为支撑。即使对象通道误设为WorldStatic，模拟物理组件、Pawn和普通球也不会被接受。未来确需WorldDynamic升降平台时，需在物理悬挂规格中统一扩展支撑规则，不能只在动画里放开所有对象。
4. 最近命中面必须非初始穿透、法线有效，同时朝世界+Z和车辆Up达到配置阈值；墙、背面及过陡表面拒绝。不穿过被拒绝的最近阻挡去寻找后面的地板。正立腾空无命中时目标为最大伸长；车体倾斜过大或自救正在执行时目标为零，并跳过探测。
5. 对接受的局部平面，沿车辆Up求装饰性轮胎的切平面位置：`offset = (Radius - Dot(ReferenceCenter - HitPoint, Normal)) / Dot(VehicleUp, Normal)`。正值压缩、负值伸长，限制在每轮行程内。更远、超出伸长可达范围的地面仍按无支撑处理；高于压缩上限时只限幅，不能抬动物理车。
6. C++将`VehicleUp × offset`依次变换到Mesh局部及**参考suspension骨骼局部空间**，自动包含组件／参考骨骼旋转与缩放。团队直接连接最终`Suspension Translation`向量，不必再猜局部X／Y／Z或填写正负号。Root保持参考姿态；悬挂节点只改平移，不能同时在其上添加额外旋转后仍依赖参考空间换算。
7. 指数平滑由表现组件每次更新推进一次。首次有效样本直接建立当前目标；长帧、瞬移、控制者变化、重新Configure及结束沿用原有重置。停更时不补算整段时间。动画更新只拷贝快照，不进行Trace或反向修改物理。

后续四点物理悬挂可将安全发布的每轮支撑／行程读数转换成同一`FHALWheelSuspensionTarget`，继续复用行程平滑与骨骼输出；届时应替换视觉Trace来源，不能叠加两套悬挂。当前没有预建第二套物理系统或运行时模式切换框架。

各显示端采用相同算法查询各自场景和可见车体变换，不复制骨骼位移／接触数据，不加入Network Physics历史。网络插值和场景更新可能产生短暂的数值差异，因此不承诺逐帧完全相同；这些差异不影响权威碰撞或冲击。不同机器必须同步场景、模型、DA和构建。

## 3. 参数位置与含义

实际入口：**玩家Vehicle Definition → Presentation → 当前车型表现DA**。Car_01通常使用`DA_Car01_Presentation`，新车型以资产生成器输出及实际引用为准，不统一修改PlayerState或GameMode。

| 位置／字段 | 默认与范围 | 含义 |
|---|---|---|
| Suspension → Enabled | false | 团队增加四个悬挂节点并检查静态基准后再勾选 |
| Response Seconds | 0.08秒，0～2 | 指数时间常数，约三倍时间达到95%；0用于即时校准观察 |
| Probe Margin Cm | 2cm，0.1～50 | 探测余量；不增加轮子允许行程 |
| Minimum Support Normal Dot | 0.5，0.25～1 | 法线相对世界Up和车辆Up均须满足；默认约60°范围 |
| Minimum Vehicle Up Z | 0.5，0.25～1 | 车体超出允许倾斜时停止探测、回到中立 |
| Wheels → 每轮 → Suspension → Max Compression Cm | 10cm，0～100 | 最大上压行程，车型独立、逐轮可调 |
| Wheels → 每轮 → Suspension → Max Extension Cm | 15cm，0～100 | 最大下伸行程，车型独立、逐轮可调 |

10／15cm仅为关闭通道下的保守测试默认，不是由车型尺寸计算出的正式数值。根据轮拱、轮径和模型比例调节，可单独设0锁定某一侧行程。不要重填旧轮半径、转向轴或已有俯仰参数。所有配置即使Enabled关闭也校验有限值／范围；非法配置按既有表现回退处理，不改变Vehicle Definition玩法有效性。

## 4. Unreal Editor手工接线

依据根目录AGENTS.md第8.3节，代码代理不修改Blueprint／uasset。完整编译结束后，由团队操作：

1. 重启Editor，打开该车型现有AnimBP（例如`ABP_Car01`），父类保持`HALVehicleAnimInstance`，Skeleton保持该Mesh的Skeleton。已有轮转、转向和Chassis节点保留。
2. 在AnimGraph原有`Local To Component`与`Component To Local`之间，为**每条轮链**增加一个`Transform (Modify) Bone`，位于该轮Steer／Spin之前。后轮没有Steer节点时，放在Spin之前。
3. 四个新节点的`Bone to Modify`分别选择当前DA内FL／FR／BL／BR对应的**Suspension Bone**。Car_01约定为`Wheel_FL_suspension_jnt`、`Wheel_FR_suspension_jnt`、`Wheel_BL_suspension_jnt`、`Wheel_BR_suspension_jnt`。节点不会因DA骨骼FName变化自动重绑定。
4. 每个新节点设置：**Translation Mode = Add to Existing；Translation Space = Bone Space；Rotation Mode = Ignore；Scale Mode = Ignore；Alpha = 1**。不要用Replace Existing覆盖骨骼原始平移。
5. 从原生只读变量`Presentation`读取对应轮位，拆分／Break轮姿态结构，将该轮的**Suspension Translation（Vector）**接到新节点Translation。不要把诊断用`Suspension Offset Cm`直接塞入Make Vector，也不要把FL接到FR。
6. 若新字段未显示，先Compile，再对已有Break／拆分节点Refresh Nodes；必要时重建该读取节点。Event Graph保持原状，不新增Tick、Trace或位移计算。
7. Compile并Save AnimBP。选中实际车辆Definition，在现有HAL Vehicle Visual Alignment检查静态轮底；已对齐时不要累积手动偏移。碰撞代理厚度／包覆形状仍需独立检查。
8. 打开对应表现DA，先展开四轮Suspension检查行程，再展开总体Suspension勾选Enabled。Response可先设0观察单轮方向，确认后使用0.08等平滑值。保留其余已调参数，Save。
9. 停止旧游戏，重新从测试场景或双进程进入。先看平地，再在一个轮位下放一个低台阶；代码代理不代为改地图。测试台阶对象类型为WorldStatic，简单碰撞启用Query，顶部高度先小于压缩行程，宽度只覆盖目标轮位。
10. AnimBP Debug Filter选择实际车辆实例，查看`Presentation → FL／FR／BL／BR → Suspension Translation／Suspension Offset Cm／Suspension Supported`。若数值变化而车轮不动，检查节点骨骼、空间、Alpha和向量连接；若整个骨骼表现回退，查看Output Log的配置错误。

```text
Reference Pose → Local To Component → 已有Chassis
→ FL Suspension → FL Steer → FL Spin
→ FR Suspension → FR Steer → FR Spin
→ BL Suspension → BL Spin
→ BR Suspension → BR Spin
→ Component To Local → Output Pose
```

已有后轮Steer节点可保留并继续输入零。父级悬挂先平移，子级Steer／Spin再旋转。四个节点在同一次Component Space控制链中，不需要为每个节点重复Local／Component转换。

## 5. 调试与人工检查

控制台`hal.VehicleSuspensionDebug 1`开启，`0`关闭。绿线为接受的支撑、红线为无有效支撑、青点为目标轮心；侧翻／自救时没有探测线。每辆车每秒最多一条`HAL VisualSuspension`日志，记录角色、Active、实际Trace数、查询耗时、四轮世界偏移及支撑标记。默认关闭，不影响规则。

`Presentation.SuspensionTraceCount`为该帧实际场景查询数，开启且姿态允许时每车4次、四车16次；关闭／侧翻／自救为0。`SuspensionQueryMicroseconds`只计游戏线程Line Trace调用块，不含完整动画、渲染或GPU开销。`stat game`中的HAL Visual Suspension计整个悬挂更新。真实四车性能仍需在最终有线PC环境测量。

人工至少覆盖：

- 平地静止／前后行驶：轮底接近基准，没有周期性下沉或累积偏移；原轮转／转向／俯仰正常。
- 单轮低台阶、坡面、离开台阶：对应轮位独立变化，平滑无明显跳跃；不要把超过行程仍悬空误判为代码承诺四轮贴地。
- 腾空、侧翻、墙边、自救、瞬移／重开：无命中伸长、倾斜归中、恢复后重新采样；不吸附墙面、不靠动画扶正物理车。
- 持球、发射、被击飞、加速／免疫：规则保持；球不会成为视觉悬挂支撑。
- Listen Server主机与客户端互换驾驶／观察，30／60／120FPS比较。表现基本接线测试不替代F4完整扰动／四台有线PC验收。

局限：中心线探测加局部平面／半径估计，不是完整轮胎体积Sweep；台阶边缘、极窄平台或轮宽接触可能与真实轮廓不同。默认只支持世界-Z重力下的四轮正立车辆；WorldDynamic平台、变化重力、额外Root／悬挂旋转动画、非均匀总缩放需另行设计。车轮可在极端姿态／超过行程时与地面分离，不能以视觉通道制造物理支撑。

## 6. 本轮验证

**代码、两个Target构建、普通自动化及基本双端数据验证完成；真人AnimBP接线／保存和画面尚待验收。** 跨设备证据见[本轮验证摘要](MVPB_Vehicle_Presentation_VP3_Verification_20261022.json)。

| 项目 | 本次结果及边界 |
|---|---|
| Editor Win64 Development | 成功；最终Editor_TestAxisFix.log，5.92秒；此前生产代码及网络夹具Editor_Fix.log亦成功 |
| Game Win64 Development | 成功；Game_Final.log，78.06秒；未改模块、插件或保存配置 |
| 完整普通回归 | **67/67、0测试警告／失败**；原63项＋4项悬挂测试，Automation_Final/index.json |
| 数学／配置 | 平地、正负高差、坡面轮径修正、行程、墙／背面／穿透／NaN、倒扣、自动骨骼空间转换和30／60／120指数平滑通过；默认关闭、范围与零行程兼容通过 |
| 场景／生命周期 | 真实简单Query场景、Movable WorldStatic地板、单轮7cm台阶、PhysicsBody／WorldDynamic排除、不累积参考偏移、空中伸长、自救／倒扣归中、长帧重置及动画只读副本通过；根变换／速度／角速度／质量／碰撞／模拟开关保持 |
| 四车查询测量 | 实际保存Car_01参考骨架、4车×120次更新、1920条射线；平均四车Line Trace合计**19.555µs**，单车最慢33.904µs；仅NullRHI独立查询场景微测，不是渲染或四机帧率验收 |
| Listen Server／Client | 实际MVPB_NetTest、loopback端口7822、NullRHI限帧60，无主动扰动；**Host 1/1、Client 1/1，0测试警告／失败**。两端均观察本车／远端从平地中立变为FL=7cm、其余轮中立，每端两车8条本地射线；客户端18次重模拟 |
| 资产保护 | **667→667，全部SHA256相同**；没有改DA、AnimBP、Mesh、BP或地图；Config／uproject未改。专用测试进程均已退出 |

双端夹具只复制实际车辆的网络运动；每端建立相同的**本地QueryOnly平台**并使用临时表现配置副本，关闭夹具车辆重力、由服务器将车放到独立高空测试区域。它证明实际网络角色、参考几何与本地探测链路可用，不证明物理悬挂、地形复制、正常驾驶台阶观感或AnimGraph最终蒙皮。没有修改保存资产，也没有新增客户端权威命中入口。最终要在正常地图按第5节真人驾驶检查。

失败历史保留：首轮Editor构建因新增Suspension成员与旧ValidateRig局部索引同名触发C4458，改名SuspensionIndex后成功。普通首轮65/67，两项测试对`FRotator(0,0,90)`的局部Y符号预期写反；世界空间重建与实际查询／规则边界断言原已通过。对照本机引擎FRotator四元数实现，修正三条测试期望为负Y，生产换算不改；最终完整67项重新通过，未删除断言或把首次失败覆盖为成功。

普通与双端报告均为本轮新跑。双端游戏启动时各有21条引擎EditorDataStorageUI工厂注册警告，发生在自动化之前并原样保留；测试报告中无警告／失败，日志未见Ensure／Assertion／Fatal。不宣称整个启动日志零警告。资产生成工具的菜单／保存／重启测试并未在本轮补做，原待验收状态保持。

本轮未在5.8.3复测、未做实际渲染或团队节点接线，未执行完整F4扰动／四台有线PC。下一步团队按第4节配置并反馈VP-3画面；确认后再讨论VP-4。源码／文档未提交／Push，跨设备前须正常提交，Saved日志不随Git同步。
