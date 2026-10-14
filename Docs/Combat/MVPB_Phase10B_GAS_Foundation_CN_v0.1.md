# MVP-B 阶段 10-B：GAS 基础与唯一 HP 来源（v0.1）

日期：2026-10-13（Asia/Tokyo）。状态：**C++、两个 Target 编译和 16/16 自动化通过；团队已完成 Test 的 PlayerStateClass 接线，本地双向命中 HP 变化及两端复制经日志复核通过。单机玩法／物理完整回归及网络模拟仍待确认，10-B 尚未最终验收。** 原施工基点 `main / fa883fe5fc76c2bd429d9d6c54af8bf878d5c845`；本次日志复核时当前提交为 `61508961a11baef8a96376a8786561b4aee00f4c`，实际使用 UE 5.8.2。设计依据见 [10-A 规格](MVPB_Phase10A_GAS_Impact_Spec_CN_v0.1.md)，最新状态入口为 [Project_Status_CN.md](../Project_Status_CN.md)。

## 1. 当前行为和职责

- `.uproject` 已启用内置 `GameplayAbilities`，`Build.cs` 增加 `GameplayAbilities / GameplayTags / GameplayTasks`。组员 Pull 后重新编译即可，不需下载第三方 GAS 插件；本次没有修改引擎文件。
- `AHALPlayerState` 是玩家 ASC Owner，持有 `UHALAbilitySystemComponent` 与 `UHALHealthAttributeSet`。当前 `ATestVehiclePawn` 是 Avatar，使用 Mixed Replication。Pawn 没有第二个 ASC。
- `APassiveTestVehicle` 自身持有同样的 ASC／Health AttributeSet，Owner=Avatar=该 Actor，使用 Minimal Replication；HP Attribute 仍复制给客户端。被动目标与玩家共享 HP 存储和扣血规则。
- `UHALHealthAttributeSet` 的 `CurrentHP / MaxHP` 是唯一运行时 HP。`UVehicleHealthComponent` 保留配置种子 `MaxHP`、`GetCurrentHP / GetMaxHP / OnHealthChanged` 接口，但不再保存或复制 `CurrentHP`。现有 UI 可以继续读相同接口。
- 服务器仅在每个 ASC 第一次完成有效绑定时，用当前车辆已保存配置初始化 HP。Attribute 初值为 0、未初始化表示尚未 Ready；客户端等待复制，不自行填一个默认 100 HP。重复 Possess、BeginPlay 补绑定、PawnClientRestart 或 Avatar 更换不自动回血，不重复初始化。未来重生／换车的数值重置须有明确规则，当前没有实现重生。
- `PossessedBy / OnRep_PlayerState / PawnClientRestart / BeginPlay` 负责玩家绑定；`UnPossessed / EndPlay` 解除旧 Health 订阅并清理当前 Avatar。旧 Pawn 延迟清理不能清除已更换的新 Avatar。清理 Avatar 保留 PlayerState 的 HP／状态；PlayerState 自身销毁时结束其生命周期。

## 2. 这一步如何接现有球命中

```text
服务器球碰撞
→ 原 FVehicleHitContext / CombatResolver
→ Health 的窄结算适配器
→ UHALApplyDamageEffect（Instant，Data.Combat.Damage 为负 HP 变化）
→ GAS CurrentHP
→ Attribute 复制 / Health 通知 / 原档位物理结果
```

`UHALApplyDamageEffect` 是 C++ 原生的通用即时 HP 结果，不需要团队新建 GE 蓝图。现有普通球仍从原 Ball DA 取固定伤害 `25`；适配器保留命中来源 Actor／Instigator 的 Effect Context，用 SetByCaller 将该次伤害交给 GAS。HP 限制在 `[0, MaxHP]`，后续合法降低 MaxHP 时也会收缩当前 HP；非法非有限伤害不能进入结算。

本次**没有**实施车辆基础攻击×动作倍率、飞行球攻击快照、显式冲击档位、最低脱球档位、主动 GA 或运动 GE。旧 `CombatResolver` 仍以扣血成功作为整次球命中通过条件；目标 HP=0／0 伤害时跳过额外冲击与命中消费的已知限制还在，按规格留给 10-C／10-D。不能将“GAS 已接入”误解为这些规则已修复。

`DefaultGame.ini` 新增 `GameplayCueNotifyPaths=/Game/GAS/Cues`，避免引擎默认扫描整个 `/Game`；目前没有新增 Cue 资产，未来表现资产使用该目录。本次未改已有 Combat 配置、物理／输入参数或球状态／Network Physics 协议。

## 3. 只读检查结果和必须的 Editor 接线

保存绑定检查见 [EditorBindings JSON](MVPB_Phase10B_EditorBindings_20261013.json)：319 个项目资产在检查前后哈希一致，未调用资产 Compile／Save。

团队随后在 `6150896` 提交中修改 Test 的 GameMode 蓝图；本次再次只读加载，确认 PlayerStateClass 已为 HALPlayerState，见 [团队接线后复核](MVPB_Phase10B_EditorBindings_PostTeam_20261013.json)。该次检查的 319 个资产也前后未变；不回写初次检查证据。

| 地图 | 当前 GameMode | 当前 PlayerStateClass | 处理 |
|---|---|---|---|
| `/Game/Maps/MVPB_NetTest` | `BP_MVPB_ListenGameMode` | `HALPlayerState` | 已符合玩家 ASC 归属，无需改联机 GameMode |
| `/Game/Maps/Test` | `BP_MVPA_GameMode` | 初次为引擎 `PlayerState`；团队接线后已为 `HALPlayerState` | 接线已确认，单机玩家／被动车 HP 回归待确认 |

两个 GameMode 的 Default Pawn 都是已调好的 `BP_RoundedVehiclePawn`，Source=Definition，Definition 是当前玩家 DA。Test 中仍有一个被动测试车；NetTest 已没有被动车实例。代理未直接修改这些资产。

### Test 地图操作步骤（尚未接线的旧工程使用）

当前分支的保存接线已正确，不需要重复改资产；以下保留给仍使用旧 GameMode 的团队副本。保存接线正确不等于单机玩法已验收。

1. 保存需要保留的资产，结束 PIE／Standalone，关闭旧 Editor；禁用 Live Coding，用本次 C++ 编译后的 Editor 打开工程。新增 native 组件／反射类型应完整重编，不靠旧 Live Coding 二进制接续。
2. Content Browser 打开 `/Game/Blueprints/Gamemode/BP_MVPA_GameMode`。进入 **Class Defaults**，在 **Classes / GameMode Classes → Player State Class** 选择 C++ **HALPlayerState**。若只见数据蓝图视图，可先点 **Open Full Blueprint Editor**；搜索 `Player State Class`。
3. **Default Pawn Class** 继续使用原来的 `BP_RoundedVehiclePawn`；本次只改 Player State Class。点 **Compile → Save**。
4. 打开 `/Game/Maps/Test`，在 **World Settings → GameMode Override** 核对仍是该 `BP_MVPA_GameMode`。如已有正确引用无需再保存地图；不要为了接 GAS 清空原场景或重调 DA。
5. 以 **Number of Players=1、Play Standalone / Standalone Game** 做单机回归。玩家和被动车都应有正确 HP，球命中每次只减少原来配置的伤害。单机原 PlayerController 可能没有 `HALGASStatus` Exec；可用 `showdebug abilitysystem` 查看玩家 GAS Attribute，或用已存在的 Health 读取／通知显示。

缺少 `HALPlayerState` 时，玩家车辆会输出 `vehicle GAS needs HALPlayerState`，没有运行时本地 HP 替代来源。该情况不代表车辆 DA 的 MaxHP 变成 0；它表示 ASC 未绑定。不要通过另写本地扣血或给 Pawn 加第二个 ASC 修补。

## 4. 双进程 Listen Server 团队验收

详细 PowerShell 双端启动、Host／Join／Start、按 PlayerId 比较 `100→75`、独立日志路径和新增只读 HUD 的开关，见 [调试 HUD 与 GAS 双端检查](../Configuration/MVPB_DebugHUD_And_GAS_Test_CN_v0.1.md)。HUD 是本地显示工具，不改变本节验收门槛。

1. 使用原 `/Game/Maps/MVPB_NetTest` 双进程 Listen Server，或从 Entry 跑原 Host／Join 流程。保持 **Use Less CPU when in Background** 关闭。主机／客户端各执行 `HALMatchStatus` 和 **`HALGASStatus`**。
2. `HALGASStatus` 会在本机日志／屏幕列出每辆车：Vehicle、ASC Owner、Avatar、Authority、Ready、HP、MaxHP、复制模式。稳定加载后两端都应看到 `Owner=HALPlayerState…`、正确 Avatar、`Ready=1`、`HP=100 MaxHP=100 Mode=Mixed`（当前 DA）。Actor 本地名称可能不同，玩家归属结合 `HALMatchStatus` 的 PlayerId 核对；不是要求两个进程的 Actor 序号完全一致。
3. 两端执行 `hal.GASLog 1`，主机执行 `hal.BallNetLog 1`。主机开始后，分别让主机击中客户端和客户端击中主机；每次单发命中后执行 `HALGASStatus`，目标应从 `100→75→50…`，另一车辆 HP 不变，主机和客户端最终值一致，不能双扣或重新初始化回血。
4. 同时回归驾驶、手刹、控球、发射后坐、三档既有击退与墙角接触。重复 `PawnClientRestart` 的初始化安全性已由自动化覆盖；真人测试重点确认正常加入和球命中没有破坏输入或物理观感。
5. 在约 **100 ms 实测 RTT、1% 丢包、20 ms 抖动** 下重复两方向命中与持球。初始 Ready／HP 需要复制时间；稳定后不得持续 HP 分歧。确认等待期客户端退出时服务器清理对应 Pawn／Avatar，已发射球仍符合阶段 8 的退出规则。开局后拒绝加入规则保持原样。
6. 结束设置 `hal.GASLog 0`、`hal.BallNetLog 0`，保存双方日志、测试提交／引擎版本及发现的问题；通过后更新状态入口和本记录，才能标记 10-B 的团队验收完成。

## 5. 已执行验证及边界

- UE 5.8.2：`HAL_Future_CreationEditor Win64 Development`、`HAL_Future_Creation Win64 Development` 均编译成功。未验证 5.8.3。
- `HAL.FutureCreation` 自动化 **16/16 成功**。既有 14 项继续通过，新增 `GAS.Health.BoundsAndIsolation`、`GAS.Health.PlayerStateAvatarLifecycle` 验证唯一 HP 存储、配置初值、扣血和钳制、实例隔离、重复绑定不回血、旧 Avatar 清理保护及 UnPossess 保留 Owner 数值。最终报告原位于 `Saved/Automation/MVPB_Phase10B_Final/index.json`。
- 首轮自动化暴露被动目标测试世界的初始化顺序缺口，修正为 Actor 在组件 BeginPlay 前设置 Owner/Avatar；本节记录的是修正后 16/16 的结果。
- 只读 Unreal 检查确认现有两个 GameMode 和 Pawn／DA 引用，319 个资产前后未变；没有代替团队执行 Test 蓝图的 PlayerStateClass 修改。
- 隐藏 NullRHI 双进程 smoke：主机监听 UDP 7777、客户端 Join succeeded；主机两车均初始化 Owner=HALPlayerState，客户端收到两车 `Authority=0, HP=100, MaxHP=100`，主机本车 `Ready=1, Mode=Mixed`。本次无真人按键、无动态球命中、无模拟延迟，不能用它证明网络扣血后的实时观感或物理回归。测试进程已清理。
- 详细摘要／文件哈希见 [10-B Verification JSON](MVPB_Phase10B_Verification_20261013.json)。本次只修改源码、插件／Cue 配置和留档；车辆／球／DA／地图资产保持原样。`Saved/` 报告和日志不入 Git，接收设备应使用该摘要并重新执行必要验证。

### 5.1 团队双向 HP 实测日志复核

2026-10-13 用户报告主机／客户端攻击均看到 HP 降至 75。本次只读复核 `Saved/Logs/MVPB_GAS_Host.log` 和 `MVPB_GAS_Client.log`，不重新运行真人命中，也不修改 C++ 或资产。完整行号、日志哈希和通知记录见 [ManualHP LogReview JSON](MVPB_Phase10B_ManualHP_LogReview_20261013.json)。

| 服务器日志 UTC 时间 | 目标（本次 ASC Owner） | 主机记录 | 客户端记录 |
|---|---|---|---|
| 08:09:05.521 | 客户端车／HALPlayerState_1 | 100→75 | 08:09:05.529，同样 100→75 |
| 08:09:52.470 | 主机车／HALPlayerState_0 | 100→75 | 08:09:52.478，同样 100→75 |
| 08:09:57.043 | 主机车／HALPlayerState_0 | 75→50 | 08:09:57.050，同样 75→50 |

两端均有三次实际 HP 减少，每次为 25；最后记录主机车 50、客户端车 75，MaxHP 均 100。第三次发生在主机车首次扣血约 4.6 秒后，不能把它和同一时刻的重复通知混为一谈。主机额外的 `75→75`／`50→50` 来自 `PostGameplayEffectExecute` 将钳制结果经 `SetCurrentHP` 再设置，触发相同数值通知；没有再次扣血。Health 通知订阅者仍会收到这次无变化通知，后续受击演出不能按通知次数判定攻击次数。

本次未找到 GAS 缺 Owner／Avatar、Ensure、Assert 或 Fatal；但日志没有 `HALGASStatus` 快照，也没有球的 `EventSeq` 命中日志，不能据此逐发审计碰撞回调／冲击档位。7～8 ms 是同机两份日志的时间差，不能当作实测 RTT。该次没有记录约 100 ms RTT／丢包／抖动模拟，因此仅标记 **本地双向扣血与属性复制子项通过**。

单独记录未修问题：Entry 菜单 `InputMode:UIOnly` 尝试聚焦不可聚焦的 Widget，两端首次菜单及客户端返回菜单时报错；与上述 GAS 扣血无关，应后续调整菜单焦点配置／目标。客户端 08:10:15 的 NetworkFailure 紧随主机关闭游戏窗口，随后返回 Entry，符合主机退出流程，不是命中时断线。其余启动阶段分析工具 DLL／Editor UI 注册／渲染警告不作为 GAS 失败证据。

## 6. 后续入口

团队完成上述接线／验收后，10-C 再处理标准命中请求／结果、攻击来源快照和伤害公式；10-D 再处理伤害与冲击完全独立、HP=0 仍受有效冲击以及最低脱球档位。10-B 不扩大到完整技能库、重生、淘汰或比赛框架。
