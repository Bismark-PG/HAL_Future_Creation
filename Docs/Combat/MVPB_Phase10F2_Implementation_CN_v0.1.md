# MVP-B 10-F-2：运动属性、物理帧桥接与恢复（v0.1）

日期：2026-10-17（Asia/Tokyo）。施工基点`main / ca38226716392389ac19d4b0184629f020b7713f`，起始工作区干净；本机UE`5.8.3-58210709+++UE5+Release-5.8`。用户指定开始10-F-2，本轮范围以[10-F-1规格](MVPB_Phase10F1_Spec_CN_v0.1.md)为准。保存数值继续使用[F1基线](MVPB_Phase10F1_Baseline_20261017.json)，当前证据汇总到[F2验证摘要](MVPB_Phase10F2_Verification_20261017.json)。不覆盖F1或10-E旧记录。

## 1. 实现与消费路径

新增`UHALMovementAttributeSet`，由原PlayerState注册到Mixed ASC，初值为两项增量0。`AccelerationMultiplierDelta`和`SpeedLimitMultiplierDelta`保存原始有限增量；不同GE的Additive贡献由GAS相加，在生成有效值时分别计算`clamp(1 + Delta, 0, 3)`。不截掉贡献，否则移除另一GE后会错误恢复。

Pawn原生组装`UHALMovementEffectBridge`。服务器绑定当前Avatar时建立Epoch并主动采样，重复绑定不重置身份；属性回调仅置Dirty，WorldPostActorTick合并采样最终配对值。同倍率刷新不发虚假参数边界；真实到期／驱散才产生新的Revision。组件无Tick，不在物理线程读取ASC、GE句柄或Tag。

```text
服务器GAS有效增量
 → GT最终双倍率／Epoch／Revision
 → Network Physics可靠Authority参数动作
 → PT记实际生效ServerFrame
 → 分段常量时间线／每帧快照
 → 驾驶施力；PostSolve采集S(F+1)
```

`FVehicleMovementParameterAction`只携带绝对值，客户端输入协议未增加倍率／时长。Standalone使用同样的GT采样，在下一正常物理步消费纯值；模拟代理继续PredictiveInterpolation，不新增驾驶施力。

`ApplyLongitudinalForces`在消费时计算前进／倒车加速度与两个驾驶上限，不覆盖基础配置，不反复ApplyConfiguration。转向继续用原基础速度标尺，刹车、抓地、滑行阻尼和超速修正率保持原值。0倍加速度不会冻结刚体；已知0倍上限仍沿用原超速修正。结束不硬写线／角速度，腾空路径不新增制动。未知旧帧恢复时暂停主动驱动和无法确定的上限修正，保留已有碰撞、基础阻力与刹车。

运动倍率没有接到伤害、冲击档位或后坐动作上；实际车速对后续球发射／碰撞的自然影响仍由原管线决定。

## 2. 历史协议与恢复

`FHALMovementSnapshot`包含Epoch、Revision、实际生效服务器帧和两个有效倍率。`FHALMovementTimeline`为带版本号的有界分段常量区间，保存覆盖起点的锚点及离散变化；查询与插值不Lerp倍率，也不按最近端点选版本。State的S(F)保存F帧开始前的快照，因此插值查询F-1，不提前执行F帧动作。

时间线容量跟随实际State History，本次服务器60帧、拥有客户端64帧；不把保存的1000ms设置直接当运行时帧数。最多支持254历史帧，超过时明确报错并将桥接标为未Ready，不静默裁掉可回滚变化。序列化先验证版本／上限再分配，拒绝NaN、越界倍率、无效身份、倒序和同Revision冲突；相同动作重复不累乘，同帧保留最高Revision。

拥有客户端每正常物理步从缓存History有界扫描最多4份新收到的精确State，将权威覆盖与实际用过的本地帧比较。窗口内迟到先验证Chaos、输入／State和参数锚点，再请求原帧回滚；不把旧动作改为当前帧。窗口外／缺锚点进入Recovery，等待后续权威检查点。恢复检查点必须是实际收到的State，成功接受后废弃其之前不可恢复的旧预测比较，避免每包重复请求同一个失效前缀。

回滚使用当帧时间线／快照与已记录动作，GE已到期也不把旧加速帧改为1。GAS时钟只在原游戏线程推进，不因物理重模拟重新施加效果或重复扣血／脱球。Epoch切换清除旧待发、历史和恢复请求；Avatar原有作用域清理继续移除其GE，PlayerState的HP和其他冷却保持原职责。

## 3. 本轮发现并修复的既有回滚缺口

第一轮真实双端测试看到倍率开始／到期和回滚请求，但车辆重模拟计数始终0，该轮客户端测试明确失败。引擎源码确认：UE5.8通用`FAsyncPhysicsTickCallback`带`RunOnFrozenGameThread`，`ConditionalApplyRewind_Internal`用PT-only任务重放，刻意跳过这类回调。因此原车辆Async Tick在正常步运行，在回滚步没有执行驾驶。

本轮增加窄的`FVehicleRewindCallback`（Chaos Presimulate／Rewind）：**只在真实IsResimming时**补调用原驾驶步骤，正常步仍由原Async Tick执行。恢复输入／参数动作发生在其前，不增加第二次正常施力，也没有自建Solver推进循环。组件结束时先以原子指针撤销回调目标、等待在途调用结束，再向Solver注销；历史仍保留的回调不会访问已销毁组件。回滚线程不执行调试绘制。

该修复也让已有驾驶、转向和脱困记忆在回滚时真正参与物理，因此F4仍须重做真实持球／冲击／墙角等组合体验。普通自动化和本轮网络smoke不代替这项团队验收。

## 4. 诊断与复验入口

`hal.MovementEffectLog 1`打开Epoch／Revision、ServerFrame／LocalFrame、双倍率、Ready／Authority／Replay／Recovery、实际历史窗口和回滚次数；`0`关闭。`HALGASStatus`和本地HUD增加有效倍率、Ready、Revision及恢复信息。模拟代理的Movement未运行，不能用它的未Ready诊断判定拥有者失败；远端效果显示可读其复制的Attribute。当前HUD仅通过源码／运行日志检查，未做本轮实际渲染视觉验收。

普通回归过滤器为`HAL.FutureCreation`。新增4项覆盖：离散边界／乱序／重复／缺口／裁剪、序列化与离散插值／旧区间差异、真实GAS贡献合并移除／重复绑定，以及真实TaskGraph Solver工作线程中的施力与历史参数消费。Solver夹具在真实物理步测积分速度，旧帧重放用生产消费函数；完整网络重模拟另由下列双进程测试证明，二者不混称。

选择性网络测试为`HAL.NetworkSmoke.Phase10F2`，必须在同一构建的NetTest Listen Server和拥有客户端各启动一次，两端帧率上限60，NullRHI。测试自动Start，两辆车依次取得**原生测试夹具GE**（持续2游戏秒、+0.5／+0.25），观察自然到期；不是F3正式5秒GE，也没有加入DefaultAbilities或可由玩家调用的Buff命令。

- 常规模式：主机在客户端目标获得效果前注入80ms单向发送延迟，随后复原，验证拥有者物理倍率开始／恢复、远端属性和真实重模拟。
- 两端额外加`-HALMovementRecoveryTest`：注入1500ms单向发送延迟，超过实际历史窗口，要求客户端检测Recovery并从检查点恢复到1/1、Ready及真实重模拟。

延迟值是注入的单向设置，未测RTT；测试不附带1%丢包／20ms抖动，也不算四台PC验收。引擎启动脚本、原始日志和HTML报告位于忽略的`Saved/Phase10F2/`；可跨设备核对的状态、路径、哈希和摘要见验证JSON。

## 5. 本轮验证与下一步

最终UE5.8.3 Editor／Game Development构建成功；`HAL.FutureCreation`全部39/39通过，0测试警告／0失败，含原35项与新增4项。最终80ms、1500ms两工况各Host／Client共4份网络报告均1/1成功、0警告／0失败；客户端完成时真实重模拟分别79／52次，运动回滚请求3／4次，最终均Revision3、倍率1/1且Ready，1500ms场景明确观察到Recovery后恢复。日志未发现Error／Ensure／Assertion／Fatal或GAS复制警告。编译仍有既有引擎／兼容API弃用和MSVC版本提示；不把“测试零警告”写成编译无警告。

329个`.uasset`／`.umap`前后SHA-256全部一致，无新增／删除；配置、DA和弹簧拳未改。所有专用进程均已退出。完整路径、源码／日志／报告哈希、早期失败和限制见[F2验证摘要](MVPB_Phase10F2_Verification_20261017.json)，不以UE退出码代替自动化结果。本轮交付未提交／Push。

下一开发入口为**10-F-3首个正式运动GE**：5游戏秒、加速度×1.5／驾驶上限×1.25、AggregateByTarget一层且跨来源合法刷新，增加窄服务器调试入口，验证刷新／驱散／自然到期、权限与Avatar清理，并提供必要的团队Editor步骤。F2不要求编辑蓝图资产；原有免疫主技能继续保留。完整驾驶／控球／发射／冲击／免疫／退出及扰动／四机留给F4，弹簧拳战斗接入仍等阶段10完成后。
