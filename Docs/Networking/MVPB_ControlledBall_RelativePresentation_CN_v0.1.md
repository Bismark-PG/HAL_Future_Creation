# 持球相对表现：实施与团队验证（v0.1）

日期：2026-10-30（Asia/Tokyo）；施工基点`main/945e616`，本机UE5.8.2-56702186。用户已授权将[四人延迟分析第5节](MVPB_FourPlayer_Ball_ClientDelay_Review_CN_v0.1.md#5-转弯速度变化时滞后的定向方案讨论尚未授权施工)的Controlled相对表现与状态交接实施；发射视觉预测仍是后续小步。开工保留前步.gitignore、状态及分析／证据，未直接编辑保存资产。

**当前：代码、本机Editor／Game构建、107项原生回归及两组真实双进程检查完成；等待团队重新进行四机画面复测。** 本轮结果与早期失败见第5节及[验证摘要](MVPB_ControlledBall_RelativePresentation_Verification_20261030.json)，不将前步只读检查或旧测试视为本次通过。

## 1. 权威与采样

- `BasicBallActor`新增原生默认子组件`HALBallPresentationComponent`。既有服务器球约束、球状态、捕获／发射许可、伤害与运动复制保持。
- Controlled时服务器注册一个PostSolve回调，直接读取同一步的球和持有车刚体已求解位置／旋转／速度。物理线程不读取UObject、蓝图、DA、GAS或GT Transform；相对坐标为不重复应用车型Scale的车体坐标，单位cm。相对线速扣除持有车平移及旋转贡献，保留相对角速以呈现滚动。
- GT在PostPhysics消费样本，以20Hz上限复制`StateSequence＋Holder身份＋PhysicsFrame＋ServerPublishSeconds＋相对姿态／速度`，首个有效样本尽快发布。不是每渲染帧RPC，也不在车辆上增加另一份球权。该Holder／序号只用于与原球状态匹配。
- 状态／持有者变化时，先撤销并排空在途采样回调，再移除；退出及组件注销也清理，避免旧球权数据读取已销毁持有者。

## 2. 客户端显示与交接

- 可见网格使用“当前持有车显示变换 × 平滑后的权威相对变换”。自己的车沿现有预测／纠正姿态，远端车沿现有插值姿态；不修改车辆或球的物理根。
- 样本须匹配Controlled的StateSequence与Holder；重复／旧样本拒绝，Holder未解析时等待，过期或无效时使用原PI显示。相对外推最多0.1秒，不允许网络停更后无限追算。权威球相对惯性与吸入结果仍保留，未建立独立的客户端吸球力或固定吸入曲线。
- 仅对`VisualMesh`临时使用世界位置／旋转；保留原网格局部偏移、旋转、缩放和父级。进入相对显示时保持上一可见世界姿态并衰减交接误差；发射、脱球、持有者失效、开关关闭或过期时，用有界交接回到物理根的PI姿态，随后恢复原相对变换。滚动来自权威旋转／角速度。
- Controlled客户端仍保留Chaos物理状态、Ignore碰撞响应、关本地重力；退出Controlled后由原Actor逻辑恢复。没有NoCollision销毁刚体、Attach真实球、SetActorLocation或新客户端约束。
- Host／Standalone画面继续直接显示服务器球；本次不消除球权首次确认、真实发射的往返等待，也没有实现假球发射、客户端命中或全局球Resimulation。异常网络下回退PI仍可能出现可见校正，不能承诺任意丢包下像素一致。

## 3. 配置与开关

原球DA新增`Presentation`结构。旧保存资产缺省读新字段默认值，既有物理／伤害／控球参数不覆盖；**无需创建新DA或改蓝图Event Graph**。新配置仅影响表现，原生运行时若其无效会记录Warning并回退PI，不阻止车辆／球物理；Editor资产Validate同时检查这组值。

| 字段 | 默认值 | 说明 |
|---|---:|---|
| Enabled | true | 启用本轮相对显示 |
| Send Rate Hz | 20 | 服务器样本复制上限，5～60 |
| Relative Blend Seconds | 0.05秒 | 权威相对姿态的平滑响应时间 |
| Handoff Seconds | 0.12秒 | 出入相对显示的有界交接 |
| Stale Seconds | 0.5秒 | 无新样本／时间过旧后回退PI |
| Maximum Extrapolation Seconds | 0.1秒 | 相对速度外推上限，不大于Stale |
| Maximum Relative Distance | 2000cm | 仅表现数据安全范围，不改变捕获／脱球距离 |

客户端控制台`hal.BallRelativeVisual 0`返回原PI显示，`1`启用，供同场A/B；不改服务器约束或球权。`hal.BallPresentationLog 1`输出模式变化和每秒一次的帧／样本／年龄计数，结束设0。原`hal.BallNetLog`、`hal.VehicleNetLog`继续用于状态和发射许可诊断。

## 4. 团队Editor步骤与手测

1. 保存需要保留的资产并关闭旧游戏／Editor；完整重编后重开。打开`BP_BasicBall`的组件列表，确认新增原生`Ball Presentation`存在即可，无需拖组件或接Event Graph。
2. 打开实际球Definition，保留已有Visual、Physics、Gameplay数值；新增Presentation可直接先用默认值。只有调整时才手动保存／Validate该DA，全部参与电脑同步同一代码／配置并重启游戏。代理不保存这些资产。
3. 按原菜单／命令行启动Listen Host与客户端进入NetTest，Host开始。两端开启`hal.BallNetLog 1`、`hal.VehicleNetLog 1`、`hal.BallPresentationLog 1`；保留`stat unit`、`stat net`短时采样。Controlled时Host应看到Published增长，客户端Received增长且Mode=1（Relative）；Mode=0为原Physics，2为Handoff。UI与日志只读，不保证实际画面已验收。
4. 主机持球后直走、急转、加速／减速／倒车，再换客户端持球；在客户端分别观察自己的球和别人球。短时切`hal.BallRelativeVisual 0/1`比较转弯时车球错位，不能改质量／约束或关闭其他网络规则来做对比。
5. 观察实际吸入过程和球面滚动；测试吸入中立即发射、快速重复发射、脱球／重捕、持球撞墙、外部打断及持有者退出。球进入Free／Launched后必须退出相对显示，回到原球运动；不得留下可见球挂在车头或瞬间跳回旧根位置。
6. 检查网络抖动／样本过期、球权切换及引用晚到时能回退并恢复。客户端碰撞调试球形代表物理根PI位置，可与相对可见网格暂时不同，不能拿可见网格反向驱动命中；Host物理判定仍为准。
7. 基础双端后按当前四台学校有线环境复测，记录版本、Host帧时间、Ping和视频。当前改造是针对车球时间线错位，若仍有明显延迟，保留诊断继续区分Host／传输／相对偏移迟到。完整F4、VP复杂组合与四机长时验收不因本步自动关闭。

## 5. 验证记录

本机UE5.8.2-56702186，最终源码226份构建输入在验证后未变。以下为本轮最终版本结果：

| 检查 | 结果与证据范围 |
|---|---|
| Editor／Game Win64 Development | 均成功；最终回调修复增量构建约17.79／18.89秒，之前完整构建记录保留；既有相机C4305等编译警告不算已清除 |
| `HAL.FutureCreation` | **107/107**；报告0测试警告／失败／未运行；包括新增`Ball.RelativePoseContract`、`Ball.RelativeVisualLifecycleAndPhysicsIsolation` |
| `HAL.NetworkSmoke.BallRelativePresentation` | **Host／Client各1/1**；真实地图与保存球／车配置，测试内调用原生AcquireBall建立服务器吸球约束，再持续输入驾驶转弯、双方发射，客户端最终返回Physics并恢复网格相对变换 |
| `HAL.NetworkSmoke.VehicleLaunchPresentation` | **Host／Client各1/1**；原发射许可、后坐力表现、预测确认／拒绝回归；客户端2次预测、1次拒绝、1次确认，39次实际重模拟。此旧夹具仅辅助设置持球状态，不代替上一行真实约束检查 |
| 资产／构建输入保护 | **1172份uasset／umap前后SHA-256全同**；226份Source／Config／uproject构建输入验证后未变，Config／uproject／Build.cs无内容改动。没有代理保存资产、提交或Push |

相对显示双端运行使用60 FPS上限、NullRHI，**80ms是Host出站延迟注入，不是实测RTT**。服务器完成点两个球分别发布43／45个样本；客户端自己的球收到42个、远端球22个，相对显示135／129帧，完成点57次车辆实际重模拟。测试断言包含实际驾驶位移、两条持球路径持续生效、球物理根保持模拟、发射后状态／显示恢复；原生隔离测试另检查可见网格立即跟随车体旋转／平移而物理根和速度未被改写。最终5份报告均0测试警告／失败／未运行，不等于启动或编译零警告，也不是可见画面的验收。

原始构建／报告／资产清单位于忽略的`Saved/BallRelativePresentation/20261030_160330`；共享JSON保留路径、SHA-256、最终报告名和失败记录，新电脑没有Saved原件不代表已重新执行测试。首轮编译修正了引擎类型命名空间／头文件／复制宏参数；首次真实联机捕获还触发了基类`OnPreSimulate_Internal`断言，已补齐UE要求的空前置回调，采样仍只在PostSolve执行。该失败Host／Client日志保留，崩溃轮没有最终报告，未冒充通过。回调修复后重新构建、重跑107项及上述两组双端，之后没有再改C++。专用测试进程均已退出，未自动重开Editor。

**待团队验收：** 重新启动同版本四台真实电脑，按第4节对比转弯、速度变化、吸入／滚动、立即发射、脱球／重捕和持有者退出。首次球权确认及真实发射的网络等待仍存在；本步针对持球期间车球姿态来自不同时间线的问题。若实测仍有发射迟滞，再按日志讨论窄范围的发射视觉预测；不以本轮通过自动关闭F4、VP复杂组合或四机长时验收。
