# 车型主技能：实现、配置与迁移 v0.1

日期：2026-11-04。用户已确认 **Car01加速、Car02抗冲击**，两车共用原主技能按键。由`main/1d1c0c2`开始实现；此前重力、墙脱球门槛等内容已在该提交。本文件描述本次正式接口，构建与测试证据见文末。

## 1. 正式配置入口

```text
Catalog.VehicleId → Vehicle BP → Vehicle Definition.Abilities
  → Grants[0]：Ability / Input Tag / Level
    → 现有 GA → 现有 GE（效果与冷却）
```

`Vehicle Definition → Abilities`增加显式版本：

| Version | 行为 |
|---|---|
| Legacy Player State | 保留旧PlayerState.DefaultAbilities，Grants必须为空；仅供未迁移资产兼容 |
| Vehicle Definition V1 | 只使用本DA的Grants；当前严格要求唯一Primary；空列表、重复槽或非法GA／等级拒绝就绪，不回退旧配置 |

旧DA读取新增字段时默认Legacy，避免编译后未配置就破坏已有测试场景。**正式车型请主动迁移至V1。** 旧DefaultAbilities是Legacy专用列表，不是自动叠加的通用技能。当前没有新增Secondary槽、技能树、装备框架或UI布局。

## 2. Editor逐步配置

1. 使用完整编译后的Editor打开工程。不要通过Live Coding补反射字段。
2. 从当前Catalog确认Car_01／Car_02对应的车辆BP，再在各BP的Class Defaults查看`Definition`引用；应修改各自的Definition DA。不要修改车辆输入、PlayerState Class或Event Graph。
3. 打开Car01的`DA_Vehicle_Player_TestBaseline_v1`，展开`Abilities`，`Version`改为 **Vehicle Definition V1**。
4. `Grants`添加且仅添加一个元素，按下表填写。Ability选择现有GA蓝图类，Input Tag在标签选择器里选择Primary，Level为1。
5. 对Car02的`DA_Vehicle_Car_02`重复操作，引用对应免疫GA。

| 车型 | Ability | Input Tag | Level |
|---|---|---|---:|
| Car01 | `GA_MovementBoost` | `Input.Ability.Primary` | 1 |
| Car02 | `GA_ImpactImmunity` | `Input.Ability.Primary` | 1 |

GA资产位于`/Game/Blueprints/GA/`。不需要复制GA／GE，也不需要重新配置技能内部的效果、持续时间、叠层刷新或冷却。本次保持其已有设置；加速仍按已有GE运作，免疫仍只影响冲击规则，不因此获得伤害免疫。

6. 保存两份DA，执行资产Validate。若显示Legacy列表不能包含Grants，检查Version是否已切换。若缺Primary／重复／GA为空，请修正后再进入房间。
7. 两车全部迁移完成后，可从实际`BP_HALPlayerState → GAS → Legacy Abilities → Default Abilities`移除旧车型Primary，防止团队后续误以为这里仍控制正式车型。**V1完全忽略此列表，清空不是V1生效的前提。** 若还有旧Legacy测试车辆依赖它，暂时保留；不要为了迁移误删其他测试的Legacy配置。原生构造器的免疫默认值同样只服务Legacy。
8. 结束旧PIE／游戏会话，再开新房间测试。每辆Pawn初始化时复制一份能力配置，本次未提供运行中DA热重载；编辑后重开会话才能保证服务器与拥有者一致。

如DA实际已改名，以车辆BP的真实Definition引用为准。代理本次不修改或保存BP／DA／地图，以上步骤由团队手动完成。

## 3. 生命周期与联机边界

- ASC继续由PlayerState持有。服务器从白名单车型的Definition读取授予，客户端不能指定GA类。
- Spec的SourceObject使用**当前车辆Pawn实例**。即使两人使用同一DA，Spec、活动效果和冷却也分别属于各自ASC。
- 换车前整体校验配置和输入槽，失败时保留旧车；授予也先整体校验，避免出现半份技能但仍Ready。
- 显式换车／新局沿用GF-2属性与Avatar效果／冷却清理规则；普通重复绑定不会重新发技能或刷新计时。
- 旧Pawn退休、取消确认或销毁只清理其自身来源。迟到的旧Pawn清理不会清除新Pawn技能／ActorInfo。
- 服务器AvatarReady及拥有者跨图Ready会检查正确技能Spec及其来源；技能在冷却或暂时不能发动不等于配置未就绪。
- 同一Input Tag跨来源冲突会报错；当前正式车型只支持一个Primary。未来增加按键须扩展现有输入命令与路由，不能只给列表再填一个Tag。

## 4. 检查与日志

测试时在控制台执行`hal.AbilityLog 1`，结束后`hal.AbilityLog 0`。日志包含授予／移除来源、GA／输入槽／等级、PlayerId、Avatar和Definition。授予失败与配置拒绝会输出Error，即使关闭普通日志也能定位。

建议团队验收：

1. Host选Car01、Client选Car02：同一按键分别发动加速／免疫；另一端观察表现和状态正常。
2. 两人都选Car01，再都选Car02：发动一人的技能不应启动另一人的效果或冷却。
3. 取消确认并换车Car01→Car02→Car01：只有当前车型技能，旧技能不能继续发动。
4. 连续开始两局并返回房间：跨图后技能仍与车型一致，旧效果／冷却按GF-2规则清理。
5. 临时复制DA做缺失／重复Primary试验：应明确拒绝初始化，不能悄悄得到旧技能。测试后恢复合法DA。

打包只读预检`Tools/InspectGameFlowPackaging.py`会列出车型版本／授予和迁移待办；添加`--require-vehicle-abilities`会将仍使用Legacy的Catalog车型作为失败。它沿序列化依赖图检查GA包；报告是依赖预检，不是实际Cook或发布包运行通过。当前保存DA尚未迁移时，严格模式失败属于预期。

## 5. 验证记录

本机UE **5.8.3-58210709** 最终Editor／Game构建成功；原生回归 **134/134**，0测试警告／失败／未运行。新增测试覆盖V1缺配／重复／无效等级、整体授予、跨来源冲突、Car01→Car02→Car01、普通重绑不刷新冷却、同车型不同玩家状态独立、销毁及迟到清理。

真实Listen Server双进程两套运行：

| 配置 | 结果与范围 |
|---|---|
| V1临时夹具 | Host用实际Car01 BP／加速GA，Client用实际Car02 BP／免疫GA；两端连续两局分别通过普通Primary输入／GAS RPC发动对应技能，检查唯一Spec／当前Pawn来源、拥有者冷却、其他车型效果不混入及跨图清理；各端1/1 |
| 原保存Legacy | 不覆盖Definition的现有保存配置，原GF-4连续两局／返回Entry；各端1/1，兼容路径保留 |

四份联机报告均0测试警告／失败／未运行。V1主机出站注入80ms，60FPS上限、NullRHI、同机测试；不代表实测RTT或真人画面。双端运行后仅适配一处旧加速保护测试的绑定调用，正式玩法源码与最终构建逐份一致，因此未重复已通过的联机流程。

最终只读预检通过，严格模式正确拒绝两份尚未迁移的DA（两个Room目录引用合计四条诊断）；Python原生GA／InputTag／Level读取检查通过。初轮测试夹具Tag类型编译失败、130/134及133/134的旧回归报告独立保留。已补直接ActorInfo替换时的旧来源清理；旧夹具不再在Pawn出生前授予，恢复Avatar改用正式绑定入口并增加清理／重授予断言，没有移除原激活断言。

**1183份保存资产逐份SHA-256全同；289份最终源码／Config／uproject构建输入冻结核对，7份Config／uproject／Build.cs均保持。** 专用Editor／游戏／Live Coding进程已退出，未提交／Push。详见[验证与保护清单](MVPB_Vehicle_Ability_Loadout_Verification_20261104.json)。

独立测试夹具仅修改新生成Pawn实例及Transient DA副本，不修改BP／CDO或保存资产。当前两份保存DA仍Legacy；团队需完成第2节迁移后进行真人技能／视觉验收。实际Cook／发布包和四台PC验收未新增结论。
