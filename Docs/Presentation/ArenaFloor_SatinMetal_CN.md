# MVPB_Match 波纹缎面金属地面

正式比赛图 `/Game/Maps/MVPB_Match` 已采用六边形拉丝钢板、整边防滑条、外圈灯带和九盏嵌地灯。中场的第十盏灯嵌在与行车面齐平的金属圆盘中央。临时图 `MVPB_Match_Temp` 可用于继续试做。两图引用同一组材质和网格，修改共享资产会同时影响两图。[中场盘与碰撞](#arena-center-inlay) · [六边形钢板与地灯调参](#arena-deck-inset-lights)

<a id="arena-deck-formal-match"></a>

## 2026-11-19 正式比赛图采用新版场地

在 `MVPB_Match` 内更新了 19 个已有 Actor，包含可见地板、中场盘、承托车辆的 `Floor`、外围侧挡 `Path`、四个出生点、主球和机关。新增九组场内地灯和一组中央地灯，每组各有一个灯壳和一个子点光源。重载比对时，正式图原有 122 个 Actor 的对象名和 GUID 均与迁入前一致。迁入后两图的 142 个 Actor 在变换、挂接、网格、用材、碰撞及照明的核对项一致。

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
| 灯壳尺寸和灯光 | 两图的 Outliner → `Presentation/Center Inlay` → `podium_007_1375` → `CenterInlay_Lamp` → `CenterInlay_Light`。灯壳长宽为普通地灯的 1.45 倍，约 157 × 64 cm。子灯默认 3200 lm，衰减半径 410 cm。 |
| 行车面与外围侧挡 | 两图的隐藏 Actor `Floor` 使用 `SM_ArenaDeck_DriveSurface`，中场世界 Z 为 −200.274 cm。内场行车面跟随板面浅浮雕并平滑过渡，外圈跨过窄装饰槽。`Path` 使用保留护盾外观的 `SM_ArenaDeck_ShieldBoundary`，侧壁负责挡车和球。三份中场专用网格位于 `/Game/Assets/Meshes/ArenaDeck/`。 |

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

### 调一盏地灯

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
