# MVP-B 车辆模型导入与 AnimBP 配置流程（v0.1）

日期：2026-10-19（Asia/Tokyo）。对应 [车辆表现方案](MVPB_Vehicle_Presentation_Rig_Plan_CN_v0.1.md)的 VP-0／VP-1。本文供素材导入、Editor 接线与联调成员使用；代码代理不直接编辑 Unreal 资产。

2026-10-20补充：Car_01已导入并通过基础只读结构／权重／依赖检查，见[素材实测报告](MVPB_Car01_AssetInspection_CN_v0.1.md)。接该车型时可从第4节开始，转向轴候选为Y／-1、轮转Y／+1；原始半径约47.5cm须乘最终总缩放，不能直接沿用默认转向Z或未校准半径。

**本轮代码范围为模型可选接入、前轮转向、四轮滚动和安全数据传递。车身倾斜、视觉悬挂、发射／冲击回弹及物理悬挂尚未接入，不要按总方案中的后续示意寻找尚不存在的字段。** 编译／自动化与实测结果以 [项目状态](../Project_Status_CN.md)及本目录施工记录为准，本文中的步骤不是已执行的导入验收。

## 1. 操作前准备与分工

1. 拉取包含本轮原生类的完整代码，核对团队统一的 UE 补丁。本机开发环境为 **UE 5.8.3**，`.uproject` 中只写 5.8，不能用它代替补丁核对。
2. 使用正常完整 Editor Target 构建成功的版本。新增反射类／组件后，应关闭旧 Editor／Live Coding 再完整编译，并重新启动 Editor。
3. 素材成员可以先导入模型／Skeleton；AnimBP、Data Asset 和车辆引用接线在 C++ 类型可用后进行。
4. 新增素材使用独立目录，避免覆盖现有物理碰撞模型、已调 Data Asset 或车辆 Blueprint；与负责接线的人协调同一二进制资产的修改。
5. 先在 `/Game/Maps/Test` 或 `/Game/Maps/MVPB_NetTest` 的副本／测试实例验证。当前默认车型仍应有原 Static Mesh 视觉配置作为回退。

建议目录和名称如下，仅是团队命名建议，不是代码硬编码路径：

```text
/Game/Vehicles/<车型>/Meshes/SK_<车型>
/Game/Vehicles/<车型>/Meshes/SKEL_<车型>
/Game/Vehicles/<车型>/Animations/ABP_<车型>_Presentation
/Game/Vehicles/<车型>/Data/DA_<车型>_Presentation
```

## 2. 素材交付清单

导入前向模型成员索取以下信息。没有的信息填“待测”，不要用截图推断尺寸。

| 数据 | 交付要求 |
|---|---|
| 源文件 | DCC／FBX 文件位置、版本或提交、导出工具与设置 |
| 单位与朝向 | 最终 UE 车辆坐标为 +X 前、+Y 右、+Z 上；记录导入旋转／缩放 |
| 参考姿态 | 车辆静止、四轮中立转向、车轮位于设计参考高度 |
| 骨骼 | 一个真正根骨骼，Chassis 与四个悬挂骨骼直接挂根；每轮 suspension→steer→spin |
| 轴心／权重 | steer／spin 轴心在正确轮位；刚性部件权重不串到其他轮子／车身 |
| 每轮数据 | 轮位、悬挂／转向／滚动骨骼名、转向局部轴及符号、滚动局部轴及符号、实际轮胎半径 |
| 材质／贴图 | 材质槽、引用与目录；首轮用 Skeletal Mesh 自身材质槽，无新增逐实例材质覆盖字段 |

当前讨论骨骼及截图见 [方案第 4 节](MVPB_Vehicle_Presentation_Rig_Plan_CN_v0.1.md)。建议直接保留以下命名：

```text
Root_jnt
├─ Chassis_jnt
│  ├─ Bumper_rear_jnt
│  ├─ Bumper_front_jnt
│  └─ Spoiler_jnt
├─ Wheel_FL_suspension_jnt → Wheel_FL_steer_jnt → Wheel_FL_spin_jnt
├─ Wheel_FR_suspension_jnt → Wheel_FR_steer_jnt → Wheel_FR_spin_jnt
├─ Wheel_BL_suspension_jnt → Wheel_BL_steer_jnt → Wheel_BL_spin_jnt
└─ Wheel_BR_suspension_jnt → Wheel_BR_steer_jnt → Wheel_BR_spin_jnt
```

当前校验要求上述控制链的直接父子关系。导出器若额外生成 Armature／包装根，需在素材导出／导入阶段处理，不能仅把未验证层级标成 Ready。保险杠／尾翼不是首轮必填控制骨骼。

## 3. 导入 Skeletal Mesh 与 Skeleton

### 3.1 FBX 路径

1. 在 Content Browser 打开目标车型目录，使用 Import 或拖入 FBX。
2. 确认导入类型包含 **Skeletal Mesh／Geometry and Skinning Weights**，不能只导入成 Static Mesh 或只导入动画。
3. 首次导入新骨架时 Skeleton 留空，让导入器创建新 Skeleton；复用已有 Skeleton 前，必须由素材负责人确认层级和参考姿态一致。首次接入本车型优先独立 Skeleton。
4. 本轮使用程序化骨骼表现，没有必需 Animation Sequence；可关闭动画、Morph Target 等不需要的导入内容。
5. 关闭 **Create Physics Asset**；若素材已有 Physics Asset，也不把它当作车辆物理根。运行时代码仍会强制关闭该 Skeletal Mesh 组件的碰撞和物理模拟。
6. 使用与素材一致的法线／切线导入设置。记录单位、旋转和缩放，先确认车型朝向与大小，再确认导入。
7. 检查导入日志、材质引用与生成的 Skeleton；保存新资产。

UE 5.8 可能显示 Interchange Pipeline Configuration，已有项目／格式也可能使用传统 FBX Import Options。按上述选项含义配置，不要求切换或关闭导入插件。界面字段可用英文关键词搜索。[Epic Interchange 参数](https://dev.epicgames.com/documentation/en-us/unreal-engine/interchange-import-reference-in-unreal-engine)

Epic 的 FBX 导入文档说明其管线使用 FBX 2020.2；导出版本不同时先做小样兼容性验证。[Epic FBX Skeletal Mesh 导入](https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-skeletal-meshes-using-fbx-in-unreal-engine)

### 3.2 已有 Unreal 资产路径

若另一个成员已在同版本 UE 中完成导入，优先通过仓库同步已保存资产及全部依赖，或用 Editor 的资产迁移功能保持依赖；不要单独复制一个 `.uasset` 后假设 Skeleton／材质已经齐全。接收后仍执行下一节检查。

## 4. 导入后逐轴校准

### 4.1 检查参考姿态与层级

1. 打开 Skeletal Mesh／Skeleton Editor，显示骨骼及坐标轴。
2. 核对 Root、Chassis 与四条控制链的名称、直接父级。
3. 转动 Chassis，确认只带动车身、保险杠和尾翼；四轮不随额外车身旋转。
4. 平移一个 suspension 骨骼，确认该轮 steer／spin 子级跟随。
5. 检查每个刚性部件的蒙皮归属；轮子不应拖拽车身或相邻轮子。

### 4.2 确定转向／滚动轴与符号

1. 分别给每个前轮 steer 骨骼一个小的正向局部旋转，记录 X／Y／Z 哪一轴产生正确转向。
2. 约定正转向对应车辆 +Yaw／向右，记录该轮应使用 `SteeringSign=+1` 还是 `-1`。
3. 给每个 spin 骨骼约 90° 正向局部旋转，确认围绕轮轴滚动而非摆动；记录对应局部轴。
4. 约定车辆向前行驶时的正确轮转方向，填写 `SpinSign=+1` 或 `-1`。
5. 四轮分别记录，不依赖镜像外观或 Skeleton Tree 的显示顺序。后轮虽然首轮不转向，名称与轴字段仍保持有效。
6. 清除临时预览变换，回到正确参考姿态；不要把校准用的临时偏移烘焙进正式参考姿态。

### 4.3 半径与显示对齐

`RadiusCm` 表示**在游戏场景里的实际滚动半径，单位 cm，包含模型／组件缩放后的结果**。例如源模型半径 20 cm、最终视觉统一缩放 1.5，则实际半径应填写 30 cm；若物理根还有缩放，也须计入实际测量。

用模型／Editor 测量核对尺寸，调整表现配置的 Relative Transform，让模型与原物理根对齐。不要为了视觉对齐改动原碰撞体质量、尺寸、重心、驾驶参数或控球点。相对模型缩放必须为正且均匀；非均匀缩放导致轮胎椭圆时应先处理素材／接入方式，不能靠一个半径值准确修复。

完成下面的实测表后交给接线成员：

| 轮位 | SteeringBone | SteeringAxis／Sign | SpinBone | SpinAxis／Sign | 实际 RadiusCm |
|---|---|---|---|---|---|
| FL | Wheel_FL_steer_jnt | 待测 | Wheel_FL_spin_jnt | 待测 | 待测 |
| FR | Wheel_FR_steer_jnt | 待测 | Wheel_FR_spin_jnt | 待测 | 待测 |
| BL | Wheel_BL_steer_jnt | 待测 | Wheel_BL_spin_jnt | 待测 | 待测 |
| BR | Wheel_BR_steer_jnt | 待测 | Wheel_BR_spin_jnt | 待测 | 待测 |

## 5. 创建 AnimBP

1. 在 Content Browser 创建 **Animation Blueprint**，选择本车型的实际 Skeleton。
2. Parent Class 选择原生 **HALVehicleAnimInstance**。若已用默认父类创建，在 Class Settings 中改为对应原生父类后重新编译。
3. 建议命名为 `ABP_<车型>_Presentation`。这应是有明确目标 Skeleton 的普通 AnimBP，不使用无 Skeleton 的 Template AnimBP。
4. 打开 Class Settings，确认父类与 Target Skeleton；不要选普通 Pawn Blueprint 或普通动画序列资产。
5. Event Graph 不需要 Cast、GetOwner、Tick 算法或输入读取；原生类会拉取表现快照。

AnimBP 创建时可指定 Skeleton 与 Parent Class；本项目的原生基类提供 `Presentation` 变量。[Epic Animation Blueprints](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprints-in-unreal-engine)

## 6. 连接 AnimGraph

### 6.1 首轮最小图

```text
Reference Pose
  → Local To Component
  → Modify Bone：Wheel_FL_steer_jnt
  → Modify Bone：Wheel_FL_spin_jnt
  → Modify Bone：Wheel_FR_steer_jnt
  → Modify Bone：Wheel_FR_spin_jnt
  → Modify Bone：Wheel_BL_spin_jnt
  → Modify Bone：Wheel_BR_spin_jnt
  → Component To Local
  → Output Pose
```

后轮 steer 可省略；若保留两个节点，接对应 BL／BR 的 SteeringRotation，默认会输出零。首轮不增加 suspension／Chassis 动画节点，因为本轮没有这些输出数据。

### 6.2 每个 Modify Bone 的设置

| 项目 | 设置 |
|---|---|
| Bone to Modify | 手工选择对应骨骼 |
| Translation Mode | Ignore |
| Rotation Mode | Add to Existing |
| Rotation Space | Bone Space，与本流程测量的骨骼局部轴一致 |
| Scale Mode | Ignore |
| Alpha | 1，C++ 无效帧本身输出中立旋转 |

节点姿态连接为组件空间，但 Rotation Space 可以采用骨骼局部空间；两者含义不同。不要省略 Local To Component／Component To Local，也不要给每个轮子重复做一组空间转换。[Epic Transform Bone](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-transform-bone-in-unreal-engine)

### 6.3 变量连接表

显示继承变量，拖入原生 **Presentation** 的 Get 节点，拆分结构体引脚，或使用对应 Break Struct 节点。将已有 FRotator 直接接到 Rotation 输入，无需自行乘倍率或组合轴。

| 骨骼节点 | Rotation 输入 |
|---|---|
| FL steer | `Presentation.FL.SteeringRotation` |
| FL spin | `Presentation.FL.SpinRotation` |
| FR steer | `Presentation.FR.SteeringRotation` |
| FR spin | `Presentation.FR.SpinRotation` |
| BL spin | `Presentation.BL.SpinRotation` |
| BR spin | `Presentation.BR.SpinRotation` |
| 可选 BL／BR steer | 对应 `Presentation.BL/BR.SteeringRotation` |

其他诊断字段有 `Presentation.bValid`、`SteeringDegrees` 和 `ForwardSpeedCmPerSecond`。Preview Scene 没有车辆表现组件时，快照无效／中立是正常现象；预览静止不能直接判定代码无效。骨骼常量轴测试与游戏运行数据测试分别进行。

编译并保存 AnimBP。Modify Bone 的目标骨骼不会随 Data Asset 中的 FName 自动重绑，表格两边必须一致。

## 7. 创建表现 Data Asset

1. 使用 Content Browser 的 Data Asset 创建入口，选择原生 **HALVehiclePresentationConfig**。
2. 建议命名 `DA_<车型>_Presentation`，先保持 **Ready For Use=false**。
3. `Mesh` 指向实际 Skeletal Mesh；`AnimClass` 指向第 5 节创建的 AnimBP，不能只选原生 HALVehicleAnimInstance 基类来代替 AnimGraph。
4. `RelativeTransform` 填模型相对物理根的平移／旋转和正的均匀缩放。
5. 核对 RootBone／ChassisBone，默认是 Root_jnt／Chassis_jnt。
6. Wheels 默认创建四项，显式 Wheel 身份为 FL／FR／BL／BR；按实测表填写半径、骨骼、轴和符号。数组顺序可变，但四种身份必须各出现一次。
7. 前轮 `bSteers=true`，后轮 `bSteers=false`。默认建议最大视觉转角 25°、响应时间常数 0.12 s；它们只改变显示，不改变车辆实际转向。
8. 检查各项有限值与正确单位，完成校准后才将 **Ready For Use=true**，保存并执行资产 Validate。

| 原生字段 | 含义／默认值 |
|---|---|
| RadiusCm | 各轮实测世界半径；默认 0，必须填写，最小 0.1 cm |
| SteeringAxis／SpinAxis | 骨骼局部 X／Y／Z；默认只是待校准的 Z／Y |
| SteeringSign／SpinSign | 必须为 +1 或 -1，不能用 0 关闭通道 |
| MaxSteeringDegrees | 最大视觉转角，默认 25°，允许 0～60° |
| SteeringResponseSeconds | 指数平滑时间常数，默认 0.12 s；0 为立即到目标 |
| RollingDeadZoneCmPerSecond | 轮心沿滚动方向低速死区，默认 1 cm/s |
| MaxSampleDeltaSeconds | 大于此间隔重置姿态，默认 0.1 s，不补算长停顿 |
| TeleportResetDistanceCm | 两次位置采样超过此距离重置，默认 1000 cm |

校验会检查轮位唯一、骨骼名称唯一、直接父子关系、半径／符号／数值范围、相对缩放、Mesh／Skeleton 与 AnimBP 的 Skeleton 精确一致。当前不隐式支持跨 Skeleton 兼容重定向。

原生基类在技术上可以被选中，用于原生数据链路验证，但它没有团队配置的 Modify Bone 图，正式接线必须选择实际 AnimBP。未 Ready 或配置无效时会输出 `skeletal presentation fallback` 警告并保留原视觉；它不会因此禁用驾驶。

## 8. 接入车辆 Definition

1. 打开实际玩家车辆 Blueprint，确认继承当前 `TestVehiclePawn`，且 Configuration Source 为已有的 Definition 模式。
2. 找到该车实际使用的 `VehicleDefinition` 资产。不要仅凭名称猜测，检查 Blueprint 与地图实例覆盖值。
3. 在 VehicleDefinition 的 Visual 分类，将新增 **Presentation** 引用设为刚创建的表现 Data Asset。
4. 保留原 `Visual` Static Mesh 配置，作为缺失／无效骨骼表现时的回退；不要清空原 Mesh 后再验收。
5. 保存 Definition，重新打开或刷新车辆预览，检查新增组件 `SkeletalVisual` 和 `VehiclePresentation`。
6. 正常情况下只显示骨骼模型，原 Static Mesh 被隐藏；物理根仍叫 `VehiclePhysicsBody`，球控制点仍在原位置。
7. 进入 Play，选择运行实例检查 `SkeletalVisual` 使用正确 Mesh／AnimClass、NoCollision、不模拟物理；不要手动开启 Physics Asset 模拟。

当前接入入口是 VehicleDefinition.Presentation。Legacy 模式继续使用原 Static Mesh；不通过在 SkeletalVisual 上临时设置 Mesh 来替代完整配置与校验，因为初始化会按 Definition 应用。

同一玩家车型在主机／客户端应拥有相同已保存配置与资产。不要只在主机临时指定模型后期待客户端自动收到未同步的本地资产配置。

## 9. 单机与双端基本验证

### 9.1 单机先验证

| 操作 | 预期 |
|---|---|
| 静止、松开所有输入 | 无轮转抖动，前轮回中 |
| 前进／倒车 | 四轮沿正确方向滚动，倒车反向；半径变化只影响显示转速 |
| 左右转向、快速反向 | 前轮平滑转向，后轮不转向；车体实际转向与之前一致 |
| 墙边持续踩油门 | 轮转跟随实际运动，而非按油门全速空转 |
| 原地／低速转动 | 各轮可因角速度产生不同的近似滚动，不代表实现了真实轮胎动力学 |
| 持球、发射、受撞脱球 | 球仍按实际物理车头／原控球点工作，没有视觉骨骼反向驱动 |
| 短暂停顿或瞬移 | 数据重置后恢复，未补算整段停更轮转 |

在 AnimBP Debug Filter 中选择实际车辆实例，看 `Presentation.bValid` 与各轮旋转。先确认变量变化，再查节点轴／空间，不要一开始就改驾驶代码。

### 9.2 主机／客户端互换

双端启动按 [现有操作说明](../Configuration/MVPB_DebugHUD_And_GAS_Test_CN_v0.1.md)执行。同机两个进程仍使用 `127.0.0.1`，其他电脑填写主机局域网 IPv4。

1. 主机开始比赛，主机驾驶，双方观察主机的转向和轮转。
2. 客户端驾驶，双方观察客户端的转向和轮转。
3. 分别松开转向，确认双方看到前轮回中。
4. 测试前进／倒车、漂移、持球／发射、加速开始／到期、退出／重新加入。
5. 记录哪端驾驶、哪端观察、使用资产／构建、网络条件及录屏／截图。各端装饰性轮角不要求逐帧同相位，但方向与转向意图应一致。

本轮服务器会读取物理实际消费的转向并以最多 15 Hz 更新一字节量化表现值；本地拥有车使用本地输入，远端观察者使用同步值。它不增加新的车辆控制 RPC，不逐帧复制骨骼。首次实际模型接线后的画面、LOD／裁剪和人工观感仍需由团队验证，不能被无模型的原生数据测试替代。

## 10. 常见问题

| 现象 | 排查顺序 |
|---|---|
| 看不到新增原生类／组件 | 确认拉取源码、完整 Editor 构建、重启 Editor；检查父类与加载日志 |
| 仍显示旧车 | 查 VehicleDefinition.Presentation 引用、Ready 状态和 fallback 警告；检查实际地图实例是否用了另一份 Definition |
| 两套车身同时显示 | 检查是否手动添加了额外 Mesh／ChildActor；正常接入只控制现有 VisualMesh 与新增 SkeletalVisual |
| 模型显示但轮子不动 | AnimClass 是否实际 AnimBP、父类是否正确、Graph 是否连接 Rotation、运行快照是否有效 |
| 轮子方向反了 | 修改对应 SpinSign／SteeringSign，检查所选局部轴；不要把物理转向输入反号 |
| 轮子绕车身公转／歪摆 | 检查骨骼轴心、父级、Bone to Modify 和 Rotation Space，回到单轴预览定位 |
| 平移／参考姿态错位 | Translation 设 Ignore，Rotation 设 Add to Existing，核对是否误用了 Replace Existing |
| 只有本地车有转向 | 检查两端是否同步同一 Definition／表现配置，组件是否实际启用，远端实例的数据与网络连接 |
| 转速明显不对 | 核对世界半径、单位、缩放和轮位；高速轮转视觉混叠也可能造成倒转错觉，不据此调整物理速度 |
| 骨骼被判不存在／层级错误 | 校验导入后的实际名字、导出器额外根、命名空间后缀及直接父子关系 |
| 远处轮子停止／模型消失 | 检查 Skeletal Mesh LOD 是否保留控制骨骼，包围盒和裁剪；当前首轮保持姿态持续更新 |
| 没有车身倾斜或悬挂 | 本轮尚未接 VP-2／VP-3，这不是遗漏的蓝图接线 |

## 11. 保存、交付与验收记录

交付 Mesh、Skeleton、材质／贴图依赖、AnimBP、表现 Data Asset、被修改的 VehicleDefinition 和必要 Blueprint／地图；正常提交到 Git 后再同步团队。不要提交 Saved／Binaries／Intermediate／DDC，也不要把骨骼截图当成完整模型交付。

本次接线验收记录建议填写：

```text
代码提交／UE补丁：
Mesh／Skeleton／AnimBP／Presentation DA／VehicleDefinition：
导入文件版本／单位／变换：
四轮轴向、符号、实测半径表：
单机结果：
主机驾驶、客户端观察结果：
客户端驾驶、主机观察结果：
回退／退出重进结果：
未通过项、日志／截图：
```

完整扰动、四台真实 PC 和与机关／技能的完整组合仍按 MVP-B 收尾统一矩阵执行。本流程要求的基本导入与双端验证应先完成，不将“最终测试后置”理解为跳过本模块接线检查。
