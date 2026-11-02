# GF-3：灰盒房间、选车与练习——实现及团队配置

日期：2026-11-02，Asia/Tokyo。施工基点`main/d1add4bf9e8b065f525ad4c1dd6c17042f30dc99`，开工工作区干净；本机UE **5.8.3-58210709**。执行依据为[分段施工v0.2](Room_VehicleSelection_Practice_Match_Construction_CN_v0.2.md)，生命周期沿用[GF-2](GF2_Vehicle_Lifecycle_Implementation_CN_v0.1.md)。

团队的三份Gameflow DA已提交。本轮只读加载确认`DA_Room_Development.Rules.Profile=Development`、Exhibition配置为Exhibition，两者引用同一有效Catalog，包含Car_01／Car_02；此前Profile待修正项已解决。代理没有编辑或保存Blueprint、DA或地图。

## 1. 本阶段实现范围

- 新增专用C++ `HALPracticeRoomGameMode`。连接成功先分配席位／颜色／编号，保持无驾驶车辆的Selecting状态；不自动生成旧DefaultPawnClass。
- “确认车辆”由本人的Controller请求服务器：验证目录ID、选车版本、所选车型配置及本席位出生点，生成真实车型Pawn，走GF-2显式初始化，再检查实际Pawn／ASC／属性初始化才授予Ready。
- 出生点缺失、重复编号、真实物理根被阻挡、配置／生命周期失败均保持未就绪，并回到保留车型的可重试选车状态。不会把没有车辆的确认点击算作准备完成；过期请求不会生成第二辆车。
- 取消先撤销Ready，再退休／解绑／销毁旧车，释放球权、作用域效果和输入，返回选车。重新确认使用全新Pawn及新的生命周期序号，席位／颜色／编号保持。
- 已确认者可练习，晚加入者独立选车；允许重复车型。沿既有权限门控保留驾驶、技能、控球／发射、冲击／脱球，标准命中不扣HP。
- 原生灰盒界面显示四席位、身份色／编号、车型、选车／准备中／练习中、失败原因及Host开始条件；通过Controller提交意图，不在Widget决定规则或生车。
- 练习时车辆上方显示对应颜色的P1～P4标记；持球光束优先使用房间身份色，旧回归入口仍使用原PlayerId色盘回退。不要求先改车身材质。
- Client离开清理席位及车辆，Open房间的其他人继续练习。Host关闭房间通知客户端回本地入口，并在短暂发送窗口后自行返回；连接丢失也保留错误文本并回入口。退出保护避免World销毁期间再创建旧房间Widget。

**GF-3不做真实开赛转场。** Host的开始请求目前会验证全体Ready并冻结名单，切到Locked，关闭加入／改车／驾驶；界面显示比赛准备中。这里是本阶段可验证终点，随后可离房重新创建房间。GF-4再承接实际ServerTravel、载入屏障、倒计时与Playing；不能把Locked当成完整比赛已经开始。

## 2. 关键实现和调用顺序

### 2.1 服务器确认与失败

`HALPlayerController::ServerRoomAction`只作用于该Controller，经GameMode的`HandleRoomAction`分派。旧`HALMatchGameMode`保留GF-1纯数据契约；只有专用Practice子类执行生成／销毁，不将原NetTest或临时GF-1／GF-2夹具自动变成生产房间。

同步确认顺序：

```text
Room.Request(Confirm, ExpectedSelectionRevision)
→ PreparingAvatar，新选车版本
→ 目录精确类、席位出生点及版本校验
→ SpawnActorDeferred / FinishSpawning，应用真实Definition
→ 仅用实际物理根检查阻挡
→ PlayerState.ChangeVehicleAvatar(FreshPawn, PreviousSerial+1)
→ Room.CompleteAvatar(Controller, SelectionRevision)
→ Ready，发布可驾驶权限
```

出生检查不使用视觉网格、PlayerStart的人形胶囊估算或假设的车体尺寸，也不把车强行挤入障碍物。失败候选会销毁，`FailAvatar`只接受仍匹配的Preparing版本，清理Ready并递增选车版本，保留车型与可读结果；成功后的迟到失败不能撤销Ready。生成为本阶段的同步窄路径，没有新增异步资产加载框架。

首次无车与已有车换车存在一个引擎边界：GAS `InitializeComponent`默认设置Owner=Avatar=PlayerState。GF-2的既有Avatar保护原先只覆盖从已有车切换，本轮仅允许“Revision=0、没有当前Pawn、ASC Owner与Avatar都为本人PlayerState”的初始状态；其他错误Avatar仍拒绝，并有反例测试。没有清除已有初始化保护。

### 2.2 输入、复制与界面

选车／确认／取消仍携带服务器选车版本；客户端按钮在请求和对应版本复制收敛期间暂时禁用，5秒无响应允许重试，服务器版本及阶段检查仍是最终保障。没有客户端“我已准备完成”的新RPC。

Widget使用原生Slate布局和UMG本地输入模式。只以0.1秒本地UI定时器观察Pawn／ASC／运动快照是否收敛并切换输入，不驱动物理、Buff或权威状态。进入菜单时通过既有输入管线提交中性命令、清观察偏移；UIOnly阻止菜单操作穿透驾驶。回练习后使用GameOnly，不反复每帧重设焦点。

这不是GF-4载入屏障：服务器Ready与客户端Pawn／属性复制分开到达，Widget等待本地绑定与运动快照只负责当前房间输入体验。跨World的身份、开局号、客户端确认及失败恢复仍待GF-4。

## 3. 团队Editor配置步骤

完整编译后重开Editor。无需新建IA、IMC、GA／GE或选车Widget Blueprint；原加速／免疫配置继续由PlayerState的DefaultAbilities和车辆Local Config负责。

### 3.1 检查现有三份DA

1. 打开`Content/Data/Gameflow/DA_VehicleCatalog_Default`，确认两条稳定ID为`Car_01`、`Car_02`，分别引用`BP_RoundedVehiclePawn`及`BP_VehiclePawn_Car_02`。车型BP继续引用自己的有效VehicleDefinition及共享Local Config。
2. 可填写每条Display Name；灰盒界面未填写时直接显示VehicleId。Icon不影响玩法有效性，本阶段未做模型预览轮播。
3. `DA_Room_Development`：Rules → Profile为Development，Catalog为上述目录；单Host即可确认、练习和锁房。
4. `DA_Room_Exhibition`：Profile为Exhibition，同一Catalog；需要四名实际参与者全部确认才允许锁房，最多四人含Host。
5. Validate Assets并保存。本轮已读取到正确保存值，不需要重复重建DA。

### 3.2 新建专用房间GameMode BP

1. 在`Content/Blueprints/Gamemode`右键创建Blueprint Class，All Classes搜索并选择 **HALPracticeRoomGameMode**。例如命名`BP_MVPB_PracticeRoomGameMode`。
2. Class Defaults → Room → **Room Config**指派`DA_Room_Development`，用于当前单人／双端开发；展出前再切为Exhibition。
3. GameMode的**Player Controller Class**使用`HALPlayerController`，或原测试GameMode中已经使用且继承它的轻量BP。
4. **Player State Class**指派现有 **`BP_HALPlayerState`**，保持你已经配置的Primary技能授予；不要为了房间测试重新配置另一套GA／GE。直接使用原生PlayerState时会使用其默认免疫能力，不等同于团队BP中的加速装配。
5. **Game State Class**使用`HALMatchGameState`。Default Pawn Class可保持None；Practice模式按服务器目录生成实际车型，不从此字段选车。
6. Room Config启用后，最小／最大人数由它的Profile决定，旧Match|Players参数不决定房间门槛。Compile并保存BP，无需Event Graph。

### 3.3 复制房间地图并配置出生点

1. 打开现有`/Game/Maps/MVPB_NetTest`，使用Save Current Level As保存独立副本，例如 **`/Game/Maps/MVPB_PracticeRoom`**。保留原NetTest的GameMode及Room Config=None，用于旧流程回归。
2. 在新地图World Settings → GameMode Override指派新建的`BP_MVPB_PracticeRoomGameMode`。
3. 检查场上 **HALPlayerStart**。建议准备四个，Spawn Slot分别0、1、2、3，不能重复，不能用普通PlayerStart代替。本席位对应同编号出生点；开发单人至少需要Slot0。
4. 箭头指向车辆初始车头方向。位置是车辆**物理根原点**，需要给两个车型的真实凸包留足地面／墙体／其他车空间；PlayerStart的人形胶囊预览不代表车辆外形。出生区域不要放机关实体或另一辆车。
5. 场景里作为展示／对照的车关闭Auto Possess Player，不要让场景车抢玩家Controller；可保留不参与操控的测试物体。确认前出现可驾驶车时先检查此项及GameMode Override。
6. 保留练习球、场地边界和所需机关；唯一主球的相机标记沿原配置。保存地图。此段不会自动建立正式比赛地图或转场地图。

### 3.4 让现有Host入口打开新房间

1. 打开`Content/Blueprints/Network/BP_MVPB_NetworkGameInstance`，Class Defaults。
2. 将 **Match Map Path**改为新房间长包路径，如`/Game/Maps/MVPB_PracticeRoom`，不写`.umap`或磁盘路径。
3. **Entry Map Path**保持`/Game/Maps/MVPB_NetEntry`。原Menu Widget Class继续使用现有Host／IPv4 Join表单。
4. 保存并从NetEntry开始测试。连接到专用Room后会自动用原生`HALPracticeRoomWidget`替换旧等待界面；不用新建或指定选车Widget、不用在Level Blueprint调用。
5. 同机客户端仍输入 **127.0.0.1**；不同电脑输入Host的局域网IPv4，使用现有7777端口。Host／Client须同步源码、Room地图、BP、三份DA及车型资源并重编。

灰盒布局和英文文本属于占位表现，后续GF-5再替换为街机入口／车型预览／正式视觉。当前步骤不要求搭建完整UI美术。

## 4. 实际操作与验收

1. Entry → Host；确认房间显示自己为P1和Host，初始没有驾驶车辆。
2. Previous／Next选择车型，点击 **Confirm & Practice**。成功后自动进入练习，鼠标隐藏；用原有油门／转向／技能／发射输入。
3. **Tab、Esc或手柄Start**打开／关闭房间菜单。菜单开启应停止继续输入油门／发射，关闭后能恢复驾驶；长按操作键跨菜单时应额外实测一次。
4. 菜单中 **Cancel / Change vehicle**销毁旧车并返回选择。重新选另一车型确认；确认时重复点击不应产生两辆车，颜色／P编号不应改变。
5. Client在Host练习中Join；它先选车，Host继续练习。分别测试两人同Car_01、同Car_02和异车型，观察车顶编号／颜色和持球归属，不只检查车型文字。
6. 球／机关命中练习车HP保持，冲击／脱球继续；原Primary技能可释放／到期。无车、未确认或锁房后不可借旧Pawn继续驾驶／施放。
7. 占用某出生点后确认，界面应显示Spawn point blocked并允许重试；移开阻挡物再确认，应成功。缺对应Slot、重复Slot及无效车型配置也不应误Ready。
8. 所有人真实Ready且满足Profile人数后Host可点击 **Host: Start**，其他玩家不能开始。GF-3目前应锁房、停止驾驶／改车／加入并显示比赛准备中；**本轮不等待它自动转场**。使用Leave room返回入口，下一段GF-4再接跨图。
9. Client点击Leave room后自己回入口，Host仍可练习、其席位空出；Host点击Leave room后双方回入口并显示Host closed the room。再创建房间应没有旧车、旧菜单焦点或球权引用残留。
10. 另测Host直接关闭进程／拔网线后的客户端错误返回，以及手柄焦点、鼠标按钮、动态相机和具体车型画面；自动化的受控退出不代表这些真人／异常网络子项全部通过。

调试命令仍可用`HALRoomStatus`、`HALGASStatus`、`HALMatchStatus`；房间实际阶段以HALRoomStatus为准，不用旧MatchPhase的Playing判断练习是否开放。正常操作不需要控制台命令。

## 5. 可选：不改保存地图的命令行开发入口

此入口使用专用原生GameMode和已保存Room DA，暂用现有NetTest地形；只影响本次运行。原生PlayerState默认能力可能与团队BP不同，正式配置仍按第3节。后续添加的RoomConfig启动参数只在**服务器InitGame**读取，客户端Join选项不改变服务器配置。

```powershell
# 同机Host（默认7777）；Standalone时去掉 ?listen 即可
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'D:\HAL_Future_Creation\HAL_Future_Creation.uproject' `
  '/Game/Maps/MVPB_NetTest?listen?game=/Script/HAL_Future_Creation.HALPracticeRoomGameMode?RoomConfig=/Game/Data/Gameflow/DA_Room_Development.DA_Room_Development' `
  -game -log -windowed -ResX=960 -ResY=720

# 第二个PowerShell窗口：Client
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  'D:\HAL_Future_Creation\HAL_Future_Creation.uproject' '127.0.0.1:7777' `
  -game -log -windowed -ResX=960 -ResY=720
```

地图／DA／车型软引用的打包资源集合及四机部署在GF-6验证；本轮没有执行打包或声称打包入口已经通过。

## 6. 验证记录与剩余边界

共享证据及逐文件哈希见[GF-3验证摘要](GF3_Verification_20261102.json)。原始构建、自动化、双端日志及离屏画面在`Saved/GameFlow/GF3_20261102`，Saved不随Git交接。

- 最终Editor／Game Development Win64均构建成功。现有引擎弃用／工具链建议警告保持，不宣称编译零警告；本轮没有Editor／Live Coding占用需要用户关闭。
- 最终普通回归 **122/122**，0测试警告／失败／未运行。新增4项覆盖实际保存Development配置、原生GameMode初始化、A→B→A实际车型生成／取消／新序号、初始Owner=Avatar及错误Avatar反例、真实凸包阻挡／移开后重试、过期确认／失败、同车型身份、晚到者选车、非Host开始拒绝、锁房权限、Standalone单Host和Round0关闭。临时世界Controller不冒充四台真实电脑。
- 真实NullRHI独立进程 **GF-3 Flow：Host／Client各1/1**。服务器使用实际专用原生GameMode和保存的Room DA／目录，以当前NetTest地形验证：Host先练习再加入Client，同Car_01→同Car_02→异车型，客户端非法ID／非Host开始拒绝，重复确认防重，取消旧Avatar、实际客户端绑定／新运动Epoch、练习技能、锁房物理阻断，最终受控Host关闭后两端回Entry且原因保留。没有以直接Possess或临时替代Room初始化冒充生产生成流程。
- 独立第二组真实双端 **Client Exit：Host／Client各1/1**，确认Client主动离房回Entry，服务器释放席位、Host仍可练习／重新接收加入，开发单人开始资格仍有效，随后Host可退出。GF-1／GF-2原有真实双端兼容复验也分别各端1/1。以上四组均0测试警告／失败，没有追加丢包／RTT或四机结论。
- 实际 **D3D12离屏1280×900画面**已检查，原生四席位／按钮／状态提示均可见；初始背景对比及提示已修正并重新截图。网络测试通过后只改了Widget的这两处显示，最终两Target及122项／D3D12截图已在该显示修正后复做；不把画面检查写成真实鼠标点击／手柄导航或完整车型观感验收。
- **1180份保存资产逐份SHA-256全同**，Config、Build.cs和uproject未改；260份最终Source／Config／uproject哈希留档。只读检查当前保存的GI Host目标仍为NetTest、旧GameMode的Room Config仍为None，故团队尚需按第3节建立并接入专用房间资产。本轮未提交／Push。

早期Slate绑定／测试指针类型编译错误、因自有失败Client未退出而发生的DLL链接占用、未覆盖的首次Owner-only GAS状态及测试未检查前提导致的崩溃均独立保留。真实Flow R3曾因Host返回时RoomChanged在World销毁期间重建Widget而ensure，已加退出／teardown保护并移除Controller委托，R4两端重新通过；没有屏蔽该错误。最终结论不以退出码替代测试JSON。

尚未完成团队专用Room BP／地图／GI接线后的真人验收、硬件输入／具体车型画面、Host进程突然退出／断网、打包、真实跨图及四机；原F4／VP-4C／FX余项仍按原安排分别验证。

下一开发环节是 **GF-4：真实跨图、全员载入就绪与同步倒计时**。团队可先完成第3节专用房间资产接线及第4节真人基础测试；本轮没有代理制作这些保存资产，也不将程序验证写成团队接线／画面全部验收。
