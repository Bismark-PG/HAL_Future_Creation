# HAL Future Creation：项目状态与接续入口

**2026-10-23资产生成工具按车型独立Definition修正。** 本机已拉取`main/ad3e4d4`，保留云端VP-3视觉悬挂及其历史验证；开工已有前轮工具的6份源码／说明修改和1份验证留档。本轮移除Shared Vehicle Defaults输入与全部参数复制来源，从C++初始值新建`DA_Vehicle_<车型>`，将其连接到新`BP_VehiclePawn_<车型>.Definition`；LocalConfig固定引用共享`DA_Local_TestBaseline_v1`，Presentation引用本车型新`DA_<车型>_Presentation`。Network Physics继续由面板选择共享DA并连接原生Settings组件；Pawn直接继承`ATestVehiclePawn`，引用位置沿用BP_RoundedVehiclePawn。最终只创建两个DA、一个玩家BP和一个默认空AnimGraph的ABP，输出目录保持。两份新DA仍待Ready，Collision Mesh／回退Visual.Mesh及车型参数需要配置。UE5.8.3 Editor Win64 Development构建成功，三项核心测试已编译；本轮Unreal启动没有日志／报告或标准输出，不能确认测试执行，实际生成／保存重启及真人待验证。667份资产和151份运行时源码／配置逐文件SHA-256全同，Build.cs／模块入口保持；未提交／Push。详见[工具现行流程](Configuration/Vehicle_Asset_Generator_CN_v0.1.md)及[本轮验证摘要](Configuration/Vehicle_Asset_Generator_IndependentDefinition_Verification_20261023.json)。下方VP-3的5.8.2验证属于另一设备历史，本轮未调整其实现或验收范围。

**2026-10-22 VP-3视觉悬挂代码／原生验证完成，待团队AnimBP接线与画面。** 本次实际接收干净`main/faf5d067b85140ddd108969f6b2c48a0ac63b8b9`，此前资产生成工具／校准／炸弹及场地对象均已提交；本机实际UE5.8.2-56702186，旧顶部5.8.3是另一设备历史。沿用表现组件、DA及AnimInstance，新增参考轮心四路WorldStatic简单Trace、每轮世界cm压缩／伸长／平滑、法线／侧翻／自救门控及自动Bone Space平移。新通道默认关闭，未改物理根、驾驶／球／GAS／网络物理协议或保存资产。完整Editor／Game成功、普通**67/67**、实际NetTest双端**Host／Client各1/1**且均0测试警告／失败，667资产前后全同；客户端18次重模拟。双端使用临时配置及本地QueryOnly平台／关闭重力夹具，证明数据链路而非正常驾驶画面或地形复制；四车16条查询微测约19.555µs，不代表实际渲染性能。首轮C4458及65/67测试符号预期问题保留，修正后重编／完整回归通过。详见[VP-3实施／详细手工步骤](Presentation/MVPB_Vehicle_Presentation_VP3_Implementation_CN_v0.1.md)与[本轮证据](Presentation/MVPB_Vehicle_Presentation_VP3_Verification_20261022.json)。当前下一步为团队在原AnimBP增加四个Translation节点、启用车型DA并检查正常地图／双端画面；之后再讨论VP-4。物理悬挂／自然落地辅助与F4仍未施工，生成器旧人工待测项保持。本轮未提交／Push。下方各旧“当前／未施工／未提交”段保留各自时点，以本段和第2节为当前入口。

**2026-10-22资产生成工具菜单恢复，当前接续基点`main/5f6c9ef`。** 用户已拉新冲突文件，本轮在最新`HALFutureCreationEditorModule.cpp`补回资产生成工具的窗口注册、Tools菜单和关闭注销，保留云端碰撞／视觉高度校准入口。最新Build.cs已含所需依赖及平铺头文件路径，本轮保持云端内容，没有增加模块。此前两份DA／空AnimBP／新车辆BP的生成源码保留，补齐创建、保存、手工AnimGraph配置、车型／高度校准、Ready及测试接入流程。UE5.8.3 Editor Target完整构建成功，新增测试源已编译；再次尝试Unreal自动化仍未产生日志／报告，不能标记运行通过，实际菜单和生成／保存后重启仍待团队验证。本轮667份资产数量及SHA-256前后全同，保留用户SKM_Car_01修改，没有保存Content资产；未提交／Push。见[工具说明与本次验证](Configuration/Vehicle_Asset_Generator_CN_v0.1.md#5-同步后菜单恢复验证)及[摘要](Configuration/Vehicle_Asset_Generator_Reconnect_Verification_20261022.json)。下方旧基点／测试数保留各次历史含义，不代替本次检查；VP-3与F4状态保持。

**2026-10-22拉新与状态合并（当前接续基点）**：本机`main`已从`fc9f2b9`快进到GitHub最新`8571fa443dfcb880058447f95a98917fa248dd76`（新增炸弹本体与调试生成）。合并保留远端炸弹记录和本地Editor自动校准／跨设备核对记录；原有16份本地源码、文档及团队手调表现DA已备份并恢复，工具和合并后的文档仍未提交／Push。下方炸弹段“未提交”与旧`fc9f2b9`接续段均为各自施工时点，不代表本次HEAD。此次只完成Git拉新及文档合并，没有重新编译／运行Unreal或重跑测试；两项功能各自既有验证不能视为合并后集成验证。运行新增炸弹功能前，保存并关闭旧Editor／游戏窗口，关闭Live Coding后完整编译Editor Target；需要独立Game构建时再编译Game Target。VP-3尚未施工，F4继续后置。

**2026-10-22炸弹范围收敛（最新用户指令）：** 道具框架／拾取／施放尚未决定，本次删除此前新增的拾取Actor、库存组件、道具GA、UseItem输入／Tag及HUD，并还原相关车辆／Local Config／网络输入脚本。只保留实体炸弹落地倒数、独立爆炸范围及统一GAS伤害／TieredV1冲击；新增开发命令HALSpawnBomb，服务器生成一颗后立即倒数，直接放场景则仍落地才计时。测试默认3秒／500cm／25伤害／Heavy可调。基点main/fc9f2b9，本机UE5.8.2；删除后重新Editor／Game构建成功、普通63/63及7808双端指令Host／Client各1/1、0测试警告／失败；客户端HALSpawnBomb实际服务器生成、立即3秒计时／销毁／范围同步、近车扣25及Heavy、远车HP不变已验证。三项炸弹普通测试包含落地、GAS免疫／降档及真实定时器；旧拾取GA测试已移除，旧结果不作为本轮证据。团队模型／prefab接线、真人画面／手感／扰动／晚加入／四机仍未验收。入口见[本体与测试指令](Combat/MVPB_Bomb_Rebuild_CN_v0.1.md)及[验证摘要](Combat/Bomb_Rebuild_Verification_20261022.json)。未改资产或保存Config，未提交／Push；VP-3与F4保持。

**2026-10-22 Editor静态外观自动校准已实现**：用户授权后，在原Editor模块新增 **Tools → HAL Vehicle Visual Alignment**，选择实际Vehicle Definition，测量LOD0刚性轮组参考几何与真实简单凸包底面，预览绝对Z；团队点击应用，仅改表现DA整体Z，支持Undo／Redo、过期预览拒绝、共面容差及共享DA保护，手动保存。无新Runtime规则、Blueprint／地图／保存DA编辑或VP-3施工。UE5.8.2 Editor／Game Development成功，最终真实渲染工具测试**11/11、0测试警告／失败**，面板截图已检查；364份资产SHA256全同，保留团队手调DA与之前接续文档。实际Car_01当前与建议Z均-134.5，重复分析显示Already aligned，不累积偏移。首次include目录与UI测试异步选择／DPI问题已修正，失败记录保留。详见[操作说明](Configuration/Vehicle_Visual_Alignment_Tool_CN_v0.1.md)和[本轮验证](Configuration/Vehicle_Visual_Alignment_Verification_20261022.json)。尚未做真人Apply／保存、新车型画面、完整玩法60项／游戏双端／5.8.3复测或F4；下一开发仍为VP-3具体方案，当前工具代码未提交／Push。

**2026-10-22静态对齐后续反馈与自动化建议（施工前记录）**：用户确认已手动调整、表现暂时正常，并提出多车型应有自动校准。团队当前表现DA修改保留；不虚构完整姿态／双端验收。当时Editor模块只有碰撞生成，提出按车型配置分析／预览、团队应用／Undo及共享DA保护的[方案](Presentation/MVPB_Vehicle_Model_Import_And_AnimBP_Setup_CN_v0.1.md#45-editor自动校准工具2026-10-22)。此方案现已实施，以上段及工具独立说明为准；VP-3未启动。

**2026-10-22 VP-3前静态对齐讨论**：团队反馈车轮底高于碰撞代理底面、游玩悬空。只读核对当前玩家DA／代理／模型文件与已有基线相同，代理底Z=-134.5、Car_01参考轮底约0；最新表现DA的变换未重新导出，134.5 cm只在零旋转／缩放1／原轮底0的条件下作为偏移起点。建议先通过车型表现DA的Relative Transform整体Z建立轮底＝物理支撑底面的静态基准，再设计VP-3分轮视觉伸缩。269 cm代理厚度大于约195 cm视觉总高，外形合理性需另行确认，正立对齐不代表侧翻／车顶也贴合。本轮只有分析与[手工校准方法](Presentation/MVPB_Vehicle_Model_Import_And_AnimBP_Setup_CN_v0.1.md#44-静态轮底与物理支撑平面校准2026-10-22)，未改资产、重编／运行或启动VP-3，也未实现自动校准工具；此前团队“无功能问题”反馈不扩写为静态外观校准完成。

**2026-10-22跨设备接续核对：本机已收到GitHub最新`main/fc9f2b962e6f5144a822cbf7d22b32114583ac42`，开工工作区干净，无需再次Pull。** 相较本会话原`fd41bbb`，已纳入车辆共享物理材质／规则、普通静止Movable地面支撑追加过滤、姿态驾驶门控、服务器长按自救及Local Config输入接线、VP-2车身俯仰／侧倾和各30°配置上限。`ABP_Car01`与`DA_Car01_Presentation`已随最新提交到达，不再把旧“未提交／尚未接线”作为当前状态。按各施工记录先后叠加清单核对155份源码／配置，50份字节完全一致、105份仅LF／CRLF差异，无其他差异；现有364份资产，AnimBP与30°快照一致，表现DA已是团队后续保存并提交的新版本，保留且不从旧快照回填。详见[本次接续复核](Presentation/MVPB_Project_Handoff_Review_20261022.json)及[VP-2接续说明](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md#9-跨设备接收与当前提交2026-10-22)。

当前开发入口仍为**VP-3视觉悬挂的具体方案／接口准备，尚未施工**；VP-1结束、自救基本功能真人确认、VP-2俯仰真人初测通过及参数后调保持。侧倾／漂移、完整画面／角色／FPS组合继续跟踪；VP-4、四点物理悬挂与自然落地辅助未实现，F4完整扰动／四台有线PC验收继续后置。本机实际UE**5.8.2-56702186**，本次收到的新施工验证使用另一设备UE**5.8.3-58210709**；后续联机前应统一补丁和构建。本轮只读核对并更新文档，未新编译／运行Unreal／自动化／读取最终DA字段或AnimGraph，不把另一设备60项、双端各1项及30°专项7项报告当成本机新复测，也未开始下一功能或提交／Push。下方各段“未提交”等说明保留各自历史时点，当前提交状态以本段及第2节为准。

**2026-10-22团队反馈：VP-2俯仰表现真人初测成功，参数后续再调。** 用户明确确认已能看到俯仰变化；记录为该通道基本表现链路初测通过，不再将“尚未接线／看不到俯仰”作为当前阻塞。各30°可配上限和现有DA／AnimBP保留，参数尚未冻结。详细见[团队反馈](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md#8-团队俯仰表现初测反馈2026-10-22)。下一开发入口可进入VP-3视觉悬挂的具体方案／接口准备，尚未施工；侧倾／漂移、双端角色及完整画面矩阵没有本次逐项反馈，不扩写为VP-2或F4全部验收。本轮仅更新文档，保留`main/846474d`下既有源码／测试／两份资产修改；没有新构建或运行，未提交／Push。

**2026-10-22 VP-2调参补充：俯仰／侧倾各通道配置上限由10°放宽到30°。** 用户反馈实际前后倾不明显并明确要求放宽；本轮基点`main/846474d656b11648becff51545e8e4216f949dd5`，VP-2代码已提交，保留团队`ABP_Car01`与`DA_Car01_Presentation`两份未提交修改。同步Editor ClampMax与运行时校验，默认值仍各4°，DA实际数值不改。UE5.8.3 Editor／Game成功，相关表现回归**7/7、0测试警告／失败**，364资产相对本轮开工SHA256全同。团队继续在原DA调参、做实际模型画面验收；本轮未重跑完整60项或双端，也未宣称30°观感／穿模通过。[配置及本轮记录](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md#7-俯仰侧倾上限放宽至302026-10-22)／[验证摘要](Presentation/MVPB_Vehicle_Presentation_VP2_Limit30_Verification_20261022.json)。本轮修改未提交／Push。

以下10月21日记录保留各自施工时点；当前提交状态、参数范围与本轮验证以上段为准。

**2026-10-21 VP-2代码与原生验证完成，待团队DA／AnimBP接线及画面验收。** 用户已确认“自救真人验收已经完成，功能可以工作，表现可以后面再优化”，自救基本功能不再列当前待办；本次随后获授权实现VP-2。基点`main/eae910e`，保留前次两份未提交接续文档，未改Unreal资产。新增世界加速度滤波驱动Chassis俯仰／侧倾、车型DA调参、空中／翻倒／自救衰减及校正重种；远端复用15Hz节奏复制一个表现许可bool。UE5.8.3 Editor／Game成功，普通**60/60**、最终双端**各1/1**通过，364资产SHA256全同。原网络首测因主机初始倒扣未产生驾驶表现失败，夹具正立接地就绪后严格复测通过；失败证据保留。无本次AnimBP节点、渲染／真人VP-2或F4最终验收。下一步按[VP-2配置与手工步骤](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md#4-animbp手工配置)在原车型DA启用Chassis，并增加一个Modify Bone节点；[验证摘要](Presentation/MVPB_Vehicle_Presentation_VP2_Verification_20261021.json)。未提交／Push。

以下VP-2接续核对、输入修正和自救施工记录保留各自时点，当前阶段与真人反馈以上段为准。

**2026-10-21 VP-2接续核对：下一开发环节为VP-2车身驾驶表现，当前完成范围核对，尚未开始VP-2代码施工。** 当前为`main/eae910e57d1b405b9a4c2e4e55a8bb38b1da7499`，检查开始时工作区干净；该提交已包含Self Right的Local Config字段／初始化复制、配置回归、团队Local DA／共享DA／IA／IMC。此前“修正未提交”不再是当前状态，远端Push未核验。VP-1结束、车轮恢复正常及支撑修正正向反馈保持；姿态门控／自救已有构建与自动化证据，但尚无输入修正后的真人验收反馈。本次仅核对代码与文档，没有新构建／运行，不追加测试通过结论。VP-2具体范围见[接续范围](Presentation/MVPB_Vehicle_Presentation_Rig_Plan_CN_v0.1.md#111-vp-2接续范围2026-10-21)；自救实测作为独立待办保留，可与表现准备并行。

以下输入修正及自救记录保留各自施工时点，提交状态以本段为准。

**2026-10-21输入入口修正：BP灰色Input是Definition模式的预期锁定，实际填写位置为玩家Vehicle Definition引用的Local Config DA → Input → Self Right Action。** 上次自救实现漏接该DA字段及初始化复制，且配置文档误写为在BP填写，现已补齐并修正文档；不切Legacy、不解除旧字段锁定。本次基点`main/c3f90ca`，Editor／Game成功，普通回归重新57/57、0测试错误／警告；新增Local可选／bool独立性、初始化前复制、旧Pawn字段锁定／Local可编辑及三个Enhanced Input绑定断言。保留团队共享DA／IMC／新IA，364资产前后全同。尚无本次修正后的真人输入或新双端验收；下文三组网络结果属于前次施工证据。[正确配置](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md#4-独立长按输入editor手工步骤)／[补充验证](Movement/MVPB_Vehicle_SelfRight_InputDefinitionFix_Verification_20261021.json)。未提交／Push。

**2026-10-21 当前进度：VP-1保持结束；用户实测确认支撑修正后“弹动有效减少”，姿态操控限制＋长按自救已完成代码、构建与自动化，待团队手工Local Config输入接线及真人验收。** 姿态／自救原施工基点为`main/f634292`，此后团队已提交为`c3f90ca`；最新输入接线补充以上文为准。新增共享DA的Recovery组、物理姿态门控、服务器长按许可、有限翻正、球／技能执行期限制及网络物理动作／历史。详见[实现与详细配置](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md)、[本轮验证](Movement/MVPB_Vehicle_Pose_And_SelfRighting_Verification_20261021.json)。当前修改未提交／Push；上一轮53项及各历史证据不改写为本轮结果。

已确认规则现已实现：近地持续低速侧翻1秒、线速≤150cm/s／角速≤60°/s，合格后长按1.5秒；松手或条件失效清零。有限翻正最多2秒，执行允许滚转短暂离地，成功仍需近地且姿态／角速达标；外部有效Medium／Heavy打断，执行结束后冷却3秒。实际接受执行时Free脱球、无额外释放冲量，执行中禁接球／发射／新技能，既有效果自然计时。规则在同一共享DA可调并纳入World冻结快照／签名；单纯侧翻和长按阶段保持原发射／技能。团队须手工创建独立IA_SelfRight、映射建议R，并在玩家Definition引用的Local Config DA中填写Self Right Action，不新增GA／GE或Event Graph。

本轮UE5.8.3 Editor／Game Development均成功；最终普通**57/57**、0测试错误／警告；新增真实Solver5工况／900步通过。实际MVPB_NetTest的30／60／120 NullRHI限帧双进程均各**1/1**，客户端重模拟分别29／44／61次；主机单向注入80ms，非实测RTT，临时签名2346082778一致。正式保存DA在普通回归的新签名915860017与临时资产路径不同，不跨夹具比较。363资产相对开工全同、Config未改。首次反射／测试编译问题、Editor占用、短暂离地过早退出及网络夹具启动同步问题已修复并保留失败证据。没有本轮真人接线／画面／几何手感或F4四机验收。

以下2026-10-20说明保留各次工作的历史基点与证据，不能将其中的“当前工程”当作10月21日HEAD。

2026-10-20车辆碰撞／控球点团队反馈：用户在调整并测试后确认“暂时没有功能性问题”。当前工程为干净`main/fd41bbb`，该提交包含生成器修复及验证留档、玩家Vehicle Definition／MVPB_NetTest更新和新`SM_Car01_Collision1`资产。本次把该反馈记录为团队基础功能初测未见问题；未提供逐项清单、双端／网络条件、四机或新日志，不扩写为完整验收。代理本轮仅更新文档，未重新编译、运行Unreal或导出最终资产数值；上次269 cm／Z92／旧DA引用是诊断时点数据，不代表团队调整后的基线。详见[操作文档第9.4节](Configuration/Vehicle_Collision_Generator_CN_v0.1.md#94-团队调整后测试反馈2026-10-20)。

2026-10-20车辆碰撞工具修复与出生排查（当前会话）：从`main/7d0f180`接续，团队已提交`SM_Car01_Collision`。修复生成器可视三角形绕序与UE约定相反造成的缺顶面外观，凸包顶点／物理规则不变；旧网格须读取配方生成新版本，参数Version1兼容，新结果记录GeneratorRevision2。UE5.8.2 Editor／Game Development成功，真实渲染工具回归6/6通过、0测试错误／警告，实际缩略图已检查。原日志明确出生碰撞拒绝于(-390,-1060,92)；新代理456×288×269 cm、起点Z92与地面高度不匹配，独立物理世界验证Z92重叠／Z200避开。保存玩家DA却仍引用旧200×140×70代理，独立当前保存配置Listen启动成功生成Pawn，故不能把原PIE内存配置等同保存基线。354份资产哈希未变，代理未改BP／DA／地图；待团队手动重新生成、接DA、核对四起点高度及球控制点，并做新代理真人双端回归。详见[操作与排查第9节](Configuration/Vehicle_Collision_Generator_CN_v0.1.md#9-缩略图面朝向修复与出生碰撞排查2026-10-20)和[本轮验证摘要](Configuration/Vehicle_Collision_Generator_WindingFix_Verification_20261020.json)。以下弹簧拳等其他工作记录保持。

2026-10-20弹簧拳第二步（最新）：已授权并完成阶段/起始时间/时长/距离/原点/拳次的RepNotify快照、客户端服务器时间插值与旧BP非复制标记兼容；权威命中不变，不新增比赛阶段门控。UE5.8.2 Editor/Game成功，专项1/1，普通回归49/50（唯一仍为Car_01接线后的旧无Rig假设）。真实正常时间60FPS、端口7799双进程以原生拳+现有BP混测，Host/Client各1/1通过，双方观察两拳伸出/收回且两玩家HP75/Medium，无客户端Action。原生初测通过；加速时间混测曾超时，改正常时间后相同严格断言通过，失败证据保留。不代表画面/手感、实际晚加入、网络扰动、持球/免疫/加速组合或四机完成；详见[同步摘要](Combat/SpringPunch_Replication_Verification_20261020.json)与[交接](Combat/MVPB_SpringPunch_Combat_Handoff_CN_v0.1.md)。保留团队4份资产改动，代理未改资产、未提交/Push。

2026-10-20弹簧拳屏幕扣血调试：新增BP默认开关Debug Damage On Screen，实际扣血时黄色文字显示6秒，包含拳头/目标名称、HP前后、实际扣血、有效冲击档位及Applied状态；只读展示，不改变结算，不额外向远端复制。保留团队DrivingLab GameMode/DA/地图改动。用户关闭Editor后，UE5.8.2 Editor/Game Development均构建成功。最新模块弹簧拳专项1/1通过、0测试警告/失败，报告Saved/SpringPunch/ScreenDebug_20261020/index.json。此为无视窗结算回归，尚未验证实际文字渲染及开关画面；前次49/50不冒充本轮结果。

2026-10-20 DrivingLab不能操控诊断：当前15:20:05本地Editor日志显示BP_DrivingLabVehicle_C_0使用DA_DrivingLabVehicle后报`TieredV1 rules require migrated VehicleDefinition.Impact`，随后物理模拟与控球关闭。源码PreInitializeComponents在配置校验失败时禁用模拟/碰撞，输入初始化也跳过；本次证据指向DrivingLab专用DA未迁移Impact，不能以更换PlayerState解决。团队须手动在DA顶层Impact将Version设TieredV1，保存后重新Play；代理未改资产，保留团队DrivingLab_Test.umap修改。本轮只读诊断，尚无修正后复测。HALGASStatus由HALPlayerController提供，普通DrivingLab控制器不保证支持，不能单凭命令无输出判断GAS缺失。

2026-10-20弹簧拳第一步施工：用户授权接入统一战斗结算，已新增可调25伤害/Medium测试默认值、独立机关SourceTag、每拳Action快照、固定出拳方向ExplicitTier、接受后去重与阶段/销毁清理；保持原时序及仅Extending车辆检测，不恢复炸弹、不新增联机表现复制。专项测试代码覆盖重复/多目标/下一拳、两类免疫、HP0/零伤害、拒绝后重试与Sweep。用户关闭Editor后，UE5.8.2 Editor/Game Win64 Development均构建成功。完整普通自动化49/50：新增SpringPunch.CombatLifecycle通过；唯一失败Presentation.OptionalRigPreservesGameplay仍假设保存玩家资产未选骨架Rig，与已接入Car_01的当前资产不符，本轮不修改表现测试/资产。没有本轮真人或Listen Server双端受击验收，联机阶段表现仍未实施。报告Saved/SpringPunch/Combat_20261020/index.json，共享证据见Docs/Combat/SpringPunch_Combat_Verification_20261020.json。详见[交接文件新增第一步](Combat/MVPB_SpringPunch_Combat_Handoff_CN_v0.1.md)，本轮未改资产或提交。

2026-10-20本机接续分析：实际为干净`main/a11228a`，团队Car_01接线资产已提交。用户说明已discard旧炸弹；本次检索Source／Config／Docs未发现HALBombActor、HALUseBomb、HALBombTest或PrepareStandaloneCombatTarget，旧炸弹验证不适用于当前源码。相较f4b17d2已纳入10-D物理线程修复、10-E免疫技能、10-F运动桥接及加速GA／GE、补球调试与VP-1表现底座。此设备`D:/Unreal Shit/UE_5.8`实际是5.8.2-56702186，与下文另一设备5.8.3历史环境不同；49项及双端结果均为既有记录，本轮仅核对Git、源码、配置与验证摘要，没有新编译、Unreal运行、资产字段导出或新验收。只补本接续说明，未恢复炸弹、修改玩法或资产；F4后置与表现待测事项保持。

最后更新：**2026-10-22（Asia/Tokyo）**。本文件维护最新进度、已确认决策和后续入口；详细规则、资产数值和验证证据由链接文档提供。10-F-1～3及加速GA、团队接线和基础双端实测均完成，相关资产和复核已提交`c825d8d`。**用户已决定将10-F-4完整集成／扰动／四机测试后置至MVP-B收尾，当前暂不施工F4-A；后续开发按用户指定事项推进。** 最终验收未完成，后续各项开发仍保留相关编译／自动化及必要双端基本检查。

当前**VP-2俯仰真人初测通过，参数精调后置**；下一开发可进入VP-3视觉悬挂方案／接口准备，尚未施工。各30°可配范围保留，侧倾／漂移、双端及完整画面回归继续跟踪，不据本次反馈写成全部验收。支撑过滤正向反馈、车轮正常及自救功能真人验收保持，自救表现以后优化。具体见[实施与团队反馈](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md)。VP-4与自然有限落地辅助尚未实现。

## 1. 先读什么、先检查什么

1. 完整阅读根目录 [AGENTS.md](../AGENTS.md) 和本文件。
2. 按用户当前指定的后续功能读取相关规格；涉及GAS／运动时先读[加速GA及团队反馈](Combat/MVPB_Phase10F3_Movement_Ability_CN_v0.1.md)、[手动日志](Combat/MVPB_Phase10F3Ability_ManualLogReview_20261019.json)、[施工验证](Combat/MVPB_Phase10F3Ability_Verification_20261019.json)、[F2桥接](Combat/MVPB_Phase10F2_Implementation_CN_v0.1.md)、[F1规格](Combat/MVPB_Phase10F1_Spec_CN_v0.1.md)与[保存基线](Combat/MVPB_Phase10F1_Baseline_20261017.json)。F1～F3及团队接线／基础反馈完成，331资产已提交；[F4方案](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)与[总计划](Combat/MVPB_Phase10F_Movement_Effects_Plan_CN_v0.1.md)保留，但A～D整体按用户决定后置至MVP-B收尾，不能沿旧入口自动开工。10-D／10-E欠项见各自[施工记录](Combat/MVPB_Phase10D_Implementation_CN_v0.1.md)、[实施](Combat/MVPB_Phase10E_Implementation_CN_v0.1.md)及[手动日志](Combat/MVPB_Phase10E_ManualLogReview_20261016.json)。规则背景见[GAS总体方案](Combat/MVPB_GAS_Architecture_Overview_CN_v0.1.md)，双端启动见[操作说明](Configuration/MVPB_DebugHUD_And_GAS_Test_CN_v0.1.md)，DA职责见[速查](Configuration/MVPB_DataAsset_QuickGuide_CN_v1.0.md)。
3. 以 [MVP-B Listen Server v0.2 计划](MVP_B_Listen_Server_Plan_CN_v0.2.md)和对应 `Docs/Networking/` 实施记录确认已存在的网络边界。早期 [MVP-A→MVP-B 交接](MVP_A_to_MVP_B_Handoff_CN_v1.0.md)用于追踪背景，不能把旧参数当成当前 DA。
4. 在本机执行 `git status --short`、`git branch --show-current`、`git rev-parse HEAD`，对照 `.uproject`、模块 `Build.cs`、`Source/`、`Config/`、相关测试和邻近说明；以 Editor 的 About 或引擎 `Engine/Build/Build.version` 核对实际补丁版本。
5. 修改前先汇报当前工程与本记录的差异和本次任务范围；保留已有改动。不因历史记录写了“下一步”就自动施工未获用户授权的环节。

车辆接续先读[VP-3实施／手工接线](Presentation/MVPB_Vehicle_Presentation_VP3_Implementation_CN_v0.1.md)及[本轮摘要](Presentation/MVPB_Vehicle_Presentation_VP3_Verification_20261022.json)，既有Chassis节点与调参见[VP-2](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md)。物理背景见[姿态／自救](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md)、[已确认计划](Movement/MVPB_Vehicle_Stability_And_SelfRighting_Plan_CN_v0.1.md)、[支撑分类](Movement/MVPB_Vehicle_Support_Classification_Fix_CN_v0.1.md)及[共享DA／PM](Movement/MVPB_Vehicle_Stability_MinimalFix_CN_v0.1.md)。保持Movable普通地面、共享规则和静态高度校准；VP-3只消费参考几何并输出骨骼位移，团队增加四悬挂节点。自救许可／回滚和副作用边界、VP-1结束及F4后置保持。

## 2. 当前工程位置和开发阶段

| 项目 | 最新接续核对结果与既有团队反馈 |
|---|---|
| 开发阶段 | VP-1结束，自救基本功能真人验收完成；VP-2俯仰初测成功、参数后调。VP-3代码／原生验证完成，当前团队四轮悬挂节点及画面待验收；之后VP-4另行讨论。物理悬挂／自然有限辅助与F4仍待做 |
| 当前本机分支／代码提交 | `main / ad3e4d4`，已接收云端VP-3提交；开工保留前轮工具源码／说明／验证留档。本轮车型独立DA修正与状态更新未提交／Push |
| 引擎基线 | 本机实际UE `5.8.3-58210709`，本轮工具Editor构建成功；核心测试已编译但无运行日志／报告，实际执行待验证。旧VP-3的5.8.2两Target／67项／双端各1项保留历史含义，不写成本轮复测；联机前统一补丁／构建 |
| 本机运行前置 | 使用本轮完整Editor构建后重新启动工具。新车型填写Collision Mesh／回退Visual.Mesh、完成动画与参数校准后启用两份DA。VP-3原车型仍按其文档接四个Translation节点并启用Suspension；旧转向／轮转／Chassis数值保持 |
| 工程绑定 | `.uproject` 的 `EngineAssociation="5.8"`，不代表所有电脑的补丁版本相同 |
| GAS 当前实现 | `HALMovementBoostAbility`，ServerOnly施加可配非周期GE后立即结束，无默认费用／冷却，独立于debug；5游戏秒、+0.5／+0.25、按目标一层刷新及Avatar作用域。团队已在原Primary替换为加速GA；原免疫GA／GE／冷却及调试入口保留 |
| 当前 HP／冲击 | 10-C伤害／快照保留；10-D同帧Internal读写已有28项自动化证据，用户2026-10-16确认击飞效果恢复。保留现有档位、水平补足和DA，各档实际响应后续调整；本次反馈未列双向角色／扰动明细 |
| 当前 Editor 接线 | 原有已保存资产保持；资产生成器当前只输入Mesh／共享Network DA，自动连接共享Local和车型独立Definition／Presentation，生成两个DA／一个BP／一个空ABP。生成／保存重启及真人待验收；VP-3原车型四个悬挂节点及DA Enabled仍需团队配置 |
| 测试地图 | 单机回归 `/Game/Maps/Test`；联机玩法 `/Game/Maps/MVPB_NetTest`；完整菜单 Host／Join 从 `/Game/Maps/MVPB_NetEntry` 启动 |

代码提交号是本次交接的比较基点，后续提交会变化。F1保存基线及F2历史证据保持；10月17日F3的原生GE／调试入口证据见原摘要。本轮运动GA补充不改保存配置、DA／地图或F2物理协议；329资产前后哈希全同，134份源码／配置及最终两Target／45项／正常GA双端证据见[本轮摘要](Combat/MVPB_Phase10F3Ability_Verification_20261019.json)。首次Live Coding阻塞、补Period防护前的通过记录独立保留；历史结果不冒充本次复测。

上段329资产是代码施工时的保护结果。随后团队新建2份加速资产并修改PlayerState，本次已记录331份资产及3项变化哈希，134份源码／配置未变化；新结论见[手动复核](Combat/MVPB_Phase10F3Ability_ManualLogReview_20261019.json)。本次没有重跑构建／45项自动化，也不把团队基本双端反馈写成F4完整验收。

随后c825d8d已提交上述3项资产和手动复核文档；不再把“资产未提交”作为当前阻塞。新的[F4方案](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)只记录待执行矩阵、分工和门槛，不代表F4测试已完成。

## 3. 已完成与验收状态

| 环节 | 已有结果 | 证据边界 |
|---|---|---|
| MVP-A 基础玩法、MVP-B DA 迁移 | 用户报告基础玩法及 DA 更新后初测无功能问题；保存的测试参数继续使用 | 危险区、模型／Rig 表现原已延期；早期迁移表不能覆盖新 DA |
| Listen Server 阶段 1～8 | 已有服务器权威输入、车辆 Resimulation、远端 PI、球状态／控球表现、发射后坐及退出清理；用户报告对应双人功能测试暂未见问题 | 各自证据见 `Docs/Networking/`；用户反馈、自动化测试和量化网络门槛分别记录，不能互相替代 |
| 阶段 9 菜单与四机验收 | Host／Join、主机 Start、诊断代码与资产已存在；用户报告线下双人测试暂未有其他问题 | [阶段 9 记录](Networking/MVPB_Listen_Phase9_Implementation_CN_v0.1.md)保留未关闭事项；尚无四台真实有线 PC 最终验收结果／记录，不标记第一里程碑全部通过 |
| 10-A | 规格、保存 DA 数值／哈希、总体说明及跨设备入口完成；本次复核 14/14 基线文件未变化 | 文档和只读检查环节；未接入 GAS，未修改玩法源码或资产 |
| 10-B | 两个 Development Target 编译、自动化 16/16、Test／NetTest 保存接线确认；双向命中 HP 日志复核通过。2026-10-14 用户确认驾驶／持球／冲击测试完成，并同意进入 10-C | 新结论来自团队反馈，测试日志尚未提供／可能未上传；未说明测试补丁版本、逐地图／被动车子项及约 100 ms RTT／丢包／抖动工况，不把这些细项写成已有日志证明 |
| 10-B 调试显示补充 | 新增 C++ 半透明只读 HUD，`hal.DebugHUD 1/0`；`HALGASStatus` 增加 PlayerId；最终两个 Target 编译、既有自动化 16/16 及双端初始数值／实际渲染检查通过 | 自动化在最终字号微调前执行，微调后重新编译并看截图；不改玩法或资产。后续驾驶／持球／冲击回归已有团队完成反馈，具体延迟工况和 HUD 观感未单独留档，详见操作说明和 [HUD 验证摘要](Configuration/MVPB_DebugHUD_Verification_20261013.json) |
| 10-C | 共用Action／请求／结果、快照、战斗Attribute和独立伤害完成；原UE5.8.3两个Target／22项自动化通过。用户2026-10-15报告另一设备已完成相关资产更新和测试，三个DA迁移已提交 | 10-D本轮只读导出确认三份Damage/Combat配置为AttackSnapshotV1，并验证当前代码下的初始HP复制；没有重做10-C真人扰动验收，不覆盖用户已完成反馈 |
| 10-D | 全来源档位、GAS降档／免疫、统一脱球及动作、DA迁移和线程修复已提交；上一轮两个Target／28项自动化通过。用户本轮确认击飞效果恢复，接受先保留档位、实际力度以后调整 | 本轮是团队功能反馈，未提供新日志或角色／网络条件细项。其余三档／抗性／连续接触及扰动回归仍待补齐，不写成完整验收；5.8.3未复测 |
| 10-E | 原生GA／GE、授予／清理、输入／调试完成；原Editor／Game、33/33普通回归与真实激活smoke通过。团队接线已提交，报告双端免疫开始／消失及效果正常；本轮两局五次命中确认双方受击、免疫Heavy→None且伤害25、HP收敛，客户端目标后续命中恢复Heavy | 手动日志未启用hal.AbilityLog，缺精确8／20计时／重复请求／实际GE类名记录；团队一般观感与日志子项分别留档。按住输入、目标持球保球、活动效果退出／换Avatar、100ms RTT／1%丢包／20ms抖动及四机未有本次新实测证据，未新增正式技能演出 |
| 独立调试补球 | 新HALSpawnBall由服务器按调用者车辆生成配置的BP_BasicBall；Editor／Game和29/29自动化通过。实际NetTest主机执行成功、客户端接收动态球通过，324资产前后未变 | 客户端实际输入命令发起RPC、驾驶捕获／发射及真人观感待团队反馈；无网络扰动测试，不算10-E技能验证，见独立验证JSON |
| 10-F | F1／F2规格、基线、桥接及F3正式GE／调试入口完成；原两Target／42项／80ms调试GE证据保持，见[F3实施](Combat/MVPB_Phase10F3_Implementation_CN_v0.1.md)及[验证](Combat/MVPB_Phase10F3_Verification_20261017.json)。加速GA追加证据及团队反馈见下一行 | F3状态共存和基本双端反馈不是完整物理组合；F4驾驶／控球／发射／冲击／免疫／退出、完整扰动与四机待完成。F2回滚驾驶补充回调仍需覆盖组合体验 |
| 10-F-3技能补充 | 加速GA、Tag、可配非周期GE／许可／刷新、3项新增普通测试、真实GA双端夹具及配置步骤完成；之前Editor／Game、45/45及双端通过。团队接线与基础双端反馈／日志复核通过，c825d8d已提交；主机／客户端各4次激活、4段效果到期恢复正常 | 完整输入边沿／发射／冲击／免疫／带GE退出／换Avatar及F4网络矩阵仍待覆盖；见[详细记录](Combat/MVPB_Phase10F3_Movement_Ability_CN_v0.1.md) |
| 10-F-4准备／后置 | [F4执行方案](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)已留档，HALNetMetrics缺RTT等诊断亦列待补；用户决定整个A～D在MVP-B末尾统一执行，当前暂不施工 | 完整集成／网络扰动／四机仍未验收；旧45项／80ms单向smoke不代替F4。后续开发继续相关编译／自动化／必要双端验证，阻塞缺陷及时修复 |
| 独立弹簧拳接入分工 | 已接统一CombatResolver伤害／档位、每拳去重和阶段RepNotify／服务器时间插值；此前专项与基本双端证据见[既有摘要](Combat/SpringPunch_Replication_Verification_20261020.json)。后续仍由另一成员负责，见[交接](Combat/MVPB_SpringPunch_Combat_Handoff_CN_v0.1.md) | 本次仅读码确认已有接入，不修改机关或重跑验证；不再将其写成尚无战斗／复制。表现手感、实际晚加入、完整组合／扰动及四机验收边界保持 |
| 炸弹本体与开发生成指令 | `8571fa4`已接收实体炸弹、独立爆炸范围、统一GAS伤害／TieredV1冲击和HALSpawnBomb；此前两Target／63项及双端各1项证据见[实施](Combat/MVPB_Bomb_Rebuild_CN_v0.1.md)及[摘要](Combat/Bomb_Rebuild_Verification_20261022.json) | 道具框架／拾取／施放尚未决定，不恢复已移除的库存／道具GA／输入；模型prefab、真人手感、晚加入／扰动／四机仍待验收，本次未做合并后复测 |
| 车辆表现VP-0／VP-1已结束 | 原生实现及10月19日数据双端历史证据保持，素材／接线已提交，用户确认VP-1收尾；随后车轮配置已修复、表现恢复正常的反馈保持。旧无Rig测试已修复，见[收尾](Presentation/MVPB_Vehicle_Presentation_VP1_Implementation_CN_v0.1.md) | 不重开已解决的轮轴／半径排查，不虚构最新保存数值；完整扰动／四机未验收。VP-2以新行和摘要为准 |
| VP-2车身驾驶表现 | C++与原生验证完成，30°范围补充两Target／7项回归通过；用户确认实际俯仰初测成功，参数以后调整，见[反馈](Presentation/MVPB_Vehicle_Presentation_VP2_Implementation_CN_v0.1.md#8-团队俯仰表现初测反馈2026-10-22)。此前60项／双端历史证据保留 | 真人反馈限俯仰可见，不追加侧倾／漂移、穿模／裁剪或完整双端／屏幕FPS结论；参数未冻结。VP-3以新行记录为准 |
| VP-3视觉悬挂 | 四轮参考探测、行程／平滑、法线／姿态／自救过滤、自动Bone Space偏移及诊断完成；本轮两Target、普通67/67、双端各1/1，见[配置与验证边界](Presentation/MVPB_Vehicle_Presentation_VP3_Implementation_CN_v0.1.md) | 当前默认关闭，团队四个Translation节点与正常地图／双端画面待验收；本轮无渲染、5.8.3、F4扰动／四机测试，物理悬挂未实现 |
| Editor静态轮底校准 | 自动测量／预览、显式应用、事务撤销及共享／过期保护完成；本机两Target／11项工具回归通过，见[操作说明](Configuration/Vehicle_Visual_Alignment_Tool_CN_v0.1.md) | 只处理参考姿态与正立基准，无运行时校准或碰撞拟合；团队实际Apply／保存、新车型及各姿态画面仍待反馈，不替代VP-3／完整玩法矩阵 |
| Car_01素材检查 | 2e4176d新增20份素材全部可加载；17骨骼直接父级符合约定，5823源顶点均单骨骼权重1，四轮绑定各自spin；未见缺失依赖，351资产未改，见[检查](Presentation/MVPB_Car01_AssetInspection_CN_v0.1.md)／[原始数据](Presentation/MVPB_Car01_AssetInspection_20261020.json) | 仅NullRHI只读几何／资产数据检查；无真实AnimGraph姿态／材质渲染／性能／玩法双端验证。1个LOD／12个材质section留待实际四车测量，PA保留但不作为当前车辆物理根 |
| 车辆反弹／姿态门控／长按自救 | 共享规则启用，支撑修正获正向反馈；自救代码／输入链路及原生验证保持，用户现确认真人验收完成、功能可工作；见[最新反馈](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md#8-团队真人验收与vp-2接续2026-10-21) | 自救表现以后优化；未追加完整攻击组合／网络扰动／四机结论。有限自然落地辅助未实现 |

本次 10-B 构建证据：UE 5.8.2 的 Editor/Game Development Target 均成功，`HAL.FutureCreation` 自动化 16/16 通过，报告原位于 `Saved/Automation/MVPB_Phase10B_Final/index.json`；隐藏双进程 Join succeeded，客户端收到两辆车的 GAS HP=100／MaxHP=100。历史阶段 9 的 14/14 结果保留在对应记录中。本次没有真人驾驶／命中及延迟测试。`Saved/` 不随 Git 交接，新电脑不存在这些报告不代表测试失败，也不能声称已重跑；共享摘要见 10-B Verification JSON。

上述是原施工验证的边界。团队随后进行了真人双向命中测试，本次已核对双方日志的三次 HP 变化，见 [HP 实测日志复核](Combat/MVPB_Phase10B_ManualHP_LogReview_20261013.json)和 [10-B 实施文档第 5.1 节](Combat/MVPB_Phase10B_GAS_Foundation_CN_v0.1.md)。另只读复核 [团队接线后的两个地图](Combat/MVPB_Phase10B_EditorBindings_PostTeam_20261013.json)，319 资产检查前后未变。此次没有新编译／新跑自动化，不将历史报告当作本次复测。

2026-10-14 接续补充：用户明确报告「10-B 的驾驶／持球／冲击都已经测试完毕，可以开始 10-C」，并说明测试日志可能未上传。该反馈作为进入 10-C 的授权及团队功能回归结论，详细边界见 [10-B 第 5.2 节](Combat/MVPB_Phase10B_GAS_Foundation_CN_v0.1.md#52-团队回归完成与进入-10-c2026-10-14)。本轮只更新文档，没有新编译、自动化或真人测试；原始证据保持原样。

随后用户确认规则并授权10-C施工。本轮新验证使用UE5.8.3：Editor/Game成功，全部自动化22/22通过（最终0警告／0失败），只读加载三份Combat DA和两个地图成功，整个施工／验证前后320资产哈希均未变；隐藏主机监听7786、客户端Join succeeded并收到两辆车HP100。详细证据及测试夹具修正见 [10-C实施记录](Combat/MVPB_Phase10C_Implementation_CN_v0.1.md)和 [验证JSON](Combat/MVPB_Phase10C_Verification_20261014.json)。未执行团队V1资产接线或真人网络模拟，不将代码完成等同阶段全部验收。

以上是10月14日交付时点。2026-10-15用户报告已在另一电脑完成10-C及相关资产更新／测试；当前1eb02c4已包含三个DA迁移，旧的“等待迁移”不再作为当前进度。接收初时本机仍有5.8.2和旧DLL，见[当时Handoff记录](Combat/MVPB_Phase10C_Handoff_20261015.json)。随后同日获授权进行10-D，已在本机可用5.8.2完整构建Editor／Game、27/27自动化通过，并只读导出四份DA；隐藏Listen Server监听7791、Join succeeded，客户端收到两辆车HP100，主机显示CombatVersion=1／ImpactReady=1／ImpactVersion=0。320资产整个施工前后未变，两个专用进程已清理。此smoke只证明现有保存资产的启动／初始复制，新版TieredV1冲击仍需团队迁移和真人验收；证据独立保存于[10-D摘要](Combat/MVPB_Phase10D_Verification_20261015.json)。

## 4. 已确认的阶段 10 决策

- GAS 管规则、结果和状态生命周期；车辆／球的 Chaos 移动、碰撞与 Network Physics 仍由 Actor／Component 管理。核心逻辑 C++，蓝图轻量配置／表现，由团队手工接线。
- 玩家 `PlayerState` 是 ASC Owner，当前车辆 Pawn 是 Avatar；玩家 ASC 采用 Mixed Replication。球、普通道具与机关默认无 ASC，球携带轻量攻击来源／效果载荷。
- 玩家主动攻击采用 `CapturedAttack × max(0, ActionMultiplier + sum(MultiplierDeltas))`。动作倍率只有一个，增量可正可负；机关用自身基础伤害。
- 飞行球在服务器接受发射时保存攻击力、倍率和稳定来源；发射者退出或飞行期间 Buff 到期不回算这次攻击。目标防御／免疫在命中时读取。
- 伤害和冲击是独立通用入口：允许 0 伤害强冲击、高伤害低冲击／无冲击。在正式淘汰规则加入前，HP 保持 0，但有效冲击仍生效。
- 2026-10-14 已确认 10-C 的 25×1 迁移、目标受伤倍率初值1且无平减护甲、伤害免疫后普通球仍消费攻击回 Free。伤害抵抗／免疫只改伤害分支，冲击抵抗／免疫只改冲击分支；两者不得互相作为接受条件或使用减免后的伤害反推冲击。冲击免疫不自动取消 Chaos 自然碰撞，具体冲击抗性公式留给 10-D。
- 冲击可按来源的物理测量选档，也可直接指定档位；最低脱球档位属于目标响应。统一档位结果，不强迫球速与车车碰撞冲量共用输入阈值。
- 10-D已定稿：主动攻击固定档；自然碰撞按公共标尺选档并由来源能力封顶；GAS整数降档最终0～3，免疫另用Tag，脱球比较有效档位。保留自然Chaos阻挡／推挤，普通碰撞不扣血；详细连续接触和物理补足边界见10-D施工记录。
- 车辆已调 DA 是基础值；局内加速、减速、低抓地等由 GE 影响有效参数，移动组件实际施力。变化参数必须能在网络物理重模拟中按帧复现，GAS Prediction 不自动解决车辆物理预测。
- 阶段 10 结构沿用到正式开发，允许小规模重构；逐环节验证，不提前创建大型通用技能／比赛框架。

## 5. 保存参数基线与已知限制

2026-09-30只读检查导出了五个DA保存值及14个项目／配置／蓝图／地图／网络资产哈希；10-B只修改其中`.uproject`和DefaultGame.ini。10-C整个施工／验证前后320资产均未变，保存值继续沿用；新增Combat版本字段只读加载仍为LegacyFixedDamage，不能以新种子显示值当作已迁移。**未重新采样PIE或证明运行时实例无覆盖**。

上段为历史保护结果；随后团队在1eb02c4有意更新玩家／被动车／普通球DA。本次保存这三份新文件哈希，接受团队已迁移／测试反馈，但未重新采样全部参数；下面表格继续表示迁移前的调参基线，不能当作新的完整字段导出。

| 内容 | 当前保存基线 |
|---|---|
| 玩家 | MaxHP `100`；质量覆盖 `800 kg`；前进加速度 `1800 cm/s²`、上限 `2400 cm/s`；正常／手刹抓地率 `12 / 0.2` |
| 发射／脱球 | 后坐最低／最高速度变化 `400 / 600 cm/s`；旧脱球冲量阈值 `100000`，重捕锁定 `1.5 s` |
| 普通球 | 固定伤害 `25`，旧击退倍率 **`1`**，半径 `50 cm`，质量 `35 kg`，线性阻尼 **`0.25`** |
| Combat | 中／重档等效速度阈值 `1000 / 2600 cm/s`；轻／中／重档水平速度变化 `250 / 600 / 900 cm/s` |

早期迁移说明里的球击退倍率 `5`、线性阻尼 `1` 等与当前快照不同，不能据此恢复旧数值或用 C++ 默认值自动覆盖当前配置。

当前必须记住的限制：

1. 伤害／冲击继续独立，10-D已实现档位与抗性；自动化不代表新碰撞标尺、墙角及真人网络手感已通过。自然接触补足是受预算限制的策略，需实测校准。
2. **仅启用TieredV1后**脱球消费实际档位，旧原始冲量路径退出。最新团队日志确认两辆玩家运行V1，免疫命中实际档位None、伤害25；只确认该子项，不等于完整物理／网络验收。
3. 等待界面不是完整冻结／比赛流程；目前无正式淘汰、观战、胜负、主机迁移或重连闭环。
4. 过去曾有自制网格误放 `/Engine/EditorMeshes` 的资源缺失问题，当前提交已修正相关引用。共享自制资产应在项目 `/Game` 路径，不能依赖仅存在于某台电脑引擎安装目录的文件。
5. 本次实测发现 Entry UIOnly 聚焦不可聚焦 Widget 的菜单错误，待整理焦点配置／目标；不影响已验证的 GAS HP 复制。主机的 `75→75` 等同值属性通知不是二次伤害；Health 订阅者仍收到通知，不能按其次数触发未来受击演出。
6. 命中球消费后转Free，现有全局拾取锁到期即可被满足条件的车辆捕获。已复核免疫目标约0.234秒后取得球；不是免疫赋予接球能力。可调球DA的Post Launch Pickup Lock Duration延后所有车辆拾取；目前无独立“被命中者禁捕时间”，若需要应另行施工，详见[10-D排查第7节](Combat/MVPB_Phase10D_Implementation_CN_v0.1.md#7-团队现象排查冲击免疫后很快捕获命中球2026-10-15)。
7. `ApplyAction_Internal`线程缺陷已修复，用户本轮确认击飞效果恢复并接受当前档位响应暂用。仍保留水平补足，不再把恢复旧完整追加作为当前待办；后续力度按配置调参另记。原第8节为历史诊断，最新结论见[10-D第10节](Combat/MVPB_Phase10D_Implementation_CN_v0.1.md#10-团队击飞回归与10-e接续2026-10-16)。

## 6. 下一环节和待确定项

**排期调整：用户决定将剩余10-F-4完整测试统一放到MVP-B收尾，当前不再以F4-A作为下一施工入口。** 用户随后已授权车辆表现代码侧准备与接口，VP-0代码核对／VP-1底座及原生验证完成，完整矩阵与门槛仍见[F4方案](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)，后置不等于最终验收通过。

当前下一开发入口可进入**VP-3视觉悬挂具体方案／接口准备**，本轮未启动施工。VP-2俯仰已获真人初测成功反馈，参数精调按用户意见后置，其余人工矩阵继续留档，不作为重复搭建已有接线的理由。VP-3沿用当前车型表现DA、AnimBP和玩家Definition引用，采用分轮视觉探测／骨骼平移，不因视觉悬挂增加物理施力；四点物理悬挂和自然落地辅助另行设计。

**VP-2已完成代码及原生验证、俯仰真人初测通过**：参数精调后置，侧倾／漂移与完整画面回归继续跟踪。下一计划为VP-3视觉悬挂；VP-4离散回弹、四点物理悬挂／自然有限落地辅助仍分开且尚未施工。

MVP-B收尾时使用届时冻结的同一提交／构建／资产，统一覆盖驾驶／漂移／持球／发射／冲击／免疫／退出、墙角／腾空／连续接触、100ms实测RTT／1%丢包／20ms抖动及四台有线PC；150ms／3%／50ms压力档单列。合并阶段9和10-D／10-E重叠欠项，逐项保留结果，不能用基础日志代替。F4-A诊断／夹具也暂不施工，届时按A→B→C→D恢复。

后续每项开发仍保留完整编译、相关自动化、涉及联机时必要双端基本验证；真实阻塞缺陷及时处理。阶段10最终验收未完成。弹簧拳已有基础战斗／阶段复制实现及原验证，后续维护仍由另一成员负责，本轮不扩展其规则或修改资产；接口和未完成边界见[交接注意事项](Combat/MVPB_SpringPunch_Combat_Handoff_CN_v0.1.md)。完整矩阵仍并入MVP-B末尾统一测试。

`ArcadeVehicleMovementComponent`保持基础配置，纵向驾驶／上限在消费时乘当帧倍率，转向继续用基础速度参照。物理历史已加入Epoch／Revision／快照及分段时间线。F2发现UE5.8通用Async Tick在回滚时被跳过，已补仅重模拟时执行原驾驶的Chaos回调，详见[F2实施第3节](Combat/MVPB_Phase10F2_Implementation_CN_v0.1.md#3-本轮发现并修复的既有回滚缺口)；不得把该回调再用于正常帧重复施力。

10-E接线已提交，基础双端免疫反馈与日志子项保持，不再重复已完成的接线。其余精确GE／冷却、输入边沿、目标持球／到期恢复、带活动效果退出及100ms实测RTT／1%丢包／20ms抖动矩阵随集成回归补齐；下一次双方先开`hal.AbilityLog 1`，具体边界见[10-E实施第7节](Combat/MVPB_Phase10E_Implementation_CN_v0.1.md)和[手动复核](Combat/MVPB_Phase10E_ManualLogReview_20261016.json)。四台真实有线PC仍是最终环境，不以同机双端替代。

10-D目前没有发现阻塞10-E的程序缺陷，也无必需追加功能；用户确认击飞恢复，档位响应力度以后调参。尚缺的降档／免疫／脱球、连续接触、双向角色及网络扰动细项继续随集成回归补齐，不把进到10-E等同10-D完整验收；新发现的缺陷仍须修。独立命中目标禁捕继续只留档，弹簧拳接入由另一成员负责，本会话不扩展该模块。

旧10-C操作只用于未迁移副本，不要求团队重复已完成内容。10-D／10-E的5.8.2证据保持；本轮5.8.3已完成最终构建、45项普通回归与正常加速GA的真实双端请求／刷新／到期检查；未重做完整真人物理组合，四台有线PC仍待另记。跨设备提交时一起带上源码、文档及团队手工保存的资产，不提交Saved／Binaries。

10-B回归反馈与旧证据保留；它们不代替10-C新规则和V1接线后的测试。后续记录实际提交／UE补丁／DA哈希／单向注入与实测RTT，不能将同机日志时间差当RTT。弹簧拳原型及接入由另一成员独立负责，10-C／10-D提供共用结算与物理入口；本次仅交接文档，该模块与资产未修改。

| 何时确定 | 尚需具体确定的内容 |
|---|---|
| 10-C 后续证据补充 | 已有迁移提交、新资产哈希和团队完成反馈；若要宣称特定RTT／丢包／抖动门槛已通过，再补当次细项日志；本机重编后可另导出V1字段与运行状态，不覆盖旧记录 |
| 10-D 后续回归／调参 | 击飞恢复已获团队反馈；补三档／降档／免疫／脱球、连续接触、双向及网络扰动记录；档位响应与碰撞标尺数值以后调整，不作为新增开发功能 |
| 10-E 团队验收 | 无额外核心算式待定；基础双端免疫反馈和双向命中日志子项已记录，接线已提交。下次开AbilityLog，补精确计时、输入边沿、目标持球、带效果退出及扰动细项；正式表现素材暂未指定，当前用调试HUD |
| VP-3人工验收与VP-4接续 | VP-3代码／构建／原生验证完成；团队在当前AnimBP接四悬挂Translation节点、启用实际车型DA，检查平地／单轮台阶／坡面／空中／侧翻自救及双端画面。俯仰／侧倾各30°保持；通过后再讨论VP-4事件与回弹 |
| MVP-B收尾统一验收 | 10-F-4完整A～D矩阵已由用户后置；届时合并原阶段9／10-D／10-E欠项，覆盖真实物理组合／角色互换／退出、100ms实测RTT／1%loss／20msjitter、压力档与四台有线PC。F2回滚驾驶需覆盖，不以单局日志代替完整验收 |
| 团队环境变更时 | 本轮设备5.8.2已有新两Target／67项／基本双端证据；另一设备5.8.3旧结果保留。联机前统一补丁／构建，真人组合／完整扰动和四台有线PC证据仍待补齐 |

## 7. 两台电脑如何接续和维护本文件

**离开设备前**：保存需要保留的 Editor 资产；结束正在运行的代理和游戏进程；更新本文件中的阶段、证据、限制与下一步；检查 Git 差异并提交要交接的源码、配置、项目资产和文档，Push 到团队仓库。不要把 `Binaries/`、`Intermediate/`、`Saved/` 或 Codex 本地数据库当作代码交接包。

**接收设备开工前**：检查本机未提交改动，确认分支后再 Pull；核对提交和实际引擎补丁版本。需要 C++ 验证时关闭旧 Editor／Live Coding 路径，使用本机引擎重新生成必要的项目文件和编译。路径可以不同，但文件内容、配置引用和版本必须可核对。并行开发应使用独立分支，避免同时改同一个 `.uasset`／`.umap`。

**维护时**：本文件保留最新状态摘要；详细算法、参数、日志和验证矩阵放在对应环节文档。更新日期、核对基点、实际交付、验证来源、未关闭事项及下一步。新的团队测试结论可以单独更新，不要求重新施工；历史基线快照保持不可覆盖，资产变化时新增快照并更新链接。规则设计改变时同时修改对应规格，避免本入口与详细文档互相矛盾。

新设备开场提示可直接使用：

> 这是同一 Unreal 项目的接续开发。请完整读取根目录 AGENTS.md、Docs/Project_Status_CN.md 和当前环节规格，再核对 Git、实际引擎版本与相关源码／配置。先汇报已完成、未验收及差异，再执行我本次指定的环节。涉及蓝图／Data Asset 编辑，请提供 Unreal Editor 操作步骤，不直接修改资产。

## 8. 本次更新记录

- 2026-10-22 VP-3：从干净main/faf5d06、实际UE5.8.2实现四轮视觉悬挂，沿用既有表现链路，默认关闭、无物理／网络协议改动。最终两Target／普通67/67／实际NetTest双端各1/1成功，667资产全同；四车16条查询微测19.555µs。首轮命名冲突及两项测试Roll符号期望修正后完整重验，原失败记录保留。当前团队手工悬挂节点、启用DA和正常地图画面待完成；无5.8.3／渲染／F4验收或VP-4施工，未提交／Push。详见新增VP-3实施和验证摘要。

- 2026-10-22：按用户要求Fetch并将main快进至8571fa4，合并状态文档顶部的炸弹与Editor校准记录，更新当前提交及运行前置。16份原有本地改动先做逐文件备份，再暂存／恢复；原stash保留作恢复备份，校准工具和团队手调DA保持未提交。此次没有改动玩法实现、保存资产、重编或运行新测试，历史专项证据不扩写为合并后验收。

- 2026-10-21自救Definition输入入口补全：以main/c3f90ca接收团队共享DA／IMC修改与新IA，确认BP旧Input被CanEditChange锁定，前次漏加Local.SelfRightAction及复制且文档入口错误。新增可选bool独立输入、初始化复制及配置／实际绑定断言，修正操作为Definition→Local Config→Input；不解锁Pawn或切Legacy。Editor／Game成功，重新57/57、364资产全同，未新跑双端／真人；保留前次网络证据，下一步团队填写Local DA并实测。见顶部补充记录及JSON；未提交／Push。

- 2026-10-21姿态门控／长按自救：从干净main/f634292接续，用户报告支撑修正后弹动有效减少。扩展原共享DA、服务器许可时钟、独立按住输入、有限物理翻正及动作／历史；执行边界Free脱球，执行中禁接球／发射／新技能，原效果及伤害／抗性分支保持。最终Editor／Game成功、57/57普通、真实Solver5组900步，30／60／120限帧双端各1/1及重模拟29／44／61次。363资产与Config保持，IA／IMC／Local Config引用待团队手工保存，真人画面／几何手感与F4未验收。反射／测试编译、DLL占用、短暂离地与双端夹具同步失败记录保留，最终严格断言通过；见[实现](Movement/MVPB_Vehicle_Pose_And_SelfRighting_CN_v0.1.md)和[证据](Movement/MVPB_Vehicle_Pose_And_SelfRighting_Verification_20261021.json)。未提交／Push。

- 2026-10-21普通静止支撑分类修正：以main/c747e4b接收用户已修复车轮DA，保留363资产和Config。用普通来源／WorldStatic／非模拟、组件及物理速度、当前姿态同步判定取代Mobility=Static门槛，保留特殊来源、有效档、伤害／抗性／免疫、脱球和ExplicitTier；补来源／预算可关闭日志、实际保存两图回归及Movable Solver。UE5.8.3 Editor／Game成功、普通53/53、Solver1/1（16组／11520步）、实际NetTest Host／Client各1/1，重模拟56次／单向80ms。实际Floor接触两玩家均StationaryWorld／Suppressed1／RuleDV0。首轮地图夹具复制物理运行体断言已修复，最终重编／重测通过；失败日志保留。侧面／车尾真人手感与自然摇摆仍待复测，姿态门控／自救未实现，F4后置保持；新增[实施](Movement/MVPB_Vehicle_Support_Classification_Fix_CN_v0.1.md)与[验证](Movement/MVPB_Vehicle_Support_Classification_Verification_20261021.json)，未提交／Push。

- 2026-10-21两项实测反馈诊断：main/747eac7，保留团队启用Config、新DA／PM和原文档改动。实际0／Min与游戏快照生效，确认Test／NetTest地面均Movable／WorldStatic／非模拟，漏过上一轮Static条件；普通碰撞旧日志仍排队向上约350～535cm/s，接触Actor未记录不能逐条归属Floor。只读实际AnimBP六节点／引脚正确，C++无轮平移；四轮Radius10与缩放1模型47.5不匹配，约4.75倍转速，默认TSR／模糊列待画面对照。363资产全同，无修复／构建／新游戏或双端测试；新增诊断与原始检查留档，未提交／Push，下一步先讨论方法，见本文顶部入口。

- 2026-10-21设置页定位排查：接收main/747eac7，团队新增共享DA及Physics目录资产仍未提交并保持。本机已有成功Editor构建，最近正常Editor日志也已加载该运行时DLL并打开VehicleStabilityRules资产；尚不能据搜索无结果认定未编译。按本机UE5.8.3的UDeveloperSettings实现确认Project／Game分类与Vehicle Stability页名，设置搜索支持当前页范围；修正文档为先清空搜索、分类侧栏Game→Vehicle Stability，再定位Shared Rules。仅源码／日志核对和文档更新，无新编译、UE运行、界面查看或资产修改，实际菜单可见性待团队核对；不把可能的搜索范围问题写成已复现原因。

- 2026-10-21最小接触修正：接收干净main/699fc0c，新增VehicleStability共享DA／项目设置／World冻结快照，统一车辆材质并在Authority过滤普通静态支撑的规则追加；档位、独立伤害／抗性、脱球和显式击飞保持。配置签名复制并阻止不一致客户端模拟，补可关闭动作诊断及测试。最终UE5.8.3 Editor／Game均成功，52/52普通回归0测试警告／失败，16组真实Solver诊断1/1通过；最终Host／Client各1/1、签名4097445932、客户端重模拟55次，主机单向80ms。361资产哈希全同，无保存Config／BP／DA／地图或弹簧拳修改。首轮普通51/52因夹具立即读取未积分的冲量而失败；后续编译因UE材质枚举拼写差异失败，均修正并保留证据。提供PM→DA→项目引用详细手工步骤，正式开关仍关闭，姿态门控／自救及真人手感待做；下一代码入口扩展同一DA实现已确认自救，再VP-2。本轮未提交／Push，见[实施](Movement/MVPB_Vehicle_Stability_MinimalFix_CN_v0.1.md)及[证据](Movement/MVPB_Vehicle_Stability_MinimalFix_Verification_20261021.json)。

- 2026-10-21反弹诊断／基线完成：以main/21d29b0、UE5.8.3保留既有文档施工，发现旧DLL落后后完整重编。只读导出当前DA／Test和NetTest，361资产前后全同；确认默认材质恢复系数0.3、代理456×288×269cm和墙体高度／地面露出情况。新增16组临时Solver对照、11520轨迹／2701接触数据；区分真实离地、接地摇摆和规则追加，详见[结论](Movement/MVPB_Vehicle_Stability_Diagnostic_CN_v0.1.md)、[基线](Movement/MVPB_Vehicle_Stability_Baseline_20261021.json)与[证据](Movement/MVPB_Vehicle_Stability_Diagnostic_Verification_20261021.json)。修正旧无Rig夹具为独立Definition副本，最终Editor／Game成功，普通50/50、诊断1/1均0测试警告／失败；首次Python受保护字段、诊断编译及事件交接失败保留。无玩法参数／资产修改、真人／新双端／F4验收或自救／全局DA实现。下一入口最小物理修正规格／实现；未提交／Push。

- 2026-10-21自救规则定案／共享DA：用户明确接受触发／长按、执行打断／重试、球／技能三组首版测试规则，并要求参数放入全局通用DA。将计划第5.1节改为已确认，新增第5.2节：项目级统一引用窄范围车辆稳定／自救DA，统一时长／门槛／响应参数，运行实例持有计时和状态，初始化形成一致的纯值物理规则快照，首版保存后重新开局生效。尚无该原生类型／uasset、运行诊断或新构建／测试；本轮仅核对既有全局CombatRules加载惯例并更新计划／状态，保留已有文档改动，未改玩法／资产、提交或Push。当前无其他已知玩法待确认项，下一入口仍为反弹诊断／基线。

- 2026-10-21自救待确认项收敛：用户接受插入流程并询问是否还有确认项，将首版建议集中到计划第5.1节的三组：持续侧翻1秒、低线速／角速及长按1.5秒；有界2秒翻正、新外部有效中／重档冲击打断和执行后3秒冷却；实际翻正开始时Free脱球／执行阶段禁接球发射及新技能、原有效果自然计时。全部仍为待确认建议，未当作用户决定；反弹诊断无新增玩法确认依赖。本轮只更新计划／状态，无源码／资产修改、UE运行、构建／测试或提交。

- 2026-10-21车辆物理插入计划：用户新增侧翻／倒扣驾驶及触地／撞墙持续弹跳反馈，并选择低速、持续侧翻后长按自救。核对现有单射线接地／施力、仅作用于脱墙辅助的姿态条件、环境档位与受预算限制的法向追加、DA BodyInstance应用和网络历史／Authority动作，新增[计划](Movement/MVPB_Vehicle_Stability_And_SelfRighting_Plan_CN_v0.1.md)与Git白名单。在VP-2之前插入反弹诊断／修正、姿态操控限制及自救，四点物理悬挂／有限落地辅助仍独立，VP-1保持结束。弹跳主因与具体恢复门槛／力度尚未定案；本轮保留上轮4份文档改动，只新增／更新计划和状态，无玩法源码／资产改动、UE运行、构建或新测试，未提交／Push；F4后置不变。

- 2026-10-21 VP-1收尾同步：用户明确当前应为VP-1结束。核对干净main/21d29b0、本机UE5.8.3及a11228a接线提交，确认现有ABP_Car01／DA_Car01_Presentation资产已入库，fd41bbb包含碰撞调整；按用户确认将阶段和下一入口同步为VP-1结束／VP-2方案准备。保留10月19日构建／49项／数据双端历史证据，明确10月20日另一设备49/50的OptionalRigPreservesGameplay旧无Rig假设尚待适配；没有新增逐项双端、扰动或四机证据，不伪称重跑全绿。本轮只改状态、实施／方案和配置说明，未改C++／资产、运行UE、编译／测试、提交或Push；F4后置与其他成员工作记录保持。

- 2026-10-20转向校准排查：核对main/44cbaf8及UE5.8.3，保留团队未提交的玩家BP／Definition修改和新增AnimBP／表现DA。用户报告接线后游戏可见小车，但左右输入令前轮绕前向轴侧倾。对照现有AxisRotation、组件输出、UE ModifyBone实现和已留档参考姿态，确认DA采用骨骼局部轴：Car_01 steer局部Z对应Mesh前向、局部Y对应Mesh向下；提供FL／FR转向Y/-1、Bone Space／Add to Existing及完整Rotator接线步骤。仅更新配置指南与状态，未读取新资产具体字段／图、修改源码或资产、启动UE、重编或运行自动化；原因与修正仍待团队实测确认，不记为已解决或VP-1画面验收通过。

- 2026-10-20 Car_01检查：接收干净main/2e4176d，确认VP-1已随9983c5f提交，新增20素材使总数331→351。通过UE5.8.3进程级Python／GeometryScripting只读检查实际SKM／SK／PA、17骨骼参考变换、源LOD顶点和刚性权重、材质父级／贴图依赖，新增[可复查报告](Presentation/MVPB_Car01_AssetInspection_20261020.json)、[结论](Presentation/MVPB_Car01_AssetInspection_CN_v0.1.md)与Tools/InspectVehicleModel.py。结构／权重／依赖满足接线基础条件，转向Y/-1、轮转Y/+1和约47.5cm原始半径均留档；实际缩放、单轴预览、AnimBP／DA、画面与双端仍待团队。351资产SHA256前后全同，无生产源码／配置数值／资产修改、新构建／自动化或最终F4验收；专用UE进程正常退出，未提交／Push。
- 2026-10-19车辆表现VP-1：用户授权代码准备及接口并要求导入说明。保留已有未提交方案／截图，以main/ca90ffb、UE5.8.3实现可选车型骨骼配置、原生轮转／转向组件及AnimInstance，Pawn新增稳定默认子对象和旧视觉回退，Movement以现有锁发布实际消费输入；观察转向采用SkipOwner的一字节变化值、最多15Hz，无新驾驶RPC或历史序列化改动。新增4项普通测试及真实双进程数据检查，最终Editor／Game成功、49/49普通测试0警告／0失败、Host／Client各1/1成功，331资产哈希全同，专用进程正常退出。首次复制宏命名编译失败和47/49夹具失败独立保留并修正；具体见[实施](Presentation/MVPB_Vehicle_Presentation_VP1_Implementation_CN_v0.1.md)、[验证](Presentation/MVPB_Vehicle_Presentation_VP1_Verification_20261019.json)与[导入配置流程](Presentation/MVPB_Vehicle_Model_Import_And_AnimBP_Setup_CN_v0.1.md)。实际模型／AnimBP／DA接线及画面待团队，VP-2～4／物理辅助／弹簧拳未扩展，F4仍后置；未提交／Push。
- 2026-10-19车辆表现方案：用户要求将已讨论方向详细留档供团队同步。核对干净main/ca90ffb与实际UE5.8.3，修正当前入口仍记旧提交／未提交F4文档的状态。新增[车辆模型与基础Rig方案v0.1](Presentation/MVPB_Vehicle_Presentation_Rig_Plan_CN_v0.1.md)及用户骨骼截图原样归档，记录骨骼／轮位、C++组件／AnimInstance／薄AnimBP职责、固定骨骼映射限制、局部轴／参考姿态、参数建议、线程快照／更新顺序、不同网络角色取数、回弹去重、视觉／物理悬挂分界、VP-0～4与团队Editor／验收流程。总体方法已认可，具体素材／数值／部分接口待核对；本轮只有文档、图片与Git白名单，无生产源码／配置数值／Unreal资产修改、构建／UE运行／自动化或新团队实测，不声称Rig已实现或阶段10最终验收通过；未提交／Push，弹簧拳仍交另一成员。
- 2026-10-19弹簧拳交接：用户明确将战斗／联机接入交另一成员并询问注意事项。本次核对当前原型与CombatResolver／Action／ImpactReceiver接口，新增[交接说明](Combat/MVPB_SpringPunch_Combat_Handoff_CN_v0.1.md)，覆盖无ASC机关固定伤害来源、独立抵抗／免疫、持久每拳Action与bHitAccepted消费、默认bApplyImpact=false／LegacyMeasurement的易漏点、方向法线约定、V1配置、Sweep末段、服务器时序复制／客户端表现、待定玩法及最小验证。保留现有未提交F4方案改动，未修改源码／资产、构建／启动UE或运行新测试；未给新机关数值定案，也未提交／Push或代替成员施工。
- 2026-10-19排期调整：用户提出10-F剩余测试在MVP-B末尾统一进行，已采纳为F4-A～D整体后置，当前不施工诊断／夹具或执行完整矩阵；F1～F3和加速GA、团队基础双端结论保持。保留原F4方案与验收门槛，最终冻结同一构建后合并阶段9／10-D／10-E欠项统一执行；后续功能仍按改动做编译／相关自动化／必要双端验证，阻塞缺陷及时处理。仅调整文档排期，保留上轮未提交说明，无源码／资产／新构建或测试，不将后置写成最终验收通过，也不自动开工弹簧拳／其他新模块。
- 2026-10-19 F4方案：用户报告已提交Git并询问下一步范围／方案；确认干净`main/c825d8d`包含3项团队加速接线资产和手动复核文档，实际UE5.8.3。新增[F4执行方案](Combat/MVPB_Phase10F4_Integration_Plan_CN_v0.1.md)，细分A本机程序验证／必要诊断、B低延迟双端玩法、C主验收／压力扰动、D四台有线PC；沿用既定10分钟、1秒振荡、球状态收敛与幂等门槛。识别现有HALNetMetrics只记物理帧／ResimRuns，不能证明RTT／loss／jitter；加速与免疫共存使用测试夹具或独立配置轮次，不加第二正式槽位。仅方案／文档／Git白名单，无源码／资产修改、构建／UE运行或新F4验收；下一施工入口F4-A，本轮未提交／Push。
- 2026-10-19团队实测接续：接收`main/a38498e`，GA源码／文档已提交，团队新GA／GE及PlayerState修改仍未提交。用户报告双端符合逻辑；本次只读核对12:04～12:05主机／Client_2日志及两个启动参考日志，8次成功激活（双方各4）、4段完整效果，实际GA／GE子类和唯一Primary正常；末次刷新至GE移除5.000／5.003／5.005／5.001日志秒，倍率1.5／1.25且同值刷新不新增Revision，到期均恢复1／1。客户端1681条物理采样均Ready1／Recovery0，实际Replay且回滚请求最大5；无崩溃／GAS告警／对战异常断线，既有Entry不可聚焦Widget错误与启动提示保留。134份源码／配置与原验证一致，331资产中的团队新增2／修改1已留哈希，代理不改资产／源码、不重编／跑自动化、不提交／Push。基本接线与双端实测通过，当前下一入口F4完整集成；见[手动复核JSON](Combat/MVPB_Phase10F3Ability_ManualLogReview_20261019.json)及[实施第9节](Combat/MVPB_Phase10F3_Movement_Ability_CN_v0.1.md#9-团队双端反馈与日志复核2026-10-19)。
- 2026-10-19：用户授权补完整加速GA并复用原Primary替换免疫。新增窄ServerOnly运动GA、可配非周期GE、能力Tag，接现有输入／GAS激活；施加后结束GA，默认无费用／冷却，重复发动刷新5秒。新增3项普通测试与真实双端夹具，提供GE→GA→PlayerState→原输入的详细步骤。首次Editor构建被Live Coding阻止，用户保存关闭后完整编译成功；复核补非零Period拒绝以防基础属性残留，最终重新顺序构建Editor／Game均成功，45/45普通回归0测试警告／0失败，正常GA Host／客户端各1/1成功（主机单向注入80ms）。双方刷新保持Revision2，最后刷新至物理恢复约5.034／5.150游戏秒且变3，客户端完成时4次回滚请求。Waiting客户端尝试是本地拒绝，权威拒绝另有普通测试；不伪称预赛请求到达服务器。329资产全同，134份源码／配置哈希留档，最终专用进程全部正常退出。保留起始4份文档改动，无蓝图／地图／保存Config／DA或弹簧拳编辑，未提交／Push。当前下一步团队手工替换Primary及实际按键测试，随后F4；详见[配置](Combat/MVPB_Phase10F3_Movement_Ability_CN_v0.1.md)与[本轮证据](Combat/MVPB_Phase10F3Ability_Verification_20261019.json)。
- 2026-10-19：用户明确需要与此前免疫一样，通过技能释放发动加速，而非debug命令。复核main/e8b18ce，源码仍无运动GA，现有Primary路由／授予可复用，免疫GA的专用校验不能套用。将当前下一入口从F4调整为F3补充运动GA，再做F4；原GE／桥接／调试验证保留，不宣称完整技能链已完成。本轮仅说明缺口与更新文档，未新增C++／资产／配置、编译或运行测试；保留此前3份未提交说明改动。
- 2026-10-18：复核干净main/e8b18ce及UE5.8.3，F3实现已经提交。用户询问通过GA／GE配置测试的详细流程；核对现有免疫专用GA、加速GE、DefaultAbilities、Primary路由和HAL Debug选类，补充F3第3.1～3.2节：当前可配置GE并用调试命令测试，尚无运动GA／通用施加GE基类；不能把免疫GA的GE引用直接替换为加速。记录原生运动GA就绪后所需授予／输入／GameMode接线，以及保留免疫双技能需第二路由的边界。本轮仅说明文档，无新源码／资产／配置修改、编译或运行测试，F4仍待实测；旧验证JSON保持。
- 2026-10-17：用户提交F2后授权下一步；接收干净main/599d60a，完成10-F-3原生5游戏秒运动GE、按目标跨来源一层刷新、服务器控制的自身请求／选择性清除、可选GE子类设置、关闭施加开关及HUD／日志。原免疫Primary、HP／其他冷却和F2物理协议保持。最终UE5.8.3 Editor／Game成功，42/42普通回归0测试警告／0失败；80ms单向注入最终Host／Client各1/1成功，客户端Waiting请求被拒绝，双方同值刷新Revision2保持、自然到期3、再施加4／清除5，免疫与约18.9秒冷却保留。主机／客户端从刷新到物理恢复约5.033／5.150游戏秒，非实测RTT。首轮跨来源夹具编译错误及一次客户端错过Waiting的启动时序失败独立留档；后者改为客户端完成检查后经已有可靠改名请求标记就绪，主机再Start，未削弱断言。329资产哈希全同，119份源码／配置留档，专用进程退出；无新蓝图／保存Config／DA／弹簧拳编辑。下一入口F4真人组合／完整扰动／四机，HUD新增行未渲染检查；本轮未提交／Push。详见[F3实施](Combat/MVPB_Phase10F3_Implementation_CN_v0.1.md)与[验证摘要](Combat/MVPB_Phase10F3_Verification_20261017.json)。
- 2026-10-17：用户提交F1后指定开始10-F-2；以干净main/ca38226实施最小运动Attribute、原生Avatar桥接、可靠Authority参数动作、离散有界时间线、历史比较与Recovery、有效施力和诊断。第一轮真实客户端测试因驾驶重模拟计数0失败，定位UE5.8通用Async Tick被PT-only回滚跳过，补仅回滚执行原驾驶的Chaos回调；另清理过时恢复前缀与未知上限。最终UE5.8.3 Editor／Game成功，39/39普通自动化0测试警告／0失败；80ms／1500ms单向注入各Host／Client共4份报告全部成功，客户端实际重模拟79／52次、回滚请求3／4次，后者检测Recovery并恢复Ready／1/1。329资产前后SHA-256全同，专用进程退出，未改配置／DA／弹簧拳。F2网络用2秒原生夹具GE，F3正式5秒GE／刷新入口和F4完整真人组合／扰动／四机仍待完成；代码文档未提交／Push。见[F2实施](Combat/MVPB_Phase10F2_Implementation_CN_v0.1.md)与[验证摘要](Combat/MVPB_Phase10F2_Verification_20261017.json)。
- 2026-10-17：用户授权开始当前小步，完成10-F-1具体技术规格、只读新基线与两个UE历史／动作机制探针；选定可靠Authority绝对参数动作＋有界时间线State检查点，固定PostSolve的F+1语义、迟到／缺帧恢复及线程／身份边界。本机UE5.8.3完整Editor／Game成功、35/35自动化0警告／0失败；最终基线导出0错误／0警告，329资产整个施工／验证前后未变。首轮无效CVar警告与并行UBT争用Trace.uba失败分别留档，修正后顺序构建／最终导出通过。仅新增测试／工具／文档，未改生产移动／GAS源码、DA或弹簧拳，无新运动GE／真实效果联机或四机验收；三个专用命令行UE进程均正常退出。下一入口10-F-2；交付尚未提交／Push，见[F1验证摘要](Combat/MVPB_Phase10F1_Verification_20261017.json)。
- 2026-10-17：用户进一步明确同意10-F首例三组规则：前进／倒车同步加成且保留其他基础参数与转向参照；两类倍率分别clamp(1+增量和,0,3)；同GE按目标跨来源一层刷新、不同GE贡献独立、到期保留速度及Avatar作用域清理。首例当前无其他已知玩法待确认项；10-F-1仍须完成物理帧技术规格与新基线，尚无施工／编译／运行或新验收。本轮仅维护计划与状态文档，保留此前未提交修改。
- 2026-10-17：用户确认10-F首例5游戏秒、加速度×1.5、速度上限×1.25、一层刷新作为测试方案。更新10-F计划第4节与状态；第6.1节集中记录作用范围、不同GE合并／0～3测试保护范围、多来源与到期／Avatar清理三组待确认建议，未擅自记为用户决定。物理帧同步／迟到恢复等技术路线由后续引擎检查定稿。本轮仅文档，无源码／资产编辑、新编译／运行或10-F验收。
- 2026-10-17：接收干净main/b8feb7b，10-F四小步计划已随Git到达，确认下一入口仍为10-F-1，具体规格／新参数基线及10-F代码未完成。完整读取AGENTS／状态／10-F计划并核对10-E、运动组件、物理历史、ASC／Avatar生命周期、球发射与冲击入口；原10-E清单99项源码／配置内容一致（含换行归一），仅Git忽略白名单有已知新增，8份团队资产哈希一致、当前329资产。本机实际UE5.8.3，Editor模块DLL仍为10月14日旧文件，运行最新代码前须完整重编；不将原5.8.2测试当作本机新验证。登记速度上限与转向共用基础字段的规格关注点。本轮只更新本状态文档，无玩法／配置／资产编辑，无编译／自动化／UE运行、DA字段导出或新团队测试。
- 2026-10-13：复核 10-A 已有交付及当前代码／资产；确认原快照 14 个文件全部一致，保留 2026-09-30 数值基线；建立本入口并在 `AGENTS.md` 中加入读取与维护要求。
- 2026-10-13：完成 10-B GAS 基础、唯一 HP 来源和 Avatar 生命周期；Editor/Game 编译、自动化 16/16、319 资产只读检查及双进程初始 HP 复制成功。定位 Test 旧 GameMode 的 PlayerStateClass 尚待手改；真人回归未完成。阶段 9 文档已有未提交改动保持原样。
- 2026-10-13：核对 10-B 已提交到 `c543c41`；新增无需蓝图接线的 C++ 本地调试 HUD 与双端操作文档，状态命令补 PlayerId。新增显示代码的两个 Target 编译及既有自动化 16/16 成功；真人扣血与延迟验收状态保持待测。
- 2026-10-13：当前提交 `6150896`，团队 Test GameMode 接线已只读确认；复核真实双端日志，客户端车 100→75，主机车 100→75→50 两端一致。登记菜单焦点错误与同值 HP 通知的原因；仅标记本地双向 HP 子项通过，单机／完整物理／网络模拟仍待确认。
- 2026-10-13：按用户要求讨论10-C兼容球／技能／机关／场地机制的统一结算方案，新增待确认草案；未修改玩法源码、配置数值或资产，未将后续功能标为已实现。
- 2026-10-14：接续核对 `main / 272ba87`、UE 5.8.3 实际安装及现有 Editor Target 编译成功日志；用户补充 10-B 驾驶／持球／冲击测试已完成并授权进入 10-C。记录团队反馈与未上传证据的边界，当前转为 10-C 规则确认；独立弹簧拳的伤害／冲击接入延期至阶段 10 完成后。本轮只改文档，未新编译／测试或修改玩法与资产。
- 2026-10-14：用户接受 10-C 三项规则；补充伤害抵抗和冲击抵抗的独立分支、普通球消费及 Chaos 自然碰撞边界和验收组合。10-C 方案已定稿，源码／编译／运行验收尚未开展；未改弹簧拳或资产。
- 2026-10-14：完成10-C源码、显式Legacy/V1迁移、攻击快照、独立结算和动作／脉冲／目标去重。UE5.8.3 Editor/Game通过，最终22/22自动化0警告／0失败，320资产保护与双进程连接／初始HP复制通过；记录首轮测试夹具修正与最终证据，提供团队手工接线步骤。当前10-C代码完成，V1资产迁移及真人验收待完成；弹簧拳与物理调参未改。
- 2026-10-15：接收main/1eb02c4，工作区起始干净；73份源码与10-C清单一致（44份仅换行差异），三个DA迁移已提交，记录用户另一设备完成资产更新／测试的反馈。原文档待迁移状态已更新；本机UE5.8.2及10月13日旧DLL尚需统一版本／完整重编，本轮仅读码与留档，不启动10-D。
- 2026-10-15：讨论10-D全来源档位制度，确认冲击免疫保留Chaos自然碰撞，记录普通碰撞选档、降档脱球、重复事件／连续接触与物理追加响应的风险；只留讨论文档，未改玩法／资产或开展编译测试。
- 2026-10-15：用户同意方案并授权10-D。完成原生全来源Receiver、显式版本门槛、发射档位快照、独立GAS降档／免疫、有效档位脱球、Authority网络物理动作及诊断。UE5.8.2 Editor／Game通过，最终27/27自动化0警告／0失败；只读确认10-C迁移保持、10-D字段仍Legacy，320资产未变；双进程初始连接／HP复制通过并清理。首轮失败与修复见施工记录。当前等待团队手工迁移及真实TieredV1物理／网络验收，未开展10-E／10-F或弹簧拳接入。
- 2026-10-15：当前main/79a73f6，四份DA有团队未提交修改并保留。按用户要求更新[双端启动说明](Configuration/MVPB_DebugHUD_And_GAS_Test_CN_v0.1.md#1-在一台电脑启动两个独立游戏进程)，两端默认开启HUD及GAS／球／Combat／车辆网络／历史日志，保留独立Host／Client日志文件。仅核对源码CVar与PowerShell语法，不新编译／启动游戏或宣称团队迁移已验收。
- 2026-10-15：复核用户本次既有Host日志：两辆玩家ImpactVersion=1，免疫目标被球命中仍扣25、实际档位None；球先Free再于0.234秒后Controlled，符合拾取锁＋自动捕获管线。留档现有球DA全局拾取锁路径、作用范围与独立命中目标锁的后续建议；仅分析／文档，未改源码或资产，未新编译／运行测试，不把局部日志扩大为10-D整体验收。
- 2026-10-15：用户要求独立命中目标禁捕只留档；另报告Heavy弱于10-D前。核对最新三次Heavy均3→3且未免疫；只读DA确认四份Version1、Heavy表及车辆原调参未变，球为Heavy、拾取锁1秒，320资产采样前后不变。两端日志均Ensure于ApplyAction_Internal，经本机UE源码确认物理线程误用GameThreadAPI；另定位主动攻击水平由完整追加改为补足。记录原27项不覆盖真实动作线程、待修复与同参数复验；本轮无源码／资产修改或新编译／动态测试。
- 2026-10-15：随后用户授权修复。改用Internal同帧V／W及线性／角冲量，新增可关闭物理执行日志和真实TaskGraph Solver回归；旧实现红测复现Ensure／错误结果，修复后Editor／Game成功、28/28自动化0警告／0失败。旧Legacy夹具与已迁移全局DA配置隔离，仅测试内存修改并恢复。当前V1双进程监听7793、Join／两车HP100复制及主机自然接触动作消费通过，无线程Ensure；未测试主动Heavy球／客户端受击／真人手感及网络扰动。320资产前后未变，两个专用进程清理；DA与水平补足算法不改，禁捕设计仍延期。详见[本次验证摘要](Combat/MVPB_Phase10D_ThreadFix_Verification_20261015.json)。
- 2026-10-16：接收main/663e1b0，工作区起始干净；相关10份源码／配置和四份DA与线程修复摘要SHA一致，上一轮修复／迁移已提交，DrivingLab等团队提交保持。用户确认击飞恢复，接受当前档位、实际力度以后调整。登记可进入10-E规格确认，首例推荐短时冲击抗性但尚未获选／编码；10-D其余回归与扰动证据仍待补齐，禁捕与弹簧拳接入仍延期。本轮只读核对和更新文档，没有新编译／自动化／Unreal运行或资产修改。
- 2026-10-16：随后用户选定8秒冲击免疫、20秒冷却且确认从发动时起算，同类效果刷新、未来允许其他GE叠层、退出清理。新增10-E规格，未编码GA／GE；独立新增HALSpawnBall及服务器请求、配置引用与生成／阻挡回归。UE5.8.2最终Editor／Game通过，29/29自动化0警告／0失败；早期夹具／本地RPC吸收失败与最终证据独立留档，不以进程退出码代替报告。NetTest监听7794、主机实际补球、客户端Join及动态球接收通过；未执行客户端亲自发起或真人捕获／发射。324资产前后未变，两个专用隐藏进程核对身份后清理，见[补球验证摘要](Combat/MVPB_DebugBall_Verification_20261016.json)。
- 2026-10-16：随后用户授权GA／GE本体。以干净main/aa278bd为基点，实现ServerOnly冲击免疫GA、8秒GE／发动时起20秒冷却GE、可配置默认授予、Avatar作用域清理保冷却／Owner退出清理、可选Bool动作输入与HALUseSkill／HUD／日志。HALImpactDebug免疫贡献改为独立无限GE以兼容Mixed复制，关闭不驱散技能。最终UE5.8.2 Editor／Game构建成功，33/33普通自动化0警告／0失败；真实NetTest双进程7795和独立60FPS复验7796均Host／Client各1项通过，验证客户端先发、同帧重复请求服务器拒绝、双方Tag复制／到期与20游戏秒冷却；60FPS服务器游戏／墙钟均20秒。首次链接／CDO构造／测试夹具／复制警告及初次联机墙钟断言失败分别留档，未伪装通过。324资产哈希前后未变，四个最终专用进程均自动退出。团队输入资产接线、真实双向持球受击／到期恢复／网络扰动、5.8.3和四机未验收；10-F／弹簧拳／禁捕未扩展。源码和文档未提交，详见[10-E实施](Combat/MVPB_Phase10E_Implementation_CN_v0.1.md)与[验证摘要](Combat/MVPB_Phase10E_Verification_20261016.json)。
- 2026-10-16：随后核对main/517b497，10-E实现已提交、工作区起始干净，本机仍5.8.2；与10-E摘要100份源码／配置全部一致。按用户问题将[实施记录第5节](Combat/MVPB_Phase10E_Implementation_CN_v0.1.md#5-以后改时长换技能时怎么配置)展开为GE／GA／PlayerState／GameMode逐步配置和日志核对，区分通用接入、免疫专用技能与调试工具；明确新Buff不统一继承免疫父类、多输入槽／车型技能库未建设、运动桥接仍待10-F。本轮仅文档变更，无编译／运行／新测试或资产修改，不回写原验证JSON，也未收到新增团队验收反馈。
- 2026-10-16：用户询问五份DA用途。按当前源码修订[DA速查](Configuration/MVPB_DataAsset_QuickGuide_CN_v1.0.md)，补充10-C攻击力／球动作倍率与10-D明确档位、公共物理响应／自然碰撞标尺，标明Legacy字段不再作为V1调参入口，并说明Local／GA／GE／PlayerState职责。本轮接续已见用户正在保存的GameMode、GA／GE／PlayerState及IA资产改动，全部保留，未导出保存值或核验其接线／效果，不将其视作新的团队验收。仅文档修订，无源码／资产修改或新编译／运行测试。
- 2026-10-16：接收干净main/6850e8a，团队接线及此前文档均已提交，源码／配置100项与原10-E摘要一致。用户报告免疫Buff正常生效／消失、效果有效、双端观感正常；复核最新及紧邻备份两局四日志，5次唯一球命中包含双向受击与2次免疫Heavy→None／Suppressed／Drop0，伤害各25且HP两端一致；客户端目标第三次命中恢复Heavy。无Ensure／Assertion／Fatal或GAS Tag复制警告；Entry焦点错误继续存在，备份客户端Host closed在主机ViewportClosed退出时出现。手动启动未开AbilityLog，未独立证明8／20精确计时、重复技能或GE类名；持球／活动效果退出／输入边沿／扰动／四机仍待补。329资产为团队新增5／改3，本轮不改源码／资产，不重编或跑新测试。日志归档到Saved并新增[手动复核JSON](Combat/MVPB_Phase10E_ManualLogReview_20261016.json)，原验证JSON保留；没有自动施工10-F。
- 2026-10-16：用户同意先将下一阶段计划／方案留档供换机开发。以干净main/e811b58、UE5.8.2为基点新增[10-F运动桥接计划v0.1](Combat/MVPB_Phase10F_Movement_Effects_Plan_CN_v0.1.md)，记录四小步、首例5秒／1.5／1.25建议测试方案、服务器权威纯值快照／物理帧历史与重模拟边界、未定规则、验收矩阵、相关源码和跨设备提示。更新总体方案状态与本入口，添加新文档Git忽略例外。只有计划留档，10-F-1具体规格／新基线尚未完成，代码未施工；无新编译／运行／自动化、DA导出或资产改动。换机前须将这些文档正常提交／Push，本轮未执行提交或远端操作。
