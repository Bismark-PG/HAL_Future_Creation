# 铁链钩取与侧向投掷技能 v0.1

## 已确认规则

车辆原地制动、向实际车头方向出钩；命中位置开始带链甩动，不把目标先拉近。出钩、选择方向、甩动和正常收钩期间禁止驾驶、漂移、主动击球、再次放技能与自救。驻车用有限制动力，不钉死世界坐标，仍可被普通外力推开。眩晕、自救执行中、腾空或翻覆不能发动。

钩子只捕获第一辆有效车辆，普通墙／实体阻挡体阻断；球与触发区域不被抓取，返回途中没有攻击检测。同一目标同时只有一个服务器抓取归属。命中后目标获得本技能独立来源的通用 `State.Stun`，**按用户最终更正，立即掉球**；没有保球特例。抗冲击不自动免疫抓取、眩晕或技能投掷。钩中／投掷本身不扣血。

命中时锁定施法车的左右方向和甩动参考圆心。命中前已按住方向不能直接投掷：窗口内先处于中立，再第一次输入左／右开始向对应方向甩动。左／右复用原转向输入，无新 IA／IMC；客户端只提交方向、序号及技能激活身份，不上报目标或命中。窗口从服务器确认命中开始，超时释放目标、收钩；没有默认自动投掷。

甩动期间，目标保持本钩子的抓取归属和 Stun；物理移动层沿命中位置开始的平面圆弧逐渐加速，钩头全程跟随目标实际连接点，链条保持绷紧。物理线程消费完甩动阶段后，才脱钩、移除本钩子的眩晕并收钩；电墙／警车等独立来源的眩晕继续。脱钩保留实际横向速度，只提供最低上抛速度；不会先刹停再赋予固定侧向速度，也不会把墙体消耗掉的速度补回来。重力、碰撞及更高的已有上升速度继续有效。施法者直到正常收钩结束才恢复驾驶。

施放任一阶段受到实际生效的击飞、Stun、强制反弹或翻覆／自救等外部失控，立即中断：取消抓取、移除自身动作锁，钩子快速收回。普通水平推动不触发该中断，自身驻车锁也不算失控。已经甩出的目标不回滚速度。淘汰、换车、离场和销毁按来源清理双方。冷却从成功出钩开始，落空／超时／中断不返还；配置或生成失败不消耗冷却。

## 程序与 GAS 边界

- `UHALHookThrowAbility`：许可、冷却、单次技能生命周期；没有 GA Tick。
- `AHALHookThrow`：服务器扫掠、抓取、超时／中断及复制的阶段；客户端只重建铁链和钩子表现。
- `UHALHookComponent`：每个 Avatar 的唯一抓取归属、已有转向输入到方向请求的窄接口。
- `FHALHookMotion`：驻车／保持命中位置的有限平面力，以及带帧号的一次投掷。通过既有 Movement Bridge／Network Physics 历史执行，不传 ASC 或 Actor 到物理线程，不用 SetActorLocation 驱动车辆。
- 抓取 GE 与普通 Stun 共用状态 Tag，但保留独立 handle 和生命周期。普通 `ApplyStun` 只比较自己的持续效果，避免抓取尚余 1 秒时吞掉另一个来源的短眩晕。

投掷脉冲预约在命令到达后的下一权威物理帧；不假定立即动作一定早于同帧移动回调。实际内部刚体会在投掷前唤醒，有限驻车力显式访问物理线程内部数据。空闲 Hook 历史仅占一个标记位，活动时只发送对应模式数据；投掷执行租约被物理线程消费后清空当前值，历史仍保留以支持重放。短生命周期钩子的复制优先级为3，避免其在普通车辆更新之后一直被延后。

移动历史格式首轮11→12，本轮 **12→13**，Host／Client 必须同时更新并完整编译；推荐本轮实际引擎 UE5.8.3-58210709。不是新增车辆物理预测框架。

## 第二轮：带链圆弧甩动与连续脱钩（2026-11-17）

用户确认动作应为「选择方向 → 带链加速扫弧 → 沿切线脱钩飞出 → 收钩」。原先直接替换水平速度、同时解除连接的首版被本节取代；历史验证保留在文末，不能作为本轮复测。

圆心取命中时施法车的位置，甩动开始时以目标实际位置确定半径；施法车被普通外力推开不会把圆心每帧拖走。轨迹目标保持半径，实际运动由有限物理驱动完成，接触／墙体可阻挡或缩短弧线。钩头显示目标实际位置，不用理想轨迹假装目标已移动。

设半径为 R、扫角上限为 θ、弧长上限为 Lmax、时长为 T、加速指数为 p、Horizontal Speed 为 Vmax，则使用 `L = min(R*θ, Lmax, Vmax*T/p)`，θ按弧度计。进度为 `u^p`，从低速建立动量，末端不减速到零；理想末端速度为 `p*L/T`，不超过 Vmax。扫角／弧长／速度是上限，并非三个可独立保证达到的目标。物理帧量化、摩擦、阻挡和外力仍会影响实际行程及速度。放慢时间、提高加速指数或减小弧长都会影响末端速度，不会在脱钩时强行补齐到 Vmax。

甩动命令同样预约下一绝对物理帧，新增模式和几何数据进入已有历史；Actor 等待物理线程到达末帧才释放。中途施法者失控、目标失效或偏离理想路径过远时，立即取消本次驱动／抓取并收钩，保留目标当时动量，不执行额外甩出。抓取 Stun 的保险期限覆盖完整选择与甩动窗口，正常超时／结束仍主动按来源移除。

甩动开始、脱钩／收钩和中断使用可靠的离散阶段通知，同时保留状态属性给初始同步；每个阶段带递增序号，客户端忽略迟到的旧阶段。这避免短短0.3秒的甩动在延迟下被合并跳过，也防止旧包让已脱钩的链条重新接回目标。没有逐帧可靠广播，物理轨迹继续走已有历史与状态同步。

沿用已经配置好的 GA、技能 DA、材质和车型引用；无需重建资产或新增蓝图节点。完整编译并重开 Editor 后，在原 `DA_Ability_HookThrow` 的 Gameplay → Rules → Swing 中查看新增字段。旧 DA 未存储的新字段使用下面的原生默认值；本轮不覆盖团队已经调整的旧参数或保存任何已有 DA。

本轮最终 Editor_R7／Game_R2 构建通过。Native_R3 全量248项通过（246项无警告、2项带既有换车负例警告）；真实Solver左右末点约`(708.718,±245.396)`cm，半径750cm保持，脱钩前后横向速度约`(-502.497,±1540.394)`cm/s不变；中断动量和CCD墙阻挡通过。真实D3D12 Render_R1 1/1检查了实际移动钩头、最后链节和目标连接点，并保存／观察甩动两端渲染图。实际Listen普通Network_R2、Host出站注入80ms的Network_Lag80_R3，双端均各1/1、零测试警告／失败、退出0；包含甩动途中外部Stun中断、独立状态清理和脱钩连续性。详见[第二轮验证摘要](Hook_Swing_Verification_20261117.json)。

测试维护只改临时夹具：旧免疫／加速／Stun／回房测试明确使用各自所需技能，避免Car01更换生产技能后改变测试前提；PostSolve位置取本帧已求解位置，按实际结束帧取样。延迟夹具等待观察者确认首轮脱钩才开始第二轮，避免跨组属性包到达顺序影响测试编排。1376份原资产／配置／共享入口哈希一致。上述是程序验证，真人弧线幅度、节奏和连接观感仍待用户，未进行四机或实际Cook／发布包测试。

## BP／DA 配置

资源由原生编辑器工具创建；BP 仍按 AGENTS.md 第8.3节由团队人工接线。车型未在需求中指定，本轮不擅自替换任何车型主技能。

1. Content Browser 打开 `/Game/Data/Abilities/DA_Ability_HookThrow`，类型应为 **HALHookThrowConfig**；所有玩法和表现调参都在这份 DA。
2. 在 `/Game/Blueprints/GA` 新建 Blueprint Class，All Classes 搜索父类 **HALHookThrowAbility**，命名 `GA_HookThrow`。Class Defaults → **Hook Throw → Skill Config** 选择上述 DA，Compile／Save。Event Graph 保持空；不用另建 GE。
3. 打开要测试的车辆 **Vehicle Definition DA**。Abilities → Version 使用 **Vehicle Definition V1**，找到唯一 `Input.Ability.Primary` 的 grant，将 Ability 换成 `GA_HookThrow`，Level 保持1；不要给同一输入同时授予两个主技能。其他配置保持原值。
4. 车辆 BP 继续引用自己的 Definition。已有 **Primary Skill Action** 和 IMC 无需改动，左／右就是原来的转向键或左摇杆。
5. 选择健康、正常站地的车辆释放主技能。钩中后先松开方向，再按左／右。超时会自动收钩，主动投掷后仍有收钩后摇。
6. 测试木桩沿用新版 `HALSkillTestDummy`，不需要添加专用蓝图事件；它继承的 Hook 接收组件和 ASC 接收抓取／眩晕／物理命令。旧 Passive 仍用于基础物理回归。

## 参数与首测默认值

| DA 区域 | 参数 | 默认／单位 | 用途 |
|---|---|---|---|
| Timing | Cooldown Seconds | 12 s | 成功出钩后冷却 |
| Gameplay → Hook | Range | 1500 cm | 最大出钩距离 |
| Gameplay → Hook | Outbound Speed／Return Speed | 2500／5000 cm/s | 出钩和收钩速度 |
| Gameplay → Hook | Hit Radius | 28 cm | 权威扫掠半径，与视觉尺寸独立 |
| Gameplay → Hook | Outlet Offset | (170,0,45) cm | 物理根局部出钩位置；X前、Y右、Z上 |
| Gameplay → Input | Choice Seconds／Direction Threshold | 1.2 s／0.35 | 方向窗口与中立阈值 |
| Gameplay → Swing | Swing Seconds | 0.3 s | 带链甩动时长，0.1～1.5 s |
| Gameplay → Swing | Swing Angle | 30° | 扫角上限，1～80°；左右自动对称 |
| Gameplay → Swing | Swing Arc Length | 250 cm | 弧长上限，20～1500 cm，避免远距离扫动过大 |
| Gameplay → Swing | Swing Acceleration Power | 2 | 加速指数，1.2～4；越大越晚发力，末端不归零 |
| Gameplay → Swing | Swing Maximum Acceleration | 20000 cm/s² | 甩动驱动的加速度上限，1000～50000 |
| Gameplay → Throw | Horizontal Speed／Upward Speed | 1800／250 cm/s | 甩动末端速度上限和脱钩最低上抛速度；实际水平动量连续保留 |
| Gameplay → Physics | Capture Stiffness／Damping／Maximum Acceleration | 80／18／8000 cm/s² | 保持命中位置的有限力及驻车制动 |
| Gameplay → Physics | Maximum Capture Drift | 350 cm | 目标被外力推离过远则脱钩 |
| Gameplay → Interrupt | Airborne Upward Speed | 100 cm/s | 自然接触观测的腾空中断补充门槛 |
| Gameplay → Debug | Draw Bounds | false | 临时显示钩头权威范围 |
| Presentation → Assets | Chain Mesh／Hook Mesh／Metal Material | 三份新资源 | 可替换美术，组件无物理碰撞 |
| Presentation → Geometry | Link Pitch／Link Scale | 22 cm／1.15 | 链节间距与粗细尺寸 |
| Presentation → Geometry | Hook Scale／Hook Chain Offset | 1.2／(-45,0,0) cm | 钩头大小与链条连接点 |
| Presentation → Geometry | Maximum Links | 256 | 单次表现链节预算，上限512 |
| Presentation → Motion | Sag／Wobble／Wobble Frequency | 12 cm／3 cm／3 Hz | 链条弧垂和轻微扰动；抓住时自动绷紧 |
| Presentation → Motion | Enter Seconds | 0.06 s | 钩子出现的短过渡 |
| Presentation → Metal | Dark／Middle／Light Color | 冷色金属三档 | 柔和卡通配色 |
| Presentation → Metal | Light Direction／Shadow Threshold／Highlight Threshold | DA内可见 | 体积明暗方向及色阶分界 |
| Presentation → Metal | Tone Softness／Glint Strength／Glint Power／Contour Strength | 0.16／0.35／20／0.35 | 过渡、金属反光和轻微轮廓压暗 |

参数会在每次释放时捕获快照；调整 DA 后重新释放。范围／速度／窗口还需满足总寿命不超过29秒的校验，不应把高速移动改成无限驻车。颜色、模型、命中范围分别可调，修改视觉模型不会偷偷改变命中。

编辑器与运行时范围一致：Range 200～4000 cm；Outbound Speed 200～10000、Return Speed 500～15000 cm/s；Hit Radius 5～80 cm；Choice Seconds 0.2～3 s；Horizontal Speed 100～6000、Upward Speed 0～1500 cm/s；Cooldown 0～120 s。驻车 Damping 1～80、Capture Stiffness 0～300、Maximum Acceleration 100～50000 cm/s²。Link Pitch 8～80 cm，两个 Scale 0.3～4，Maximum Links 16～512；Sag 0～80、Wobble 0～30 cm，Wobble Frequency 0～20 Hz。三档颜色 RGB 0～4；Tone Softness 0.02～0.4，Glint Strength 0～2，Glint Power 2～80。先调投掷速度与方向窗口，再调整粗细／颜色，最后调整链节密度，避免同时改动多项而无法定位原因。

## 资源及施工约束

新资源位于 `/Game/Assets/FX/HookThrow`：`M_HookThrow_ToonMetal`、`SM_HookThrow_ChainLink`、`SM_HookThrow_Head`。三档柔和过渡、交错的有厚度链节和收尖钩头延续卡通金属规范；Unlit 材质使用表面法线／视线的人工三阶明暗与高光，保持环境内可读性。铁链用有上限的 ISM 实例排列，钩头为独立网格；不模拟链条刚体，不让视觉组件参与攻击碰撞。

不复用／覆盖 Car06 的铁链资源或修改其技能 DA；新材质参数由本技能 DA 推入实例。替换 Mesh 时同时校准 Hook Chain Offset、Link Pitch、Link Scale 和 Hook Scale。链条不做复杂绕障碍缠绕；有效命中和墙体阻挡由服务器钩头扫掠判定。

`HALBuildHookThrowFXCommandlet` 仅用于首次原生制作，**同名资源已存在就拒绝覆盖**；不是日常调参入口。应使用引擎命令类加载语法 `-run=HAL_Future_CreationEditor.HALBuildHookThrowFXCommandlet` 调用，不能为了重跑而删除队友调过的素材。Definition／GA 对 DA 的硬引用会带入这些新资源；发布前仍应执行实际 Cook 检查。

## 验收入口

`hal.HookLog 1` 查看出钩、抓取、释放和中断；`hal.HookPhysicsLog 1` 查看实际内部物理帧、睡眠状态及投掷速度。测试结束均恢复0。原生专项为 `HAL.FutureCreation.HookThrow`，真实渲染专项为 `HAL.RenderSmoke.HookThrow`，双端为 `HAL.NetworkSmoke.HookThrow`（独立测试进程使用 `-HALHookFixture`，不在日常游戏开启）。

双端自动夹具可执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'D:\HAL_Future_Creation\Tools\TestHookThrow.ps1' -EngineRoot 'D:\Program Files\Epic Games\UE_5.8' -Port 7933
```

它在各进程中创建临时隔离地面、只为出生实例替换技能授予，第一轮由客户端选左、服务器验证真实位移，第二轮由服务器施加外部眩晕验证中断；不会编辑保存的BP／地图／Definition。日志／报告在 Saved/HookThrow，进程由脚本自己创建与清理。`-HostLagMilliseconds 80`仅向Host出站注入80ms延迟，不表示实测RTT。

真人需要覆盖：左右输入和预先按住、空钩、墙角／斜坡、木桩与移动玩家、抓到持球车立即掉球、抗冲击目标、额外眩晕来源、施法者在各阶段被击飞／Stun、目标或施法者淘汰／换车／离场、延迟、特效连接点与四车同时施放的可读性。程序测试不替代这部分手感与观感验收。

## 2026-11-17 首轮工程验证（历史；不代表第二轮复测）

最终 Editor_R16／Game_R4 完整构建通过，UE5.8.3-58210709。Native_R6 全量247项通过：245项无警告，2项为既有换车负例测试警告，失败／未运行0。专项覆盖命中立即掉球、独立眩晕保留、冲击免疫目标、重复抓取、方向身份／中立规则、墙阻挡、超时、Avatar 清理、有限物理命令、单帧投掷和历史序列化；空闲 Hook 历史增加量断言为1bit。

Render_R2 使用真实 D3D12 离屏渲染，1/1、零测试警告／失败，观察了交错链节与钩头、三阶金属像素以及收钩。Network_R8 实际 Listen 双端各1/1、零测试警告／失败；Host／Client 观察到约452.92／452.33 cm的向左位移。Network_Lag80_R1 同样双端各1/1；Host 出站注入80ms，观察到452.93／461.76 cm，不能解释为实测RTT或固定投掷距离。投掷由真实内部物理帧执行，横向目标1800 cm/s、最低上抛250 cm/s，地面摩擦与碰撞仍会改变行程。外部 Stun 中断第二次捕获、双方清理和无额外扣血均通过。

早期失败保留在本地日志：原生夹具帧确认与提前销毁清理、物理力使用 External API、空闲历史的带宽负担、同帧回调时序漏投掷，以及低频 PlayerState 短 Stun 未被观察到；这些问题均已修正并重新验证。现在 Stun 起止调用 ForceReplication，使普通 Stun 与钩取都能及时同步，玩法持续时间不变。详见[可同步验证摘要](Hook_Throw_Verification_20261117.json)。

保护检查：除新增文档白名单的 `.gitignore` 外，开工时1370份已有资产／Config／uproject／共享启动入口哈希不变，包含用户已修改的测试地图与新增 Car08 GA。仅新增本技能的4份资源。尚未进行团队真人手感／技能车型接线、四机压力与 Cook／发布包验证；当前为可接线测试的首版，不表示最终美术验收。
