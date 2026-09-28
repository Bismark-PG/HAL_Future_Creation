# MVP-B Listen Server 阶段 8：退出清理与故障测试（v0.1）

日期：2026-09-28。状态：C++ 清理路径及自动化验证已完成；**真人双窗口退出、故障条件与两人连续运行尚待团队验收，阶段 8 未完成。** 依据：`Docs/MVP_B_Listen_Server_Plan_CN_v0.2.md` 第 14、17、19 节。阶段 7 的双人发射补测由团队报告暂未见问题，记录于 `MVPB_Listen_Phase7_Implementation_CN_v0.1.md`。

## 施工边界与结果

- GameMode 在 `Logout` 时先让车辆控球组件解除真实约束并清除 `HeldBall`，然后逐颗检查球快照，最后才 `UnPossess` 和销毁 Pawn。组件 `EndPlay` 仍提供非正常销毁的同一清理后备路径。整个流程只在服务器改写玩法状态，不依赖蓝图 Event Graph。
- 若离场车辆持球，球变为 `Free`，Holder 与离场车辆重捕获锁清空，使用球已有的 `PostLaunchPickupLockDuration` 配置提供短暂全局拾取保护；没有引入新默认数值。若球已经 `Launched`，只清除即将失效的发射者 Pawn 指针，保留 `Launched`、原速度和本局稳定 `LastLauncherPlayerId`，直至这次发射自然结束。任一种快照变化只增加一次球状态序号；重复清理不会再增加。
- 球命中上下文携带稳定来源 `PlayerId` 和每颗球单调递增的服务器命中事件序号，供未来 GAS 归属与事件去重使用。现阶段伤害仍由原 `CombatResolver` 与 Health 路径结算；一次成功命中后球从 `Launched` 回 `Free`，重复物理回调不能在同一次发射中再次伤害。没有接入 GAS、分数或新比赛框架。
- `hal.BallNetLog 1` 可看到退出清理后的球 State、Holder、Launcher、PlayerId、Sequence，以及成功命中的 `EventSeq`；退出清理本身另记录一行 `Ball vehicle-exit cleanup`。日志在 Shipping 不按帧刷屏，测试结束把开关恢复为 `0`。
- 本版不需要编辑或保存蓝图、地图、Data Asset。玩家为阶段 7 测试调整的车辆后坐参数保持原状。四台真实有线电脑压力测试仍属于后续阶段 9／MVP-B 末期验收。

## 已完成验证

- `HAL_Future_CreationEditor Win64 Development` 与 `HAL_Future_Creation Win64 Development` 均编译成功。
- `HAL.FutureCreation` 自动化 **14/14** 通过，报告：`Saved/Automation/MVPB_Phase8_Initial/index.json`。新增 `HAL.FutureCreation.Ball.VehicleExitCleanup` 验证持球离场转 `Free`、无离场 Holder／锁指针、重复清理幂等，以及发射后离场保持 `Launched` 和稳定 PlayerId。
- 无输入双进程连接／客户端退出烟测日志：`Saved/Logs/MVPB_Phase8_DisconnectHost.log`、`Saved/Logs/MVPB_Phase8_DisconnectClient.log`。本项只覆盖连接和退出路径，不会自动形成持球、发射或命中。

## 团队在 Unreal Editor 的验收步骤

1. 保存需要保留的工作，关闭旧 Editor 与游戏进程后，以新编译的 C++ 重开项目。加载 `/Game/Maps/MVPB_NetTest`，沿用双进程 Listen Server 和已有网络 DA；关闭 **Use Less CPU when in Background**。主机用 `HALMatchStatus` 确认两名玩家，再执行 `HALStartMatch`。两端执行 `hal.BallNetLog 1`，可同时用 `hal.VehicleNetLog 1` 查看发射请求，结束后均设回 `0`。
2. **持球退出**：让客户端车辆吸到球，先在两端确认 `Controlled` 与同一 Holder，再关闭客户端游戏窗口（或在客户端控制台输入 `quit`）。主机应记录 `Ball vehicle-exit cleanup`，球快照变 `Free`、Holder 空、锁定车辆不再指向已退出 Pawn；主机车辆稍后可正常拾取、驾驶并发射该球。不得留下球固定在离场车辆位置或不可再争夺。
3. **发射后退出**：重新开一局，让客户端持球发射，在球仍为 `Launched` 时退出客户端。主机应看到 `Launched` 继续运动，Launcher Pawn 指针清空而 `PlayerId` 保留；随后球碰到主机车辆时仍可权威命中且同一发射最多结算一次伤害，或按既有低速规则自然回 `Free`。录下发射前后的球状态序号与命中 `EventSeq`。
4. **主机退出**：另开一局，主机在 `Playing` 时关闭主机窗口；客户端应与该 Listen Server 断开，本局结束。本阶段没有主机迁移或断线重连，客户端的连接错误 UI 属于阶段 9。
5. **故障与长时测试**：低延迟通过后，沿用既有网络模拟配置，依次覆盖约 `50 ms RTT`、`100 ms RTT + 1% loss + 20 ms jitter`，并在 `150 ms RTT + 3% loss + 50 ms jitter` 下做压力观察。重复争球、发射、快速连按、硬碰撞、持球退出、发射后退出；检查无永久球权分叉、重复发射／后坐／伤害，扰动结束后 State、Holder、Launcher 与服务器收敛。两人至少连续运行 `10 分钟`，记录是否还能正常驾驶、争球、发射及两端日志。若异常，请按球 `StateSequence`、发射 `LaunchSequence`、命中 `EventSeq` 对齐两端日志，不先调整物理 DA 掩盖故障。

上述真人可见行为和扰动指标未由无输入烟测或自动化单元测试证明；团队结果返回前不能把阶段 8 标记为验收通过。
