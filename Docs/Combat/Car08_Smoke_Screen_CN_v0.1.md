# Car08 主动技能：扩散黑烟 v0.1

2026-11-17，日本时间。接续 `main/8a97ed1`，保留团队 Car08 资产、胎痕修复及既有未提交讨论稿／忽略规则。用户已确认玩法并授权技能与特效施工。**代码、五份新资源、两 Target 构建及程序验证完成；团队 GA／车型接线、真实管口校准和画面／手感验收待执行。**

## 已确认规则与默认值

2026-11-19 更新：Car08 车型 DA 已授予烟幕为主技能。车辆调参和补能接口见[车辆大招能量](Vehicle_Ultimate_Energy_CN.md)。

- Car08 车头**朝上的排气管**喷烟。喷口跟车，烟团脱离后留在世界中；上升、翻卷和行驶共同形成向后的烟带，不将整片烟云附着车体拖动。
- 喷射 4 s，每段从喷出时起存留 6 s，含末尾 1 s 消散。成功发动消耗 100 点，车型自然回满用 18 s。额外补满后，喷射窗口结束即可再次施放，两次留下的烟团独立消散。原地和倒车均可喷。
- 半径由 100 cm 在 1.2 s 内扩至 350 cm。有效高度首测 300 cm，烟团边缘与消散同时降低视觉浓度和区域强度。
- 只干扰视野，不直接伤害、减速、改变抓地／质量／碰撞或球的状态，其他技能原有规则不因烟自动失效。
- 施放者免疫**自己这片烟的屏幕干扰**，仍看见世界烟云，车身也可被自己的烟遮住；进入别人的烟正常受影响。冲击免疫不等于烟雾免疫。
- 多源屏幕干扰取最强值，设最大强度 0.8，淡入 0.2 s、离开后 0.5 s 恢复；不是任意叠加到完全黑屏。
- 普通阻挡墙体阻断烟雾影响，烟不做流体绕墙模拟。以同一高度内的车辆位置判断，不以俯视相机进入烟为条件。
- 烟中的目标血条、编号与描边隐藏，正常 HP／技能／计时／菜单 HUD 保留；追捕头顶提示也遵守同一隐藏规则。
- 眩晕、腾空时暂停新增烟段；翻覆／自救同样暂停，原发射窗口继续计时。恢复后仅在剩余窗口内继续喷；已有烟团继续计时消散。
- 淘汰、换车、离场、比赛结束时清理所属烟及其影响，能量按车辆和比赛生命周期结算。

初版发动许可沿用铺设型技能：正立接地、非眩晕／自救、Avatar／HP／房间阶段有效时才提交；发动后的暂停不延长原喷射窗口。烟段有生长初期的短渐入，不把刚出生的零浓度当成满浓度烟。

## 技术职责

`UHALSmokeScreenAbility` 和大招基类负责许可、扣能和喷射窗口。服务器 `AHALSmokeScreen` 保存烟段、出生时间、墙体裁剪和各目标独立 GE 句柄。`UHALSmokeObscuredEffect` 授予 Avatar 作用域的 `State.Vision.SmokeObscured`，离开一片烟只移除该来源的贡献。

同一 `FHALSmokePuff` 几何／时钟函数用于权威影响和客户端表现重建。服务器判定包含采样间的高速穿越；客户端局部屏幕可使用已复制烟区和自身预测车位做表现反馈，不能提交命中／修改权威状态。烟段按服务器时间扩张／到期，晚加入不重置寿命。不修改车辆网络物理历史协议。

喷口用 **Rules → Outlet Offset** 表示相对于物理根的 cm 偏移，服务端落地采样和客户端 Niagara 向上喷流共用该值。初值 `(130,0,180)` 只是通用首测占位，必须对照 Car08 实际排气管顶端校准；不会改车型已有 Mesh、骨骼、碰撞、操控或相机参数。

Niagara 实现有体积层次的向上喷流和翻卷烟团。为兑现“盖住球、机关和后处理，而正常 HUD 不遮”，独立的原生 `UHALSmokeScreenView` 以低于普通 HUD 的 PlayerScreen 层绘制**世界投影的、按墙体裁剪的烟团覆盖**，再绘制该玩家的局部全画面烟纹。此层不抢焦点、不改相机距离，不依赖 Niagara 排序值去碰运气盖住后处理。当前为俯视视角设计，不宣称任意相机／VR 的完整体积渲染。

原生房间头顶编号／血条、追捕头顶提示、测试靶血条和 Damage Feedback 描边接入同一只读隐藏策略；正常 HUD、实际伤害及追捕规则保持。已有 Designer 自定义世界目标 Widget 应使用 `HALSmokeScreenView.ShouldHideWorldTarget(Target)` 同步隐藏，代码代理不修改 WBP 图。

## 资产与团队手动接线

资源生成器 `HALBuildSmokeScreenFXCommandlet` 已通过 UE API 创建并保存以下五个新资产，同名已有资源时拒绝覆盖；没有修改任何 BP／地图／既有车型 DA：

- `/Game/Data/Abilities/DA_Ability_Car08_SmokeScreen`
- `/Game/Assets/FX/SmokeScreen/NS_SmokeScreen_UpwardJet`
- 同目录 `M_SmokeScreen_ToonPuff`、`M_SmokeScreen_CloudCoverage`、`M_SmokeScreen_LocalVeil`

本工程现有 GA、技能 DA 与车型主技能槽已经接通。下面保留手动接线步骤，供团队复查或重建资产时使用。

1. 完整编译并重开 Editor。Content Browser → `/Game/Blueprints/GA` → Blueprint Class → All Classes，选 **HALSmokeScreenAbility**，命名 `GA_Car08_SmokeScreen`。
2. 打开该 GA → **Class Defaults → Smoke Screen → Skill Config**，选 `DA_Ability_Car08_SmokeScreen`，Compile、Save；不用写 Event Graph。
3. 打开 `/Game/Data/MVP_B/DA_Vehicle_Car_08` → **Abilities**，保持／迁移至 `Vehicle Definition V1`；把唯一的 `Input.Ability.Primary` 条目的 Ability 指向新 GA、Level=1。不要增加两个 Primary，不改 BP_HALPlayerState。Save。
4. 确认 Car08 实际 BP 的 Definition 指向这份 DA，且房间 Catalog 已登记该车型。若尚未登记，由团队沿现有车型流程手动添加；不覆盖其他车型或修改当前地图。
5. 开技能 DA → **Rules → Outlet → Outlet Offset**。在 Car08 BP 视口确认物理根为局部原点，量取排气管顶端相对于该根的 XYZ，填入同一偏移；保持喷射朝车体上方。运行 `hal.SmokeDebug 2` 后发动技能，橙色箭头应从管口向上。模式 2 暂时绕过本机遮蔽层，便于看见管口／箭头，玩法判定不变；校准后用 `hal.SmokeDebug 0` 恢复正常表现。偏移是物理根坐标，不是直接复制 SkeletalVisual 的骨骼局部坐标。
6. 普通画面不需要新建 HUD／Level BP。若团队制作了自定义头顶 UI，在更新目标数据时调用 **Should Hide World Target**，True 时隐藏目标那一组血条／编号，False 恢复；正常技能栏、自己 HP、菜单不要接这个条件。

调参入口均在本技能 DA：Rules 控制时间、半径、高度、采样和管口；Presentation 控制喷流引用、深浅颜色、流速、浓度、屏幕强度与过渡。浓烟范围与屏幕干扰强度独立调节。

管口量取不方便时，可由团队在 Car08 BP 视口临时添加一个直接挂在 CollisionRoot 下的 Scene Component，移到排气管顶端，读取它的 Relative Location 后填入 Outlet Offset；这个参考点不承载玩法逻辑。颜色／流速等同名材质参数由技能 DA 在运行时写入，调试时优先改技能 DA，不要只改母材质的默认值。

烟幕组件使用 PlayerScreen Z=-1000，自定义驾驶 HUD 使用非负 Z Order。Car08 的主技能资源可在驾驶 HUD 或 `hal.DebugHUD 1` 的大招能量读数查看。

## 团队快速测试

1. 用现有 Development 单人流程，或双进程 Listen 进入 Playing／可驾驶练习状态，选 Car08。控制台 `hal.SmokeLog 1` 后执行 `HALUseSkill`，确认出现 `Smoke started`；请求成功文字不能代替 started。
2. 自己发动：能看见管口喷流和扩散烟团，场景烟可以遮住自己的车，但不会附加自己的全屏烟纹。停止行驶、倒车和转弯时，旧烟团保持世界位置，各自生长／到期。
3. 进入对方烟区：场景有动态灰黑遮蔽，正常 HUD／菜单可见，目标头顶标识隐藏；离开后约 0.5 s 恢复。检查球、机关及受击轮廓没有盖在烟上；不把自己 Debug HUD 中的 HP 列表当作应隐藏的世界血条。
4. 先用普通墙分隔两车，确认不会隔墙干扰。检查不同高度／楼板、窄柱和移动墙的效果与边界，必要时调整 Wall Rays／Wall Refresh Seconds。
5. Host 用 `HALGASStatus` 查真实 PlayerId，再用 `HALStun <PlayerId> 2` 检查喷射暂停；腾空也应暂停，旧烟继续消散，落地／解除后只在剩余 4 s 窗口里继续。不要把数字 PlayerId 写成固定测试值。
6. 两片烟重叠、离开其中一片、淘汰／换车／退出房间及新局后，检查没有残留屏幕层或状态。测试完关闭 `hal.SmokeLog 0`、`hal.SmokeDebug 0`。

## 验证计划与当前限制

- 原生专项：独立烟段出生／消散、高度和边缘、非有限配置、来源叠加、本人免疫、墙阻断、高速横穿、眩晕／腾空暂停和淘汰清理；HP／驾驶数值不被修改。
- 真 RHI：实际 UI 材质输出、动画／覆盖、UI 层顺序以及真实 Niagara 粒子；不以 NullRHI 证明画面正确。
- 双进程 Listen：正常 Primary 请求、双方本人免疫／对方区域遮挡、复制时钟和烟团范围、停发与清理，低延迟及扰动分别记录。
- 团队：真实 Car08 管口、边缘、急转／加减速、遮挡球与电墙、正常 HUD、不同自定义 WBP、四人同时喷烟和发布包。

墙体足迹是有限射线的保守径向裁剪，分辨率在 Wall Rays 调整；移动阻挡物按 Wall Refresh Seconds 更新足迹，并在影响判定时再次做即时墙体检查。采样与绘制有明确预算，不用真实流体。复杂窄柱／急角及四人同放性能仍须实际验收；不能将程序测试扩大为这些画面／性能已通过。

## 本轮实际验证

本机 **UE5.8.2-56702186**，最终 **Editor R11／Game R6** 构建成功。最终运行时在 Editor R10／Game R5 已固定；最后一次仅让网络截图等待烟幕强度超过 0.65，不改运行时或验收断言。未升级本机引擎；近期另一设备使用 5.8.3，跨设备应统一补丁并完整重编，不能沿用本机 DLL。

最终 `FullNative_R2`：`HAL.FutureCreation` **242/242 通过，0 失败／未执行**，其中一项为既有 `VehicleTest.InvalidAndBlockedReplacementRetainsCurrentCar` 地图警告。新增烟区原生专项包含三维墙／楼板阻断、独立来源、自己免疫、暂停与清理；销毁早于 BeginPlay 的情况也会释放 GE，不依赖 EndPlay 必然执行。

最终真实 D3D12／Slate `Render_R5` **1/1**，0 测试警告／失败。两种对比背景证明烟团中心不泄露底图、墙外和范围外保留原背景；在线性色空间测得屏幕层削弱背景对比，**59392** 个场景像素通过、HUD 改变 **0**，动画改变 **14020** 像素。该小样是材质／覆盖与层级验证，不能代替所有机关组合的场景画面验收。

真实 `MVPB_NetTest` 两个 Editor `-game`／D3D12 进程：低延迟 **Low_R2 双端各1/1**、Host 出站 **80 ms 注入 Lag80_R1 双端各1/1**，四份报告均0测试警告／失败、进程正常退出0。80 ms是出站模拟值，不是实测RTT。两端通过正常 Primary 请求发动；夹具仅替换出生实例的技能授予，并在双方发动后由服务器互换位置，使两车进入对方旧烟区；**没有以真实持续驾驶输入验证整条喷烟轨迹**。观察到权威 Tag、本人免疫、Niagara 实际粒子、本地视图、停发后旧烟、无 HP 伤害、冷却与最终清理；已目视检查真实本人免疫及充分显现后的 Client 屏幕截图，正常前景 Debug HUD 保持可见。

开工原有 **1351** 份资产、**4** 份 Config 和 `.uproject` 共 **1356/1356** 逐份 SHA256 不变，仅新增五份技能／表现资源；原有447份Source中仅7份接入标签／隐藏策略／测试访问，新增13份Source。未编辑BP、地图、既有车型DA，未改车辆网络物理格式，未提交／Push。详见[验证摘要与源码／资源哈希](Car08_Smoke_Screen_Verification_20261117.json)。原始报告和截图在本机 `Saved/Car08Smoke/20261117`。

早期失败保留：首轮C++接口错误、生成器的顶点Alpha／Niagara加载模式、测试夹具被回拨到负时间、出生早期销毁漏清来源、渲染对比的sRGB度量，以及首轮Host提前结束使Client超时，均有对应修正与最终复测；不将这些旧轮次记作通过。当前仍待团队接线、管口贴合、连续驾驶／复杂地形／四人同放、完整房间旅行／晚加入、实际丢包／抖动、Cook与发布包验收。
