# MVP-B 10-F-3：首个运动GE与调试施加（v0.1）

日期：2026-10-17（Asia/Tokyo）。施工基点`main / 599d60ab5856eea348d1b20e3bd437d4ff6fa8f5`，起始工作区干净；实际UE`5.8.3-58210709+++UE5+Release-5.8`。用户授权接续下一步，本轮完成10-F-3，不扩展F4完整集成或弹簧拳接入。架构沿用[F1规格](MVPB_Phase10F1_Spec_CN_v0.1.md)与[F2桥接](MVPB_Phase10F2_Implementation_CN_v0.1.md)，保存DA基线继续使用[F1记录](MVPB_Phase10F1_Baseline_20261017.json)。本轮证据见[F3验证摘要](MVPB_Phase10F3_Verification_20261017.json)，不覆盖F2历史报告。

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

## 4. 本轮验证边界

新增3项普通测试覆盖真实World／GAS计时的5秒自然到期、跨来源一层刷新且重计5秒、不同GE贡献／选择性驱散、关闭施加开关、Waiting／无Avatar／空配置拒绝、Avatar切换与销毁、HP／其他冷却保留。原39项继续作为回归；GAS使用World游戏时间，不用手动移除模拟自然到期，也不修改全局帧计数。

`HAL.NetworkSmoke.Phase10F3`另起真实NetTest Host／Client，60FPS、NullRHI；主机发送方向注入80ms延迟（不是实测RTT）。客户端先发：比赛开始前请求确实被拒绝；Playing后客户端同帧三次请求仍一层，主机确认该请求只影响客户端，然后自己施加。双方约2秒刷新，确认物理Revision保持、原到期点继续有效、自然到期恢复后Revision递增；第二轮并行开启原免疫技能，再主动清除加速，确认免疫／冷却和Primary授予保留。两轮间保留可复制的中性区间，避免测试自身把远端“移除／重施”压在同一次状态采样里。

最终UE5.8.3 Editor／Game Development构建成功；普通自动化42/42、0测试警告／0失败（原39＋新3），最终Host／Client各1/1成功、0警告／0失败。主机从刷新请求到物理恢复约5.033游戏秒，客户端观察约5.150秒；GAS Spec本身为5秒，观测包含GT采样、物理边界与复制延迟。Revision依次1→2、刷新保持2、自然到期3、再次开始4、清除5；最终双方免疫仍有效，原冷却约剩18.9秒。日志无Error／Ensure／Assertion／Fatal或GAS复制警告；编译仍有既有引擎／兼容API弃用及MSVC版本提示。

329资产前后SHA-256全同，无新增／删除，未改保存Config／DA／弹簧拳。全部专用进程均退出。详细报告、源码／日志哈希与早期失败见[F3验证摘要](MVPB_Phase10F3_Verification_20261017.json)。首轮编译只在跨来源测试夹具发现旧Target接口要求可写GE指针，改用GAS Spec-to-target接口后修复；不对原生CDO做const_cast。一轮网络复测因固定启动等待导致客户端错过Waiting，客户端报告失败；随后改为客户端完成检查后经已有可靠SetName请求标记就绪、主机收到标记才Start，保留原拒绝断言并通过最终复测。最终仅该实时网络夹具在42项普通回归之后改变，生产／普通测试源码保持，两个Target重编并重跑网络；没有把失败报告或进程退出码当成功。

本轮HUD字段已编译及日志核对，未做新增行的实际渲染视觉验收。

下一入口为**10-F-4集成回归**：团队做真实驾驶／漂移／控球／发射／冲击／免疫／退出与墙角／腾空／连续接触组合，补实测RTT、丢包／抖动和四台有线PC证据。F3的GE状态共存检查不等于完整物理组合验收，F2的1500ms历史恢复仍为其独立历史证据。本轮不改车辆调参、F2物理协议、弹簧拳或独立禁捕设计，改动未提交／Push。
