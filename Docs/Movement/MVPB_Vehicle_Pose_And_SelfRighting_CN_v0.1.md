# 姿态操控限制与长按自救：实现和配置（v0.1）

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
| Maximum Recovery Angular Speed | 4rad/s | 恢复力矩不继续加速超过此角速；不清除已有更高角速 |

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
