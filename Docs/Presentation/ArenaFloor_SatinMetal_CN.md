# MVPB_Match 波纹缎面金属地面

正式比赛图 `/Game/Maps/MVPB_Match` 已采用六边形拉丝钢板、整边防滑条、外圈灯带和九盏嵌地灯。中场的第十盏灯嵌在与行车面齐平的金属圆盘中央。正式图与临时图 `MVPB_Match_Temp` 共用可见地板和灯壳网格，正式图独立配置行车面、外环钢板和夜场灯光。[观众身体摇摆](#arena-crowd-sway) · [前五排欢呼与后仰角度](#arena-five-row-crowd) · [八段闪光与烟花](#arena-full-cheers) · [八段星际观众](#arena-full-crowd) · [外环与南侧看台](#arena-night-rim) · [行车碰撞](#arena-flat-drive-surface) · [比赛图灯光](#arena-match-lighting) · [六边形钢板与地灯调参](#arena-deck-inset-lights)

<a id="arena-crowd-sway"></a>

## 八段看台前五排的身体摇摆

前五排的观众沿座排左右轻晃，腰部贴住座位。头、肩、手和举起的道具随上身移动，布料还会独立飘动。相邻人群的起拍时间约错开三分之一个周期，再沿座排交替提前、延后。每排有自己的基础速度，每小组也有轻微变速和不同摆幅。靠赛道的前两排同时向后倾斜 28°，第六排起静止。[同机位开关对照与贴边实拍](Images/MVPB_Match_CrowdSway_20261123.webp)

在八段共用的 `/Game/Assets/Materials/ArenaNight/MI_ArenaNight_SouthCrowd_Motion` 中展开 **Body Sway**。调整参数时先勾选左侧覆盖框。

| 参数 | 正式值 | 调节效果 |
|---|---:|---|
| `Body Sway Enabled` | 开启 | 单独开关身体摇摆，可用于同镜头对照。 |
| `Body Sway Angle` | 5° | 单侧最大摆角，范围 0–8°。各组按上限的 65%–100% 取值，相邻组之间平滑过渡。 |
| `Body Sway Period` | 2.5 秒 | 完成一次左右往返的基准时间，范围 0.5–6 秒。各组在基准上下 12% 内错速。 |

身体摇摆和道具共用 `Front Row Motion → Cheering Rows Upper Height` 的排数边界。材质在顶点上计算节奏，在取样前按人物高度横移图集坐标。腰线以下的位移为零。颜色和透明轮廓来自同一次取样，全场沿用 4,240 个观众顶点和原有的 1024 × 512 图集。想单看摇摆幅度时，可暂时关闭 `Animate Front Row Props`，并在护栏旁观察举围巾观众的头部相对座位的位移。

**逐排实拍。** 以固定近景、平光分别拍摄八段的第一至第六排，每处 25 帧。前五排共 40 处都测到面部横移，头部左右两端相距 4.72–13.60 px，腰部参照区的像素差均为零。第六排的面部和道具参照区也都静止。另在西侧第四排固定机位连续跟踪四名观众 4.08 秒，其中一对在 47 个重叠的四分之一秒观测窗里有 25 个朝相反方向移动。

**单独开启身体摇摆的帧耗。** UE 5.8.3、RTX 5070 Ti、D3D12、1920 × 1080。两组都开启前五排的道具动作、28° 后仰和八段看台氛围特效。两次独立启动，交换开关顺序，三个机位各四对采样，共 26,801 个有效帧、140.34 秒。下表差值在每对内计算后取中位数。全场俯视的四对样本有正有负，差值落在重复测量的波动内。

| 机位 | 开启后的整帧 GPU 中位数 | 摇摆带来的 GPU 配对差值 |
|---|---:|---:|
| 西侧护栏旁的驾驶镜头 | 4.968 ms | +0.026 ms |
| 看台近景 | 4.928 ms | +0.011 ms |
| 全场俯视 | 3.935 ms | −0.003 ms |

另用相同机位做两对虚拟阴影贴图计数。开关两组的每帧动态失效页数中位数在贴边、近景、全景分别为 321、391、143，静态失效页数均为 0。三个机位的阴影深度绘制调用数中位数分别为 50、36、14，两组读数相同。阴影深度绘制的 GPU 配对差值分别为 +0.001、+0.004、+0.001 ms。学校设备的四人混战帧耗待实测。

正式图在独立进程重载后核对了八段材质绑定。Windows 正式图 Cook 共处理 852 个包，零错误、零警告，产物包含正式图、观众母材质、动态实例和图集。本机逐排读数、同框错峰读数和两项性能报告位于 `Saved/StadiumSway/{motion.json,groups.json,performance.json,shadows.json}`，烹制日志在同目录 `FinalCook.log`。

<a id="arena-five-row-crowd"></a>

## 2026-11-23 全场前两排后仰与前五排欢呼

本节实拍与性能数据记录加入身体摇摆以前的版本，用于单独核对观众后仰和布料动作。现行版的身体参数及额外帧耗见[观众身体摇摆](#arena-crowd-sway)。

正式图 `MVPB_Match` 的八段看台从赛道向外数，第一排的围巾和第二排的旗帜随观众面片向看台后方倾斜 **28°**，转轴位于腰部与座位交接处。第三至第五排直立，各段按排交错安排围巾组和旗帜组，沿用原图集中穿插的普通观众和空位。挥旗和举围巾都只动布料，时序按看台、座排和人群组错开。第六至第十三排使用原来的静态观众。[西侧贴边、第三至第五排动作与西北接缝实拍](Images/MVPB_Match_FiveRowsCrowd_20261123.webp)

八段共用 `/Game/Assets/Materials/ArenaNight/MI_ArenaNight_SouthCrowd_Motion`，资产名沿用南侧试点。东南、西南、东北、西北四段在看台静态网格组件的 `Element 4`，另四段在 `Element 5`。在实例的 `Seat Geometry → Front Row Lean Degrees` 调前两排的后仰角。在 `Front Row Motion` 调 `Flag Tip Travel`（旗尖摆幅）、`Scarf Sag`（围巾弯曲）和 `Cheer Tempo`（节奏）。`Animate Front Row Props` 可单独切换动作以作对照。`Cheering Rows Upper Height` 是相对于现有导入网格轴点的排底高度，单位为厘米。正式值 2345 位于第五排与第六排之间。

逐段读取了实际网格和正式图材质绑定。全部 40 个活动座排各实拍一组道具，固定镜头下每组取 16 帧、覆盖 4.17 秒。隔离灯光影响后，40 个道具采样区均出现形变，40 个面部参照区及八段第六排的静态参照区最大通道差均为 0。正式图 1600 × 870 的西侧贴边驾驶实拍中，同一名观众的青绿色衣服从约 17 px 高增至 38 px。另以真实驾驶镜头从西侧开过西北段接缝，行驶 18.38 米。关卡重载核对了 159 个 Actor 及组件。全场继续复用 3,824 个观众三角形和一份 1024 × 512 图集，材质每像素采样一次。

**全场性能。** 对照组为改动前的正式配置，仅南侧两排有道具动作。两组均开启八段闪光与小烟花。在 RTX 5070 Ti / Core Ultra 7 270K Plus 上，以 D3D12、1920 × 1080、100% 渲染比例独立启动两次，调换采样次序。每个机位四对样本，去除每段首尾各 0.2 秒后，共 27,244 帧、140.32 秒。差值为各对差值的中位数。

| 机位 | 新版整帧 GPU 中位数 | 整帧 GPU 增量 | 整帧时长 P95 增量 |
|---|---:|---:|---:|
| 西侧护栏旁的驾驶镜头 | 4.912 ms | +0.079 ms | +0.045 ms |
| 看台人物占据大半画面的近景 | 4.904 ms | +0.325 ms | +0.372 ms |
| 全场俯视 | 3.960 ms | +0.008 ms | +0.013 ms |

近景的四对 GPU 增量为 +0.291 至 +0.437 ms，贴边驾驶为 +0.026 至 +0.138 ms。学校设备上的四人混战帧耗待实测。本机逐排读数、驾驶画面量测与逐段帧表位于 `Saved/StadiumFiveRows/{motion_check.json,edge_check.json,performance.json}`。

UE 5.8.3 的 Windows 单图 Cook 完成，零错误、零警告，产物包含正式图、观众母材质、动态实例和图集。本机 Exhibition 一主三客完成首局返房及第二局开赛，四端均通过且零测试警告。日志与结果分别位于 `Saved/StadiumFiveRows/Cook.log` 和 `Saved/StadiumFiveRows/Network/Summary.json`。

<a id="crowd-view-angle"></a>

## 比赛镜头下沿的观众可见性试验

UE 5.8.3，在正式图的独立 D3D12 游戏进程中，将车辆固定在西侧护栏旁（X −7760、Y 1532 cm）。玩家相机俯角 80°、水平视场角 90°，分辨率 1600 × 870。西侧第一、二排的原面片竖直，顶部与底部处在相同半径，材质已开启双面。这个角度里，前景台阶上的人影清楚，彩色人物自身却压成细条。

当时在运行时替换西侧观众材质槽，让前两排绕腰部与座位的交界向场外后仰。以同一组青绿色衣服的像素纵向跨度衡量（去掉上下各 1% 的色样），直立约 17 px，18° 约 31 px，28° 约 39 px。团队随后选定 28° 作为全场前两排的正式角度。[同机位、同裁切的三档实拍](Images/MVPB_Match_CrowdSightline_20261123.jpg)

<a id="south-front-row-motion"></a>

## 2026-11-23 南侧前排挥旗与举围巾

南侧第一排的围巾在两手之间松下再绷平。第二排的旗面从旗杆向旗尖传递波动，挥几下会歇一拍。相邻组各自起拍。开到南侧护栏旁可以看到两个动作，近景能分辨布料和手的相对运动。[南侧前排、贴边停车与行驶实拍](Images/MVPB_Match_SouthCrowdMotion_20261123.webp)

在正式图搜索 `stand_001_0118`，打开 `StaticMeshComponent → Materials → Element 5 → MI_ArenaNight_SouthCrowd_Motion`，展开 `Front Row Motion`。勾选参数左侧的覆盖框即可调值。

| 参数 | 试点值 | 调节作用 |
|---|---:|---|
| `Animate Front Row Props` | 开启 | 关闭即可对照静态版。试点时其余七段使用共享母材质的静态分支。 |
| `Flag Tip Travel` | 8 | 旗尖单侧摆幅，单位为原图集纹素，范围 0–12。旗杆旁的布料位移较小。 |
| `Scarf Sag` | 5 | 围巾中部最大下垂量，单位为原图集纹素，范围 0–6。两端和手的位置固定。 |
| `Cheer Tempo` | 1.0 | 播放速度倍率，范围 0.2–2.0，同时缩放动作和休息时间。 |

材质先按原有座排选择图集坐标，仅在两种道具的局部区域偏移取样，颜色与透明轮廓共用一次取样。试点复用原网格和 1024 × 512 图集。动作随观察深度在 46–95 米之间逐渐减弱。若美术重排图集中的道具，需同步调整母材质 `M_ArenaNight_SouthCrowd` 中 `Front row scarf and flag, anchored in atlas texels` 节点的定位范围。关卡重载核对了 159 个 Actor，场景配置差异集中在南侧看台的一个材质槽。

**动作验证。** 在 1280 × 720 的南侧固定近景中，连续 85 帧采样里，旗尖附近的亮色条纹上下行程为 17 px，围巾中段上沿行程为 7 px，静态对照均为 0 px。实拍中能分别找到只动旗面、只动围巾的时刻。另用无光照画面隔离场灯变化，九处固定采样区覆盖握旗手、围巾两端、脸、身体下缘和邻座，逐像素对照的最大通道差为 0。驾驶实拍使用玩家自己的比赛镜头，沿南侧护栏实际行驶 38.4 米。

**南侧单段的性能。** UE 5.8.3，RTX 5070 Ti / Core Ultra 7 270K Plus，D3D12、1920 × 1080、100% 渲染比例。保留八段正常的闪光与烟花，同机位轮换施工前保存的静态材质和南侧动态实例。两次独立启动调换次序，三个机位各做四对采样，首尾各去掉 0.2 秒，共 27,316 个有效帧。表中的差值先在每一对内计算，再取四对的中位数。

| 机位 | 动态版整帧 GPU 中位数 | 动态相对静态的 GPU 差值 | 整帧时长 P95 差值 |
|---|---:|---:|---:|
| 南侧贴边驾驶机位 | 4.881 ms | +0.019 ms | +0.027 ms |
| 南侧前排近景 | 4.665 ms | +0.038 ms | +0.085 ms |
| 全场俯视 | 3.902 ms | +0.009 ms | −0.066 ms |

贴边驾驶的四对 GPU 差值范围为 −0.049 至 +0.030 ms，近景为 +0.020 至 +0.118 ms。整圈推广后的实测见上方「全场前两排后仰与前五排欢呼」。南侧试点的 Windows 单图 Cook 零错误、零警告，产物包含动态实例、共享母材质、图集与正式图。Exhibition 本机一主三客完成返房和第二局开赛，四端零测试警告。试点的像素对照、帧表与四端报告分别在本机 `Saved/StadiumCrowdMotion/visual_checks.json`、`performance.json` 和 `Network/Summary.json`。

<a id="arena-full-cheers"></a>

## 2026-11-23 八段看台的闪光与小烟花

八段各使用一处前排烟花落点，座位间的暖白短闪会在各自约 13.2 米宽的范围换位置。七处新落点沿各段前排内缘取样，分别调整沿座排的位置和高度，金色火花朝看台展开。八段共用南侧的 `NS_ArenaNight_FrontRowCheers` 和两份粒子材质，本地播放。独立载入正式图，确认八个实例均关闭碰撞、投影和网络复制，原有 152 个 Actor 及其配置通过对照。正式图现有 159 个 Actor。[东侧与东北侧同镜头、东侧近景实拍](Images/MVPB_Match_FullStandCheers_20261123.webp)

**在 Editor 调整节奏。** 在 World Outliner 搜索 `FrontRow_Cheers`，八个 Actor 都在 `Presentation/Grandstand Ambience`。挪动单段 Actor 可调整烟花落点和闪光区域。选中它的 Niagara 组件，`Random Seed Offset` 控制实例随机偏移，`Warmup Tick Count` 控制启动时预先推进多少步。七个新增实例每步 0.16 秒。各自启动后，闪光每轮随机取 0.82–1.50 秒，烟花每轮随机取 4.1–5.4 秒。

| 方位 | Actor 名前缀 | 预热步数 | 起始提前量 | 实例随机偏移 |
|---|---|---:|---:|---:|
| 南 | `South` | 0 | 0 秒 | 0 |
| 东南 | `SouthEast` | 13 | 2.08 秒 | 113 |
| 东 | `East` | 4 | 0.64 秒 | 307 |
| 东北 | `NorthEast` | 22 | 3.52 秒 | 569 |
| 北 | `North` | 9 | 1.44 秒 | 863 |
| 西北 | `NorthWest` | 26 | 4.16 秒 | 1097 |
| 西 | `West` | 17 | 2.72 秒 | 1367 |
| 西南 | `SouthWest` | 6 | 0.96 秒 | 1619 |

**五分钟的同时触发情况。** 在游戏世界内同帧重新激活八个系统，每 0.05 秒读取各段实际存活粒子，共 6,000 次，有效读数覆盖全部八段。每段记录到 255–261 次闪光和 63–64 簇烟花，闪光沿座位的横向跨度为 13.09–13.18 米。同一个采样间隔内最多三段开始闪光、两段开始放烟花。86% 的时刻只有零至两段闪光可见，偶尔会有更多段重叠，此次没有记录到八段同亮。

**性能对照条件。** UE 5.8.3，正式图、独立游戏进程、D3D12、1920 × 1080、100% 渲染比例、关闭垂直同步和帧率上限。测试机为 Core Ultra 7 270K Plus / RTX 5070 Ti。每个机位固定构图和车球位置，在同一份地图内交替让 0、1、8 个看台 Niagara 组件运行，三种配置均使用全场星际观众。两次独立启动反转机位及开关顺序。每个机位、每种配置采四组，各组采 7.5 秒并略去首尾各 0.2 秒。有效采样共 48 组、64,120 帧，约 341 秒。性能采样时关闭实拍和粒子回读。下列数据衡量进入比赛后的持续开销，七处新增实例在激活时另有共 97 个预热步，未计入表中。

| 固定机位 | 整帧 GPU，全关 | 整帧 GPU，南侧 | 整帧 GPU，八段 | 八段相对全关 GPU 差 | 新增七段 GPU 差 | 新增七段游戏线程差 |
|---|---:|---:|---:|---:|---:|---:|
| 场中央驾驶 | 5.664 ms | 5.616 ms | 5.600 ms | −0.045 ms | −0.040 ms | +0.025 ms |
| 东侧贴边驾驶 | 4.877 ms | 4.853 ms | 4.829 ms | −0.006 ms | −0.003 ms | +0.018 ms |
| 八段同时入镜 | 3.900 ms | 3.943 ms | 3.954 ms | +0.047 ms | +0.013 ms | +0.008 ms |
| 北侧前排近景 | 4.638 ms | 4.627 ms | 4.700 ms | +0.026 ms | +0.035 ms | +0.015 ms |

各配置显示四组整帧 GPU 中位数的中位数。后三列按同一机位、同一轮的两种配置先求差，再取四个差的中位数。贴边驾驶新增七段的 GPU 差在 −0.079 至 +0.013 ms 间波动。全景新增七段平均每帧多约 9.4 次绘制提交，全景的整帧 P95 为 5.31 ms，较南侧的配对差为 +0.040 ms。本轮可以看出少量 CPU 与绘制开销。学校设备上四人同时释放技能时的帧耗需要实机另测。

Windows 单图 Cook 零错误、零警告，产物包含正式地图、星际观众图集、共享 Niagara 系统及两份粒子材质。Exhibition 本机一主三客完成首局返房和第二局开赛，四端均为零测试警告、零失败。四端回归使用 NullRHI，渲染性能取自上表的独立 D3D12 游戏进程。本机粒子与性能记录在 `Saved/StadiumCheers/{timing_match.json,performance.json}`，四端报告在同目录的 `Network/Summary.json`，原始帧表在 `Saved/Profiling/CSV/StadiumFX_*.csv`。

<a id="arena-night-rim"></a>

## 2026-11-23 外环钢板、贴边护网与南侧看台

在正式图的 World Outliner 搜索 `SouthFrontRow_Cheers` 可以定位看台试点。用比赛镜头开到南侧偏东的护栏旁，约在世界坐标 **X 1830、Y −7410 cm**，停车数秒可看见不同座位的暖白短闪，以及朝看台内侧展开的低矮金色火花。[同机位前后对照](Images/MVPB_Match_RimNight_20261123.jpg) · [驾驶视角实拍动图](Images/MVPB_Match_FrontRow_20261123.webp)

| 要调的效果 | 编辑入口 | 首版设置 |
|---|---|---|
| 外环钢板与警示漆 | 关卡 Actor `base_circle_inflatable_006_1373` 的 `MI_ArenaNight_ApronSteel` | `Gunmetal` 调钢板底色，`Brush Strength` 为 0.56，`Panel Roughness` 为 0.445。两排钢板绕场拼接，每隔三块留一组哑光短标。`Amber Paint` 和 `Paint Coverage` 调漆色和覆盖强度。 |
| 青色网丝与挡墙线 | `Path → Materials → MI_ArenaNight_SidelineNet` | `Line Width` 为 0.018，`Glow Strength` 为 1.70。`Visible Top Fraction` 为 0.30，控制宽顶面上从赛道向看台的显示距离。`Visible Height Cm` 为 143，控制侧网向上淡出的高度。贴地亮线位于挡墙内壁、离行车面约 12 cm。 |
| 看台闪光与火花 | `/Game/Assets/FX/ArenaNight/NS_ArenaNight_FrontRowCheers` | `Camera Flashes` 在约 13.2 m 长的座位区随机取点，每次亮 0.20 秒。`Gold Fan` 每轮发出 17 颗火花，单颗持续 0.76 秒。各发射器的 `Emitter State → Loop Duration` 分别控制间隔，火花的 `Spawn Burst → Spawn Count` 控制数量。 |

场边材质实例位于 `/Game/Assets/Materials/ArenaNight/`。护网材质直接画在原挡墙网格上。调宽度从 `Visible Top Fraction` 入手，调网丝粗细用 `Line Width`。南侧试点由 `Presentation/Grandstand Ambience` 下的 `SouthFrontRow_Cheers` 本地播放。

**本机验证（UE 5.8.3）**

- 在 997 × 640 的 D3D12 驾驶画面中，于停靠车辆上方取 58 条水平扫描线，测得青色护网可见宽度的中位数由 197 px 降至 61.5 px，约为原来的 31%。同一段护栏内，近白青光像素占比由 27.8% 降至 0.28%。
- 停车录制 6.2 秒，Niagara 回读到闪光分布在相距最远 7.25 m 的座位上。沿护栏实际行驶 20.19 m，驾驶镜头下也拍到了闪光与扇形火花。动图前段为停车观察，后段为沿墙行驶。
- 重新加载正式图后，核对原有 151 个 Actor 的身份、变换、组件碰撞、场灯和比赛模式。沿 72 个方向测量行车面，并在离地 45、120 cm 两个高度测量侧墙，共 216 条射线的命中位置与修改前一致。
- Windows 地图 Cook 收齐七份新增资产，零错误、零警告。本机 Exhibition 一主三客完成首局返房和第二局开赛，四端通过。不同显示器上与多人技能同屏时的观感待团队实测。

<a id="arena-full-crowd"></a>

## 2026-11-23 全场八段星际观众

八段看台共用南侧试点的 `M_ArenaNight_SouthCrowd` 和 `T_ArenaNight_SouthCrowd`。第一排举围巾，第二排挥旗，后排按小群穿插空位。每段分别调节空位、整群位置和两种道具的位置。南侧保留已确认的排布。八个方向均已在正式图中实拍。[全景、贴边驾驶与八段前排对照](Images/MVPB_Match_FullCrowd_20261123.jpg)

**在 Editor 调整单段。** 在 `MVPB_Match` 的 World Outliner 搜索下表的 Actor，选中 `StaticMeshComponent`，在 Details 搜索 `Custom Primitive Data`，调整 `Custom Primitive Data Defaults`。数组依次是四个浮点数。南侧空数组使用四个零默认值。

| 索引 | 材质参数 | 调整效果 |
|---:|---|---|
| 0 | `Crowd Group Phase` | 整段人群沿座排错开，1.0 等于一整组图案。 |
| 1 | `Crowd Extra Gap` | 正值拉开组间空位，负值收紧。0.10 表示每一组多留原图案宽度的 10%。有效范围 −0.025 至 0.12，人物宽度固定。 |
| 2 | `Front Scarf Phase` | 单独移动举围巾的第一排，单位同索引 0。 |
| 3 | `Front Flag Phase` | 单独移动挥旗的第二排，单位同索引 0。 |

| 方位 | 看台 Actor | 观众材质槽 | 整群 / 空位 / 围巾 / 旗帜 |
|---|---|---:|---|
| 南 | `stand_001_0118` | 5 | 0 / 0 / 0 / 0 |
| 东南 | `stand_002_0126` | 4 | 0.31 / 0.065 / −0.12 / 0.22 |
| 东 | `stand_001_0120` | 5 | 0.68 / −0.024 / 0.14 / −0.27 |
| 东北 | `stand_002_0128` | 4 | 0.12 / 0.10 / 0.28 / −0.09 |
| 北 | `stand_001_0122` | 5 | 0.54 / 0.028 / −0.23 / 0.31 |
| 西北 | `stand_002_0130` | 4 | 0.86 / 0.078 / 0.11 / −0.29 |
| 西 | `stand_001_0116` | 5 | 0.23 / −0.015 / 0.34 / −0.13 |
| 西南 | `stand_002_0124` | 4 | 0.76 / 0.044 / −0.18 / 0.16 |

**全场性能实测。** UE 5.8.3、D3D12、RTX 5070 Ti，以独立游戏模式载入正式比赛图，用测试车和球固定构图。输出 1920 × 1080，渲染比例 100%，关闭动态分辨率、垂直同步和帧率上限。两次独立启动换序测试三种布局，每个机位每种布局采六组，各组保留中间 120 帧，共 8,640 帧。“南侧试点”使用改造前保存的试点材质，其余七段使用原观众。全景镜头能同时看到八段的外排。

| 固定机位 | 原观众整帧 GPU | 仅南侧试点 | 全场八段 | 同组配对中位差，八段相对原观众 |
|---|---:|---:|---:|---:|
| 场中央驾驶 | 5.641 ms | 5.714 ms | 5.672 ms | −0.009 ms |
| 东侧贴边驾驶 | 4.911 ms | 4.911 ms | 4.923 ms | +0.006 ms |
| 八段同时入镜 | 4.037 ms | 4.055 ms | 4.045 ms | +0.010 ms |
| 北侧看台近景 | 4.770 ms | 4.785 ms | 4.761 ms | −0.022 ms |

前三列取各采样组的 GPU 帧时间中位数，再对六组取中位数。末列先比较同一轮相邻采样里的原观众和八段版，再对六个差值取中位数。相对已采用的南侧试点，补齐七段后，贴边驾驶的配对差为 **+0.009 ms**，全景为 **−0.014 ms**。这些差值小于组间波动，本机没有测出可辨认的帧率损失。学校目标机上的四人混战尚待实测。

全场观众复用原有的 8 个材质段，LOD0 合计 4,240 个顶点、3,824 个三角形。每像素取样一张图集。D3D12 `ListTextures` 记录满分辨率图集为 1024 × 512、DXT5、11 级 mip，整场共用一份 **704 KiB**。观众由静态面片绘制，个体不用更新动画或同步网络状态。重新打开地图后，比对 152 个 Actor 及其组件，关卡差异为七个观众材质槽和对应的布局值。南侧前排同机位对照的轮廓位移为 0 像素。

单图 Windows Cook 收入正式地图、共享材质和图集，零错误、零警告。本机原始帧表在 `Saved/Profiling/CSV/Stands_Full{A,B}_*.csv`。逐组结果在 `Saved/StadiumCrowd/performance.json`，各方位截图在同目录的 `Visual/`，重载核对在 `reload.json`，Cook 记录在 `Cook.log`。

<a id="south-crowd"></a>

## 2026-11-23 南侧星际观众试点

从比赛镜头沿南侧贴边行驶，约在 `X 1830、Y −7410 cm`，可看到朝场内欢呼的星际观众。最前排有举围巾的观众，第二排有挥旗姿势。后排由不同头形、耳鳍和头盔的小群组成，群与群之间留出空位。旗与围巾采用静态姿势，座位间的闪光和火花继续由 `SouthFrontRow_Cheers` 播放。[近景与驾驶机位对照](Images/MVPB_Match_SouthCrowd_20261123.jpg)

在正式图 `MVPB_Match` 的 World Outliner 搜索 `stand_001_0118`，打开 `StaticMeshComponent → Materials → Element 5`。该槽引用 `/Game/Assets/Materials/ArenaNight/M_ArenaNight_SouthCrowd`。两条普通人群、围巾组、旗帜组预先排成一张 RGBA 图集。材质用原面片顶点选择座排并错开图案，每个像素采样一次图集。`Group Tile Angle` 增大时人物和组间空位一起变宽；单独修改空位、身高及姿势可编辑源图后重新导入 `T_ArenaNight_SouthCrowd`。`Clothing Brightness` 控制衣服亮度，`Shadow Fill` 控制暗部可见度。[可重新导入的观众图集](../../SourceArt/ArenaNight/T_ArenaNight_SouthCrowd.png)

南侧原有观众材质槽的 LOD0 为 540 个顶点、488 个三角形。相邻发光看台灯带为 32 个三角形，各自占一个材质段。试点继续使用原几何，当时另外七段引用旧观众材质，供当轮对照。

**本机对照。** UE 5.8.3、D3D12、RTX 5070 Ti，PIE 有效视口 997 × 640。两次独立启动分别用原材质和试点材质交替渲染。每个机位每次采 8 组，每组 96 帧，去掉首尾各 8 帧，共计 2,560 帧。下表是各组中位数再取中位数，计量对象为整帧 GPU 时间。

| 固定机位 | 首次，原观众 → 星际观众 | 独立重载，原观众 → 星际观众 |
|---|---:|---:|
| 贴边驾驶 | 3.043 → 3.098 ms | 2.977 → 3.134 ms |
| 南侧看台近景 | 2.822 → 2.913 ms | 2.952 → 2.907 ms |

贴边驾驶两轮相差约 +0.05 和 +0.16 ms，看台近景两轮差异方向相反。分辨率、同时入镜的面积和设备都会影响数值；学校电脑上的多人混战帧时间待实测。D3D12 的 `ListTextures` 显示试点图集为 1024 × 512、DXT5、11 级 mip，最高 mip 驻留时占 704 KiB；原观众的 1024 × 128 贴图占 192 KiB。若多个看台复用本图集，各段可共享这一份纹理。[Epic 材质与几何性能指南](https://dev.epicgames.com/documentation/en-us/unreal-engine/performance-guidelines-for-artists-and-designers?application_version=4.27)

重新载入磁盘上的正式图并比对 152 个 Actor 的身份、变换、组件碰撞和网格材质，差异仅有 `stand_001_0118` 的 `Element 5`。Windows 地图 Cook 收入新材质及图集，零错误、零警告。两轮帧样本与截图在本机 `Saved/ArenaCrowdBudget/SouthArt`、`SouthReload`；原始 CSV 在 `Saved/Profiling/CSV/CrowdBudget_*.csv`。分配内存的读数见 `Saved/ArenaCrowdBudget/SouthReload.log`。

<details>
<summary>人物原画的 ImageGen 提示词</summary>

Production texture source for an arcade sci-fi sport videogame. A transparent RGBA sprite sheet of exactly TWELVE distinct cutout extraterrestrial spectators. Strict contact sheet of FOUR evenly sized columns by THREE evenly spaced rows, one completely isolated waist-up seated fan in each cell. All twelve look slightly upward toward the camera / a race, with their lower torsos cut off cleanly at the waist and their shoulders, heads, antennae, and props completely within their own cell. Generous truly transparent padding between every cell, nothing touches canvas boundaries. Transparent everywhere behind the figures and in negative spaces. No text, frame, base, stadium, seats, background, floor, vignette, gradients of atmosphere, cast shadow, glow or sticker outline. Original retro-future sports mascots rendered with bold tactile rounded shapes and two to three softly separated planes of matte color, hand-designed silhouette, elegant graphic poster finish, quiet warm stadium light grazing the top edge. Very legible at 20 pixels high. Medium-dark plum, rusty coral, terracotta, muted petroleum teal, small amber and warm ivory highlights, deep wine shadows. Vary heads, body shapes, gaze and height naturally. First row left to right: slender coral alien with two club-tipped antennae, round teal astronaut with broad amber-ivory visor, purple round chunky fan with a side fin, wide dark teal manta-shaped fan with a creamy winged brow. Second row: stout plump coral fan with small ears leaning toward a friend, slender plum fan with an asymmetric swept-back crest, squat teal pilot with a narrow pale visor leaning forward, young amber long-eared fan with a lifted hand. Third row: ONE plump alien with two hands holding a clearly readable wide striped woven coral-and-amber scarf horizontally over its head, ONE teal alien waving a lightweight warm ivory and coral pennant on a short diagonal rod, one quiet coral helmeted fan tilting its head, one quiet purple creature with sleek pointed ears clapping at chest level. All four in last row must fit entirely in separate cells, including flags/scarf; first two rows no banners or props. Flat sprite atlas, not an image of a crowd or a stadium. No thin decorative micro-detail.

</details>

<a id="arena-flat-drive-surface"></a>

## 2026-11-19 正式图全场平整圆形碰撞

在正式图的 World Outliner 选择 `Floor`，其 Static Mesh 为 `/Game/Assets/Meshes/ArenaDeck/SM_ArenaDeck_FlatDriveSurface`。圆盘半径 82.50 m、厚 1 m，顶面位于世界 Z −200.274 cm。整盘只有一个 256 边的简单凸包，简单查询和复杂查询都使用它。环纹、中央盘和十个灯壳由原有的可见网格表现，`Path` 的侧壁负责挡车和球。临时图继续使用原行车面。

在本机 UE 5.8.2 中从磁盘重新加载正式图，核对了 151 个 Actor 的身份、变换、网格、材质、碰撞和灯光属性，唯一一处差异是 `Floor` 的网格引用。

| 本次验证 | 结果 |
|---|---|
| 全场高度与边缘 | 从场心、旧环纹到外围共 23,040 次顶面检测，命中高度均为 Z −200.274 cm，法线均竖直向上。沿 720 个方向测得圆盘实际半径为 82.494～82.500 m。圆盘至少伸入近地侧壁内缘 39.7 cm，外侧 1,440 次检测均未命中圆盘。三个高度共 2,160 次侧挡检测的位置与修复前一致。 |
| Car01 连续全油门 | 固定 60 Hz，同样六条路线共约 480 m，单步水平掉速 ≥0.10 m/s 的采样帧由 46 降至 0，最大竖直速率由 0.683 降至 0.0021 m/s。保留正式比赛模式、球、机关和 D3D12 渲染，东西、南北两条穿环路线约 205 m，同一指标由 12 降至 0。 |
| 自由球与碰壁 | 半径 50 cm 的球落稳后球心距地 50 cm。球自由滚过原环纹 33.6 m，全程保持 `Free`。在侧挡内缘距场心最远的方位，球沿径向以约 18.0 m/s 滚向 `Path`，碰壁后以约 5.05 m/s 向场内折返。Car01 朝同一段侧壁持续给油，首次命中后继续顶住 1 秒，车辆留在场内。 |
| 联机进场 | 同一台电脑运行 Exhibition 一主三客，四个进程都完成首局、返回练习房及第二次开赛，自动化报告均为 1 项通过、0 警告、0 失败。四台实机的驾驶手感待团队复测。 |
| Windows 地图 Cook | 以正式图为入口，使用项目正常的依赖收集规则，Cook 退出码 0。已核实产物包含正式图、新圆盘和 `TilingCurl32`。首次使用 `-CookSinglePackage` 的精简检查漏收轮胎打滑特效的这项依赖，正常 Cook 已收齐。 |

复跑可从世界坐标 `X=-5550、Y=3020` cm 朝 +X 全油门驶过两侧环纹。地面高度、车辆和球的轨迹及联机报告保存在本机 `Saved/ArenaFlatFloor/`。

<a id="arena-visible-ring-profile"></a>

### 橙色外圈和金属中环的可见高度

从正式图读取三个可见地面网格，沿 72 个方向每隔 1 cm 取顶面高度，以平整的行车面为零点。金属中环的大面（距场心 50～69 m）和橙圈的两段大面（71～75 m、76～81 m）均与零点相差不足 0.01 cm。六边形中场的局部浮雕最高约 +3.75 cm。金属环与橙圈之间的窄槽深约 11～12 cm，橙圈内的窄槽深约 18～19 cm，橙圈最外沿向下收边。

在两环交界处用正式比赛相机做同机位光照对照。关闭光照后，橙色深浅分块仍可见，金属亮边和橙圈上的明暗渐变明显减弱。这些明暗变化沿同心接缝分布，容易让水平环面看起来像盘沿。图中纵向高度按厘米放大，虚线为车辆和球的行驶平面。[实测剖面与比赛相机对照](Images/MVPB_Match_RingProfile_20261119.png)

<a id="arena-drive-surface-contact-review"></a>

## 2026-11-19 正式图行车顿挫排查

本机 UE 5.8.2（CL 56702186），`main/fb3c635`。修复前，MCP 读到隐藏地面 `Floor` 使用的 `SM_ArenaDeck_DriveSurface` 有 24,909 个三角面，碰撞模式为 `Use Complex Collision As Simple`。从正式图导出的网格上能辨认出整圈装饰纹样。凸起顶点位于距场心约 28.4～48.4 m 处，最高比平面高 3.75 cm，最大坡角约 4.33°。可见钢板、中场盘和十个灯壳均关闭碰撞。读取更早的地图 `45eef7b` 后，五处简单射线首先命中的是旧 `Path` 的水平顶面。场地更新后由 `Floor` 直接承托车身，`Path` 承担外围阻挡。

用 `BP_RoundedVehiclePawn` 的实际输入和 `SM_Car_01_Collision` 刚体在独立单机 PIE 中测试。每种地面均按住油门走同样六条路线，累计约 480 m。四条穿过环纹，两条位于中场平面和远外圈平面。下表采用固定 60 Hz，移开球和机关以隔离地面影响。掉速帧表示相邻两个采样之间水平速率下降至少 0.10 m/s。

| 仅在测试进程中使用的地面 | 掉速帧数 | 单步最大水平掉速 | 最大竖直速率 |
|---|---:|---:|---:|
| 正式图原始行车面 | 46 | 0.418 m/s | 0.683 m/s |
| 同一网格，仅将 Z 缩放到 1% | 0 | <0.001 m/s | 0.0092 m/s |
| 顶面同高的单块简单碰撞板 | 0 | <0.001 m/s | <0.001 m/s |

原始地面的 46 个掉速帧全部发生在穿过环纹的四条路线，记录的 1,950 次车身接触均来自 `Floor`。保留正式图的 GameMode、球和机关，使用 D3D12 离屏再跑东西向、南北向路线，两条各出现 6 个掉速采样，车身接触也全部来自 `Floor`。本轮能复现连续的小幅磕绊，纯地面路段未出现整车卡死。此前的过盘验收主要覆盖平整的中心区域。

在世界坐标约 `X=-5550、Y=3020` cm 处让车朝 +X 持续加速，驶到 `X=4400` 附近，途中先后穿过两侧环纹。网格起伏造成掉速的结论来自保留原三角划分和材质、仅改变高度的对照。修复前的采样和位置图在本机 `Saved/FloorContactInspection/`。[正式图圆盘修复与复测](#arena-flat-drive-surface)

<a id="arena-match-lighting"></a>

## 2026-11-23 正式比赛图夜场灯光

打开 `/Game/Maps/MVPB_Match`。驾驶区用冷蓝主光勾出车身、球和拉丝钢板。靠近地灯时，车身下缘会掠过一小片暖琥珀色。看台用香槟金、橘金和珊瑚色组成八个方向的灯区，外圈的光带顺着座席延伸。[同机位调整前后实拍](Images/MVPB_Match_NightRace_20261123.jpg)

| 想调哪里 | 编辑器入口 | 本版设置 |
|---|---|---|
| 中场整体明暗 | Outliner → `DirectionalLight`、`SkyLight`、`Match_Exposure` | 主光 1.35 lux、6900 K，`Specular Scale` 0.60。天光 0.32，sRGB (184, 202, 239)。自动曝光补偿 −0.40 EV。 |
| 地灯照到车身和地面的范围 | `DeckLamp_01_Light`～`DeckLamp_09_Light`、`CenterInlay_Light` | 普通灯 420 lm、半径 3.95 m、sRGB (255, 151, 70)。中场灯 570 lm、半径 4.40 m、sRGB (255, 157, 77)。 |
| 十盏地灯的灯芯与外围短灯带 | `MI_ArenaNight_InsetLamp` → `Lens Emission` / `Lens Tint`。外圈在 `MI_ArenaNight_Perimeter` → `Lamp Intensity` / `Lamp Color` | 灯芯发光强度 1.65，线性 RGB (1, 0.26, 0.042)。外圈短灯强度 1.20，线性 RGB (1, 0.225, 0.035)。 |
| 看台光束 | Outliner 搜索 `StandWash_` | 四盏照座席，各 21,500 lm。四盏斜照外沿，各 32,000 lm。原灯名的 `Ice` 对应香槟金 (255, 191, 125)，`Gold` 对应琥珀 (255, 154, 73)，`Orchid` 对应珊瑚 (255, 132, 115)，均为 sRGB。 |
| 看台灯面、条幅底纹与座席配色 | Content Browser → `/Game/Assets/Materials/ArenaNight/` | `M_ArenaNight_Grandstand` 的 `Palette Mix` 控制原座席图案与暖色的混合，当前 0.86。`MI_ArenaNight_StandLenses` 和 `MI_ArenaNight_StandRibbon` 分别控制灯面和条带。灯面发光 1.60，条带 0.12。 |

夜场地灯的两个材质实例继承原有地板实例。可以在正式图里独立调灯，同时沿用地板的拉丝、拼缝和灯壳细节。

**实拍与回归。** UE 5.8.3、D3D12 SM6 下，从磁盘重载正式地图，用同一辆车和主球拍摄了中场驾驶、近中场、全场、看台和外沿驾驶五个机位。成像亮度按截图的 0～255 取值，驾驶画面中央区域的中位数从 111 降到 74（约 33%）。普通地灯附近从 219 降到 156，中场灯附近从 214 降到 169。看台座席区域的平均 sRGB 从 (75, 114, 161) 变为 (126, 82, 78)。两次捕获中，五个机位、车辆和球的记录坐标均一致。原图和采样位置在 `Saved/ArenaNightRace/{Before,After,measurements.json}`。

逐项复核了正式图的 151 个 Actor、网格和碰撞配置。Windows 地图 Cook 零错误、零警告，产物含正式图和五份夜场材质。Exhibition 本机一主三客完成首局返房并再次开赛，四端均通过且零测试警告。重载记录、Cook 和四端报告在 `Saved/ArenaNightRace/{reload.json,Cook.log,Network}`。四台实机显示器上的暗部和多人技能同屏观感待团队实测。

## 2026-11-19 正式比赛图灯光首版留档

中场以偏冷的环境光保留钢板拉丝和球的明暗面，琥珀色地灯照亮经过的车身。沿看台选择了八个已有灯具，按灯面原色配三盏青蓝、三盏琥珀和两盏淡紫聚光灯。四盏照座席，另外四盏斜照赛道外沿。各束光略微错开，中场留给地灯和机关特效。[调整前后同机位实拍](Images/MVPB_Match_Lighting_20261119.jpg)

在正式图的 World Outliner 搜索下列名称即可调节。

| 想调整的效果 | 选择哪个 Actor | 11 月 19 日设置 |
|---|---|---|
| 整个画面再亮或再暗一点 | `Match_Exposure` → **Exposure Compensation** | +0.34 EV，原为 +1.0 EV。数值越低画面越暗，自动曝光仍随视野适应。 |
| 主光角度与钢板反光 | `DirectionalLight` | 3.65 lux，6150 K，Pitch −42°，Yaw −32°，光源角直径 1.55°，`Specular Scale` 0.72。 |
| 阴影面亮度与颜色 | `SkyLight` | 强度 0.68，浅冷蓝 sRGB (214, 231, 255)。 |
| 场内灯的光晕及车身暖光 | `DeckLamp_01_Light`～`DeckLamp_09_Light`、`CenterInlay_Light` | 普通灯 1950 lm，衰减半径 485 cm。中场灯 2200 lm，半径 525 cm。统一为暖琥珀 sRGB (255, 190, 122)。 |
| 观众席的色相、落点及覆盖范围 | 搜 `StandWash_`，选择对应方位的聚光灯 | 照座席的四盏为 17,500 lm、半径 59 m、外锥角 41°。照赛道外沿的四盏为 33,000 lm、半径 84 m、外锥角 36°。旋转子灯可移动光斑。八盏均关闭投影与体积雾散射。 |

UE 5.8.3 从磁盘重载，核对了八盏聚光灯的挂接、瞄准和颜色，以及十盏地灯和场景曝光。正式图原有 142 个 Actor 的身份均与调整前一致，新版共 151 个 Actor。D3D12 实车同机位出图，中场驾驶画面亮度中位数从 146 降到 107，约降 27%。同一画面里，球面中央采样从 204 变为 199（均为 0～255 的成像读数）。固定机位单独开关灯组，开灯时灯边钢板采样增亮约 81，青蓝灯下的座席增亮约 33，外沿琥珀灯下的球面红蓝通道差增加约 10。原图和采样在本机 `Saved/MatchArenaLighting/`。

本机 Exhibition 一主三客在正式图完成首局返房并进入第二局，四个进程均零测试警告、零失败。Windows 地图 Cook 产出正式图，退出码为 0。四台实机同场释放技能时的可读性和 GPU 帧耗待团队实测。

<a id="arena-deck-formal-match"></a>

## 2026-11-19 正式比赛图采用新版场地

在 `MVPB_Match` 内更新了 19 个已有 Actor，包含可见地板、中场盘、承托车辆的 `Floor`、外围侧挡 `Path`、四个出生点、主球和机关。新增九组场内地灯和一组中央地灯，每组各有一个灯壳和一个子点光源。重载比对时，正式图原有 122 个 Actor 的对象名和 GUID 均与迁入前一致。迁入当时两图的 142 个 Actor 在变换、挂接、网格、用材、碰撞及照明的核对项一致。

`DA_Room_Development` 和 `DA_Room_Exhibition` 均指向正式图。正式图的 World Settings 继续引用 `BP_MVPB_ListenGameMode`，四个出生槽编号为 0～3，主球沿用地图原有定义。正式图的资产依赖包含 11 份场地材质、材质实例和网格，可以独立加载。后续调灯可直接打开正式图，在 Outliner 的 `Presentation/Deck Inset Lights` 和 `Presentation/Center Inlay` 下选中对应灯组。

本轮验证使用 UE 5.8.3-58210709，从磁盘重新加载正式图。

- 1,216 条向下射线命中 `Floor`，64 条侧向射线命中外围 `Path`。盘心至半径 20 m 的钢板采样位于世界 Z −200.274 cm。
- D3D12 单车以约 9.6～15.1 m/s 穿过中心灯，自由球以 12.6～15.6 m/s 滚过并保持 `Free`。两者过盘时的垂直速度在 0.01 cm/s 的记录精度内为零。另向外围发球，命中 `Path` 后折返。
- 弹簧拳将实车 HP 从 100 扣至 75。电墙、电球分别触发 1 s 眩晕，打滑区内外的抓地系数依次为 0.30 和 1.00，黑洞把距中心 280 cm 的车辆吸至 42 cm 内。
- 本机 Exhibition 一主三客从房间进入正式图，四人完成第一局返房及第二局开赛。四个独立进程各通过一项测试，均为零测试警告、零失败，退出码均为 0。本机报告位于 `Saved/ArenaDeckPromotion/`。学校四台机器上的画面与帧耗留待真人复测。
- 针对 `MVPB_Match` 执行 Windows Cook，退出码为 0。逐项检查产物，正式地图及全部 11 份场地依赖均已写出，临时地图未进入此次产物。Cook 日志另记录了既有冲击特效材质 `M_FX_ImpactBurst` 在 DX11／SM5 下使用 `pass` 时触发 FXC 编译重试。该特效在 DX11 打包版中的画面待复测，日志在 `Saved/ArenaDeckPromotion/cook.log`。

<a id="arena-center-inlay"></a>

## 2026-11-18 中场嵌盘与临时图行车面

中场盘沿原钢板开口的 32 个边点收口，直径约 11.85 m。盘面使用环向拉丝、沉头螺钉和切削边，中央复用场内地灯的分段琥珀灯片。车辆和球经过盘沿、灯壳和相邻六边形时，由同一块连续的地面承托。[改造前后同机位实拍](Images/ArenaDeck_CenterInlay_20261118.jpg)

| 要调整的内容 | 在哪里调 |
|---|---|
| 圆盘的钢色、外圈颜色和拉丝 | `/Game/Assets/Materials/ArenaDeck/MI_ArenaDeck_CenterInlay`。在 **Center Inlay** 下调 `Steel Color`、`Trim Color`、`Brush Strength`（默认 0.72）和 `Steel Roughness`（默认 0.36）。 |
| 灯壳尺寸和灯光 | 两图的 Outliner → `Presentation/Center Inlay` → `podium_007_1375` → `CenterInlay_Lamp` → `CenterInlay_Light`。灯壳长宽为普通地灯的 1.45 倍，约 157 × 64 cm。临时图的子灯为 3200 lm，衰减半径 410 cm。正式图的当前数值见[正式比赛图灯光](#arena-match-lighting)。 |
| 行车面与外围侧挡 | 正式图的隐藏 Actor `Floor` 使用 `SM_ArenaDeck_FlatDriveSurface`，整个圆形行车面位于世界 Z −200.274 cm。临时图的 `SM_ArenaDeck_DriveSurface` 仍有浅浮雕。两图的 `Path` 均使用 `SM_ArenaDeck_ShieldBoundary`，侧壁负责挡车和球。网格位于 `/Game/Assets/Meshes/ArenaDeck/`。 |

四个 `HALPlayerStart` 和场上主球分别按旧承托面的出生净高重新定位，均下移 201.93 cm。四个弹簧拳以各自底座底面定位，电墙和电球以机关地面锚点定位。黑洞和扩张打滑区的原点对齐当地板面。机关沿用原来的平面位置、朝向和玩法配置。

### 最终地图验证

- UE 5.8.3-58210709 从磁盘重载临时图。16 个方向共 1,216 条向下射线均命中 `Floor`，盘心、接边和周围 20 m 半径内的采样均为 Z −200.274 cm。两种检测精度、两种高度的 64 条侧向射线均命中外围 `Path`。四个出生点和九个球／机关 Actor 的高度逐个核对。
- 另取 4,750 个位置比对可见网格和行车面。九盏原地灯处的支撑高度与灯下钢面在 0.01 cm 内一致。内场浅浮雕边缘采用缓坡，内场板面抽样最大高度差 2.22 cm。外圈装饰槽由连续的行车面跨过，抽样中最深的槽底低于行车面约 23 cm。
- D3D12 单车 PIE 使用真实油门输入。车辆以 9.65～15.06 m/s 横穿中场，148 次车身支撑接触全部来自 `Floor`。自由球以 12.64～15.56 m/s 滚过同一灯面，球心 Z −150.27 cm，过盘后继续保持 `Free`。车身和球心的高度采样在 0.01 cm 显示精度内均平稳。另向场边滚球，实测命中 `Path` 并反弹，法线方向附近的 X 速度由朝外变为朝内 4.02 m/s。
- 实车驶入电墙触发 1.0 s 眩晕和 150 cm 回推，弹簧拳命中并扣除 25 HP。另一轮实测中，扩张打滑区将抓地系数降至 0.30，出区恢复为 1.00。电球施加 1.0 s 眩晕和 100 cm 回推，黑洞将静止车辆从距中心 280 cm 吸到 43 cm 以内。
- 正式比赛地图的 SHA-256 与开工前一致。以上为本机 UE 编辑器验证，临时图的四人联机和打包版待团队复测。

<a id="arena-deck-inset-lights"></a>

## 2026-11-18 内场钢板与嵌地灯

打开 `/Game/Maps/MVPB_Match_Temp`。中央六边形采用 800 cm 的对边距，每条边长约 462 cm。被选中的接缝从一个顶点铺到另一个顶点，三板交汇处按边的方向拼合。板心的拉丝通过定向高光、粗糙度和细微法线起伏显现。相邻钢板的加工方向有小幅差异，移动车辆或相机时可以看到高光变化。[同机位钢板对比](Images/ArenaDeck_FullEdge_Brushing_20261118.jpg)

### 调钢板

打开 `/Game/Assets/Materials/ArenaDeck/MI_ArenaDeck_Center`，在 **02 Central plates** 勾选相应参数即可覆盖母材质默认值。

| 想调的效果 | 参数 | 当前值与用法 |
|---|---|---|
| 拉丝更明显或更柔和 | `Brush Strength` | 0.62，可调 0～1。先调这个。设为 0 可以单独对比去掉拉丝后的板面。 |
| 拉丝更细或更宽 | `Brush Pitch Cm` | 4.5 cm，可调 2～14 cm。减小会让纹理更细密。远景会逐渐过滤过密的纹理。 |
| 有多少条边铺防滑条 | `Grip Insert Coverage` | 0.31，控制被选中的接缝比例。长度随六边形边长变化。 |
| 防滑条横向宽度 | `Grip Strip Width Cm` | 40 cm。设为 0 可隐藏防滑条。 |

### 调一盏地灯（临时图首版数值）

九盏灯分别嵌在内场九块钢板上，最近两盏间距约 21.2 m。单盏灯壳尺寸为 108 × 44 cm，带金属倒角和分段琥珀灯片。它们按可见板面定位，灯壳关闭碰撞与重叠事件。每盏灯的真实照明由一个原生 Point Light 提供。[全场灯位](Images/ArenaDeck_InsetLights_Layout_20261118.jpg) · [灯壳近景和同一车位的局部光开关对比](Images/ArenaDeck_InsetLights_20261118.jpg)

| 在哪里调 | 可以改什么 |
|---|---|
| World Outliner → `Presentation/Deck Inset Lights` → `DeckLamp_01`～`09` | 选择灯壳 Actor 来移动、旋转整组。展开灯壳可以选中对应的 `DeckLamp_XX_Light`。 |
| 子灯 → **Light** | `Intensity` 为 3000 lm，`Attenuation Radius` 为 390 cm，`Light Color` 为 sRGB 255 / 180 / 100。改变半径调光能照多远，改变流明调经过灯位时的亮度。可多选九个子灯统一调节。 |
| 子灯 → **Transform / Light** | 灯光中心位于灯壳上方 105 cm，`Source Radius` 为 16 cm，`Specular Scale` 为 0.18。地面高光过亮时可先降低 `Specular Scale`。九盏灯使用 Movable，关闭投影和体积雾散射。 |
| `/Game/Assets/Materials/ArenaDeck/MI_ArenaDeck_InsetLamp` | `Lens Tint` 和 `Lens Emission` 控制所有内场灯片的颜色与自发光。当前自发光倍率为 5。改变整套灯的色相时，应同步调整子灯的 `Light Color`。 |

灯壳使用 `/Game/Assets/Meshes/ArenaDeck/SM_ArenaDeck_InsetLamp`，单个模型 54 个三角形。外圈材质内的短灯仍在 `MI_ArenaDeck_Perimeter` 中调节。

### 本轮验证

UE 5.8.3、D3D12 SM6。独立 Unreal 进程从磁盘重载，核对钢板的 23 个输入与 7 个着色输出、地灯材质、九组父子关系、光强、颜色、衰减半径和无碰撞设置，零错误、零警告。对照正式图，原有 122 个 Actor 中仅有前两轮已设置的两个地板材质覆盖，本次临时图新增 9 个灯壳和 9 个点光源。整边抽查覆盖三种板宽、三个方向和 6,561 对跨缝位置，含距端点约边长万分之一处的两侧采样。

单车 PIE 使用实际 `BP_RoundedVehiclePawn` 和油门输入，以约 9.5 m/s 驶过 01 号灯。定格车辆后仅切换该盏点光源，车侧与邻近钢板出现局部暖光，车顶的变化很轻。四车同时释放技能时的可读性和帧耗待团队实测。

<a id="arena-deck-ground-heights"></a>

### 改造前的内场钢板与车辆接触面

2026-11-18 在正式图 `MVPB_Match` 和临时图 `MVPB_Match_Temp` 分别取九个灯位坐标与 01 号灯旁的行车采样点。两图的十处结果逐点一致。下表为 UE 世界 Z，单位为厘米。

| 层次 | 采样高度 | 核对结果 |
|---|---:|---|
| 护盾 Actor `Path` 的简单碰撞顶面 | +1.65 | 使用 `/Game/Meshes/Wall`，`BlockAll`，一块凸包。凸包横跨所测内场，向下的 Visibility 简单射线先命中这里。 |
| 隐藏的 `Floor` 顶面 | −160.50 | 组件关闭可见性并启用 `BlockAll`。同一位置改用复杂射线，首先命中这层。 |
| 可见中央钢板 `base_circle_inflatable_007_1374` 的上表面 | −196.52 或 −200.27 | 由可见三角面求得，单独检测该钢板也得到相同高度。嵌地灯壳按此面定位。 |

临时图单车 PIE 中，让车辆落到 01 号灯附近，再沿内场开过灯位。从车辆物理根 `VehiclePhysicsBody` 收到的 174 条向上支撑接触全部来自 `Path.StaticMeshComponent0`，其中行驶中的 99 条接触点 Z 约为 +1.51～+1.52 cm。实车支撑面因此比可见钢板高约 **1.98～2.02 m**。此前提到的 **36～40 cm** 仅是隐藏 `Floor` 与可见钢板之间的距离。这个高度差描述的是两个表面，视觉轮胎的位置还取决于车型和挂接偏移。本机逐点数据和接触记录位于 `Saved/ArenaDeckLights/support-inspection.json`。

<a id="arena-center-disk-survey"></a>

### 改造前的中场小圆盘与钢板开口

2026-11-18 在 `main/824d288` 的 `MVPB_Match_Temp` 中读回场景并测量。蓝色小圆盘是 `podium_007_1375`，网格为 `SM_0255_podium_007`，直径约 11.36 m，采用 `BlockAll` 和复杂网格作为简单碰撞。外围六边形钢板属于另一个 Actor `base_circle_inflatable_007_1374`。沿世界 X、Y 正向从盘心穿过接缝取样，分别核对可见三角面和隔离单个 Actor 后的射线命中，两轴结果一致到 0.01 cm。

| 位置 | 可见表面世界 Z |
|---|---:|
| 盘心、半径 50% 处 | −241.18 cm |
| 盘沿内侧，半径 98% 处 | −204.16 cm |
| 相邻钢板接边，半径 102% 处 | −204.06 cm |
| 再往外的钢板面，半径 115% 处 | −200.27 cm |

盘沿与钢板的接边接近等高，盘心比周围钢板低约 41 cm。盘心和盘内采样点单独检测钢板均未命中，替换件需要覆盖这个开口。包含全部 Actor 时，以上位置的简单射线均先命中 `Path` 顶面 +1.65 cm，复杂射线均先命中隐藏的 `Floor` −160.50 cm。中场盘整平和整个内场行车高度对齐应分别核对。局部测量数据位于 `Saved/CenterDiskReview/survey.json`。本次为磁盘地图只读测量。

## 2026-11-15 旧版波纹地面的资产与接线

| 项目 | 路径或值 |
|---|---|
| 母材质 | /Game/Assets/Materials/M_ArenaFloor_Metal |
| 材质实例 | /Game/Assets/Materials/MI_ArenaFloor_Metal |
| 当前法线 | /Game/Assets/Textures/T_ArenaFloor_RandomRelief_N |
| 竞技场中央可见网格 | base_circle_inflatable_007_1374 → Static Mesh Component → Materials → Element 0 |

关卡中名为 `Floor` 的组件关闭了可见性，`Path` 使用半透明竞技场护盾材质。早期记录的 `Floor` 命中可以在内场用复杂射线复现。简单射线和实车接触的分层结果见上文。预览时仅在内存里替换中央可见网格的 Element 0，并创建测试面光。[内场高度复核](#arena-deck-ground-heights)

材质使用 Opaque / Default Lit，Metallic 为 1，Roughness 为 0.54，Metal Color 为线性 RGB 0.16 / 0.21 / 0.29。Arena Pattern Amount 默认 0.50，从原场地颜色图集的 UV0 采样并与底色混合。设为 0 可得到纯色金属。

法线的 X/Y 表示地面沿世界 +X/+Y 的坡度，主纹理按世界 XY 坐标采样。Wave Strength 默认 0.58，高光起伏过强时可从 0.45 试起。

## 随机凹凸法线

母材质的三处 Wave Normal 采样引用 T_ArenaFloor_RandomRelief_N，实例从母材质继承。贴图由固定种子分布的多尺度正负起伏生成，位置、尺寸、长宽比和朝向独立变化。贴图尺寸为 1024 × 1024，UE Normalmap 压缩、sRGB 关闭、XY 循环。原 T_ArenaFloor_Wave_N 可用于对比。

使用同一保存场地、俯视镜头、灯光和曝光，对比了旧方向波纹与新随机凹凸。采样参数为主纹理 850 cm、放大 2300 cm，细节采样 200 cm。新版呈现不规则斑状高光，放大后仍有细小起伏，并随原覆盖渐变淡出。源图法线方向偏向指标从 0.768 降至 0.031，数值越接近 0 表示坡向越均匀。UE 5.8.3 保存命令退出 0，SM6 无材质编译错误，实例重载验证三处采样及 1024 × 1024 贴图。对比图位于 Saved/Inspections/ArenaMetalFloor/RandomRelief，正式比赛镜头待确认。

## 波纹区域过渡和近看细节

当前保存图的两个 Noise 原先使用 6 层、−1～1，正阈值把覆盖范围集中在较高的噪声值附近。现在覆盖和疏密各使用 1 层 Simplex、0～1。覆盖路径保留原来的世界坐标映射，遮罩由 SmoothStep 连续控制。疏密仍由原两路 Normal RGB 的 Lerp 控制。

| 实例参数 | 默认值 | 作用 |
|---|---|---|
| Wave Coverage Start | 0.30 | 低于此值时，波纹强度为零。提高可增加完全平滑的区域。 |
| Wave Coverage Feather | 0.42 | 遮罩的渐变宽度。默认完整波纹强度的阈值是 0.72。 |
| Wave Tile Size Cm | 850 | 控制原有两路主波纹的平铺范围，保留疏密变化。 |
| Wave Detail Tile Size Cm | 200 | 控制小波纹的世界平铺尺寸，独立于主波纹。 |
| Wave Detail Strength | 0.28 | 控制小波纹相对主波纹的强度。 |

细节采样复用同一 Wave Normal 贴图，按世界 XY / Detail Tile Size 采样，并附加固定的 UV 偏移。Custom 先叠加主波纹与细节的 XY 斜率，再一起乘以原 Wave Strength 和覆盖遮罩，最后归一化到世界法线。这样细节也随边界淡出。

在 UE 5.8.3 以同一俯视角、曝光和灯光捕获修改前后画面，并对比主平铺尺寸 2300 cm 时启用及关闭细节层的结果。同一地面区域有 1031 / 9775 个采样位置的亮度变化至少 6 / 255，横向每 3 像素的平均亮度差从 1.04 升至 1.89。保存后重新加载验证参数及节点连接，PCD3D_SM6 无材质编译错误。保存前的中间接线曾触发一次缺输入警告，最终重载没有该警告。修改前预览写图后异常退出，修改后预览写出五张截图，再关闭专用会话。地图、实例与法线贴图开工及结束的 SHA-256 分别一致。预览留档于 Saved/Inspections/ArenaMetalFloor，尚待正式比赛相机下验收。

## −75° 俯视角高光

矩形面光放在需要出现高光的位置上方，水平面上位于镜头相反的一侧，朝向目标地面。局部波纹改变法线，在法线接近光源方向和观察方向的夹角平分线时，反射会进入镜头。

| UE 属性 | 实测起点 |
|---|---|
| Rect Light Transform | Location 2000, −670, 1653.5 cm；Rotation Pitch −71.5° / Yaw 180° / Roll 0° |
| Source Width / Height | 1280 / 300 cm |
| Intensity / Attenuation Radius | 6000 lm / 4500 cm |
| Rect Light Mobility / Lighting Channels | Movable；仅 Channel 1 |
| 中央可见网格 Mobility / Lighting Channels | Movable；Channel 0 与 Channel 1 |
| Indirect Lighting Intensity / Diffuse Scale / Specular Scale | 0 / 0 / 1 |
| Cast Shadows / Volumetric Scattering Intensity | Off / 0 |

测试位置对应地面坐标 1380, −670, −196.5 cm。地面 Channel 0 继续接收现有光源。通道控制动态直射；场景的 Lumen 间接光与反射仍需在比赛镜头中检查。地面上局部过亮时，可优先降低 Rect Light 强度，或者沿地面平移灯具变更高光落点。整张画面随镜头变亮时，固定曝光对比可帮助判断变化来自曝光还是灯光。

## 2026-11-15 验证

UE 5.8.3-58210709，DX12。通过引擎接口创建三份资产，关闭后重载验证贴图压缩、sRGB、实例 Parent、四项标量与四条输出。渲染稳定后报告贴图 512 × 512。临时场景捕获涵盖波纹法线强度 0 与 0.58、Rect Light 开关、约 −75° 与 −84°。固定曝光、关闭 Bloom 的截图抽样中，加灯后中央地面区域亮度平均增加 100.43 / 255，峰值 216 / 255；机关抽样区域平均变化 −3.42 / 255，前后均无亮度 ≥248 / 255 的像素。对比固定相机下的平法线，观察到波纹形成的高光变化。

素材生成命令正常退出。最终截图序列全部写出后，预览 Editor 在 Python 回调退出阶段异常退出。正式关卡无保存，现有地图 SHA-256 与开工一致。本轮证据位于 Saved/Inspections/ArenaMetalFloor；实机比赛摄像机、自动曝光、整场周围物体与打包仍待复核。

<a id="arena-deck-hex"></a>

## 2026-11-18 临时图中央六边形钢板首版留档

打开 `/Game/Maps/MVPB_Match_Temp`。中央圆盘 `base_circle_inflatable_007_1374` 的 Element 0 引用 `/Game/Assets/Materials/ArenaDeck/MI_ArenaDeck_Center`。中央使用正六边形大板，板心留白。部分接缝的中段嵌有一截带斜纹的防滑压条，两块相邻钢板共用这截压条，中间能看到板缝，两端在三板交点前收住。外圈由 `MI_ArenaDeck_Perimeter` 绘制同心压条和琥珀短灯。[上版与本版的同机位实拍](Images/ArenaDeck_Hex_Comparison_20261118.jpg)

中央板面采用均匀的钢蓝底色和细微的无方向粗糙度变化。场馆灯光能在板面上形成整片冷色反光，切削倒角上的高光更窄，防滑纹表面更哑。六边形、压条和倒角均在材质着色阶段生成。

| 在中央实例中调节 | 本版值 | 实际作用 |
|---|---|---|
| `Plate Length Cm`、`Plate Angle Deg` | 800 cm、15° | 长度表示正六边形两条相对平边之间的距离。角度旋转整片拼板。 |
| `Grip Insert Coverage` | 0.31 | 控制带防滑条的接缝比例。设为 0 可预览只有拼缝的钢板。 |
| `Grip Strip Width Cm` | 40 cm | 控制整条防滑条横跨板缝的总宽。设为 0 隐藏压条。 |
| `Face Metallic`、`Panel Roughness` | 0.975、0.375 | 前者调中央板面的金属响应，后者调高光宽度。 |
| `Trim Roughness`、`Bevel Depth Cm` | 0.22、3.4 cm | 前者调切削边的反光宽度，后者调倒角的法线起伏。普通暗缝宽 `Seam Width Cm` 为 9 cm。 |
| `Finish Grain Strength` | 0.42 | 调节细微的无方向粗糙度，设为 0 可单看整板反射与倒角。 |
| `Steel Color`、`Trim Color` | (0.126, 0.181, 0.234)、(0.370, 0.445, 0.520) | 板面与切削边的线性 RGB。 |

UE 5.8.3-58210709，D3D12 SM6。独立进程从磁盘重载，核对母材质的 21 个输入、五条着色输出、中央参数和关卡中的两个材质实例引用，退出 0，零错误、零警告。几何抽查在三种板宽、三个边方向上对 7,605 对跨缝采样逐一比对，验证了两侧压条的位置一致及三板交点留空。独立单车游戏进入 Playing，实拍画面包含车辆、球和新地板。原图位于本机 `Saved/ArenaDeckHex/QA/Driving-Hex.png`。四车交战时的可读性待团队实测。

<a id="arena-deck-temp"></a>

## 2026-11-18 临时图金属拼板首版留档

在 Content Browser 打开 `/Game/Maps/MVPB_Match_Temp`。中央采用钢蓝色错缝拼板、切角、浅倒角、埋头紧固件及局部六角防滑嵌板。外圈采用三道同心弧形压条，两道布置成组三枚的琥珀短灯，中间一道布置通风槽。灯被嵌在暗色灯座里，用于标示场地外沿。现有场馆光照提供钢面的反光，短灯的亮度由材质的 Emissive 输出控制。[三个相同机位的施工前后对照](Images/ArenaDeck_Temp_Before_After_20261118.jpg)

三个专用资产位于 `/Game/Assets/Materials/ArenaDeck/`。母材质 `M_ArenaDeck_Panels` 使用 Opaque / Default Lit，输出 Base Color、Metallic、Roughness、Normal 和 Emissive Color。纹样使用相对网格 Actor 原点的世界厘米坐标，拼缝、倒角、穿孔均为材质表现。材质图中包含完整的纹样计算，运行时直接使用这三个材质资产。

| 临时图中的位置 | 放置的 Actor | Static Mesh Component → Materials → Element 0 |
|---|---|---|
| 中央圆盘 | `base_circle_inflatable_007_1374` | `MI_ArenaDeck_Center` |
| 过渡外圈 | `base_circle_inflatable_005_1372` | `MI_ArenaDeck_Perimeter` |

### 调整比赛镜头下的效果

直接打开对应实例，勾选要修改的参数，即可在关卡里预览。两个实例的 `Perimeter Mode` 分别为 0（中央）和 1（外圈）。钢色与灯色的下表数值使用线性 RGB。

| 想调整的效果 | 参数与本版设置 | 调整方式 |
|---|---|---|
| 中央板块的大小和方向 | `Plate Length Cm` 800，`Plate Angle Deg` 15° | 增大长度会减少视野中的拼缝。宽度按板长的 66% 生成。 |
| 六角防滑嵌板的疏密 | `Grip Insert Coverage` 0.31 | 调低可让更多中央板块保持整面钢板。该值控制带嵌板的板块比例。 |
| 钢板的颜色和反光 | `Steel Color` 中央 (0.056, 0.113, 0.184)，外圈 (0.059, 0.092, 0.129)。`Panel Roughness` 中央 0.445，外圈 0.43 | 提高粗糙度会让高光更宽、更柔。边条另由 `Trim Color` 和 `Trim Roughness` 控制。 |
| 板缝和倒角的分量 | `Seam Width Cm` 中央 9、外圈 12。`Bevel Depth Cm` 中央 2.8、外圈 3.2 | 宽度控制暗缝，深度控制边缘法线与高光。 |
| 外圈的径向宽度和分段 | `Ring Start Radius Cm` 4881，`Band Width Cm` 683，`Segments Per Ring` 28 | 起始半径与中央圆盘的实测半径对齐。调整分段数可改变灯组沿圆周的间距。 |
| 琥珀灯的可见度 | `Lamp Color` (1, 0.31, 0.048)，`Lamp Intensity` 2.6，`Lamp Length Cm` 83 | 调高强度让灯更亮，增加长度让每枚灯在远景更易辨认。 |

### 验证

UE 5.8.3-58210709、D3D12 SM6。独立 Unreal 进程从磁盘重载最终母材质和两份实例，核对五条输出、两种布局和最终参数，退出 0，零错误、零警告。副本与正式图各有 122 个可枚举场景 Actor。逐一核对 Actor 类型和变换、Primitive 组件的可见性、移动性与碰撞配置、静态网格与用材时，差异为上表两个组件的材质覆盖。施工前后对正式地图、旧地板母材质和实例及三块导入网格分别校验了 SHA-256。

另用临时图和现有 `BP_VehicleTestGameMode` 启动了独立单车游戏，`BP_RoundedVehiclePawn` 正常进入 Playing，车辆相机截到车身、球、机关和金属拼板，分辨率 1440 × 900。本机实机原图为 `Saved/ArenaDeckTemp/QA/Driving-Final.png`。四辆车近距离交战时技能与灯带的辨识度待团队实测。
