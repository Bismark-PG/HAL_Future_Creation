# MVP-B VP-4B：有效冲击车身回弹实现与配置（v0.1）

日期：2026-10-26（Asia/Tokyo）。基点`main/5c84320c71977f86a97139e45a64063dac3be2d9`，本机UE5.8.3-58210709。用户接受已留档方案并授权施工；开工保留进度同步文档及接续JSON两份未提交改动。代码、Editor／Game构建和原生自动化已完成，1154份Unreal资产前后全同；团队启用DA及实际画面验收待完成。完整结果见第6节，未提交／Push。

## 1. 最终结果入口

`UHALImpactReceiverComponent::Resolve`保留现有物理排队、免疫／降档、脱球和自救打断流程。在成功收尾、最终Summary已写入Result之后，把已解析的受力方向及结果交给现有车辆表现组件。只处理TieredV1统一冲击路径，不轮询LastImpact，也不从HP通知或速度尖峰猜测命中。

表现资格必须同时满足：Hit被接受、无拒绝原因、ImpactApplied、ImpactOutcome=Applied、有效档位为Light／Medium／Heavy、非冲击免疫、非SupportSupplementSuppressed。普通静止地面支撑即使保留Medium诊断档位也不播放；自然碰撞已满足响应、追加RuleDeltaVelocity为0的真实侧向碰撞仍可播放。

伤害与本通道独立：0伤害／伤害免疫＋有效冲击可以播放；冲击免疫／None不播放受击回弹，普通Chaos自然运动仍保留。普通接触沿用原Contact episode与CombatAction幂等检查，持续顶撞不按每次回调重播。没有新增伤害、规则冲量、物理后坐、脱球或技能行为。

## 2. 方向、包络与合成

- 服务器接受冲击时将受力方向转换为受击车辆局部方向，随事件发送。局部X驱动Pitch，局部Y驱动Roll；保留斜向分量，复用车型Chassis轴与符号。近乎纯竖直的世界冲量不臆造水平动画，首版不增加垂直压缩。
- 使用有限时长解析包络：Rise平滑到峰值，Return平滑回零，起点／峰值／终点速度均为零；不使用逐帧弹簧积分。最多8个重叠事件，分别推进后求和，最终统一限幅。
- 驾驶倾斜与发射回弹仍保留独立状态。受击包络在受影响轴的峰值提高受击通道权重，避免被反向驾驶／发射倾斜抵消；淡出后恢复原贡献。最终Pitch／Roll分别受Chassis的Total Pitch／Roll Limit Degrees限制，不重复在蓝图相加。
- 普通腾空允许短促回弹，不依赖四轮接地。车身Up.Z低于配置阈值或正在自救时，现有受击事件永久淡出；该姿态下新收到的事件只消费身份、不启动，随后扶正不会重播旧动作。
- 复用原`Presentation.ChassisRotation`。新增只读诊断`ImpactPitchDegrees`、`ImpactRollDegrees`、`CombinedChassisRollDegrees`；原CombinedChassisPitchDegrees继续表示最终俯仰。没有新增骨骼节点或AnimBP事件图逻辑。

## 3. 联机、身份与生命周期

没有客户端受击预测。服务器在已配置且启用的表现组件生成目标实例内递增非零序号，并发送一次不可靠Multicast；事件带服务器游戏时间、有效档位和量化的目标局部单位方向。主机本地、拥有者客户端和其他观察者走相同消费函数，客户端不提交命中或伤害。

接收端按服务器时间计算事件年龄并进入当前包络阶段，允许最多0.25秒的小幅未来时钟偏差，超过则拒绝；超龄／已过包络、重复／乱序、旧绑定事件不播放。过期或姿态阻止的合法身份仍推进序号水位，避免后续误复活。普通姿态重置保留序号水位、清理活动包络；重配、控制者变化、长帧、瞬移、结束继续使用现有清理边界。新Pawn有新的组件实例，不把旧Actor的事件移交给新Avatar。

该Multicast仅用于装饰，丢包时允许漏一次回弹，不保证每个观察者必达，不承担HP、击飞或脱球传输；晚加入不补历史动作。物理重模拟不产生新的视觉身份。各端插值／时钟会导致显示时刻不同，不承诺同一墙钟帧姿态一致。

## 4. 团队配置步骤

代理不编辑uasset／Blueprint／地图。代码构建通过后由团队操作：

1. 重启Editor加载新反射字段。打开实际玩家Vehicle Definition，通过Presentation引用进入现有`DA_Car01_Presentation`（以实际引用为准）。
2. 展开新增 **Impact Rebound**，勾选 **Enabled**，保存DA；旧资产默认关闭，新通道未启用时不生成该类表现通知。
3. 使用下表起点检查轻／中／重实际受击，再调整幅度和时长。参数属于车型表现DA，球／炸弹／弹簧拳仍只提交既有档位，不分别填写动画角度。
4. **原AnimBP不需增加节点**：原Chassis_jnt Modify Bone保持Rotation=`Presentation.ChassisRotation`、Add to Existing／Bone Space；四轮转向／滚动／悬挂、发射回弹保持原接线。若旧Break结构节点提示字段变化，Refresh Nodes后Compile／Save。
5. 保存后停止旧游戏并重新启动主机与客户端，两端使用同一代码／构建及配置。先单Host检查方向与程度，再双端互换受击／观察角色。

| 字段 | 初始值／范围 | 含义 |
|---|---|---|
| Enabled | false | 团队手动启用 |
| Light Pitch／Roll Degrees | 各3°，0～30° | Light受击峰值 |
| Medium Pitch／Roll Degrees | 各6°，0～30° | Medium受击峰值 |
| Heavy Pitch／Roll Degrees | 各10°，0～30° | Heavy受击峰值 |
| 各档Rise Seconds | 0.04秒，0.01～0.3秒 | 起势时间 |
| Light／Medium／Heavy Return Seconds | 0.18／0.25／0.32秒，0.05～2秒 | 回正时间 |
| Pitch Sign／Roll Sign | +1，只允许±1 | 艺术方向调整，叠加在已校准Chassis轴／符号之上 |
| Minimum Horizontal Direction | 0.1，0.01～1 | 单位世界冲量的水平分量过小时不播放；不把垂直支撑误映射为左右回弹 |
| Minimum Vehicle Up Z | 0.5，0.25～1 | 默认约60°以上倾斜抑制受击动画，属于表现门控，不改变驾驶许可 |
| Pose Fade Seconds | 0.08秒，0.01～0.5秒 | 严重倾斜／自救时永久淡出活动事件 |
| Maximum Event Age Seconds | 0.5秒，0.05～2秒 | 迟到装饰消息的年龄保护，不是回弹持续时间 |

最终总幅度继续在 **Chassis → Composition → Total Pitch／Roll Limit Degrees** 调整，原默认各30°、允许0～45°。放大某档角度但总限幅更小时，输出仍按总限幅裁切；本轮不自动修改任何现有DA值。方向应以实际模型检查：默认局部+X受力使用正Pitch贡献，+Y使用正Roll贡献，Chassis已校准的轴／符号决定骨骼旋转方向；反向时只改对应Impact Sign，不改物理根。

## 5. 调试与手工检查

`hal.VehicleImpactVisualLog 1`记录服务器Publish和各端Consume的目标、序号、最终档位、年龄、方向、是否起播及计数，默认0关闭。AnimBP Debug Filter选择真实车辆，读取Impact Pitch／Roll Degrees及最终Chassis Rotation。

1. 球、车辆侧撞／墙、炸弹和弹簧拳都沿统一结果产生方向明确的回弹，降档后使用有效档位幅度。
2. 冲击免疫／None无规则受击回弹；0伤害或伤害免疫＋有效冲击仍有回弹。不要把自然物理滚转当成这次骨骼动画。
3. 平地停车、车顶／侧面支撑不会反复触发；自然响应足够而额外补足0的有效侧撞仍能演出。
4. 重复接触不重播，分离再撞与多个不同有效命中可独立播放、有限叠加。持续发射同时受击时，输出有界、回正后驾驶倾斜恢复。
5. 腾空有效，严重翻倒／自救平滑收回；正立后不补播旧受击。退出、换Pawn、重开、短暂停顿恢复无旧姿态残留。
6. 主机／客户端分别受击并互看，检查30／60／120实际屏幕FPS、迟到和清理。完整网络扰动、全部技能／机关组合及四台PC仍留F4，VP-4C另做A／B组合收尾。

## 6. 本轮验证

本轮证据独立留档：[VP-4B验证摘要](MVPB_Vehicle_Presentation_VP4B_Verification_20261026.json)。Saved原始日志／报告不纳入Git，以路径、SHA-256、报告计数与源码哈希供追溯。

| 检查 | 实际结果 |
|---|---|
| Editor Target | 初次完整构建成功196.61秒；修正斜向受击峰值合成及联机夹具启用时机后最终构建成功40.04秒 |
| Game Target | 最终Win64 Development构建成功111.34秒；最终构建后未再修改源码 |
| 普通回归 | `HAL.FutureCreation` **75/75成功**，0测试警告／失败／未运行；其中4项新增VP-4B测试 |
| 真实双进程 | NetTest地图，Listen Server／Client各 **1/1成功**，0测试警告／失败；60 FPS上限，主机`-PktLag=80`由日志确认 |
| 双端事件与结束 | 两端LocalStarts／RemoteStarts均恰好2、序号均2，活动包络清空且最终Chassis Rotation归零；冲击免疫不新增事件、Heavy降两档按Light发布，伤害请求为0 |
| 输出与重模拟 | Host本车／远端峰值2.990°／9.989°，Client本车／远端峰值9.993°／2.993°；客户端31次重模拟，没有产生额外回弹身份 |
| 资产与配置 | 1154份uasset／umap逐份SHA-256全同，无新增／删除；Config、uproject、模块依赖保持；专用UE进程已退出 |

普通新增测试包括30／60／120步长的解析包络、方向／符号、斜向与反向驾驶／发射合成、配置边界、事件重复／乱序／过期／序号回绕、8条有限缓存、严重姿态永久淡出／身份消费、实际CombatResolver＋Receiver的伤害／冲击免疫独立、自然补足0、普通支撑过滤、连续接触与CombatAction重复抑制，以及仅更新表现不改变物理根／速度／角速度／质量／碰撞／HP。

联机夹具使用已保存玩家BP，但只在运行中复制并启用表现DA；两车在空中分离并关闭重力，服务端通过真实统一结算入口提交自然响应已足够的接触，避免场景机关／落地和额外规则冲量影响表现计数。它证明消息和姿态数据链路，不代替正常驾驶、模型骨骼或真人画面。80 ms是主机出站注入延迟，未测量RTT，也没有注入丢包／抖动；不能等同F4网络档位或四机验收。原生30／60／120步长也不等同实际屏幕FPS验收。

现有MSVC优选版本提示、C4996弃用警告及引擎启动widget／trace警告保留；上表“0测试警告”仅指自动化报告。最新Car_01模型几何／实际AnimGraph画面、本轮启用DA后的真实球／炸弹／弹簧拳与驾驶组合仍待团队检查。当前阶段是**VP-4B代码与原生验证完成、等待DA启用及画面初测**；反馈后再进入VP-4C组合／生命周期收尾，F4按原决定留MVP-B末尾。
