# MVP-B 车辆状态 FX-5 电击迁移与接线

2026-11-07，开工基点 `main/7dbe29c`，本机 UE 5.8.3-58210709。FX-5 的程序入口已接通。团队须在 Editor 中启用共享电击配方、保存并验收实际画面。

## 1. 触发与播放职责

本地车仍在本地回弹时立即亮起，观测车仍可根据复制的电击回弹亮起，ASC 视觉 Tag 到达后接续同一段表现。已有 1.5 秒未确认收尾、重复通知去重、受击起点、伤害、回弹和眩晕规则沿用原逻辑。`UHALShockedPresentationComponent` 负责这一特殊触发和原有闪光、扩散、丝带、光照及焦痕时间轴。

启用共享配方后，该驱动把本地有效段提交到每车状态 FX。同一电击 Tag 可同时被驱动和 ASC 观察维持，两份来源聚合为一份通用运行实例。普通调试来源只检验通用账本，真实电击仍从机关接触路径触发。共享配方来自已准备好的目录，每段播放在开始时选定配方，释放、焦痕和焦痕期间再触发沿用同一选择。配置关闭、目录尚未 Ready 或校验失败时，这一段走原 BP 配置。资产晚到可供下一段使用。

同一个渲染器统一管理实际 Mesh 的 Overlay。现有电击材质使用独立参数合同，与其他不兼容表面按活跃或淡出优先于纯残留、`Surface Priority` 及 Tag 排序选择；底层计时继续进行。丝带、附加 Arc System 和可选点光继续随车播放。焦痕到期恢复当时应显示的其他表面或原 Overlay。共享路径使用 FX-1 选出的可见车身和显式注册部件，换 Mesh 时同段效果迁到新目标。

## 2. 保存资产基线

开工时以 Unreal Editor 命令行读取车辆 Blueprint CDO 的电击组件、目录与效果定义。完整的 27 个字段及曲线导出见[保存资产基线](MVPB_Vehicle_Status_FX5_Saved_Baseline_20261107.json)。记录中的 Car01 对应 `BP_RoundedVehiclePawn`。

| 保存车型 | Overlay | Ribbon System | Arc System | Arc Offset Z | 丝带 Width |
|---|---|---|---|---:|---:|
| Car01 | `M_FX_VehicleShock_Overlay` | `NS_VehicleShockRibbon` | None | 20 cm | 10 cm |
| Car02、Car03、BP_TestVehiclePawn | None | None | None | 0 cm | 2.6 cm |

保存的 `DA_VehicleStatusFX_Default` 已将 `State.Visual.Shocked` 指向 `DA_StatusFX_Shocked`。保存配方已有同一电击材质，`Enable Rendering` 关闭，通用时序仍是 0.1 / 0.3 / 0 秒。以下以实际有完整视觉资产的 Car01 为迁移基准。

## 3. Editor 配置

1. 先在当前版本用 Car01 撞电击墙，录一份短视频以便比较闪光、丝带和焦痕时长。
2. 打开 `/Game/Data/StatusFX/DA_StatusFX_Shocked`。确认 `Overlay Material` 是 `/Game/Assets/FX/Materials/M_FX_VehicleShock_Overlay`，`Surface Layout` 为 None，`Scalars`、`Colors`、`Curves`、`Niagara Layers` 保持空。该着色器使用专用电击参数。
3. 在 `Timing` 设 `Fade In Seconds = 0`、`Fade Out Seconds = 0.6`、`Residue Seconds = 4.0`。`Fade In Seconds` 是通用实例的包络，首帧闪光由电击驱动负责。首轮可保留 `Surface Priority = 0`。
4. 勾选 `Enable Rendering` 和 `Use Shared Shock Look`。展开 `Shock Look`，设 `Ribbon System = /Game/Assets/FX/NS_VehicleShockRibbon`、`Arc Offset Cm = (0, 0, 20)`、`Arc Settings → Width Cm = 10`。额外 `Arc System` 保持 None，`Enable Light` 关闭。
5. 核对下表。保存后对定义和目录执行 **Validate Assets**，重新启动双端进程，避免正在运行的目录或表现实例继续使用旧设置。各车 Definition 的 `Status FX Catalog Override` 留空时自然使用项目默认；自定义目录可直接引用同一份电击定义。

| 字段 | Car01 保存值 |
|---|---|
| Flash / Wipe / Wipe Radius | 0.06 s / 0.12 s / 400 cm |
| Base Brightness / Fade Filament Gain / Flicker Steps | 8 / 40 / 14 |
| Brightness Curve（秒，倍率） | (0, 1.6)、(0.4, 1)、(1, 0.55)、(1.6, 0.4) |
| Filament Power Curve（秒，指数） | (0, 34)、(0.4, 48)、(1.6, 70) |
| Arc Count / Subdivisions / Roughness / Lift Fraction | 4 / 4 / 0.32 / 0.07 |
| Branch Chance / Branch Length / Width Cm | 0.55 / 0.30 / 10 |
| Arc Scale / Arc Color（RGBA） | 1 / (0.22, 0.52, 1, 1) |
| Retract Arcs On Release / Feather / Scorch Rise Seconds | 开 / 0.25 / 0.25 s |
| Light Color（RGBA） / Peak / Radius / Offset | (0.55, 0.88, 1, 1) / 2200 cd / 280 cm / (0, 0, 40) cm |
| Light Flicker Floor / Light Flash Boost | 0.6 / 0.6 |

## 4. 团队画面验收

- 同一辆车先测 Host，再测远程控制车。客户端撞墙当下应闪一次，回弹确认时亮度与扩散连续，另一端能观察到同一轮电击。眩晕结束后检查丝带收束、焦痕淡去和原 Overlay 恢复。
- 用 Car01 与原视频比对，再换 Car02、Car03 确认读取同一配方，检查三个车身的包覆和丝带范围。对独立 Mesh 的可见部件，按 FX-1 注册后检查其表面。
- 在焦痕还可见时再次电击，确认新一轮电击正常且焦痕连续。切换车型或离开回合，检查旧车材质、光照、丝带清理。
- 可打开 `hal.ShockedPresentationLog 1` 查看播放挂载与资源数量，执行 `hal.StatusFXStatus local` 查看目录、来源和实例。需要回退时关闭 `Use Shared Shock Look`，保存并重启进程。

FX-6 负责与第二种视觉状态一起验收实际组合画面、多人压力及首次播放。

## 5. 本轮构建与验证

- UE 5.8.3 Win64 Development Editor 和 Game Target 均通过。全量 `HAL.FutureCreation` 为 183/183，包含新增的共享配方接续、换 Mesh、焦痕回收与 Overlay 仲裁用例。
- D3D12 离屏 `SharedShockHandoffAndCleanup` 通过。临时开启的配方引用现有保存电击材质，验证同实例交接、真实 Mesh 上的 MID、换 Mesh 和原 Overlay 恢复。本项是组件检查，三车型的丝带及材质画面仍由团队用保存配方验收。
- `ElectricPrediction.Accept`、`ElectricPrediction.Reject` 在 `MVPB_NetTest` 双端各 1/1。确认用例在客户端记录到权威眩晕之前的预测表现，同一次接触只开一段。首轮误用练习房直连地图，夹具停在比赛准备条件前而超时；换回直连比赛图后两项通过。`Camera.RoomElectricContinuity` 从真实入口启动，Host 出站加 80ms，练习房与比赛各有 6 次接触，两端均通过。这些网络回归沿用当前保存配置。
- 1193 份保存 `.uasset`／`.umap` 的施工前后 SHA-256 一致。原始命令行、日志与报告位于 `Saved/StatusFX/FX5_20261107`。验证进程均已退出。保存配方实际画面、打包和压力验收另行记录。
