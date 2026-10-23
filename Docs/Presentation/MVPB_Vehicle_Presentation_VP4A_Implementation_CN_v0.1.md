# MVP-B VP-4A：发射视觉回弹与增强配置（v0.1）

日期：2026-10-23（Asia/Tokyo）。基点`main/bf21ebc024df84c00a67a78aba949806776d3664`，开工工作区干净，本机UE5.8.2-56702186，885份资产。用户授权VP-4A，并反馈现有发射回弹不明显。此前VP-3接线／初测保持；单Host测试GameMode已由团队提交，另一成员的弹簧拳更新保留。

## 1. 两种后坐／回弹

| 通道 | 配置入口 | 实际影响 |
|---|---|---|
| 本步发射视觉回弹 | 当前Vehicle Definition → Presentation → 车型表现DA → Launch Rebound | 车身骨骼短促后仰／回正，增强动作可读性，不改变速度或命中 |
| 既有物理后坐 | 当前Vehicle Definition → Ball Control → Launch | 发射时车辆真正向后改变速度，继续使用原权威／预测物理动作 |

施工前没有独立发射视觉回弹，原车身主要受VP-2加速度俯仰影响。不能把提高模型后仰幅度当作增加物理后坐，也不能为视觉再加一次冲量。本步不修改已保存DA、模型、AnimBP、地图、球发射强度或物理参数。

## 2. 实现与联机规则

- 沿用既有表现组件、DA、帧快照与AnimInstance，增加独立`LaunchRebound`通道；默认关闭，保留旧资产行为。回弹复用现有Chassis局部Pitch轴及符号，不旋转整个物理根。
- C++使用有限时长解析包络：Rise阶段平滑到峰值，Return阶段平滑回零，起点／峰值／终点速度为零。不是依赖帧率的弹簧积分；不出现自动不断振荡。配置可逐车型调整。
- 客户端输入边沿已观察到唯一受控球时即时预播。服务器仍按原球状态／请求检查决定发射，不从表现反推玩法；拒绝通过原可靠拥有者结果使预播平滑收回。
- 成功发射沿用`LaunchSequence`并携带服务器游戏时间。原拥有者结果RPC新增时间字段；另加一次轻量不可靠Multicast给当前观察者，主机本地也走同一消费函数。可靠结果与Multicast先后到达不会重复起播；确认保留预播年龄，不重置回弹。
- 远端按服务器事件年龄进入对应包络阶段；旧事件、已过包络、重复／倒序未知身份、旧生命周期动作拒绝。允许最多0.25秒的小幅未来时钟误差并按年龄0处理；更异常时间拒绝。装饰Multicast允许丢失，不补播晚加入的历史动作，不承诺每次观察者视觉必达。
- 最多保留8个重叠发射包络；递增序号水位跨普通姿态重置保留，避免已结束／清理的预播被迟到确认复活。长帧、瞬移、控制者变化、重配／结束沿用原有清理；事件年龄还须通过当前绑定的时间界线。
- 驾驶俯仰滤波独立保留。发射包络越接近峰值，驾驶Pitch贡献越弱，在单发峰值由发射幅度主导，避免后坐造成的减速前倾抵消后仰；随后平滑恢复原驾驶俯仰。Roll仍保留，最终Pitch／Roll分别统一限幅。
- `Presentation.ChassisRotation`直接输出最终合成姿态。原`ChassisPitchDegrees`仍为驾驶滤波诊断；新增`LaunchPitchDegrees`和`CombinedChassisPitchDegrees`只作诊断，不要在蓝图再次相加。
- 不改变Network Physics历史、伤害、冲击／免疫、持球／发射资格、物理后坐公式；物理重模拟不生成新的视觉发射。没有扩展VP-4B受击、落地回弹、持续振动或摄像机震动。

RPC字段有变化，主机和客户端都须同步同一提交并完整重编；不要用旧游戏进程与新进程混测。

## 3. 如何启用和增强视觉回弹

团队在Unreal Editor手工操作，代码代理未编辑任何uasset：

1. 保存需要保留的工作并关闭旧Editor／游戏；完成完整Editor Target构建后重新打开项目。
2. 找到实际玩家Vehicle Definition（常用`DA_Vehicle_Player_TestBaseline_v1`），沿其`Presentation`引用打开当前车型表现DA（Car_01通常为`DA_Car01_Presentation`）。不要在车辆BP旧灰色字段或PlayerState中调此效果。
3. 展开 **Launch Rebound**，勾选 **Enabled**。新字段默认关闭，必须由团队启用并Save。
4. 建议先用以下值观察，再按车型轮拱／画面调节：

| 字段 | C++默认／范围 | 增强建议与含义 |
|---|---|---|
| Pitch Degrees | +8°，-30～30° | 先8°，不够明显可到12°；正值沿已校准的Chassis Pitch方向，负值反向 |
| Rise Seconds | 0.05秒，0.01～0.3 | 到峰值的时间；较小更利落，过小低帧率可能跳过峰值 |
| Return Seconds | 0.25秒，0.05～2 | 从峰值回正的时间；可到0.35秒，延长可见性 |
| Rejection Fade Seconds | 0.08秒，0.01～0.3 | 服务器拒绝预播后的收回时间，通常保持 |
| Maximum Event Age Seconds | 0.5秒，0.05～2 | 未知远端事件的最大允许年龄，不是回弹持续时间 |

5. 展开 **Chassis → Composition**，检查 **Total Pitch Limit Degrees**（默认30°，0～45°）没有小于期望发射幅度；**Total Roll Limit Degrees**默认30°、0～45°。这是最终模型偏移上限，不是物理翻车角限制。原Max Pitch／Roll Degrees仍只控制驾驶倾斜。
6. 保留既有Chassis Pitch Axis／Sign及轮转／悬挂调参。若后仰方向反了，可以只把Launch Rebound的Pitch Degrees改负，不必改坏已有驾驶俯仰方向。
7. **Save DA**并新开游戏测试。现有AnimBP已经把`Presentation.ChassisRotation`连接到`Chassis_jnt`、Rotation Add to Existing／Bone Space时，**无需新增节点**。只有新车型没有该节点时，按VP-2文档添加一次；禁止为发射重复叠加同一Chassis Rotation。
8. 如字段未显示，确认Editor完整重启；如需要新增诊断引脚，对Break／拆分节点Refresh Nodes，Compile／Save。Event Graph不新增Tick、Curve或插值。

新通道不要求Chassis的驾驶Enabled同时开启，但仍使用其已校准Pitch轴和符号；最终动画节点必须存在。先检查Enabled／引用／节点和最终上限，再逐渐增大幅度，不必直接拉满30°。

## 4. 如果需要增强真实向后位移

保持本步视觉配置独立，在**实际车辆Definition → Ball Control → Launch**调整：

- **Base Recoil Delta Speed**：静止／低速发射的最低向后速度变化。可从当前已调值增加25%～50%试验。
- **Moving Recoil Fraction**：前进速度参与后坐的比例；想增强高速发射反馈时调整。
- **Max Recoil Delta Speed**：速度变化上限，须不小于Base；当前效果已经触顶时，单独增加Fraction不会继续变强。

原公式为`Clamp(Max(Base, Max(前向速度,0) × Fraction), Base, Max)`，速度单位cm/s。C++初始值180／0.15／420只是初始默认，**不代表团队当前保存DA值，不能用它覆盖现有调参**。例如当前Base确实180时，可先试270，并保证Max至少270；若当前不同，按其实际值增幅测试。每次改一个变量并同时观察发射后躲避／停车距离。

上述为团队调参说明，本轮没有代团队增加真实后坐或修改球速。

## 5. 调试与验收

控制台`hal.VehicleLaunchVisualLog 1`记录Predict／Accept／Reject及序号、年龄、起播／确认计数，`0`关闭，默认关闭。查看AnimBP的`Launch Pitch Degrees`、`Combined Chassis Pitch Degrees`、最终`Chassis Rotation`。峰值数字变化而模型不动时优先检查AnimGraph节点和空间。

单Host即可检查静止／前进／倒车发射，8°／12°、0.25／0.35秒差异，无球不回弹，反复发射可回正，轮转／悬挂和球行为保持。双端另查主机发射、客户端发射的本车／观察者，预播确认不二次弹，拒绝平滑收回，多个事件／退出／重开不留旧姿态。完整扰动／四台PC仍按F4统一收尾；单人测试不能代替客户端预测验收。

## 6. 本次验证

代码与原生验证完成，团队启用DA、实际画面及手感仍待反馈。跨设备摘要见[本次验证JSON](MVPB_Vehicle_Presentation_VP4A_Verification_20261023.json)，原始报告在本机`Saved/VehiclePresentationVP4A/20261023/`，Saved不提交。

- UE5.8.2：Editor／Game Win64 Development均成功；最终普通回归**71/71，0测试警告／0失败**，其中新增4项发射专项。覆盖30／60／120Hz解析包络、峰值／回正、确认不重播、拒绝淡出、乱序／重复／迟到、有界重叠、序号回绕、旧配置默认关闭、驾驶俯仰抵消与统一限幅。
- 原生Authority测试使用保存的车辆／球类，通过真实输入／发射路径产生一次成功事件；视觉Tick不改变根变换、线／角速度、质量或碰撞，长帧清零，迟到同序确认不复活。世界不推进物理模拟，证明表现更新边界，不代替实际驾驶。
- 实际`MVPB_NetTest`双进程Listen Server，7824端口、NullRHI、60FPS，**Host／Client各1/1、0测试警告／0失败**。客户端立即预播成立；本地2次起播＝1次真实发射＋1次被权威拒绝的预播，确认1／拒绝1；两个观察者各只起播1次。Host本地／远端峰值各8°；客户端远端8°、本地两次重叠原始峰值10.074°，最终均归零。客户端记录25次重模拟，未增加起播次数。
- 网络夹具使用临时DA副本、关闭车辆重力并由服务器移到隔离高处。控球准备是原生BeginControl＋测试内存HeldBall引用，拒绝使用短暂陈旧客户端球视图并立即恢复；实际输入／请求／服务器检查／RPC仍走生产路径。**不据此宣称捕获约束、正常驾驶或AnimGraph渲染已验收**，没有注入网络扰动或测RTT。
- 首两次完整回归均70/71：新增Authority夹具最初误用Legacy全局规则，且遗漏Pawn BeginPlay，导致HP尚未初始化、V1攻击快照拒绝发射。移除Legacy范围、补正常BeginPlay和Ready断言；没有绕过生产发射条件，先专项4/4、再完整71/71通过。早期失败报告保留，不覆盖。
- **885份uasset／umap前后SHA-256逐文件完全一致**；178份最终源码／配置留哈希，只有11份预期生产／测试源码变化，Config、uproject、模块依赖与其他成员代码保持。专用UE进程均已正常退出。本轮未提交／Push。
- 最终普通日志无Warning／Error；双端各有21行启动期`LogEditorDataStorageUI`工厂未注册警告，均不在自动化用例内。双端无Error／Ensure／Assertion／Fatal，本次不扩展修理引擎编辑器启动提示。

本轮没有进行实际渲染、DA启用／保存、正常驾驶组合、UE5.8.3、完整100ms实测RTT／1%丢包／20ms抖动或四台有线PC验收。下一步先按第3节手工启用并检查单Host及双端画面，反馈后进入VP-4B；VP-4C组合收尾及F4继续独立记录。
