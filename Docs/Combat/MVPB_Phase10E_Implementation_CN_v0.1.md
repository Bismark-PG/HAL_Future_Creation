# MVP-B 10-E：主动冲击免疫GA／持续GE施工与操作（v0.1）

日期：2026-10-16（Asia/Tokyo）。原施工基点`main / aa278bd4a315c4afd0ccaf4eab7217d39e8b82cd`，原施工开始工作区干净；本机UE5.8.2 CL56702186。[已确认规格](MVPB_Phase10E_Ability_Effect_Plan_CN_v0.1.md)保持。**代码、Editor／Game编译、33项普通自动化和真实双进程技能请求／复制检查完成；随后团队接线已提交6850e8a，并反馈双端免疫生效／消失及效果正常。本次手动日志确认双向受击、伤害独立与HP收敛，完整扰动等细项仍待补。** 原施工证据见[验证摘要](MVPB_Phase10E_Verification_20261016.json)，新反馈与日志见第7节及[手动日志复核](MVPB_Phase10E_ManualLogReview_20261016.json)。接续从[Project_Status](../Project_Status_CN.md)开始。

## 1. 本轮行为与边界

服务器接受技能后，本车获得8秒`State.Combat.ImpactImmune`；发动时起算20秒冷却，均使用GAS的游戏世界时间。免疫GE与冷却GE分开，GA完成一次施加后立即结束，没有Ability Tick。免疫不免伤、不清除已有速度或冲量，继续保留自然推挤／阻挡／减速。

同一免疫GE按目标聚合、最多一层、成功重复施加刷新为8秒；冷却期间按技能会被拒绝，不能靠连按刷新。其他合法来源直接施加同一GE可刷新，但不重启主动技能的冷却。其他GE仍可独立配置叠层、刷新和到期策略，不存在全局禁止叠层。

车辆丢失／替换时清理`Effect.Scope.Avatar`效果，PlayerState上的冷却继续计时；Owner退出时移除全部活动GE并清理能力。只移除对应效果／作用域的贡献，不能清空其他GE或`HALImpactDebug`独立授予的同名免疫Tag。正式淘汰／重生、独立命中目标禁捕、加速／低抓地桥接和弹簧拳不在本轮。

## 2. 代码职责和未来扩展

| 入口／类型 | 职责 |
|---|---|
| `FHALAbilityGrant`／PlayerState.DefaultAbilities | 配置GA类别、输入Tag与等级；原生初始配置给主技能一项免疫GA。更换Pawn不重复授予／重置冷却；未来其他来源可复用同一授予接口 |
| `UHALAbilitySystemComponent` | 授予去重、输入Tag到Spec、GAS内置激活通道、作用域清理、只读剩余时间和可关闭日志 |
| `UHALImpactImmunityAbility` | InstancedPerActor／ServerOnly；检查Playing、有效Avatar／GAS就绪和冷却；提交冷却并施加免疫，失败时只退还自己刚提交的冷却 |
| `UHALImpactImmunityEffect` | 默认HasDuration=8秒；目标免疫Tag；Avatar作用域；同类一层刷新 |
| `UHALImpactImmunityCooldownEffect` | 默认HasDuration=20秒；独立冷却Tag；Owner作用域，无Avatar清理Tag |
| `VehicleLocalConfig.PrimarySkillAction` | 新增可选Bool Input Action，旧Local资产留空仍有效，不自动改旧键位／参数 |
| `FVehicleInputCmd.bPrimarySkill` | 只表达本地离散输入；连续RPC、物理历史捕获／反序列化会剔除，重模拟不再次发动GA |

输入为`Enhanced Input → FVehicleInputCmd → 输入Tag → ASC.TryActivateAbility → GAS服务器许可`。测试控制台复用同一入口；客户端只发送已有Spec的激活请求，不提交目标、免疫结果或持续时间。`Requested=1`在客户端只表示请求已提交，须以服务器效果和复制状态判断成功。

Avatar清理记录对象身份，旧Pawn的延迟回调不能清掉新Pawn的效果；销毁中弱Avatar引用已经失效时，仍能清理当前Pawn的作用域。授予和效果都不依赖玩家索引0、Actor临时名字或核心代码中的资产路径。

没有新增通用Buff管理器／技能树／装备框架。8／20秒默认值在原生GE的Scalable Float配置上，团队可用GE子蓝图调整；GA效果引用与PlayerState授予数组可配置。未改车辆／球／Combat DA已调数值，未添加逐帧技能物理逻辑。

为避免旧`HALImpactDebug`的Loose Tag复制策略与Mixed模式GE贡献冲突，该命令的独立免疫改用原生无限时调试GE。命令参数和用途保持，关闭时只移除它自己的句柄，不驱散技能／其他GE、不退还技能冷却；它不是正式8秒技能。此兼容修正已纳入多来源测试。

## 3. 无需新资产的双端试用

1. 保存工作并关闭Editor／游戏，使用本次完整构建的版本。按[双端启动说明](../Configuration/MVPB_DebugHUD_And_GAS_Test_CN_v0.1.md)从Entry进入，两端加入后由主机Start。当前技能要求`AHALMatchGameState.Playing`，旧Test地图未配置该GameState时会拒绝，先用NetTest。
2. 两端分别执行`HALGASStatus`。本地玩家应有`PrimarySkillGranted=1`；远端ASC的能力Spec在Mixed模式下不必对其他客户端公开，不能因远端为0就判断未授予。
3. 在想发动的一端控制台输入`HALUseSkill`。主机／客户端都可用，无参数／PlayerId。不依赖团队先创建GA／GE蓝图；原生类已经可运行。
4. 本地HUD和`HALGASStatus`可见`ImpactImmune=1`、`ImmunityRemaining`约8、`CooldownRemaining`约20。8秒后免疫结束，约12秒冷却仍在；20秒后可以再发动。剩余值`-1`表示Loose Tag、无限或远端Mixed缺少时长数据，不能读成已到期。
5. 免疫期间让对方发球命中：仍应扣25HP，实际档位None，无本次规则击飞／脱球；到期后有效冲击恢复。两端互换角色，再测试持球发动、连按、不同方向接触和退出。若此前用`HALImpactDebug`加过免疫，先清除该命令自己的贡献，避免把额外免疫误认成技能未到期。
6. `hal.AbilityLog 1`输出`Ability input`、服务器`Ability accepted`、`Ability rejected`、`Effect added/removed`和`Avatar effects cleared`；设0关闭。HUD仍是调试表现，本轮没有擅自替换材质／添加正式技能演出。

## 4. 团队手工接键鼠／手柄（不改Event Graph）

控制台可立即测技能；正式按键由团队补下面三处引用，代理不编辑`.uasset`。

1. Content Browser定位现有输入目录；右键**Input → Input Action**，创建`IA_PrimarySkill`。打开后将**Value Type**设为**Digital (bool)**，不设8秒Hold触发器：8秒由GE管理，输入只取按下边沿。Save。
2. 打开玩家Vehicle DA，沿`LocalConfig`引用定位当前实际Local DA（当前测试资产通常为`DA_Local_TestBaseline_v1`）。沿它的`Default Mapping Context`引用打开现用IMC；保留原映射，在Mappings新增`IA_PrimarySkill`，例如键盘`Q`与手柄`Gamepad Face Button Top`（Xbox Y／PlayStation △），先确认选定按键未被原操作占用。Save IMC。
3. 回到Local DA，在新增`Primary Skill Action`指定`IA_PrimarySkill`，其余相机／操控引用和Ready保持原值，Save。无需在Pawn或Level Blueprint添加事件／RPC／施力逻辑。
4. 完整停止并重新开局。按一下应发动一次，按住不应每帧重发；松开／取消后输入位复位。若控制台成功但按键无效，核对玩家实际引用的Local DA、IMC与Bool类型，不重置已有调参。

## 5. 以后改时长／换技能时怎么配置

原生类已经可以运行。本节是把现有免疫技能变为团队可调资产的操作，不是再次编写技能。四个新资产和现有GameMode的引用须由团队在Editor手工完成；本节没有编辑任何资产。以下以保持8秒／20秒为例，确认接线有效后再调数值。

### 5.1 创建两个GE子蓝图

1. 停止PIE／Standalone测试。Content Browser可新建项目内目录，例如`/Game/Gameplay/Effects`；目录只用于整理，不是C++硬编码依赖。
2. 空白处右键 **Blueprint Class → All Classes**，搜索并选择 **HALImpactImmunityEffect**，创建`GE_ImpactImmunity`。不要选`HALDebugImpactImmunityEffect`，也不要在此例改选普通`GameplayEffect`；本例依靠原生父类的免疫Tag、刷新和作用域配置。
3. 打开资产，点击 **Class Defaults**。若只有数据默认值页面，直接编辑；若需完整工具栏，点击 **Open Full Blueprint Editor**。清空Details过滤，再搜索`Duration`。
4. **Duration Policy**保留 **Has Duration**。展开 **Duration Magnitude**：计算类型保持 **Scalable Float**，展开其Scalable Float值并填写 **Value = 8**，不选Data Table／Curve Table行。不同面板可能将该常数直接显示在Magnitude行；目标是修改持续时长的Scalable Float常数。不要把8填到Period中。
5. 保留父类组件：授予`State.Combat.ImpactImmune`，效果自身有`Effect.Scope.Avatar`和`Effect.Vehicle.ImpactImmunity`。前者是目标的有效状态，后两者是GE自身分类；Avatar清理按效果分类查找，不能混放。无需在Event Graph手动加Tag／定时移除。
6. 需要核对刷新时，清空搜索并找 **Stacking**：**Stacking Type = Aggregate by Target**、**Stack Limit Count = 1**、**Stack Duration Refresh Policy = Refresh on Successful Application**；**Deny Overflow Application**保持关闭，以允许已达一层时成功重施并刷新。其他设置沿用父类。Compile／Save。
7. 用同样的 **Blueprint Class → All Classes**流程，以 **HALImpactImmunityCooldownEffect**为父类创建`GE_ImpactImmunityCooldown`。在Class Defaults将 **Duration Policy = Has Duration**、**Duration Magnitude = Scalable Float，Value = 20**。保留其授予的`Cooldown.Ability.ImpactImmunity`；不要添加`Effect.Scope.Avatar`，否则换Pawn可能清掉冷却。Compile／Save。

两个GE分别表达“持续状态”和“再次发动的限制”，所以免疫8秒到期后冷却还应剩约12秒。

### 5.2 创建并配置GA子蓝图

1. Content Browser可新建`/Game/Gameplay/Abilities`。右键 **Blueprint Class → All Classes**，选 **HALImpactImmunityAbility**，创建`GA_ImpactImmunity`。
2. 打开 **Class Defaults**，Details搜索`Immunity`，把 **Immunity Effect Class**指向刚创建的`GE_ImpactImmunity`。
3. 清空搜索，再搜`Cooldown`，在继承的 **Cooldowns**分类中把 **Cooldown Gameplay Effect Class**指向`GE_ImpactImmunityCooldown`。该字段来自引擎GameplayAbility，不是去Local DA里找。
4. 保留原生父类的 **Instanced Per Actor／Server Only**设置。无需添加ActivateAbility Event Graph、RPC、Tick、Delay或手动冷却。Compile／Save。

### 5.3 将GA配到PlayerState的初始授予

1. 在项目蓝图目录创建 **HALPlayerState**的子蓝图，例如`BP_HALPlayerState`。若项目已实际使用同基类的PlayerState子蓝图，直接编辑现用子蓝图，不重复创建平行配置。
2. 打开 **Class Defaults**，搜索`Default Abilities`，展开 **GAS → Abilities → Default Abilities**。原生默认通常已有一项；修改这一项，不在后面再加一个相同InputTag的项。
3. 配置为下表，Compile／Save。若数组确实为空，再用`+`添加一项；当前示例总共只需一项主技能。

| 字段 | 本例值 | 含义 |
|---|---|---|
| Ability | `GA_ImpactImmunity` | 实际授予的GA子蓝图Class |
| Input Tag | `Input.Ability.Primary` | 输入槽身份；不要填Ability Tag或Cooldown Tag |
| Level | `1` | 授予等级，本例直接使用常数8／20 |

### 5.4 让实际GameMode使用这份PlayerState

1. 打开测试场景实际使用的`BP_MVPB_ListenGameMode`，点击 **Class Defaults**，搜索`Player State Class`，改为`BP_HALPlayerState`。修改的是PlayerState，不是PlayerController或Default Pawn。
2. 保留现有Default Pawn `BP_RoundedVehiclePawn`及其他类引用。Compile／Save这个GameMode。
3. 打开`MVPB_NetTest`的 **World Settings**，确认 **GameMode Override**仍指向这份GameMode；若原本正确，无需修改或重存地图。菜单Entry的GameMode不承担这份车辆技能配置，不把Entry改成对战GameMode。
4. 完整结束旧游戏进程／PIE，再重新Host／Join并Start，确保新PlayerState和授予在新局生成。这里只编辑配置蓝图，不需要重新编译C++；已有实例不会因修改Class Defaults可靠地变成新授予。

### 5.5 确认新资产确实被使用

1. 两端执行`hal.AbilityLog 1`与`HALGASStatus`，本地玩家`PrimarySkillGranted=1`。执行`HALUseSkill`；若第4节IA／IMC／Local已接线，也可按主技能键。
2. 主机日志应出现包含`GA_ImpactImmunity`的`Ability accepted`，`Effect added`应出现`GE_ImpactImmunity`和`GE_ImpactImmunityCooldown`的子蓝图名，而不只是原生类名。Granted=1只能证明有技能，不能单独证明已切到新GA资产。
3. 初始剩余约8／20；约第8秒免疫结束、冷却剩12，第20秒可再发动。修改GE时长后须开新局再核对；其他独立免疫来源会影响状态合并，先移除此前HALImpactDebug自己的免疫。

后续只调整这项技能的数值时，通常改两个GE并开新局即可；不必每次重建PlayerState或改GameMode。后续换主技能时，只替换当前PlayerState的那项Ability引用；IA／IMC和`Input.Ability.Primary`可继续表示同一个主技能键。

### 5.6 哪些结构沿用，哪些不能套给全部Buff

| 层次 | 当前实现 | 未来使用边界 |
|---|---|---|
| 项目接入层 | PlayerState持ASC、Avatar绑定、可配置授予／去重、InputTag激活、GAS权威请求、效果查询／复制 | 后续GA／GE可复用，不为每个Buff再写网络RPC或到期计时系统 |
| 生命周期约定 | `Effect.Scope.Avatar`清理、旧Pawn身份保护、Owner退出清理、按GE贡献移除 | 后续效果选择自己的作用域；需要跨Avatar保留的效果不应盲目带Avatar Tag |
| GAS现有能力 | Duration、Modifier、Tag、叠层／刷新／驱散 | 属于引擎通用能力，本轮已接入并测试；不需要自己再造一套Buff管理器 |
| 免疫技能专用 | HALImpactImmunityAbility／免疫GE／免疫冷却GE、对应Tag和许可检查 | 是可沿用的正式技能实现。可配置免疫变体，但GA明确检查免疫Tag和TieredV1，不能只把GE替成加速／攻击Buff就期待通用 |
| 调试／验证 | HALUseSkill、HALImpactDebug无限GE、调试HUD／日志、自动化夹具 | 是开发工具，不代表正式技能演出或玩家获取Buff的渠道 |

未来同类免疫变体可以沿用本节全套配置。不同语义的新Buff通常以原生`GameplayEffect`或合适的项目C++ GE父类创建配置资产，填写已有Attribute的Modifier／Tag、Duration及每个GE自己的叠层规则；不继承本免疫GE来避免误带免疫和刷新规则。效果由已有／新建的服务器C++ GA、拾取或机关入口施加；被动／机关Buff不必为了施加一个GE额外创建GA。

新动作、新目标选择或复杂许可由对应C++ GA实现，再做轻量GA子蓝图引用效果与数值，复用同一ASC与授予入口。不同技能若不共享冷却，应使用各自冷却Tag，不能全部复用`Cooldown.Ability.ImpactImmunity`。当前只接了一个Primary输入槽；增加额外技能键还需在C++补对应输入路由／Tag／本地Action引用，不能只在Default Abilities任填一个新Tag就自动获得按键。

玩家不同车型的配置化技能库／运行时替换尚未建设，当前DefaultAbilities属于PlayerState初始配置；这不会自动按Vehicle DA挑不同技能。2026-10-19接续补充：F2运动桥接、F3加速GE和窄`HALMovementBoostAbility`现已实现；用户选择替换原Primary的免疫授予，复用原按键，第二技能输入不在本轮范围。最终Editor／Game、45项普通回归及真实GA双端通过；团队子蓝图接线／实际按键仍需手工完成，具体步骤与证据见[加速技能补充](MVPB_Phase10F3_Movement_Ability_CN_v0.1.md)。免疫GA不能直接换任意Buff GE使用。物理动作与命中仍走Movement／Combat Resolver，GA／GE不接管每帧物理。

## 6. 验证记录与局限

最终Editor和Game Development Target构建成功。`HAL.FutureCreation`报告为33成功、0警告、0失败、0未运行，包含本轮新增4项技能测试；它在最后一次仅调整独立联机测试的时钟断言前执行，正式技能源码未再变化。最终两个Target已包含该测试调整并重新编译。324个`.uasset`／`.umap`前后SHA256一致，没有资产接线或调参变更；历史验证JSON保持原样。

新增普通回归覆盖真实World／GAS计时、冷却拒绝、免疫不免伤、刷新、独立免疫来源／驱散、其他GE叠层、重复授予、旧Avatar延迟回调与销毁清理、离散输入不重放。计时测试用连续引擎帧推进独立World，不伪造GFrameCounter，不用测试定时器手工移除效果。

另有显式`HAL.NetworkSmoke.Phase10E`游戏上下文测试，不在普通`HAL.FutureCreation`前缀内自动运行；只用于独立主机／客户端的真实GAS激活RPC和复制、双方免疫到期及20秒冷却。它调用原有主机Start和技能入口，不绕过GameMode／GA许可，不驱动车辆或模拟命中。

真实NetTest Listen Server／客户端各1项通过、0警告、0失败：客户端先同帧提交3次请求，服务器只接受第一次并拒绝两次冷却重复请求，再发动主机技能；两端均观察自己及对方免疫，到期后免疫消失且冷却仍在，冷却于服务器第20游戏秒结束。Owner接收GE时长，远端Mixed只要求Tag，不要求看见对方Spec／精确剩余秒数。专用隐藏测试进程自动退出并核对无残留。原始日志／报告在`Saved/Phase10E`，不随Git同步，共享JSON保留摘要与哈希。

不限FPS的NullRHI运行中，服务器20游戏秒对应16.55墙钟秒，TimeDilation=1。只读检查引擎`BaseGame.ini`的`MinUndilatedFrameTime=0.0005`及`AWorldSettings::FixupDeltaSeconds`下限夹限，与超高帧率下游戏时钟走快相符；另以进程参数`t.MaxFPS 60`独立复验，Host／Client再次各1项通过，服务器免疫8游戏秒、冷却20游戏秒＝20墙钟秒，客户端复制收敛。没有修改工程时钟、网络物理或GE数值配置；这也不等于已完成真人显示／手感测试。

施工中的失败均保留：5.8.2的`UGameplayEffect::SetStackingType`未导出导致初次链接失败，改为构造／测试模板局部设置公开默认属性；原生构造中`FindOrAddComponent`创建未命名对象导致首次启动失败，改为命名`CreateDefaultSubobject`；测试夹具缺少完整Actor初始化导致退出用例失败，补齐引擎生命周期；调试Loose Tag与GE的复制策略警告通过上述独立调试GE修正。初次联机测试错误用墙钟断言GAS时长，改用服务器游戏时钟与实际GE持续时间，不调整8／20或工程时间配置。各次失败日志及最终结果分别记录，不以进程退出码替代自动化报告。

以上为原施工验证时点。本机仅5.8.2，未验证5.8.3；随后团队基础双端免疫反馈见下节。按键／手柄输入边沿、持球受击、延迟／丢包／抖动和四台真实有线PC仍须按实际结果留档，不因自动化或本次局部手动日志通过就扩大验收范围。

## 7. 团队双端免疫反馈与日志复核（2026-10-16）

用户报告免疫Buff正常生效／消失、免疫效果有效，客户端与主机表现均暂未见问题。本轮接收干净`main / 6850e8a2b2a51d49798a21f340c36ecdcc84c307`，团队已经提交GA／两份GE／BP_HALPlayerState、GameMode引用、IA／IMC和Local接线。原10-E摘要所列100份源码／配置按原始或LF归一化哈希全部一致，无生产源码改动。本轮只读日志、记录文件哈希并维护文档，不启动游戏、重编、跑自动化或修改资产。

检查最新`Saved/Logs/MVPB_GAS_Host.log`／`MVPB_GAS_Client.log`及紧邻的一组`08.03.26／08.03.27`备份，合计两局四份日志；均为UE5.8.2，Entry进入NetTest、127.0.0.1同机双进程。双方运行的ASC Owner确为`BP_HALPlayerState`。下面时间已由日志UTC转为日本时间：

| 日本时间 | 攻击方向 | 服务器结果 | 两端受击者HP |
|---|---|---|---|
| 17:02:53.852 | 主机→客户端 | 无免疫，Heavy→Heavy，Applied | 100→75 |
| 17:03:09.005 | 主机→客户端 | ImpactImmune=1，Heavy→None，Suppressed，Drop=0 | 75→50 |
| 17:03:21.589 | 主机→客户端 | 免疫已不在，Heavy→Heavy，Applied | 50→25 |
| 17:05:38.982 | 客户端→主机 | 无免疫，Heavy→Heavy，Applied | 100→75 |
| 17:05:51.460 | 客户端→主机 | ImpactImmune=1，Heavy→None，Suppressed，Drop=0 | 75→50 |

五次攻击ID各不相同，各次`Raw／Resolved／Actual=25`；两次免疫命中`SourceTier=3、EffectiveTier=0、Impact=6`（Suppressed），说明免疫只抑制规则冲击，伤害仍有效。其余命中`EffectiveTier=3、Impact=5`（Applied）；客户端目标第三次命中已恢复正常档位。双方HP记录收敛，服务器的75→75／50→50等同值通知不是二次扣血。最后一局未记录主机目标免疫结束后的再次命中；主机端消失与双端观感来自用户反馈，分别保留证据来源。

四份日志均无Ensure／Assertion／Fatal，未见GAS Tag复制策略警告。仍有已知Entry错误`InputMode:UIOnly - Attempting to focus Non-Focusable widget`，本轮不修UI；启动期还见可选性能分析DLL缺失、TSR驱动回退、EditorDataStorageUI注册及渲染CVar线程提示。备份客户端的Host closed／ConnectionLost发生于主机`ViewportClosed`退出之后，不作为对战中异常断线。旧球`hit vehicle`文字仍可能把免疫结果显示为`tier Light`，这是兼容枚举输出，V1实际结算看`HAL Combat`的EffectiveTier与Impact，不据此判定免疫失败。

本次启动命令是未含`hal.AbilityLog 1`／`hal.ImpactPhysicsLog 1`的旧版本，四份日志均无`LogHALAbility`或`HALGASStatus`快照。因而不能从本次日志单独证明精确8秒GE／20秒冷却、重复技能请求拒绝、实际GA／GE子蓝图类名或物理动作消费；此前自动化仍是独立历史证据。后续测试开始前可在两端执行`hal.AbilityLog 1`，必要时拍下`HALGASStatus`；更新后的启动命令已经包含这些日志开关。无须为此修改技能逻辑。

本次确认基础双端免疫功能的团队反馈和上述日志子项。尚无明确的键鼠／手柄按住边沿、目标持球受击、Avatar替换／带活动GE退出、100ms实测RTT／1%丢包／20ms抖动或四机的新实测证据，不标记10-E全矩阵通过，不自动进入10-F。当前共有329资产，与原324资产基线相比为团队有意新增5项、修改3项，哈希另存于复核JSON；未重新导出GE字段，不将资产存在等同于逐字段校验。原始日志快照在`Saved/Phase10E/ManualReview_20261016`，不随Git同步；[新复核JSON](MVPB_Phase10E_ManualLogReview_20261016.json)保留关键原文、行号和哈希，原施工JSON不回写。
