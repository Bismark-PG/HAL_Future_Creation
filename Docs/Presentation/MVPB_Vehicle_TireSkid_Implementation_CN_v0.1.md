# 车辆漂移／急刹拖尾：实现与团队配置

日期：2026-10-28（Asia/Tokyo）；施工基点`main/08b43d0`，本机UE5.8.3-58210709。用户同意在动态摄像机等事项之前，先接入已有轮胎拖尾素材。本文不是四点物理悬挂或自然落地辅助施工记录。

## 1. 职责与范围

- 原Presentation DA增加`Tire Skid`分组；配置车型使用的Niagara系统、轮位、触发阈值、位置偏移和现有User参数。
- C++表现组件执行漂移／有效制动判定，管理每轮独立Niagara段、地面采样与清理；复用既有表现Tick，无新增Actor Tick、GA或GE。
- 动画蓝图和权威运动／伤害／球状态规则不变。物理线程只在已有锁保护的表现快照里增加已消费输入的三位需求，不改变输入协议或格式3物理历史。
- 本车使用当前本地输入与实际运动反馈；服务器读取真实消费的输入，沿既有最高15Hz表现发布节奏更新一个字节，远端使用该字节与各自收到的车辆运动。粒子、地面痕迹及逐帧位置不复制。
- 旧资产默认`Enabled=false`，本代理不修改`.uasset`／`.umap`、BP或Niagara图。无效拖尾配置只禁用拖尾并输出警告，不让正确车型退回占位模型。

## 2. 已有素材的实际接口

只读加载及T3D导出确认`/Game/Assets/FX/NS_TireSkid`是Niagara System，含`Minimal` Ribbon与`Sparks`发射器；本次不保存资产。

| 项目 | 当前保存资源 |
|---|---|
| User参数 | `User.SlipRatio`：float；`User.RibbonID`：int，默认均0 |
| 痕迹材质 | `/Game/Assets/Materials/M_TireSkid`，透明、Unlit、双面，支持Niagara Ribbon |
| 空间 | 发射器使用World Space，旧痕迹不会跟随整辆车移动 |
| 当前形状参数 | Ribbon初始化寿命3秒；宽度乘数18，强度参与资源内部计算。不是新代码强制的车型参数 |
| 当前贴地朝向 | Ribbon Renderer的Facing Mode为Custom，`Particles.RibbonFacing`初始值为世界`(0,0,1)` |

现有资源没有暴露独立的宽度／寿命／颜色User参数。当前先保留资源原值，通过DA的Slip Ratio调现有资源支持的强度；若要独立调宽度或残留时长，在Niagara编辑器调整对应模块，再视团队需要显式暴露User参数，不向未绑定参数写入假配置。

## 3. 团队手工配置

### 3.1 打开当前车型的表现DA

1. 更新并完整编译Editor Target后打开项目；本轮新增反射字段不能仅靠Live Coding加载。
2. 找到实际玩家BP使用的Vehicle Definition，沿其`Presentation`引用打开表现DA。当前车型资产为`/Game/Data/Car_Presentation/DA_Car_01_Presentation`和`DA_Car_02_Presentation`；以实际车辆引用为准，两种车型分别配置。
3. 不更改原Mesh、AnimClass、轮径、骨骼轴和ABP；不把Niagara放进AnimGraph，也不新增角色技能或PlayerState授予项。
4. 在表现DA展开`Tire Skid`，用下表作为首轮平地测试起点，保存DA并执行Validate Assets。

| 字段 | 初始配置／含义 |
|---|---|
| Enabled | 勾选 |
| System | `NS_TireSkid` |
| Wheels | 两项：`BL`、`BR`。可改为四轮，但必须唯一有效 |
| Start Speed／Stop Speed | 400／300cm/s；开始／保持阈值，停止阈值不得高于开始阈值 |
| Drift Start Lateral Speed／Drift Stop Lateral Speed | 150／100cm/s；漂移还须手刹保持与足够实际侧滑 |
| Maximum Ground Gap Cm | 8cm；实际装饰轮底与探测面的允许间隙 |
| Surface Offset Cm | 1cm；沿地面法线轻微抬高，避免痕迹与地面争深度 |
| Contact Offset Cm | `(0,0,0)`起步；按车辆局部方向微调，单位为世界cm，不随骨骼滚动 |
| Slip Ratio Parameter | `User.SlipRatio`，保持拼写 |
| Ribbon ID Parameter | `User.RibbonID`，保持拼写 |
| Slip Ratio | 1.0，初测先保证明显；范围大于0至1 |
| Surface Normal Parameter | **保持None**，当前素材没有此User参数 |
| Maximum Segment Distance Cm | 200cm；两个接地点跳变过大时重新起段，避免长线跨接 |
| Maximum Fade Seconds | 5秒；停发后的安全清理上限，须大于素材中最长粒子寿命 |

现有宽度／寿命调整位置：打开`NS_TireSkid`，选`Minimal`，在Particle Spawn中查看Initialize Particle的Lifetime，以及设置Ribbon Width的乘法输入。当前保存值分别3秒与18。不要将组件世界缩放当作独立轮胎宽度参数。`M_TireSkid`另有`SkidColor`和`OpacityScale`材质参数，需通过团队制作的材质实例／渲染器引用调整，不宣称本次DA已接入它们。

### 3.2 地面与轮位

使用已有四轮参考几何和世界轮径，并在视觉悬挂启用时考虑其装饰位移；不会读取正在旋转的spin骨骼位置作为下一帧的参考。视觉悬挂启用时共用同一轮点查询；关闭视觉悬挂仍可为拖尾独立查询，最多四条短Trace。轮位对应的地面不存在或间隙过大时，该轮停止生成。

探测继续使用WorldStatic对象类型，允许对象通道为WorldStatic的Movable普通地面，排除模拟刚体、车辆、球和WorldDynamic玩法物体。纯表现区域平面保持NoCollision；真正地面需有简单查询碰撞。地面对象类型与Mobility不是同一概念。

### 3.3 斜坡朝向的可选素材调整

代码会把Niagara组件朝向对齐命中法线，但当前Ribbon的每粒子Facing写死为世界向上，因此**首次平地试装可直接使用；完整斜坡贴合还需要团队修改素材绑定**。

可由团队在`Minimal → Particle Spawn → Set Variables`中，将`Particles.RibbonFacing`从固定`(0,0,1)`改为链接`Engine.Owner.SystemZAxis`，让新粒子记录当前组件的世界向上轴。保持World Space，不在Particle Update中把旧痕迹统一改成车辆的新朝向；地面法线变化时已有痕迹应保留生成时的朝向。

另一种明确接口是新建Vector类型User参数`User.SurfaceNormal`，在Particle Spawn绑定给`Particles.RibbonFacing`，然后将DA的Surface Normal Parameter设置为同名。未实际创建、绑定前保持None；校验会拒绝不存在或类型不匹配的参数。本代理没有执行上述任一种素材修改，也不把说明当作斜坡画面验收。

## 4. 触发与生命周期规则

- 漂移：手刹需求有效，平面车速达到开始阈值，实际侧向速度达到漂移阈值。保持阶段采用较低停止阈值；直线按手刹不自动等于漂移。
- 急刹：车体前向速度为正时使用制动键，或倒车中使用油门反向制动，且纵向速度达到阈值。制动键在低速兼任倒车，静止起步和加速倒车不触发。
- 松油门滑行、减速区域降低上限、单纯被撞产生侧滑且无主动需求，不触发。漂移和制动都结束时立即停发，不等待侧滑完全结束。
- 对应轮位需有效接地；空中、严重倾斜、驾驶许可失效、Stun和正在自救时停发。旧段停止生成后自然消退；重新接地／重新触发使用新组件与Ribbon ID，不与旧段连线。
- 明显位置跳变、长帧重置、Controller变化、重配表现及EndPlay清理当前和残留段；停止表现后没有孤立无限发射器。每车最多保留16个待消退段，并按Maximum Fade Seconds兜底销毁；这不是修改粒子自然寿命。
- Dedicated Server／无渲染World仍可检查纯表现判定数据，不创建Niagara组件。调试日志可用`hal.VehicleTireSkidDebug 1`开启、0关闭，仅在状态变化时打印Inputs／Reasons／Wheels及运动速度；Reasons的1=漂移、2=制动，Wheels按FL/FR/BL/BR位1/2/4/8，默认后轮为12。

## 5. 团队画面验收

1. 平地正常前进、松油门、静止按制动／倒车：无拖尾；高速制动出现后轮痕迹，停止后旧痕迹自然淡出。
2. 有车速时手刹转向产生实际侧滑：出现；直线手刹、低速原地操作或松开手刹：不持续生成。
3. 轮胎离地、坡边单轮失去支撑、被击飞、翻车及自救：对应轮停止生成；重新落地后重新起段，没有空中跨线。
4. 单独经过减速区／打滑区、无主动漂移或制动：无误触发；同时主动漂移时遵循各轮接地。
5. 先Host再Client互换驾驶，双方看到对方的出现／停止；画面时刻可受复制与插值影响，不要求粒子一模一样。
6. 验证两种车型的宽度、轮位、颜色和残留时长；必要时按3.3调整坡面Facing后补斜坡画面验证。记录截图／视频、参数及角色，不将原生测试代替美术验收。

## 6. 本轮验证状态

本机UE5.8.3-58210709，最终Editor／Game Win64 Development完整构建及链接成功；最后一次仅调整网络夹具后分别20.95／15.81秒。完整`HAL.FutureCreation` **88/88**，0测试警告／0失败；新增3项覆盖真实保存Niagara类型契约、轮位／阈值校验、漂移与反向制动／低速倒车／无需求侧滑／零停止阈值、真实地面查询、WorldDynamic排除、悬挂关闭时工作、离地／自救／重置。

真实D3D12离屏`HAL.RenderSmoke.TireSkid.ComponentLifecycle` **1/1**，0测试警告／失败；实际创建保存NS组件，检查User.SlipRatio覆盖、世界接地点与偏移、停发保留旧段、新组件重新起段及重置清理。此项验证组件和资源接入，**不包含渲染图像检查或真人美术验收**。

最终`MVPB_NetTest`两个独立进程、60FPS上限／NullRHI，端口7842的Host／Client **各1/1**；另在7843重复独立启动，**双方再次各1/1**，四份网络报告均0测试警告／失败，所有专用进程退出0。实际网络物理消费手刹需求，观察者位为1，释放后为0，同时保持既有双方转向表现检查；服务器另外确认实际消费输入。无人工延迟注入，不称作实测RTT／丢包／抖动／四机通过，也不以NullRHI状态同步冒充远端粒子画面验收。

1169份`.uasset`／`.umap`相对用户保存关闭Editor后的本轮保护清单逐份SHA-256完全相同；包含原本已有未提交修改的NetTest地图，未覆盖或保存资产。199份Source／Config／uproject最终哈希、构建与报告摘要见[共享验证JSON](MVPB_Vehicle_TireSkid_Verification_20261028.json)。Config／uproject未变，模块只增加已存在的内置Niagara依赖。原BuildAndLaunch及区域反馈文档改动保留，无提交／Push。

早期结果单独保留：首轮Editor链接失败来自测试夹具直接引用RHI全局，改为Engine公开接口后成功；首次拖尾专项2/3，地面断言虽通过但夹具Controller缺HALPlayerState触发GAS错误，补齐夹具后最终88/88；一轮双端已连接并Playing却未满足检查条件而超时，不作为通过证据。网络夹具随后增加就绪等待、持续模拟按住输入及两秒诊断，保留真实输入→物理消费→表现复制与释放归零的断言，没有直接设置观察者状态；最终连续两轮独立启动通过。最后一次网络夹具修改后，两Target、88项、真实RHI及这两轮双端均已复验，没有再修改C++。

编译输出仍含本机MSVC非首选版本提示及引擎既有弃用告警；“0测试警告”指自动化报告，不是所有引擎启动／编译日志完全没有Warning。原始日志／导出在忽略的`Saved/TireSkid/20261028/`，最终通过报告在`Validated/`及`ValidatedRepeat/`；早期`TireTests/`和`Final/Network_*`保留失败时点。

**当前下一步是团队按第3节启用保存DA并完成第5节实际画面测试**。初测重点为平地漂移／急刹、低速倒车不误触发、两种车型轮位／宽度／寿命以及Host／Client观察者；斜坡Facing尚需团队手工绑定。当前不是已完成资产接线或完整视觉／F4验收。

## 7. 团队初测反馈与后续入口（2026-10-28）

用户随后反馈“经过测试暂时没有太大问题”，记录为拖尾基础初测正向反馈，下一项转为动态相机0.1方案讨论。`36ed9b4`已提交本轮拖尾代码／文档；当前团队在Car_01表现DA及NetTest地图仍有本地修改，全部保留。本代理未重新导出这些保存值、检查最新Niagara图或运行新的测试；一般反馈不扩写为逐车型／网络角色／斜坡Facing／所有组合已经验收，前节详细矩阵继续保留。
