# 普通静止支撑分类修正（v0.1）

日期：2026-10-21；代码比较基点 `main/c747e4be0e39a0d65a9b266fbaa3ba74cb5078e5`；引擎 UE5.8.3-58210709。用户授权修正“允许移动但实际静止的普通地面”，并要求保留球、车辆及特殊机关的独立处理。车轮配置由团队修复，用户反馈表现恢复正常，本轮未修改表现资产。

本轮代码及构建、自动化检查已完成；侧面／车尾着地的真人手感仍待复测。完整证据、日志哈希和资产清单见[验证摘要](MVPB_Vehicle_Support_Classification_Verification_20261021.json)。此前[诊断](MVPB_Vehicle_Landing_And_WheelGhosting_Diagnosis_CN_v0.1.md)与各次验证JSON保留其历史结论，不以新结果覆盖。

## 1. 修正原因和行为

`Test` 与 `MVPB_NetTest` 保存的普通 Floor 都是 `Mobility=Movable`、`ObjectType=WorldStatic`、`Simulate Physics=false`。旧实现要求 Mobility 必须 Static，导致静止地面漏过支撑过滤。Mobility 表示允许移动，不能单独用来判断接触时是否正在运动。

现在先识别来源，再沿用共享DA的支撑坡度检查：

| 来源 | 分类与处理 |
|---|---|
| 球、含 ImpactReceiver 的车辆、含 ImpactSource 的特殊来源 | `SpecializedSource`，保留独立接触规则；静止或法线朝上也不转成普通支撑 |
| 正在物理模拟的组件 | `SimulatedSource`，保留原接触补量 |
| 非 WorldStatic | `NonWorldStatic`，不自动扩展为普通地板 |
| 组件速度、接触点物理速度或物理角速度非零 | `MovingSource`，保留原补量；覆盖组件速度为零但刚体正在平移／旋转的情况 |
| 组件位置／旋转与当前物理姿态尚未一致 | `PendingMotion`，保留原补量，避免将待同步的运动目标当成静止来源 |
| WorldStatic、非模拟、普通来源，速度近零且位置／旋转一致 | `StationaryWorld`；Static 和静止 Movable 均可进入支撑判定 |
| 来源、物理体或数值无效 | 对应无效原因，不启用支撑过滤 |

“近零”使用引擎 `KINDA_SMALL_NUMBER` 数值容差，不新增可调驾驶阈值。位置／旋转用于比较物理姿态，组件缩放不参与这一运动判断；当前地面组件缩放与刚体姿态缩放并不相同。

只有 `StationaryWorld` 且**几何法线与法向冲量方向**都满足共享DA坡度限制，才清除该次普通接触的规则追加。当前共享DA仍为45°。水平墙面及超过该坡度的面保留原有界追加。

过滤只将规则补量置零，继续完成有效档位、接触回合、抗性／免疫、伤害和持球脱落结算；有效 Medium／Heavy 的落地仍可脱球。ExplicitTier 的球／机关攻击不经过这项分类，原水平追加、最低向上速度和翻滚参数继续有效。没有修改弹簧拳模块。

本轮没有清零自然速度、抵消 Chaos 冲量或自动扶正。分类是当前接触的保守判断，不提供平台运动历史框架；有独立攻击语义的机关仍应使用 ImpactSource／ExplicitTier，不能只靠组件 Mobility 表达机关规则。

## 2. 代码职责与日志

- `VehicleStabilityRules` 提供仅在游戏线程调用的来源分类；读取组件与现有物理体状态，不新增 Tick。
- `HALImpactReceiverComponent` 在原 PhysicalContact 预算路径调用分类及既有支撑策略，之后继续原结算与物理动作排队。
- `FHALCombatHitResult` 增加本地诊断字段；没有新增复制字段或改变网络物理历史格式。
- `CombatResolver` 在原可关闭的 `hal.CombatLog` 行补充来源与测量信息。

服务器控制台输入：

```text
hal.CombatLog 1
hal.ImpactPhysicsLog 1
```

前者检查接受／排队命令，后者辅助核对实际物理动作执行。主要新增字段如下：

| 字段 | 含义 |
|---|---|
| ContactActor／ContactBody | 接触来源Actor及组件 |
| Mobility／ObjectType | 来源的移动权限和碰撞对象类型；当前Floor分别为2和0 |
| SupportSource | 来源分类原因；它本身不代表坡度判定也通过 |
| Normal／NormalImpulse | 几何法线与原生碰撞冲量 |
| NaturalDV／RelativeAfter | 原补量预算使用的自然法向速度变化与碰撞后相对速度估算 |
| SupportSuppressed／RuleDV／RuleUp | 是否清除了支撑追加、接受的追加速度与最低向上速度 |

这些是结算诊断及动作命令，不能直接等同于画面已积分速度。结束排查后将两个CVar恢复为0。

## 3. 本轮验证及边界

| 检查 | 结果 |
|---|---|
| Win64 Development Editor／Game | 均构建成功；Game在中断恢复后完成 |
| 普通回归 | **53/53**，0测试错误／警告；新增 SavedMapFloors |
| 真实 Chaos Solver 诊断 | **1/1**；Movable、非模拟 WorldStatic 地面，16组／11520步，调用生产分类函数 |
| 独立 Listen Host／Client | 各 **1/1**，0测试错误／警告；实际 MVPB_NetTest，客户端重模拟56次 |
| 资产保护 | 本轮开始后的363份 uasset／umap SHA256全部相同；保留用户已修改的车轮表现DA；Config未改 |

普通回归覆盖 Static／静止 Movable 地面、组件运动、刚体平移、绕接触轴旋转、WorldDynamic、模拟物体、墙面、球／车辆／特殊来源、ExplicitTier、冲击免疫，以及过滤后仍脱球和重捕锁。SavedMapFloors 只读加载两图，将保存的地面配置复制到独立临时测试世界后走完整战斗结算，不改加载地图。

真实联机 Host 日志还记录了两辆玩家车直接接触当前地图 `Floor_0.StaticMeshComponent0`：`Mobility=2 ObjectType=0 SupportSource=StationaryWorld SupportSuppressed=1 RuleDV=0`，Heavy／Medium有效档仍保留。例如UTC 05:45:32.875的主机车 Heavy 接触，NaturalDV=350.427，而规则追加为0；05:45:42.817客户端车的服务器权威接触也为0。这补充了人工碰撞请求以外的实际 OnComponentHit 管线证据。

双端夹具使用临时共享规则，签名4097445932，两端一致；主机单向注入80ms，不是实测RTT。团队已保存正式DA／PM的旧日志签名492667503来自不同资产路径，不能要求它与临时夹具签名相同。该检查覆盖基础输入、物理复制与回滚，不代表弹簧拳真人手感、所有攻击组合或F4完整扰动／四机验收。

Movable地面的零／Min重翻诊断，累计支撑规则追加为0，触地后最大间隙约0.000043cm，但严格低速稳定仍约9.63游戏秒。墙面原追加保留。这说明额外向上补量已去除，接地摇摆仍可能持续；该诊断的接触回合在夹具建模，不能单独替代整局游戏结算与真人观察。

首轮普通回归在新地图夹具中误用 `CopyBodyInstancePropertiesFrom` 复制已有物理体，触发 `FromInst->OwnerComponent.Get() == 0` 断言，进程退出1、未生成完整JSON报告。已改为逐项复制公开配置，不复制运行句柄；重编Editor后的最终53/53通过。初次失败日志保留，不能将其写成通过。编译保留既有MSVC14.51非推荐及引擎弃用警告，不宣称零编译警告。

## 4. 团队复测流程

无需新建DA、修改AnimBP或将Floor改成Static。当前 `DA_Vehicle_Stability_Test_v1`、`PM_Vehicle_LowBounce_v1` 和项目级引用继续使用；完全退出旧Editor／游戏后，重新打开当前构建即可。

1. 在 Test 和 MVPB_NetTest 分别测试车顶、左右侧面、车尾着地；先松开驾驶输入，再对照带输入的表现。开启日志，普通Floor支撑应出现 `StationaryWorld`、`SupportSuppressed=1`、`RuleDV=0`。只对符合支撑法线／方向且进入预算分支的有效接触作此判断。
2. 重复弹簧拳击飞与落地：ExplicitTier击飞仍保留向上速度／翻滚；落到普通Floor后不再叠加支撑规则速度。记录仍有摇摆时的接触来源、姿态和录像，区分离地弹跳与接地转动。
3. 检查斜撞墙、墙角、车辆互撞、Free球、发射球与特殊机关；原攻击和独立接触响应应保留。
4. 持球落地有效Medium／Heavy仍应脱球，保留重新捕获锁；检查冲击免疫与伤害免疫继续各自独立。
5. Host／Client分别受击、驾驶与观察，留双方日志。若残留问题只有车轮表现，另按团队已修正配置追踪，不回滚本轮支撑规则。

本轮未执行上述真人画面／手感矩阵，也未新增姿态驾驶门控、长按自救或有限落地辅助。后续按[已确认自救计划](MVPB_Vehicle_Stability_And_SelfRighting_Plan_CN_v0.1.md)扩展同一共享DA；VP-1结束与10-F-4后置状态保持。本轮修改未提交／Push。
