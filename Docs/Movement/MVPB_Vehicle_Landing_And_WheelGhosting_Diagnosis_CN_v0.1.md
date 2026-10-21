# 侧面／车尾落地与车轮虚影诊断（v0.1）

后续状态：用户已修正车轮配置并反馈表现正常；随后授权的普通静止支撑修正已完成代码、构建和自动化检查，详见[实施与复测入口](MVPB_Vehicle_Support_Classification_Fix_CN_v0.1.md)。下文及只读检查JSON保留修正前的诊断时点，不表示车轮仍待修或本轮仍未施工。

日期：2026-10-21；起点`main/747eac7`，检查期间团队已将共享规则接线与此前文档提交为`c747e4b`，源码未变；最终证据基点为该提交。UE5.8.3-58210709。用户要求**先检查、分析与提出修复方法，暂不修复**。本轮只读源码、既有游戏日志、保存DA／地图和AnimBP节点／连接；未修改玩法源码、Config或资产，也未重新编译／开展新PIE与画面对照。

最新事实与证据见[只读检查JSON](MVPB_Vehicle_Landing_And_WheelGhosting_Inspection_20261021.json)。上一轮[52/52等施工验证](MVPB_Vehicle_Stability_MinimalFix_Verification_20261021.json)保留历史边界，本次发现其静态地面夹具没有覆盖实际地图的Movable地面，不能继续将它解释为所有正式场景接触均已正确过滤。

## 1. 当前接线与用户反馈

- 用户报告：弹簧拳击飞后车顶着地较快停止弹动；侧面／车尾着地偶有弹动；行驶时轮胎有细小前后运动与虚影。
- 当前`DefaultGame.ini`已启用共享规则，引用`DA_Vehicle_Stability_Test_v1`。新DA与`/Game/Data/Physics/PM_Vehicle_LowBounce_v1`为团队新增资产，检查开始时未提交，期间已随`c747e4b`提交；本轮保持其内容。
- 保存DA已Ready，SuppressSupport=true、坡度45°；PM为Restitution=0、OverrideCombine=true／Min、Friction≈0.7、StaticFriction=0。
- 最新游戏日志多局确实输出`VehicleStabilityRules Snapshot=492667503 ... Restitution=0.000 SupportSlope=45.0 SuppressSupport=1`。它与旧临时测试夹具签名不同是正常的：实际资产路径也参与签名，不能据此认定网络配置冲突。
- 本次保存检查共363份uasset／umap，检查前后SHA256全部一致；未执行资产Save／Reimport／显式CompileBlueprint。

## 2. 落地弹动：确认的过滤遗漏

[Receiver接触分类](../../Source/HAL_Future_Creation/HALImpactReceiverComponent.cpp:253)要求：WorldStatic、非模拟、**Mobility必须Static**、来源无车辆／球／特殊ImpactSource，再通过法线与冲量方向坡度检查才清除追加。

实际保存的两张地图却都是：

| 地图／组件 | Mobility | Collision Object Type | Simulate Physics |
|---|---|---|---|
| Test／Floor_0.StaticMeshComponent0 | **Movable** | WorldStatic | false |
| MVPB_NetTest／Floor_0.StaticMeshComponent0 | **Movable** | WorldStatic | false |

`Mobility=Movable`表示组件允许移动，不能单凭它断言组件正在运动。当前普通地面会在这个条件被排除，即使规则已启用且法线向上，也无法进入支撑过滤。**这是上一轮实现与实际地图接入之间的遗漏；此前只读基线也含这个值，上一轮应当纳入判断和回归覆盖。**

最新既有日志还有直接的异常命令证据：

| 日志UTC时间 | 来源／有效档 | 支撑过滤 | 接受的规则命令 |
|---|---|---:|---|
| 04:16:19.236 | SpringPunch／Medium | 0 | 水平600，最低向上500cm/s，属于正常显式击飞 |
| 04:16:20.528 | Collision／Medium | **0** | 向上351.645cm/s，RuleUp=0 |
| 04:16:28.053 | Collision／Heavy | **0** | 向上535.522cm/s，RuleUp=0 |
| 04:16:37.649 | Collision／Medium | **0** | 向上456.742cm/s，RuleUp=0 |

日志区分了显式攻击的最低向上速度和普通碰撞法向追加。上述数值是**接受／排队的命令**，不是已经积分后的速度。当前日志未打印接触Actor、组件和法线，不能把每一条Collision精确归属到Floor，也不能仅凭这些行量化每次画面弹跳；但保存地面必然漏过过滤，以及普通碰撞仍在排队向上速度这两点都已确认。

### 2.1 为什么侧面／车尾更容易表现出来

先前真实Solver诊断已显示：低回弹消除部分离地反弹，但重翻后仍约9.63游戏秒才进入严格低速稳定，且最终未必正立。因此剩余运动至少要区分：规则追加、接地摇摆和真正整体离地。

车顶较平稳着地时，支撑面／角运动可能更快收敛；侧面、尾端、边角着地时接触点会切换，转动能量和碰撞体尺寸更影响摇摆。当前代理456×288×269cm，仍不是扁平车体；没有四点物理悬挂或有限落地辅助。以上是结合已测几何与Solver结果的物理解释，尚未对用户这几次落地做逐帧姿态复现。

另外，接触追加预算使用接触点速度，而非仅质心速度；角速度通过`W×r`影响预算，侧翻时两者可能差异较大。当前异步接触回调的测量时序也应记录后再分析，不凭本轮日志直接重写预算公式。源码的单条向下接地射线仍未加入姿态驾驶门控，侧翻时驾驶／抓地力也可能继续参与，不能把所有残留运动归为材质弹性。

### 2.2 建议修复顺序（本轮未执行）

1. **先补齐支撑分类。** 对确认不会运动的测试Floor，可先手工改为Static做最窄场景对照。正式代码则应区分“普通稳定支撑”与“组件允许移动的标志”，让当前这种WorldStatic／非模拟的普通地面进入支撑识别；保留球、车辆、移动平台／特殊机关的独立来源语义，不能把所有朝上碰撞一律吞掉。
2. **补实际接触诊断与回归。** 记录来源Actor／组件、Mobility／ObjectType、几何法线、法向冲量、Natural／RelativeAfter、过滤原因和动作帧；覆盖实际保存地图、Static地面、Movable但静止的WorldStatic地面、真实移动平台、水平墙、斜坡、球与显式机关攻击。尤其不能再仅用人工Static地面声称实际接入已覆盖。
3. **保持档位与脱球。** 修正只改变普通支撑的规则追加，不早退冲击结算，不改有效档、独立伤害／免疫、持球脱落或显式击飞；弹簧拳Medium的RuleUp=500应继续保留。
4. **再看纯自然摇摆。** 支撑追加确为0后，重复车顶／侧面／尾端着地。若仍过长，先核对代理厚度、轮底／模型相对位置与质心惯性，再设计近地、短时、有上限的Pitch／Roll耗散或有限辅助，保留Yaw与攻击起飞阶段。姿态驾驶门控和长按自救继续按原已确认计划实施。

不采用全局大阻尼、锁Pitch／Roll、每帧清零Z或一落地自动扶正。低回弹不承诺任何着地姿态都立即静止。

## 3. 车轮：确定的半径错误与渲染嫌疑

### 3.1 已排查的路径

- [C++表现组件](../../Source/HAL_Future_Creation/HALVehiclePresentationComponent.cpp:176)只输出SteeringRotation／SpinRotation，没有轮胎前后平移或悬挂平移；VP-2～4仍未实现。
- 只读导出实际`/Game/Blueprints/AnimBP/ABP_Car01`，原AnimGraph的六个ModifyBone节点全部是Translation=Ignore、Scale=Ignore、Rotation=Additive／Bone Space；对应FL／FR转向与四轮Spin引脚连接一致。没有发现“误接平移／错误旋转空间”证据。
- 当前Mesh、Skeleton与原检查文件SHA一致。原几何／参考姿态显示四轮spin局部Y对应轮轴，轴心与轮网格包围盒中心基本重合；未见明显偏心或跨轮权重，不能未经新证据就要求团队重做蒙皮。
- Mesh、表现相对Transform与物理Scale均为1；保存四轮`RadiusCm`却全部是**10**。该模型半径约**47.5cm**。转速按`速度/半径`计算，所以现在约为正确值的**4.75倍**。

半径错误会让轮胎纹理和轮辐每帧跨过更大角度，加重跳动、走样及拖影的可能，但它本身不会让spin骨骼中心平移。因此“视觉上轮子前后晃动”与“骨骼中心真的前后移动”必须分开验证。

### 3.2 当前渲染证据与尚未确认内容

新只读进程默认CVar为`r.AntiAliasingMethod=4`（TSR）、`r.DefaultFeature.MotionBlur=1`、`r.MotionBlurQuality=4`；用户原游戏启动日志也有TSR与MotionBlur质量设置。默认值不排除摄像机／PostProcess的覆盖，本轮没有新画面、速度缓冲或骨骼运行轨迹采样。

TSR使用帧历史及运动向量进行重投影；运动向量不正确或细节变化过快时可能有残影。Epic建议先切换／关闭抗锯齿来隔离TSR，并核对运动向量；运动模糊同样依赖速度缓冲。[Epic TSR排查](https://dev.epicgames.com/documentation/en-us/unreal-engine/temporal-super-resolution-in-unreal-engine)、[Epic Motion Blur](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-motion-blur)。这是本项目的**优先排查假设**，尚未证实就是当前虚影的唯一来源。

表现组件目前在PrePhysics采样刚体速度并积分轮角，根刚体由异步物理／联机预测更新；这种取样时序在发生修正时也可能影响转速平滑，但当前没有证据说明必须改TickGroup。只调整Tick顺序也不能解释或消除已存在的半径错误。

### 3.3 建议修复与对照（本轮未执行）

1. **先校准四轮半径。** 当前总缩放1，可用47.5cm起步，后续按世界实测半径调整；保留已正确的Steering／Spin轴与符号。未来若缩放变动，RadiusCm随世界半径换算。
2. **单独隔离运动模糊。** 同一路段、同车速先记录原CVar，再临时`r.MotionBlurQuality 0`。若虚影明显减少，进一步调游戏摄像机／后处理的模糊强度，而不是改车辆物理。
3. **再隔离时间累积。** 保持模糊关闭，临时`r.AntiAliasingMethod 1`（FXAA），与原TSR比较。若残影随之消失，检查骨骼运动向量／Previous Bone Transform、动画求值是否跳帧、组件Per Bone Motion Blur设置和TSR历史重投影；不要直接把永久关闭TSR当作最终方案。结束后恢复记录的原值；当前默认参考为质量4／AA方法4，修改画质后先重新查询，不能盲目写死。
4. **若FXAA且无模糊仍有位置晃动，采实际骨骼。** 在稳定直线行驶时记录spin／steer骨骼相对同一SkeletalVisual的变换，叠加骨骼轴心标记；纯直线下轴心应固定。若骨骼中心稳定而轮缘晃动，继续查几何／着色；若中心确实动，再核对运行姿态链、实例覆盖和模型参考姿态。
5. **最后区分整车与单轮。** 如果车身也同步抖，才扩展到根物理插值／网络纠正和表现取样时序；只有轮胎虚影时，先解决半径和渲染路径。不额外叠一套视觉位置平滑，以免与既有网络物理插值重复。

## 4. 范围、验证与下一入口

本轮只读进程最终成功、0命令行错误／警告；初次Python类型入口和对象迭代器查询失败，以及直接读取受保护图属性的限制已保留。最终通过原生类路径、只读对象迭代和T3D文本副本取得实际节点／引脚，没有修改蓝图。加载时引擎可能建立派生缓存，不等于显式编译或保存Blueprint。

未进行新C++构建、自动化、物理夹具修改、游戏新复现、30／60／120FPS或Host／Client新对照；不将旧Solver／52项测试当作本轮复测。全部363资产保持，团队DefaultGame.ini及新增DA／PM保留。

建议下一修复小步是**补支撑分类与真实地图回归，同时由团队校准轮半径并做模糊／TSR对照**；根据对照结果再决定渲染／取样修正和有限落地辅助。自救规格与VP-1结束状态保持，当前没有自动实施这些修复。
