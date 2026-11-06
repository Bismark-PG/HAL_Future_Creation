# MVP-B 状态 FX：FX-3C 观察入口与诊断

日期：2026-11-06。承接[已确认分步方案](MVPB_Vehicle_Status_FX3_Plan_CN_v0.1.md)及[FX-3B 本车资源](MVPB_Vehicle_Status_FX3B_Implementation_CN_v0.1.md)。

## 1. 本步行为与边界

车辆基类在现有 `RefreshStatusFXBinding()` 后调用 `ObserveAbilitySystem()`。运行组件读取当前 ASC 已有的 `State.Visual` 子 Tag，并监听后续变化。每个精确 Tag 只提供一个 ASC 来源；多个 GE／Tag 计数维持同一状态时，不据此创建多份实例或增加亮度、粒子、伤害。

ASC、命名调试来源、外部租约可以共同维持同一实例，分别撤销。共享 DA 保持只读，来源全部退出后仍走 B 的淡出／残留／释放。当前使用本地观察时间，不从 Tag 存在反推服务端 GE 身份、叠层或准确命中时间。

**FX-3C 继续准备资源和请求。** 新组件不写 Overlay、不注册或激活 Niagara，不接管原电击；已有 `HALShockedPresentationComponent` 继续播放。材质协调／真实粒子采样属于 FX-4，旧电击迁移属于 FX-5。本步未改 UI、输入键位、HP、物理规则或网络载荷，不需要修改 BP／DA／地图。

## 2. ASC 观察合同

`ObserveAbilitySystem(InASC)` 仅接受本次 B 绑定的 ASC 身份，且该 ASC 的 Avatar 必须为组件所属车辆。初次绑定先订阅再读取完整当前状态，覆盖 Tag 先到、PlayerState／Avatar 后就绪的情况。同一绑定重复调用不会增加订阅、来源或重启时钟。

监听使用 `State.Visual` 根 Tag 的 `AnyCountChange`，随后读取 `GetOwnedGameplayTags()` 的显式 Tag 集合，逐 Tag 比较。隐含父 Tag 不自动成为另一份效果；显式配置的父、子 Tag 若同时实际持有，则分别作为两个状态。只监听 NewOrRemoved 会漏掉“子 Tag 已保持计数为正时，父 Tag 单独成为显式状态”的情况。

回调弱引用本组件／预期 ASC，并捕获运行绑定代次。换 ASC／Avatar 通过 B 的 `Bind()` 清空旧代次、来源、订阅和资源，重新读取新绑定当前状态。旧回调无法退出新实例。ASC 销毁或转交给其他 Avatar 时，已有更新／请求验证／诊断及下一次来源操作会清理旧绑定。

### UE 5.8.3 通知缺口的补充

本机引擎 `AbilitySystemComponent.cpp::UpdateTagMapSingle_Internal` 的单个 loose tag 移除，仅在聚合计数出现显著变化时执行 deferred delegates。当子 Tag 仍使父层级计数为正，显式父 Tag 的单次移除可能漏发包括 AnyCountChange 在内的通知。

组件沿用 B 已有的活跃空间更新，在刷新、诊断查询和缓存 Ready 入口重核 ASC 快照，不新增空闲 Tick 或定时器。未配置／等待目录时不轮询；缓存就绪前必先重核并批量撤销过期来源，再准备资源，避免另一个已过期等待 Tag 被中途补成活跃再留下尾迹。未收到通知且没有更新／查询／Ready 事件时，纯等待记录可以暂时保留，但不会生成资源；下一次入口先纠正。未修改引擎或改变 GAS 的 Tag 计数。

## 3. 团队如何调试

重新构建并进入实际游戏窗口后，在该窗口控制台执行。Host 与 Client 各自操作本地世界；命令不会发送 RPC 或修改任何 ASC Tag／GE。

```text
hal.StatusFXStatus
hal.StatusFXStatus local
hal.StatusFXLog 1
hal.StatusFXDebug local State.Visual.Shocked A 1
hal.StatusFXDebug local State.Visual.Shocked B 1
hal.StatusFXStatus local
hal.StatusFXDebug local State.Visual.Shocked A 0
hal.StatusFXDebugClear local
hal.StatusFXLog 0
```

- `StatusFXStatus` 默认列出全部车辆，可用 `local` 或日志中的实际 `PlayerId` 筛选。PlayerId 不等同于画面上的 P1／P2 顺序。它同时显示共享缓存与每车目录状态。
- `StatusFXDebug` 参数依次为本地车辆／PlayerId、已注册视觉 Tag、来源名、0／1。相同名字重复 1 是刷新；相同 Tag 的 A／B 为两个独立来源。名字遵循 FName 语义、最多 64 字符，每车最多 16 个命名调试来源。换 Tag 前先关闭原来源。
- `StatusFXDebugClear` 默认清理本地车，可用 PlayerId 或 `all`；只撤销调试来源，保留真实 ASC 状态和外部来源。调试来源也会随换车／解绑／退出清理。
- `StatusFXLog` 默认 0，开启后只记录来源／绑定变化，不逐帧刷屏。显式 `StatusFXStatus` 无论该开关值如何都会打印快照。
- 状态查询／调试命令与日志开关在非 Shipping 构建提供。不要把它们作为正式玩法或 UI 接口。

此时应从诊断看到两来源共享一个实例；撤销 A 后 B 继续维持；清理后淡出再回到空闲。**当前没有新增可见 FX 属于预期**，不以调试命令替代真实电击的权威测试。要测试真实来源，可用已配置的电墙触发，观察 `Kind=ASC`。

## 4. 如何阅读日志

日志写入当前游戏进程的正常 log，常用前缀为 `LogHALStatusFX` 与 `LogHALStatusFXRuntime`。

| 字段 | 含义 |
|---|---|
| Bound／Observer／ASC／Avatar | B 运行绑定、有效 ASC 观察及当前对象身份 |
| Binding／MeshGen | 来源／实例所属代次，以及具体外观资源代次 |
| Leases | 已分配且尚未撤销的租约，包含尚未提交的外部租约 |
| SubmittedSources | 已有定义并提交进 A 模型的来源数，不是 GAS 层数，也不等于 Active 阶段数 |
| ASC／Debug | 各输入适配器维护的来源数；即使定义未到也可大于 0 |
| PendingRecipes | 保留需求但尚无可用精确效果定义的数量 |
| Instances／Targets／MID／Niagara | 逻辑实例、当前目标及本车实际持有的资源对象数量 |
| Registered／Active | 新路径粒子注册／激活数，FX-3 阶段应为 0 |
| Source Kind／Name／Age／RecipeSubmitted | 具体来源分类、稳定名字、本次观察经过时间及是否已提交到模型 |
| Instance Phase／Sources／Clock／Intensity／Recipe | 阶段、维持来源数、实例曲线时钟、包络强度与实际使用的定义路径 |

`Load=Ready` 仅表示共享资产已加载并验证。`PendingRecipes>0` 时，先检查目录是否未配置／Loading／Failed，以及已 Ready 目录是否缺少该精确 Tag；局部覆盖还须结合默认目录状态。`Phase=Waiting` 且已有定义时检查可用目标。`Rendering=PreparedDormant` 表示本步准备态，不证明 GPU 预热、材质融合或三角面采样成功。

## 5. 验证与保护

最终构建、原生与双端结果见[验证保护记录](MVPB_Vehicle_Status_FX3C_Verification_20261106.json)。原始材料位于 `Saved/StatusFX/FX3C_20261106/`，不纳入 Git。

最终 R4 Editor／Game 构建成功，原生 **172/172**，专项观察与 GF7 三回合的 Host／Client 分别各 **1/1**，五份最终报告均为 0 测试警告／失败／未运行／进行中，所有专用验证进程已退出。双端为本机 Listen Server、Host 出站 80ms 注入、NullRHI；不是校园四机、实际 GPU 绘制、Cook 或 Shipping 验收。构建中的 MSVC 非首选版本及引擎弃用提示与自动化计数分开。

开工干净 `main/94cad64`，包含团队提交的 B。1193 份保存资产逐份哈希全同；已有 318 输入修改运行组件 cpp／h、缓存诊断命令、车辆观察装配和复用的组件测试，共 5 份，其他 313 份保持；新增观察实现与专项联机测试 2 份 C++，最终 320 输入冻结一致。Config／Build.cs／uproject 保持，未编辑或保存 BP／DA／地图，未提交／Push。

新增五项原生用例覆盖已有状态绑定、计数刷新、精确父子 Tag、多来源、真实资源复用、重复／替换绑定、捕获的旧回调、弱 ASC 销毁、定义迟到／撤销、通知漏失后的 Ready 重核、未配置和调试边界。复用 B 的资源夹具，保持既有六项组件及 A 模型测试。

新增双端观察用例使用真实权威 Stun／Shocked GE，验证 Listen Server 与客户端／观察者都形成 ASC 来源；客户端控制台调试不改变服务端状态、HP 或物理版本；ASC 与两个调试来源合并，同车重绑后读取已有效复制状态，结束后释放本车资源并回到无 Tick。当前保存目录的效果为表面材质配方，双端日志实际为每车 MID=1、Niagara=0，因此双端实际验证 MID 隔离／来源／清理，不能写成实际 Niagara 网络画面验收；粒子对象的实例独立与真实销毁由原生瞬态夹具覆盖。另运行原 GF7 三回合流程，验证观察接入与回合／跨图／返房兼容。两端时钟不要求同帧开始。

R1 编译失败因新联机夹具以 `auto` 复制不可复制的 `FNativeGameplayTag`，改为 `FGameplayTag` 值。R2 Editor／Game 编译成功，但原生报告为 171 通过／1 失败：层级用例发现上述通知缺口，同时原断言误把仍淡出的父效果粒子算成重复生成；补快照批量协调，并改为比较增加来源前后的资源数量，保留父状态退出／源数量等断言。旧失败日志与报告独立保留，不以退出码 0 代替报告全绿。

R3 原生已 172/172 通过；专项联机首轮停在尚未选车的房间，在状态测试开始前中止了已核对命令行的两份专用进程，未生成完成报告。原因是复用了 GF7 启动器，但专项夹具尚未实现 GF7 的选车／确认流程。R4 补齐既有房间接口调用，并仅对生成的 GameMode 实例使用瞬态 Profile 副本（比赛 90 秒，结算 3 秒）；未改保存配置、CDO 或资产。中止运行、runner 退出码 1 与原日志独立保留；专项 R4 两角色退出码 0、各 1/1、0 测试警告／失败／未运行／进行中。

## 6. 下一环节

FX-3 完成后进入 FX-4：先明确每个 Mesh 的 Overlay 唯一所有者、优先级／有限组合、原材质恢复，再落实 Bounds 与 Static／Skeletal 粒子适配。FX-5 才迁移已调电击的提前反馈、权威接续和焦痕。真实画面、GPU 成本、四车性能、Cook／发布包与校园多机验收分开记录。
