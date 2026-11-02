# GF-1：流程数据与权限基础

日期：2026-11-02，Asia/Tokyo。执行依据：[房间流程v0.2](Room_VehicleSelection_Practice_Match_Construction_CN_v0.2.md)。本环节仅建设数据、权威请求及权限基础；GF-2换车／新局生命周期、GF-3出生与UI、GF-4转场／载入／倒计时、GF-5表现及GF-6闭环仍分别实施。

## 1. 当前边界

新增车型目录、房间配置、玩家选车数据及公共房间快照，并接入既有GameMode／GameState／PlayerState／Controller。没有新增Tick或改写资产。GameMode的 **Room Config默认空**，旧NetTest及Host→Start行为继续保留。

**现在不是完整可玩的房间版本。** GF-1确认车辆后只进入PreparingAvatar，不生成车；未来GF-3完成生成／Possess后调用服务器CompleteAvatar。开始成功只冻结名单并进入Locked，实际ServerTravel及全员载入屏障在GF-4接入。练习不扣血已有权限查询，但统一HP结算策略明确留GF-2，尚未将其记为功能完成。不要将新Room Config直接指派到团队目前使用的回归GameMode来替代旧流程。

现有GameState的WaitingForPlayers／Playing保留给旧流程；新房间的公共阶段和每位玩家状态分开，不通过修改旧枚举让所有系统隐式认为已开赛。

## 2. 数据与归属

| 类型 | 职责 |
|---|---|
| UHALVehicleCatalog | 服务器车型白名单，稳定VehicleId→车辆BP软类引用，附可选展示名／图标；BP仍引用原Definition，无配置复制 |
| UHALRoomConfig | 显式Exhibition／Development规则及Catalog，未来由房间GameMode指定，不能根据PIE玩家数或打包状态推断 |
| FHALRoomPlayerData／AHALPlayerState | 席位、显示编号、颜色、所选ID、Selecting／PreparingAvatar／Ready、SelectionRevision和具体ReadyAvatar；ASC Owner继续是PlayerState |
| FHALRoomData／UHALRoomComponent | 公共阶段、Host、当前成员、冻结名单、RoundId与Revision；组件为GameState原生子对象，默认Inactive，快照复制 |
| AHALPlayerController | 自己的选车／确认／取消／Host开始请求与结果事件；客户端不传目标玩家或车辆类／资产路径 |
| HALGameFlow::Permissions | 游戏线程统一权限快照；驾驶阻断沿已有MovementEffectBridge进入权威物理时间线，物理线程不查询房间UObject |

公共阶段：Inactive → Open → Locked → Loading → Countdown → Playing，异常／离开可进入Returning。GF-1只提供服务器阶段推进契约，GF-4必须在其实际载入屏障及倒计时条件满足后调用，不能用固定延时冒充全员就绪。Returning的实际回房／入口处理后续实现，不自动迁移Host或悄悄减少展示人数。

展示模式最少 **4人**；开发模式最少 **1人**；两者最多 **4人，包含Host**。未启用Room Config时仍使用旧MinimumPlayersToStart／MaximumPlayers。新房间的开发配置允许Standalone权限路径；GF-1只能验证到名单锁定，单人完整转场仍待GF-4。

席位由服务器确定性分配最低空位，Number=Seat+1，颜色独立于车型。多个玩家可选择同一车型。取消确认保留本次席位／颜色／编号及车型偏好，递增选车版本；其他玩家离开不会重排席位。颜色目前是房间身份数据，尚未宣称完成材质／HUD／球权光束配色连接，该表现统一由GF-3／GF-5接入。

## 3. 请求、就绪与权限

客户端入口：`SelectRoomVehicle(VehicleId)`、`ConfirmRoomVehicle()`、`CancelRoomConfirmation()`及既有`RequestStartMatch()`。内部RPC仅携带动作、ID及本人的选车版本；服务器从RPC Controller验证PlayerState归属与房间成员关系。非法ID／无效目录配置、过期版本、非成员、错误阶段及非Host开始均被拒绝。客户端不能直接改PlayerState数据、宣布Avatar就绪或推进公共阶段。

选车必须在Selecting；确认只进入PreparingAvatar并递增SelectionRevision。服务器CompleteAvatar检查同一版本、所选精确类、实际Controller／Pawn／PlayerState关系、车辆配置及ASC Owner／Avatar和Health／Combat／Impact初始化。成功才进入Ready，并绑定当前具体Pawn；换了Pawn不能继承旧就绪。客户端所需输入／载入就绪的进一步屏障由GF-3／GF-4实现，GF-1服务器检查不等同于客户端全部完成加载。

开始只接受本地权威Host。服务器检查人数、所有当前参与者Ready且实际Avatar仍有效，然后一次性保存冻结名单、递增RoundId并切Locked；失败请求不锁房。名单保存玩家、席位、车型及确认版本，不能依赖遍历顺序或旧Pawn指针作跨图身份。锁定后拒绝加入、取消及改车；PostLogin再次检查席位以处理并发加入，PreLogin通过不是已保留席位。

结果用`GetLastRoomResult()`读取，`OnRoomRequestCompleted`事件通知UI。Room状态变化用`OnRoomChanged`，玩家变化用`OnRoomDataChanged`；GF-3可按事件刷新界面，不需建立第二份权威状态机。版本复制和结果事件有先后差异，客户端连续操作应等待最新状态，不依靠“刚点击过”猜服务器已经接受。

| 环境／阶段 | 驾驶／控球／技能／自救 | 手动观察 | 机关／标准命中结算 | HP伤害策略查询 |
|---|---|---|---|---|
| 旧NetTest，Room关闭 | 保留原规则；Waiting仍可驾驶／控球，技能等原先要求Playing的入口继续要求Playing | 保留原规则 | 保留原规则，电墙仍按旧Playing检查 | 保留原规则 |
| Room Open，未就绪 | 禁止；真实旧Pawn也不能因仍存在而取得资格 | 有受控Pawn时可观察，仍受UI／焦点门控 | 场地可运行，实际目标有效性仍由各系统检查 | false，消费接GF-2 |
| Room Open，Ready且同一Avatar | 允许，Stun／翻车／自救等既有条件继续独立生效 | 允许 | 允许练习相关交互 | false，消费接GF-2 |
| Locked／Loading／Countdown | 禁止 | 有受控Pawn时保留观察 | 禁止附加玩法结算，Chaos自然稳定过程保留 | false |
| Playing，冻结名单内同一Avatar | 允许 | 允许 | 允许 | true |
| Returning | 禁止 | 禁止 | 禁止 | false |

权限不是免疫、治疗或效果。新模式驾驶阻断与Stun合并到既有`bControlsBlocked`权威快照，在物理历史和重模拟中消费；不增加逐帧Ability或改变当前物理协议格式3。控球／球发射、两项正式GA、观察、自救、开发加速／Stun入口、电墙及统一命中阶段门控已消费对应查询。Damage查询将在GF-2接入统一结算，届时仍须验证“不扣血但冲击／脱球／命中消费继续”的独立性。

`HALRoomStatus`可在本地控制台输出公共阶段／Profile／人数门槛／Round、本人席位／编号／车型／玩家阶段／选车版本和最近结果。枚举值按HALRoomTypes.h顺序打印；Inactive是旧流程，不意味着连接失败。

## 4. 团队可准备的数据资产

完整重编后可以先创建数据资产，本阶段不接精细UI、不替换现用地图GameMode。

1. 在Content Browser建议新建`/Game/Data/GameFlow`。
2. 右键Miscellaneous → Data Asset，选 **HALVehicleCatalog**，命名如`DA_VehicleCatalog_Default`。
3. 在Vehicles中增加条目：稳定ID例如`Car_01`，Vehicle Class选择实际`BP_RoundedVehiclePawn`；另一个ID例如`Car_02`选择`BP_VehiclePawn_Car_02`。ID不使用展示中文名或资产路径，也不能以仅大小写不同的ID区分。
4. BP继续引用各自原VehicleDefinition。目录会检查具体可驾驶类、Definition来源／有效性、Convex驾驶碰撞及TieredV1迁移；任何条目不合法会拒绝房间配置，不能靠填展示名／图标绕过。Car_02如仍有旧Impact迁移问题，先按原迁移流程修正并保存，不修改公共规则来掩盖。
5. Display Name／Icon可先留空，元数据不影响车型驾驶有效性；预览模型／最终选车素材留GF-5按需要接入。
6. Validate Assets并保存目录。再新建两份 **HALRoomConfig**，例如`DA_Room_Exhibition`与`DA_Room_Development`，Catalog均指向上述目录，Rules.Profile分别选择Exhibition／Development。Validate并保存。
7. 当前旧测试GameMode的Room Config保持None。GF-3将给出专用房间地图、GameMode和出生／UI步骤；启用前需要GF-2生命周期及伤害策略。GF-1不要求新建房间／正式／过渡地图或编辑Level Blueprint。

所有资产由团队在Editor手工创建与验证，代理只提供C++和操作说明。软引用目录不等于已验证打包资源齐全，最终资源集合与发布包仍在GF-6核查。

## 5. 本轮验证与下一入口

基点`main/eadd240a67f6c3c83857ecf5011e22f29a145e8e`，本机UE **5.8.3-58210709**。**GF-1规格、代码、完整编译与程序验证完成；团队数据资产准备、真人界面／完整流程验收尚未完成。** 详情及跨设备保护基线见[本轮验证摘要](GF1_Verification_20261102.json)。

- 最终Editor／Game Development Win64完整构建成功。引擎头文件C4996、既有相机测试C4305及编译器版本建议警告保持，不宣称编译零警告。
- 最终普通回归 **114/114**，0测试警告／失败／未运行。其中GF-1新增6项，覆盖目录与配置、席位／颜色／重复车型／EndPlay清席位、选车版本和无效开始、真实Avatar就绪绑定及权限发布、冻结名单／阶段推进、原生夹具中的展示4人／开发1人门槛。没有把4个夹具Controller当作四台真实电脑。
- 真实独立进程Listen Host／Client各 **1/1**，0测试警告／失败。两端同一车型、不同复制席位／颜色；客户端使用自己的Controller RPC选车／确认、非Host开始被拒绝，服务器校验实际Pawn与ASC后Ready，Ready确实解除实际物理阻断；Host锁房后RoundId=1、冻结2人，两端实际物理快照再次阻断（Revision=4）。客户端直接调用修改／推进也被拒绝。
- 双端使用既有NetTest、NullRHI和服务器临时Development目录，未保存资产，没有新UI或跨图。当前地图先按旧流程正常生成车辆并Playing，再由夹具启用Room基础；它验证数据／权限与旧入口兼容，不是GF-3生产选车生车或GF-4全流程验收。
- **1177份保存资产逐份SHA-256全同**，Config及uproject未改。最终248份Source／Config／uproject及完整资产哈希纳入共享JSON。22份已有源码修改、12份新增源码／测试；开工已有上轮进度文档改动，保留后继续维护。本轮未创建提交／Push。

首轮Editor因Controller缺少PlayerState头文件失败，已补齐。首轮回归中，新夹具在物理尚未消费前误判下一份权限发布，并以同一数组内引用执行Add触发TArray断言，进程退出3且没有完整最终报告；修正为等待／检查发布合并规则、先复制条目，并加强双端真实物理解锁断言，R2的114项及双端均通过。之后另补PlayerState已EndPlay的席位清理与断言，最终重新两Target／114项／双端均通过。未忽略错误或修改生产物理时钟来适配测试，原始失败日志分开保留。

最终报告在`Saved/GameFlow/GF1_20261102/Final/Regression/index.json`及同目录的`Network_Host/index.json`、`Network_Client/index.json`；日志、脚本及首轮记录位于本轮Saved目录，均不入库。可交接基线与结论在共享JSON中保存，最终复验后没有继续修改C++，专用进程均已退出。

本轮完整Target构建包含现有FX源码，但NullRHI／数据检查不增加其真人或Niagara画面验收结论。没有执行手柄／鼠标UI、真正房间配置地图启动、单人完整转场、四台有线PC或实测RTT／丢包／抖动；这些按GF-3～GF-6及原F4分开记录。跨设备先同步本轮源码／文档及团队保存资产，统一补丁并重编，不混用旧RPC／DLL。

下一入口为 **GF-2：换车与新局GAS生命周期**。它需要明确区分首次初始化、重复绑定、换车及新局，清理旧Avatar作用域能力／效果／区域／球权及输入状态，重新应用正确车型种子；同时消费练习HP策略，验证A→B→A、重复调用幂等和命中／冲击独立。GF-1没有清除初始化保护或改写ASC数值来绕过这些待办。
