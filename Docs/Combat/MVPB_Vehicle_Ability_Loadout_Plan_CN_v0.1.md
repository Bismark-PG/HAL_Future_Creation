# 车型技能配置与授予方案 v0.1

日期：2026-11-04。方案核对基点：干净`main/1d1c0c2`。用户已确认Car01加速／Car02抗冲击并授权实施。本文保留设计基线；当前接口、迁移步骤与验证入口见[实现与配置](MVPB_Vehicle_Ability_Loadout_Implementation_CN_v0.1.md)。下节“当前限制”描述施工前状况。

## 1. 当前限制

`AHALPlayerState`持有ASC，原生构造器在`DefaultAbilities`中放入免疫Primary；团队BP可将它覆盖为加速。`GrantVehicleAbilities()`当前读取这份PlayerState数组，并以PlayerState作为SourceObject。不同车型使用同一PlayerState时，会得到同一份技能。

当前`VehicleDefinition`没有车型技能授予列表。不能仅通过已有Definition字段把Car01／Car02分配成不同技能。不要为每款车创建不同PlayerState、在输入里按车型名分支，或在车型BP的Event Graph授予／清除技能。

现有可复用基础：`FHALAbilityGrant`（GA类、InputTag、Level）、ASC按来源移除技能、普通重复授予去重、GF-2显式换车／新局及Avatar效果／冷却清理。

## 2. 推荐数据归属

在`UVehicleDefinition`新增 **Abilities** 配置（Version／Grants），复用`FHALAbilityGrant`。本期每个正式可选车型必须配置唯一的`Input.Ability.Primary`，Level默认1。直接配置GA类，GA继续引用自己的GE。

```text
车型目录 VehicleId
  → 车辆BP
    → Vehicle Definition
      → Abilities：GA Class / Input Tag / Level
        → GA自身的效果／冷却GE引用
```

本期不额外创建Ability Set／技能树／装备系统。已有GA／GE可以跨车型复用；以后若有多车型共享整套技能的实际需求，可把数组提取成DA，并保持配置读取接口不变。

`PlayerState.DefaultAbilities`仅保留显式Legacy用途，V1不读取或叠加它；其他来源授予须与车型输入槽互斥。旧序列化字段可保留用于兼容；正式Definition车型缺配时明确拒绝，不静默使用旧默认技能。Legacy测试路径如需保留，必须显式区分，不把它当成正式车型的回退。

## 3. 两款车的配置示例

下表为用户确认分配；保存DA仍由团队按配置文档手动迁移。

| 车型／Definition | Ability | Input Tag | Level |
|---|---|---|---:|
| Car01：`DA_Vehicle_Player_TestBaseline_v1` | `GA_MovementBoost` | `Input.Ability.Primary` | 1 |
| Car02：`DA_Vehicle_Car_02` | `GA_ImpactImmunity` | `Input.Ability.Primary` | 1 |

现有资产：`/Game/Blueprints/GA/GA_MovementBoost`、`/Game/Blueprints/GA/GA_ImpactImmunity`。效果／冷却仍由对应GA／GE控制，不需要重建两种技能逻辑。本期不改变加速叠层刷新、免疫规则或各自冷却。

两车继续使用同一`IA_PrimarySkill`及`Input.Ability.Primary`。InputTag表达“按了哪个技能槽”，GA类决定该车实际发动什么技能。当前只接通Primary；新增Secondary等输入仍需要对应输入命令／Enhanced Input／路由扩展，配置数组本身不代表额外按键已可用。

## 4. 服务器授予与生命周期

1. 服务器根据白名单车型BP取得Definition，预先校验车型与玩家通用授予的槽位／GA类／等级，不接受客户端指定GA类或GE。
2. 换车前先验证新车型配置，避免销毁旧车之后才发现缺配。
3. 沿用GF-2旧Avatar退休、球权／区域清理与现有属性／效果重置，再绑定新Avatar。
4. 从新Definition授予技能到同一个PlayerState ASC。**SourceObject使用当前车辆Pawn实例**，不要使用共享Definition资产或把车型技能继续当作PlayerState永久来源。
5. 旧Avatar退出时利用现有按来源清理，取消并移除属于该Pawn的技能。延迟到达的旧车清理不能撤销新车授予。
6. 客户端通过GAS复制得到Spec；继续使用现有技能请求管线，服务器检查当前Avatar／阶段／Stun等规则。

车型授予成功与输入槽唯一性应进入Avatar准备完成检查。此处检查“技能是否正确安装”，不要求`CanActivateAbility=true`；冷却、资源或暂时Stun不能被误判为配置未就绪。房间跨图屏障也需考虑拥有者端收到正确授予。

同一个Pawn的重复Possess／OnRep／绑定必须幂等，不重复授予、不重置冷却、不回血。显式换车／新局沿用GF-2当前已定的Avatar Buff与冷却重置策略，普通重绑继续保留正在运行的计时。

## 5. 校验与错误处理

- 正式车型必须有唯一Primary；拒绝空GA、抽象／失效类、无效InputTag及Level<1。
- 同一列表及玩家通用／车型来源之间均检查输入槽冲突。当前ASC激活管线要求一个输入匹配恰好一个Spec，不能把两个Primary同时塞进去。
- 授予批次先整体校验，不能部分授予后仍报告Ready；失败返回可读配置原因。
- 保留车型GA自己的GE与发动条件验证。通用授予校验不假定所有未来GA都必须使用同一种GE。
- 缺配不静默落回PlayerState的旧加速／免疫Primary。
- 输出可关闭日志：PlayerId、VehicleId／Definition、Avatar、GA类、InputTag、授予／移除来源与失败原因。

## 6. 接口完成后的Editor迁移顺序

**接口已进入实施，完整编译后按实现文档迁移。显式Legacy默认兼容已有DA；正式车型设为Vehicle Definition V1。**

1. 保存并关闭Editor／Live Coding，完整编译新字段后重新打开工程。
2. 打开Car01、Car02各自的Vehicle Definition，将Abilities.Version设为Vehicle Definition V1，在Grants中按第3节填GA／InputTag／Level，并保存。
3. 从实际使用的`BP_HALPlayerState.DefaultAbilities`移除旧车型Primary；原生构造器默认Primary也须同步取消或限制在显式Legacy路径。仍被Legacy车辆使用时可暂留，V1完全忽略该列表；其他来源的合法授予独立处理。
4. 不修改车辆主技能IMC／IA，不为车型切换PlayerState Class；核对两份车型目录引用仍指向正确BP／Definition。
5. 校验全部正式车型后再开新房间。迁移期间缺配应报明确错误，不能给某一车型悄悄补错技能。

未来新增车型沿用“车辆BP引用Definition，在Definition填技能列表”的同一流程。新车使用现有GA时不需要新增C++车型判断；仅新的技能行为需要新的C++能力逻辑或已有原生GA的轻量配置子类。

## 7. 选车UI与打包

选车页将来从同一Definition读取技能配置，避免在Widget或Catalog再维护一份独立GA映射。静态名称／图标／说明可增加独立只读展示数据；比赛中实际效果与冷却从当前ASC获取，不靠选车页模拟。当前不扩展复杂UI或技能预览演出。

采用可追踪的资产引用链`Catalog→BP→Definition→GA→GE`。更新现有只读打包预检，核查两种车型的GA／GE都在依赖图中；实际Cook／发布包仍须运行验证。

## 8. 必须完成的验证

| 场景 | 通过条件 |
|---|---|
| 两端不同车型 | 各自Primary只发动对应技能，伤害／冲击免疫保持原边界 |
| 两端相同车型 | 各有独立Spec、Buff和冷却，不共享运行状态 |
| Car01→Car02→Car01 | 旧技能消失、新技能唯一，已有生命周期清理符合GF-2 |
| 同Pawn重复绑定 | 授予不增加、冷却／效果计时不刷新 |
| 取消确认／销毁／迟到清理 | 不遗留旧车技能、不移除新车技能 |
| 连续两局／跨图 | 使用当前车型重新授予，Ready屏障正确，旧效果／冷却按原规则清理 |
| 配置缺失／重复Primary | 拒绝就绪并给出明确原因，不能双技能或无声失效 |
| 既有回归／发布包 | 迁移测试夹具后回归通过；GA／GE依赖及实际包验证完整 |

施工结果以实现文档与本轮验证JSON为准；此前132项及手感调整验证保留原时点，不冒充车型技能验证。
