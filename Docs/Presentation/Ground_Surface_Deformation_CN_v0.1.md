# 通用地面隆起与波动表现

## 配置入口

SlipperyTrail 的入口为 `DA_Ability_SlipperyTrail → Slippery Trail → Presentation → Deformation`。先在母材质完成下节 WPO 接线，保存后勾选 **Enabled**。其余范围、扩散和寿命继续由同一技能 DA 配置。

| 参数 | 默认值及含义 |
| --- | --- |
| Enabled | 默认关闭。接线后启用网格细分、独立材质实例和 WPO 参数 |
| Width Subdivisions | 8，横向等分数量，允许 2～16 的偶数 |
| Vertex Spacing | 40 cm，沿路径的目标顶点间距 |
| Bulge Height | 8 cm，中央基础隆起，允许 0～60 cm |
| Wave Amplitude | 2 cm，波动幅度，允许 0～20 cm，且不超过基础隆起 |
| Wave Period | 3 s，一轮波动的时间 |
| Wave Length | 180 cm，波纹的空间周期 |
| Wave Direction | 30°，波纹沿世界 XY 平面的方向 |

`Ground Lift` 用于整个底面抬高，隆起通过 WPO 单独执行。圆端包含同心细分，外边缘权重为 0，中心为 1。横向与圆端使用同一条平滑权重曲线。长路径自适应增加纵向间距，将横截面总数控制在 512 内。

区域变宽或 Wave Length 调短时，可提高 Width Subdivisions，并适当减小 Vertex Spacing，让波纹获得足够的顶点采样。

## 材质接线

保留当前底纹、绿紫高光和透明度输出，在母材质增加以下节点。所有参数名区分大小写。

1. 新建三个 Scalar Parameter，分别命名 `HAL_GroundBulgeHeight`、`HAL_GroundWaveAmplitude`、`HAL_GroundWavePhase`，默认都填 0。新建 Vector Parameter `HAL_GroundWaveUV`，默认填 `(0,0,0,0)`。
2. `TextureCoordinate[0]` 接 `DotProduct.A`。`HAL_GroundWaveUV` 经过只勾选 **R、G** 的 `ComponentMask`，接 `DotProduct.B`。
3. `DotProduct` 输出与 `HAL_GroundWavePhase` 相加，接 `Sine`，将 Sine 的 **Period** 设为 **1**。
4. Sine 输出乘 `HAL_GroundWaveAmplitude`，再加 `HAL_GroundBulgeHeight`。
5. 添加 `VertexColor`，取 **R**，与上一步结果相乘，得到 Z 偏移。
6. 用 `AppendVector` 将 `Constant2Vector(0,0)` 与 Z 偏移合成三维向量，接 **World Position Offset**。
7. Apply／Save 母材质，在技能 DA 勾选 **Deformation → Enabled**，保存后释放技能测试。

计算式如下，其中 `UV0` 是当前地面材质的平铺坐标。

```text
phase = dot(UV0, HAL_GroundWaveUV.rg) + HAL_GroundWavePhase
height = HAL_GroundBulgeHeight + HAL_GroundWaveAmplitude * sin(2*pi*phase)
WorldPositionOffset = (0, 0, VertexColor.r * height)
```

运行时会为每片地面建立自己的 MID，从该片 DA 快照写入高度和幅度。`HAL_GroundWaveUV` 由纹理平铺尺寸、波长与方向计算，`HAL_GroundWavePhase` 由服务器起始时间计算。材质直接使用这两个参数，即可让观察者在中途看到相同的波动阶段。

材质的 **Max World Position Offset Displacement** 可设为 0，使用组件提供的完整包围盒。如需在材质中显式限制，限制值应覆盖本材质所有配方的 `Bulge Height + Wave Amplitude`。组件按世界 Z 方向扩展包围盒，覆盖从底面到最大隆起的完整高度。

现有 Unlit 油膜的明暗通过 Emissive 图表现。WPO 提供真实顶点起伏，比赛镜头下可结合当前底色与反光调节凸起的可读性。

## 后续地面效果复用

`FHALGroundSurfaceDeformation` 可以直接成为其他技能或区域 DA 的字段。调用方将自身已计算的中心点、地面法线、半宽与连接关系转换为 `FHALGroundSurfacePoint`，交给 `HALGroundSurface::BuildRibbon`。单点可生成圆形区域，多点可生成带圆端的条带，断开的点分别生成独立区段。

调用方创建自己的 `UHALGroundSurfaceComponent`，用 `ConfigureSurface` 传入材质、平铺尺寸及变形配置，再用 `SetMesh` 更新形状。已有稳定形状时，每帧只调用 `SetSurfaceAge` 更新波动阶段。到期时由所属区域隐藏或销毁组件。

岩浆可以配置更大的起伏及流纹，雪地可以将 Wave Amplitude 设为 0，使用静态柔和隆起。该能力输出表现网格和材质参数，区域本身继续提供伤害、抓地或其他玩法结果。当前首个接入者为 SlipperyTrail。

## 验收

- 在低视角观察原地圆斑、直行条带和弯道，确认中央平滑隆起、圆端连续、外边缘贴地。再切回比赛相机检查可读性。
- 分别设置 Wave Amplitude 为 0 和 2，确认静态隆起及动态波动。设置 Bulge Height 和 Wave Amplitude 为 0 时观察平面输出。
- 让车辆腾空后重新落地，确认两段之间的空隙和到期清理。斜坡、急转弯、轨迹绕回处检查表面穿插和半透明叠色。
- Host 与客户端使用同版代码和材质资产，比较同一片区域的波动阶段、范围及共同消失。

## 本轮验证

- 基点 `main/b06ecae`，UE 5.8.3-58210709。Editor／Game Development R3 完整编译链接通过。原生专项 R3 **11/11**，含公共配置、材质实例隔离、服务器相位、包围盒、细分权重、圆端、断段、网格数量约束及既有打滑区域规则。
- D3D12 离屏专项 R3 **1/1**。使用临时生成的 WPO 测试材质，读取 128×128 画面，平面可见像素 3286，隆起前后轮廓变化 422 像素，不同波动阶段变化 364 像素。该记录验证公共参数和顶点变形，实际油膜材质按上文手动接线后验收观感。
- D3D12 本机 Listen Server／Client 双端专项各 **1/1**，Host 出站注入 80 ms。临时配方启用变形，检查本地 MID 接收 8 cm 隆起参数、区域扩宽至完整范围、低抓地和摆动、到期恢复及组件回收。
- 中间记录保留。Editor R1 的测试代码 const 指针调用已修正。首轮双端使用 NullRHI，两个显示断言失败，改为两端 D3D12 后通过。
- 日志及报告在 `Saved/GroundSurface/20261111`，最终报告为 `Native_R3`、`Render_R3` 与 `Network_R3_Rendered`。1243 份保存资产、Config 与 uproject 的保护基线逐份一致。
