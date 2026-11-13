# Car03：柔和卡通火花与立体气流

**状态：2026-11-14（日本时间）程序、保存资源、技能 DA 迁移与本机专项验证完成，待团队风格验收。** 从 `main/69a16d5` 接续，使用 UE5.8.3-58210709；本轮未提交／Push。

## 本轮范围

承接[上一轮接触飞溅与风格讨论](Car03_BattleTop_Polish_FX_CN_v0.1.md)，保持既定“夸张卡通、颜色与轮廓较柔和、圆润”的量产纲领。接触粒子强度已经得到用户认可，本轮保留保存配方的 Contact Count=7、尺寸／寿命／速度／散射／视觉重力／阻力，只换造型和分色。Ground Sparks=None 也保留。

火花改为有弧度的圆头短笔触，少量圆点／小菱形作变化，使用概括的三档颜色和较低发光，去掉写实热白芯。气流使用 Niagara 曲面 Mesh 粒子：每条开放弧有真实横截面厚度、不同高度／倾角／起始朝向、独立寿命和上升运动，避免单个平面图案整体随车身自转。

这些是表现修改。技能许可、持续时间、碰撞分类、伤害、每目标 CD、推送、甩球、眩晕／自救及清理规则沿用原实现。车身 Spin Rate 与气流 Wind Flow Degrees Per Second 是两个独立参数。

## 资源与引用

五个新资源位于 `/Game/Assets/FX/BattleTop`：

| 资源 | 用途 |
|---|---|
| `NS_BattleTop_ToonWind` | 本地空间的持续气流 Mesh 粒子 |
| `SM_BattleTop_WindArc` | 110° 开放弧，平滑法线、有高度的椭圆横截面、无碰撞 |
| `M_BattleTop_ToonWind` | Unlit，按表面朝向切换暗／中／亮三档暖色，并控制粒子生灭 |
| `NS_BattleTop_ToonContact` | 复制原 ContactSpray 的运动配方，替换卡通材质 |
| `M_BattleTop_ToonContact` | 圆润笔触轮廓、概括分色与短暂出现／缩小 |

`DA_Ability_Car03_BattleTop → Presentation → FX` 使用：Wind=`NS_BattleTop_ToonWind`，Wind Material=`M_BattleTop_ToonWind`，Contact Sparks=`NS_BattleTop_ToonContact`。旧 `NS_BattleTop_Wind`、`M_BattleTop_Wind`、`NS_BattleTop_ContactSpray` 和旧火花均保留，可以手动切回。

代码通过现有表现组件创建独立 Niagara Component 和 MID，不需要增加 BP 图节点。车辆各自持有材质实例，技能 DA 仍是颜色和运行时参数的来源；直接修改母材质同名参数的默认值会被 DA 覆盖。

## 调参入口

重开 Editor 后，进入上述技能 DA，展开 **Presentation → FX**。不同 Editor 的嵌套分类可能平铺显示；可搜索字段英文名。

| 字段 | 新增默认值／保留值 | 调整效果 |
|---|---|---|
| Wind Flow Degrees Per Second | 240°/s | 气流推进速度，可设负数反向；不影响车身自转／碰撞 |
| Wind Band Count | 5 | 稳态近似弧段数量，1～8；优先调少以保留车辆和球的可读性 |
| Wind Band Lifetime | 1.2 s | 每条弧的寿命；发射率自动按数量／寿命计算 |
| Wind Layer Height | 45 cm | 出生高度的错落范围；不是整体抬高 |
| Wind Tilt Degrees | 12° | 弧段初始倾斜幅度，0～30° |
| Wind Band Width Scale | 1 | 弧带径向厚度，0.35～2.5；不等于半径或整体尺寸 |
| Wind Rise Speed | 18 cm/s | 弧段生命周期中的局部向上运动 |
| Wind Glow Strength | 0.8 | 气流色彩亮度，0～3；建议先保持在 1 附近或以下 |
| Contact Glow Strength | 1 | 接触火花色彩亮度，0～3；无需靠高白光显示轮廓 |
| Main／Accent／Highlight Color | 保留团队暖色 | 主体中间色、暗部基色、亮部色；暗部会适度压暗 |
| Radius Scale | 保留 1.45 | 依据车辆真实外观 Bounds 调整气流覆盖范围 |
| Opacity | 保留 0.55 | 气流透明度，建议先试 0.55～0.8 |
| Contact Count | 保留 7 | 已验收的发射数量，本轮未强制改成旧建议的 32 |
| Ground Sparks | 保留 None | 本轮没有重新开启底部火花 |

建议先调 Flow、Layer Height、Tilt、Width，最后调颜色和亮度。高度／倾角／数量／寿命属于新粒子出生配置；游戏运行时临时修改后，旧粒子需要过一轮寿命才全部更新，重新释放技能可获得一致对照。

## 制作约束

气流材质仍为 Unlit，但方向分色使用网格法线，能表达真实曲面朝向。它不依赖场景灯光，也不是贴图里的固定明暗渐变。弧带中心留空，出生／结束淡出，不使用完整封闭实心圆盘覆盖车辆。

气流使用外表面单面渲染，避免透明弧带的内外表面叠显产生条纹；接触 Sprite 仍为双面。制作新 Mesh 时要核对表面绕序、法线与第 0 个 Mesh 槽，不能在 Niagara 自动创建的空槽后再追加实际网格。

Niagara User 参数由 C++ 表现组件提供。继续制作资源时保留 `User.WindMaterial`、`User.SpawnRate`、`User.Lifetime`、`User.YawRate`、`User.ScaleMin/Max`、`User.PositionMin/Max`、`User.RotationMin/Max`、`User.Velocity`。接触系统保留原 BurstCount／VelocityMin/Max／SizeMin/Max／LifetimeMin/Max／Gravity／Drag／SparkColor，并使用新增 `User.StyleParameters.x` 传入 Contact Glow Strength。

材质的 `Envelope` 由原技能过渡控制，必须保留；`BandWidthScale` 的 WPO 依赖弧网格 VertexColor RG 编码，换网格时不能随意删除顶点色。方向分色遵循曲面法线，不需要真实阴影或光照。旧 `SpinAngle` 接口仍供旧风环使用，新气流不读取它。

可追踪配方为 Editor 模块 `HALBuildBattleTopCartoonFXCommandlet`。新资源创建入口使用 `-run=HAL_Future_CreationEditor.HALBuildBattleTopCartoonFX`，需要 `-AllowCommandletRendering`。同名资源已存在时拒绝覆盖，**正常使用无需重新运行**。显式 `-Apply -SkillAsset=...` 仅迁移三项资源引用，并逐字段确认其他保存配置一致；不编辑 Blueprint。维护用 `-RefreshCompiledData` 仅完整刷新两份新 Niagara 的编译数据，不重建造型或改变 DA。

复制 Niagara 后新增脚本参数／模块时，生成器调用 PostEditChange、强制完整编译并等待完成再保存。仅在 Editor 渲染测试中临时编译成功，不能证明独立 Game 加载的保存副本已就绪；本轮额外使用全新双进程验证了这一点。

## 验证与团队验收

本机 UE **5.8.3-58210709**：最终 **Editor R13／Game R4** 编译链接成功。最终统一专项 **Final_R3：16/16**，0 测试警告／失败／未运行／进行中，包含 BattleTop、技能授予、战斗结果和三项 D3D12 渲染。新资源检查了默认 Mesh 槽、真实横截面、持续粒子、宽度、层高、独立流速、忽略车身 SpinAngle、退出隐藏和七粒子自然消失；实际输出气流 10928 个暖色像素、接触笔触 1681 个，已查看图片。

最终 **Network_R6**：当前 NetTest 地图的 D3D12 双进程，Host 出站额外 80 ms；Host／Client 各 **1/1**，报告无测试警告／失败。使用客户端实际技能请求、拥有者／观察者参数复制、原 ObserveContact 入口的合成权威接触、真实 cosmetic multicast、数量7、技能到期归位／粒子清理与冷却。本轮没有通过改变物理碰撞／伤害资格使测试通过。合成接触不代替真人撞墙、持续擦碰及四人比赛验收。

早期 Render_R1～R3 发现空默认 Mesh 槽；单面绘制后修正绕序以匹配外表面法线。Network_R1～R4 发现复制的 Contact 系统在独立 Game 未就绪，完整编译刷新后修复；Network_R5 用临时隔离 RPC 诊断通过，最终 Network_R6 已移除该临时入口并恢复原 ObserveContact。失败记录保留在 Saved，不冒充通过。

保存资产／Config／uproject 共 **1277** 份开工基线，只有授权的技能 DA 改变，其余 **1276** 份逐份哈希一致。DA 在保存前经过全部反射字段比较，保存后重新加载数值核对：只改 Wind、Wind Material、Contact Sparks，Contact Count=7、Ground=None 和原尺寸／寿命／速度等保留。原有 368 份源码只有两份表现源码和三份相关测试发生改变；车辆移动、BattleTop 玩法组件、GA、碰撞规则、BP 和地图未被代理修改。

日志、原始报告、备份及基线位于 `Saved/BattleTop/Cartoon_20261113`（不入 Git）。便于跨设备核对的摘要见[本轮验证记录](Car03_BattleTop_Cartoon_Verification_20261113.json)。历史首版 UE5.8.2 的 16/16 不作为本轮证据。没有执行四机／发布包测试或全量历史套件。

下图是本轮实际地图、实际车辆的 D3D12 截图；是测试夹具的瞬间画面，最终配色／密度／遮挡仍由团队验收。

![Car03 卡通气流与接触火花实际截图](../../SourceArt/BattleTop/Car03_BattleTop_Cartoon_D3D12_Host_20261113.png)

团队重点看：静止／驾驶中气流是否都有前后高低层次；不同方向撞墙时笔触是否清晰；自救／眩晕／到期／淘汰后是否按既定规则暂停或清理；低透明度和高透明度下是否遮住球／车辆中心。程序像素检查不替代最终风格与手感验收。
