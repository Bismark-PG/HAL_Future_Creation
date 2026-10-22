# MVP-B 炸弹本体与生成测试指令

日期：2026-10-22。本机 main/fc9f2b9，UE5.8.2-56702186。

## 1. 当前范围

用户重新限定范围：道具框架、拾取方式及施放方式尚未决定，本次只保留实体炸弹及爆炸功能。此前试作的拾取Actor、单槽库存、道具GA、输入Tag、UseItem输入字段、HUD和HALUseBomb均已移除；车辆／Local Config／连续输入／网络物理脚本恢复本轮施工前内容。不要再按旧文档创建拾取物或道具输入资产。

- `AHALBombActor`：可直接放入场景的物理Actor／轻量蓝图prefab基类。首次有效地面接触后开始倒数，墙、车、普通球及其他炸弹不算地面；地面法线Z默认至少0.5，允许Movable地板。直接放在地面时启动后探测，弹跳不重新计时。
- `HALSpawnBomb`：开发测试指令，服务器在当前车辆前方生成一颗实体炸弹，**生成成功后立即开始倒数，不等待落地**。主机和客户端控制台均可使用，客户端只请求生成，不提供命中目标或伤害。
- 倒数结束后销毁炸弹实体，生成独立`AHALBombExplosion`，在爆炸瞬间对范围内车辆每车结算一次。显示范围默认保留0.5秒，不对后来进入者追加伤害。
- 可调测试默认：3游戏秒、球体半径30cm、爆炸半径500cm、固定伤害25、Heavy冲击、允许炸到自己。无距离衰减、墙体遮挡或连锁爆炸；这些不是已冻结的最终玩法规则。
- 范围按车辆根碰撞体的最近点距离判断；水平从爆心向外推，完全重合时用确定的世界+X方向。抬升／翻转使用现有档位表。

## 2. GAS与物理边界

炸弹、范围没有ASC，不新增炸弹技能GA。测试指令是开发工具，不代表未来道具使用架构。

`服务器爆炸范围查询 → FHALCombatAction／HitRequest → CombatResolver → GAS伤害GE＋HALImpactReceiver → 现有Authority物理动作`。

保留`Combat.Source.Bomb`来源Tag、固定配置伤害及生成时保存的使用者PlayerId。目标伤害倍率／伤害免疫由现有GAS即时处理；显式TieredV1冲击继续遵守GAS降档／冲击免疫、有效档位脱球及Medium／Heavy打断自救。不会直接修改HP、单独追加径向冲量，或给炸弹新增ASC。炸弹伤害不读取使用者AttackPower／输出Buff。

爆炸明确查询ECC_PhysicsBody简单碰撞，与现有车辆碰撞一致。倒数使用服务器定时器，客户端按复制的终点及服务器时间显示文本；没有新增Tick。

## 3. 最快测试

1. 重新编译并打开工程，在`MVPB_NetTest`中Play／建立Listen Server并控制车辆。
2. 控制台执行`HALSpawnBomb`，每次成功请求生成一颗。默认在车辆前方350cm、车体包围盒中心向上100cm生成；必要时自动保证车身包围盒＋炸弹半径＋25cm的间隙。方向取实际车头的水平投影。
3. 炸弹文本立即从3秒开始倒数；到期实体消失并出现橙色调试球。近处车辆默认扣25HP及Heavy冲击。测试指令可在Waiting使用，不要求比赛已开始，不赋予任何库存／技能。
4. 被墙或其他刚体阻挡时不强行生成；命令输出失败原因，可移动后重试。Shipping中不执行测试生成。
5. `Project Settings → Game → HAL Debug → Bomb`可选Default Bomb Class（默认原生HALBombActor，也可选团队制作的BP）、Bomb Forward Distance及Bomb Height Offset。请求使用服务器配置，客户端不能选择类或参数。无效类／配置不回退生成替代品。

原生炸弹不硬编码模型资产，默认有BOMB／倒数文本。正式可见模型须按下一节配置。

## 4. 团队prefab配置

依据AGENTS.md第8.3节「代码代理不得直接修改蓝图」，由团队手工操作，无需Event Graph。

1. 新建基于`HALBombExplosion`的蓝图，例如`BP_BombExplosion`。可附加无碰撞Mesh／Niagara表现。原生橙色球为开发调试显示，不是正式演出。
2. 新建基于`HALBombActor`的蓝图，例如`BP_Bomb`。给BombVisual指定模型／材质，保持无碰撞；实际碰撞由BombBody球形刚体提供。
3. Class Defaults → Bomb中设置Config（Fuse Seconds、Explosion Radius、Damage、Impact Tier及自伤开关），Explosion Class选上一步；必要时调整Minimum Ground Normal Z。不得用图表复制倒数／伤害规则。
4. 直接拖BP_Bomb进场用于落地计时测试；从高处放置应先落地再计时，直接贴地放置应由初始探测启动。
5. 若要用命令生成同一个prefab，将HAL Debug的Default Bomb Class选BP_Bomb。命令仍立即计时，使用该类配置的时长、伤害和档位。

**本次不创建拾取物、库存、道具Input Action、IMC映射或道具能力。**

## 5. 验收与证据

本轮删除道具后已重新完成UE5.8.2 Editor／Game Win64 Development构建；普通回归63/63、0测试警告／失败。其中炸弹3项为落地启动／销毁去重、GAS伤害／冲击免疫与降档、显式启动／重复启动拒绝及真实3秒定时器。报告Saved/Bomb/BodyOnlyRegression_20261022/index.json。此前道具测试已删除，63项并非沿用旧报告。

新的7808双进程、60FPS上限、正常游戏时间指令测试Host／Client各1/1，0测试警告／失败。客户端实际调用HALSpawnBomb，服务器只生成一颗并立即启动3秒倒数，双方观察计时、实体销毁与独立范围；近车100→75及Heavy，远车保持既有75，未把地图拳头先前扣血归给炸弹。报告Saved/Bomb/BodyOnlyHost_20261022/index.json及BodyOnlyClient_20261022/index.json；本轮没有人工网络扰动。启动时一次SDK平台检查碰到并行UBT锁提示，但最终两个Target及实际测试报告均成功。

团队仍需检查：场景高处落下／初始贴地、墙／车／球接触、斜坡／墙角、重复碰撞不重置；范围内多车／边缘／范围外、伤害0／None档／HP0；GAS两类免疫／降档和持球脱球；Listen Server双方命令、实际晚加入／扰动／四机及模型／倒数可读性。本轮自动化不代替真人画面与手感。

既有Car01启动缺少旧Decals1／Decals_21材质引用提示未在本轮处理。未修改Unreal资产或保存Config，未提交／Push。共享结果见[Bomb_Rebuild_Verification_20261022.json](Bomb_Rebuild_Verification_20261022.json)。