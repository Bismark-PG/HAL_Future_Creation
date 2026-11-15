# 正式技能测试木桩：配置与 Fred 场景使用

更新：2026-11-15。旧 `BP_PassiveTestVehicle`／Passive Definition 与源码保持，继续用于基础回归。本页介绍新的 `HALSkillTestDummy`；编译与自动化的实际结果见文末。

## 1. 职责与边界

新木桩继承 `TestVehiclePawn`，复用真实Chaos碰撞根、驾驶状态接收、区域GE、骨骼表现、受击／状态特效、PursuitTarget。它单独持有ASC和Health／Combat／Impact／Movement属性集，因为它不是玩家，不伪造PlayerState、Controller或房间成员。玩家车仍由PlayerState持有ASC。

核心区别：木桩可接收技能；不能因为它是合法目标，就认为它可以施放技能。`CanReceiveSkillEffects` 为独立目标接口，Car06施放者的 `VehicleReady` 检查保持。新木桩不接收驾驶输入、不授予Definition中的主动技能、不吸球、不执行自救、不占房间席位／胜负统计；可以被自然碰撞推动和由技能上抛。共用重力、物理材质及伤害／冲击抵抗仍沿用正式规则。

默认中性输入的木桩可以验证油膜GE和滑移后的物理结果，不能代替玩家驾驶验证“打滑是否好控制”。HP归零时清理区域绑定及Avatar状态效果，保留可读的0HP；之后不再作为技能目标。本版不添加自动驾驶或自动回血／复位；死亡后重新Play，或提高独立Definition的HP以支持重复测试。

## 2. 两份原生DA

程序提供两份新增原生示例（不修改既有车型DA）：

| 资产 | 用途 |
| --- | --- |
| `/Game/Data/Testing/DA_Vehicle_SkillDummy_Default` | 从Car02正式Definition复制的独立物理／外观／血量／抗性配置；HP=1000，保留合法车型配置 |
| `/Game/Data/Testing/DA_SkillTestDummy_Default` | 类型HALSkillTestDummyConfig，引用上面的Definition，配置TARGET标签与血条／WBP |

所有资产须随源码正常纳入Git同步。不要把Saved中的检查报告或Binaries当成团队交接内容。

想创建自己的变体：

1. 复制 `DA_Vehicle_SkillDummy_Default` 为 `DA_Vehicle_Dummy_Fred`。在独立副本调 **Health → Health → Max HP**；在Combat调伤害承受倍率，在Impact调Tier Reduction等。中性回归保持 Damage Taken Multiplier=1、Tier Reduction=0。
2. 外观可继续使用示例的Car02 Presentation；也可从另一辆正式车的Definition复制外观／物理配置。保持 **Collision Shape=ConvexMesh**、有效简单凸包Collision Mesh、Ready For Use、有效Local Config／Movement／BallControl。木桩不消费玩家输入，但底层正式Definition校验仍要求这些完整。
3. 不能引用旧Passive Box Definition；不能仅给组件勾选Simulate Physics来替代有效Definition。骨骼Mesh／AnimBP／Status FX仍按正式Presentation DA配置。
4. 复制 `DA_SkillTestDummy_Default` 为 `DA_SkillDummy_Fred`。**Vehicle Definition** 指向独立副本，**Label** 可填 `TARGET A`（1～40字符），**Show Health** 开启，**Widget Class** 暂留None。
5. 如果从零创建：Content Browser → Miscellaneous → Data Asset，分别选 **VehicleDefinition** 与 **HALSkillTestDummyConfig**，再按上述规则填好。复用示例通常更容易。

TestConfig为生成时配置；不支持在运行中替换整份DA来重建Avatar／物理。改配置后结束并重新Play。**Definition V1校验要求一个合法Primary Grant，保留复制来的配置，不要清空。木桩不会执行玩家Grant步骤，因此该条目不会授予木桩任何主动技能**，也不会退回PlayerState的默认技能。

## 3. 创建轻量BP

按照项目规则由团队在Editor手动完成；代理不修改Blueprint资产或Event Graph。

1. Content Browser → Blueprint Class → All Classes，搜索 **HALSkillTestDummy**，创建 `BP_SkillTestDummy`，推荐放 `/Game/Blueprints/Testing`。
2. Class Defaults → **Skill Test → Test Config**，指向 `DA_SkillTestDummy_Default` 或自己的 `DA_SkillDummy_Fred`。
3. 输入配置保持 **Auto Possess Player=Disabled、Auto Possess AI=Disabled**。不用配置AIController，不要设Player0，不要授予主技能，不要在Event Graph写BeginPlay／Tick。
4. 组件中已经具有健康、冲击、区域接收、PursuitTarget和表现组件，不需再添加副本。继承的Definition由TestConfig选择，使用TestConfig作为唯一入口；组件字段不是另一套调参来源。
5. Compile／Save。可把同一个BP多次拖进场景，每个实例选择不同TestConfig；不必为每个数值变体复制一个BP。

木桩只用于摆放目标，不要登记进正式VehicleCatalog，也不要选它作为GameMode的玩家Default Pawn Class。测试模式的车型列表已主动排除该类型。

## 4. Fred_TestScene：推荐单人离线测试

2026-11-15只读核对时，Fred保存的GameMode Override为 `BP_MVPB_ListenGameMode`，默认Pawn为Car06。为了单人快速测试，推荐在本测试地图切换现有 **BP_VehicleTestGameMode**；不修改正式房间地图或全局GameMode默认值。

1. 完整重开本轮编译后的Editor，打开 `/Game/Maps/Fred_TestScene`。
2. 推荐先复制现有 `BP_VehicleTestGameMode` 为 **BP_FredSkillTestGameMode**，再在Fred的World Settings中将GameMode Override设为这个副本。其父类必须是HALVehicleTestGameMode，Controller=HALVehicleTestPlayerController、PlayerState=HALPlayerState、GameState=HALMatchGameState。
3. 打开副本Class Defaults：Default Pawn Class可指定 **BP_VehiclePawn_Car_06**；在 **Vehicle Test → Test Vehicle Classes** 加入Car03／Car04／Car05等其他要测的正式车辆BP。默认车型自动包含在列表中，运行时通过面板切车。也可以直接沿用原测试GameMode及其Car02默认值，但修改共享BP会影响其他引用地图。
4. 地图保留一个PlayerStart，位置高于地面且能放下车辆完整碰撞体；不要让木桩或墙挡住出生点。
5. 拖入 `BP_SkillTestDummy`，让碰撞体底部在地面上方有少量落地余量。先放在PlayerStart侧面约400～600cm，避免初始重叠。测试Car06首测Radius=900cm，可先把木桩摆在这个范围内。
6. Play设置 **1 Player／Play Standalone**，正常Play。测试模式自动进入Playing，无比赛倒计时。使用左上角车型选择切换正式车辆，按原主技能键测试。
7. 新木桩在初始生成和换车后继续存在；旧Passive及地图遗留普通车辆仍沿用原清理规则。这是专门针对新木桩的保留边界，不代表所有预摆车辆都保留。新木桩不会进入可驾驶车型候选列表。
8. 选择Car06，靠近木桩发动，观察TARGET／HP、六星从左向右增长、逮捕时手铐和共享Stun、保护结束。警车移动出范围可以验证衰减。木桩不需要房间确认或占玩家席位。

观察相机、F2及Alt鼠标操作继续按 [单车离线测试文档](SingleVehicleTest_Setup_CN_v0.1.md) 使用。新木桩仅作为目标，不会抢相机；动态相机的OtherPlayers只取有PlayerState的正式玩家，因此木桩不会自动拉远玩家镜头。

## 5. 保留Listen模式时

也可以保持Fred当前Listen GameMode，正常建立允许技能的Playing流程后使用新木桩。它仍不计入最低玩家人数，因此放三个木桩不能满足四人／两人开局条件；开发房间或独立的测试模式应负责开局规则。Room Open练习期的伤害／技能权限仍生效，木桩不会绕过正式比赛阶段。

联机需要两端拥有相同代码与新增DA／BP资源。木桩在服务器生成／结算，客户端读取复制HP、区域／眩晕／保护状态与UI。木桩没有输入所有权，使用普通服务器刚体复制，不启用玩家Network Physics History。

## 6. 血条、六星和WBP替换

木桩自带屏幕空间TestHealth组件：原生TARGET、HP条和数值；Stun时血条变橙色。配置在木桩TestConfig，**Widget Class=None**使用原生回退。关闭Show Health可隐藏这套木桩提示；Car06六星是独立的技能Overlay，仍由警车技能DA控制。

自制血条WBP：父类 **HALSkillTestDummyWidget**，Designer创建实际Root／布局，填写TestConfig.Widget Class。**Get Dummy View**返回Label、HP、MaxHP、bReady、bStunned；**Receive Dummy View**每0.1秒推送只读数据。bReady=false时显示未就绪状态，不把0当成确定死亡。存在Designer布局后原生绘制退出；空WBP继续使用回退。默认组件绘制区域150×60，中心对应原生血条顶部，制作布局时保持这个锚点便于与六星对齐。

六星继续由 `DA_Ability_Car06_PolicePursuit → FX → UI` 的Star Count／Size／Gap／Stars Above Health调节；需要替换时使用HALPursuitHUDWidget，与木桩血条WBP是两个独立接口。

## 7. 验证入口与限制

新增原生过滤器：`HAL.FutureCreation.SkillDummy`。双端过滤器：`HAL.NetworkSmoke.SkillDummy`（隔离夹具使用 `-HALSkillDummyFixture`，不替代团队地图手测）。

UE **5.8.3-58210709**：最终 **Editor R8／Game R4** 构建通过。**Final_R2共49项全部通过**（47项无警告成功＋2项带警告成功，失败／未执行均0），包括3项新木桩专项、GAS Health／Impact／GF2换车、Stun、油膜、车辆测试工具、Car03／04／05／06及实际D3D12渲染回归。两项警告分别是既有无效／阻挡出生负例的故意Warning，以及龙卷风渲染夹具的D3D12保留虚拟资源258GB超过256GB预算提示；不记成零警告。

**Network_R2**：真实Listen Host／Client各1/1、零测试警告／失败、退出0，Host出站注入80ms延迟。验证每实例TestConfig复制、无Controller／PlayerState情况下目标ASC初始化、实际扣血、六星、共同Stun、逮捕后保护尾段、原生血条Widget绑定、服务器销毁后的双端清理；PlayerArray始终只有两名真实玩家。客户端先预载配置并确认木桩就绪，再由主机通过原主技能请求发动，未伪造客户端命中／效果结果。

开工 **1327/1327** 份既有资产／地图／Config／uproject及Passive源码哈希保持，新增两份原生DA。未修改保存的BP或Fred地图，未提交／Push。原始证据在 `Saved/SkillTestDummy/20261115`，可同步摘要见 [验证记录](Skill_Test_Dummy_Verification_20261115.json)。

早期失败均保留：R1／R2的指针、复制宏及测试API编译问题；R3链接被Editor占用（用户关闭后解除）；Logic_R1暴露空Primary Grant违反正式Definition校验，改为保留有效条目但跳过授予；Logic_R2／Final_R1暴露未初始化测试Actor导致EndPlay不触发，已补完整夹具；旧Health回归引用已删除的基线DA，更新为现有Car02碰撞资产；Network_R1暴露目标保护定时器误用施放者资格，已改为接收资格，并补客户端就绪握手。最终报告覆盖修正后的源码。

**未验收范围**：团队仍需按第4节手动创建BP、在Fred摆放并验证完整画面、真实驾驶／技能接触和手感；自动化不代团队编辑保存的BP或地图。原生油膜夹具显式提供接地遥测，不冒充驾驶实测；双端验证血条Widget实例及视图数据，未对新木桩血条进行逐像素图像验收。自制WBP、四机、打包／Cook、真实晚加入及非本次过滤器涵盖的机关仍待团队验证。新技能的目标判定应复用 `CanReceiveSkillEffects`，施放者继续使用独立的权限／姿态／冷却检查。
