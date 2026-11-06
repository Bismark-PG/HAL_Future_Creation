# MVP-B 状态 FX：FX-3B 本车资源与生命周期

日期：2026-11-06。承接已确认的 [FX-3 分步方案](MVPB_Vehicle_Status_FX3_Plan_CN_v0.1.md)与 [FX-3A 模型](MVPB_Vehicle_Status_FX3A_Implementation_CN_v0.1.md)。

## 1. 本步范围

新增 `UHALVehicleStatusFXComponent`，由 `ATestVehiclePawn` C++ 基类创建 `StatusFXPresentation`。普通车型继承即可，不需要团队逐个 BP 添加组件或 Event Graph。

组件拥有本车 FX-3A 账本、尚未取得定义的需求、已准备的 MID／Niagara、FX-1 目标快照和借用式渲染请求。接 FX-2 的 GameInstance 异步缓存，不在提交状态时同步加载资源，不以表现 Ready 阻塞驾驶或比赛。

**FX-3B 只准备资源，不接管画面。** MID 不写入任何 Mesh 的 Overlay；Niagara 不注册、不激活、不采样。现有 `HALShockedPresentationComponent` 继续显示电击。本步不增加 ASC Tag 监听、调试命令或 RPC；这些观察／诊断入口属于 FX-3C。Overlay 唯一所有者、参数曲线求值、融合及真实表面采样属于 FX-4，电击迁移属于 FX-5。

## 2. C++ 使用合同

| 接口 | 合同 |
|---|---|
| `Bind(Provider, Cache, Vehicle, AvatarIdentity)` | 同一组引用重复绑定不重启。Provider 必须属于本车；更换身份／配置／缓存时清空旧来源和资源。专用服务器不创建表现运行绑定。 |
| `AcquireSource()` | 获得绑定代次内的独立来源租约；未绑定时返回无效值。不产生实例或资源。 |
| `Submit(Source, ExactVisualTag)` | 租约首次固定到一个 `State.Visual` 子 Tag。等待定义时保存第一次观察时间；重复提交不增加来源、不重启。不是 Gameplay Tag 或 GE 的写入口。 |
| `RemoveSource(Source)` | 永久撤销租约。最后来源退出按定义淡出／残留；未准备好的等待需求立即撤销，不补播过期尾迹。 |
| `CaptureRenderRequests(Out)` | 更新当前空间与生命周期后返回本帧借用请求。下游不可把旧变换当作新帧位置。 |
| `IsRequestCurrent(Request)` | 先推进过期／失效清理，再检查 Owner／Avatar、实例绑定与 ID、Mesh 代次和当前 FX-1 组件／资产身份。旧副本的强度／变换仍须重新获取。 |
| `Unbind()` | 撤销来源、移除两个事件订阅、销毁 Niagara、释放 MID 所有权、关闭 Tick；可重复调用。 |

`GetRuntimeSnapshots()` 是上次更新后的只读逻辑快照，后续诊断可使用；不以容器非空替代 GPU 可见或真实采样成功。

## 3. 资源所有权与渲染请求

- 共享 Catalog／Definition 保持只读，由 FX-2 缓存持有硬引用；每车只保存自身运行记录。
- MID 按本车目标 Id＋基础材质复用。两 Tag 引用同一基础材质时可以借用同一候选 MID，两辆车始终使用不同 MID。当前不会把各 Tag 的参数依次写入这个 MID；FX-4 必须先确定每个 Mesh 的优先级／组合结果，再输出一次。
- Niagara 按实例＋目标＋LayerId 持有，Outer 为本车 Actor。Bounds 层使用 BoundsSystem，MeshSurface 层按 FX-1 实际 Static／Skeletal 种类准备对应模板。选择模板不等于已经完成三角面采样。
- 请求包含实例阶段／强度／曲线时钟、只读 Recipe、FX-1 完整几何、Mesh 代次、候选 MID 和粒子层的弱引用。后续渲染器应重新取当帧请求，并在使用延迟工作结果前验证身份。
- 无效目标、Mesh 替换、实例结束或解绑时，粒子实际执行 `DestroyComponent()`。未赋给 Mesh 的 MID 释放最后拥有引用后由 GC 回收；不会保留在共享 DA 中。

本步逻辑快照中的 `bAvailable` 表示“定义和可用目标已准备”，不是“已经绘制／GPU 预热完成”。当前合法定义的对象分配正常成功是准备前提，渲染效果需后续分层验收。

## 4. 等待、时间与重绑

| 情况 | 处理 |
|---|---|
| Catalog 尚未 Ready／未配置／缺少精确 Tag | 仅保留需求，不创建伪资源；由缓存变化事件重新查询，无同步加载。 |
| 定义晚到且来源仍有效 | 以首次观察后的已过秒数播种新实例曲线时钟；持续段补入，不把时间重置为零。 |
| 定义晚到前来源已结束 | 撤销需求，之后的共享缓存事件不能复活它。 |
| 定义已 Ready，但暂无可用 Mesh | 保留 Waiting 实例与时钟；目标恢复后准备资源。等待期间来源全部退出则直接释放。 |
| FX-1 注册／移除部件或重新配置 | 事件立即淘汰旧资源代次，重取实际目标。逻辑实例和时钟保留。 |
| 直接修改 Mesh 资产／可见性而未通知 | 活跃更新和请求获取都会检查 FX-1 当前身份；借用请求验证亦会拒绝旧身份。 |
| 仅位置／旋转／缩放变化 | 更新几何，不重建 MID／粒子。 |
| Avatar 身份替换、弱身份失效、Owner 退出／组件注销 | 强制清理，撤销旧租约和请求，不把旧状态带入新车。 |

共享缓存异步完成后只广播可用性。组件事件使用弱 UObject 回调并捕获运行绑定代次，不捕获 Mesh；重绑后旧事件不拥有新车的状态。Mesh 另有资源代次，不能只靠 A 的实例句柄判断 Mesh 仍有效。

时间来自本地单调时钟，以本次绑定为零点，不受 World 时间倍率／回退影响。没有服务端事件时间的普通 Tag 仍无法保证两端完全同帧起播。

## 5. 车辆生命周期与更新

车辆 BeginPlay 绑定 Provider／GameInstance 缓存及当前 ASC 身份，ASC 初始化后再次幂等绑定。`UninitializeAbilitySystem` 同步解绑本组件；退场走已有统一退出链路。组件自身 EndPlay／OnUnregister 也执行清理，覆盖世界销毁与跨图。

空闲或只有尚无定义的需求时不 Tick。存在逻辑实例时启用原生 PostPhysics 更新，并依赖 VehiclePresentation 更新顺序，刷新几何、检查静默目标变更并推进过渡。最后尾迹结束后关闭。此处的更新只管理表现，不读写权威物理、HP 或网络输入。

## 6. 验证与证据

最终结果见 [FX-3B 验证保护记录](MVPB_Vehicle_Status_FX3B_Verification_20261106.json)。原始基线、构建日志和报告位于 `Saved/StatusFX/FX3B_20261106/`，不纳入 Git。

最终 R5 Editor／Game Target 编译成功，全量原生 **167/167** 通过，包含本步六项组件用例，0 测试警告／失败／未运行／进行中。构建仍有本机 MSVC 非首选版本和引擎弃用 API 提示，与自动化测试计数分开记录。

Host／Client 的 GF7 三回合回归各 1/1，通过回合切换、跨图与返房；Host 出站 80ms 注入，双端 NullRHI，使用原有瞬态测试 Profile。这验证原流程与组件装配的兼容性，不是新 ASC 状态观察／新 FX 绘制验收。1193 份保存资产逐份哈希未变；已有 315 输入仅修改车辆装配 cpp／h 及两处退出保护，其他 311 份保持，新增 3 份 C++，最终 318 输入冻结一致；Config／Build.cs／uproject 保持。未提交／Push。

新增组件自动化覆盖两车真实资源隔离、同 Tag 多来源复用、同基础材质候选复用、旧 Overlay 保持、迟到资源及撤销、Mesh 代次／空间更新、部件事件、淡出／残留、实际粒子销毁与 MID 垃圾回收、Avatar 替换／销毁、Actor 销毁、无缓存以及 Static／Skeletal 模板选择。延迟目录用内存记录模拟完成时点，FX-2 原有真实 StreamableManager 测试随全量回归执行；不把模拟完成写成新一次磁盘异步压力测试。

全量 R2／R3 在新增强制 GC 用例中先后暴露已有自救／冲击接收组件的退出边界：先前世界已退出，组件进入 EndPlay，直接使用空 World 清理 Timer。分别补充 `UVehicleRecoveryComponent::EndPlay`、`UHALImpactReceiverComponent::EndPlay` 的空 World 保护，后者同时保护 Owner 已失效时的根组件解绑。保留两次原日志与退出码 3，重新完整验证；不把崩溃记录写成通过，也不移除实际 GC 验证。

R4 进程退出码为 0，但报告为 166 通过／1 失败，不能算全绿。原因是新用例用 `RF_NoFlags` 强制回收了先前只读测试加载的独立地图资产，触发 WorldPartitionSubsystem 仍初始化的引擎 Ensure。改用引擎标准 `GARBAGE_COLLECTION_KEEPFLAGS`，尊重 Editor 的 `RF_Standalone` 资产生命周期；本车 MID 为非 Standalone 瞬态对象，仍通过真实 GC 验证弱引用失效。保留 R4 报告，后续以最终报告为准。

## 7. 团队配置与下一步

本步不要求更改现有车辆 BP、Presentation DA、状态 FX DA 或地图。重新构建后车辆会自动拥有该组件；当前没有新增可见电击变化属于预期，也不能以当前画面宣布 FX-4／FX-5 完成。

下一步 FX-3C：接入 ASC 视觉状态观察，提供受控调试来源与状态／来源／资源诊断，验证初始状态、重复／替换绑定和 Host／Client 的本地运行边界。仍保持旁路，不提前接管旧电击。
