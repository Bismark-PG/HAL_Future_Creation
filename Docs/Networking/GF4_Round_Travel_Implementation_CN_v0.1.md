# GF-4：真实跨图、全员就绪、同步倒计时与恢复

日期：2026-11-03，Asia/Tokyo。依据[确认基线v0.2](Room_VehicleSelection_Practice_Match_Construction_CN_v0.2.md)，接续`main/2e7d72c826abf26c33da4b688abccf63aba8e182`及[GF-3](GF3_Practice_Room_Implementation_CN_v0.1.md)。

用户报告GF-3“经过测试暂无问题”，本轮记为团队基础功能初测正向反馈；没有收到逐项／网络工况／四机日志，不补写这些细项。开工保留团队已修改的`BP_MVPB_NetworkGameInstance`、新建的`BP_MVPB_PracticeRoomGameMode`和`MVPB_PracticeRoom`；只读核对Room BP使用HALPlayerController、BP_HALPlayerState、HALMatchGameState及DA_Room_Development，GI Host目标已指向新房间。

## 1. 本阶段交付和边界

启用DA中的Round Travel后，Host开始的实际路径：

```text
房间内全员确认并完成车辆初始化
→ 服务器校验目标地图／人数／实际Avatar
→ 冻结本局名单、车型及RoundId，关闭加入／改车
→ 退休并销毁练习车，清球权／输入／旧绑定
→ ServerTravel + Seamless Travel进入另一地图
→ 还原身份、生成同车型新车、显式BeginNewRoundAvatar
→ 等待每位玩家的本局客户端就绪
→ 共享服务器倒计时结束时刻
→ Playing，开放驾驶／技能／球权及正常HP伤害
```

场景车和球在目标World重新建立，不搬运练习刚体或其速度／运动历史。新局满血并重新应用对应车型种子，清练习Buff／Debuff及冷却；旧物理协议仍为3。每位玩家继续使用自己的本地跟随相机，观察输入和积分重新建立。

本阶段增加开发用Host返回房间入口，以验证连续两次开局；**不实现五分钟计时、淘汰、观战、胜负／结算**。GF-5仍负责正式街机入口／选车预览／视觉及完整硬件焦点体验，GF-6负责部署／四机与组合闭环。

Round Travel默认关闭，保留旧GF-3锁房终点及NetTest回归兼容。按第4节配置后才实际跨图。

## 2. 代码职责及跨World身份

### 2.1 服务器规则与一次性交接

继续复用`AHALPracticeRoomGameMode`，由一次性交接数据区分房间World与正式World，跨图部分拆在`HALPracticeRoomTravel.cpp`。目标URL显式使用当前房间GameMode BP类，因此Controller／PlayerState类及团队已配Primary技能保持一致；不用另建一套角色技能或规则BP。

`FHALRoundTravel`只传值及配置资产：FlowId、本次RoundId、Host的PlayerKey、成员身份／车型／选车版本、生命周期计数、源房间路径、目标路径和失败原因。**不携带旧World、Controller、PlayerState或Pawn引用。** 每位玩家以服务器分配的PlayerKey识别，颜色／编号与车型分开，允许重复车型。

GameInstance只保管并按目标地图消费这份一次性交接，不负责比赛阶段或玩家准备判定；可复制的当前状态仍在新GameState的Room组件。开局时复制配置到GameInstance所属的临时对象，冻结本房间人数规则及本次转场参数，避免持有旧World或在载入中途变更本局规则。

PlayerState补`CopyProperties`，应对引擎Seamless Travel更换Controller／PlayerState的行为；保留身份与生命周期序号，清旧Avatar引用。新World按可信交接恢复计数，再由GF-2显式入口重新初始化。普通Possess／OnRep仍不会自动回血或清冷却。

RoundId在返回房间后保持，下一次开始递增；FlowId在本次房间会话内保持。返回房间保留车型偏好、席位、颜色和编号，撤销确认并递增选车版本，不能用上一局消息自动确认或启动新局。

### 2.2 客户端载入屏障

服务器先完成实际新Pawn／ASC／Health／Combat／Impact初始化，再发出属于该Avatar的LoadToken。客户端Controller的本地定时器等待：

- 新Pawn由本人控制、PlayerState及ASC Owner／Avatar匹配；
- 本局Lifecycle.RoundId及Avatar匹配；
- 输入组件已经建立；
- Health及各初始化标记就绪；
- 新物理快照就绪，Epoch有效。

随后通过本人的Controller上报FlowId、RoundId、LoadToken和实际Avatar。服务器同时验证本连接已经载入当前World、所有身份与实际初始化条件；错误World／旧Round／旧Token／其他Pawn均不会计入。重复成功确认幂等，没有第二个玩家“准备按钮”。

新局名单中的所有成员都完成服务器初始化及本局客户端确认，才进入Countdown。没有用固定等几秒替代载入检查，也不会少一个人就自动缩小展示局继续开赛。客户端确认仍不是对GPU着色器／所有美术渲染完成的承诺。

Controller更换期间，本地Player／拥有连接可能晚于Actor出现。UI、就绪上报及原有Controller Tick等待有效本地拥有关系后运行，避免无Player创建Widget或向空连接发送引擎物理时间戳RPC。没有修改引擎源码或网络物理数据格式。

### 2.3 同步倒计时及权限

服务器设置一个`CountdownEndServerTime`，客户端以GameState服务器时间显示剩余秒数。只有服务器到时才能切Playing；倒计时期间保持驾驶、技能、捕球／发射和危险机关结算关闭，允许必要的物理稳定和本地视角。

Playing同时更新Room阶段及旧MatchPhase，恢复正常伤害；练习阶段仍按GF-2独立禁止扣血而保留冲击／命中消费，不以永久免疫GE代替。

## 3. 参数及失败恢复

在两份`HALRoomConfig` DA的 **Round Travel** 分类配置：

| 参数 | 默认值／范围 | 用途 |
|---|---|---|
| Enable Round Travel | false | 完成目标关卡配置后启用；关闭时保留GF-3锁房终点 |
| Match Map | 空 | 选择实际正式地图，必须存在且与当前房间地图不同 |
| Countdown Seconds | 3秒，0.5～10秒 | 全员就绪后倒计时 |
| Load Timeout Seconds | 45秒，1～180秒 | 从接受开始／发起转场起计算服务器等待上限 |

配置在新建房间／开局时读取并进入交接快照。修改后保存，重新Host测试；本阶段不增加运行中热重载。超时使用服务器单调墙钟判定，包含地图载入和客户端状态收敛；引擎阻塞载入期间不能抢占磁盘操作，服务器恢复Tick后会执行超时处理。

| 情况 | 行为 |
|---|---|
| 目标缺失／目标等于房间／配置无效 | 开始预检拒绝，不撤销原练习车或误锁房 |
| 未允许PIE无缝转场或GameMode禁用Seamless Travel | 预检拒绝；按第5节使用支持的启动方式 |
| 正式出生失败、载入超时 | 返回源房间，保留连接及身份／车型，撤销确认，显示原因 |
| 非Host在载入／倒计时／正式阶段离开 | 本次流程取消，剩余成员返回房间重新确认；不替换Host或少人继续本局 |
| Host离开 | 其他玩家返回本地入口并显示原因，无主机迁移 |
| 引擎转场／网络失败，无法维持一致房间 | 回本地入口并保留可读原因；不无限等待或重试 |

回入口用GameInstance的跨World调度，不把恢复请求只挂在即将销毁的World定时器上。返回房间后允许重新加入和确认；展示Profile仍要求四名实际参与者，开发Profile允许一名。

## 4. 团队Editor配置步骤

代理没有改写或保存BP／DA／地图。已有GF-3房间、输入和技能配置继续使用。

1. 完整编译并重开Editor，保留新建的`MVPB_PracticeRoom`、`BP_MVPB_PracticeRoomGameMode`和GI引用改动。
2. 复制当前有效测试竞技场为独立正式地图，例如 **`/Game/Maps/MVPB_Match`**。首版地形可以和练习房相同，但应保存为另一个地图文件；不要将Match Map指回PracticeRoom。
3. 正式图检查HALPlayerStart的Spawn Slot 0～3，不能重复；位置要容纳两种车的真实物理根。保留正确配置的球、主球相机标记、边界与机关；场景展示车关闭Auto Possess Player。
4. 正式图可使用同一PracticeRoom GameMode BP，运行时由服务器交接和URL决定正式阶段。原NetTest的保存GameMode／Room Config保持旧回归用途，不必改它。
5. 打开 **DA_Room_Development** → Round Travel，Match Map选择新正式地图，Enable Round Travel勾选，Countdown Seconds先保持3，Load Timeout Seconds先保持45。Validate并保存。
6. **DA_Room_Exhibition**指向同一正式地图并启用；Rules.Profile继续Exhibition，开发DA继续Development。不要靠PIE Number of Players修改人数门槛。
7. 房间GameMode BP确认父类HALPracticeRoomGameMode，Room Config指向所测DA，PlayerState Class仍为 **BP_HALPlayerState**，Controller／GameState仍为原HAL类；Use Seamless Travel保持开启。无需新建GA／GE、输入或另一套PlayerState。
8. GI的Match Map Path仍指 **PracticeRoom**，不是正式地图。玩家始终先从入口Host／Join进房间，再由Host开始统一进入正式图。
9. 可暂不填写Project Settings → Maps & Modes → Transition Map，引擎会使用临时过渡World。需要专用加载场景时由团队手工建立空Transition Map并配置，不在里面放正式车辆／球或Level Blueprint玩法。
10. 保存地图、Room DA及BP，然后按第5节从入口测试。所有联机端同步相同源码、资产与引擎补丁并完整重编。

直接打开正式图Play不代表已经完成本局交接；本轮验收应从房间开始。旧NetTest仍可直接启动作既有回归。打包地图集合、软引用资源部署和四机将在GF-6统一验证，本轮没有打包通过结论。

## 5. 操作与测试清单

1. 开发单人从NetEntry → Host → 选车确认 → Host: Start，检查真正进入正式地图、倒计时，然后可驾驶／释放技能／正常受伤。
2. 双端在房间选不同车型，再测同车型；确认颜色／编号／车型跨图保持，各自相机与输入正确。
3. 开始前产生技能效果、冷却、速度或球权；正式图应满血、技能可用、无旧球权／效果、无旧运动和相机输入残留。原生测试有清理证据，实际持球／硬件输入仍应实测。
4. Loading与Countdown期间持续按油门／技能／发射，不能提前驱动正式规则；倒计时结束后再验证输入。不要只以UI按钮灰色判断服务器禁控。
5. 开发Profile的正式阶段打开房间菜单，Host可点 **Host: Return to room**，也可使用`HALReturnToRoom`。它用于开发验证，无正式胜负／结算含义；Exhibition界面不提供此按钮。
6. 返回房间后重新确认，再开始第二局；检查RoundId递增、车型偏好保留、HP／效果／球权／历史重新建立。客户端用旧确认或就绪消息不能提前开始下一局。
7. 分别在等待就绪时让Client离房、Host离房，检查对应恢复；再补真实慢机、断网和进程突然终止工况。受控离房通过不等于突然断线／硬件四机全部验收。
8. 需要调试时用`HALRoomStatus`、`HALGASStatus`、`HALNetMetrics`。旧MatchPhase只在Playing更新，加载与倒计时看Room状态。

推荐使用独立`-game`进程或Standalone／Listen测试。PIE需先允许`net.AllowPIESeamlessTravel 1`并检查引擎无缝转场限制；默认不允许时会拒绝开始，避免悄悄退化成丢失本局身份的硬重连。本轮主要证据来自独立进程，不将其标成PIE全验收。

开发调试可在Client控制台设`hal.RoundReadyDelay 2`，仅延迟本地已经就绪后的确认；恢复设0。若高于服务器载入超时，应返回房间。它是测试等待屏障的工具，不模拟真实磁盘载入速度或实测RTT。

### 不改保存DA的临时启动方式

服务器启动URL可覆盖本次RoundMap，内部复制临时配置，不保存DA。客户端Join参数不能更改服务器配置。示例复用已建立的PracticeRoom及旧NetTest作为不同目标World：

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'D:\HAL_Future_Creation\HAL_Future_Creation.uproject' `
  '/Game/Maps/MVPB_PracticeRoom?listen?RoundMap=/Game/Maps/MVPB_NetTest' `
  -game -log -windowed -ResX=960 -ResY=720
```

同机Client仍连接127.0.0.1:7777，异机连接Host局域网IPv4。可用`?LoadTimeout=5`临时缩短服务器测试上限；日常验收用DA默认45秒。Standalone去掉`?listen`即可。

## 6. 本轮验证和交接

共享结果、报告及逐文件SHA-256见[本轮验证与保护基线](GF4_Verification_20261103.json)。原始资料在`Saved/GameFlow/GF4_20261103`，不提交Saved／Binaries。最终结果如下：

- **UE5.8.3 Editor／Game Development Win64构建成功**。最终Editor增量6.59秒，稳定源码的Game完整`-Rebuild`78.87秒；现有引擎弃用及工具链建议警告仍存在，不宣称编译零警告。本轮开工没有Editor／Live Coding占用，不需要用户关闭；验证进程已全部退出。
- **普通回归125/125，0测试警告／失败／未运行**。新增三项覆盖配置、无旧Actor引用的交接、身份／席位／Host唯一性、错误Flow／Round／Token／Avatar、全员屏障／服务器倒计时、PlayerState身份复制与返回后撤销确认。
- **真实独立进程双端连续两局：Host／Client各1/1，0测试警告／失败**。从团队保存的PracticeRoom地图及GameMode BP出发，以服务器URL临时指定旧NetTest为另一目标World；实际经过房间→正式图→房间→第二次正式图→入口。不是用直接Possess代替跨图。两端确认身份／车型保持、新局HP／效果／冷却及新物理Epoch；Round1倒计时结束时刻均为17.836，Round2均为24.515（对应服务器World时钟，不是RTT）。
- **三组恢复双端分别各1/1**：5秒上限配Client延迟确认15秒，超时返回房间并撤销确认；Client在Loading主动离房，Host返回房间；Host在Loading主动离房，两端回入口并保留原因。三组均0测试警告／失败。延迟确认和受控离房不冒充真实慢磁盘、突然掉线或四机验收。
- **GF-3与GF-2兼容双端分别各1/1**；GF-3保持未启用跨图时的锁房终点，GF-2使用原NetTest自动出生流程。另有最终Listen单Host1/1、实际D3D12离屏Standalone连续两局1/1，均0测试警告／失败。无效目标地图拒绝的独立1/1支持证据在最终PIE前缀／菜单显示修正前执行；最终原生回归仍覆盖非法配置。所有16份采用的报告均检查JSON，未以进程退出码替代断言。
- **实际D3D12、1280×900倒计时画面已看图**：席位、车型、状态提示和禁用按钮可读。它不代表真人鼠标／手柄焦点、持续按键和车型视觉全部验收。
- **1182份保存资产逐份哈希全同**；Config、Build.cs及uproject未改；266份最终Source／Config／uproject留档。保留开工时一份GI修改及两份未跟踪Room资产，本代理没有编辑或保存它们，未提交／Push。

早期失败独立留档：UHT／C4458的继承Pawn命名冲突已更名；首轮真实Client在Controller更换期间缺LocalPlayer却创建Widget，已延迟创建；后续无拥有连接的引擎物理时间戳RPC由实际`TickActor`入口等待拥有关系解决，初版`PlayerTick`保护不足，未压制警告。Game一次增量链接使用旧unity对象失败，稳定源码完整重建后成功。首轮GF-2兼容脚本误用新Practice GameMode导致旧夹具缺初始Pawn，改回原NetTest启动后双端通过，未削弱断言；失败报告仍保留。

尚未执行团队GF-4正式资产接线／真人双端验收、实际持球跨图与硬件持续按键、PIE完整测试、突然断网／进程终止、真实磁盘慢载入、实测RTT／丢包、打包或四台PC。就绪屏障不承诺GPU资源全部完成；阻塞载入期间的超时要等服务器恢复Tick处理。

GF-4完成代码／程序验证后，团队还需保存正式图并开启DA的Round Travel，完成人工接线及基础双端反馈。下一开发环节为GF-5街机入口／选车表现；GF-6、原F4／VP-4C及四机余项继续独立记账。

## 7. 2026-11-03团队初测及GF-5接续

后续已接收干净`main/817e443`：团队提交独立`MVPB_Match`和两份Room DA更新，用户报告初步测试暂无问题、后续继续更多测试。该反馈更新第6节当时“正式资产待接线／初测”的状态，不补写未提供的逐项／网络扰动／四机结论；原GF-4构建与测试证据不变。

GF-5已接续实现街机入口、四席位／静态车型预览、本地焦点及输入切换、加载和错误反馈，见[实施、可选品牌DA与Catalog接线](GF5_Arcade_FrontEnd_Implementation_CN_v0.1.md)。现有Room／GI地图引用继续使用，无需重建GF-4资产；下一开发环节GF-6，团队真人硬件及完整组合分别验收。
