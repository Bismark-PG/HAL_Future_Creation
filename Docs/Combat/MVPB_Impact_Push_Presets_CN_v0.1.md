# MVP-B 冲击水平推送预设：实现与配置（v0.1）

日期：2026-10-26（Asia/Tokyo）。本轮基点`main/bff8753040a50b5e11811453a8692024b9ebb878`，起始工作区干净，UE5.8.3-58210709。用户确认保留三档、支持Heavy＋近推／远推，并授权施工。代码、完整Editor／Game构建及最终普通77项／实际双端通过；团队配置和真人距离／画面待验收。球／弹簧拳旧配置兼容是硬约束，1154保存资产全同；VP-4C和F4不因本步启动。未提交／Push。

## 1. 规则与实现

三档继续管理免疫／降档、向上击飞／翻转、冲击脱球和VP-4B骨骼回弹。来源额外选择`EHALImpactPushPreset`，只调整主动攻击沿水平受力方向的速度变化目标。没有按米强制位移、瞬移或每帧追赶目标点，没有改质量、阻尼、抓地、输入或伤害倍率。

| 预设 | C++默认水平倍率 | 允许配置 |
|---|---|---|
| Standard（标准） | 1.0 | 固定，不提供可编辑倍率；旧资产缺少字段即为此值 |
| Short（近推） | 0.5 | 0～1 |
| Long（远推） | 1.5 | 1～3 |
| ExtraLong（超远推） | 2.0 | Long～3 |

三个可编辑倍率集中放在现有`UCombatRulesDefinition.ImpactPushPresets`，所有主动来源复用。不新增DA类型，不给机关逐一填写冲量。值必须有限且满足表中次序，未知枚举／非法规则不静默回退。Short=0只关闭本事件的水平规则补足，Heavy的向上／翻转／脱球仍按Heavy；已有自然运动不会被抵消。

统一结算为：服务器接受命中→冲击免疫／降档→有效档位→该档HorizontalDeltaSpeed乘预设倍率→减去本次自然水平响应→只追加非负剩余量。免疫或有效None始终不追加本事件的规则击飞／脱球。伤害先按既有GAS独立结算，损坏的冲击预设不让已接受伤害路径依赖冲击成功。

例：假设Heavy水平基值900cm/s，则Short／Standard／Long默认目标450／900／1350cm/s；自然水平响应已有100时分别只追加350／800／1250。Heavy被降为Medium后先取Medium基值，再乘同一个预设倍率，不能继续使用Heavy的目标。结果不是覆盖目标绝对速度，保留受击前速度。

范围限于`ExplicitTier`的主动球／机关／爆炸。普通墙体、静止地面、车车和自由球的`PhysicalContact`保持Standard；仍使用原接触episode、支撑过滤和恢复系数能量预算，非Standard的普通接触请求会拒绝冲击。公共倍率不用于放大墙角反弹。

## 2. 来源、快照与网络

- 球：`Ball Definition → Gameplay → Impact → Push Preset`；运行时配置在发射时进入原AttackSnapshot，命中读取快照，不读取活配置。
- 弹簧拳：原BP的Class Defaults → Spring Punch → Combat → `Impact Push Preset`；与Impact Tier分开。每次进入Extending捕获，当前拳不随中途修改变化，下一拳重新捕获。
- 炸弹：原Bomb Config的`Impact Push Preset`；爆炸Actor初始化时捕获到原CombatAction，范围内目标共享该次预设，不新增拾取／技能行为。

这些来源默认Standard，现有Damage／Tier／GE／曲线／网格／场景引用不重填。不要求保存旧资产来做迁移，也没有改变已有Version枚举。Legacy来源仅支持Standard，新预设明确用于已迁移的TieredV1。

服务器在ImpactReceiver解析完有效档位后计算实际物理动作；原`FVehicleImpactAction`携带已解析DeltaVelocity、向上／角速度并进入现有Chaos／网络物理重放。未修改动作协议，不让客户端用当前GAS属性／预设重算历史动作。新增只读LastImpact诊断`PushPreset`和`DesiredHorizontalDeltaSpeed`随原Summary复制；VP-4B仍只消费有效档位／方向，其骨骼幅度不随推送倍率自动增加。

## 3. Unreal Editor配置步骤

### 3.1 保留已有球／弹簧拳行为

完整编译后重启Editor，原资产新字段默认Standard，保持即可；原球伤害、Impact Tier、发射速度以及弹簧拳伤害／档位／伸缩曲线均不需重新填写。原表现DA与AnimBP不需增加接线。

### 3.2 集中调整倍率

1. 在 **Edit → Project Settings → Game → Vehicle Knockback** 查看当前 **Combat Rules** 引用；项目通常为`DA_Combat_TestBaseline_v1`，以实际绑定为准。
2. 打开该DA，搜索或展开 **Impact Push Presets**，查看Short Multiplier／Long Multiplier／Extra Long Multiplier，默认0.5／1.5／2.0。原Ball Impacts和Collision Impacts整组保持，不为一个远推机关改全局Heavy基值。
3. 首次可直接用默认倍率。调整后保存，校验Ready For Use及数据验证错误；Short≤1≤Long≤ExtraLong≤3。
4. 公共规则沿用原初始化缓存。本轮没有新增热重载按钮，修改共享DA后保存并重启Editor，再重开两端游戏，确保重新加载；两端须同一代码／引擎补丁／保存配置。

### 3.3 Heavy＋近推／Heavy＋远推

**球**：打开实际Ball Definition，Gameplay → Impact，保留Version=TieredV1、Tier=Heavy，将Push Preset设为Short或Long，Save；重新生成并发射球。已经在飞的球保留发射时快照，不用它检查新配置。

**弹簧拳**：打开原弹簧拳BP，选Class Defaults，搜索`Impact`；Spring Punch → Combat中把Impact Tier设为Heavy，Impact Push Preset选Short或Long，Compile／Save。若要场上两种拳同时比较，团队可创建两个轻量子BP分别设置Class Defaults；代理不修改BP或地图。不要改Punch Distance来替代击退距离：该参数控制拳本体伸出长度。

**炸弹**：原炸弹BP的Bomb Config中保留Impact Tier=Heavy，Impact Push Preset选Short或Long；其他倒计时／范围／伤害保持。新生成的炸弹用于测试。

## 4. 标定与手工检查

预设调整的是水平速度变化，不承诺固定米数。地面抓地、刹车／手刹、朝向、初速度、腾空时间、斜坡、墙体及其他碰撞都会影响最终位置；同为Heavy时近推仍有Heavy击飞高度。不同预设跨档位时距离可重叠，档位不是距离的唯一排序。

先在相同平地、车型、静止、无输入、无障碍条件比较Standard／Short／Long。分别记录沿受力方向的水平位移投影，区分首次落地点和落地后停稳位置；可用间隔已知的场景标尺，不能只按屏幕像素。之后补前后／左右／斜向、迎面／顺向行驶、刹车／手刹、墙角、腾空、连续命中与主机／客户端互换。标准条件标定成“约N米”只作为参考，不做实战固定距离承诺。

检查Heavy近推与远推伤害、有效档位、脱球阈值、向上／翻转和骨骼受击幅度一致；降档后两者都先降档再选基础响应；冲击免疫无规则追加、伤害免疫仍可受推。近推面对已有大自然冲量不强行拉回／减速。

调试可开`hal.ImpactPhysicsLog 1`看原物理动作的Event／Frame／QueuedDV／QueuedDW。LastImpact只读摘要可看Preset、EffectiveTier和DesiredHorizontalDeltaSpeed；Desired值是扣除自然响应前的目标，不是最终速度或实际距离。日志默认关闭，检查完设回0。

## 5. 本轮验证

当前证据见[本步验证摘要](MVPB_Impact_Push_Presets_Verification_20261026.json)。实际结果：

| 检查 | 本轮结果 |
|---|---|
| Game Target | 首次修复后普通构建成功32.45秒；同步最终夹具后强制Unity完整构建成功23.56秒 |
| 提交后Unity兼容性 | `-DisableAdaptiveUnity`保留Unity合并、关闭按工作区拆分；早期37.77秒与最终23.56秒均成功，私有函数重名修复不依赖未提交文件被拆出 |
| Editor完整构建 | 先NoLink编译103.47秒；团队关闭Editor后实际链接成功2.98秒；最终一次性夹具版本编译／链接成功6.13秒 |
| 普通自动化 | 最终二进制`HAL.FutureCreation` **77/77成功**，0测试警告／失败／未运行；前次77项亦成功，分别保留报告 |
| 真实双端 | `HAL.NetworkSmoke.ImpactPushPresets`最终Host／Client各 **1/1成功**，均0测试警告／失败；NetTest、60 FPS上限、端口7834、主机出站`PktLag=80`日志确认 |
| 物理与复制 | 两端均为Heavy＋Short／Long，摘要目标450／1350cm/s；Host实际X速度450／1350，Client在完成观察条件前采样最大315.074／1350（近推车为插值中的观察对象）；两车HP100不变，客户端36次重模拟 |
| 保存资产保护 | 1154份uasset／umap逐份SHA-256全同，无新增／删除；Config／uproject／Build.cs保持 |

首轮Game构建暴露了此前已提交VP-4B两份回弹源码中的匿名命名空间`Newer`／`SmoothUnit`重名；未提交时Adaptive Unity把它们分开，提交后合并才触发。已将受击侧私有函数改为独有名称，行为保持；修复后普通与强制Unity构建通过。首次失败日志独立保留，不以关闭Unity规避。现有MSVC优选版本提示／C4996警告保留。

普通回归包括新增两项预设配置／实际结算测试，以及原球命中、弹簧拳生命周期、炸弹、GAS免疫、物理Solver／重放、支撑和VP-4B表现等；新增12种三档×预设组合、共享DA自定义倍率、先降档后缩放、自然响应扣除／不反向拉回、零水平Heavy、None／损坏枚举／普通接触隔离，并验证球发射／拳开始时捕获预设、伤害独立和短推仍按有效档位脱球。

前两轮双端均0/1超时，失败报告／日志保留。第二轮诊断已收到正确档位／预设／目标，HP重置为100，但物理V始终0。夹具每帧调用SetLinearDamping，即使值为0仍使Chaos的DynamicMisc数据变脏；改为准备时仅设置一次后，在保留原速度／HP断言的情况下通过。另在服务器隔离两车时清除自动化启动前场景弹簧拳的扣血，仅作用运行夹具。未因此修改生产物理Handler、施力规则或网络动作协议。

双端是NullRHI、临时空中隔离、关闭重力／阻尼的原生夹具，真实统一Resolver／Receiver／网络物理动作和双方速度被验证；它不等于正常驾驶画面或真人距离标定。Client记录的是完成观察条件前的最大采样，不是近推车最终稳定速度；80ms是配置的主机出站延迟，不是实测RTT，未注入丢包／抖动。旧VP-4B的75项／双端没有冒充本步证据。Saved日志／报告不提交，摘要保留路径、哈希、失败和最终计数；所有专用UE进程已退出。现有MSVC／C4996及引擎启动警告仍在，“0测试警告”仅指自动化报告。

下一步团队按第3／4节配置并做画面／距离／手感初测，Standard无需改动。VP-4C另行进入，F4继续留MVP-B末尾。本轮源码／文档未提交／Push。

## 6. 跨设备接收状态（2026-10-27）

水平预设已随`a09a3c4`提交并接收至当前`main/914cc53`，本文件开头／第5节“未提交”保留原施工时点。此后只有3份轮胎打滑FX／材质新增，Source／Config／uproject未再变化。按VP-4A全量、VP-4B及本步清单逐层核对，当前182份源码／配置无内容差异（178份原字节、4份仅LF／CRLF差异），详见[接续快照](../Presentation/MVPB_Project_Handoff_Review_20261027.json)。

本机实际UE5.8.2，原77项／双端／强制Unity构建为另一设备UE5.8.3证据，本轮不重编／运行或导出资产值，Saved报告未随Git到达。代码完成状态保持；尚未收到新的团队Heavy＋Short／Long画面／距离／手感验收反馈，不把档位倍率当作固定米数或把本次接收当作真人测试。下一步仍为团队初测反馈，再进入VP-4C。
