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

打开 `/Game/Assets/FX/Materials/M_SlipperyTrail_OilSheen` 母材质，在材质图的空白处新增一组地面变形节点。以下是一条独立连接到 World Position Offset 的分支。所有参数名按下表逐字填写，程序会按这些名字向运行时材质实例写入数值。

1. **创建四个参数节点。** 在材质图空白处按住 **S** 并单击左键，创建 Scalar Parameter。选中节点，在 Details 的 **Parameter Name** 填写名称，在 **Default Value** 填写 0。重复创建下表前三个参数。按住 **V** 并单击左键，创建 Vector Parameter，将名称填为表中最后一项，打开默认颜色，把 R、G、B、A 四个分量都设为 0。也可右键搜索完整节点名创建。 [Epic 材质实例操作](https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-and-using-material-instances-in-unreal-engine)

   | 节点类型 | Parameter Name | Default Value | 运行时来源 |
   | --- | --- | --- | --- |
   | Scalar Parameter | `HAL_GroundBulgeHeight` | 0 | DA 的 Bulge Height |
   | Scalar Parameter | `HAL_GroundWaveAmplitude` | 0 | DA 的 Wave Amplitude |
   | Scalar Parameter | `HAL_GroundWavePhase` | 0 | 根据服务器时间计算的波动相位 |
   | Vector Parameter | `HAL_GroundWaveUV` | `(0,0,0,0)` | 由平铺尺寸、波长及方向计算 |

   母材质的默认高度和幅度设为 0。游戏中启用 Deformation 后，C++ 会分别写入 DA 的 8 cm 和 2 cm 等实际配置。

2. **计算波纹的空间位置。** 右键搜索并创建 `TextureCoordinate`，选中后将 **Coordinate Index** 设为 0，**U Tiling / V Tiling** 都设为 1。新建 `ComponentMask`，在 Details 只勾 **R、G**，将 **B、A** 取消勾选。将 `HAL_GroundWaveUV` 的 **RGB 总输出**接到这个 Mask 的输入。新建 `DotProduct`，将刚创建的 TextureCoordinate 输出直接接 **A**，Mask 的输出接 **B**。这两条输入都是二通道。 [Epic ComponentMask 与 DotProduct](https://dev.epicgames.com/documentation/en-us/unreal-engine/math-material-expressions-in-unreal-engine)

3. **让波纹随时间运动。** 新建第一个 `Add`，将 DotProduct 输出接 **A**，`HAL_GroundWavePhase` 接 **B**。新建 `Sine`，将这个 Add 的输出接到 Sine 输入。选中 Sine，在 Details 中将 **Period** 填为 **1**。Sine 输出保留 −1～1 的范围。一轮实际持续 3 秒由 DA 的 Wave Period 和程序计算控制，此处的 Period=1 对应相位变化一个单位。 [Epic Sine 节点](https://dev.epicgames.com/documentation/unreal-engine/math-material-expressions-in-unreal-engine)

4. **把波形变成厘米高度。** 新建第一个 `Multiply`，将 Sine 输出接 **A**，`HAL_GroundWaveAmplitude` 接 **B**。再新建第二个 `Add`，将这个 Multiply 输出接 **A**，`HAL_GroundBulgeHeight` 接 **B**。这个 Add 的输出就是中央高度。以基础高度 8 cm、幅度 2 cm 为例，其数值在 6～10 cm 之间变化。

5. **让边缘贴地。** 新建 `VertexColor`，使用标有 **R** 的红色输出引脚。新建第二个 `Multiply`，将第 4 步的 Add 输出接 **A**，VertexColor 的 **R** 接 **B**。程序在生成网格时已经写入这项权重，中央为 1、外边缘为 0，中间平滑过渡。因此中央获得完整高度，边缘获得 0 cm 偏移。圆端也使用相同权重。

6. **合成 WPO 的三维偏移。** 在空白处按住 **2** 并单击左键，或右键搜索 `Constant2Vector`，把其两个分量都设为 0。新建 `AppendVector`，将这个 Constant2Vector 接 **A**，第 5 步的 Multiply 输出接 **B**。AppendVector 的结果为 `(0,0,Z偏移)`。将它接到主材质输出节点的 **World Position Offset**。输入顺序决定最终的 XYZ，A 提供前两个分量，B 提供第三个分量。 [Epic AppendVector](https://dev.epicgames.com/documentation/unreal-engine/math-material-expressions-in-unreal-engine)

7. **保存并在游戏中验证。** 点击母材质的 **Apply**，等待编译结束，再 **Save**。打开 `DA_Ability_SlipperyTrail`，确认 **Presentation → Ground Material** 引用 `MI_SlipperyTrail_OilSheen`。展开 **Presentation → Deformation**，勾选 **Enabled**，先使用 Width Subdivisions=8、Bulge Height=8、Wave Amplitude=2、Wave Period=3，保存 DA。重新释放一次技能，让新生成的区域读取这份参数。

效果以游戏中生成的地面条带为准，那里同时具备程序写入的材质参数和顶点权重。可先将 DA 的 Wave Amplitude 设为 0，观察静态隆起，再改回 2，观察缓慢起伏。母材质预览读取默认零值，实际隆起与波动通过游戏内生成的条带观察。

计算式如下，其中 `UV0` 是当前地面材质的平铺坐标。

```text
phase = dot(UV0, HAL_GroundWaveUV.rg) + HAL_GroundWavePhase
height = HAL_GroundBulgeHeight + HAL_GroundWaveAmplitude * sin(2*pi*phase)
WorldPositionOffset = (0, 0, VertexColor.r * height)
```

运行时会为每片地面建立自己的 MID，从该片 DA 快照写入高度和幅度。`HAL_GroundWaveUV` 由纹理平铺尺寸、波长与方向计算，`HAL_GroundWavePhase` 由服务器起始时间计算。材质直接使用这两个参数，即可让观察者在中途看到相同的波动阶段。

材质的 **Max World Position Offset Displacement** 可设为 0，使用组件提供的完整包围盒。如需在材质中显式限制，限制值应覆盖本材质所有配方的 `Bulge Height + Wave Amplitude`。组件按世界 Z 方向扩展包围盒，覆盖从底面到最大隆起的完整高度。

现有 Unlit 油膜的明暗通过 Emissive 图表现。WPO 提供真实顶点起伏，比赛镜头下可结合当前底色与反光调节凸起的可读性。

### 顶视角下的体积明暗

当前网格法线取自原地面采样，WPO 修改顶点高度。地面组件关闭投影。Unlit 油膜的颜色由 Emissive 输出，因此隆起主要通过轮廓和透视表现。在接近顶视的比赛镜头下，需要额外的明暗来表现中央和边缘之间的曲率。若采用 Default Lit，还需要提供变形后曲面的法线，并把受光底色与自发光纹路分别接入 Base Color 和 Emissive。

首轮可以利用已有的 VertexColor.R 做柔和的中央亮、边缘暗，按以下步骤验证体积可读性。

1. 新建 `Lerp`，A 填 `0.35`，B 填 `1.0`，Alpha 接用于 WPO 的同一份 **VertexColor.R**。
2. 新建 `Multiply`，A 接原本输出到 Emissive Color 的最终油膜颜色，B 接这个 Lerp。
3. 将 Multiply 输出接 Emissive Color，Apply／Save，再在比赛镜头下释放技能对照。数值为明暗首测值，美术后续可按画面调节。

该分支提供风格化明暗。WPO 继续负责实际隆起。可从低视角观察网格轮廓确认高度，再回到比赛镜头检查明暗效果。Wave Amplitude 为 0 时表现静态隆起，配置大于 0 的幅度才会出现缓慢起伏。

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
