# 车辆通用状态 FX：FX-1 统一外观目标与坐标入口 v0.1

日期：2026-11-06；开工基点 `main/1f046a8`，工作区干净；本机 UE5.8.2-56702186。

总体方案见 [六环节计划](MVPB_Vehicle_Status_FX_Discussion_CN_v0.1.md)。本文只交付 FX-1。当前电击仍走原表现代码，公共目录、运行实例、材质协调和电击迁移依次在 FX-2～FX-6 实施。

## 1. 本环节提供什么

复用 `UHALVehiclePresentationComponent`，给后续状态 FX 提供当前实际显示的外观 Mesh、资产身份、局部中心／尺寸、世界变换和物理根局部空间的整体包围体。没有新增组件、Tick、复制字段、RPC 或项目插件。

| 接口 | 用途 |
|---|---|
| `CaptureStatusFXTargets(Out)` | 游戏线程取得当前帧快照；骨骼外观成功启用时选骨骼，否则选已配置的静态 Fallback |
| `RegisterStatusFXPart(Id, Part)`／`UnregisterStatusFXPart(Id)` | 外观组装代码一次性注册／移除额外部件；不按效果反复配置 |
| `OnStatusFXBindingsChanged()` | 重配置、部件注册／移除、清理时发出本地失效通知 |
| `AreStatusFXBindingsCurrent(Snapshot)` | 核对绑定版本、组件／资产身份及当前可见目标集合；不表示变换仍是最新 |
| `ClearStatusFXBindings()` | 退场立即撤销绑定；重新 Configure 才建立新的外观集合 |

`Body` 是主外观的保留语义 ID，**不是物理碰撞体**。其他部件按语义 ID 字典序排列；不依赖 Actor 组件遍历首项。额外部件必须是同一车辆拥有的 StaticMesh／SkeletalMesh，不能重复注册主 Mesh、Fallback 或物理根；同 ID 同组件重复调用不产生新绑定，活跃 ID 不允许悄悄换成另一组件。

部件可在同车组装过程中先注册、再注册到 World；实际查询仅接纳已注册、可见且未 HiddenInGame、具有资产且不自行模拟物理的目标。隐藏 Actor、隐藏／销毁／卸载的 Mesh、无资产、非有限或零缩放变换均安全排除。隐藏物理根不会直接排除独立可见的外观子组件。主 Mesh 无效时不会任意搜索一个可见测试块补位。

配置重建会清空旧部件注册，组装代码须在 Configure 后重新注册新部件。绑定只使用弱引用，不延长旧外观生命周期。退场走 Pawn 原有 `RetireForVehicleTransition` 撤销入口，EndPlay 同样清理；不改变伤害、移动、碰撞或电击触发规则。

## 2. 坐标与使用约定

`FHALVehicleFXTarget` 保存 `LocalBounds`（含中心）、`MeshToWorld` 和此部件的 `VehicleBounds`；`FHALVehicleFXTargetSnapshot.VehicleToWorld` 是实际物理根的组件变换，整体 `VehicleBounds` 合并所有当前有效部件。

所有长度单位为 cm，Extent 为半尺寸。局部点转换必须按顺序执行：

```cpp
const FVector VehiclePoint = Target.MeshPointToVehicle(LocalPoint, Snapshot.VehicleToWorld);
// 等价于 VehicleToWorld.InverseTransformPosition(MeshToWorld.TransformPosition(LocalPoint))。
```

包围体将局部八个角逐点变换后重新求轴对齐盒，保留中心偏移、旋转、非均匀与镜像缩放。不要先把两个变换合成一个近似 RelativeTransform，也不要把 Extent 当成以零点为中心的真实形状。最终依据的是引擎报告的 Mesh Bounds，**不是骨骼三角形表面采样或权威碰撞形状**；真实贴表面的 Niagara 适配在 FX-4。

每次定位／更新效果前重新查询快照，不能跨帧缓存里面的变换。普通动画和车身回弹只更新几何，不反复重建材质／粒子。`AreStatusFXBindingsCurrent` 会发现同组件换资产、可见性改变和组件失效；这些直接组件变更不保证发出绑定委托，消费者还需在更新边界检查集合身份。FX-3 将据此处理实例重绑；目前没有持续运行的 FX 消费者。

## 3. 团队操作与检查

本环节不需要创建 DA、改蓝图 Event Graph 或逐辆配置特效资源。

1. 使用本次编译后的 Editor，保持 Live Coding 关闭。打开现有 `MVPB_NetTest`，或从 Entry 走房间／选车流程；开发配置可单人进入练习。
2. 在运行中的游戏窗口按 `~` 打开控制台，输入 `hal.VehicleFXTargets`。PIE 中应先点击目标游戏窗口，再输入命令。
3. 世界中显示青色框：每个当前有效 Mesh 的局部包围盒；黄色框：在车辆物理根空间合并的整体包围盒。框保留 10 秒，显示的是执行命令瞬间的快照。车辆移动后重新输入即可刷新。
4. 在 Output Log 搜索 `VehicleFXTargets`／`VehicleFXTarget`。检查 Owner、Role、Revision、Targets、Id、Kind、Component、Asset、CenterCm、ExtentCm 和 MeshToWorld。`ExtentCm` 是半长宽高。
5. 依次选择 Car_01、Car_02、Car_03 重复。当前标准车型应选中各自正在显示的 SkeletalVisual；物理碰撞代理、隐藏 Static Fallback、箭头或 UI 不应成为 Body。
6. 观察转向、回弹和骨骼表现后再次执行，确认框与当前外观位置一致。尺寸是 Bounds，车顶空隙或动画包围余量不能单凭此框判断为错误。
7. Listen Server 两个游戏窗口分别执行，按各自正在显示的外观检查；命令只查本进程，不发送服务器请求。检查结束无需保存地图或资产。

命令仅非 Shipping 构建可用，默认不持续运行。无有效外观时日志显示 Targets=0，不生成替代外观或修改物理配置。

若以后车型使用多个独立 Mesh，由 C++ 外观组装入口在 Configure 后为车身附件注册语义 ID；普通单 Mesh 车型自动接入。不要为每个效果在蓝图中重复添加注册逻辑。未来明确需要团队配置多部件时再扩充轻量配置入口，当前没有要求团队手写 Blueprint 核心逻辑。

## 4. 本次验证与限制

规格、C++与程序验证已完成；团队可见画面及真实四机检查尚未执行，不以原生测试替代视觉验收。完整哈希保护清单、构建日志／报告路径及报告摘要见[本次验证记录](MVPB_Vehicle_Status_FX1_Verification_20261106.json)。

- UE5.8.2 Editor／Game 两个 Target 首轮构建成功。Editor 编译输出包含原有 `Tests/HALCameraTests.cpp` 的 C4305 精度转换警告，本次没有修改该文件；构建成功不等于所有编译器警告为零。
- 新增 `HAL.FutureCreation.StatusFX` 四项原生测试 **4/4**；之后全量 `HAL.FutureCreation` **148/148**，两份报告的警告／失败／未运行／进行中均为 0，两次专用进程退出码均 0。
- 覆盖非零 Bounds 中心、旋转／镜像／非均匀缩放的解析坐标值、无效几何；静态／骨骼路径切换；部件确定排序／去重、隐藏／销毁／换资产／卸载；实时几何刷新而不改变物理；实际保存的 Car_01／Car_02／Car_03 初始化、退场及生产诊断命令。
- 开工前后 **1190 份保存资产逐份 SHA256 相同**。最终 **303 份 Source／Config／uproject 输入**与构建前冻结版本相同；Config、Build.cs、uproject 未修改。没有编辑 BP／DA／地图、提交或 Push。
- 测试使用 NullRHI，仅证明数据选择／空间换算／生命周期和既有原生回归，不能证明骨骼表面粒子贴合、现有电击已迁移或屏幕效果正确。本环节没有修改网络协议／物理，也没有新增实际 Listen Server 或校园四机测试；第3节画面检查和原有MVP-B团队欠项保持。

## 5. 下一环节

FX-2：共用效果定义、Tag 目录、项目默认引用与校验／预加载／Cook 引用。FX-1 当前只建立可复用外观入口，不能据此宣称电击已完成公共 DA 迁移或支持多状态材质共存。
