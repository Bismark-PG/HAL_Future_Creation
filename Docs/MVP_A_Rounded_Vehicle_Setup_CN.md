# MVP-A 圆角测试车：引擎配置与验证

## 1. 代码变更

- 玩家测试车的 Box 根组件改为 `VehiclePhysicsBody`（Static Mesh Component，C++ 属性仍叫 `CollisionRoot`）。
- 物理根继续负责 Chaos 模拟、驾驶、车头方向、控球约束、后坐力及碰撞脱球；`VisualMesh` 只负责显示。
- 保留默认质量 800 kg、线性阻尼 0.15、角阻尼 0.8、CCD、碰撞事件和原有子组件位置。
- 物理根默认 Hidden In Game，不会连带隐藏 `VisualMesh`。
- 未指定网格、没有简单碰撞或使用 Complex As Simple 时，在 Output Log 报错并停用物理；未模拟物理的车辆不会启动自动控球。
- 本次不修改木桩车、墙体、蓝图或关卡资产，不增加悬挂、落地辅助或自动翻身。

## 2. 编译与迁移注意事项

这次更换了原生根组件类型和子对象名。保存并关闭 Editor 后，完整编译 `HAL_Future_CreationEditor / Development / Win64`，再打开项目；不要用 Live Coding 迁移组件模板。

旧蓝图可能缓存旧 Box 模板，因此推荐新建直接继承 C++ `TestVehiclePawn` 的测试蓝图，而不是复制旧蓝图或继承旧蓝图。旧资产保留用于查阅参数，不删除。

旧蓝图即使能打开，也可能因为新物理根没有指定网格而不能运行。先完成下列配置，再替换测试场景的玩家车辆。

## 3. 制作圆角占位网格

工程已经启用 Modeling Tools Editor Mode。以下操作由团队手动完成，输出一个新网格，不编辑共享的引擎 Cube。

1. 在测试关卡空白位置，切换左上角模式下拉框到 Modeling（建模）。
2. 使用 Create / Shapes → Box，输出类型选 Static Mesh。
3. 制作整体尺寸约 `X=200、Y=140、Z=70 cm` 的低矮长方体，长轴沿本地 +X；Width / Depth 的对应以视口坐标轴为准。各方向 Subdivisions 保持 1。
4. 完成/Accept。找到生成的网格，将其命名为 `SM_MVP_RoundedVehicle`，保存到项目自己的 Meshes 目录。
5. 选中该网格 Actor，进入 Model → PolyGroup Edit（部分界面缩写为 PolyEd）。Box 工具生成的网格自带 PolyGroups。
6. 在 Selection Filter 中只启用边选择，Shift+点击逐一选中 Box 的 12 条外边（转动视角检查背面）。
7. 点击 Edge Edits → Bevel，先使用 `Bevel Distance=15 cm、Subdivisions=3、Round Weight=1`。这是第一轮测试建议值，不是固定玩法规则。
8. 先 Apply/完成 Bevel 子操作，再 Accept 整个 PolyGroup Edit 工具。观察形状：棱角已圆润，底部仍有明显平面；出现自交或底部过度收窄时，降低 Bevel Distance。
9. 在 Modeling 的 XForm → Edit Pivot 中，将资产 Pivot 设置到包围盒中心并 Accept。Actor 的世界位置不重要，但资产本地中心应接近 `(0,0,0)`，车头方向应为 +X。
10. 网格的尺寸直接做进资产，不靠车辆根组件的非均匀缩放修正。保存资产。

第一轮保持与原 Box 相近的外包尺寸，避免同时改变车宽、重心和驾驶参数，便于对比卡墙改善程度。这里的倒角量是建议起点；如果俯视轮廓仍太方，可以另做一个更圆润的版本对比。

参考：[Epic 建模快速入门](https://dev.epicgames.com/documentation/en-us/unreal-engine/modeling-mode-quick-start-in-unreal-engine)、[PolyGroup Edit](https://dev.epicgames.com/documentation/unreal-engine/polygroup-edit-tool-in-unreal-engine)。

## 4. 生成简单凸包碰撞

1. 双击 `SM_MVP_RoundedVehicle`，打开 Static Mesh Editor。
2. Collision → Remove Collision，删除生成工具可能附带的旧 Box 碰撞，否则尖角 Box 仍可能参与物理。
3. Collision → Auto Convex Collision；在 Convex Decomposition 面板中以 `Hull Count=1、Max Hull Verts=32、Hull Precision=100000` 为起点，点击 Apply。车身本身是凸形，先使用一个凸包。
4. 开启视口的 Show → Simple Collision，检查绿色线框是否真实贴合圆角，而不是一个尖角长方体。凸包近似会损失部分曲面细节；如近似过粗，可在工具允许范围内增加顶点数并重新生成。
5. Details → Collision Complexity 设置为 `Simple And Complex`。不要使用 `Use Complex Collision As Simple`，也不要重新添加 Box Simplified Collision。
6. 保存网格。

参考：[Epic Auto Convex Collision](https://dev.epicgames.com/documentation/en-us/unreal-engine/add-a-collision-hull-to-a-static-mesh-using-the-auto-convex-collision-tool?application_version=4.27)（基础操作参考；5.8 的面板布局以实际界面为准）。

## 5. 新建并配置测试车辆蓝图

1. Content Browser → Add → Blueprint Class → All Classes，搜索 C++ `TestVehiclePawn`，创建 `BP_RoundedTestVehiclePawn`。
2. 选择根组件 `VehiclePhysicsBody`（某些界面可能显示 C++ 属性名 `CollisionRoot`），将 Static Mesh 指定为 `SM_MVP_RoundedVehicle`。
3. 检查根组件：Scale `(1,1,1)`、Mobility=Movable、Collision Presets=PhysicsActor、Collision Enabled=Query And Physics、Simulate Physics=true、Enable Gravity=true、Simulation Generates Hit Events=true、CCD=true、Override Mass=true 且 Mass=800 kg。
4. 保持根组件 Hidden In Game=true；不要对所有子组件传播隐藏。
5. 选择 `VisualMesh`，也指定 `SM_MVP_RoundedVehicle`，将 Relative Location / Rotation 归零、Scale 设为 `(1,1,1)`，配置正常材质槽 Element 0（不是 Overlay Material）。保持 No Collision、Simulate Physics=false、Visible=true、Hidden In Game=false。
6. 对比旧车辆蓝图，复制六个输入资产引用：Default Mapping Context、Steering、Throttle、Brake、Handbrake、Launch。
7. 手动复制已调好的 ArcadeMovement、BallControl、Health 和 CameraBoom / TopDownCamera 参数，以及任何额外的纯表现设置。不要复制旧 Box 根组件。
8. 默认 ForwardArrow 位于 `(110,0,0)`，BallControlPoint 位于 `(170,0,20)`。此尺寸下先保持默认；如果改变车长，再检查带球是否与车头发生重叠。
9. Compile、Save。确认组件树只有一个真正模拟物理的车身，VisualMesh 不参与碰撞。

物理根与视觉网格暂时可以使用同一个资产。未来正式外观与简化碰撞网格可分别配置，保持物理体略小于外观。

## 6. 替换玩家出生方式

- 如果玩家由 GameMode 自动生成：打开当前测试 GameMode 蓝图，将 Class Defaults → Default Pawn Class 改为 `BP_RoundedTestVehiclePawn`，Compile / Save。关卡 World Settings 的 GameMode Override 应使用这个测试 GameMode。
- 如果玩家是预放置的车辆：先在测试场景里停用旧车辆的自动占有，再放置新蓝图，设置 Auto Possess Player=Player 0。不要同时让 GameMode 和场景车辆各生成/占有一辆玩家车。
- 出生点放在地面上方，避免凸包嵌入地面。地面仍需对 Visibility=Block。

## 7. 手动测试

1. 检查 Output Log，没有 VehiclePhysicsBody 配置错误或 Cannot Simulate Physics 警告。
2. 低速斜擦墙、侧面贴墙行驶、倒车脱离、墙边静止漂移左右转向。
3. 高速正撞、斜撞、墙角连续碰撞、腾空和落地；圆角减少勾边，但不保证任何情况下都不会翻车或卡住。
4. 自动控球、带球、移动/静止发射及后坐力、碰撞脱球和重新捕获锁定。
5. 球击中圆角车辆后的扣血和不同档位击退，确认碰撞事件及目标物理体仍正确。
6. 在同一墙体和驾驶参数下与之前 Box 的体验记录对比；旧蓝图不能作为仍可运行的 Box 对照。需要实时 A/B 测试时，另做一个同尺寸尖角 Box 网格并生成简单碰撞，在同一个新蓝图中切换物理和视觉网格。若持续顶墙仍难转身，再检查摩擦、接地 Trace 是否命中墙以及横向抓地力；不要一次同时修改全部因素。

悬挂、落地辅助、倒扣翻身和动态摄像机不在本次施工范围。

## 8. 本次验证记录

- `HAL_Future_Creation / Win64 / Development`：编译通过。
- `HAL_Future_CreationEditor / Win64 / Development`：完整编译通过。
- `git diff --check`：通过。
- 未创建或修改模型、蓝图、关卡资产，未运行 PIE；墙边手感、凸包形状和球流程回归需要按上述步骤手动验证。

## 9. 驾驶参数迁移核对（2026-09-17）

使用 `Tools/InspectVehicleTuning.py` 通过 Unreal 只读加载已保存的 Class Default Object，比较旧 `BP_TestVehiclePawn` 与新 `BP_RoundedVehiclePawn`。不编译或保存蓝图。

检查时，新车的驾驶参数仍是 C++ 默认值。旧车有以下 9 项自定义数值未迁移；在新蓝图的 Arcade Movement 中手动恢复即可，不复制旧的碰撞组件引用。

| 分组 | 参数 | 新车检查值 | 旧车应恢复值 |
|---|---|---:|---:|
| Steering | Steering Angular Acceleration | 4.5 | 8 |
| Steering | Full Steering Speed | 800 cm/s | 500 cm/s（5 m/s） |
| Steering | High Speed Steering Scale | 0.45 | 0.8 |
| Steering | Handbrake Steering Multiplier | 1.45 | 1.8 |
| Steering | Yaw Damping Rate | 2.2 | 1.6 |
| Steering | Handbrake Yaw Damping Rate | 0.8 | 0.35 |
| Grip | Lateral Grip Rate | 8 | 10 |
| Grip | Handbrake Grip Rate | 1.8 | 0.2 |
| Grip | Max Lateral Grip Acceleration | 4500 cm/s² | 2800 cm/s²（28 m/s²） |

Drive 数值与六个输入资产引用一致，无需重设。Handbrake Minimum Steering Authority=0.5、Handbrake Reverse Steering Speed=150 cm/s 也一致，保留即可。

恢复步骤：新蓝图 → Arcade Movement → 清空搜索 → 按上表填写 → Compile / Save → 重新进入 PIE。确保 GameMode 的 Default Pawn Class 使用新蓝图。先测试正常中高速转向，再测试漂移与墙边低速甩尾。此次没有直接修改蓝图，也没有运行 PIE；圆角碰撞改变了接触与惯性，因此相同调参不保证手感与旧 Box 完全相同。

只读脚本已成功输出三组参数。诊断进程另有原生 CDO 构造期 `GetSimplePhysicalMaterial: GEngine not initialized` 日志，进程退出码为 1，并非完全无错误运行；此项未在本次迁移中修复，不能据此声称物理初始化验证通过。

## 10. 低速顶墙甩尾辅助（2026-09-17）

本次在 `ArcadeVehicleMovementComponent` 内实现，不改蓝图、凸包或关卡，不覆盖第 9 节已调好的驾驶参数。

- 仅在接地、车身未严重侧翻、低速且同时按住油门＋漂移＋方向时，探测车头前方附近的墙。
- 使用物理根自己的简单碰撞几何进行前向 Sweep，不使用视觉模型包围盒替代圆头凸包。最多 4 次查询、记录 2 个墙面；忽略车辆和球，并过滤地面/坡面法线。
- 墙的 Object Type 必须为 `WorldStatic`，并实际阻挡车辆物理根。物理测试车与球不触发此辅助。
- 全面启用后，只把油门动力的顶墙分量降到默认 5%，保留沿墙分量；刹车、倒车、阻力和限速修正不参与此削弱。
- 用有限 Yaw 角加速度追踪目标转速，不设置位置、姿态或角速度，不锁 Pitch/Roll、不自动扶正，也不额外施加离墙推力。
- 释放组合键、离开墙面或超过退出速度后，默认 0.2 秒淡出；腾空、严重侧翻、停用功能或重置输入时立即清除辅助。
- 速度阈值使用地面平面内的总速度，包含横向滑动；常规中高速漂移沿用现有计算。

### 引擎配置

Editor 完全关闭后编译 `HAL_Future_CreationEditor / Win64 / Development`，再打开项目。不要新建蓝图或迁移组件。

打开实际使用的 `BP_RoundedVehiclePawn` → Components 选择 `Arcade Movement` → 清空 Details 搜索 → 展开 `Arcade Vehicle → Wall Escape`（也可搜索 `Wall Escape`）。新增参数默认如下，第一轮无需更改：

| 参数 | 默认值 | 用途 |
|---|---:|---|
| Enable Wall Escape | 开启 | A/B 对照开关 |
| Wall Escape Enter Speed | 300 cm/s（3 m/s） | 开始辅助的最大平面速度 |
| Wall Escape Exit Speed | 450 cm/s（4.5 m/s） | 活跃状态退出速度，避免临界抖动 |
| Wall Escape Probe Distance | 15 cm | 凸包前向探测距离，不是车头到车中心距离 |
| Wall Escape Minimum Steering | 0.2 | 摇杆方向死区 |
| Wall Escape Into Wall Throttle Scale | 0.05 | 顶墙动力保留比例，0 完全削弱、1 不削弱 |
| Wall Escape Target Yaw Rate | 2.6 rad/s（约 149°/s） | 满方向时目标甩尾转速，不是角速度硬上限 |
| Wall Escape Yaw Response Rate | 10 s⁻¹ | 转速追踪响应强度 |
| Wall Escape Max Yaw Acceleration | 18 rad/s² | 辅助转向角加速度上限 |
| Wall Escape Blend In Time | 0.08 s | 从正常驾驶混合到辅助 |
| Wall Escape Blend Out Time | 0.2 s | 从辅助混合回正常驾驶 |

Compile / Save 蓝图后重新进入 PIE。GameMode / Player Start 和输入映射不需要为此功能修改。

### 手动验证与调参

1. 先在开阔处测试正常驾驶、中高速漂移、倒车，再正面撞一段独立 Cube 墙。
2. 持续按 Xbox B 油门，加入左摇杆左/右方向＋RB 漂移，不按 A 刹车。左右各测试一次；预期顶墙动力减弱，车头更容易转开，转开后油门自然沿新车头方向带车离开。
3. 分别测试松 RB、松方向、松油门，以及沿墙斜擦、90° 内墙角、连续碰撞和高速撞墙减速后的重新转向。
4. 在同一位置把 Enable Wall Escape 关闭，对比辅助前后；再开启。不要同时改凸包、摩擦和其他驾驶参数。
5. 测试球命中、车车碰撞、带球贴墙、发射后坐力和脱球，确认不因辅助引入新的异常。腾空或倒扣不应触发本功能。
6. 如果不能触发：在蓝图组件的 `Arcade Vehicle → Debug` 开启 `Draw Wall Escape Debug`，Compile / Save。PIE 时满足输入和低速条件会显示青色探测方向线；绿箭头表示当前有效墙面外法线，橙箭头表示淡出。箭头从车中心绘制，不是接触点。
7. PIE 中 F8 退出占有，选择正在运行的玩家车辆，再选 Arcade Movement，查看 Debug 的 `Grounded`、`Wall Escape Active`、`Wall Escape Blend`。确认接地、有效组合输入、墙为 WorldStatic、物理根有简单碰撞且双方 Block。Class Defaults 不是运行时状态。

已经触发但转身仍慢：先把 `Max Yaw Acceleration` 从 18 调到 24；能起转但转速偏慢，再把 `Target Yaw Rate` 从 2.6 调到 3.2。如果仍持续顶墙，可把 `Into Wall Throttle Scale` 从 0.05 调到 0。只改一项后测试；不要无上限增加转向力量。

退出时动力恢复过快，可把 `Blend Out Time` 调到 0.25–0.3 秒；转开后仍觉得拖沓则调到 0.12–0.15 秒。

辅助保留真实碰撞后果：凸包转动仍需要空间，过窄夹角、多个阻挡组件或不合理摩擦可能继续卡住；本次不保证完全原地旋转或必定脱困。实际圆头凸包的手感、翻车和连续碰撞需要人工 PIE 回归，自动化测试不能代替。

### 本次验证

- Editor 与 Game Target：完整编译通过。
- 自动化测试组 `HAL.FutureCreation.Vehicle.WallEscape`：最终 5 项通过，涵盖输入/速度门槛与迟滞、单墙及双墙动力投影、混合时间、限幅转速追踪，以及临时物理场景中的 Static Mesh 根探测、支撑地面、动态阻挡排除、开阔地面、倒扣/空中禁用和功能开关。临时物理体使用只读的引擎 Cube 网格，尚未针对玩家自己的圆头凸包做动态驾驶验证。测试不保存任何资产。
- 启动日志仍包含之前已记录的原生 CDO 构造期 `GEngine not initialized` 3 条错误；这不是本次新增错误，未在此次修复。两轮测试退出码均为 0，不代表启动日志完全无错误。
- 未运行玩家操作的 PIE，未修改蓝图/关卡/网格资产。
