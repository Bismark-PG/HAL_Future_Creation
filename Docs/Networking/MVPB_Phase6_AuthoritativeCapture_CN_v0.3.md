# MVP-B 阶段 6：持球客户端物理复制修正（v0.3）

日期：2026-09-26。状态：**C++ 修正已写入；Editor/Game Target 编译通过，自动化 13/13 通过；团队已确认客户端球移动、无画面卡住、持球滚动及发射后运动正常。争球、脱球锁定、状态一致性及网络扰动复测尚未完整记录，阶段 6 暂不标记验收完成。** 本文接续 `MVPB_Phase6_AuthoritativeCapture_CN_v0.2.md` 的权威轨迹方案，并记录其首次实测失败和修正。

## 实测问题与证据

团队观察：主机持球时，主机球已吸向车头，客户端球留在原地；客户端持球时，主机能看到正常获取并移动，客户端会卡住约 1 秒，恢复后球仍未移动。

客户端 PIE 日志 `Saved/Logs/HAL_Future_Creation_2.log` 在进入 `Controlled` 时记录：

```text
Invalid Simulate Options: Body (BP_BasicBall.PhysicsRoot) is set to simulate physics but Collision Enabled is incompatible
Ensure condition failed: PhysicsRoot->IsSimulatingPhysics()
```

v0.2 把客户端持球球体设为 `Collision Enabled = NoCollision`，又希望它保持 Chaos 模拟以消费服务器刚体复制。UE 5.8 的 `UPrimitiveComponent::ShouldCreatePhysicsState()` 将 `NoCollision` 视为不需要创建物理状态；这使 `IsSimulatingPhysics()` 变为 false，PI 没有可驱动的球体。客户端卡顿与 Ensure 的错误报告采集发生在同一时间，日志显示该过程约 1.2 秒；这是卡顿的直接证据，不是网络延迟或约束调参问题。v0.2 的无输入 Join 烟测和 12/12 自动化没有触发捕球，因此未发现此错误。

## 修正与边界

客户端收到 `Controlled` 后保留球的 **Collision Enabled 原模式**，只把所有 Collision Response 设为 `Ignore`，并关闭客户端本地重力。这样 Chaos 刚体继续模拟和接受服务器运动复制，球也不会在客户端与预测车辆、墙体发生本地接触。退出 `Controlled` 时恢复该球在 BeginPlay 捕获的原始通道响应与重力状态；Collision Enabled 和 Object Type 全程不改。UE 在运行时把逐通道改动后的 Profile 名称标为 `Custom`，不影响恢复后的有效通道响应，也不修改蓝图资产。客户端不建立约束，不用控制点或固定 0.18 s 曲线移动球；服务器权威约束、碰撞、球权与命中逻辑未改。进入状态时保留可关闭的 `hal.BallNetLog 1` 诊断：`Sim=1`、`Collision` 保持物理兼容模式、`Gravity=0`。若模拟仍不可用，记录一次 Error 供诊断，不再用 Ensure 中断游戏画面；该情况仍是验收阻断项。

本次新增自动化断言：球在保持物理碰撞模式的同时把所有通道响应设为 Ignore 后，仍然 `IsSimulatingPhysics()`；另外模拟客户端直接执行 `Controlled → Free` 的 RepNotify，检查刚体模拟、碰撞模式、Object Type、通道响应和重力。这覆盖了上轮漏掉的状态切换，但仍不能代替真实两进程物理复制和 PI 可见效果。

## 运动规则复盘

v0.2 中的车辆、自由球、发射球、持球球体权威／预测分工表仍适用；其中 `Controlled` 行应以本版修正：客户端**仍保留 Chaos 物理状态，碰撞通道全部 Ignore**，服务器球位置/速度/旋转复制继续交由 PI；客户端仍没有权威球约束或命中裁决。两端不运行相同约束模拟，但吸入位置来源统一为服务器刚体运动。未来提高冲击力会放大本地车辆碰撞预测与服务器接触结果的差别；不得在客户端本地 Hit 回调重复施加权威伤害或击退。发射反作用力的帧对齐预测仍属于阶段 7。

## 团队复测

关闭旧 Editor 与游戏进程后，用新编译版本打开 `/Game/Maps/MVPB_NetTest`，双人 Listen Server，主机执行 `HALStartMatch`。保留现有球/车辆 DA 和蓝图引用；不需编辑或保存 `.uasset`。

1. 两端执行 `hal.BallNetLog 1`。先由主机持球，再由客户端持球。在**客户端窗口**确认球从原位置随服务器吸入轨迹移动，且不出现约 1 秒画面卡住；状态日志应显示客户端 `Controlled` 的 `Sim=1`、`Gravity=0`，不再出现 `Invalid Simulate Options` 或 Ensure。检查球滚动、调试球形跟随及持球车辆正常驾驶。
2. 捕球中转向、加速、立即发射或被撞脱球；确认 `Free`／`Launched` 后客户端恢复原有碰撞/重力，球不留在旧位置。重复主机和客户端捕球至少数轮。
3. 低延迟通过后，在约 100 ms 实测 RTT、1% 丢包、20 ms 抖动下再测。记录吸入可见性、PI 修正、车辆回拉以及碰撞后是否在 1 秒内收敛。两端测试完成后执行 `hal.BallNetLog 0`。

若仍不动，请保留两端日志与视频，并记录 `Controlled` 行的 `Sim`、`Collision`、`Gravity`；暂停阶段 6 后续验收，不通过调节球质量、约束力、车辆动力或网络 DA 掩盖问题。

## 验证记录

- `HAL_Future_CreationEditor Win64 Development` 与 `HAL_Future_Creation Win64 Development` 均编译成功。
- `HAL.FutureCreation` 自动化 **13/13** 通过，新测试 `HAL.FutureCreation.Ball.ControlledProxyPresentation` 成功；报告在 `Saved/Automation/MVPB_Phase6_AuthoritativeCapture_v03_Final/index.json`。
- 先前仅 12 项通过、以及 13 项新测试初版因夹具未运行 RepNotify 造成的失败，均不能作为本修正的通过记录。最终版本直接执行客户端状态切换，已核对测试确实覆盖对应路径。
- 本轮未手工编辑 `.uasset`、`.umap`。客户端可见吸入、滚动、卡顿消失与 RTT 行为尚未由自动化证明。
- 团队在后续双人测试中确认：客户端球开始移动、画面不再卡住、持球仍滚动、发射后运动正常。网络条件、两端状态序号、同时争球与脱球锁定的专项结果尚未提供，不能把这四项观察扩大解释为阶段 6 全部验收通过。
- 四台真实电脑、同一有线网络是 **MVP-B 末期最终验收环境**，不是阶段 6 的通过前置条件。阶段 6 仍按双人 Listen Server 和约 100 ms 扰动条件完成当前纵切验证；最终四机长时压力测试留给后续阶段。
