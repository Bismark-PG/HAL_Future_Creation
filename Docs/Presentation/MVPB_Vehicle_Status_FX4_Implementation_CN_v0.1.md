# FX-4A～4D 实施与 Editor 配置

日期：2026-11-07。承接[施工合同](MVPB_Vehicle_Status_FX4_Plan_CN_v0.1.md)。**两个兼容表面通道是首版能力与验收目标；状态数量独立管理，显示通道允许扩展。** 下面的测试配方只是纯视觉输入，不新增正式技能、Tag授权或伤害规则。

## 1. 代码边界

- 4A：运行时状态FX和旧电击通过同一个Overlay协调器设置／恢复每Mesh表面。旧电击从起手到焦痕结束优先；其他通用表面的时钟继续推进，其他粒子可继续。车辆装配仍能配置原始Overlay，重新配置时协调器识别更新后的基线。结束、失效目标、解绑和退场释放各自资源。
- 4B：`HALStatusFXSurfaceLayout` 共享DA声明母材质、通道Id及参数前缀。默认 `Primary/FX0_` 和 `Secondary/FX1_`，数组可扩展。运行时每Mesh一次输出，同布局不同通道可合成。先比较活跃／淡出与纯残留，再比较DA优先级，同级按精确Tag字典序。贡献者替换时清除旧参数，失去显示机会的状态和来源仍然有效。
- 4C：Bounds层读取实际外观Mesh的局部中心／半尺寸，按完整组件变换附着。出粒率可按近似包围面积调节，外扩按比例；强度、年龄、残留与预算每帧写入，实例复用。玩法状态、碰撞与权威网络不参与该过程。
- 4D：Static／Skeletal模板由实际组件种类选择。CPU三角面采样按指定LOD检查资产条件，骨骼源使用当前帧蒙皮数据。未配置或无法绑定的单层给出诊断；仅在配置了替代Bounds模板时降级。CPU路径不保证复现仅在GPU端计算的变形或布料。

`Enable Rendering` 默认关闭，因此保存的旧定义行为保持。`State.Visual.Shocked` 通用路径继续只观察／准备，旧驱动的电击材质参数、提前反馈、焦痕、灯光和电弧不在本轮迁移。真实电击迁移归FX-5。

## 2. 测试表面：先做两种颜色

1. 在 Content Browser 新建测试材质 `M_FX4_Test_2Channel`。推荐 Surface、Translucent、Unlit；在 Usage 中确认支持 Skeletal Mesh。静态及骨骼模型都需实际看一遍，现有基础车身材质不需替换。
2. 创建以下**全局**参数并接到材质输出。两个通道的 `Intensity`、`Age`、`Residue` 均为 Scalar，默认0；`Tint` 为 Vector，通道0设红、通道1设蓝。图中使用 `Age` 做轻微脉冲，以便验证被遮挡期间时钟继续。一个可用示例：

   ```text
   W0 = saturate(FX0_Intensity + FX0_Residue)
   W1 = saturate(FX1_Intensity + FX1_Residue)
   Emissive = FX0_Tint.rgb * W0 * (1 + 0.05 * sin(6 * FX0_Age))
            + FX1_Tint.rgb * W1 * (1 + 0.05 * sin(6 * FX1_Age))
   Opacity = 0.4 * saturate(W0 + W1)
   ```

3. 通过 **Add → Miscellaneous → Data Asset** 选 `HALStatusFXSurfaceLayout`，创建 `DA_FX4_TestLayout`。填上述材质，保留默认 `Primary/FX0_`、`Secondary/FX1_`。将来扩展需先在母材质接好新前缀，再在同一个布局DA增加通道，不逐车型设置。
4. 创建两份 `HALVehicleStatusFXDefinition`，分别启用 **Enable Rendering**，选择同一个 **Surface Layout**，通道分别 `Primary`／`Secondary`，**Overlay Material** 可留空。要覆盖 Tint 时，在配方 **Colors** 中填参数名 `Tint`，前缀由布局加上。**Scalars／Curves** 也填未加前缀的名字；曲线X单位为实例开始后的秒，末键后保持。不要在这些列表中重复使用保留名 `Intensity`、`Age`、`Residue`。优先级和 FadeIn／FadeOut／Residue 按DA调整。
5. 独立样式不填 **Surface Layout**，直接设置 **Overlay Material**，材料中提供 `FX_Intensity`、`FX_Age`、`FX_Residue` 三个Scalar；必要时可在配方中改名。独立材质参与优先级选择；任意独立材质不会自动混合。
6. 在测试目录DA登记 `State.Visual.DebugA`／`State.Visual.DebugB` 对应两份配方，保留原电击条目。建议使用复制的测试目录，暂时从项目 **Vehicle Status FX → Default Catalog** 或测试车型的目录覆盖引用；结束后恢复正式配置。保存并重启本次游戏进程，目录和渲染契约按绑定缓存。

## 3. Niagara：先接 Bounds，再接真实表面

为 FX-4 建独立测试系统，保留正式旧电击资产。所有模板首版使用 **CPU Sim** 和 **Local Space**。在 Niagara 的 **User Parameters** 创建下列参数，名称和类型需完全一致：

| 参数 | 类型／单位 | 用途 |
|---|---|---|
| `User.FX_Intensity` | Float，0～1 | 当前淡入／淡出强度，可驱动颜色／透明度 |
| `User.FX_Age` | Float，秒 | 本车状态已经过的时间 |
| `User.FX_Residue` | Float，0～1 | 独立残留权重 |
| `User.FX_SpawnRate` | Float，个／秒 | 已乘表现包络；发射率直接消费，无需重复乘强度 |
| `User.FX_MaxParticles` | Integer | 本层获分配的活粒子预算 |
| `User.FX_BoundsCenter` | Vector，局部cm | Mesh局部中心 |
| `User.FX_BoundsExtent` | Vector，局部cm | 已按 PaddingFraction 外扩的半尺寸；需全尺寸的模块用×2 |
| `User.FX_SourceMesh` | 对应 Static Mesh 或 Skeletal Mesh **Data Interface** | 仅真实表面模板，代码绑定实际组件；不要使用普通Mesh资产对象参数代替 |

- **Bounds**：在 Particle Spawn 中用 Box Shape／位置模块消费中心与尺寸；Emitter Update 的发射逻辑使用 `FX_SpawnRate`。模板应将实际发射数量限制在 `FX_MaxParticles - 当前存活数` 的非负范围（同层多个Emitter要共享分配），并使生命周期能在 FadeOut／Residue 内结束。只声明参数不接图不会自动产生粒子或硬性限制第三方模板。
- **Mesh Surface**：分别复制一份 Static、一份 Skeletal 模板。使用各自的 Mesh Location／三角面采样模块，把 Mesh Data Interface 绑定到 `User.FX_SourceMesh`，从实际三角面位置／法线初始化粒子。检查采样模块是世界位置还是本地位置；本方案使用 Local Space，世界点要通过模块转换到模拟空间，避免重复乘组件变换。Skeletal适配器指定当前帧数据和按需蒙皮。
- **资产条件**：Static Mesh 打开 **Allow CPU Access**；Skeletal Mesh 在采样LOD的LOD Info中打开 **Allow CPU Access**。确认LOD存在且保留三角面，应用／保存并在重新打开的测试中验证。`Sampling LOD` 默认0；首版以固定采样LOD为合同，不随相机距离重建系统，模板不要再配置独立的LOD User绑定覆盖它。极端缩放、不同LOD与骨骼姿态都要实看。
- **配方**：在 **Niagara Layers** 添加唯一 Layer Id，Bounds只填 **Bounds System**，Mesh Surface填 **Static Surface System** 和 **Skeletal Surface System**。可选 **Surface Fallback Bounds System**；空则失败时只跳过该层，有配置则输出替代并记录原因。`Spawn Rate` 单位个／秒，`Maximum Particles` 是本层上限，`Scale Rate By Size` 基于近似包围盒面积；项目设置可限制每车粒子／粒子层预算。不要把预算当作玩法状态上限。

## 4. 游戏窗口中的验收

```text
hal.StatusFXDebug local State.Visual.DebugA A 1
hal.StatusFXDebug local State.Visual.DebugB B 1
hal.StatusFXStatus local
hal.StatusFXDebug local State.Visual.DebugA A 0
hal.StatusFXDebugClear local
```

先确认两通道同时工作，再逆序关闭，并用同通道高／低优先级验证遮蔽与当前时钟恢复。进真实电墙，旧电击应继续正常；它持有的表面暂时优先，焦痕结束后通用样式按当时状态恢复。全部结束要恢复车型原Overlay。用至少一台静态和一台骨骼车检查缩放、滚转、隐藏部件和换车／退场；确认真实采样来自表面，并对缺CPU数据分别检查跳过和显式Fallback。Host／Client调试命令本地独立，真实状态仍由既有ASC复制结果驱动。

`hal.StatusFXStatus` 的 **SurfaceChannels** 是各当前目标实际使用的通用通道总数（多部件会求和），与 **Instances／ASC／Debug** 独立。**OverlayTargets** 可包含旧电击占用；**RejectedSurfaces／RejectedParticles** 与具体原因帮助区分未配置、预算、模板错误与编译等待。**PreparedHeadless** 表示已配置但没有渲染环境；**Rendered**／**Active** 表示已提交给Niagara，不保证任意模板的图形已正确接线。

## 5. 验证记录与后续

原始材料保存在 `Saved/StatusFX/FX4_20261107/`。A／B阶段只编译，四步均完成后才执行集中测试。最终构建／原生／双端／实际RHI结果以同版证据为准。团队按第2～4节完成保存资产接线和实际粒子画面验收后另记；测试夹具不代替正式材质、Niagara图、Cook或四车性能验收。

最终 R9 Editor／Game 编译成功，原生 **181/181**（新增9项）、D3D12 离屏双通道 **1/1**、专项观察和GF7三回合的Host／Client分别各 **1/1**。六份最终报告均0测试警告／失败／未运行／进行中。离屏基础像素RGB `(4,4,4)`，红蓝双通道 `(243,32,224)`，关闭蓝通道后 `(246,5,4)`；是真实材质输出测试，使用瞬态静态Mesh／材质夹具。专项Host为D3D12，日志确认旧电击挂载到 `SkeletalVisual`；Client及三回合回归为NullRHI，Host出站注入80ms。原生夹具验证真实组件DI绑定、CPU预检、预算、Bounds参数及回收，正式Niagara采样画面仍待团队配置。详见[验证保护记录](MVPB_Vehicle_Status_FX4_Verification_20261107.json)。

开工干净 `main/73e0c13`。1193份保存资产逐份未变；已有320输入仅改12份，其他308份保持，新增8份，最终328份冻结一致。Config／uproject保持；Build.cs只为Editor验证夹具增加RHI／RenderCore依赖。专用进程已退出，未提交／Push。

迭代记录：初期编译分别修正UHT单位标记、UE5.8静态Mesh返回类型、Editor渲染夹具所需RHI／RenderCore链接及捕获枚举；失败日志保留。R4／R5原生各179通过／2失败，暴露Niagara公开参数视图的短名称／类型差异；修成带完整名称和DI类的原始参数检查后，R6专项8/9，剩下的静态夹具缺LOD Section。补齐数据并检查实际CPU三角面／权重缓存后R7专项9/9。R7离屏RHI报告1失败，两次只读到暗灰；固定透视、曝光与同步着色器编译后的R9实际像素通过。任何失败的进程退出码0都不算测试通过。

下一开发入口为FX-5：先采集当前保存的旧电击材质、参数、主弧和焦痕基线，再迁移即时反馈、权威接续和表现尾迹；正式资产画面与FX-6组合／性能仍独立验收。
