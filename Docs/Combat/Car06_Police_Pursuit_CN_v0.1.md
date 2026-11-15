# Car06 警车主技能：追捕（v0.1）

2026-11-15，接续 `main/f169aa9`，UE5.8.3-58210709。用户已接受下列玩法、首测值及全部边界，并授权技能与对应表现开发。当前代码、原生资源和工程专项已完成，**团队 GA／Definition 接线及真人验收待执行**；具体证据与限制见末节。未直接修改 Blueprint，保留开工已有 GameMode BP。

- 发动警笛，固定持续时间，追捕范围跟随车辆；正常驾驶，不额外增强车速，自身不作为目标。
- 每目标独立通缉值；有效范围内按秒累计，离开有效范围按秒衰减至零，不立即清空；重入继续累计。
- 单次技能结束清理尚未完成的通缉记录，下次从零开始；已经施加的 Stun 正常计时，不因警笛结束或施放者退场被提前解除。
- 达到阈值触发逮捕，清空该目标通缉值；每目标每次警笛最多逮捕一次，不在原地重复累计／连续控制。
- **警车受到眩晕或执行自救时，追捕仍然继续，范围内目标仍累计、范围外仍衰减；警笛时间正常推进。不得实现为暂停追捕或全体衰减。** 共用 Stun 对警车自身驾驶的限制照常，追捕许可单独判断，不与驾驶许可绑在一起。
- 淘汰、换车、离场和比赛结束清理施放者追捕及记录。目标淘汰、退出／换 Avatar 时清理对应记录，不传递给新车。
- 判定为水平圆形范围＋高度差限制＋普通实体墙遮挡；边界少量缓冲避免提示闪烁。目标腾空但仍满足高度／遮挡条件时继续追捕。
- 多警车记录独立、不合并通缉值；目标已经 Stun 时暂停累计。逮捕后有跨警车共享的短暂再次逮捕保护，只限制 Car06 的逮捕，不影响电墙。此处是“被追捕目标已 Stun”的规则，与上面的“施放警车已 Stun 仍追捕”分开。
- 逮捕只施加共用 `ApplyStun`，不直接扣血，不附加电墙回弹或电击外观。共用 Stun 阻止驾驶／技能／捕球／发射／自救并解除持球，不冻结刚体；现有车型的专属 Stun 响应继续沿用。
- 伤害抵抗、冲击抵抗不自动抵抗逮捕；未来控制抵抗／眩晕免疫走独立接口。
- 目标可见通缉进度，区分累计／衰减和接近逮捕，警车范围可辨认。UI 沿用原生回退＋Widget Blueprint 可替换。

实现边界：GA 负责发动／冷却，C++ 服务器 Actor 负责范围和每目标记录，目标组件管理跨警车保护及标识，GAS 施加共用 Stun／逮捕／保护状态；不新增 GA Tick。全部参数及表现引用放在一份 Car06 技能 DA。

2026-11-15：用户接受 Car05 为暂定初版后，确认 Car06 参数与两个细节，并补充红蓝警灯、淡红扫描范围、血条上方六星从左往右填充和逮捕标识四层表现。程序与表现均按该授权落地。

## 已确认首测方案（2026-11-15）

下列参数和两个细节已经用户全部批准。尤其“施放后警车眩晕／执行自救仍追捕”保持。

### 首测参数

全部位于同一份 Car06 技能 DA，均可后续调试，不作为最终平衡结论。

| 参数 | 建议首测值 |
| --- | --- |
| 警笛持续时间 | 8 秒 |
| 追捕半径 | 900 cm（9 m） |
| 最大目标高度差 | 300 cm（3 m） |
| 通缉阈值 | 100 |
| 范围内累计速率 | 25／秒，即零值连续追捕 4 秒达到阈值 |
| 范围外衰减速率 | 20／秒 |
| 逮捕 Stun | 1.5 秒 |
| 主技能冷却 | 18 秒，自成功发动起算；活跃中不可重复发动 |
| 逮捕保护 | 本次逮捕后的 Stun 结束后额外 2 秒，仅阻止 Car06 再逮捕 |

### 发动与共享保护边界

1. **发动条件**：允许腾空／翻倒时新发动，因为警笛不需要地面生成点；眩晕或自救执行中仍不得新发动，沿用 ASC 的现有入口限制。已发动之后无论眩晕／自救均继续。施放者存活、Avatar／流程许可和冷却检查照常。
2. **逮捕保护**：由目标跨来源共享，覆盖逮捕 Stun 期间及恢复后的额外 2 秒；其他警车对该目标不累计／不逮捕，未完成的通缉值按离开范围速率衰减，避免保护一结束立刻满条再控。保护起算按共用 Stun 真正结束，而非固定从警笛开始计；不影响电墙或其他普通 Stun。目标仅因非逮捕来源 Stun 时，暂停累计，范围外仍衰减。

## 表现与资源

| 资源路径 | 用途 |
| --- | --- |
| `/Game/Data/Abilities/DA_Ability_Car06_PolicePursuit` | 唯一技能 DA，类型 **HALPolicePursuitConfig**，包含 Timing、Rules、全部 FX／UI／声音调参及引用 |
| `/Game/Assets/FX/PolicePursuit/M_Pursuit_Range` | 淡红半透明 Unlit 圆盘，边界环＋旋转扫描及扩散纹理 |
| `/Game/Assets/FX/PolicePursuit/M_Pursuit_Signal` | 警灯及手铐的 Unlit 发光材质 |
| `/Game/Assets/FX/PolicePursuit/SM_Pursuit_Range` | 半径100 cm的基准圆盘，无碰撞 |
| `/Game/Assets/FX/PolicePursuit/SM_Pursuit_Handcuffs` | 双手铐状态标识，无碰撞 |
| `/Game/Assets/FX/PolicePursuit/SW_Pursuit_Siren` | 本轮原创双音变频、2秒循环警笛，可替换为其他 SoundBase |
| `Content/Assets/FX/PolicePursuit/Source/Pursuit_Siren.wav` | 可随 Git 同步的原始 WAV；声音导入来源已指向此文件，不依赖本机 Saved |

这是轻量原生持续网格／Point Light／Audio Component 配方，不依赖 Niagara 执行玩法。红蓝球形发光灯＋无阴影点光按同一服务器阶段交替，范围和标识也由原生组件驱动；未来可换材质或 Mesh，不允许其反向决定通缉／逮捕。默认警灯球 Mesh 引用 Engine BasicShapes Sphere。

星级不是贴图文字：原生 Slate 绘制六个空心／实心五角星，**从左往右连续填充**，每颗对应阈值的1/6，离开后同方向退回。存在通缉值才显示；多个警车分别计数，UI 显示最高的归一化进度，绝不合并数值。同进度时按来源 Actor 名称确定显示样式。逮捕成功后本次通缉清零、星星隐藏，显示手铐及 ARRESTED 提示；状态持续至真正的 Stun 结束，保护尾段不继续显示逮捕标识。

## Editor 接线：由团队手动执行

1. 完整重开本轮编译的 Editor。打开已有 **DA_Ability_Car06_PolicePursuit**，确认类型 HALPolicePursuitConfig，以及 FX → Assets 中 Range Mesh／Material、Beacon Mesh、Signal Material、Arrest Mesh、Siren Sound 引用完整。首版资源已生成，不需重新制作。
2. 在 `/Game/Blueprints/GA` 新建 Blueprint Class → All Classes → **HALPolicePursuitAbility**，命名 **GA_Car06_PolicePursuit**。打开 **Class Defaults → Police Pursuit → Skill Config**，指定上述 DA，然后 Compile／Save。Event Graph 保持为空，不需新增 GE、Tick、Overlap 或伤害节点。
3. 打开已有 `/Game/Data/MVP_B/DA_Vehicle_Car_06`，确认 **Abilities → Version = Vehicle Definition V1**，把唯一 **Input Tag = Input.Ability.Primary** Grant 的 Ability 改成此 GA，Level=1。没有该条目才新增，不要再叠加另一条相同输入。沿用已配置好的 Combat／Impact；正式共用配置应为 Attack Snapshot V1／Tiered V1。
4. 继续使用 `/Game/Blueprints/Vehicles/BP_VehiclePawn_Car_06` 及该 Definition。所有车辆的原生基类已自动添加 **Pursuit Target** 组件以接收状态，不需要在 BP 中重复添加。核对房间 `DA_VehicleCatalog_Default` 的 Car06 条目指向现有车辆 BP；已登记时保留原条目。
5. 选 Car06，在允许技能的比赛／开发流程中按原主技能键。联机两端都更新代码和资源，完整重新启动。没有新增 Input Action，暂无需修改 Car Presentation DA。

本轮没有代团队创建 GA Blueprint 或修改 Car06 的 Definition／车型 BP，遵守 AGENTS.md 的 Blueprint 手动编辑边界。

## 警灯挂点与调参

所有项目位于同一技能 DA，运行时每次释放捕获快照，修改后重新释放技能。

- **Timing → Cooldown Seconds**；**Pursuit → Rules** 中为已确认时长、Radius、高度差、通缉阈值／速率、Stun 和保护。距离／高度比较使用车辆物理根中心。**Exit Buffer=25 cm** 是已进入目标的退出缓冲；新进入仍严格使用 Radius。**Scan Interval=0.05 s** 为服务器扫描间隔，不是技能攻击 CD。
- **Presentation → FX → Roof Lights**：默认 **Automatic Roof=true**，优先使用已注册视觉 Body 的上方，未就绪时用物理体顶部回退。位置跟随车辆本身的上／左右方向，避免模型导入轴将两灯上下堆叠；默认 Roof Lift=8 cm、Light Spacing=35 cm、Beacon Radius=9 cm。Flash Half Period=0.22 s 表示每侧亮起的半周期。
- 需要精确安装在原模型警灯上时，关闭 **Automatic Roof**。**Roof Mount → Part Id** 保持真实注册的表现部分（默认 Body），**Socket Or Bone** 填团队在模型上实际创建的挂点名，**Offset** 调局部位置／旋转。Socket 优先于骨骼；需要缺失挂点时回退到 Mesh 原点才勾选 **Fallback To Mesh Origin**。精确模式解析失败会跳过警灯，不影响追捕、范围或声音。
- **Colors → Red／Blue** 控警灯和标识配色；**Light Intensity=1800 lm、Light Radius=180 cm、Signal Glow=2** 控点光与发光表面。点光不投影，避免四车警灯产生大量阴影成本。
- **Range**：Range Opacity=0.12、Range Ring Opacity=0.36、Scan Opacity=0.18、Scan Speed=0.65、Scan Width=0.12、Ring Width=0.025、Ground Lift=3 cm。范围半径自动取 **Rules.Radius**，不用另配视觉倍率。圆盘表示最大水平范围；实体墙仍可挡住通缉累计，淡红提示不表示隔墙有效。
- **UI**：Star Count=6、Star Size=18、Star Gap=3、Stars Above Health=52（DPI 缩放后的 Slate 单位），调整与原血条／P编号间距；Star Color／Star Outline 控填充与轮廓。默认位于原血条和 P 编号上方，不改血条规则。
- **Arrest**：Arrest Size=65 cm、Arrest Height=55 cm、Arrest Pulse Speed=1.5，控制手铐标识。标识不参与碰撞，在各客户端朝向本地相机。
- **Audio**：Siren Sound 可替换为配置为循环的音效；Siren Volume=0.35，Inner Radius=200 cm、Falloff=2500 cm。原 WAV 可重导入，不必修改技能代码。双端自动化关闭声音，不替代真人音量／空间听感验收。

## Widget Blueprint 替换接口

**FX → UI → Widget Class** 留空，使用原生六星及逮捕提示。需要自制布局时，创建父类为 **HALPursuitHUDWidget** 的 WBP，添加实际 Designer Root／布局，在此字段引用；存在 Designer 布局后，原生星级绘制退出，以该 WBP 为准。空 WBP 仍使用原生回退。

- **Get Pursuit Views** 返回 Target、Fraction、bArrested、bProtected 和 Style；**Receive Pursuit Views** 每0.1秒推送只读视图。
- 收到空数组时清除旧目标和标识；Overlay 可以保留到换世界，不应依赖整个 Widget 被销毁才清空 UI。
- 以 **Target** 作为对象身份，不依赖数组位置。可用 Project World Location To Widget Position，将目标物理体顶部投影到屏幕；原血条基准为碰撞体 Bounds 中心＋Z*(Extent.Z+30)。
- 第 i 颗星填充量：`Clamp(Fraction * StarCount - i, 0, 1)`，i 从0开始，布局从左到右。可以换纹理、动画和形状，不能从 UI 修改通缉值或施加 Stun。
- 每个本地玩家只有一个不拦截输入的追捕 Overlay，由 LocalPlayer Subsystem 持有，换世界时清理。所有 Car06 建议共用同一技能 DA／Widget Class。

## 精确挂灯、六星与 Fred 测试木桩补充（2026-11-15）

### 本次保存资产核对

UE5.8.3 Python Commandlet 只读加载保存的 BP／DA／地图，无 PIE、保存、重导入或代码构建。1324份 Content 资产／地图、Config及uproject文件前后哈希一致；进程退出0，汇总0错误／0警告。原始输出：`Saved/Car06Pursuit/ConfigAudit_20261115/report.json`，日志同目录 `Inspect.log`。以下为保存状态，不能代替实际游戏验收。

- `GA_Car06_PolicePursuit.SkillConfig` 已引用本技能 DA；`DA_Vehicle_Car_06` 已使用 VehicleDefinitionV1，唯一 Primary Grant 引用此 GA。
- 当前技能 DA：Duration=8、Radius=900、Threshold=100、GainPerSecond=25、**DecayPerSecond=40、StunSeconds=3**、ProtectionSeconds=2、CooldownSeconds=18。后两项不同于首测默认的20／1.5，是用户当前保存值，核对过程保持。
- `DA_Car_06_Presentation.Mesh` 为 `/Game/Assets/Cars/Car_06/SKM_Car_06`；当前 Automatic Roof=true、Light Spacing=35、Beacon Radius=9，尚未指定精确 Socket。
- 当前 Widget Class=None，六星原生回退启用；Star Count=6、Size=18、Gap=3、Stars Above Health=52。
- `/Game/Maps/Fred_TestScene` 的 GameMode Override 是 `BP_MVPB_ListenGameMode`，Controller 为 HALPlayerController，GameState 为 HALMatchGameState，默认 Pawn 为 Car06；当前没有保存的 TestVehiclePawn／PassiveTestVehicle 实例。不是单车测试 GameMode。

### 如何把两灯对准模型原警灯

当前代码**额外生成两颗发光灯体及两个点光源**，不会自动把原模型红蓝灯罩的材质改成闪烁。精确挂点可以把这两颗灯对准原有警灯；“原灯罩自身发光交替”是另一个材质联动需求，当前没有逐灯材质槽／遮罩参数绑定接口。不要直接修改多车共用的母材质来模拟单车技能状态。

1. 打开 `SKM_Car_06`，在 Skeleton Tree 找到实际承载车顶的稳定车身骨骼（如模型确有 `Chassis_jnt` 可用它；以当前骨骼树为准）。右键该骨骼 **Add Socket**，命名示例 `FX_PoliceLight_Center`。名称是本步骤新建的约定，不表示资产里已经存在。
2. 把 Socket 移到原红／蓝灯两个发光中心的**中点**。若该 Skeleton 被其他车型共用，可使用该车型的 Mesh Socket，避免把车型专属挂点加到共享 Skeleton。保存对应 Mesh／Skeleton。
3. 用坐标工具核对 Socket 的**局部 Y 轴**沿两个灯的连线，局部 X 沿车头、Z 朝车顶外侧。导入模型的骨骼轴不一定等于车辆世界轴，以工具显示为准；Socket Rotation 或后面的 Offset Rotation 都可校正。代码沿最终挂点的局部 Y 正负方向布置灯。
4. 打开 `/Game/Data/Abilities/DA_Ability_Car06_PolicePursuit` → **Presentation → FX → Roof Lights**，取消 **Automatic Roof**。展开 **Roof Mount**：Part Id=`Body`、Socket Or Bone=`FX_PoliceLight_Center`、Offset Location／Rotation先归零、Scale=(1,1,1)，Fallback To Mesh Origin 保持关闭。
5. **Light Spacing** 填两灯中心的距离（cm）。它是完整间距：35表示分别在挂点两侧17.5cm。**Beacon Radius** 是额外灯体半径，按原灯大小缩放，建议先试3～5cm再目测。只改Socket／Offset调位置，避免为警灯修改车辆根或整车Mesh导入旋转。
6. 默认红灯在挂点 **-Y** 侧、蓝灯在 **+Y** 侧。颜色位置相反时，将挂点绕自己的Z轴转180°，或在 Offset Rotation 中做相应校正。直接交换 Red／Blue 会同时改变范围／逮捕等共用配色，优先修正挂点朝向。
7. 精确模式的 **Roof Lift 不再参与定位**。高度使用 Socket 或 Offset Location。Socket 名错误、Part Id 不存在或视觉部分尚未注册，会跳过警灯；范围和技能逻辑仍可继续。不要通过勾选原点回退掩盖错误挂点。
8. 保存后停止／重新释放技能，观察静止、转弯和翻滚时两灯是否仍在原灯位置。技能会捕获配置快照，修改DA不自动改变已经运行的实例。挂点变动建议结束当前Play再测。

亮度调 Signal Glow（可见灯体）与 Light Intensity／Light Radius（照亮周围），闪烁速度调 Flash Half Period；0.22秒意味着完整红蓝交替周期约0.44秒。原模型表面不受点光或已有高强度 Emissive 时，点光不会自动让灯罩明暗交替。当前Mesh确有共享 `Emission` 等槽，但这次没有验证红蓝灯面分别使用哪个槽／遮罩，不能把某个槽名当成已确认的独立灯罩绑定。

### 六星如何启用和调节

不需要在Car06或每辆受影响车辆上添加 Widget Component，也不需要把星星图片贴到血条。技能运行时由每个本地玩家的追捕 Overlay 读取服务器复制数据，自动把星级投影到**被追捕目标**上方。

在同一技能 DA 的 **Presentation → FX → UI** 保持 Widget Class=None，直接使用现有原生绘制：

| 参数 | 当前值 | 调节效果 |
| --- | --- | --- |
| Star Count | 6 | 六颗星；不改变通缉阈值 |
| Star Size | 18 | 星星尺寸，DPI缩放后的Slate单位 |
| Star Gap | 3 | 相邻星间距 |
| Stars Above Health | 52 | 相对血条投影基准向上的屏幕距离，越大越高 |
| Star Color | 灰色 | 已填充部分颜色 |
| Star Outline | 深灰 | 星形轮廓颜色；当前描边线宽固定为1.6，没有DA线宽参数 |

规则进度由 Rules.Threshold／GainPerSecond／DecayPerSecond 决定。当前阈值100、增长25/s时约每0.667秒填满一颗，4秒填满六颗并逮捕；当前衰减40/s，满进度量级约2.5秒衰减完（实际逮捕前进度低于100）。有通缉值才显示，从左向右连续填充；离开时反向退回。每个警车独立累计，UI取最高归一化值。逮捕后本来源进度归零，显示手铐／ARRESTED；其他来源若仍有残余值，星级可与逮捕标识短暂并存。保护期间仍按规则衰减。

要自制WBP：新建父类 **HALPursuitHUDWidget** 的 Widget Blueprint，在Designer放置实际Root／布局，填写技能 DA 的 Widget Class。通过 **Receive Pursuit Views** 每0.1秒更新，或 **Get Pursuit Views** 读取；以Target维护每个目标的UI，Fraction是0～1。第i颗星（从0开始）填充为 `Clamp(Fraction * 6 - i, 0, 1)`。Designer存在布局后原生绘制退出，需要WBP自行实现目标投影／六星／逮捕提示；空数组清除旧提示。该替换接口只管追捕UI，不会同时替换现有血条UI。

原生血条／P编号当前由房间UI按房间成员绘制。Passive既不是房间玩家也不是该UI的目标，放进场景不会自然获得P编号／血条；技能六星虽然有自己的Overlay，也无法绕过追捕目标资格。

### Passive 资产仍可用的部分与实际缺口

只读核对旧 `BP_PassiveTestVehicle` 仍为 Configuration Source=Definition，引用 `DA_Vehicle_Passive_TestBaseline_v1`；两份资产与2026-10-17留档的SHA256一致。当前加载值：Ready For Use=true、Collision Shape=Box、Box Extent=(100,70,35)、Max HP=100、Combat.Version=AttackSnapshotV1、Damage Taken Multiplier=1、Impact.Version=TieredV1、Tier Reduction=0、Collision Tier Cap=Heavy。

原生 `APassiveTestVehicle` 是 **Actor**，已拥有独立ASC／Health、Combat、Impact属性集及ImpactReceiver，无玩家输入、驾驶Movement或自动控球。ImpactReceiver会接入当前全局VehicleStability规则、共用Gravity Scale与物理材质。因此不是仍停留在MVP-A的纯旧HP木桩；不要仅因Abilities显示LegacyPlayerState就给它填Primary GA，它本来不消费车型技能Grant。

| 测试项目 | 旧Passive适用性（源码检查；本轮未PIE） |
| --- | --- |
| 基础物理推动、球／共用结算管线造成伤害和分档冲击 | 接收路径存在，可继续作为基础木桩 |
| Car03陀螺／Car04冲锋接触伤害和冲击 | 按ImpactReceiver识别车辆接触的路径存在，可测试规则结果；无完整车型表现组件，不等于能验全套受击表现 |
| Car05龙卷风车辆受击 | 扫描只遍历TestVehiclePawn，Passive不会作为车辆接收；在部分场景对象查询中还可能被视为实体障碍 |
| Car06通缉／逮捕／六星 | 扫描只遍历TestVehiclePawn，还要求有效Controller、当前Avatar与PursuitTarget；Passive没有这些，不会累计。仅在BP增加PursuitTarget组件也不能解决类型／资格限制 |
| SlipperyTrail／抓地力与驾驶失控 | 接收扫描只遍历正式车辆，且Passive无驾驶Movement／区域接收器，不能验证实际打滑手感 |

**只用旧Passive测基础项目的配置**：把 `BP_PassiveTestVehicle` 拖入 Fred 场景，实例 Configuration Source=Definition、Definition指向旧Passive DA。需要独立耐久／外观时复制该DA作为 `DA_Vehicle_Dummy_Fred`，再改实例引用；保持Box与有效Static Visual。不要直接引用正式Car06 Definition，它是ConvexMesh而Passive代码明确只接受Box；也不要仅给Passive填Skeletal Presentation，它不消费该表现配置。重复测试可提高木桩Max HP（有限值）或重新Play，保持Damage Taken Multiplier=1、Tier Reduction=0作为中性基准，避免用免疫掩盖效果。

**完整技能木桩建议（审计时未施工，后续已实现）**：现有示例DA、团队BP与Fred接线见[正式技能木桩配置](../Configuration/Skill_Test_Dummy_CN_v0.1.md)。实现保留旧Passive作为基础回归夹具，新木桩基于正式车辆Pawn；单独持有ASC而不伪造PlayerState／Controller，复用Movement、状态及表现接收，具有独立DA与轻量BP入口。追捕“能发动技能”与“能接收技能”独立判断，连逮捕保护定时器也使用目标接口；不加入正式房间席位／胜负统计，不自动驾驶／施放主动技能。首版无自动回血／复位，测试结束后重开Play。不能简单把普通Pawn拖进场景后靠Auto Receive Input或占用Player0替代该生命周期。

**Fred当前模式注意**：本次保存的是Listen GameMode，仍受比赛／房间阶段权限约束；测试技能应进入允许技能的Playing阶段，房间Open练习期可能不允许技能／HP伤害，具体依模式。若后来改用 `BP_VehicleTestGameMode`，其原生 `ChangeTestVehicle` 会在初始生成和每次成功换车时销毁其他TestVehiclePawn及Passive；需要同步维护测试目标保留规则，不能只在地图里放好就默认会保留。本轮没有更改该清理规则。

## 代码职责与未来约束

| 文件／类 | 职责 |
| --- | --- |
| HALPolicePursuitAbility | ServerOnly GA、合法发动、冷却、唯一活跃实例，无 GA Tick |
| HALPolicePursuit | 服务器定时扫描、遮挡／高度／退出缓冲、独立通缉记录及单次逮捕资格；客户端只复制／表现 |
| HALPursuitTargetComponent | 目标跨来源保护、共用 Stun 真正结束后的尾段、逮捕标识及 Avatar 清理 |
| HALPolicePursuitPresentation | 警灯、点光、扫描网格、警笛及目标手铐；Tick 只负责表现 |
| HALPursuitHUDWidget／HUDSubsystem | 只读聚合、六星绘制、WBP 替换、每本地玩家生命周期 |
| HALBuildPolicePursuitFXCommandlet | 原生资源制作与音频源路径维护；默认拒绝覆盖既有资产，不是日常调参入口 |

Native Tags：`Ability.Vehicle.PolicePursuit`、`Cooldown.Vehicle.PolicePursuit`、`State.Vehicle.Arrested`、`State.Vehicle.PursuitProtected`。后两者由 Avatar Scope 的无限 GE 管理，由目标组件在 Stun 结束／保护到期或退出时移除；Stun 始终调用现有 `ApplyStun`，绝不复制另一套驾驶禁用或电击逻辑。已发动追捕的生命周期检查与“能否新发动”分开，禁止用 IsStunned／IsExecuting 取消正在运行的警笛。

不把通缉值改为全局共享单值 Attribute，不允许多个警车相加；不允许客户端报告命中或进度。新增抵抗类型应与伤害／冲击抵抗独立。表现资产、星星数量和填充动画不决定阈值／保护时长。精修可替换 Mesh／Material／Sound 与 WBP，逻辑仍由权威 C++ 管理。

## 验证、已知边界与团队验收

最终 UE5.8.3 **Editor BuildEditor_R7／Game BuildGame_R4** 编译通过。原生＋真实 D3D12＋Stun／Car03／04／05回归 **Final_R3 24/24**，零测试警告／失败。扫描改变 **135160** 像素，手铐标识改变 **146** 像素，四颗已填星／两颗空星的中心亮度为 **231／231／231／231／14／14**，验证左到右真实填充。红／蓝灯交替、半径驱动视觉缩放、原声循环资源均有检查。

最终 **Network_R4 Host／Client 各1/1**，零测试警告／失败、退出0，Host 出站80 ms，真实双警车完成通缉、互相逮捕、Stun 结束后的保护尾段、已有技能在施放者 Stun 中持续、表现挂接、无额外 HP 伤害及结束清理。最后只修正网络夹具从“自身技能发动4秒后”错误假设改为“观察到自身 Stun 后仍保持超过0.35秒”，因为两次技能可以独立错开启动；生产代码在 Final_R3 与最终网络间保持不变，未重复未受影响的24项。

开工 **1316/1316** 份既有 Content／Config／uproject 哈希不变，新增 **六份 uasset＋一份 WAV 源文件**；用户 GameMode BP、Car05 调参／资源、Car06 原 BP／Definition 均保留。源码新增窄接口并纳入 Git 白名单，未提交／Push。原始日志／BMP 在本机 `Saved/Car06Pursuit/20261115`，可同步[验证摘要](Car06_Police_Pursuit_Verification_20261115.json)。

中间失败不计作通过：首轮有指针／颜色API／声音工厂受保护入口编译问题，已修复；逻辑首测1/2中同步夹具在同一 GFrameCounter 多次 Tick，TimerManager 不重复派发，改为在已推进世界时间显式刷新目标保护，真实到期另由双端验证；首轮星形闭合数组元素别名触发断言，实际 RHI／网络均捕获并修复；Network_R3 客户端启动时差检查失败按上述修正，最终重编并复验。

仍待团队：按上文接 GA／Definition，真实 Car06 模型的警灯精确位置、实际相机下星星和手铐大小、声音与多警笛混音、复杂坡面／墙角、实体墙遮挡与边界手感、真实自救动作期间追捕、淘汰／换车／返房、四机／Cook／发布包及真实晚加入。自定义 WBP 未由代理创建或真人验收，本轮验证原生绘制与只读视图，团队替换时需单独检查布局。原生自救连续性是状态夹具，未冒充真实输入长按自救；自动化关闭声音，音频只验资源，未冒充听感通过。所有测试使用原生 Match GameMode＋临时授予，不代替团队完整房间 BP 流程验收。
