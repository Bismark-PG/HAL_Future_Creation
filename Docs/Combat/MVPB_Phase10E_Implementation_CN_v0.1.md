# MVP-B 10-E：主动冲击免疫GA／持续GE施工与操作（v0.1）

日期：2026-10-16（Asia/Tokyo）。基点`main / aa278bd4a315c4afd0ccaf4eab7217d39e8b82cd`，本轮开始工作区干净；本机UE5.8.2 CL56702186。[已确认规格](MVPB_Phase10E_Ability_Effect_Plan_CN_v0.1.md)保持，用户随后明确授权GA／GE本体施工。**代码、Editor／Game编译、33项普通自动化和真实双进程技能请求／复制检查完成；团队输入接线与实际对战验收待完成。** 详细证据见[本轮验证摘要](MVPB_Phase10E_Verification_20261016.json)，接续从[Project_Status](../Project_Status_CN.md)开始。

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

原生类可以直接使用，不必为了本轮测试创建以下资产。要给策划调参时，由团队手工建立：

1. 分别创建`HALImpactImmunityEffect`、`HALImpactImmunityCooldownEffect`的GE子蓝图，例如`GE_ImpactImmunity`和`GE_ImpactImmunityCooldown`。Class Defaults搜索**Duration**，保持Has Duration，在Duration Magnitude的Scalable Float常数中调整秒数；保留各自继承的Tag／组件／作用域。免疫GE的刷新策略不要误改为累加时长。
2. 创建`HALImpactImmunityAbility`的子蓝图，例如`GA_ImpactImmunity`，在Class Defaults把`Immunity Effect Class`和继承的`Cooldown Gameplay Effect Class`指向上述两个GE。
3. 若需替换原生默认授予，创建`HALPlayerState`的轻量子蓝图，在`Default Abilities`将原主技能项的Ability替换为新GA，InputTag保持`Input.Ability.Primary`、Level保持1。不要再加一个相同输入Tag的项。
4. 在网络GameMode子蓝图将Player State Class指向该PlayerState子蓝图，保留已调Default Pawn和其他类。Compile／Save确有改动的资产、重启整局测试；不要在运行中改CDO来假装技能状态已刷新。

未来可叠层GE自行选择Stacking Type／Stack Limit等，不修改本免疫GE来代表所有Buff。其他技能可配其他GA类别／输入Tag，再通过相同ASC入口激活；本轮不建完整技能栏。

## 6. 验证记录与局限

最终Editor和Game Development Target构建成功。`HAL.FutureCreation`报告为33成功、0警告、0失败、0未运行，包含本轮新增4项技能测试；它在最后一次仅调整独立联机测试的时钟断言前执行，正式技能源码未再变化。最终两个Target已包含该测试调整并重新编译。324个`.uasset`／`.umap`前后SHA256一致，没有资产接线或调参变更；历史验证JSON保持原样。

新增普通回归覆盖真实World／GAS计时、冷却拒绝、免疫不免伤、刷新、独立免疫来源／驱散、其他GE叠层、重复授予、旧Avatar延迟回调与销毁清理、离散输入不重放。计时测试用连续引擎帧推进独立World，不伪造GFrameCounter，不用测试定时器手工移除效果。

另有显式`HAL.NetworkSmoke.Phase10E`游戏上下文测试，不在普通`HAL.FutureCreation`前缀内自动运行；只用于独立主机／客户端的真实GAS激活RPC和复制、双方免疫到期及20秒冷却。它调用原有主机Start和技能入口，不绕过GameMode／GA许可，不驱动车辆或模拟命中。

真实NetTest Listen Server／客户端各1项通过、0警告、0失败：客户端先同帧提交3次请求，服务器只接受第一次并拒绝两次冷却重复请求，再发动主机技能；两端均观察自己及对方免疫，到期后免疫消失且冷却仍在，冷却于服务器第20游戏秒结束。Owner接收GE时长，远端Mixed只要求Tag，不要求看见对方Spec／精确剩余秒数。专用隐藏测试进程自动退出并核对无残留。原始日志／报告在`Saved/Phase10E`，不随Git同步，共享JSON保留摘要与哈希。

不限FPS的NullRHI运行中，服务器20游戏秒对应16.55墙钟秒，TimeDilation=1。只读检查引擎`BaseGame.ini`的`MinUndilatedFrameTime=0.0005`及`AWorldSettings::FixupDeltaSeconds`下限夹限，与超高帧率下游戏时钟走快相符；另以进程参数`t.MaxFPS 60`独立复验，Host／Client再次各1项通过，服务器免疫8游戏秒、冷却20游戏秒＝20墙钟秒，客户端复制收敛。没有修改工程时钟、网络物理或GE数值配置；这也不等于已完成真人显示／手感测试。

施工中的失败均保留：5.8.2的`UGameplayEffect::SetStackingType`未导出导致初次链接失败，改为构造／测试模板局部设置公开默认属性；原生构造中`FindOrAddComponent`创建未命名对象导致首次启动失败，改为命名`CreateDefaultSubobject`；测试夹具缺少完整Actor初始化导致退出用例失败，补齐引擎生命周期；调试Loose Tag与GE的复制策略警告通过上述独立调试GE修正。初次联机测试错误用墙钟断言GAS时长，改用服务器游戏时钟与实际GE持续时间，不调整8／20或工程时间配置。各次失败日志及最终结果分别记录，不以进程退出码替代自动化报告。

本机仅5.8.2，未验证5.8.3。真人键鼠／手柄、持球击中、延迟／丢包／抖动和四台真实有线PC仍须团队按实际结果留档；不因无UI自动化通过就声称这些子项已验收。
