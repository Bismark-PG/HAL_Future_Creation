# MVP-B 阶段 6：客户端捕球吸入表现（v0.1）

日期：2026-09-26。状态：**历史方案，已被 `MVPB_Phase6_AuthoritativeCapture_CN_v0.2.md` 取代；不要按本文调参或验收。** 本文保留当时的设计和验证记录，说明为何不再采用客户端固定时长插值。

后续复盘指出：客户端固定 0.18 s 曲线虽能遮住瞬移，却与服务器 Chaos 约束产生不同的吸入轨迹。正式施工已删除这一参数和插值实现，改为持球期间沿用服务器物理 Transform 复制与客户端 PI。以下内容仅供变更追溯。

## 现象与设计边界

服务器以物理约束把刚捕获的球吸向车辆控制点；客户端在收到 `Controlled` 后原先立即将无碰撞球根组件放到控制点，因此在客户端窗口表现为瞬移。这是客户端表现缺口，球权、碰撞和最终物理仍由服务器决定。

客户端现在从收到 `Controlled` 时**当前可见球位置**开始，在短时间内平滑靠向**持续移动的** `BallControlPoint`，结束后精确跟随。滚动由每帧实际显示位移计算，包含吸入过程。过渡期间球根组件依旧不模拟物理且无碰撞；不在客户端建立约束、不向车辆施力、不更改服务器控制、发射、脱球、伤害或网络复制协议。该过渡是表现近似，不保证逐帧复现服务器的 Chaos 约束轨迹。

## 参数与资产

`UBallDefinition.Presentation.ClientCaptureBlendDuration` 为表现专用参数，单位秒，合法范围 0–1，默认 **0.18 s**；设为 0 可恢复瞬间跟随。旧球 Data Asset 加载新结构时使用此默认值。C++ 仅从 Definition 读取新参数，没有覆盖已经调好的球质量、阻尼、约束力或车辆数值。本轮代码代理未直接修改 `.uasset` 或 `.umap`，现有测试球不需要重新接线。

若团队要调整：在 Unreal Editor 打开 `/Game/Data/MVP_B/DA_Ball_Normal_TestBaseline_v1`，展开 `Presentation → Client Capture Blend Duration`，以 0.18 s 为基线小幅调整，保存 Data Asset；重新启动双进程游戏后比较主机与客户端窗口。不要通过车辆动力、球约束力或网络 PI 设置掩盖这个表现差异。若仅核对默认值，无须保存资产。

## 双进程复测

关闭旧游戏窗口，使用新编译的 C++ 打开 `/Game/Maps/MVPB_NetTest`，运行两人 Listen Server。主机执行 `HALStartMatch`。保持 **Use Less CPU when in Background** 关闭。

1. 分别让主机车和客户端车捕获静止或缓慢移动的球，在**客户端窗口**观察：球应从捕获前的可见位置短暂靠向车头，而不是瞬移；吸入期间继续滚动，到位后贴着控制点稳定跟随。主机窗口继续保留已有物理吸入过程。
2. 在吸入尚未结束时加速、转向、发射，或让碰撞造成脱球；检查球不会停在旧控制点，车辆不会回拉，离开 `Controlled` 后球恢复服务器物理复制。反复捕获同一球，确认每次从当次可见位置开始。
3. 先在低延迟测试，再在约 100 ms 实测 RTT、1% 丢包、20 ms 抖动下复测。记录明显的位置跳变、持续回拉、滚动方向错误或状态不一致；短暂的到达时间差并不代表球权分叉。

## 验证记录

- `HAL_Future_CreationEditor Win64 Development`、`HAL_Future_Creation Win64 Development` 编译通过。
- `HAL.FutureCreation` 自动化 **12/12** 通过，报告：`Saved/Automation/MVPB_Phase6_CaptureBlend/index.json`；其中配置测试覆盖新参数从 `UBallDefinition` 到球 Actor 的读取。
- 自动化没有视觉窗口，无法证明吸入观感或网络压力下的动态切换；团队双窗口复测仍是本项验收条件。
