# MVP-B 10-F-3：首个运动GE与调试施加（v0.1）

日期：2026-10-17（Asia/Tokyo）。施工基点`main / 599d60ab5856eea348d1b20e3bd437d4ff6fa8f5`，起始工作区干净；实际UE`5.8.3-58210709+++UE5+Release-5.8`。用户授权接续下一步，本轮完成10-F-3，不扩展F4完整集成或弹簧拳接入。架构沿用[F1规格](MVPB_Phase10F1_Spec_CN_v0.1.md)与[F2桥接](MVPB_Phase10F2_Implementation_CN_v0.1.md)，保存DA基线继续使用[F1记录](MVPB_Phase10F1_Baseline_20261017.json)。本轮证据见[F3验证摘要](MVPB_Phase10F3_Verification_20261017.json)，不覆盖F2历史报告。

2026-10-19技能入口补充：用户要求复用原Primary替换免疫，现已增加`HALMovementBoostAbility`；最终UE5.8.3 Editor／Game完整构建、45/45普通回归及正常GA请求的真实Host／客户端各1/1通过（80ms单向注入）。见[独立验证](MVPB_Phase10F3Ability_Verification_20261019.json)。团队应按[加速GA／GE详细配置](MVPB_Phase10F3_Movement_Ability_CN_v0.1.md)手工接线。下文3.1／3.2保留开发前的缺口分析，不能再据此判断源码仍缺少运动GA；10月17日的42项／双端结果仍只证明当时GE调试路径。

## 1. 正式测试GE

新增普通`UGameplayEffect`派生类`UHALMovementBoostEffect`，不继承免疫GE、不新增GA，也不替换原Primary输入／默认授予。默认值：

| 项目 | 原生默认值／规则 |
|---|---|
| 持续时间 | HasDuration，5游戏秒，无周期Tick |
| 加速度 | `HALMovementAttributeSet.AccelerationMultiplierDelta`，Additive +0.5 |
| 驾驶上限 | `SpeedLimitMultiplierDelta`，Additive +0.25 |
| 叠层 | AggregateByTarget，一层；同一定义跨来源也共用这一层 |
| 合法重施 | RefreshOnSuccessfulApplication，从这次起重新计5秒；不累计剩余时间，不增加倍率 |
| 到期 | ClearEntireStack；移除自身贡献，GAS驱动物理桥接恢复，不直接写速度 |
| 身份 | 资产Tag `Effect.Vehicle.MovementBoost`及`Effect.Scope.Avatar`；授予`State.Movement.Boost` |

最终有效值仍由F2分别计算`clamp(1+所有GE增量和,0,3)`。同一GE刷新而数值不变时，桥接不增加物理Revision；真正开始／移除／到期才生成边界。前进／倒车同时作用，其他基础配置、转向参照、球／伤害／冲击和后坐公式均保持原职责。F2的2秒原生测试夹具只用于其历史验证，F3调试命令使用本节正式5秒GE。

Avatar丢失／替换时沿用已有作用域清理；延迟到达的旧Pawn清理不影响新Avatar。PlayerState HP和其他冷却保留；Owner退出清除残留GE与授予。

## 2. 团队现在如何测试

完整重编后按[双端启动说明](../Configuration/MVPB_DebugHUD_And_GAS_Test_CN_v0.1.md)进入NetTest，两端加入，由主机Start到Playing。在**希望加速的那一端自己的游戏控制台**执行：

```text
hal.MovementEffectLog 1
hal.AbilityLog 1
HALMovementBoost
HALGASStatus
```

无需新蓝图资产、输入绑定或DefaultAbilities接线；主机和客户端都用同一命令。它只请求当前控制车辆，不接受PlayerId、目标、倍率、时长或GE类别参数。服务器从当前Controller／PlayerState／Avatar确定目标，从项目设置确定GE，检查配置／GAS就绪及Playing，再施加。Entry、无有效车辆、Waiting、错误GE配置均拒绝；Shipping中这组调试入口不生效。旧Test图没有匹配GameState时也拒绝，先用NetTest。

生效后本地HUD显示`Movement boost: ACTIVE`与剩余秒数，物理行倍率约1.50／1.25，`HALGASStatus`含`BoostRemaining`、有效倍率／Revision等。持续约2秒后再次输入`HALMovementBoost`，剩余时间应重新接近5秒，倍率不翻倍；原来那次5秒到时仍有效，最后一次合法刷新约5游戏秒后恢复1／1。物理消费和HUD刷新存在正常的帧边界量化，客户端另有复制延迟。

主动移除：

```text
HALClearMovementBoost
```

此命令只移除带`Effect.Vehicle.MovementBoost`身份的加速族（含使用该身份的子类），重复执行可安全无操作；不移除其他独立运动GE、免疫或冷却。它不是“清除所有Buff”。清理仍要求当前合法车辆与Playing。

服务器可关闭后续调试施加：

```text
hal.MovementBoostDebug 0
```

默认1；主机改为0后，客户端也不能绕过。改回1恢复；此开关只控制调试施加，不驱散已有GE，也不禁止Playing中用`HALClearMovementBoost`清理，不影响未来正常GA／道具对GE的使用。`hal.MovementEffectLog 0`关闭物理诊断，`hal.AbilityLog 0`关闭GE事件日志，`hal.DebugHUD 0`关闭HUD。

日志搜索`HALMovementBoost:`检查权威接受／拒绝、目标PlayerId、实际GE类、Remaining／Stacks及服务器GameTime；客户端收到结果回执。`Effect added/removed`由既有AbilityLog控制。Tag／剩余时间表示GAS状态，Movement的Epoch／Revision／Frame／Replay表示实际物理消费，二者不能混作同一时点。Mixed复制下远端Tag可见但时长可能为-1，需看该玩家自己的Owner端；远端车不新增驾驶施力，其Movement未Ready不代表拥有者失败。

## 3. 可选Editor配置：以后调整数值

当前无需修改资产就能测试。若团队希望在Editor调时长／倍率，手动执行以下步骤；代理本轮没有创建或保存蓝图／地图。

1. 内容浏览器中新建Gameplay Effect蓝图，父类选`HALMovementBoostEffect`，例如保存为`/Game/Blueprints/GE/GE_MovementBoost`。它继承的是普通GE父类，不是`HALImpactImmunityEffect`。
2. 打开Class Defaults。Duration Policy保持Has Duration，Duration Magnitude的Scalable Float默认为5。Modifiers保留两项：AccelerationMultiplierDelta／Additive／0.5，以及SpeedLimitMultiplierDelta／Additive／0.25；修改的是增量，不填写1.5／1.25，否则有效倍率会错误变成2.5／2.25。
3. 保持Aggregate By Target、Stack Limit Count=1、Refresh On Successful Application、Clear Entire Stack，Deny Overflow Application关闭；保留上述Avatar作用域、效果身份和授予Tag。调试入口会拒绝破坏这些基础契约的配置。
4. Project Settings中搜索`HAL Debug`，在Movement的`Movement Boost Effect Class`选择此GE子类。默认原生`HALMovementBoostEffect`也可继续使用；空引用／加载失败会拒绝，不悄悄回退默认GE。设置引用由正常Config保存与Git同步。
5. Compile／Save蓝图并提交新资产和配置。两端均换成同一版本后重开测试局；新GE类与旧GE类属于不同定义，不在活动效果中热换配置来测试“同一定义刷新”。

不要把GE放入`BP_HALPlayerState.DefaultAbilities`；该数组只授予GA。未来技能／道具／机关可在服务器通过正常GAS接口施加此GE，复用F2桥接，不调用调试Controller作为玩法核心。纯持续GE本轮无新输入槽或GA冷却；原免疫Primary仍按8／20秒规则工作。

### 3.1 通过GA／GE配置测试：开发前能力边界（2026-10-18留档）

本机已提交F3到`main / e8b18ceb55e38c7b47189f22300f13b572e7895c`。当前有原生加速GE和调试施加入口，**尚无运动GA原生基类／通用“施加指定GE”GA**。现有`HALImpactImmunityAbility`是免疫专用GA，会验证免疫Tag与冷却；把它的Immunity Effect Class直接换成加速GE会被拒绝，不应给加速GE补免疫Tag来绕过检查。不能把纯配置说明写成当前已支持完整“按键→运动GA→GE”。

现在可执行的完整GE配置流程：

1. 停止旧PIE／游戏。在Content Browser的`/Game/Blueprints/GE`右键Blueprint Class，打开All Classes，选择`HALMovementBoostEffect`，创建例如`GE_MovementBoost`。同名原生类可搜索；若列表没有它，先核对当前C++模块已编译加载，再创建资产，不改选免疫父类。
2. 打开Class Defaults，设置／确认Has Duration和5秒，Period保持0。Modifiers中已有两项，直接编辑已有元素：AccelerationMultiplierDelta／Additive／0.5和SpeedLimitMultiplierDelta／Additive／0.25，避免追加重复项。增量不是最终倍率，最终分别为1.5／1.25。
3. 核对前节的一层／按目标聚合／成功刷新／到期清整层，以及继承组件中的最终Tag。Scope和Effect身份属于GE资产Tag，State属于授予目标的Tag；不在Event Graph手动加Tag或用Delay清理。Compile、Save。
4. 在Project Settings搜索HAL Debug，将Movement Boost Effect Class指向`GE_MovementBoost`。此引用只决定`HALMovementBoost`调试命令使用哪一个GE；**不会自动创建／授予GA，也不会改变`HALUseSkill`或主技能键**。服务器使用自己的设置，两端需同步资产及Config并完整重开局。
5. Host／Join进入NetTest并Start。双方各自执行`hal.AbilityLog 1`、`hal.MovementEffectLog 1`和`HALMovementBoost`，服务器日志应显示`Effect=GE_MovementBoost_C`（或实际子类名），否则仍在用原生默认类。约2秒重施检查刷新，最后一次重施5秒后恢复，另用`HALClearMovementBoost`检查主动清除。

### 3.2 “按键→GA→GE”的开发前缺口（2026-10-19留档）

2026-10-19用户已明确，这条技能释放路径正是预期的测试方式。因此当前下一开发入口应先补运动GA，再进入完整F4；此前“F3完成”仅覆盖原定GE／调试施加范围，不表示运动GA已经实现。

先补一个窄C++运动GA基类，负责当前Avatar／Playing许可、服务器施加可配置运动GE和结束本次GA；GE本身继续负责5秒计时／一层刷新，Movement继续负责物理。为了允许重复按键刷新，GA施加GE后应结束本次动作，不等待5秒；首例不借用免疫20秒冷却，也不新增逐帧GA。**这段原生支持尚未施工，本节是后续接线位置说明，不是已可选的类／字段。**

原生支持就绪后的配置链为：

```text
测试地图的GameMode
 → Player State Class
 → BP_HALPlayerState.Default Abilities：GA类／Input Tag／Level
 → GA子蓝图的运动GE引用
 → GE_MovementBoost
 → Movement AttributeSet与既有物理桥接
```

团队届时创建运动GA子蓝图，在Class Defaults选择`GE_MovementBoost`，保留原生ServerOnly等策略，Event Graph无需补核心逻辑。`Project Settings > HAL Debug`的GE选择与GA自身GE引用是两个独立使用入口，修改一处不会自动修改另一处。

目前C++只路由`Input.Ability.Primary`。若只验证单个运动GA，可在测试专用PlayerState／GameMode配置中用它替换Primary项（Ability=新GA，Input Tag=Input.Ability.Primary，Level=1），复用现有`HALUseSkill`／IA_PrimarySkill；这样该测试配置的Primary不再发动免疫。不要在数组再加一个相同InputTag，代码会拒绝重复授予，激活也要求该Tag恰好匹配一个GA。

若要求免疫和加速各自有按键，必须先补第二输入Tag／Action／C++路由，再分别配置授予；仅在Default Abilities填一个自定义Tag不会自动产生新技能键。现有键位引用位于玩家Vehicle DA指向的Local DA：Primary Skill Action→IA_PrimarySkill→IMC映射。实际NetTest的GameMode须引用对应PlayerState；Entry菜单不承担这份技能授予。完整结束旧局后再Host／Join，确保新PlayerState和Ability Spec重新生成。

以上两节记录获施工授权之前的分析与操作边界。后续用户已授权补GA，最新实现、编译／测试状态及替换原Primary的步骤以[独立补充文档](MVPB_Phase10F3_Movement_Ability_CN_v0.1.md)为准，不改变F4待验收状态。

## 4. 本轮验证边界

新增3项普通测试覆盖真实World／GAS计时的5秒自然到期、跨来源一层刷新且重计5秒、不同GE贡献／选择性驱散、关闭施加开关、Waiting／无Avatar／空配置拒绝、Avatar切换与销毁、HP／其他冷却保留。原39项继续作为回归；GAS使用World游戏时间，不用手动移除模拟自然到期，也不修改全局帧计数。

`HAL.NetworkSmoke.Phase10F3`另起真实NetTest Host／Client，60FPS、NullRHI；主机发送方向注入80ms延迟（不是实测RTT）。客户端先发：比赛开始前请求确实被拒绝；Playing后客户端同帧三次请求仍一层，主机确认该请求只影响客户端，然后自己施加。双方约2秒刷新，确认物理Revision保持、原到期点继续有效、自然到期恢复后Revision递增；第二轮并行开启原免疫技能，再主动清除加速，确认免疫／冷却和Primary授予保留。两轮间保留可复制的中性区间，避免测试自身把远端“移除／重施”压在同一次状态采样里。

最终UE5.8.3 Editor／Game Development构建成功；普通自动化42/42、0测试警告／0失败（原39＋新3），最终Host／Client各1/1成功、0警告／0失败。主机从刷新请求到物理恢复约5.033游戏秒，客户端观察约5.150秒；GAS Spec本身为5秒，观测包含GT采样、物理边界与复制延迟。Revision依次1→2、刷新保持2、自然到期3、再次开始4、清除5；最终双方免疫仍有效，原冷却约剩18.9秒。日志无Error／Ensure／Assertion／Fatal或GAS复制警告；编译仍有既有引擎／兼容API弃用及MSVC版本提示。

329资产前后SHA-256全同，无新增／删除，未改保存Config／DA／弹簧拳。全部专用进程均退出。详细报告、源码／日志哈希与早期失败见[F3验证摘要](MVPB_Phase10F3_Verification_20261017.json)。首轮编译只在跨来源测试夹具发现旧Target接口要求可写GE指针，改用GAS Spec-to-target接口后修复；不对原生CDO做const_cast。一轮网络复测因固定启动等待导致客户端错过Waiting，客户端报告失败；随后改为客户端完成检查后经已有可靠SetName请求标记就绪、主机收到标记才Start，保留原拒绝断言并通过最终复测。最终仅该实时网络夹具在42项普通回归之后改变，生产／普通测试源码保持，两个Target重编并重跑网络；没有把失败报告或进程退出码当成功。

本轮HUD字段已编译及日志核对，未做新增行的实际渲染视觉验收。

下一入口为**10-F-4集成回归**：团队做真实驾驶／漂移／控球／发射／冲击／免疫／退出与墙角／腾空／连续接触组合，补实测RTT、丢包／抖动和四台有线PC证据。F3的GE状态共存检查不等于完整物理组合验收，F2的1500ms历史恢复仍为其独立历史证据。本轮不改车辆调参、F2物理协议、弹簧拳或独立禁捕设计，改动未提交／Push。
