# 姿态操控限制与长按自救：实现和配置（v0.1）

**2026-10-29侧翻自救补充以第9节为当前入口。** 保留原已保存参数；允许将Minimum Recovery Tilt设为Drive Exit Tilt以关闭角度空档，补充仅供自救使用的近地支撑采样及Block拒绝原因。较快响应由团队按第9节修改共享DA；不缩短长按，不直接修改保存资产。构建与验证以本次新增记录为准，下面10月21日结果为历史。

日期：2026-10-21；施工基点 `main/f634292b4893fafdf9bba52e5b8e1a928f2469e5`；本机UE5.8.3-58210709。用户反馈上一轮修正后“弹动有效减少”，并授权开始本步。原[已确认规则](MVPB_Vehicle_Stability_And_SelfRighting_Plan_CN_v0.1.md#51-首版测试规则2026-10-21用户已确认)继续有效；VP-1结束，10-F-4完整集成／四机仍后置。

本轮代码、Editor／Game编译和自动化检查已完成，**手工输入接线与真人手感仍待团队完成**。完整报告及保护哈希见[本轮验证摘要](MVPB_Vehicle_Pose_And_SelfRighting_Verification_20261021.json)。代理未保存或编辑BP、DA、Input Action、地图。

## 1. 行为与实现边界

普通驾驶只在有效近地支撑、地面坡度及车身向上轴满足规则时施力；侧翻／倒扣时关闭油门、刹车／倒车、抓地、转向和脱墙辅助，保留Chaos惯性、重力和碰撞。门控不只清除本地输入，服务器和拥有客户端的物理执行／重模拟共用同一逻辑。

支撑探测仍是窄范围中心射线，使用当前Internal刚体姿态投影代理包围盒高度，不再拿延迟的GT世界Bounds高度判断当前翻转姿态。当前不是四点轮胎悬挂，也不保证所有轮子同时触地。地面坡度和车身相对地面倾角分别判定；进入／退出阈值加短暂稳定时间，所需记忆进入物理历史。

自救由服务器 `VehicleRecoveryComponent` 的游戏时间定时器判定：近地且持续低速侧翻1秒，才累计有效长按1.5秒。释放或条件失效清空进度；高速、腾空、无控制者、无有效HP、非比赛可操作阶段或物理观测失效时不能开始。单纯侧翻和长按阶段保持原发射／技能规则。

真正接受翻正开始时，服务器释放持球为Free，保留原重捕锁，不添加释放／发射冲量、不产生攻击球。执行期间禁止自动捕球、主动发射和新技能；已有GE自然计时，不回血、不驱散、不授予无敌。

翻正使用PD角加速度及速度／时限约束，主要修正车身向上轴，尽量保留Yaw。倒扣时使用确定的备用翻转轴，避免叉积为零；无坐标传送、不直接设旋转、不清零线速度。碰撞仍由Chaos解决，卡墙／没有空间时可能超时失败，不强行穿过几何。执行允许车体滚转引起的短暂离地，成功仍要求回到有效近地支撑；2秒结束仍未满足则超时退出，不因短暂离地重新增加时限。成功提前结束；结束后3秒冷却，长按阶段取消没有冷却。

新的外部有效Medium／Heavy冲击打断，阈值采用抗性／免疫后的档位。ExplicitTier攻击具有明确来源语义；原生PhysicalContact还要识别来源并要求来源朝受击方向的运动，避免地面／墙、自身翻正撞上静止物体而打断。普通地板、静止墙和自身接触不作为外部攻击；若接触运动信息无法证明是外来运动，保守保留恢复。该判定不改变原伤害或冲击本身的结算。

## 2. 线程和联机

- `FVehicleInputCmd.bSelfRight` 表达按住意图，随原连续输入管线传递；客户端不提交已长按多久、完成结果或复位Transform。输入比较、插值、合并、失效释放和序列化已接入。
- 服务器GT依据物理线程发布的只读观测计时并验证权限；许可计时与球／GAS副作用不在回滚中执行。
- 服务器接受开始／结束时发送 `FVehicleRecoveryAction`，由现有Network Physics动作通道执行；本地单机通过受锁队列交给下一物理步。
- 动作携带执行序号、是否活动、目标向上轴和备用轴。当前阶段／执行累计时间、驾驶门控迟滞状态随物理历史恢复；相同序号的重复开始不重置时限，历史回放不重复脱球或技能操作。
- 物理执行还独立限制2秒时长，GT暂时卡顿也不会无限施力。共享DA规则冻结到每World纯值快照，全部参数纳入配置签名；两端不一致沿用原阻止模拟机制。
- Owner退出、失去控制者或切换Avatar取消本Pawn的待执行状态；不清空PlayerState上的既有效果。恢复不复用Primary技能槽，也不是GAS逐帧Ability。

## 3. 共享DA配置

1. 完整编译后重新打开Editor，停止旧PIE／独立游戏。
2. 打开当前 `/Game/Data/MVP_B/DA_Vehicle_Stability_Test_v1`。继续使用原来的全局DA、PM及 **Project Settings → Game → Vehicle Stability → Shared Rules** 引用，不另外新建总配置DA。
3. 展开新增 **Recovery** 结构，首轮采用以下默认值；旧资产未序列化新字段时将读取C++默认值，仍建议检查并保存一次，使团队接续可见。

| 配置组／字段 | 首轮值 | 作用 |
|---|---:|---|
| Pose → Enabled | true | 启用姿态门控与长按自救；共享接触组开关仍独立 |
| Maximum Ground Slope | 45° | 有效地面法线坡度 |
| Drive Enter Tilt／Drive Exit Tilt | 50°／65° | 相对地面法线的允许驾驶迟滞 |
| Drive Stable Seconds | 0.1秒 | 重新允许驾驶所需稳定时间 |
| Support Distance | 30cm | 代理投影包围盒以下的探测余量 |
| Minimum Recovery Tilt | 70° | 达到此倾角才算侧翻／倒扣候选 |
| Flipped Seconds | 1秒 | 连续满足近地、侧翻和低速的资格时间 |
| Maximum Linear Speed | 150cm/s | 资格线速上限 |
| Maximum Angular Speed | 60°/s | 资格角速上限及成功收敛检查 |
| Hold Seconds | 1.5秒 | 资格成立后累计长按 |
| Maximum Execution Seconds | 2秒 | 有界翻正时限 |
| Cooldown Seconds | 3秒 | 成功、打断、超时后的重试冷却 |
| Minimum Interrupt Tier | Medium | 有效外部冲击打断门槛 |
| Success Tilt | 15° | 回到近地且低角速时的成功倾角 |
| Angular Position Gain／Angular Damping Gain | 25／7 | 翻正PD增益 |
| Maximum Angular Acceleration | 30rad/s² | 恢复力矩对应的角加速度上限 |
| Maximum Recovery Angular Speed | 内部4rad/s；Editor可能显示229.18°/s | 恢复力矩不继续加速超过此角速；不清除已有更高角速 |

字段带范围、有限值与跨字段校验：`Success Tilt < Drive Enter Tilt < Drive Exit Tilt < Minimum Recovery Tilt`，打断档位只允许Medium／Heavy。不要把 `Support Distance` 设得很大来掩盖真实腾空，也不要通过无限增大增益补偿卡住的几何。

保存DA。调参后停止本局并重新开局，两端使用一致代码与资产。新参数改变了签名，不能再要求它等于旧4097445932／492667503；应比较本次Host和Client是否一致且没有mismatch。

## 4. 独立长按输入：Editor手工步骤

1. Content Browser在现有输入目录新建 **Input → Input Action**，建议 `IA_SelfRight`，**Value Type=Digital (bool)**。不要添加Hold、Tap或持续时间Trigger，1.5秒由服务器代码计算。
2. 打开玩家现有Input Mapping Context，新增 `IA_SelfRight` 映射。首轮键盘建议 **R**，控制器建议未占用的 **Gamepad Special Left**；如该键已用于菜单，选择另一个独立按键。不要占用Primary技能或发射键。
3. 打开当前玩家 `BP_RoundedVehiclePawn`，在Class Defaults清空搜索，查看 **Vehicle → Configuration → Definition**。打开其引用的玩家Vehicle Definition，再打开其中 **Local → Local Config** 指向的DA（现有基线通常为`DA_Local_TestBaseline_v1`）。在该 **Local Config DA → Input → Self Right Action** 中选择 `IA_SelfRight`。保留原Default Mapping Context及其他Action引用；无需Event Graph连线。**Definition模式下BP里的旧Input字段有意置灰，运行前由Local Config覆盖，不在BP里填写。**
4. 保存 **Local Config DA、IMC和IA**。当前已有玩家Definition／Local引用时无需改BP；如果确实调整了引用，再Compile并Save对应BP。提交本轮原生源码、文档、共享DA及手工保存的输入配置资产；不提交Saved／Binaries／Intermediate。
5. 重启Host／Client，新局按住映射键。松手的Completed／Canceled都会清除意图；换Pawn、退出、重新获得控制也会重置输入。若键盘失去游戏焦点，先释放再重新按住。

代码入口是原生输入回调，不需要创建新的GA／GE，原加速技能配置保持。没有默认硬编码R键后备绑定；未配置IA／IMC／Local Config引用时长按不会触发。新增SelfRightAction为可选字段，旧Local资产仍可加载；填写时必须Digital (bool)，且不能与驾驶、发射、Primary Skill共用同一个IA。

## 5. 调试与团队复测

控制台开启：

```text
hal.DebugHUD 1
hal.VehicleRecoveryLog 1
hal.CombatLog 1
```

既有只读Debug HUD增加Pose Drive／Ground、Self-right阶段、Hold百分比、CD和Outcome。阶段为Idle、Eligible、Holding、Executing、Cooldown；这是开发反馈，不是正式恢复UI。日志默认关闭，服务器输出阶段切换、执行序号及物理帧；客户端以复制状态显示服务器进度。

1. 平地低／高速、倒车、手刹、加速、允许坡面应保留驾驶。侧翻／倒扣后按油门、左右、刹车和手刹，应不能像轮胎着地时驱动；不能靠加速GE绕过门控。临界倾角不应反复开关。
2. 倒扣稳定后按住：观察资格等待及Hold累积；提前松手、继续翻滚或速度超限应清零，不能接着旧进度完成。高速飞行、墙面法线和远离地面时不能启动。
3. 分别在左右侧、车顶、车头／车尾翻倒状态测试R自救。开始后松手仍应执行；成功提前结束，卡墙／低顶无空间则应失败退出，2秒后不得继续加力。检查没有坐标跳变、穿墙或回出生点。
4. 持球开始自救应Free脱落、没有发射攻击／释放冲量，执行中不接球、不发射、新技能被拒绝；长按阶段仍允许原技能／发射。已有加速／免疫及Debuff继续计时，HP不恢复。
5. 外部球／弹簧拳、车辆碰撞分别检查有效Light不打断、有效Medium／Heavy打断；抗性降档、冲击免疫与伤害免疫保持独立。普通Floor／墙面支撑和自身翻正擦碰不得反复触发冷却。
6. 成功、受击打断、超时后检查3秒冷却；单纯长按取消不产生冷却。持球重捕锁按原规则自然到期。
7. Host／Client互换角色，测试重复输入、松手、退出／重连、换Avatar和带既有效果退出。分别在30／60／120屏幕FPS及基本延迟下录像；自动化的NullRHI或MaxFPS限帧不替代实际画面验收。

## 6. 验证记录

UE5.8.3 Win64 Development **Editor／Game均成功**；普通回归 **57/57，0测试错误／警告**。其中真实Solver使用当前保存凸包，5个工况共900步：正立油门有效，左右侧翻普通输入不能驱动，侧面／倒扣主动翻正均在2秒内达到正立区域；重复开始不刷新执行时限。该夹具单独执行生产物理路径，不包含GT许可计时，后者由纯时钟测试及真实联机流程补充。

| NullRHI进程FPS上限 | Listen Host | Client | 客户端重模拟次数 |
|---|---|---|---:|
| 30 | 1/1 | 1/1 | 29 |
| 60 | 1/1 | 1/1 | 44 |
| 120 | 1/1 | 1/1 | 61 |

三组均运行实际MVPB_NetTest及临时规则夹具，主机单向注入80ms，并非实测RTT；双方观察到Hold和执行阶段、各一组开始／停止序号2、正立和Succeeded结果。临时规则签名2346082778；普通回归中保存正式DA的新签名915860017，资产路径不同，不跨夹具比较。NullRHI限帧证明计时／数据路径的基本检查，不等同于真实30／60／120屏幕渲染和手感验收。

失败记录保留：首次UHT因角加速度单位字符串／Blueprint uint32字段失败，改为注释单位和只读int64展示；新增测试句柄const和Sphere头文件问题已修正。两次链接被运行中的Editor／Live Coding锁定，用户关闭后成功；受限进程视图漏报已改用完整视图核对。首轮双端过早因滚转短暂离地退出，已修正执行边界并重跑57项；后续夹具启动较晚错过阶段，以及在就绪同步前提前提交Host长按造成两次测试动作的问题，已增加就绪同步和开始前序号断言，最终三组保持严格断言通过，没有放宽阶段／序号要求。

本轮363份uasset／umap相对开工前全部SHA256相同，Config未修改；既有车轮配置保留。编译仍有既有MSVC版本和引擎弃用警告，不宣称零编译警告。本轮未提交／Push。

尚未验证：团队手工IA／IMC／Local Config接线后的真实输入、HUD画面、低顶／墙角等几何手感、完整球／机关／技能组合与真实退出／换Avatar联机矩阵。普通测试已覆盖脱球／锁、技能拒绝、既有效果保留、免疫／抗性后打断档位、普通／自身接触排除及Owner退出接口，但不将这些孤立测试写成完整真人组合验收。完整网络扰动和四台真实PC继续留MVP-B收尾。

## 7. Definition输入接线补充（2026-10-21）

用户截图显示BP的Vehicle/Input均为灰色。`TestVehiclePawn::CanEditChange`在Definition模式下有意锁住旧Pawn配置，输入引用实际来自`VehicleDefinition.LocalConfig`；BP上显示的Primary Skill／Self Right为None也不能单独证明运行时引用为空。不要切换Legacy或解除旧字段锁定来绕过统一配置。

初次自救实现漏加了`UVehicleLocalConfig.SelfRightAction`及`ApplyDefinition`复制，且上文第4节原步骤错误地要求在BP填写。已补Local可选字段、类型／独立性校验和初始化复制，并修正第4节。原57项和三组双端用原生意图入口验证了后续物理流程，**没有覆盖这条缺失的Editor资产配置链路**；新增断言检查Definition模式的Local引用在BeginPlay／输入Setup前到达Pawn，且生成Started／Completed／Canceled三个Enhanced Input绑定。补充构建与回归证据另存于[本次摘要](MVPB_Vehicle_SelfRight_InputDefinitionFix_Verification_20261021.json)，不覆盖前次JSON。

补充验证基点`main/c3f90ca`，本机UE5.8.3 Editor／Game Development均成功，重新运行完整普通回归57/57、0测试错误／警告。保留用户已保存的共享DA、IMC及新IA，364份uasset／umap相对本次开工全部SHA256相同，Config未改。没有重跑新Host／Client或验证团队实际输入接线；下一步重启Editor，在当前玩家Definition引用的Local Config中填写Self Right Action并保存，再新开游戏实测。本次修正未提交／Push。

## 8. 团队真人验收与VP-2接续（2026-10-21）

用户确认：**“自救真人验收已经完成，功能可以工作，表现可以后面再优化”**，并授权开始VP-2。将此记录为自救功能真人验收完成；表现优化列后续事项，不再将输入接线或基本功能作为VP-2阻塞。输入补充代码及团队Local DA／共享DA／IA／IMC已在`eae910e`提交。

本条来自团队实际操作反馈，未附新的逐项角色／FPS／网络扰动清单或日志，不扩写为所有组合和四机最终验收。前文“尚无真人反馈／未提交”均保留为当时的历史记录，当前结论以本节及项目状态为准。

## 9. 侧面着地自救与较快翻正响应（2026-10-29）

用户反馈同时存在“没有长按进度”和“触发后尝试翻正但没翻起”，另要求加快实际翻正，未要求缩短按键时间。核对基点`main/ac9ffe1`、UE5.8.2-56702186，开工工作区干净。该提交已包含上次合并、车型资产重命名及团队NetTest地图更新；保留当前保存资产，不从旧基线回填。

### 9.1 已核对边界与修改

- 原默认Drive Exit Tilt=65°、Minimum Recovery Tilt=70°，65°～70°附近可以出现已经不能驾驶、又不符合自救倾角的空档；旧校验还禁止把两者设为相等。现在允许`Drive Exit Tilt <= Minimum Recovery Tilt`，团队可以配置为65°／65°。原已保存70°保持，未用C++覆盖；相对有效地面法线判定，视觉车身倾斜不作为物理姿态。
- 原中心近地射线可能漏掉只支撑代理一部分的台沿，或者首先命中墙面。仅当中心未提供有效可用地面时，新增固定顺序、位于代理投影范围内的偏移射线，继续使用原垂直距离与坡度限制。结果只供自救资格／成功检查，普通驾驶、轮胎表现接地仍使用原中心探测；这不是物理悬挂，也不会给腾空车辆无限距离找地。
- HUD新增服务器复制的`Block`拒绝原因，`hal.VehicleRecoveryLog 1`在阶段／原因改变时记录Ground、Tilt、Speed、AngularDeg、Held与Frame。可区分未开始计时和已执行后Timeout／Interrupted，不再只显示Idle而没有依据。
- 原PD翻正本体、世界向上目标、角速／角加速度限制、2秒时限及碰撞保留；不传送、不直接改旋转、不增加抬升／穿透或落地自动扶正。更快响应使用已有DA参数。规则签名标记为RecoveryV2，物理历史序列化格式3不变，两端必须同步重编并使用相同资产。

原已编译版本的当前保存凸包理想平地Solver基线1/1通过，左右侧翻和倒扣可翻起。这只能排除理想平地完全不可翻正，不能确认用户原局是哪一个拒绝条件，亦不能说明所有贴墙几何都有空间。没有取得失败原局的Recovery日志，不将上述代码边界冒充唯一原因。

### 9.2 团队Editor调参步骤

1. 完整重编后重开Editor，停止PIE／Standalone。先在Project Settings → Game → Vehicle Stability确认Shared Rules当前引用，打开该DA；当前配置路径仍是`/Game/Data/MVP_B/DA_Vehicle_Stability_Test_v1`。
2. 展开 **Recovery → Pose／Eligibility**。保留Drive Enter Tilt=50°、Drive Exit Tilt=65°，将 **Minimum Recovery Tilt设为65°**；这关闭默认角度空档。若该DA已使用其他驾驶阈值，则让Minimum Recovery Tilt等于实际Drive Exit Tilt，勿照抄65°覆盖其他已调值。
3. 展开 **Recovery → Response**，先记下原值，再试以下较快组合：

| 字段 | 原生初值 | 建议测试值 | 作用 |
|---|---:|---:|---|
| Angular Position Gain | 25 | **60** | 增强纠正倾角的角加速度需求 |
| Angular Damping Gain | 7 | **12** | 配合更强响应抑制过冲与摆动 |
| Maximum Angular Acceleration | 30rad/s² | **80rad/s²** | 放宽恢复力矩的角加速度上限 |
| Maximum Recovery Angular Speed | 内部4rad/s＝显示约229.18°/s | **内部6rad/s＝显示约343.77°/s** | 提高翻正过程的角速上限 |

**单位对照（2026-10-29截图确认）：** 该字段原生存储单位是rad/s，当前Editor自动以deg/s显示；截图末行229.183121 deg/s对应内部4rad/s。若输入框后缀为deg/s，建议填写 **343.774677**（约343.77°/s），对应内部6rad/s，不能直接填写裸数字6，否则会成为6°/s、反而大幅降低翻正角速。上方另一个`Maximum Angular Speed=60deg/s`是启动资格／成功角速阈值，保持原值。Angular Position Gain=60、Angular Damping Gain=12、Maximum Angular Acceleration=80按截图无单位数值填写，不需要一起换算。

4. **保留当前Hold Seconds、Flipped Seconds、Maximum Execution Seconds和Cooldown Seconds。** 本次只读实际保存DA为：长按1.5秒、侧翻资格等待**2秒**（已不同于旧原生1秒）、执行上限2秒、冷却3秒；因此在刚稳定侧翻时按住，约需2+1.5秒才开始执行。Maximum Execution Seconds是失败退出的上限，减小它不会加快翻正，反而可能提前超时。Support Distance、线速／角速资格及普通Physics Body阻尼先保持，避免同时改变驾驶／受击表现。
5. Save该DA并Validate。停止本局、重新启动Host／Client，两端使用同一份代码与DA；共享快照不在本局内热更新。R的IA／IMC／Local接线保持，不需要新增GA或修改PlayerState。
6. 控制台输入`hal.DebugHUD 1`、`hal.VehicleRecoveryLog 1`。依次测左右侧、65°～70°斜靠、倒扣、靠墙／墙角和窄支撑；低速稳定后长按R，观察Holding→Executing→Succeeded。开始后可以松手。再测腾空／高速翻滚仍不能启动、受外部Medium／Heavy打断、持球执行时释放、冷却及双端观察。

`Block`含义：`Tilt`未达到自救倾角；`NoGround`未取得近地支撑；`SteepGround`坡度不合格；`LinearSpeed`／`AngularSpeed`尚在移动／翻滚；`Stabilizing`等待配置的Flipped Seconds资格时间（当前保存2秒）；`NotHeld`未收到按住意图；`FrameGap`服务器计时样本间隔过大；`OwnerOrPhysics`控制者、HP、比赛或物理新鲜度条件不足；`Stunned`被眩晕；`Cooldown`等待重试。`None`表示没有上述阻塞，不等于动作已结束。日志在权威Host的运行日志查看，客户端HUD显示复制结果。

已进入Executing却Timeout时，检查空间与代理几何；加强有限力矩不会保证穿出夹缝。上述数值是可回退的测试响应，正式平衡仍以后续实际车型与双端手感为准。自动化数据不代表团队已保存新DA或完成真人画面验收。

### 9.3 本轮验证

本机UE5.8.2-56702186 **Editor／Game完整编译成功，最终普通回归104/104，真实Listen Server双端Host／Client各1/1**，最终3份报告均0测试警告／失败／未运行。新增角度边界、窄支撑／中心遗漏、墙面拒绝、腾空距离及速度拒绝检查；真实Solver使用保存凸包／阻尼，8工况共1440步。15°以内且角速不超过60°/s的收敛时间：原响应侧翻1.017秒、倒扣1.417秒；建议响应左右侧各0.583秒、倒扣0.817秒。是受控平地数据，不保证受阻几何或所有车型相同。

双端在实际NetTest世界中使用双方相同的临时原生平地隔离夹具，左右90°侧翻；均观察到Holding／Executing、开始和结束序号2、两车Succeeded及正立，客户端实际49次重模拟。60FPS上限／NullRHI，主机出站注入80ms，非实测RTT，无丢包／抖动注入；保存共享DA未更改，联机夹具用临时默认规则，较快响应由上述Solver对比验证。

初次普通回归102/104，两个旧测试还引用团队已重命名的SM_CarCollisionProxy；改为从当前玩家Definition读取碰撞Mesh，保持原断言后104/104。初次双端各0/1超时，旧固定地图位置使主机车持续NoGround、客户端车已成功；只将联机夹具移到临时地板隔离区域，不放宽自救资格／成功或时限。失败报告均保留，最终重编后再次完整回归通过。

1172份uasset／umap与开工／用户保存关闭后的基线逐份SHA-256相同，219份构建输入最终未变，Config／uproject／Build.cs未改；全部专用进程已退出。完整构建仍有既有C4305等警告，不宣称零编译警告。详见[本次验证](MVPB_Vehicle_SideRecovery_Verification_20261029.json)。本次未提交／Push；保存DA的65°及较快组合、实际R键／墙角／低顶／复杂组合画面仍待团队按9.2节保存与复测，F4／四机后置保持。

### 9.4 调参后Host无法操作：保存配置诊断（2026-10-29）

团队调参后反馈Host开始比赛即无法操作。最新实际游戏日志`HAL_Future_Creation_2.log`明确先报告`Invalid VehicleStabilityRules ... Invalid Recovery ... Vehicle blocked; no fallback`，随后仍进入`Playing with 1 players`；是共享规则初始化失败，不以Playing或输入焦点作为恢复依据。

只读加载当前保存DA得到：Drive Enter Tilt=50°、Drive Exit Tilt=65°、**Minimum Recovery Tilt=60°**，其余响应为60／12／80／4（末项为内部rad/s），资格等待2秒、长按1.5秒、执行上限2秒、冷却3秒。60°小于65°违反校验关系。团队在Editor打开共享DA → Recovery → Eligibility，将 **Minimum Recovery Tilt从60改为65**，保留Drive Exit Tilt=65；若采用9.2较快组合，将Response → Maximum Recovery Angular Speed的内部4rad/s改为6rad/s，即当前Editor显示约229.18deg/s改为343.77deg/s。后者不是初始化失败原因。

保存并停止旧局、重新Host开始；不需要重编，也不需要重新设置输入或蓝图。新局应不再有Invalid Recovery／Vehicle blocked，并出现有效VehicleStabilityRules Snapshot。Editor的通用Validate Assets结果不能替代新局原生规则初始化日志。未放宽配置校验或静默退回C++默认值。

本次只有日志与现有原生只读诊断检查（1/1），没有新C++、编译或完整联机回归。两份团队修改的Stability／Local DA前后哈希相同，代理未保存资产；实际修正65°／6、恢复驾驶与翻正手感仍待团队操作确认。见[配置诊断留档](MVPB_Vehicle_Recovery_ConfigDiagnosis_20261029.json)，原9.3构建／自动化保持原资产基线，不冒充本次保存配置验收。
