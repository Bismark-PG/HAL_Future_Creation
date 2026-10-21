# 车辆反弹最小修正与共享DA配置（v0.1）

日期：2026-10-21；施工基点`main/699fc0c4453ce452ec3692283ebfc133b72535b3`；本机UE5.8.3-58210709。

本步实现车辆接触材质统一入口和普通静态支撑补量过滤，承接[反弹诊断](MVPB_Vehicle_Stability_Diagnostic_CN_v0.1.md)。**代码需要团队按第3节创建材质／DA并启用项目设置后，才会作用于正式关卡。** 未改保存资产、原驾驶参数、代理尺寸或墙高；姿态驾驶门控和长按自救是下一小步，尚未实现。VP-1结束与10-F-4后置保持。

## 1. 具体规则与职责

| 接触／攻击 | 本步行为 |
|---|---|
| 普通静态地面／坡面 | PhysicalContact，来源组件Mobility=Static、ObjectType=WorldStatic、无物理模拟，且来源不是球、车辆或带ImpactSource的特殊来源；几何法线与法向冲量方向都在世界向上45°以内时，规则追加速度为0 |
| 水平墙面／超过支撑坡度的面 | 保留原`PhysicalSupplement`上限与接触回合，不套用球的抬升／翻滚参数 |
| 车撞、Free球、移动物体、带ImpactSource的机关 | 保留原接触补量路径；带特殊来源的静态组件也不当作普通地板 |
| 球／机关ExplicitTier攻击 | 保留原水平追加、最低向上速度、翻滚目标，不经过支撑过滤 |
| 档位、抗性／免疫、脱球、伤害 | 原流程保持；落地有效Medium／Heavy仍可脱球，不能因为追加为0而提前返回 |

这只消除额外规则速度，不抵消Chaos自然冲量，不清空Z速度或角速度，不自动扶正。45°是**本步补量过滤**的工程默认值，不代表未来轮胎可驾驶坡度或翻车角度。普通地板仍可产生冲击档位与事件；后续自救打断必须识别外部有效攻击来源，不能把地面支撑事件直接当作打断。

[VehicleStabilityRules](../../Source/HAL_Future_Creation/VehicleStabilityRules.h)提供原生Data Asset、项目设置与每World快照。车辆Receiver在PreInitialize的配置检查中应用共享材质，玩家与物理测试车均接入；球没有Receiver，不替换球的材质。运行共享材质优先于VehicleDefinition的Body材质覆盖；质量、惯性、尺度与阻尼仍来自VehicleDefinition。

引擎`SetPhysMaterialOverride`会重算质量属性，因此首轮保持PM的密度／RaiseMassToPower及VehicleDefinition原质量覆盖。自动化比较相同几何的共享材质开／关质量和惯性；后续若修改PM密度或取消车型质量覆盖，应重新核对实际质量／惯性，不把材质当作普通Buff入口。

每局首次车辆初始化时，读取并校验DA，将规则复制成纯值，并复制一份运行物理材质；同World所有车辆共用此快照。运行中编辑源DA／PM不会改变已生成的快照，新World重新读取。物理线程沿用原动作执行，不读取DA或ASC。

项目设置默认`Use Shared Rules=false`，用于显式迁移；关闭时保持原接触规则／材质。启用后缺失或无效DA会阻止车辆配置，不回退旧规则。DA／PM路径、支撑配置及接触、质量／睡眠／软接触相关材质数值生成CRC签名，经Receiver复制；客户端不一致时记录Error并停用该车物理，须统一配置后重新连接。签名用于配置诊断，不是安全协议，也不覆盖工程全部物理设置或所有车型DA。

## 2. 本步新增的调试输出

开启`hal.CombatLog 1`，原`HAL Combat`行增加：

- `SupportSuppressed=1`：本次适用普通静态支撑过滤。
- `RuleDV`：接受并排队的规则速度增量，单位cm/s，不含自然Chaos冲量。
- `RuleUp`：显式攻击的最低向上速度目标，普通物理接触为0。

这些数值是动作命令，不等于物理步已经执行后的速度。实际执行继续看`hal.ImpactPhysicsLog 1`的`BeforeV/BeforeW/QueuedDV/QueuedDW`。共享规则在新局首个车辆初始化时输出`VehicleStabilityRules Snapshot=... Restitution=... SupportSlope=... SuppressSupport=...`。不同车辆的Receiver会复用同World快照。

## 3. 团队手工配置流程

### 3.1 创建车辆物理材质

1. 完整编译本轮C++后打开Editor，停止PIE／独立游戏。Content Browser新建目录，例如`/Game/Data/MVP_B/Physics`。
2. 右键新建**Physics → Physical Material**（也可搜索Physical Material）；选择普通`PhysicalMaterial`类。命名建议`PM_Vehicle_LowBounce_v1`。
3. 打开材质，首轮填：

| 字段 | 首轮值 | 目的 |
|---|---:|---|
| Friction | 0.7 | 保留本次保存基线的动摩擦 |
| Static Friction | 0 | 保留基线，不顺带改静摩擦 |
| Restitution | **0** | 零回弹对照起点 |
| Override Restitution Combine Mode | **勾选** | 覆盖项目Average合并 |
| Restitution Combine Mode | **Min** | 与当前默认地面／墙材质组合时取低回弹 |
| Override Friction Combine Mode | 不勾选 | 继续项目原摩擦合并 |
| Density、Sleep、Raise Mass To Power等 | 保留引擎默认 | 本步先隔离接触回弹，不同时重调睡眠或质量 |

4. 保存。不要修改Engine默认物理材质，不要把这个材质赋给球、地面或墙。

物理合并发生在**接触双方之间**。球仍使用自己的材质，但球撞车辆时也会受车辆Min策略影响；这是需要实机复测的接触手感变化，不能声称球车自然反弹完全相同。球撞场地的材质配置没有改变。若另一方显式使用更高优先级的Multiply／Max合并，不能保证Min最终获选，应核对双方配置；当前已采样的测试地面／墙为默认Average。

### 3.2 创建共享车辆规则DA

1. Content Browser右键 **Miscellaneous → Data Asset**，父类搜索并选择 **VehicleStabilityRules**。这是Data Asset实例，不需要创建蓝图子类。
2. 建议命名`DA_Vehicle_Stability_Test_v1`，放在`/Game/Data/MVP_B`。
3. 填写并保存：

| 字段 | 值 |
|---|---|
| Ready For Use | 勾选 |
| Vehicle Physical Material | 上一步的`PM_Vehicle_LowBounce_v1` |
| Suppress Support Supplement | 勾选 |
| Maximum Support Slope Degrees | **45**（单位°，合法0～80） |

本步DA只有接触组。已确认的侧翻1秒、150cm/s、60°/s、长按1.5秒、执行2秒、冷却3秒，会在下一步连同自救实现加入**同一个共享DA**；当前没有这些可用字段或恢复输入，不要为此自行堆Event Graph。

### 3.3 项目级引用与启用

1. **Edit → Project Settings**，先清空搜索和筛选，在分类侧栏进入 **Game → Vehicle Stability**，再展开`Shared Rules`。若分类侧栏被收起，先展开分类导航。UE5.8搜索框可以限定在当前已选设置页内；直接搜索页名不保证跳转到该页。进入目标页后，可搜索`Use Shared Rules`定位字段。
2. 勾选 **Use Shared Rules**，将 **Shared Rules**指向`DA_Vehicle_Stability_Test_v1`。确认不是原Combat DA，也不是车型VehicleDefinition或表现DA。
3. 保存项目设置，核对`Config/DefaultGame.ini`中的`[/Script/HAL_Future_Creation.VehicleStabilitySettings]`：`bUseSharedRules=True`和`SharedRules`应指向所建资产。由团队提交这个配置文件以及新建的PM／DA；不只提交C++或文档。
4. 保留现有玩家BP→VehicleDefinition→Presentation／ABP引用，不需要改玩家Event Graph或逐车设置PM。
5. 完全结束旧局后重新进入测试图。独立Host／客户端均重新启动，使用同一代码、DA、PM和DefaultGame.ini；只改主机本地资产会触发配置不一致。
6. 检查双方日志：快照启用、Restitution=0、SupportSlope=45、SuppressSupport=1，且没有`Invalid VehicleStabilityRules`／`VehicleStabilityRules mismatch`。错误时先修复并保存引用，再新开局。

找不到设置页时，先使用上述分类入口并清空筛选，再核对Editor是否加载本工程的新DLL。新增原生设置类型时，旧Editor／Live Coding会话可能需要完整重启；只有本机尚未编译当前源码或编译失败时，才需要关闭Editor后构建`HAL_Future_CreationEditor / Development / Win64`。上一轮已经完整编译，不应仅因为搜索页名无结果就认定未编译。

回退对照：停止本局，取消`Use Shared Rules`，保存并重新开局，恢复VehicleDefinition原材质和原支撑追加；或保持共享规则启用、只取消DA的`Suppress Support Supplement`，比较同材质下的规则追加。两种对照必须区分。无需删除原车型DA中的配置。

## 4. 团队验收步骤

使用同车型、同初速／高度、同场地分别录像／留Host和Client日志，比较旧配置与新配置：

1. 平地低速／高速、倒车、手刹、斜坡接触；碰撞后松开输入，观察自然衰减。
2. 正撞／斜撞短墙、墙角、连续擦墙；记录姿态变化和墙面追加。现有约50cm露出地面的墙与269cm厚代理仍可能造成翘头／翻滚，本步没有修改几何。
3. 水平下落、倾斜落地、球Heavy击飞后着地；普通地面补量应为0，允许自然摇摆和翻车。Heavy起飞的RuleUp／翻滚仍应存在。
4. Free球争夺、自动捕球／带球／发射／重捕锁；有效Medium以上落地仍脱球，冲击免疫／降档后的脱球门槛与旧规则一致。
5. 球分别命中主机车和客户端车；技能免疫仅阻止规则冲击，伤害按原独立分支。已有弹簧拳模块做一次显式命中回归即可，无需修改其代码。
6. 两端分别驾驶／观察，在30／60／120屏幕FPS及基本延迟下核对效果；本步自动化的固定60Hz物理采样不替代这些真人画面测试。完整丢包／抖动／四机仍留MVP-B收尾。

侧翻／倒扣仍可驾驶的问题将在下一步解决；这不是本步反弹修正失败或自动自救已经存在的证据。若低回弹后仍不合理摇摆，依据录像与接触日志再定有限落地辅助，不能直接加全局大阻尼或锁定Pitch/Roll。

## 5. 验证记录

构建／报告／完整哈希见[验证JSON](MVPB_Vehicle_Stability_MinimalFix_Verification_20261021.json)。最终Editor／Game Development均成功；普通回归**52/52、0测试警告／0失败**；真实Solver诊断**1/1**，16组各720步。主机／客户端最终各**1/1**，签名均4097445932、两个车辆根使用同局零回弹材质且模拟有效；客户端实际重模拟55次。主机单向注入80ms，非测量RTT；此联机夹具检查配置复制／物理／输入，不包含真人球车接触或完整自救／攻击组合。

| 受控工况 | 修正前 | 支撑过滤后 |
|---|---:|---:|
| 零／Min重翻，累计规则追加cm/s | 129.517 | **0** |
| 同工况最大触地后间隙cm | 6.503 | **约0（0.000043）** |
| 同工况严格低速稳定s | 8.350 | **9.633** |
| 500cm水平落下，保存材质→零／Min后触地间隙cm | 28.213 | **0** |
| 零／Min短墙，累计墙面规则追加cm/s | 115.249 | **115.249** |

重翻稳定时间更长但最终落姿可能不同；不得把消除弹跳写成已经更快正立。零回弹／支撑过滤后真实接地摇摆仍需后续姿态／辅助设计。Solver对照调用生产预算／过滤函数和真实物理动作，接触回合在夹具建模；完整Authority结算／抗性／脱球由普通测试补充，不能把它单独冒充整个游戏管线。

361份uasset／umap施工前后SHA256全同，保存Config无修改。首次普通回归51通过／1失败：新夹具未推进Solver，却立即把排队动作当作已执行速度读取；改为核验接受动作诊断，并由独立真实Solver测试验证执行。另一次编译将UE物理材质枚举误写为`AbsoluteThickness`，核对本机头文件实际为`AbsoluteThickess`后修正并重编。失败报告／日志保留，不以退出码0代替通过。编译仍有既有MSVC14.51非推荐和引擎弃用警告，不宣称零编译警告。

保存材质／DA与正式项目设置由团队按第3节创建；本步专用联机使用固定命名的临时内存DA／PM，不保存资产，不代表团队配置或真人手感验收。
