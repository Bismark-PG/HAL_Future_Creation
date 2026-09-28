# MVP-B Listen Server 阶段 7：发射预测与反作用力（v0.1）

日期：2026-09-28。状态：C++ 首轮施工、Editor/Game Target、既有自动化测试及无输入双进程联网加载通过。**团队随后报告：主机和客户端正常发射后坐、快速连按、未持球拒绝回滚及约 100 ms RTT／1% 丢包／20 ms 抖动补测均暂未见问题；据此阶段 7 的双人功能验收通过。** 这是团队可见行为反馈，未附两端日志与量化修正数据；MVP-B 最终四机验收不在此处提前宣称完成。依据为 `Docs/MVP_B_Listen_Server_Plan_CN_v0.2.md` 第 7.2、12、19 节。阶段 6 规格中的同时争球、脱球锁定、退出和扰动专项记录仍应按其各自验收范围补齐。

## 规则与代码边界

- 发射仍是一次按键边沿，不进入连续驾驶输入历史。`LaunchSequence` 是 Pawn 内单调序号；服务器只处理新序号。客户端同时提交已观察到的球 `StateSequence`、本地物理帧和估算的服务器物理帧。服务器用当前真实 `HeldBall`、Holder 和球序号决定是否允许发射；客户端位置、方向、力度和声称的命中结果均不能授权发射。球权快照仍是唯一玩法状态来源，不在车辆上复制第二份持球指针。
- `UBallControlComponent::CalculateRecoilDeltaVelocity` 复用既有 DA 调好的 `BaseRecoilDeltaSpeed`、`MovingRecoilFraction`、`MaxRecoilDeltaSpeed`，客户端本地预测与服务器接受时采用同一计算。发射成功时，服务器立即提交球的权威状态和初速度；反作用力交给车辆 Network Physics Action 在物理线程执行。没有 Network Physics 历史的单机路径继续使用原来的下一物理步队列。不修改任何车辆／球的 Data Asset 数值。
- 持球客户端按键时，仅在已收到**唯一且有效**的 `Controlled + Holder=this` 快照、并已取得 Network Physics 物理帧偏移时，向自己的车辆历史排入一次预测反作用力。服务器收到 RPC 后排入一次权威 Action；即使拒绝，也排入零反作用力 Action，并向拥有客户端发回接受／拒绝回执。客户端的 Action 使用 `PredictedAutonomousOnly`：其预测冲量不会作为服务器指令上报。远端车辆不执行此本地动作，继续显示服务器物理复制。
- 客户端上报的物理帧只用于诊断时序，不用于要求服务器回滚过去的物理帧，也不替代球权验证。服务器在当前可执行帧施加权威反作用力；网络延迟使客户端预测帧与服务器执行帧不同，差异由 Network Physics 权威状态及重模拟消化。这一点需要实际测试确认观感，不能仅由编译证明。`hal.VehicleNetLog 1` 可查看请求、球序号、帧偏差、接受／拒绝及客户端回执；结束后设回 `0`。
- 目前没有客户端权威球轨迹或另一个本地假球。真实球发射和命中仍等服务器确认，避免复制出两套可碰撞球。现有输入触发的即时车辆反作用力是本阶段的首个本地反馈；额外的非权威 VFX／音效接线须经团队实际手感测试后决定。

## Unreal Editor 接线

本版**不需要编辑**地图、蓝图或 Data Asset。保存当前需要保留的资产，关闭旧 Editor 和游戏窗口，再以本版 C++ 重新启动。继续使用 `/Game/Maps/MVPB_NetTest`、双进程 Listen Server、已接好的车辆和球 Network Physics DA。测试时保持 **Use Less CPU when in Background** 关闭。

## 双窗口验收步骤

1. 主机与客户端进入地图，主机 `HALMatchStatus` 确认两人后执行 `HALStartMatch`。两端执行 `hal.VehicleNetLog 1`；先在低延迟下让主机持球并发射，再让客户端持球并发射。每次只按一次，确认球状态仅 `Controlled → Launched` 一次、车辆反作用力仅一次、发射后能继续驾驶。客户端按键时应能立即感到自身后坐，主机窗口随后看到同一辆车的权威反作用力。
2. 连续快速按两次发射键，及在未持球时按键。服务器对该球只能接受一次发射；拒绝记录出现时客户端不应永久保留多余后坐或出现持续回拉。观察球快照最后一致、没有第二次球发射或二次冲量。丢球锁定期间尝试发射亦应拒绝。
3. 在约 **100 ms 实测 RTT、1% 丢包、20 ms 抖动** 下重复前两项，特别记录客户端按键到后坐的主观延迟、服务器确认后是否出现明显双重后坐、一次超过 1 秒的持续回拉或球状态永久分叉。保持两端日志与一段同步视频。若失败，先提供对应 `LaunchSequence` 的两端日志，不修改 DA 动力或球质量来掩盖帧时序问题。
4. 结束后两端执行 `hal.VehicleNetLog 0`。要进一步诊断 UE 的物理动作匹配，可临时在两端执行 `np2.Resim.NetworkedActions.EnableDebugLogs 1`，记录后设回 `0`；该开关可能产生较多日志。

## 已验证与未关闭

- `HAL_Future_CreationEditor Win64 Development` 与 `HAL_Future_Creation Win64 Development` 均通过；最终代码的 `HAL.FutureCreation` 现有 13 项自动化全部通过，报告：`Saved/Automation/MVPB_Phase7_Final/index.json`。这些测试没有模拟真人按键或拒绝后的动态回滚。
- 隐藏双进程无输入烟测中，客户端进入 `/Game/Maps/MVPB_NetTest`，服务器记录 Join succeeded，客户端两辆车均启用 Network Physics 历史；日志：`Saved/Logs/MVPB_Phase7_SmokeHost.log` 与 `MVPB_Phase7_SmokeClient.log`。这不能证明发射预测体验。
- 最终展示的四台真实有线电脑仍属 MVP-B 末期阶段 9 验收；阶段 7 先完成双人低延迟与模拟扰动实测。
- 未来“由球／机关／技能指定冲击档位”须在 Combat Resolver 中统一命中资格、档位来源和目标反应，并将当前独立的数值脱球阈值迁入同一权威结算。此设计属于后续冲击／GAS 施工，本阶段不改动伤害或脱球判定。
