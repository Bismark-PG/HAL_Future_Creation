# Car03 BattleTop：过渡、重复接触与 Niagara 首版

日期：2026-11-13。规则来源：[已确认规格](../Combat/Car03_Battle_Top_Ability_Design_CN_v0.1.md)。代码、首版资源及程序验证完成；团队接线和画面／手感验收待完成。

**2026-11-14 更新：柔和卡通火花与立体气流已完成程序／保存资源／技能 DA 迁移及本机专项验证，见[当前版本配置与验收](Car03_BattleTop_Cartoon_FX_CN_v0.1.md)。** 使用五个新的 ToonWind／ToonContact／WindArc 资源，保留 Contact Count=7 和 Ground=None；旧资源保留。UE5.8.3 Editor／Game 构建、最终16项规则与实际渲染、双端原接触观察入口专项已通过；团队风格与真实碰撞画面仍待验收。以下“未施工”“先改32”等段落保留各自历史时点，不代表当前待办。

## 柔和卡通造型与立体气流方案讨论（2026-11-13）

团队现已把保存技能 DA 的 Contact Sparks 切换到 `NS_BattleTop_ContactSpray`，只读核对 Contact Count仍为7。用户确认效果有进步、强度可以，但火花样式偏写实；暖色风环也存在类似风格问题，且更像车灯转出的光环、缺乏立体气流层次。下节“尚未切换／先改32”是此前快照，本轮不将增加数量作为待办。

只读检查保存材质与当前生成／运行源码后，原因与建议如下，均为方案，尚未施工：

- 接触火花目前为沿速度拉长的Sprite，材质使用浅色热芯、连续橙红渐变及较强发光，容易呈现金属灼热飞屑的感觉。建议保留当前触发、数量、尺寸／速度／散射强度，先将单粒造型改为圆头、短粗、略弯的飞溅笔触，辅以少量简单小块；以两／三档概括颜色、克制的亮面和圆润轮廓统一画风。柔和感来自造型与色差，保持清楚分色。短促出现、收缩退场的动画可进一步强化卡通节奏。
- 风环目前由一个固定朝向的平面Sprite承载，三组窄弧都在同一平面，材质 SpinAngle 直接跟随外观角度；弧段以窄径向渐变和高光细线表达，缺少真实高度、弧面法线和各段独立生消。因此主要需要几何与运动层次，而不是继续加亮或加转速。
- 建议风环采用Niagara的少量弧形Mesh粒子：以有轻微弧面截面的宽条气流片，形成低层／略高层、不同半径和倾斜的开放旋流。弧段长短、粗细与相位错开，留出缺口及车身中心；每段经历出现、绕行推进、少量外扩／上浮和收尾，避免所有段固定成一整圈同步旋转。第一轮可先试3～5个清楚的大弧段，再按比赛镜头调整。
- 气流速度建议与车身转速独立，在同一技能DA暴露风带宽度、层高／倾斜、流动速度、数量／密度和发光强度。暖色系保留，弧面按表面朝向做暗／中／亮分色，发光以辅助可读为目标。Unlit仍可做方向分色，真实网格弧度／高度提供轮廓与遮挡关系。

实现边界：Niagara负责形状、颜色和粒子运动；C++仅补充技能DA到表现系统的参数／时间桥接并沿用起止与清理。GAS、伤害、每目标CD、碰撞资格、反向切向喷射方向及球规则保持。优先做接触火花造型，再做立体气流；不把额外烟团、完整旋风柱或全屏后期加入首版范围。

原始只读报告在 `Saved/BattleTop/StyleReview_20261113/Review.json`。本轮未保存资源、修改代码、构建或运行渲染／联机测试，不把用户对强度的认可扩展为新风格或完整工况验收。

## 当前接续核对（2026-11-13）

最新 `main/69a16d5` 已提交首版和接触飞溅加强的源码／资源。只读加载当前保存配方，确认以下实际状态；没有保存或修改资产。

| 项目 | 当前核对结果 |
| --- | --- |
| 接触专用资源 | `NS_BattleTop_ContactSpray`／`M_BattleTop_ContactSpray` 已存在且可加载 |
| 技能 DA / Contact Sparks | **仍引用 `NS_BattleTop_Sparks`**，新资源尚未接入 |
| Contact Count | **7**，尚未迁移为加强方案的32 |
| Ground Sparks | **None**，保留团队当前隔离选择，不自行恢复 |
| 风环 | Wind／Wind Material 已引用；Radius Scale=1.45，Opacity=0.55 |
| 时间／转速 | 持续10 s，1500°/s，进入0.15 s／退出0.2 s |
| 重复命中 | 每目标0.8 s CD |
| 加强字段 | 可读到12×40～22×85 cm、0.30～0.55 s、650～1700 cm/s、65°、重力1／阻力1.8的字段值 |

因此当前准确阶段为：**加强版代码与资源已落地，待保存配方切换和当前版本验证**。下文“尚未生成资源／Editor PID 43740”保留原施工时点，不作为当前状态。

原验证摘要的16/16及双端结论来自 `eb1079c`、UE5.8.2-56702186；当前本机为UE5.8.3-58210709，且 `HALBattleTopRenderTests.cpp` 哈希已与该摘要不同。本轮未重建或执行新渲染／玩法测试，不把旧结果扩大到接触加强版。只读核对报告位于 `Saved/BattleTop/Progress_20261113/Saved_Recipe_Audit.json`。

下一施工先仅将 Contact Sparks 切到专用 ContactSpray、Contact Count设为32，保留其他保存字段；然后核对当前两Target构建，运行新接触散射／轨迹／像素及相关规则／双端清理专项，再在真实碰撞中验收更大、更密、更分散的观感。当前监听GameMode BP与NetTest地图的未提交修改保持。

## 行为与职责

- 进入默认 **0.15 s**、退出 **0.2 s**，均包含在原持续时间内。平滑外观自转和气流，退出时归回物理车头朝向；不缩短伤害窗口，不按外观转速缩放伤害。两段之和大于技能时长时按比例压缩。
- 每目标独立 **0.8 s** CD。伤害和技能额外推送一起限频，自然刚体碰撞不变。持续贴靠由服务器每 0.05 s 用实际碰撞凸包复查，不要求重新撞击；离开重进保留该目标 CD。每次合法命中使用独立攻击 ID，复用发动时的来源／伤害快照。
- 气流按复制的服务器起点、时长和转速重建；独立的物理车头和驾驶规则不变。接触火花由服务器发送带发动 ID 的限频不可靠事件；丢包只影响少量粒子，不影响命中。过期／前次发动事件丢弃。
- 接触火花沿相对接触点运动在接触面投影的反方向，包含外观自转切向速度，额外加入少量离面散射。火花不表示本次一定扣血；伤害 CD 和火花间隔独立。
- 底部火花为各渲染端本地需求：接近静态地面且移动时，每次 2 粒。原地／腾空关闭；自救暂停接触结算及两类火花，保留自转／气流。眩晕保留技能；到期、淘汰、换 Avatar 或退出清理。

原生 `HALBattleTopComponent` 管阶段、服务器接触和表现生命周期；GA／GE 仍管许可、持续与冷却。Niagara 管粒子运动及绘制。没有新增逐帧 GA、蓝图规则或物理根旋转。

## Editor 接线（原技能 DA／GA 可沿用）

1. 关闭旧游戏进程，重开本次构建的 Editor。Content Browser 搜索现有 **DA_Ability_Car03_BattleTop**，打开该技能配置；不要改车型的基础移动 DA。
2. **Contact → Target Cooldown Seconds** 先填 `0.8`。保留已调好的伤害、冲击档位、推送预设及球速。
3. **Presentation** 中填写 **Spin Enter Seconds = 0.15**、**Spin Exit Seconds = 0.2**；**Spin Rate** 沿用已保存值。
4. 展开 **Presentation → FX → Systems**，从 `/Game/Assets/FX/BattleTop` 选择：

   | 字段 | 资源 |
   |---|---|
   | Wind | `NS_BattleTop_Wind` |
   | Contact Sparks | `NS_BattleTop_Sparks` |
   | Ground Sparks | `NS_BattleTop_Sparks`（与接触火花共用，运行时数量／速度不同） |
   | Wind Material | `M_BattleTop_Wind`，也可以右键该材质创建独立 MI 后引用 |

5. **Look** 首测保留琥珀橙／珊瑚红／浅金默认色；Opacity `0.55`，Radius Scale `1.15`。半径按实际车型外观 Bounds 计算，不用每车另做系统。颜色为线性 RGB；只影响本技能实例，运行时不会改母材质资产。
6. Save。现有 `GA_Car03_BattleTop → Skill Config` 和 Car03 Definition 的 Primary Grant 若已正确，不需重新创建或修改。没有 FX 资源仍可使用技能，只是不显示相应粒子。

## 常用调参

| 字段 | 默认值／意义 |
|---|---|
| Contact Interval | `0.08 s`；持续摩擦的火花限频，与 0.8 s 伤害 CD 无关 |
| Minimum Slip Speed | `80 cm/s`；相对切向运动门槛，包含自转 |
| Contact Count | 每次 `7` 粒，范围 1–24 |
| Ground Interval | `0.16 s`；底部每次仅 2 粒 |
| Ground Minimum Speed | `150 cm/s`，即 1.5 m/s |
| Ground Distance | 碰撞根底部向下 `30 cm` 内有静态地面才显示 |

风环分为较淡宽带与亮细弧，保留透明中心和车辆轮廓。需要更淡先降 Opacity；范围不合适先调 Radius Scale。火花的同车并发实例上限 24，每个最多 0.6 s 后销毁，技能结束立即清理；不依赖循环发射器自行停止。

## 资源维护

首版资源由 Editor 模块 `HALBuildBattleTopFXCommandlet` 通过 UE 材质／Niagara API 创建，生成代码作为可追踪配方留档。工具只创建上述四个新资源，遇到同名资源直接拒绝覆盖，不修改 BP、技能 DA 或地图。正常使用不需要运行工具，也不要在团队调过粒子后重新生成覆盖。

Niagara 参数合同：风环使用 `User.WindMaterial`、`User.WindSize`（二维直径，cm）；运行时 MID 输入 `SpinAngle`（圈数）、`Envelope`、`Opacity` 与三个颜色。风环位于实际外观中心，直径显式传入 Sprite Size，避免模型原点和组件缩放造成地下／过小的风环。火花使用 `User.BurstCount`、`User.VelocityMin/Max`（局部 cm/s，局部 Z 为喷射方向）、`User.SparkColor`。更换 Niagara 时保留这些语义；判定和伤害不能移入粒子系统。材质未显式覆盖时，优先使用风环资源中 `User.WindMaterial` 的默认材质。

## 验证与团队接续

本机 UE **5.8.2-56702186**。最终 Editor R13／Game R2 构建通过；统一专项 **16/16**，0 测试警告／失败，包括 BattleTop 持续贴靠、独立 CD、离开重进、过渡／归位／序列化，车型技能授予、Combat 和 Car04 回归。真实 D3D12 检查保存的 Niagara 和材质：暖色像素、旋转、淡出、显式尺寸变化及定向火花均通过，并检查了实际 Car03 场景截图。首次持续接触夹具的玩家 ID／非固定碰撞体、Niagara 枚举格式和火花 RGB／Alpha 接线问题均在最终复测前修正，未掩盖失败记录。

当前 NetTest **双进程 Listen Server** 的低延迟和 Host 出站 **80 ms** 注入两轮，两端均各 **1/1**，0 测试警告／失败；覆盖双方实际技能请求、拥有者与观察者自转／气流各一实例、重复输入、到期归位／粒子销毁与冷却。采用仅影响进程的测试配方，保存的 GA／DA／BP 未代填。最终 1265 份原有保存资产＋4 份 Config＋uproject 共 **1270** 项与开工逐份一致，新增只有四个 FX 资源。详细摘要：[验证与哈希](Car03_BattleTop_Polish_Verification_20261113.json)。原始日志／报告／截图在 `Saved/BattleTop/Polish_20261113`。

本轮未重跑全量原生套件；不将以前的旧路径测试失败记为本次通过。接触火花 RPC 的真人观感、移动中的底部火花门槛、复杂碰撞、四台真实电脑和发布包尚待团队专项验收。程序像素检查和双端起止检查不能替代这些验收。

团队接线后重点看：进入／退出是否足够快且不跳头；持续贴靠能按 CD 重复伤害；墙／车火花是否沿面反向；底部火花是否稀疏且离地关闭；Host、本机预测车和远端车的自转／气流起止是否一致。真人手感、四机和发布包仍按项目收尾验收。

## 配置后反馈：撞墙火花与气流强度（2026-11-13）

团队反馈普通围墙、机关及单人／Host 均看不到接触火花。只读检查当前保存技能 DA：四个资源槽均正确，Spin Rate=1500°/s、Duration=10 s，Opacity=0.55、Radius Scale=1.15，接触间隔=0.08 s、数量=7、最低切向速度=80 cm/s。不是简单漏配资源。本轮未改源码、保存资产或进行新构建。

检查期间 `NS_BattleTop_Sparks` 于 16:26 保存，哈希已不同于原验证摘要；保留当前资源，不覆盖。针对当前文件重新运行真实 D3D12 `HAL.RenderSmoke.BattleTop`，**1/1、0 测试警告／失败**，火花输出仍为 72 个有效像素（固定近景夹具）。这说明当前粒子资源能输出，不代表真实碰撞已经触发。报告为 `Saved/BattleTop/Feedback_20261113/Render_Current/index.json`，当前资源 SHA256=`892CEE13EEFC19DE9A344852FFE088364828277DB1F68BFC09FCC9A226DE4E90`。

早先独立 Editor 脚本对照只观察到风环／底部火花，没有识别到墙面接触火花。当时将其判为“触发缺失”的结论过早：后续团队按下方隔离检查确认 **# Particles > 0，但仍不可见**，在该实测案例中已经产生粒子，应转查显示层，不能继续以脚本未观察到证明没有触发。早期脚本 API／碰撞观察尝试不计为通过，记录位于 `Saved/BattleTop/Feedback_20261113`。此前 16/16 仍不替代真实场景可见度验收。

当前排查入口以团队粒子数证据为准：先做接触专用资源的尺寸／寿命对照，再查世界位置、深度遮挡、材质和渲染器。当前严格普通墙分类对部分机关的适用边界不变，但未被证实与本次“有粒子而不可见”有关，不据此修改碰撞资格或玩法。仅对另行出现“没有实例／粒子”的案例，再补 Hit→分类／阈值→事件→Spawn 诊断。

气流可立即在 **技能 DA → Presentation → FX → Look** 调整：

| 目标 | 参数 | 首轮对照值 |
|---|---|---|
| 更大覆盖范围 | Radius Scale | 1.15 → **1.45** |
| 风带更明显 | Opacity | 0.55 → **0.85** |
| 更亮、更有发光感 | Main／Accent／Highlight Color 的线性 RGB | 保留色相，分别乘 **1.5**；如 Main=(1.5, 0.375, 0.0375) |

一次改一项并重新发动技能观察。颜色和 Opacity 请在技能 DA 改：运行时会将这些值写入动态材质，MI 中同名值会被覆盖。当前 Spin Rate 同时控制车身和气流旋转，已为 1500°/s，不建议用继续加转速替代强度调节。首版为平面弧形风环；独立风速、风带宽度、带数和独立发光倍率尚未参数化，不能通过不存在的 Wind Strength 配置来调。

生成首版的火花规格偏保守：Sprite Size=3×18 cm，Lifetime=0.22 s。在俯视小窗口中可能太细、太短，尚未确认是唯一原因。团队已确认有粒子，当前优先做以下可见度对照。

1. 停止游戏。在 `/Game/Assets/FX/BattleTop` 复制 `NS_BattleTop_Sparks` 为 **NS_BattleTop_ContactSparks**。
2. 打开副本，选中发射器，展开 **Particle Spawn**，找到列出 `Particles.SpriteSize`／`Particles.Lifetime` 的参数设置模块。先设 **Sprite Size X=10、Y=50 cm，Lifetime=0.35 s**，Compile／Save。尺寸在此 Niagara 模块修改，修改系统概览中的 User.BurstCount 默认值会被运行时覆盖。
3. 技能 DA → Presentation → FX → Systems → **Contact Sparks** 指向副本；Ground Sparks 保持手动隔离检查中的 None，待对照结束恢复原系统。**FX → Contact → Contact Count** 可先试 **16**；Save。颜色、运动速度、伤害和碰撞资格保持当前配置。
4. 重启测试。调试 HUD 筛选改为 `fx.Niagara.Debug.Hud SystemFilter=*BattleTop_ContactSparks*`，确认新系统有粒子后再观察实际墙面。旧的 `*BattleTop_Sparks*` 筛选不匹配这个副本名称。
5. 放大后可见，则继续调整尺寸／寿命／数量到正常强度；仍完全不可见，则下一步查世界位置和深度遮挡／渲染器，不能据此转回“未触发”的结论。不要将关闭深度测试当成正式修复。

副本只赋给 Contact Sparks，保留底部火花原样。运行时 0.6 s 会销毁单次火花组件，粒子寿命不应超过这个边界。本轮仅同步团队实测结论、核对生成配方／资源哈希并更新操作留档；未代改 Niagara／DA／BP、未重编或新增测试通过结论。

## 撞墙火花的手动检查（2026-11-13）

当前没有专用的火花逐段诊断命令。`hal.AbilityLog 1` 只证明技能许可／开始／结束；`hal.CombatLog` 不能覆盖所有纯表现火花。以下内置 Niagara HUD 命令名及参数已核对本机 UE5.8.2 源码，本轮只补操作说明，未新增场景测试或修复结论。

1. 停止游戏。打开技能 DA → Presentation → FX → Systems，记录 Ground Sparks 原引用后，**暂时清为 None**；Contact Sparks 保持 `NS_BattleTop_Sparks`。Save。气流与伤害配置不动。此举区分共用同一 Niagara 资源的底部／接触火花，不是修复。
2. 使用 `/Game/Maps/MVPB_NetTest`，一名玩家在 Editor 内通过 Play 启动 PIE，便于同时看 Output Log。若处于 WaitingForPlayers，用 `HALStartMatch` 开局。Standalone 也可用下面命令，但其日志需看对应进程在 Saved/Logs 下的文件。
3. 游戏控制台逐条执行：

   ```text
   hal.AbilityLog 1
   fx.Niagara.Debug.Hud Enabled=1 OverviewEnabled=1 OverviewMode=0 FiltersEnabled=1 SystemFilter=*BattleTop_Sparks* ShowRegisteredComponents=1 OverviewLocation=20,250
   ```

   第二条是一整行。HUD 若未显示，先执行 `fx.Niagara.Debug.Hud`，检查控制台是否列出内置帮助／是否提示命令不存在；不要将 HUD 未显示当成火花没有触发。
4. 发动 BattleTop。Output Log 搜索 `BattleTop started Pawn=...`，在 `BattleTop stopped` 前分别测试轻微擦墙、带一点沿墙移动的撞墙、持续贴靠。先用四周普通围墙，避免机关伤害／眩晕干扰。`HALUseSkill: Requested=1` 只表示请求提交，不能替代 started。
5. 观察 HUD 中 `NS_BattleTop_Sparks` 的 **# Registered／# Active／# Particles** 数字是否变化；只出现名称不算创建证据，因为概览可能保留已有系统条目。火花寿命短，建议重复几次或录屏后看数字。

   | 现象 | 能得到的结论 |
   |---|---|
   | 多次碰撞仍未观察到 Registered／Active 增加 | 未观察到实例创建；接下来查 Hit→资格／阈值→事件→Spawn 各段，不再反复调整颜色 |
   | 实例增加，Particles 一直为 0 | 已创建组件，但发射／模拟／裁剪等仍需检查，不能据此认定粒子已可见 |
   | Particles 大于 0，但墙边看不见 | 已有粒子，接下来查尺寸、位置、遮挡、透明度和材质 |

   HUD 只反映客户端／本机的粒子状态，不能直接指出 C++ 提前 return 的具体原因。这里已是单人／Host 复现，可先排除远端传输差异；最终根因仍须结合原生诊断。
6. 检查后恢复 Ground Sparks 原资源并 Save；关闭显示与临时日志：

   ```text
   fx.Niagara.Debug.Hud Enabled=0
   hal.AbilityLog 0
   ```

后续回报记录：是否看到 started、三种接触分别是否增加实例／粒子、粒子是否实际可见。无需先改质量、碰撞体、伤害 CD 或其他玩法参数。

## 接触飞溅加强（2026-11-13，施工中）

团队已通过放大对照看到火花，新增需求是更大、更密、角度更散。当前源码已接入可调接触飞溅与新资源生成配方，**尚待 Editor 关闭后重编／生成／验证，不是已完成结论**。

接触专用资源计划为 `NS_BattleTop_ContactSpray`／`M_BattleTop_ContactSpray`，保留当前原火花资源的调整。显式迁移只修改现有技能 DA 的 Contact Sparks 引用及 Contact Count=32，其他保存参数通过字段逐项比较保护。底部原资源／关闭选择沿用团队当前设置。

技能 DA 的 FX → Contact Spray 新增：随机尺寸下限 12×40 cm、上限 22×85 cm；寿命 0.30–0.55 s；沿主切向速度 650–1700 cm/s；散射半角上限 65°；视觉重力倍率 1、阻力 1.8/s。可配置，均为纯表现。尺寸／寿命由 Niagara 逐粒随机取样，快慢与粗细产生层次；亮金核心、金橙外缘及到期淡出由新材质执行。

主方向仍为反向接触切向运动。事件增加表面法线，建立“切向前进／离开表面／偏上”的喷射坐标系；侧向分量按半角约束，避免朝墙内发射，重力从世界向下变换到该局部坐标系。新 Niagara 消费大小、寿命、速度、重力和阻力参数。粒子仍用世界空间位置的独立组件，不跟车拉动。接触间隔、资格、伤害、推送、球、气流及物理运动规则不变；Host／Client 须使用同版二进制。

验证计划：几何半角／朝外／重力坐标与配置边界；真实 D3D12 粒子可见度、广角相对窄束、阻力／下落轨迹与自然消失；已有 BattleTop、技能授予和战斗回归；双端起止／资源引用与清理检查。保存资产有备份及哈希保护，生成工具拒绝覆盖同名资源。
