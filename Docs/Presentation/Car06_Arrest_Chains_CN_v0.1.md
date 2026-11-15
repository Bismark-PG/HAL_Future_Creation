# Car06：车顶交叉的立体束缚铁链

更新：2026-11-15。用户确认用两道金属链圈绕住车身、车顶X交叉，替换粉色悬浮手铐；采用柔和卡通金属、收紧／回弹／解除，不模拟真实铁链刚体。

当前已补齐独立Wire Radius及沿链圈循环流动，参数与使用方式见下表，本轮验证见第6节；第5节52项为初版历史证据。

## 1. 实现边界

- 只由 `UHALPursuitTargetComponent` 的真实逮捕状态驱动；普通电墙／其他Stun不会自动绑铁链。链条不伤害、不碰撞、不改变眩晕／保护／通缉、驾驶和球权规则。
- 默认两圈共128个链环，在一个InstancedStaticMesh组件中显示；相邻链环转90°形成扣接。两个竖直包裹平面相对车头为±36°，车顶交叉处错层，避免重叠。
- 优先从已注册 `Body` 表现部分取得车辆坐标中的Bounds，处理模型导入轴／相对缩放；使用车辆完整世界姿态，随转向、腾空和翻滚，不面向摄像机。
- 绑定是基于表现模型包围范围的近似，非逐顶点贴合／物理链。车辆形状差异可调Fit；不修改碰撞根或车型DA来适配链条。
- ArrestStart／ReleaseStart使用复制的服务器时间。晚加入直接显示当时阶段，不重播收紧。共用Stun真正结束时开始短暂松开；该尾段不延长控制限制。淘汰／Avatar退出／显式Clear立即隐藏清理。
- Tick仅在表现活跃或短解除尾段运行；Dedicated Server不创建网格。坏挂接／无资源跳过链条，不回退粉色手铐，也不影响追捕规则。

## 2. 引用及配置

沿用 `/Game/Data/Abilities/DA_Ability_Car06_PolicePursuit`，新增 **Presentation → FX → Arrest → Chains**。不需要修改Car Presentation DA、GA BP、车型Definition或关卡BP。

新增原生资源：

| 资源 | 契约 |
| --- | --- |
| `/Game/Assets/FX/PolicePursuit/SM_Pursuit_ChainLink` | 单个有孔椭圆立体链环；长轴X、环平面XY，尺寸约23.4×12.4×3.4cm，单材质槽，运行时无碰撞 |
| `/Game/Assets/FX/PolicePursuit/M_Pursuit_ChainToon` | Unlit／Masked，真实几何法线驱动暗／中／亮三档及柔和金属反光，细小遮罩渐隐；不依赖环境曝光才能读出链环厚度 |

旧Arrest Mesh／Size／Height／Pulse与手铐资产仅保留序列化兼容；新效果不使用它们。警灯、范围、音量与六星参数保持原位置。

## 3. 日常调参

所有新增控制都在同一技能DA；修改后重新释放技能，运行中的实例使用捕获快照。

| 参数 | 首版默认 | 调节用途 |
| --- | --- | --- |
| Enabled | true | 仅开关铁链，不改变逮捕规则 |
| Part Id | Body | 真实注册的表现部分；不是骨骼名 |
| Bounds Scale | (0.94,0.94,1) | 三轴贴合范围，使用车辆轴而非导入骨骼轴 |
| Center Offset | (0,0,-2) cm | 整组链圈局部偏移 |
| Vehicle Fits | Car02及默认木桩两项 | 按实际Definition引用覆盖Center Offset／Bounds Scale，适配前置驾驶室 |
| Lower／Upper Height Fraction | 0.15／1 | 在视觉Bounds的最低至最高处选包裹高度，避开轮胎底部；有顶部装饰时可微调上边界 |
| Clearance | 18cm | 链圈与选定范围之间的余量；穿车时适量增加 |
| Links Per Loop | 64 | 每圈数量，16～96且必须为偶数；越多链环越短密 |
| Cross Angle | 36° | 每圈相对车头的偏转，两圈取正负形成X |
| Profile Exponent | 4 | 2更椭圆，较大更接近圆角矩形 |
| Link Width | 18cm | 链环短轴宽度 |
| Link Overlap | 1.3 | 链环长轴与采样间距之比，避免断开或过分拥挤 |
| Wire Radius | 2.8cm | 新版材质的金属线半径，直径为两倍；独立于链环中心线大小 |
| Wire Scale | 1.25 | 旧平面厚度兼容项，放在Advanced；新版材质由Wire Radius接管截面，日常无需调整 |
| Crossing Separation | 8cm | 车顶交叉上下错层，避免两链穿插 |
| Tighten Seconds／Initial Looseness | 0.24s／0.28 | 收紧时间／初始外扩 |
| Rebound Amount／Seconds | 0.025／0.3s | 收紧后的轻微阻尼回弹 |
| Release Seconds／Looseness | 0.18s／0.18 | 解除时长／外扩，始终是纯表现 |
| Flow Speed | 45cm/s | 沿闭合链圈的线速度；0停止，负值反转。圈的形状与X交叉位置保持固定 |
| Opposite Loop Flow | true | 第二圈反向；关闭时两圈按各自路径相同参数方向运行 |
| Flow Ramp Seconds | 0.25s | 收紧完成后由静止逐渐进入流动；0为立即达到设定速度 |
| Dark／Middle／Light Color | 灰蓝金属三色 | 独立配色，不再混合警灯红蓝色得到粉色 |
| Light Direction | (0.35,-0.55,0.75) | 风格化世界打光方向，决定三档明暗 |
| Shadow／Highlight Threshold | -0.15／0.5 | 三档覆盖范围 |
| Tone Softness／Glint Strength | 0.16／0.35 | 分色过渡／柔和金属亮斑 |

**Vehicle Fits的匹配方式**：使用当前目标的Vehicle Definition资产引用，不依赖Actor名字或车型编号。Car02和 `DA_Vehicle_SkillDummy_Default` 的偏移为(50,0,-2)cm、Bounds Scale为(0.8,0.78,0.75)，将交叉点移到前置驾驶室并收小原始包围范围。每项必须填写Definition且不重复。复制Definition制作Fred木桩变体后，需在此数组加一条指向副本的配置；可以直接复制原车型的贴合数值。其他车型无匹配条目时使用上面的默认值。XY偏移会同步限制链圈宽度，避免移动交叉点后整圈继续越过整车范围。

## 4. 美术后续替换规范

可换Link Mesh／Material，保持单环长轴X、环面XY和居中原点；程序按真实Mesh Bounds适配长／短轴，环不要带碰撞或自行模拟。材质参数 `DarkColor`、`MiddleColor`、`LightColor`、`LightDirection`、`ShadowThreshold`、`HighlightThreshold`、`ToneSoftness`、`GlintStrength`、`Visibility`、`WireRadius` 必须保持对应类型。Visibility=0时必须不可见，1时完整显示。

新版原生材质使用WPO重建圆形金属截面：原生Mesh的UV0.x/y分别是沿链环和绕金属截面的0～1角度，中心线椭圆半轴10／4.5cm、源线径半径1.7cm。WPO按每个实例的变换换算成世界厘米，改变金属粗细而不扩大中心线；没有修改原Mesh资产。新美术Mesh如不满足这套几何／UV契约，需同步制作兼容WireRadius的材质，不能直接套用旧WPO公式。金属半径自动限制为链环中心线较小半轴的45%，避免小链环把孔完全填死；若WireRadius继续增加却不再变粗，先增大Link Width，或适当减少Links Per Loop使长轴有空间。此限制不改变外层车辆包裹路径。

不要在Niagara／BP里重算逮捕时长、施加Stun或延迟恢复操作；后续粒子、音效可以读同一状态作为附加表现。不要给旧Arrest Mesh重新填链条并期待它自动包裹；新接口使用单环实例化。

## 5. 验证与团队入口

接续 `main/a105c89`，实际引擎UE5.8.3-58210709。最终 **Editor BuildEditor_R6／Game BuildGame_R3** 成功。**Final_R2共52/52通过**：50项无测试警告，2项带警告，失败／未执行／运行中均0。警告为既有车型切换的配置／阻挡负例，以及D3D12资源保留虚拟大小258GB超过256GB预算；没有将带警告项目写成无警告通过。

覆盖新链圈几何、偶数闭环扣接、128个实际实例、上限192、车顶错层、服务器时钟收紧／解除／晚加入相位数学，以及木桩、追捕、Stun、Car03～05、油膜、车型切换、Impact和Health回归。真实D3D12使用实际Car02模型，等待资源编译与纹理驻留后抓取未绑／收紧／侧面／倾斜／解除／消失图像；最终链条改变29176个像素，跟随完整姿态且无碰撞，结束／Clear无残留。中途两轮专项各4/4、两轮总回归各52/52；贴合与预览资源就绪问题作为质量迭代处理，未降低断言。

真实Listen双端 **Network_R1 Host／Client各1/1**，零测试警告／失败、退出0，Host出站80ms。两端均观察到实际逮捕中的两圈铁链、共用Stun结束后的松开、保护期剩余阶段铁链已消失；复核木桩伤害／通缉／血条和销毁清理，始终仅2名真实玩家。此项使用原生测试流程，不代替完整房间BP流程。

开工1334份既有Content／地图／Config／uproject／Passive源码中，仅原Car06技能DA保存了新增Chains字段，其余 **1333份哈希一致**；旧Rules、Cooldown及全部原有FX字段逐项比较不变。新增上述两份原生资源，没有改动BP或地图，保留团队开工已有GameMode BP改动。原始报告／BMP在本机 `Saved/Car06Chains/20261115`，可同步证据见[验证摘要](Car06_Arrest_Chains_Verification_20261115.json)。未提交／Push。

团队验收步骤：

1. 完整重新打开已编译的Editor，打开原Car06技能DA，展开 **Presentation → FX → Arrest → Chains**，确认Enabled开启、Link Mesh／Material已填。资源和默认接线已保存，无需重建GA或修改车辆BP。
2. 进入原测试流程／Fred_TestScene，让Car06追捕玩家车或新技能木桩。原技能键、通缉门槛及Stun配置沿用；本次读取的既有DA为Duration8s、Radius900cm、Gain25/s、Decay40/s、Threshold100、Stun3s、Protection2s、Cooldown18s，未改这些值。
3. 观察快速收紧、车顶X交叉、侧面包裹，车辆转向／翻滚时铁链随车。Stun结束应恢复原规则许可，铁链只保留0.18s松开尾段；淘汰／退出／显式清理立即去掉链条。
4. Fred木桩若引用复制后的Vehicle Definition，在Vehicle Fits中加一条指向该副本；先复制Car02／默认木桩的Fit，再调Center Offset／Bounds Scale。以目标实际Definition为准，TestConfig包装资产并非匹配键。
5. 重点用正式俯视相机复核链环粗细、交叉点、穿模／悬空和遮挡；其他车型按需增加Fit。先改贴合配置，不改碰撞体或模型导入轴。

**仍待团队真人验收**：当前是包围范围近似，侧面与复杂车身可有间隙，不能视为真实表面贴合。自制WBP、真实晚加入、四机、完整房间、Cook／发布包均未在本轮验收；晚加入只验证时钟数学。正式场景的画风与贴合满意度仍由团队确认。

## 6. 独立线径与沿链圈流动（2026-11-15，当前）

用户实测提出铁链偏细并希望沿链方向流动，确认新增独立金属线径和循环移动。保留按Body尺寸自动适配、Car02／默认木桩已有Vehicle Fits、两圈交叉位置、眩晕与保护规则。新字段均位于原技能DA，旧参数不重置。

Wire Radius直接控制金属半径，建议先试2.8～3.5cm；Link Width继续调环的宽度。流动首测45cm/s，建议30～60cm/s，收紧后渐入，解除时减速并沿用松开消失。两圈相反方向为默认，可反转或关闭；它是链环沿路径行进，不是整组绕车顶旋转。

循环距离使用ArrestStart／ReleaseStart和当前服务器时间解析积分，再按各自链圈长度取模；不积累Tick误差，晚加入直接取得当时相位。链环朝向插值跟随路径切线，保持交错90°扣接。流动沿用已有短生命周期表现Tick，没有新增玩法Tick、物理碰撞或逐帧网络消息。

最终UE5.8.3 **Editor BuildEditor_R2／Game BuildGame_R1** 编译通过。首轮Editor因错误引用`Algo/UpperBound.h`失败，已改为`Algo/BinarySearch.h`并复编；没有将失败轮算作通过。原生材质升级正常退出，0错误／0警告；只给现有铁链材质添加线径WPO，原三阶颜色图保留，未改Mesh。

专项 **Focused_R1 5/5**、最终受影响回归 **Final_R1 53/53** 通过；最终51项无警告、2项带既有车型切换负例／D3D12虚拟资源预算警告，失败／未执行／运行中均0。包含正反流动、两圈方向、平滑启动／解除、短Stun提前解除、零速度／零渐进、超长时钟相位、独立线径不改实例中心线、晚加入相位数学及原有技能回归。

最终实际D3D12图像：0.8cm细线径覆盖13887像素，4.2cm请求（按小环安全上限钳制）覆盖34661像素，约2.5倍；改变25967个像素，链环实例变换完全一致。0.13秒流动相位变化改变20569像素，且实际实例位置确实移动，沿用128个实例、最大192的预算。检查转向／翻滚、松开和即时Clear，无碰撞／结束残留。首轮冷启动截图的车身材质尚未完整呈现，最终热加载截图已确认真实彩色Car02模型；链条专项两轮均通过，未将首轮车身预览作为最终美术证据。

80ms Host出站延迟、实际D3D12的 **Network_R1 Host／Client各1/1**，0测试警告／失败、退出0。两端确认WireRadius绑定、逮捕期间实际链环移动、Stun结束后松开、保护尾段铁链已消失，并回归木桩伤害／通缉／血条／销毁；只有2名真实玩家。

开工1336份受保护文件仅 `M_Pursuit_ChainToon` 修改，其余 **1335份哈希一致**，包括原技能DA的完整文件、链环Mesh、全部BP／地图／Config／uproject和Passive源码。新增字段使用结构默认值，保存重载确认2.8cm／45cm/s／反向true／渐入0.25s；旧Rules／Cooldown／所有FX和旧Chains字段逐项相同。团队GameMode BP已有改动保留，未提交／Push。详见[本轮验证摘要](Car06_Arrest_Chains_Flow_Verification_20261115.json)，原始报告／BMP位于本机 `Saved/Car06Chains/20261115/Flow`。

**团队入口**：完整重开本轮Editor，使用原Car06技能，无需重新接GA或BP。在原技能DA的Chains内，Geometry调Wire Radius，Animation调Flow Speed／Opposite Loop Flow／Flow Ramp Seconds；保存后重新释放技能。若想明显更粗，可先试Wire Radius3.2～3.5cm；触及安全上限时，再将Link Width适量提高到24cm，必要时将Links Per Loop从64降至48（必须偶数），为粗金属和环孔同时留空间。自动包裹和原Vehicle Fits继续使用。

真人画风、最终粗细／速度、复杂车型贴合仍待团队确认；真实晚加入、四机、完整房间BP、自制WBP及Cook／发布包未在本轮验收。前节初版证据保留为历史，当前工程证据以上述本轮摘要为准。
