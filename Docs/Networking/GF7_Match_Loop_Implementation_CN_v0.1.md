# GF-7基础比赛闭环：实现与接线 v0.1

**2026-11-05接续：** 本阶段已提交为`86f513e`；当前`main/3a8af57`随后新增电击墙Client高速二次回弹修正。下文“未提交”和138项／9份报告均保留原GF-7交付时点；新维护更改了10份源码，当前组合须重编并集成复验，不把两份历史证据合并成新通过。本次接续只检查源码／共享JSON，未重新执行UE测试，详见[核对摘要](GF7_ElectricWall_ProgressSync_20261105.json)。

日期：2026-11-05。基点`main/726b804`；开工保留前轮草案／状态／白名单修改。用户已确认四组推荐规则，并再次明确全部UI保持原生回退＋Widget Blueprint可替换。代码、Editor／Game构建及本机程序验证已完成；真实打包、团队操作与校园四机仍待验收。本轮未提交／Push。

## 1. 比赛规则与参数

只在正式Round World的Playing启用：默认300游戏秒，加载／开赛倒计时不占时长，本地菜单不暂停服务器。HP归零后退场／观战；至少两人开局时仅剩一人获胜，单人Development可独立测试计时。超时比较存活者绝对HP，0.001 HP内视为并列；并列者共同获胜，最后同批全灭为平局。

在实际Room Config DA的 **Match Rules** 调参：`Duration Seconds`默认300（1～3600秒），`Results Seconds`默认8（1～60秒）。观战距离1400cm（300～5000）、FOV60°（30～120）、Pitch−65°（−89～−30）。服务器复制实际使用的规则快照，客户端不能通过自己改DA决定胜负。首次测试可设Duration=15、Results=3，随后恢复300／8。

现有DA会读取新增字段默认值；没有创建新GA／GE、IA／IMC或Widget资产。完整编译后重开Editor，需要调数值时手动保存DA／Validate再新开会话。项目中Room未启用真实跨图或旧NetTest兼容入口不自动变成正式比赛。

## 2. 淘汰、退出与返回

淘汰批次在属性／碰撞回调结束后统一裁决；HP首次归零即锁定该局致死快照，同批后续治疗不能复活参赛者。清理当前Pawn的球权、技能、区域、自救和输入后销毁车辆，不保留碰撞残骸。观战使用本地独立相机跟随存活车辆，按编号确定顺序，支持上一辆／下一辆；目标退出或淘汰会自动换目标，无目标时保留最后关注位置的俯视。不会Possess其他玩家或绑定他人ASC。

Playing中普通Client退出算弃权，其他人继续；如果因此只剩一名，按最后存活规则结束。Loading／Countdown中退出仍取消本次开赛。Host被淘汰时继续承担Listen Server并观战，Host进程退出则全员回入口。Exhibition返回房间后人数不足四人，需要补人才能再开赛。

结果只确定一次，记录Flow／Round身份和各玩家值快照，禁止被迟到HP／旧World事件改写。结果阶段关闭新伤害和玩法操作，显示8秒后服务器统一返房；Host可提前返回，正常结算返房不需要`-HALAllowTestRoundEnd`。原开发／展示手动中止仍保留，记为Aborted，不伪造冠军。返房保留身份、席位、颜色和车型偏好，清确认；下一局按原流程恢复满血、清效果／冷却并重建球与物理历史。

## 3. UI保持原生回退和Designer替换

配置入口仍是GameInstance的Room Screen Class／Pause Screen Class，父类 **HALPracticeRoomWidget**；未配置时显示原生菜单、计时HUD、观战按钮和结果。有效WBP继续使用自己的Designer，不叠加原生布局或车顶编号。没有新增必须配置的Results Widget资产或隐藏的默认蓝图。

自定义WBP在 **Receive Room View** 事件中读取新增字段：

| 字段／动作 | 用法 |
|---|---|
| MatchRemaining | 正式比赛剩余秒数，只用于显示；建议Ceil后格式化mm:ss |
| bSpectating／SpectatedNumber | 显示观战面板与当前目标；0为场地兜底 |
| bCanCycleSpectator | 上／下一辆按钮启用状态 |
| Cycle Spectator(-1／1) | 绑定左右按钮；仅改变本地视角 |
| bShowResults／ResultsRemaining | 显示结果面板与返房倒计时 |
| ResultSummary | 临时结果文字，可直接显示或用结构数据自行排版 |
| Match.Players | 稳定PlayerKey、编号、名字、颜色、车型、Life、HP、MaxHP、bWinner；无旧World的Actor引用 |
| Match.Reason／RoundId | 区分最后存活、时间到、全灭、手动中止及具体回合 |
| bCanReturnToRoom／Return To Room | Host可提前返房；服务器仍检查权限和版本 |

观战面板应独立于普通MenuPanel：`bSpectating`为true时显示，即使`bMenuVisible=false`；本地输入此时归UI，不开放驾驶。Results面板按bShowResults显示。不要在WBP用Delay后Open Level、客户端自行计时结束或写HP；自动结束／返房已由服务器负责。保留已有Leave／Resume／焦点接口。

## 4. 团队配置与快速验收

1. 重开本次完整编译的Editor。打开实际房间使用的`DA_Room_Development`或`DA_Room_Exhibition`，在Match Rules临时设Duration Seconds=15、Results Seconds=3，保存并Validate。无需重建房间、车辆、GA／GE或输入资产。切换正式规则时恢复300／8；修改DA后新开会话。
2. 先用Development启动Host与Client，选车确认，Host开赛。进入正式Playing后开始倒计时；练习房间及开赛倒计时不消耗比赛时间，本地菜单也不会暂停比赛。
3. 用球／机关扣血到0，确认旧车消失、释放持球，不能继续驾驶／击球／发动技能／自救；若还有至少两名存活者，观战可上／下一辆。两人开局淘汰一人会立即进入结果；要测试持续观战，请用三人以上。
4. 无人淘汰等到超时，分别验证HP不同、最高HP相同；通过可控机关尝试同批全灭，后者也有原生测试证据。核对结果页的胜者、OUT／LEFT状态和最终HP。不要把跨多个服务器批次的死亡当成同时淘汰。
5. 检查自动返房与Host提前返房，Client不能替Host结束结果页；返房后身份／颜色／车型保留，确认清除，再开一局应满血且无上一局Buff／冷却／球权。
6. 四人Exhibition测试Host先淘汰、Client中途退出、观战目标退出自动切换；Host退出房间与Host仅被淘汰分别测。Exhibition返房后不足四人时不能再次开赛，先补人。
7. 若配置WBP，按第3节接Receive Room View及语义动作；原生结果页与观战按钮不应覆盖Designer。先验证未指派类的原生回退，再验证自定义类；按键／焦点、不同分辨率、长名字和真人观战手感需团队检查。

开赛检查额外等待远端车辆的`AcknowledgedPawn`与当前Pawn一致，Host按钮在接管确认前短暂不可用是正常保护，避免立即跨图时带入旧世界的车辆引用。这里没有增加人为固定等待时间。

## 5. 验证与证据边界

本机UE **5.8.3-58210709**，Editor与Game Target最终构建成功。既有MSVC非首选工具链／引擎弃用警告保留，报告0测试警告不代表编译零警告。详细哈希和报告摘要见[本轮验证记录](GF7_Match_Loop_Verification_20261105.json)，原始日志位于`Saved/GameFlow/GF7_20261105/`。

| 验证 | 实际结果与范围 |
|---|---|
| 原生回归Regression_R3 | 138/138，0测试警告／失败／未运行／进行中；含4项GF7确定性判定、批次死亡、截止快照／单人、弃权／Host淘汰测试 |
| ThreeRounds_R3 | 真实双进程，各端1/1且退出0；三局依次验证最后存活、超时并列、同批全灭，自动返房并恢复下一局满血／来源技能，最终回入口 |
| FourPlayers_R2 | 真实四进程，各端1/1且退出0；Exhibition、Host淘汰／独立观战相机、跟随目标弃权后自动切换，其余继续、超时两人并列、Host通过Widget动作提前返房，未启用测试结束许可 |
| LegacyTwoRounds_R2 | 原GF4.TwoRounds保存BP／地图双端，各端1/1且退出0；两次Playing、身份／新局清理／最终回入口均通过，旧手动中止返房保持兼容 |
| UI渲染 | D3D12 1280×720：原生观战／结果，另有内存UMG替换夹具接收结构化结果事件；后者证明Designer接口，无保存WBP修改，不代表正式UI美术验收 |

双端／四端GF7测试中Host出站注入80ms，60FPS；这是同机注入条件，不是实测RTT或四台校园PC。探针仅在已生成GameMode实例上使用瞬态Room DA副本缩短计时，并通过权威ASC测试入口设置HP；实际伤害机制沿既有Resolver／GAS，真人完整击球淘汰仍需上表步骤验收。最终截图`GF7_NativeSpectator00001.png`捕获切目标当帧，有镜头移动模糊，仅作观战HUD／按钮显示证据；`GF7_Results_Host_100001.png`显示原生胜者／弃权HP快照；`GF7_Results_Client_200001.png`显示替换夹具收到并列结果。此前迭代的00000图保持不覆盖。

首轮Editor编译的局部变量遮蔽错误已修正；首轮双端三局虽均到达结果，但旧世界NetGUID解析报错、夹具在Controller接入前发送一次名字导致最终协调超时，故保留为失败。补远端车辆接管确认屏障，并让测试夹具等待拥有者连接和重试协调后，完整重验通过；没有屏蔽网络错误或放宽生产权限。最终审查补充HP归零锁定与同批治疗断言后，以Editor_R6／Game_R2完整重编，重新运行上述全部回归；9份最终报告与295份输入冻结对应，旧通过结果保留为此前迭代。

代理没有修改／保存BP、DA或地图；不新增必配UI资产。保存资产保护、最终输入冻结与原手动返房兼容结果一并记入验证JSON。真实打包／Cook、四台校园PC、真人按键与最终画面、F4／VP-4C等集中验收仍保持独立。

## 6. 返房后重新加入被错误退回入口（2026-11-05已定位，待修复）

团队报告：Client在Results离房，Host回到准备房间；Client使用同一IPv4再次加入，Host短暂看到成员后该成员退出。**这是缺陷，准备房间处于Open且有空位时应允许普通新连接加入；它不要求恢复断线前的旧局身份，也不属于此前后置的断线重连功能。**

本机Client日志`HAL_Future_Creation-backup-2026.11.05-05.55.26.log`四次记录`Your room identity could not be restored.`；例如第2344～2366行先由服务器Welcomed并加载MVPB_PracticeRoom，随后收到该明确退回原因。网络已到达Host并通过初始接纳，不能将本次故障归因于IPv4写错或防火墙。

源码原因：`InitGame`消费跨图交接后将`bRestoringTravel`设为true，该标记在此GameMode生命周期内保持。`HandleStartingNewPlayer_Implementation`只根据此标记就要求在`Travel.Players`中恢复旧PlayerKey，普通新连接也进入该分支。UE的普通`PostLogin`在Super调用内先执行HandleStartingNewPlayer，而项目AddMember在Super之后才执行；新连接此时还没有已导入的旧身份，恢复查找失败后服务器发出返回入口RPC，随后短暂加入／退出与团队画面一致。

修复方向：明确区分UE普通PostLogin与真正HandleSeamlessTravelPlayer引发的旧玩家恢复，只在服务器可信的跨图上下文中执行ImportTravelMember；普通新连接在Open时仍走正常席位分配／选车。不要直接在Host先恢复后清全局标记，否则慢到的旧客户端可能无法恢复；也不能放开Playing／Loading的加入闸门。

待补回归：Results离房→Host返房→同一Client重新加入并完成下一局；Host返房后全新第三名玩家加入；仍在线的慢客户端正常随图返回；房满／已开局加入仍拒绝。当前临时处理为Host退出并重新创建房间，再让Client加入；单独重启Client不解决Host GameMode仍走旧恢复分支的问题。

本轮仅日志／源码及本机引擎调用顺序核对，**没有修改C++、重编或执行复现／修复自动化，问题尚未修复**；原138项和各双端结果不覆盖这个新增场景。见[诊断证据](GF7_ReturnedRoom_Rejoin_Diagnosis_20261105.json)。

## 7. 普通入房与无缝交接入口修正（2026-11-05）

第6节保留修复前诊断。本轮经用户授权开始修正，无新增玩法规则待确认：显式退出后再次输入IPv4属于普通新连接，重新分配PlayerKey并重新选车；仍在线跨图的玩家才恢复原身份、席位与车型。没有增加断线旧局重连。

`HALPracticeRoomGameMode`覆盖UE的`HandleSeamlessTravelPlayer`，用仅覆盖该调用栈的服务器上下文标记包住Super；`HandleStartingNewPlayer`只在该真实交接上下文内读取Travel身份。普通PostLogin不再因整个World的`bRestoringTravel`而误走恢复分支。没有提前清除Travel快照，较晚加载的旧客户端仍可恢复；比赛／Loading关闭加入规则保持。

Room在交接期间临时预留未导入旧玩家的席位及容量，避免新连接抢占其颜色／编号；导入成功、明确退出或服务器已无对应Controller时释放。Host不能在仍有待导入玩家时开始下一局。此表仅存在当前服务器内存，不是断线重连保留名额，没有新增复制字段、资产配置或Tick。

已补两项原生回归，覆盖普通启动回调与真实无缝回调交错、较晚旧玩家保留席位、重复新登录、新Key、满员／退出释放／关闭加入闸门；另补四角色真实网络夹具`HAL.NetworkSmoke.GameFlow.GF7.ResultsLeaveRejoinAndNewPeer`：三人完成第一局，其中一人Results退出后用原进程调用生产`JoinMatch`再次加入，一人持续在线，Host返房后再启动全新第四人，四人完成下一局并返回入口。测试只复制瞬态Room配置缩短计时，不保存BP／DA／地图。

**当前验证状态：** 团队关闭Editor后，本机UE5.8.2-56702186的Editor／Game Target均完整构建成功。首轮DLL占用导致的链接失败保留；没有通过Live Coding跳过重编。已有相机C4305及启动警告保留，0自动化警告不代表0编译／启动警告。原始报告与保护快照在`Saved/GameFlow/RejoinFix_20261105_154208/`，可跨设备读取的摘要、构建输入及报告哈希见[本轮验证](GF7_ReturnedRoom_Rejoin_Verification_20261105.json)。

| 本轮检查 | 实际结果 |
|---|---|
| HAL.FutureCreation | 141/141，0测试警告／失败／未运行／进行中，含上述两项新增回归 |
| ResultsLeaveRejoinAndNewPeer | Host／Returning／Staying／Fresh四进程各1/1；原进程Results退出后通过生产JoinMatch重新加入；在线者保留Key／席位，新连接新Key并重新选车，四人完成下一局及返入口 |
| 原GF7.ThreeRoundMatch | 真实双端各1/1；最后存活、超时并列、同批全灭与自动返房／新局状态恢复通过 |
| 原GF4.TwoRounds | 保存BP／地图真实双端各1/1，连续两局及最终返入口通过 |
| 电墙误判撤销（额外整合检查） | 双端各1/1；仍保留原断言 |
| 电墙高速DrivingContinuity（额外整合检查） | **双端超时失败，未修复、未放宽断言。** 当前旧NetTest使用Car_02；服务器两次接触、Seq=1，Client排队一次预测，但夹具未观察到权威Stun前负向X速度（Early=0／Velocity=0），Client未发送完成信号，Host亦超时。没有据此确认是生产回弹问题还是当前车型／夹具观测时序问题，需独立排查 |

共13份报告，其中11份干净通过、2份电墙高速失败；不能写成整个GF-7＋电墙组合全通过。额外电墙夹具运行旧`BP_MVPB_ListenGameMode`，不经过本次PracticeRoom跨图入房入口，本轮未修改其生产／测试源码或资产。返房与GF7夹具为同机真实进程、NullRHI／60FPS、Host出站注入80ms；电墙为150ms±40ms、10%丢包。以上注入不是实测RTT或校园四机。新加入Fresh仅参与第2局，探针`Played=2／Results=2`是最后观察到的RoundId，不是Fresh参加了两局。

1183份保存资产前后逐份SHA256全同，298份最终Source／Config／uproject输入与成功构建时一致；Config、uproject、Build.cs和Target.cs保持。专用验证进程均已退出；未提交／Push，没有新增BP／DA必配步骤。本轮没有真人LAN、发布包或渲染／手感验收。

团队复测：双方更新同版源码并完整重编；完成一局后Client在结果页退出，Host留在原房并自动返回准备；Client保持同一游戏进程，输入原IPv4，重新选车确认并完成下一局。另试全新玩家加入、仍在线玩家正常随图返回，以及房满／开始后加入仍拒绝。无需Host关闭重建房间；若再次被退回，请保留双方日志及返回原因。后续独立排查电墙高速专项，再继续发布包／校园四机及原MVP-B欠项。
