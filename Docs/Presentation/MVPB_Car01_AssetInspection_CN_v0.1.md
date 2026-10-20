# Car_01 已导入素材检查（v0.1）

日期：2026-10-20（Asia/Tokyo）。检查基点为干净 `main / 2e4176d9370261d8165664c41dac3ebb21e7386a`，UE `5.8.3-58210709+++UE5+Release-5.8`。VP-1 准备代码已在 `9983c5f` 提交，最新 `2e4176d` 新增 20 份车辆素材。

**结论：Car_01 满足进入 VP-1 AnimBP／Data Asset 接线的基础条件，未发现依赖或骨骼链阻塞。实际画面、单轴姿态预览、最终缩放和双端运行仍待验收。** 已导入素材不等于已替换运行中的车辆。

详细数据见 [只读报告](MVPB_Car01_AssetInspection_20261020.json)，复查工具为 [InspectVehicleModel.py](../../Tools/InspectVehicleModel.py)。下一步按 [导入与 AnimBP 配置流程](MVPB_Vehicle_Model_Import_And_AnimBP_Setup_CN_v0.1.md)第 4 节校准及第 5～9 节接线执行。

## 1. 文件和依赖

| 项目 | 实际结果 |
|---|---|
| Skeletal Mesh | `/Game/Assets/Cars/SKM_Car_01`，类型确认为 SkeletalMesh |
| Skeleton | `/Game/Assets/Cars/SK_Car_01`，已被 Mesh 引用 |
| Physics Asset | `/Game/Assets/Cars/PA_Car_01`，已被 Mesh 引用 |
| 新增资源 | 模型相关 3 份、材质实例 12 份、贴图 5 份，共 20 份，全部可加载 |
| 材质 | 12 槽、12 个 LOD0 section；父材质 `/InterchangeAssets/Materials/FBXLegacyPhongSurfaceMaterial` 可加载 |
| 依赖 | 导入资源直接非 Script 包依赖及材质父资产未发现缺失；未执行 Shipping Cook 或完整递归打包验收 |
| 法线贴图 | Tire_Normal：sRGB=false、TC_NORMALMAP；其余 4 份：sRGB=true、TC_DEFAULT |
| 原始 FBX | 导入元数据为 `D:/maya files/HAL_Future_Creation_Assets_Files/Car_01_test.fbx`，本机无此文件；素材成员需保留可追溯源文件与导出设置 |

PA_Car_01 可以保留。当前 SkeletalVisual 仍为 NoCollision、不模拟物理，权威移动与碰撞继续由原 VehiclePhysicsBody 执行，不用 PA_Car_01 替换车辆碰撞根。

## 2. 骨骼和权重

- 共 **17 根骨骼**。Root_jnt 为唯一根；Chassis 和四条 suspension→steer→spin 链的直接父子关系符合现有源码的 ValidateRig 结构约定。
- 前轮／前保险杠位于 +X，后轮位于 -X；左轮为 -Y、右轮为 +Y，符合 +X 前／+Y 右／+Z 上。
- 四个 spin 轴心与对应轮网格包围盒中心基本重合，Z≈47.5 cm，未见明显偏心的静态数据迹象。
- LOD0 有 **5823 个源顶点、10050 个三角形、10850 个渲染顶点**；源／渲染顶点可因接缝等不同。
- 所有源顶点均为单骨骼、权重 1；无无权重／未归一顶点、多影响顶点或跨轮权重。
- FL／FR／BL／BR 各有 435／436／435／435 个源顶点绑定到各自 spin；其余属于 Chassis、前后保险杠和尾翼，支持预期的刚性分组。

本次按原生源码条件比对骨骼名／直接父级，没有通过 Python 调用非反射的 ValidateRig()。真实配置尚未创建，因此不记为完整 Config.Validate()／运行接线通过；蒙皮观感还需姿态预览。

## 3. 实测轴向与尺寸

| 配置 | FL／FR／BL／BR 的候选值 |
|---|---|
| SteeringAxis | **Y**，该局部轴在 Mesh 空间指向 -Z |
| SteeringSign | **-1**，令正转向对应 Mesh +Yaw |
| SpinAxis | **Y**，该局部轴在 Mesh 空间指向 +Y |
| SpinSign | **+1** |
| bSteers | FL／FR=true，BL／BR=false |

轴向根据实际参考四元数推导，对齐度均为 1；**不能沿用默认 SteeringAxis=Z**。上述以模型 +X 前、旋转 Add to Existing／Bone Space 为前提，接线后仍做小角度单轴、前进／倒车预览。

Root 本身有非零导入旋转，子骨骼全局参考姿态已补偿。方案要求 Root 不叠加驾驶动画，不要求导入 Root 四元数为单位值；此项不阻塞现有代码。

| 几何数据 | Mesh 缩放 1 时 |
|---|---|
| 长／宽／高 | **435.156／248.552／194.801 cm** |
| 四轮 X／Z 外包围半径 | 约 **47.500 cm**，直径约 95 cm |
| 各轮轴心到最远 XZ 顶点 | 约 **47.834 cm** |
| 前轮中心 | X≈109.053，Y≈±89.515，Z≈47.5 cm |
| 后轮中心 | X≈-120.344，Y≈±94.952，Z≈47.5 cm |

轮转可先以 47.5 cm 为缩放 1 的校准起点；最远顶点半径与外包围半径略有不同，最终装饰滚动半径按预览确定。RadiusCm 填**最终世界半径**，需包含实际总缩放。例如总均匀缩放 0.5 对应约 23.75 cm；0.5 只是换算示例，尚未定案。

模型与既有物理根／控球点的场景对齐仍待执行。应调整表现 RelativeTransform，不因外观改动已调物理参数。当前 1 个 LOD、12 个材质 section 可进入接线测试；四车渲染、材质效果、裁剪与性能尚未测量，不标记通过。

## 4. 下一步与证据边界

项目扫描未发现 AnimBlueprint 或 HALVehiclePresentationConfig 资产，玩家 Definition.Presentation 为空，已保存车辆继续使用旧 Static Mesh。接线成员继续：

1. 在 Mesh／Skeleton Editor 验证本报告轴向、符号及蒙皮，确定实际缩放和世界轮半径。
2. 创建目标为 SK_Car_01、原生父类 HALVehicleAnimInstance 的 AnimBP，连接前轮 steer／四轮 spin 的六节点图，读取 Presentation 各轮 Rotation。
3. 创建 HALVehiclePresentationConfig，填 Mesh／AnimBP、骨骼、轴向／符号和半径，确认后才标 Ready。
4. 引用到实际 VehicleDefinition.Presentation，保留原 Visual 回退；单机检查后主客互换驾驶，确认持球／发射基准保持正确。

使用 UE 命令行和进程级 PythonScriptPlugin／GeometryScripting 只读加载；SkeletonModifier 仅查询其内部副本，LOD0 复制到临时 DynamicMesh 后读权重与位置。未调用 Commit／Save／Reimport，未编辑 Blueprint。

**351 份 uasset／umap 前后 SHA256 全同，检查进程正常退出。** 本次没有生产源码／配置数值／资产修改、编译／自动化／PIE／双端／渲染检查。原 49/49 与数据双端结果是历史代码证据，不能替代本次素材画面验证。

本次新增只读检查脚本、JSON、本文和状态入口更新，尚未提交／Push。VP-2 车身动态、VP-3 视觉悬挂、VP-4 回弹未施工；F4 仍后置到 MVP-B 收尾。
