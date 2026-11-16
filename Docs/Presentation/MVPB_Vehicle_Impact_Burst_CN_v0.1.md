# MVP-B 车辆受击漫画打击特效（Impact Burst）v0.1

> 一句话：**每一次被权威接受的撞击，都会在真正的接触点上画一记漫画爆点，力道越强爆点越大、尖刺越多、颜色越烫。**
> 全程程序化——没有贴图、没有图集、没有 Niagara，整个造型由一个 Custom HLSL 节点从精灵 UV 算出来。

---

## 1. 触发链路

```
碰撞 / 攻击
  └─ UHALImpactReceiverComponent::Resolve           权威，已有流程
       ├─ Rules.SelectTier(速度, Cap)               已有：Light / Medium / Heavy
       ├─ Summary.EffectiveTier                     已有：扣掉抗性后的实际分级
       └─ Presentation->PublishAcceptedImpact(Result, AcceptedDirection, Hit.ImpactPoint)
            ├─ AllocateImpactSequence()             一次撞击一个身份，回弹与爆点共用
            ├─ PublishImpactBurst(...)  →  MulticastImpactBurst(FHALImpactBurstEvent)
            └─ MulticastImpactRebound(...)          已有车身回弹，行为未改
                 ↓（所有客户端 + Listen Server 本机）
            ConsumeImpactBurst → AcceptBurst → 取一个精灵槽 → 写 MID 参数
                 ↓（每帧，走表现组件既有的 Tick，没有新增 Tick）
            UpdateImpactBursts → AdvanceBursts → 写 FX_Age01 → 过期就回收精灵
```

**什么时候会出现**：`IsImpactReboundEligible(Result)` 为真的每一次撞击，也就是「权威接受、实际施加了冲量、没有被免伤或流程压制、分级不是 None」。普通车对撞不扣血（`bApplyDamage = false`）但**有**撞击分级，所以照样有爆点——这是碰碰车该有的打击感。扣血与否由 FX-6 受击红闪负责，两个信号分工不同：

| 信号 | 含义 |
|---|---|
| 漫画爆点（本文件） | 撞到了，而且撞得有多重 |
| 受击红闪 `State.Visual.Hit` | 我真的掉血了 |

**不会出现**：被免伤吃掉、被流程压制、分级降到 None、或者权威根本没接受的接触。爆点**不参与任何玩法判定**，删掉它规则完全不变。

---

## 2. 力道怎么变成大小

| 来源 | 值 | 用途 |
|---|---|---|
| `Summary.EffectiveTier` | Light / Medium / Heavy | 轮廓颜色（暖白 → 金黄 → 橙红） |
| `Summary.DesiredHorizontalDeltaSpeed` | cm/s，权威写入的目标推开速度 | 映射成 0..1 强度 → 尺寸、尖刺数、亮度、寿命 |

`BurstStrength()` 把 `MinimumPushSpeed`(150) ~ `MaximumPushSpeed`(900) 线性映射到 0..1，量化成一个字节随事件复制。强度再线性插值出：

- **尺寸** `MinimumSizeCm`(90) → `MaximumSizeCm`(240)，精灵直径，单位真的是 cm
- **尖刺数** `MinimumSpikes`(7) → `MaximumSpikes`(13)，材质内部向下取整
- **寿命** `LifetimeSeconds`(0.22) + `StrengthLifetimeSeconds`(0.06) × 强度
- **亮度** `FX_StrengthGain`(1.6) 的插值

> **诚实的限制**：`DesiredHorizontalDeltaSpeed` 是权威**写入的目标推力**，它跟着分级、PushPreset 和抗性减级走，所以实际上接近「三档 + 少量变化」，不是完全连续的碰撞速度。要做到真正连续，得把碰撞回调里算出的 `NormalImpulse.Size() / Mass` 一路带进 `FHALImpactSummary`——那是另一个改动，本轮没做。

---

## 3. 造型怎么来的

`Tools/ImpactBurst/ImpactBurst.hlsl`，一个 Custom 节点，输入全是参数引脚：

```hlsl
float2 p = UV * 2.0 - 1.0;
p /= scale;                                  // 弹出 + 尾段微胀
float r = length(p);
float ang = atan2(p.y, p.x) + Seed * 6.2831853;
float s1 = pow(abs(sin(ang * lobes * 0.5)), k);
float s2 = pow(abs(sin(ang * (lobes * 0.5 + 1.0) + 2.3)), k) * Irregular;
float shape = max(s1, s2);                   // max 而不是平均：尖刺才是尖的
float rad = lerp(InnerRadius, 1.0, shape) * 0.98;
float fill = 1.0 - smoothstep(rad - aa, rad + aa, r);   // aa = fwidth(r)，一像素硬边
```

**两个踩过的坑**，调参时别踩回去：

1. **`Sharpness` 不能小于 1**。第一版用 0.55，`pow` 指数小于 1 会把所有波瓣推向 1.0，画出来是一朵**雏菊**不是爆点。现在预设 4.0，越大尖刺越细越长。
2. **两条谐波要用 `max()` 合并，不能相加平均**。相加会互相填平谷底，尖刺变钝；`max()` 才能保留硬尖角，再用 `Irregular`(0.78) 把第二条压矮，长短交错才像手画的。

形状是随机相位的：`FX_Seed` 由权威事件序号哈希而来，**所有客户端对同一次撞击画出一模一样的爆点**，但相邻两次撞击形状不同。

| 参数 | 预设 | 作用 |
|---|---|---|
| `FX_InnerRadius` | 0.34 | 谷底半径。越小尖刺越长 |
| `FX_Sharpness` | 4.0 | 尖刺锐度。**低于 1 会变成花** |
| `FX_Irregular` | 0.78 | 第二组尖刺的长度比，0 = 完全规则的星 |
| `FX_EdgeWidth` | 0.2 | 轮廓带宽度。细尖刺会整根吃到轮廓色 |
| `FX_StartScale` / `FX_PopFraction` | 0.5 / 0.18 | 弹出：前 18% 寿命从半尺寸弹到满尺寸 |
| `FX_TailGrowth` / `FX_FadeStart` | 0.22 / 0.5 | 尾段：后半段一边胀一边淡出 |
| `FX_FillGain` / `FX_EdgeGain` | 1.4 / 3.0 | 填充与轮廓亮度，轮廓要烫到吃 Bloom |

材质本体是 **Unlit / Translucent / Two Sided / Disable Depth Test**。关掉深度测试是刻意的：漫画爆点是贴在画面上的「印章」，被自己的车切掉一半会很糟。如果实测发现它穿墙穿得难看，把 `M_FX_ImpactBurst` 的 Disable Depth Test 关掉，再把 `VerticalOffsetCm` 调高就好。

---

## 4. 渲染载体

一个 `UMaterialBillboardComponent` 精灵池，挂在被打中的那台车下面，但开了 **Absolute Location**——爆点钉在世界空间的接触点上，车被撞飞了爆点也不跟着跑。

- 每车最多 `MaximumConcurrent`(4) 个同时存在，满了就回收最旧的那个
- 精灵**懒加载**：第一次真的要画才 `NewObject`，不打就永远不创建
- `BaseSizeX/Y` 是半边长，所以代码写的是 `SizeCm * 0.5`，配置里的尺寸是真正的直径
- 材质只在 `BeginPlay` 用 `LoadSynchronous` 解析一次，**命中当下不会同步加载任何资产**；专用服务器完全不加载

---

## 5. 调参入口

全部在 **Project Settings → Game → Vehicle Impact Burst FX**（`UHALImpactBurstSettings`，写进 `Config/DefaultGame.ini`）。**不需要改任何车辆资产、DA 或蓝图。**

```ini
[/Script/HAL_Future_Creation.HALImpactBurstSettings]
Burst=(bEnabled=True,Material="/Game/Assets/FX/Materials/MI_FX_ImpactBurst.MI_FX_ImpactBurst")
```

- `bEnabled=False` 一键关掉整条路径（连广播都不发）
- `Material` 清空 → 逻辑照跑但什么都不画，可以用来隔离问题
- 造型微调改 `MI_FX_ImpactBurst` 的参数；尺寸、颜色、寿命、数量改 Project Settings
- 设定是在 `BeginPlay` 读一次的，**改完要重开 PIE 才生效**

调试：`hal.VehicleImpactBurstLog 1` 打印权威发布与客户端消费的序号、分级、强度与落点。

---

## 6. 自动化指令

### 6.1 建材质（编辑器**关闭**时跑）

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\UEProjects\HAL_Future_Creation\HAL_Future_Creation.uproject" -run=pythonscript -script="C:\UEProjects\HAL_Future_Creation\Tools\ImpactBurst\CreateImpactBurstAssets.py" -unattended -nosplash -AllowCommandletRendering
```

编辑器开着时 `.uasset` 被锁，这条会以 exit 255 失败在 `Error moving file`。此时改为在开着的编辑器里跑同一支脚本即可，脚本是幂等的，两条路结果一样。

### 6.2 编译（编辑器**必须关闭**，项目的 launcher 自己会挡）

```bash
powershell -ExecutionPolicy Bypass -File "C:\UEProjects\HAL_Future_Creation\Tools\BuildEditor.ps1" -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
```

> 这台机器上不带 `-EngineRoot` 会以 `No usable engine for '5.8'` 失败——launcher 的引擎自动探测找不到 Launcher 版引擎，显式指定即可。

### 6.3 跑自动化测试（独立 NullRHI 行程，不要在开着的编辑器里跑）

```bash
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\UEProjects\HAL_Future_Creation\HAL_Future_Creation.uproject" -ExecCmds="Automation RunTests HAL.FutureCreation.Presentation+HAL.FutureCreation.StatusFX;Quit" -unattended -nopause -nosplash -NullRHI -log
```

---

## 7. 团队验收步骤

1. 开 `Content/Maps/Test`，PIE。
2. 用一台车去撞另一台车，看接触点上有没有爆点，位置对不对（应该在两车相碰那一侧，不是车心）。
3. 慢速轻撞 vs 高速重撞，比较**大小、尖刺数、颜色**（暖白 → 金黄 → 橙红）是否能一眼分出轻重。
4. 撞墙、撞电击墙、被球击中，三种来源都要看一次；球击中时如果攻击没带接触点，爆点会退回到车身朝向来源那一侧的边缘。
5. 连续快速对撞，确认同时最多 4 个、旧的会消失、没有残留不消的精灵。
6. 俯视角下确认爆点**没有挡住球的轨迹、危险区边界和车辆姿态**（AGENTS.md 第 12 节）。太大就调 `MaximumSizeCm`，太久就调 `LifetimeSeconds`。
7. 双端（Listen Server + Client）各看一次：客户端的爆点会比本地碰撞晚一个 RTT，**这里没有本地预测驱动**。

---

## 8. 回滚

| 想做的事 | 动作 |
|---|---|
| 暂时关掉 | Project Settings 里 `bEnabled=False`，或 ini 改一行 |
| 只关画面、留逻辑 | `Material` 清空 |
| 完全移除 | 删 `HALImpactBurstSettings.*`、`HALVehicleImpactBurst.cpp`、`Tests/HALVehicleImpactBurstTests.cpp`、两份材质资产；把 `PublishAcceptedImpact` 的第三个参数和 `FHALImpactBurst*` 三个结构体拿掉 |

车身回弹、伤害、冲量、球规则完全没有被这次改动碰过。

---

## 9. 已验证 / 未验证

**已验证**（2026-11-15）

- `HAL_Future_CreationEditor` Win64 Development **编译通过**，新增／改动的 9 个源文件**零新增警告**。
- `HAL.FutureCreation` **全量自动化测试 218/218 通过**（217 Success + 1 SuccessWithWarnings，0 失败、0 未运行、0 进行中），独立 `UnrealEditor-Cmd.exe … -NullRHI` 行程。唯一那条警告是既有的 `VehicleTest.InvalidAndBlockedReplacementRetainsCurrentCar`（测试地图的 PlayerStart 提示），与本次改动无关。
- 新增两条测试：`ImpactBurst.StrengthSeedAndPlacement`（强度映射、量化往返、种子决定性、分级配色、落点与退路）与 `ImpactBurst.IdentityBudgetAndLifetime`（身份去重、迟到丢弃、预算淘汰与精灵回收、寿命推进、序号回绕、关闭后保留水位）。既有的 `Presentation.Impact.IdentityPoseAndConfiguration` 同样通过，车身回弹没有被这次重构碰坏。
- 材质在编辑器内与 headless 各建一次，两次都是 **0 条编译讯息**、215 条 pixel shader 指令、22 个节点、17 个参数全部暴露；脚本幂等，第二次不重建资产也不留孤儿节点。
- 离屏预览确认造型：**白色实心星芒 + 长短交错的硬尖刺 + 尖端暖色轮廓**。
- 设定确实从 `DefaultGame.ini` 读进来了：headless 探测拿到 `enabled=True`、材质软引用解析到 `MI_FX_ImpactBurst`、尺寸 90/240、推力 150/900、寿命 0.22、同时上限 4、参数名 `FX_Age01`。
- **全程未提交。**

**未验证**（本轮没有做，不要当成做过）

- **PIE 实机画面**：落点准不准、俯视角下可读性、关闭深度测试会不会穿墙穿得难看、大小与寿命的手感。
- 双端延迟下的表现（**客户端比本地碰撞晚一个 RTT，这里没有本地预测驱动**），以及与受击红闪／电击／`BattleTop` 同时出现的叠加观感。
- 效能与 Cook：Cook 收集已经接到既有的 `ModifyCook` 委托上，但没有真的跑过打包。
- **车对车互撞时两台车各自会发一个爆点**（两边都是「被撞的一方」），接触点几乎重合。预期看起来是一记更大的爆点，但没有实测确认这是否过亮。

---

## 10. 这轮踩到的三个坑

1. **`pow` 指数小于 1 会把爆点变成雏菊**。第一版 `Sharpness = 0.55`，所有波瓣被推向 1.0，画出来是一朵花。现在预设 4.0。另外两条谐波必须用 `max()` 合并，相加平均会把谷底填平、尖角变钝。
2. **局部变量不能叫 `Config`**。表现组件已经有一个 `Config` 成员，本专案把 C4458（declaration hides class member）当成错误，5 处全部编译失败。组件方法里一律改用 `BurstConfig`。
3. **UE Python 读 bool 要去掉 `b` 前缀**：`bEnabled` 在 Python 端是 `enabled`，写 `b_enabled` 会报 `Failed to find property`。另外 `unreal.HALImpactBurstSettings` 这种模块属性不一定存在，用 `unreal.load_class(None, '/Script/<Module>.<Class>')` 稳。
