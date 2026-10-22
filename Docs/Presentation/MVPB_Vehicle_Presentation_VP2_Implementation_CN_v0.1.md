# MVP-B VP-2：车身驾驶表现与配置（v0.1）

日期：2026-10-21（Asia/Tokyo）。施工基点`main/eae910e57d1b405b9a4c2e4e55a8bb38b1da7499`，UE5.8.3-58210709。开工保留前次接续核对的两份未提交文档。用户确认自救真人验收完成、功能可用、表现以后优化，并授权开始VP-2。

## 1. 范围与数据流

沿用VP-1的`UHALVehiclePresentationComponent`、`UHALVehiclePresentationConfig`和`UHALVehicleAnimInstance`，新增加减速俯仰、转弯／漂移侧倾和中断／校正处理。表现只叠加在`Chassis_jnt`，不修改车辆物理根、力／扭矩、球控制点、发射方向、HP或GAS。车轮与Chassis平级，因此不随额外车身动画倾斜。

```text
外部物理速度／位置／朝向 + 驾驶姿态许可／自救状态
→ C++世界速度差 → 投影当前前向／右向 → 加速度滤波 → 死区／角度限幅
→ Presentation.ChassisRotation
→ 原AnimBP的Chassis Transform (Modify) Bone
```

视觉悬挂为VP-3；发射／冲击／落地回弹和持续振动为VP-4后续范围。本步不增加GA／GE、物理悬挂、自然落地辅助或自动扶正。

## 2. 计算与联机边界

- 使用安全的游戏线程外部物理API读取运动，世界速度差求加速度后再投影，避免旋转局部坐标产生伪加速度。
- 先以指数时间常数滤波加速度，再做死区和角度限幅，避免60Hz物理状态在120Hz渲染下交替产生零／双倍速度差，经过提前截断而丢失一半倾斜。
- 纵向加速度为正时默认后仰、负时前倾；侧向加速度为正时默认向左外侧倾斜。倒车、滑行和漂移仍依实际运动，不以按键直接指定姿态。
- 服务器／本地拥有车读取Movement已发布的近地、驾驶许可与自救物理状态，并检查自救组件执行阶段。未知／未激活物理状态不启用驾驶倾斜；腾空、翻倒及自救时滤波到零。重新允许地面驾驶的第一样本跳过速度差，避免把落地速度变化当作驾驶加速。
- 客户端远端车不运行Movement支撑探测，因此服务器在既有ObserverSteering的15Hz发布节奏增加一个`bObserverDrivingLeanAllowed`布尔属性，SkipOwner；远端仍从实际接收的车体速度计算倾斜，不复制骨骼角度，不增加RPC。实际更新受Actor网络调度影响，远端许可可能短暂滞后；当前姿态倒扣时也会直接禁用。
- 加速度滤波状态仅属于表现，不进入Network Physics重模拟历史，不施加任何物理修正。位置相对速度预测的误差过大或速度尖峰会清零并重种车身采样；小校正仍会被滤波／限幅，不能声称能区分所有碰撞与网络校正。
- 沿用VP-1的大DeltaTime、长距离瞬移、控制者变化、重新Configure／结束清理。Chassis小距离校正重置不重置轮角；原有大距离瞬移等全表现重置保持。动画求值只读`NativeUpdateAnimation`拷贝的帧。

## 3. DA字段

位置：**当前玩家Vehicle Definition → Presentation → 实际车型表现DA（现有Car_01一般为DA_Car01_Presentation）→ Chassis**。这是车型表现配置，不是全局Vehicle Stability共享规则。以当前引用为准，不新建重复DA。

| 字段 | 默认／建议首值 | 含义 |
|---|---:|---|
| Enabled | 默认false，接线后勾选 | 兼容现有已保存资产，未启用时车轮照常工作、车身输出零 |
| Pitch Axis／Sign | Y／-1 | 俯仰骨骼局部轴与方向 |
| Roll Axis／Sign | X／+1 | 侧倾骨骼局部轴与方向；必须与Pitch轴不同 |
| Max Pitch Degrees／Max Roll Degrees | 各4° | 各通道绝对上限，允许0～30°（2026-10-22按用户要求放宽）；现有DA的实际值由团队自行调整 |
| Full Pitch Acceleration／Full Roll Acceleration | 各1800 cm/s² | 对应通道达到上限的加速度，允许1～10000；小于此值按比例响应 |
| Response Seconds | 0.15秒 | 指数时间常数，约0.45秒达到阶跃响应95%；允许0.01～2秒 |
| Acceleration Dead Zone | 20 cm/s² | 小加速度噪声忽略，超过后连续映射到满幅；必须小于两个Full值 |
| Correction Reset Distance Cm | 100 cm | 相对期望位移的误差超过此值时重种车身采样，允许1～1000cm |
| Maximum Sample Acceleration | 20000 cm/s² | 超过此幅度的速度尖峰整样本丢弃、清零；不转成受击回弹，允许1000～100000且不得小于Full值 |

默认Pitch绕本地Y负向、Roll绕本地X正向。历史Car_01只读报告的**Chassis组件空间参考旋转为单位朝向**，因此在模型仍保持+X前／+Y右／+Z上且表现相对旋转未改变时可用以上起点。它与父骨骼下的原始局部四元数不同；不要因轮子的SteeringAxis=Y就推断Chassis也绕Y进行侧倾。最后必须按实际运行模型预览确认。

字段运行前统一校验有限数值、范围、符号和不同轴；即使Enabled关闭也保留合法配置。非法表现设置走原有静态视觉回退，不把可选表现错误升级成玩法配置失效。运行时热调／换车仍不是本阶段新增能力，保存配置后新开游戏检查。

## 4. AnimBP手工配置

代码代理未编辑任何uasset／Blueprint／AnimGraph。完整构建结束后按以下步骤操作：

1. 重启Editor，打开现有`ABP_Car01`。父类保持`HALVehicleAnimInstance`，Skeleton保持`SK_Car_01`。
2. 在AnimGraph中沿用已有`Reference Pose → Local To Component → 车轮Modify Bone → Component To Local → Output Pose`。在`Local To Component`之后、`Component To Local`之前增加一个**Transform (Modify) Bone**，建议放在车轮节点前面。
3. 选中新节点，**Bone to Modify = Chassis_jnt**。只改该骨骼，不改Root_jnt。
4. **Translation Mode = Ignore；Rotation Mode = Add to Existing；Rotation Space = Bone Space；Scale Mode = Ignore；Alpha = 1**。Rotation引脚显示时，直接连接新输出。
5. 获取原生只读变量`Presentation`，Break／拆分结构体，取 **Chassis Rotation** 连接该节点的Rotation。不要把诊断用Chassis Pitch Degrees／Roll Degrees直接填入Make Rotator；C++已经完成轴向和符号映射。
6. 新结构字段未出现时，先编译AnimBP，必要时右键结构拆分／Break节点选择Refresh Nodes；重新创建该读取节点。Event Graph无需计算、积分或Tick。
7. 保持原轮子节点的轴向、空间和输入不变，Compile并Save。
8. 打开上一节指向的车型表现DA，展开Chassis，核对建议参数，勾选Enabled并Save。原Vehicle Definition和玩家BP已经引用同一DA时，不需重新绑定；只有DA此前无引用才手工补对应引用。
9. 停止旧游戏，重新Play／启动Host和Client。AnimBP Debug Filter选择实际车辆实例，观察`Presentation.bValid`、`Driving Lean Active`、`Chassis Rotation`、`Chassis Pitch Degrees`和`Chassis Roll Degrees`。

最终图示：

```text
Reference Pose → Local To Component
→ Modify Bone (Chassis_jnt, Rotation = Presentation.ChassisRotation)
→ 原前轮Steer／四轮Spin节点
→ Component To Local → Output Pose
```

若加速后车头朝下，先反转Pitch Sign；若转弯向内侧倒，先反转Roll Sign；若轴不对，在模型预览查看Chassis骨骼局部坐标后改对应Axis。符号只接受+1或-1。先小幅验证单通道，可临时把另一通道Max Degrees设0，再恢复4°。不修改物理根或整套模型相对旋转来修正某一个骨骼轴。

## 5. 团队实测清单

1. 静止／匀速时趋于中立；起步后仰，刹车前倾，倒车时按实际加减速反向；贴墙持续油门不持续后仰。
2. 左右弯向外侧倾；漂移依实际侧向加速度变化，松手后回正，未出现固定手刹姿态。车轮仍保持已校准结果。
3. 小车腾空、侧翻／倒扣、自救时附加驾驶倾斜衰减；物理翻滚可见，车身动画不抵消物理姿态。自救功能仍可用。
4. 持球、发射、加速技能、冲击后脱球无规则变化；本步不会新增独立发射／冲击弹簧回弹。
5. 主机驾驶、客户端观察，再互换；两端都看到有界倾斜，不要求逐帧角度相同。检查30／60／120屏幕FPS和暂停／恢复，保留实际构建／资产及录像。
6. 若车身不动，先查DA Enabled、实际Definition引用和Driving Lean Active，再查Chassis Rotation是否变化；变量变化但模型不动时查AnimBP节点、骨骼、Alpha、Rotation模式和空间。空中Active=false且旋转正在衰减属于预期。

动作、轴向、穿模、裁剪和手感由实际模型画面验收；原生快照或NullRHI数据测试不代替团队接线验收。10-F-4完整网络扰动／四台PC与所有组合继续留MVP-B收尾。

## 6. 编译与原生验证

**代码、构建和原生验证完成；团队DA／AnimBP接线及实际模型画面尚待验收。** 详见[验证摘要](MVPB_Vehicle_Presentation_VP2_Verification_20261021.json)。

| 项目 | 本次结果与边界 |
|---|---|
| Editor／Game Win64 Development | 均成功，最终包含网络夹具修正的Target分别5.24／9.29秒；日志Editor_AlignedFixture.log／Game_AlignedFixture.log。此前完整生产代码构建亦成功 |
| 普通自动化 | **60/60，0测试警告／失败**；原57项加3项Chassis测试，新增配置断言也通过；Automation_Initial/index.json |
| 数学／采样 | 30／60／120时间步恒定加速度、60Hz物理发布／120Hz采样、正负方向／轴映射、上限、匀速旋转无伪加速度、异常速度／位置重种和恢复门控通过 |
| 组件／边界 | 快照到AnimInstance只读拷贝、非驾驶时衰减、长帧／禁用清理、车轮通道继续有效，以及表现不修改根Transform／速度／角速度／质量通过 |
| 真实Listen Server／Client | 最终**Host 1/1、Client 1/1，0测试警告／失败**；实际NetTest、端口7811、loopback、NullRHI限帧60，无主动扰动、无实测RTT；客户端35次重模拟 |
| 资产保护 | **364→364，全部SHA256相同**；未改BP／DA／IMC／地图，Config与uproject无Git改动；最终无Editor／游戏进程残留 |

双端使用表现DA的运行实例副本临时启用Chassis，实际FVehicleInputCmd经现有输入／物理历史管线驱动车辆；不模拟物理键盘，也不修改保存的AnimBP。主机本车／对方车俯仰绝对峰值约0.570°／0.515°，客户端约0.515°／0.596°，两端均观察到本车和对方的有效驾驶倾斜。服务器随后移至空中并滚转超过侧立，双方快照均关闭驾驶倾斜并衰减到中立。此验证不能代替实际Chassis骨骼空间、节点、蒙皮与渲染观感验收。

失败与修正保留：首次Editor编译因新Chassis成员与ValidateRig原局部变量同名触发C4458，索引改名ChassisIndex后成功。首轮端口7810网络测试两端各0/1：主机车未响应，客户端也未观察到主机响应。复测诊断明确主机初始Up.Z=-1、Ground=1、Driving=0，处于倒扣，驾驶动画被正确禁用。网络夹具增加两车正立、分离、落地就绪后再发驾驶输入，原本车／远端响应及衰减断言全部保留，最终端口7811各1/1通过；无需修改生产逻辑。原失败报告保存在摘要中，不覆盖为通过记录。

普通60项已针对最终生产代码／普通测试通过；此后只修改网络夹具的初始姿态和诊断，并重新编译两Target、运行该专项双端，没有把此前报告写成再次普通复测。编译保留既有MSVC14.51新于推荐14.50和引擎C4996警告，不宣称零编译警告。日志原位在`Saved/VehiclePresentationVP2/20261021`，跨设备以已入库摘要为依据；代理未提交／Push。

## 7. 俯仰／侧倾上限放宽至30°（2026-10-22）

用户反馈前倾／后倾不明显，随后明确要求将10°配置上限放宽至30°。本次基点`main/846474d`，团队已提交VP-2代码，开工保留`ABP_Car01`及`DA_Car01_Presentation`两份资产修改。

`Max Pitch Degrees`与`Max Roll Degrees`的Editor ClampMax和运行时校验均允许**0～30°，包含30°**；原生默认值仍各4°，不自动改变现有DA数值。重启Editor后在原车型表现DA中自行设置需要的角度，保存后重新测试；原AnimBP接线沿用。两个上限分别作用于俯仰和侧倾通道，不代表合成旋转总角度被限制为30°。

如果尚未达到设定角度，继续调整`Full Pitch Acceleration`：降低它会让较小的实际加速度产生更明显的前后倾。`Response Seconds`控制滤波快慢，并同时影响俯仰和侧倾。30°实际模型姿态、穿模与观感仍由团队预览验收。

本次UE5.8.3 Editor／Game Win64 Development均成功，分别38.72／36.64秒；相关表现回归**7/7、0测试警告／失败**。更新既有测试，确认两通道30°有效、30.01°拒绝，Editor元数据范围一致，以及实际标量输出能达到30°；原七项表现检查继续通过。364份资产相对本轮开工全部SHA256相同，保留用户两份资产修改；没有重跑完整60项或双端，旧报告保持历史边界。详见[上限修改验证摘要](MVPB_Vehicle_Presentation_VP2_Limit30_Verification_20261022.json)，日志原位`Saved/VehiclePresentationVP2/Limit30_20261022`。源码／文档未提交／Push。

## 8. 团队俯仰表现初测反馈（2026-10-22）

用户确认：**“测试初步成功，能够看到俯仰变化，参数可以后续再调”**。记录为VP-2实际模型俯仰表现初测通过，参数精调后置，不要求本轮冻结最终角度、加速度映射或响应时间；保留当前DA／AnimBP接线与各30°可配范围。

本条仅表示用户明确报告的俯仰初测，不扩写为侧倾／漂移、主客互换、全部帧率、穿模／裁剪及F4完整验收通过。本轮只同步反馈和进度，没有新源码／资产修改、编译或运行测试，也不改写此前验证JSON。

下一开发入口可进入VP-3视觉悬挂的具体方案／接口准备，仍采用C++计算、分轮悬挂骨骼平移和精简AnimBP。VP-3尚未施工；四点物理悬挂、自然落地辅助和VP-4离散回弹保持独立，VP-2其余回归与参数细调继续留档跟踪。
