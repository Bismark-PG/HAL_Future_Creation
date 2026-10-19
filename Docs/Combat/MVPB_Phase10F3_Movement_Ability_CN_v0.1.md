# MVP-B 10-F-3补充：Primary加速技能与GA／GE配置（v0.1）

日期：2026-10-19（Asia/Tokyo）。施工基点`main / e8b18ceb55e38c7b47189f22300f13b572e7895c`，实际UE`5.8.3-58210709+++UE5+Release-5.8`。起始保留此前4份未提交说明改动。用户明确要求通过释放技能启动加速，复用现有Primary并替换免疫技能，不新增输入槽。沿用[F3的5秒GE](MVPB_Phase10F3_Implementation_CN_v0.1.md)和[F2运动桥接](MVPB_Phase10F2_Implementation_CN_v0.1.md)，不改蓝图资产／地图、车辆DA或网络物理协议。验证结果在本轮独立记录，不覆盖10月17日证据。

最新团队反馈：同日团队已手工完成加速GA／GE及Primary替换，报告双端表现符合逻辑；12:04～12:05的日志确认正常技能链路、刷新与到期恢复，见第9节及[手动复核摘要](MVPB_Phase10F3Ability_ManualLogReview_20261019.json)。当前下一入口为10-F-4集成回归。第2～7节保留为可重复使用的配置流程，不要求重建已完成的资产。

## 1. 代码职责与配置链

新增`UHALMovementBoostAbility`，`InstancedPerActor / ServerOnly`。它检查当前PlayerState／车辆Avatar、配置与Health／Movement属性、Playing阶段及合法GE，随后通过正常GAS接口施加`MovementEffectClass`并结束本次GA。五秒持续、跨来源一层刷新和自然到期仍由GE管理，Movement按原物理历史消费。

默认运动GE为原生`HALMovementBoostEffect`：5游戏秒，AccelerationMultiplierDelta +0.5、SpeedLimitMultiplierDelta +0.25，按目标一层成功刷新。该GA默认没有Cost／Cooldown GE，不借用免疫20秒冷却；施加后立即结束，后续按键可以刷新还在运行的GE。技能不调用`HALMovementBoost`，不读取HAL Debug的GE选择或调试开关，也不修改免疫／伤害／HP。

```text
原技能键／IA_PrimarySkill
 → 现有Primary输入路由
 → PlayerState ASC的Input.Ability.Primary授予
 → GA_MovementBoost（HALMovementBoostAbility子类）
 → GA自身的Movement Effect Class
 → GE_MovementBoost
 → Movement AttributeSet与既有物理桥接
```

原免疫GA／GE代码和资产保留。替换的是当前测试PlayerState的Primary授予引用；不自动覆写团队保存的DefaultAbilities，原生PlayerState的旧默认也不当作团队蓝图已迁移。下面步骤由团队在Editor手工完成。

## 2. 开始配置前

1. 保存并关闭运行中的Editor／游戏／Live Coding，使用本轮源码完整编译Editor Target，再重新打开项目。新增C++类不能靠仍运行的旧二进制出现。
2. 先停止PIE。确认当前使用NetTest对战GameMode及`BP_HALPlayerState`，不要改Entry菜单的GameMode。保留原免疫GA／GE资产，以后切回时只需恢复授予引用。
3. 以下名字是推荐资产名，可以换名，但各引用必须对应。不要在蓝图Event Graph添加技能核心、服务器RPC、Delay、Tick或施力节点。

本机完整Editor构建命令（PowerShell，在Editor／游戏／Live Coding关闭后运行）：

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' HAL_Future_CreationEditor Win64 Development 'D:\HAL_Future_Creation\HAL_Future_Creation.uproject' -WaitMutex -NoLiveCoding
```

确认`Result: Succeeded`后再打开Editor；遇到Live Coding占用提示时先保存关闭占用进程，不把独立Game Target成功当作此步骤完成。

## 3. 配置加速GE

如果已经按F3步骤创建了加速GE，可以直接复用并核对这些值。

1. Content Browser中在项目目录（例如`/Game/Blueprints/GE`）右键`Blueprint Class → All Classes`，选择**HALMovementBoostEffect**，创建`GE_MovementBoost`。
2. 打开`Class Defaults`，Duration Policy=`Has Duration`，Duration Magnitude使用Scalable Float，值=`5`；Period=`0`。
3. Modifiers已经从原生父类继承两项，直接核对已有项，不重复添加：

| Attribute | Modifier Op | Magnitude |
|---|---|---:|
| HALMovementAttributeSet.AccelerationMultiplierDelta | Additive | 0.5 |
| HALMovementAttributeSet.SpeedLimitMultiplierDelta | Additive | 0.25 |

这是增量，`1+0.5=1.5`、`1+0.25=1.25`；不要填写1.5／1.25。无需修改车辆基础DA。

4. Stacking Type=`Aggregate By Target`，Stack Limit Count=`1`，Stack Duration Refresh Policy=`Refresh On Successful Application`，Stack Expiration Policy=`Clear Entire Stack`，Deny Overflow Application关闭。
5. 保留继承组件的最终Tag：GE资产Tag含`Effect.Scope.Avatar`、`Effect.Vehicle.MovementBoost`；目标Granted Tag含`State.Movement.Boost`。不要混放，也不要用免疫Tag替代。
6. Compile、Save。

## 4. 配置加速GA

1. 在项目目录（例如`/Game/Blueprints/GA`）右键`Blueprint Class → All Classes`，选择**HALMovementBoostAbility**，创建`GA_MovementBoost`。不要从免疫GA复制后仅更换GE引用，它的父类验证不同。
2. 打开`Class Defaults`，搜索`Movement Effect Class`，在`Ability → Movement Boost`对应字段选择**GE_MovementBoost**。
3. 保留父类`Instancing Policy = Instanced Per Actor`、`Net Execution Policy = Server Only`。Cooldown Gameplay Effect Class、Cost Gameplay Effect Class保持`None`，不沿用免疫冷却GE。
4. 不添加ActivateAbility事件图，原生代码已完成施加与EndAbility。Compile、Save。

GE引用留空、时长非正、Period不为0、Tag／一层刷新配置不合法时会拒绝发动，不静默回退。非周期契约防止Periodic Modifier把增量执行到基础属性、到期后无法正常恢复。`Project Settings > HAL Debug > Movement Boost Effect Class`只用于旧调试命令；本技能读取**GA的Movement Effect Class**。如果调试命令生效而按键技能效果不同，首先核对这两处独立引用。

## 5. 用加速替换原免疫Primary

1. 打开NetTest实际GameMode使用的`BP_HALPlayerState`，点击`Class Defaults`，搜索`Default Abilities`。
2. 找到原来的`Input.Ability.Primary`项，**修改这一项的Ability**，不要再增加第二个Primary：

| 字段 | 配置 |
|---|---|
| Ability | GA_MovementBoost |
| Input Tag | Input.Ability.Primary（保持） |
| Level | 1 |

3. 数组保持原来的一个Primary技能。不要把GE放进授予数组，不改成Ability.Vehicle.MovementBoost或State.Movement.Boost；这两个是能力／状态标识，不是输入槽。
4. Compile、Save PlayerState。
5. 打开实际`BP_MVPB_ListenGameMode`的Class Defaults，确认Player State Class是刚修改的PlayerState。保留Default Pawn、PlayerController等原引用；NetTest World Settings的GameMode Override若已正确，不改或重存地图。

完成后，这份配置的原技能键发动加速，不再授予该槽的免疫GA。旧免疫资产不用删除；以后恢复时把此处Ability改回原GA即可，仍需完整重开局。

## 6. 复用原技能键

已能按键发动免疫的工程无需新建IA／输入Tag／槽位。原`IA_PrimarySkill`和映射可直接复用。

如需排查按键引用：沿玩家Vehicle DA的LocalConfig找到实际Local DA，确认Primary Skill Action=`IA_PrimarySkill`，其Value Type=`Digital (bool)`；沿Default Mapping Context找到现用IMC，保留之前的技能键映射。实际按键以IMC为准，不因为切换GA强制改为某个键。无需新增Hold触发器，5秒由GE管理；按下边沿提交一次，按住不会逐帧发动，松开再按可刷新。

## 7. 双端实际测试

1. 完整结束旧PIE／游戏进程，再Host／Join进入NetTest，由主机Start到Playing。仅改Class Defaults不会可靠地替换旧局已经授予的Ability Spec。
2. 两端可开启诊断`hal.AbilityLog 1`、`hal.MovementEffectLog 1`，执行`HALGASStatus`检查本地PrimarySkillGranted=1。诊断命令不是技能施加入口。
3. **按之前释放免疫的技能键**。服务器日志应出现`Movement ability: Accepted=1`，Ability包含`GA_MovementBoost`、Effect包含`GE_MovementBoost`（实际资产名可不同）。如果只显示原生类，说明使用了原生默认引用；如果仍出现免疫Ability accepted，检查PlayerState／GameMode和是否重开局。
4. 本地Movement boost显示ACTIVE，倍率约1.50／1.25。原免疫状态和20秒免疫冷却不应因本次加速而出现；HUD保留的Immune／CD字段通常为0，不代表加速失败。
5. 约2秒后松开并再次按技能键，效果剩余应重新接近5秒，倍率不翻倍。原到期点仍有效，最后一次按键约5游戏秒后恢复1／1；已有速度按原规则收敛，不硬清零。
6. 互换主机／客户端操作，检查仅发动者自己加速。Mixed复制下远端剩余时长可能为-1，重点看各自Owner端及服务器日志。
7. 可在主机设`hal.MovementBoostDebug 0`后继续按技能键，正常GA仍应生效；这验证没有走调试施加路径。可选`HALUseSkill`仅用于排查，它调用同一个Primary技能请求，不是直接施加GE；主要验收仍按实际技能键。

不能用客户端“Requested=1”独立证明成功；它只代表提交请求。以服务器接受、复制的GE／属性和物理帧值判断。Waiting／无合法车辆／错误GE配置应被拒绝。最终资产接线和真人按键／手感由团队按上述步骤验证，代理不保存蓝图。

## 8. 验证记录与阶段边界

本轮结果见[独立验证摘要](MVPB_Phase10F3Ability_Verification_20261019.json)：

- **代码、Editor／Game Win64 Development完整构建通过**。用户保存关闭Editor／Live Coding后，完整Editor成功；随后补充拒绝非零Period的防护与对应测试，最终顺序重编Editor／Game均成功（5.23／9.83秒）。仍有既有引擎C4996弃用及MSVC版本提示，不宣称零编译警告。首次Live Coding阻塞及之前通过的报告独立保留。
- **最终普通回归45/45通过，0测试警告／0失败／0未运行**。新增3项覆盖正常Primary激活、同帧重复／两秒刷新／自然GAS到期、调试开关与设置独立、Avatar／其他冷却／HP隔离、空GE／非正时长／非零Period／非车辆Avatar拒绝；旧42项也在最终源码下重跑，不借用历史报告。
- **真实Listen Server／客户端各1/1通过**，测试`HAL.NetworkSmoke.Phase10F3Ability`，NetTest、NullRHI、60FPS，主机发送方向注入80ms（非实测RTT）。夹具仅替换运行时PlayerState的Primary为原生加速GA，关闭调试施加；客户端先经`RequestPrimarySkill → ASC → GA`正常请求，主机确认对方生效后发动自己的技能。重复请求仍一层、无免疫／免疫冷却；两秒刷新后物理Revision仍为2，原到期点继续有效，最后刷新至物理恢复主机约5.034、客户端约5.150游戏秒，恢复1／1且Revision变3。客户端完成时已有4次回滚请求；不将此当作完整恢复／驾驶手感验收。
- Waiting用例在普通权威World确认拒绝；双端夹具的客户端Waiting尝试由本地CanActivate拒绝，不声称该次请求到达服务器。Playing后的真实客户端激活由服务器日志及Owner复制／物理消费共同确认。
- **329份uasset／umap施工前后SHA-256全部相同**，134份源码／配置哈希另存摘要。三个最终UE测试进程正常退出，最终日志未见Error／Ensure／Assertion／Fatal。施工交付时保存资产仍是原免疫授予；后续团队接线见第9节。本段构建／自动化过程未执行真人按键／手感／HUD实际渲染验证，未提交或Push。

施工交付时的代码／编译／自动化出口已完成，团队接线及后续实测记录见第9节。自动化证明的是原生类和正常技能激活通道，不代替团队保存的子蓝图接线或真人按住／松开行为验收。原始报告位于`Saved/Phase10F3Ability`，不随Git同步；共享验证摘要保存哈希、测试清单和关键日志。

F4的完整持球／发射／冲击／免疫组合、网络丢包／抖动与四台有线PC仍保留。本次只替换当前Primary的测试配置，不建设第二槽位、技能库或运行时换技能框架。

## 9. 团队双端反馈与日志复核（2026-10-19）

用户报告双端表现符合逻辑。本轮接收`main / a38498e46e0cb1275f347af1e1badf7d104222f0`，加速GA源码／文档已提交；当前331份资产，团队新增`GA_MovementBoost`／`GE_MovementBoost`、修改`BP_HALPlayerState`，这3项仍未提交。代理只读日志及哈希、维护文档，没有改源码／资产、启动UE或重跑编译／自动化。原摘要中134份源码／配置哈希全部一致，45项等构建证据仍是之前独立的验证结果。

复核`Saved/Logs/MVPB_10C_Host.log`与`MVPB_10C_Client_2.log`，确认UE5.8.3、Entry进入NetTest、127.0.0.1:7777，12:03:00开始双人Playing。另检查紧邻Client_2备份与早先Client启动日志，它们不是额外一局配对测试。快照保存在`Saved/Phase10F3Ability/ManualReview_20261019`；[共享JSON](MVPB_Phase10F3Ability_ManualLogReview_20261019.json)保存原文、行号与SHA-256，不覆盖施工验证JSON。

详细Ability／Movement日志中途才开启，完整观察到服务器接受8次技能：主机PlayerId257四次，客户端PlayerId258四次；实际类均为`GA_MovementBoost_C`／`GE_MovementBoost_C`，Duration／Remaining均5秒，Primary输入Matches均1。主机两个效果窗口各含一次刷新，客户端首个窗口含两次刷新，第二个无刷新；每次新窗口Stacks=1，刷新期间数值始终1.50／1.25，没有额外物理Revision边界，也未见免疫／免疫冷却GE被施加。

下表时间为日本时间12点，秒差使用日志UTC时间戳计算；GA自身另记录5游戏秒。它们不是RTT指标。

| 发动者／窗口 | 最后一次发动或刷新 | GE移除 | 秒差 | 物理Revision／恢复 |
|---|---|---|---:|---|
| 主机1 | 12:04:17.851 | 12:04:22.851 | 5.000 | 4→5，1／1 |
| 主机2 | 12:04:25.966 | 12:04:30.969 | 5.003 | 6→7，1／1 |
| 客户端1 | 12:04:54.506 | 12:04:59.511 | 5.005 | 2→3，1／1 |
| 客户端2 | 12:05:00.127 | 12:05:05.128 | 5.001 | 4→5，1／1 |

客户端Owner日志对应收到两次新GE与两次移除，物理消费确认2／4为加速、3／5为恢复。共159条服务器、1681条客户端物理采样均Ready=1、Recovery=0；客户端回滚请求计数最大5，存在实际Replay记录，末尾恢复1／1。Replay逐帧日志数量不是独立回滚次数或性能结论。Actor后缀在不同进程不一致属正常：服务器客户端Pawn_C_1在客户端为本地Pawn_C_0，按PlayerId／Epoch关联，不把名字差异当错误目标。

未见Ensure／Assertion／Fatal、GAS配置／复制警告或对战中NetworkFailure／超时。两端Entry启动各有既有`InputMode:UIOnly - Attempting to focus Non-Focusable widget`错误，另有音频采样率转换和EditorDataStorageUI启动提示；不声称日志零Error。客户端12:05:10.558关闭窗口，主机随即清理连接，主机12:05:11.708也正常关闭，属于主动退出。

**结论：团队加速GA／GE接线和基础双端技能实测通过，观察到的刷新、到期、复制及物理恢复正常，当前没有新增阻塞问题；下一入口10-F-4。** 已有少量控球／碰撞脱球记录，仍不能代替完整发射／冲击／免疫组合、按住输入边沿、带活动GE退出／换Avatar、实测RTT／丢包／抖动和四机验收。本轮未施工F4、未提交或Push；换机前应将上述3项团队资产与本次留档一起提交。
