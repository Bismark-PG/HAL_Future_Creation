# MVP-B 车辆特效团队交接

适用于特效制作、双端验收和接续开发。核对基点为 `main/e43e1fa`，引擎为 UE **5.8.3-58210709**。本轮交接核对保存资产引用与源码，实际画面由团队按下文验收。优先完成 FX-5 三车型共享电击，再验收 FX-4 的通用材质与粒子模板。两项结果都可用于后续 FX-6 组合测试。 [项目当前进度](../Project_Status_CN.md)

## 量产特效视觉纲领（2026-11-11 补充）

用户已试玩 SlipperyTrail 新涂鸦材质，反馈其风格比旧油膜明显接近预期；颜色将由用户继续调试。此反馈确认视觉方向，具体材质尚需继续调参，未扩展为所有场景或双端验收通过。下述纲领适用于后续技能、地面区域、车辆状态及机关特效，后文车型／引用清单仍保留原交接时点。

**保持《Need for Speed Unbound》启发的夸张卡通与手绘涂鸦风格，整体呈现应稍柔和、圆润。** 保留清楚的轮廓、概括色块与动作节奏，在颜色、纹理和边缘上减少当前首版过于尖锐、刺眼的感觉。

- **颜色：** 保留卡通分色与必要对比，适度缓和相邻色块和笔触的明暗／色彩反差；具体配色由用户调试，不将首版绿紫配色固化为通用要求。
- **纹理与形状：** 优先使用圆润、饱满、较宽的手绘笔触，减少密集细碎纹理、针状尖端和锯齿感。允许少量尖锐形状表达速度／冲击，但应服务效果语义。
- **描边与过渡：** 保留可读轮廓，粗细应可调；颜色带之间允许短而柔和的过渡。软化程度以比赛镜头下仍能清楚辨认效果为准。
- **制作与验收：** 柔和化与卡通风格同时成立，保留车辆、球、危险区域边界的可读性。以比赛镜头检查大形、颜色与笔触比例，再检查近景细节。

SlipperyTrail 当前描边调节方法见[涂鸦材质：描边粗细与柔和度](Slippery_Trail_Graffiti_Material_CN_v0.1.md#描边粗细与柔和度)。本次只补充视觉方向与配置说明，没有按此修改现有资源。

2026-11-11 后续施工已在现有涂鸦母材质接入平滑体积明暗与独立的真实投影开关，原 MI 配色和技能 DA 保留。地面表现网格的 **UV1 保留给平滑权重梯度**，复用地面材质时勿将其改作笔触平铺；基础纹理仍使用 UV0。参数及复用边界见[体积明暗与投影](Slippery_Trail_Graffiti_Material_CN_v0.1.md#体积明暗与投影2026-11-11)和[公共地面合同](Ground_Surface_Deformation_CN_v0.1.md#平滑体积明暗的附加合同2026-11-11)。上一段“本次只补说明”保留纲领确认时点。

SlipperyTrail 结束消散也已接入：玩法到期先移除区域来源并结束 GAS，随后仅保留视觉图块退场；DA 管 `Dissolve Seconds`，MI 的 **05 Dissolve** 管图块／边缘外观。不得用剩余画面反向注册玩法作用。见[消散配置与验证](Slippery_Trail_Dissolve_CN_v0.1.md)；不含额外蒸汽粒子，其他区域的生命周期没有自动改动。

SlipperyTrail 圆润油漆溢边已接入：DA 的 `Presentation → Paint Edge` 控制薄的纯表现余量，MI 的 **06 Paint edge** 控制噪声大形、细节与按厘米调节的随形描边。**UV2 保留给边缘距离／余量（cm），UV1 继续保留给体积梯度。** 不得以视觉溢边扩大打滑查询或添加碰撞；原主体覆盖保持。见[配置、通道合同与本轮验证](Slippery_Trail_Paint_Edge_CN_v0.1.md)。

同日新增地面主体朝向三色，参考 TireSkid 的三档卡通明暗思路。MI **07 Three tone shading** 调独立三色、虚拟来光和门槛，满强度时替代旧高度分色并关闭 04 的额外明暗倍率，ToonStrength=0 恢复旧配方；轮胎烟尘资源本身没有修改。当前保留 C++ 地面网格，Niagara／液团暂缓；详见[三档明暗配置](Slippery_Trail_Three_Tone_CN_v0.1.md)。

2026-11-12 追加暗绿色边缘与中间色缓冲，仍在 MI 的 07 分组调节。新模式边缘色独立于原 InkColor，避免影响网点；Paint Custom 的 RGB 合同保持，A 增加轮廓内深度。详见[边缘配置、合同与验证](Slippery_Trail_Edge_Buffer_CN_v0.1.md)。

## 1. 从哪辆车、哪个组件找起

实际选车由 `/Game/Data/Gameflow/DA_VehicleCatalog_Default` 的 Vehicle ID 找到 Pawn BP。Pawn 的 **Vehicle → Configuration → Definition** 指向车辆定义。定义的 **Visual → Presentation** 指向车型表现 DA，里面放 Skeletal Mesh、Anim Class、车身姿态与回弹、悬挂、漂移／急刹拖尾。状态特效另走 **Project Settings → Game → Vehicle Status FX → Default Catalog**，普通车型的 **Visual → Status FX → Status FX Catalog Override** 留空时继承项目默认。

### 当前保存的车型清单

| 车型 | Pawn BP | Vehicle Definition | 表现 DA 与 AnimBP | 已保存的效果开关和引用 |
|---|---|---|---|---|
| Car01（`Car_01`） | `BP_RoundedVehiclePawn` | `DA_Vehicle_Player_TestBaseline_v1` | `DA_Car_01_Presentation` → `SKM_Car_01` / `ABP_Car_01` | 悬挂、发射回弹、后轮拖尾开，受击回弹关。旧 BP 有电击 Overlay 和丝带。 |
| Car02（`Car_02`） | `BP_VehiclePawn_Car_02` | `DA_Vehicle_Car_02` | `DA_Car_02_Presentation` → `SKM_Car_02` / `ABP_Car_02` | 悬挂、发射回弹、受击回弹、后轮拖尾开。旧 BP 的电击 Overlay 和丝带为空。 |
| Car03（测试用 `Car_03`） | `BP_VehiclePawn_Car_03` | `DA_Vehicle_Car_03` | `DA_Car_03_Presentation` → `SKM_Car_03` / `ABP_Car_03` | 悬挂、发射回弹、受击回弹、后轮拖尾开。旧 BP 的电击 Overlay 和丝带为空。当前默认选车目录未登记此车。 |

表中 BP 在 `/Game/Blueprints/Vehicles`，定义在 `/Game/Data/MVP_B`，表现 DA 在 `/Game/Data/Car_Presentation`，AnimBP 在 `/Game/Blueprints/AnimBP`，三款 Mesh 在各自的 `/Game/Assets/Cars/Car_0n`。三份表现 DA 的拖尾都使用 `NS_TireSkid`，轮位是 `BL` 和 `BR`。三个车辆 Definition 的状态目录覆盖均为空。清单来自本次对保存 BP、DA 和目录的只读核对。

**Car03 的双端入口。** 当前 `DA_Room_Development` 和 `DA_Room_Exhibition` 都引用同一默认选车目录，只有 `Car_01`、`Car_02`。测试 Car03 时，在独立测试分支由成员在 Editor 打开房间配置的 `Catalog`，在 `Vehicles` 加入 `Vehicle Id = Car_03`，`Vehicle Class = BP_VehiclePawn_Car_03`，保存并重启两端。该改动仅用于三车验收，是否加入正式选车列表由负责选车的成员确认。 [房间和目录接线](../Networking/GF3_Practice_Room_Implementation_CN_v0.1.md)

### 每辆玩家车继承的组件

| Pawn 组件或运行模块 | 工作与引用位置 |
|---|---|
| `VehiclePhysicsBody`、`VisualMesh`、`SkeletalVisual` | 物理根负责碰撞。主画面由 `SkeletalVisual` 或静态外观回退输出。状态 FX 从当前实际可见的外观 Mesh 取目标。 |
| `VehiclePresentation`（`UHALVehiclePresentationComponent`） | 驱动 AnimBP 的车轮、车身姿态、发射和受击回弹、悬挂，按车型 Presentation DA 运行。轮胎拖尾由它按配置生成，每轮使用本车的 Niagara 实例。 |
| `StatusFXPresentation`（`UHALVehicleStatusFXComponent`） | 每车独立汇总视觉 Tag 来源，按当前 Mesh 管理 MID 与 Niagara，并在换车、换图和退场时清理。通用表面与共享电击通过同一个 Overlay 协调器输出。 |
| `ShockedPresentation`（`UHALShockedPresentationComponent`） | 电击专用播放驱动，接本地回弹、观测者复制状态及权威 ASC Tag，负责闪光、扩散、丝带、点光与焦痕。共享电击 DA 开启后读取共用配方，回退模式读车 BP 的旧组件设置。 |

这些组件由 `ATestVehiclePawn` 基类组装。新车继承基类、接好 Definition 和可用的外观 DA，即可进入已有表现链路。单一 Skeletal Mesh 内的车身和车轮按一个 Mesh 处理。独立 Mesh 的尾翼等由 C++ 外观组装在 `Configure` 后通过 `RegisterStatusFXPart` 注册唯一语义 ID，重组时重新注册。运行时只接受同车、可见、已注册、有资产、具有有效变换且不独立模拟物理的外观组件。主车身使用保留 ID `Body`。 [FX-1 外观目标合同](MVPB_Vehicle_Status_FX1_Implementation_CN_v0.1.md) · [车辆模型与 AnimBP 接线](MVPB_Vehicle_Model_Import_And_AnimBP_Setup_CN_v0.1.md)

### 容易找错的资产

| 效果 | 资源和引用入口 |
|---|---|
| 车辆受电 | `/Game/Assets/FX/Materials/M_FX_VehicleShock_Overlay` 是车身材质，`/Game/Assets/FX/NS_VehicleShockRibbon` 是车身丝带，使用 `M_FX_ShockArc_Ribbon`。新入口是 `/Game/Data/StatusFX/DA_StatusFX_Shocked` 的 `Overlay Material` 和 `Shock Look → Ribbon System`。旧入口是车辆 BP 的 `ShockedPresentation`。 |
| 漂移／急刹拖尾 | `/Game/Assets/FX/NS_TireSkid`，材质 `/Game/Assets/Materials/M_TireSkid`。入口为各车型 **Presentation DA → Tire Skid → Enabled / System / Wheels**。系统参数 `User.SlipRatio` 为 Float，`User.RibbonID` 为 Integer。现有痕迹使用 World Space，生成后留在地面。材质、宽度及粒子寿命在 Niagara 和材质资产调节。 |
| 持球连接光束 | `/Game/Assets/FX/NS_BallMagnetBeam`，丝带材质 `/Game/Assets/FX/Materials/M_FX_MagnetBeam_Ribbon`。入口是 **BP_BasicBall → MagnetBeam → Beam System**，球端随持球状态更新，换车时仍从当前持有车辆取起点。 |
| 两柱之间的电弧 | 素材为 `/Game/Assets/FX/NS_ElectricArc`，当前保存的 `/Game/Blueprints/MapMechanic/BP_ElectricWallGimmick` 的四个 `ArcVisual` 系列 Niagara 组件都引用它。柱、可见墙面和场景接触属于机关。柱间电弧的表现接线放在机关侧，受电车辆仍由自己的状态特效驱动。 |

拖尾与持球光束沿用各自的触发和资源参数。制作时区分拖尾的世界空间、通用状态粒子的 Mesh 局部空间、以及电击专用的丝带参数。 [拖尾配置与斜坡测试](MVPB_Vehicle_TireSkid_Implementation_CN_v0.1.md) · [持球光束制作](MVPB_BallMagnetBeam_FX_Authoring_CN_v0.1.md) · [柱间电弧制作](MVPB_ElectricArc_FX_Authoring_CN_v0.1.md)

若要替换电击专用资产，保留 Overlay 的 Scalar `Brightness`、`FilamentPower`、`Flash`、`WipeRadius`、`FlickerSteps`、`ScorchAmount`，以及 Vector `HitLocationLocal`。车身丝带从驱动读取 `ArcPositions`（Position Array）、`ArcWidths`（Float Array）、`ArcPointsPerRibbon`／`ArcRibbonCount`（Integer）、`ArcAlpha`（Float）和 `ArcColor`（Linear Color）。 [电击驱动](../../Source/HAL_Future_Creation/HALShockedPresentationComponent.cpp)

## 2. 通用状态特效的制作合同

```text
GAS 的显式 State.Visual.* Tag ─┐
电击专用回弹/确认入口 ────────┼→ 每车 StatusFXPresentation → FX-1 外观目标 → Overlay / Niagara
仅本机的调试来源 ────────────┘                  ↑
项目默认目录或车型覆盖 → 精确 Tag → 共享效果定义 ─┘
```

- **配置分层。** `DA_VehicleStatusFX_Default` 是 **HALVehicleStatusFXCatalog**，将精确 Tag 映射到 **HALVehicleStatusFXDefinition**。效果定义填材质、Niagara 模板、强度曲线、优先级及视觉时序。一个 Tag 在一个目录中只能登记一次。车型覆盖只列真正特殊的 Tag。有效覆盖目录未列出的 Tag 继承默认，覆盖目录未就绪或无效时应先查看错误原因。
- **运行时隔离。** 同一车辆同一 Tag 按来源聚合，刷新不重启，来源全部移除后进入淡出或残留。MID、Niagara 和计时都归本车，配方 DA 作为只读共享数据。电击可提前开始，同一段播放在服务器确认后继续。FX 的淡出／焦痕时长独立于玩法状态时长。
- **表面层。** 同一 Mesh 由一个协调器写入 Overlay 并恢复原值。**两个兼容表面通道是首版能力和验收目标，状态数量独立管理，显示通道允许扩展。** 共用 `HALStatusFXSurfaceLayout` 和母材质时，默认通道为 `Primary/FX0_`、`Secondary/FX1_`。每通道接全局 Scalar `Intensity`、`Age`、`Residue`，在母材质中带前缀；配方的自定义参数填未加前缀的名字。独立 Overlay 默认用 `FX_Intensity`、`FX_Age`、`FX_Residue`，可在定义中改名。独立材质和其他不兼容表面按活跃／淡出优先于纯残留，再按 `Surface Priority`、精确 Tag 字典序选用；遮蔽期间各效果仍独立计时。共享电击材质当前参与这种择优，两个通道的合成用同布局的两份通用配方单独验收。
- **粒子层。** 配方内 `Layer Id` 唯一。**Bounds** 只填 Bounds System，用外观 Mesh 局部包围盒近似。**Mesh Surface** 分别填 Static 与 Skeletal 两份模板，使用相应的 Mesh Data Interface。Static Mesh 需打开 **Allow CPU Access**，Skeletal Mesh 在指定 Sampling LOD 打开同项。只有显式填写 Surface Fallback Bounds System 才会退到 Bounds。首版模板使用 CPU Sim、Local Space，采样模块若输出世界坐标须转换一次；CPU 采样需实测蒙皮以及 GPU 专属变形的差异。
- **接好图再验收。** 图中消费 `User.FX_Intensity`、`User.FX_Age`、`User.FX_Residue`、`User.FX_SpawnRate`（Float），`User.FX_MaxParticles`（Integer），`User.FX_BoundsCenter`、`User.FX_BoundsExtent`（局部 cm，Vector，Extent 为半尺寸）。表面模板还需正确类型的 `User.FX_SourceMesh` DI。发射率已经乘表现包络。模板按获分配预算限制发射，多 Emitter 共享该层分配。项目默认通用预算为每车 512 粒子／16 层，特殊电击丝带另做实际性能检查。 [FX-4 材质和 Niagara 接线细节](MVPB_Vehicle_Status_FX4_Implementation_CN_v0.1.md#2-测试表面先做两种颜色)

### 新资产交付规范

1. 共用 Niagara 放 `/Game/Assets/FX`，共用特效材质放 `/Game/Assets/FX/Materials`，状态目录和定义放 `/Game/Data/StatusFX`。沿用 `NS_`、`NE_`、`NM_`、`M_`／`MI_`、`T_` 和 `DA_` 前缀。独立两套表面模板可在文件名写清 `Static`／`Skeletal`。已有拖尾材质按现路径引用。
2. 从临时测试目录接入已有 `State.Visual.DebugA`／`State.Visual.DebugB` 验证外观；保留电击条目。要接新正式状态，先由程序确认已存在的玩法状态、视觉 Tag 及来源，再在目录登记配方。资产列表随提交附上项目内路径、依赖和参数的单位、类型、默认值。
3. 在 Editor 保存并对定义和目录执行 **Validate Assets**。两端重启后查看目录是否 Ready，随后检查实际画面。项目默认目录为软引用，已有 Cook 收集入口，定义中的材质与粒子为硬引用。增加新依赖后仍须用实际打包版检查是否可加载。目录 Ready 表示加载及结构验证通过，图形输出和首播耗时另验。
4. C++ 负责玩法、权威判断、组件生命周期与通用绑定；团队在 Unreal Editor 手动制作和保存 BP、AnimBP、材质、Niagara、DA 与地图。接续 agent 负责读源码、整理接线步骤和实现授权的 C++ 修改，遵守 [AGENTS.md](../../AGENTS.md) 的蓝图边界。纯特效任务使用现有事件及状态入口，碰撞回调、蓝图 Tick、材质和 Niagara 图保持在表现职责内。车辆 HP、伤害、眩晕、球权、物理与网络规则由已有权威链路决定。

## 3. 本轮操作和验收

### A. 启用共享电击

当前保存的 `DA_StatusFX_Shocked` 已引用车身电击材质，`Enable Rendering` 和 `Use Shared Shock Look` 都关闭。两项开启并配齐后才进入共享路径，开启前 Car02、Car03 的旧配置不会输出车身电击 Overlay 或丝带。

1. 用现有 Car01 拍一段旧效果作对照，画面覆盖碰墙、退墙和焦痕消退。
2. 打开 `DA_StatusFX_Shocked`，勾选 **Enable Rendering** 和 **Use Shared Shock Look**。按 FX-5 配置单填写 `NS_VehicleShockRibbon`、丝带 Width 10 cm 和其余 Car01 基线；电击 Fade Out 为 0.6 秒，Residue 为 4.0 秒，额外 Arc System 保持 None，灯光默认关闭。材质继续使用 `M_FX_VehicleShock_Overlay`。共享电击用专用材质字段与丝带协议，`Surface Layout` 和通用 Niagara／参数列表保持空。
3. 保存、校验并重启两端。用三款车轮流进场。若要对照旧效果，关闭 **Use Shared Shock Look**、保存并重启，按需要在同一场景复测。 [FX-5 逐字段配置与原始参数](MVPB_Vehicle_Status_FX5_Implementation_CN_v0.1.md#3-editor-配置)

### B. 启动与看日志

同一提交、同一引擎补丁和同一份已保存配置下，从两个独立 **Development** 游戏进程打开 `/Game/Maps/MVPB_NetEntry`。一个点 Host，另一个 Join。本机填 `127.0.0.1`，两台电脑填 Host 的局域网 IPv4。入口框只填 IP，程序使用端口 7777。两端用 Development 房间确认车辆，在练习房先测拖尾与换车，再由 Host 开赛去测电击。当前保存的 `MVPB_Match` 和旧直连图 `MVPB_NetTest` 依赖电墙 BP，练习房地图没有该直接依赖。若需在练习房复现，在独立测试地图布置电墙并注明地图和 Actor。

从项目根目录分别执行下列 PowerShell，把第二个进程的 `$role` 改为 `Client`，引擎路径按各自电脑填写。

```powershell
$role = 'Host'
$editor = 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project = (Resolve-Path '.\HAL_Future_Creation.uproject').Path
$logDir = Join-Path (Split-Path $project) 'Saved\Logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$log = Join-Path $logDir "VehicleFX_$role.log"
& $editor $project '/Game/Maps/MVPB_NetEntry' -game -log -windowed -ResX=1280 -ResY=720 "-abslog=$log"
```

游戏控制台命令如下，Debug A/B 要先配置对应测试配方；真实电击请实际碰墙。

```text
hal.StatusFXStatus all
hal.VehicleFXTargets
hal.ShockedPresentationLog 1
hal.StatusFXLog 1
hal.StatusFXDebug local State.Visual.DebugA A 1
hal.StatusFXDebug local State.Visual.DebugB B 1
hal.StatusFXDebug local State.Visual.DebugA A 0
hal.StatusFXDebugClear local
```

### C. 验收清单

| 顺序 | 操作 | 通过标准 |
|---|---|---|
| 1. 三车型电击 | Car01 对照旧视频，再用 Car02、Car03 各撞墙。轮换 Host、拥有该车的 Client、另一端观察角色。 | 本车回弹立即亮起，权威确认接续一次，观察端可见。退墙后丝带收束、焦痕消散、原 Overlay 恢复。三车的丝带尺度和包覆方向可读。 |
| 2. 重入与清理 | 焦痕仍在时再撞墙，然后在活跃或残留时返房、换车、再开局。 | 再触发时焦痕自然接续。旧车不残留材质、粒子或光，下一辆车能正常触发。 |
| 3. 通用双通道与组合 | 测试目录加载 DebugA/B，同布局两种颜色一起打开，交换关闭顺序；再把通用状态和真实电击叠在一起。 | 两个兼容通道都可见。电击与不兼容材质按优先级择优，纯焦痕让位给活跃效果，剩余效果按当时时钟恢复；其他粒子可继续。结束后恢复原 Overlay。 |
| 4. 粒子目标与回退 | 先测 Bounds，再测 Static／Skeletal Surface。至少测一个静态外观和一个骨骼车身，包含偏移、滚转、缩放与隐藏部件。临时用测试资产覆盖缺 CPU 数据场景。 | 粒子来自真实外观，随正确空间变换。失败层能在日志定位，配置替代时才看到 Bounds 回退。正常状态和驾驶继续可用。 |
| 5. 相关表现回归 | 直行、漂移／急刹、松手、离地与斜坡；拾球、脱球、发射和换持有者。 | 拖尾在合适的动作与接地条件下出现，停发后已生成痕迹留在地面直至消失；持球光束跟随实际持有者并按球状态收尾。 |
| 6. FX-6 压力项 | 记录首次命中与后续命中；四车同时触发电击，并叠加第二状态；有打包版本时重复。 | 记录卡顿、帧时间、粒子／层计数和视频。首播、Cook 和多人性能单独记录通过或失败。 |

### D. 排查与回传

- 目录未 Ready，先查默认或车型覆盖路径、Tag 重复、定义引用和素材校验报错。目录正常但无实例，查运行窗口和来源；有实例无目标，查 `hal.VehicleFXTargets` 与实际可见 Mesh。
- 有目标而无图，先看 `Enable Rendering`、材质参数名、粒子预算、CPU Access、LOD、DI 类型与图中参数绑定。`SurfaceChannels` 统计当前通用通道，多 Mesh 求和；`OverlayTargets` 可包含电击。**Rendered／Active** 表示已提交给 Niagara，仍需实看屏幕输出。
- 回传记录至少包含提交号、引擎补丁、地图、Vehicle ID、角色及对端观察结果、DA／资产路径与调过的字段、操作步骤、预期／实际、Host／Client 日志及短视频。问题可先归类为引用、触发、材质合成、粒子采样或回收，再交给对应成员。

## 4. 给接续 agent 的源码入口

| 查什么 | 从哪里读 |
|---|---|
| 车型组装与退场 | [TestVehiclePawn.cpp](../../Source/HAL_Future_Creation/TestVehiclePawn.cpp)、[VehicleDefinition.h](../../Source/HAL_Future_Creation/VehicleDefinition.h)、[HALVehiclePresentationConfig.h](../../Source/HAL_Future_Creation/HALVehiclePresentationConfig.h) |
| 目标 Mesh、Bounds 与部件生命周期 | [HALVehiclePresentationFXTargets.cpp](../../Source/HAL_Future_Creation/HALVehiclePresentationFXTargets.cpp)、[HALVehicleFXTargets.h](../../Source/HAL_Future_Creation/HALVehicleFXTargets.h) |
| 目录、配方、预加载与预算 | [HALVehicleStatusFXSubsystem.cpp](../../Source/HAL_Future_Creation/HALVehicleStatusFXSubsystem.cpp)、[HALVehicleStatusFXDefinition.h](../../Source/HAL_Future_Creation/HALVehicleStatusFXDefinition.h)、[HALVehicleStatusFXSettings.h](../../Source/HAL_Future_Creation/HALVehicleStatusFXSettings.h) |
| 来源、实例、渲染与恢复 | [HALVehicleStatusFXObservation.cpp](../../Source/HAL_Future_Creation/HALVehicleStatusFXObservation.cpp)、[HALVehicleStatusFXRuntime.cpp](../../Source/HAL_Future_Creation/HALVehicleStatusFXRuntime.cpp)、[HALVehicleStatusFXRenderer.cpp](../../Source/HAL_Future_Creation/HALVehicleStatusFXRenderer.cpp)、[HALStatusFXParticleBinding.cpp](../../Source/HAL_Future_Creation/HALStatusFXParticleBinding.cpp) |
| 电击、拖尾、持球光束 | [HALShockedPresentationComponent.cpp](../../Source/HAL_Future_Creation/HALShockedPresentationComponent.cpp)、[HALVehicleTireSkid.cpp](../../Source/HAL_Future_Creation/HALVehicleTireSkid.cpp)、[HALMagnetBeamPresentationComponent.cpp](../../Source/HAL_Future_Creation/HALMagnetBeamPresentationComponent.cpp) |

FX-5 的既有证据包括 UE 5.8.3 两 Target 编译、183/183 原生、共享电击的 D3D12 组件检查与现有保存配置的双端回归。共享 DA 在三车上的实际画面、正式粒子模板、打包及压力验收以团队报告为准。 [FX-5 验证详情](MVPB_Vehicle_Status_FX5_Implementation_CN_v0.1.md#5-本轮构建与验证) · [FX-6 范围](MVPB_Vehicle_Status_FX_Discussion_CN_v0.1.md#6-建议施工顺序与验收)
