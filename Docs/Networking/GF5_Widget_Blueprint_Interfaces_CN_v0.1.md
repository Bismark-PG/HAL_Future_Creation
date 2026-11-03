# GF-5补充：Widget Blueprint界面替换与原生回退

日期：2026-11-03，Asia/Tokyo。接续`main/35aa00c41566b5ba2161445574b61950006a024b`，开工干净，实际UE5.8.3-58210709。用户明确要求：当前代码布局作为未配置Widget Blueprint时的默认回退；配置后以Widget Blueprint布局为准。本轮只调整C++接口并交付手工接线步骤，不创建、修改或保存Blueprint／地图资产。

## 1. 配置优先级与范围

在`BP_MVPB_NetworkGameInstance → Class Defaults → Front End → Screens`配置：

| 配置项 | Widget Blueprint父类 | 使用时机 | 留空时 |
|---|---|---|---|
| Entry Screen Class | HALArcadeEntryWidget | NetEntry入口／连接／错误恢复 | 现有原生街机入口 |
| Room Screen Class | HALPracticeRoomWidget | 选车、驾驶HUD、载入／倒计时及通用房间菜单 | 现有原生房间界面 |
| Pause Screen Class | HALPracticeRoomWidget | 已确认玩家在练习或Playing打开本地菜单 | 继续使用Room Screen Class；它也为空则用原生界面 |

三项独立可选。自定义暂停页不必继承自定义选车页，二者只需共同继承HALPracticeRoomWidget。加载／倒计时由Room页面消费阶段状态；此时不会切到暂停页。

引用有效Widget Blueprint后，C++使用其Designer WidgetTree，跳过原生Slate布局和原生车顶编号绘制；没有额外叠一层原生选车／菜单。清空引用即可恢复对应原生界面。抽象／失效类型回退到有效的房间配置或原生类；**有效但Designer为空的Blueprint显示空内容，不因没有摆控件而偷偷切回原生布局**。

旧`Use Native Arcade Entry`／`Menu Widget Class`保留序列化兼容并退出新入口选择流程；现在统一使用Entry Screen Class。不要把旧WBP_MVPB_NetMenu直接当作新入口父类，它仍属于早期HALNetworkMenuWidget接口。

跨World的阻塞地图加载幕仍使用原生静态Slate提示，以免跨图保留旧World的UMG对象；正式图内的Loading／Countdown页面可以自定义。本轮没有增加MoviePlayer加载系统、3D车型预览、技能展示数据或正式比赛结算。

## 2. 最小自定义入口

1. 完整编译，打开Editor；Content Browser → User Interface → Widget Blueprint，父类选 **HALArcadeEntryWidget**，例如`WBP_HAL_Entry`。
2. Designer放Canvas／Overlay根、标题、创建按钮、IPv4输入框、加入按钮、取消连接按钮和状态文字。控件勾选Is Variable，需要手柄焦点的按钮保持Is Focusable。
3. 创建按钮OnClicked调用继承的 **Host Room**；加入按钮读取输入框Get Text → To String，调用 **Join Room**。保留服务器地址格式校验，不拼接控制台命令或地图路径。
4. 取消按钮调用 **Cancel Connection**；返回按钮调用 **Request Menu Back**。
5. 在Overrides添加 **Receive Entry View**，Break FHALEntryView：用Message更新状态文本；bConnecting为true时禁用创建／加入和地址输入，显示取消。初始状态及后续变化均由C++推送，也可在Construct读取一次 **Get Entry View**。
6. 如果制作待机／连接两页，用bAttract控制WidgetSwitcher；进入连接页的按钮调用 **Open Connections**。若只需要一个连接页，也可以直接展示创建／加入表单，无需另建规则状态机。
7. 在当前页更新时调用 **Set Menu Focus Target**，传该页的创建／继续／取消按钮。输入框内的Enter由UMG的OnTextCommitted处理，按OnEnter分支调用Join Room；不会被原生焦点环吞掉。
8. 编译并保存Widget，将GI的Entry Screen Class设为它；从NetEntry重新启动验证。

品牌DA仍可供原生回退使用。自定义Widget如需共用品牌内容，可自行增加HALFrontEndConfig类型的资产变量并引用同一DA，读取其只读标题／图片／强调色；布局及动画由该Widget决定。

## 3. 最小选车／房间Widget

创建父类 **HALPracticeRoomWidget** 的`WBP_HAL_Room`。建议根Canvas下分两个区域：MenuPanel和HUDPanel。根保留，MenuPanel根据View.bMenuVisible切Visible／Collapsed，HUDPanel按需要显示身份与驾驶提示；C++不会强制隐藏整个自定义Widget，因为它也可同时承担HUD。

在Overrides添加 **Receive Room View**，按需调用一个自己的`RefreshVisuals(View)`表现函数；Construct也可读取 **Get Room View**初始化。不要用Blueprint Tick查找玩家／重建名单。

常用只读字段：

| 字段 | 用途 |
|---|---|
| bRoomAvailable／bCatalogLoaded | 房间／车型元数据是否已到达；未到达显示等待占位 |
| Phase／LocalPhase／bMenuVisible | 区分选车、练习菜单、载入、倒计时、正式阶段及本地菜单显隐 |
| Vehicles | VehicleId、显示名、介绍、Icon、PreviewImage；不包含可由客户端上报的出生类 |
| SelectedVehicleId／SelectedVehicleName／Description／LoadedPreview | 当前服务器认可的选择及已异步加载的静态图；LoadedPreview为空时显示自定义占位 |
| Seats | 固定四项，含Occupied、Seat、Number、PlayerName、Color、VehicleId、Phase、Local、Host及ClientLoaded |
| bCanSelect／bCanConfirm／bCanCancel／bCanStart／bCanResume／bCanReturnToRoom／bCanLeave | 对应按钮启用状态；最终仍由服务器检查 |
| Status | 可读状态或错误原因 |
| LoadedPlayers／ExpectedPlayers／CountdownRemaining／RoundId | 载入人数、服务器时钟派生的剩余秒数及本局标识；UI不得按自己的倒计时开赛 |
| LocalNumber／LocalColor／bHost／bRequestPending | 本人身份、Host标记与请求等待显示 |

在Receive Room View中：

1. 用Vehicles生成或更新选车卡片，以VehicleId作为稳定标识；允许多位玩家选择同车型，卡片不要自行锁定别人的车型。
2. 四个席位按Seat索引显示，使用服务器给出的Number和Color。自定义页面得到完整玩家名，自行决定换行／截短。
3. MenuPanel显隐跟随bMenuVisible；选择、载入／倒计时、Playing菜单可用WidgetSwitcher分别制作。
4. 按bCan…更新按钮Is Enabled。载入／倒计时只展示状态，不触发额外准备请求或自己Spawn车辆。
5. 将LoadedPreview接Image的Set Brush From Texture；为空时用占位。模型旋转／技能说明未来可在此页面增加本地表现模块，不应生成真正的驾驶Pawn来做预览。

按钮OnClicked接以下继承函数：

| 按钮 | 调用 |
|---|---|
| 上一个／下一个车型 | Cycle Vehicle，Direction传−1／1 |
| 指定车型卡片 | Select Vehicle By Id，传该卡片VehicleId |
| 确认并练习 | Confirm Selection |
| 取消确认／换车 | Cancel Selection |
| Host开始 | Start Room Match |
| 继续驾驶 | Resume Driving |
| 离开房间 | Leave Current Room |
| 开发用返回练习房 | Return To Room，仅Development／Host／Playing可用 |
| 通用返回 | Request Menu Back，沿用本地返回键语义 |

这些函数调用既有Controller请求，UI不传目标玩家、车辆类、HP、伤害、Ready／RoundId等权威结果。重复／过期／非Host／非法车型仍按原服务器规则处理。

## 4. 独立暂停菜单与导航

可选再创建父类HALPracticeRoomWidget的`WBP_HAL_Pause`，只放继续、换车、开始／开发返回、离房及状态文字，仍消费同一份Room View和相同按钮接口。设到Pause Screen Class后，练习／正式阶段打开菜单时切到它，关闭菜单回到Room页面；取消确认返回选车，载入／倒计时也回Room页面。

这会重建页面实例：临时动画／鼠标悬停放Widget内，长期车型偏好及比赛状态继续由既有PlayerState／房间保存，不存到Widget。联机“暂停菜单”只接管本地输入，不暂停服务器比赛。

自定义页面使用UMG原生导航和Designer的Navigation设置。C++不截获它的上下左右、Tab、Enter和手柄A；普通Button OnClicked及输入框文本提交照UMG方式连接。Esc／手柄B／Start默认走C++通用返回；有子面板时可以在Blueprint的On Preview Key Down中先关闭子面板并返回Handled，未处理返回Unhandled继续默认行为。驾驶中打开菜单仍用现有Controller按键。

Set Menu Focus Target用于指定当前页面优先焦点，支持本页面内嵌套卡片的控件；C++等待可见布局路径建立后聚焦，关闭菜单不抢回游戏焦点。按钮要可聚焦、可见且启用；切换WidgetSwitcher时更新目标，避免传入其他页面对象。不要自行Remove From Parent移除整个流程页面、Set Input Mode、清空全部IMC或修改复制状态；显隐用MenuPanel／HUDPanel，继续驾驶用Resume Driving。C++保留页面实例切换、菜单与驾驶输入、按住键处理、失焦中和及跨图清理。

## 5. 团队检查顺序

1. 三项均None：确认原生入口、选车及菜单继续可用。
2. 只配置入口，再只配置Room，确认相互独立；最后配置独立Pause，检查同一房间正确切换。
3. 自定义布局中点击选车／确认／取消／开始，检查真实车型、权限、错误反馈和服务器就绪，而不只看按钮视觉。
4. 练习打开菜单、继续、换车、Loading／Countdown、Playing、开发返回房间和离房；不同World不可继续引用旧Pawn／Widget。
5. 清空三项引用并重新启动，恢复原生界面；有效但空Designer应保持空内容，便于发现未制作的页面。
6. 补真实鼠标／键盘／手柄、嵌套卡片／焦点、按住键／Alt+Tab、多窗口、实际Blueprint资产、打包与四机验收。

## 6. 本轮验证

共享结果、报告及逐文件SHA-256见[本轮验证与保护基线](GF5_Widget_Interfaces_Verification_20261103.json)，原始资料位于`Saved/GameFlow/GF5_WidgetExtension_20261103`，不提交Saved／Binaries。

- **最终UE5.8.3 Editor／Game Development Win64构建成功**，分别77.76／86.57秒。引擎弃用及工具链建议警告仍存在，不宣称编译零警告；开工没有Editor占用，本轮验证进程已退出。
- **129/129普通回归，0测试警告／失败／未运行**。新增两项检查配置优先级／清空恢复、只读字段及未绑定界面的请求无效；原127项继续通过。
- **实际D3D12 1280×720自定义UMG流程1/1，0测试警告／失败**。只在内存构造UMG WidgetTree，通过真正焦点路径和Button OnClicked调用公开接口，验证入口优先、初次与变化通知、未变化时不重复通知、四席位／车型数据、确认、独立同父类暂停页、继续及重复继续幂等、取消重选、实际正式图开赛／倒计时、返回房间撤销确认、清空配置恢复原生房间／入口。已检查自定义暂停和恢复原生的截图，测试布局不是正式美术。
- **原生回退完整UI流程1/1**，原断言保留，覆盖非法IPv4、实际Join取消后Host、选车／练习、真实跨图／倒计时／Playing输入及回入口。
- **真实独立进程双端连续两局，各1/1，0测试警告／失败**。Host使用原生界面，Client在自己的GameInstance使用内存UMG界面，走保存的PracticeRoom、Match地图及Room DA；身份／车型、新局状态、物理Epoch、共享倒计时、第二局及回入口正确。Client确认延迟2秒是屏障检查，不是实测RTT。
- **1183份保存资产逐份哈希全同**，Config、Build.cs及uproject未改；285份最终Source／Config／uproject哈希留档。没有代理Blueprint／地图／DA修改或新建，未提交／Push。

早期失败独立保留：自定义流程首次未等待实际按钮启用／焦点而超时；随后将SetUserFocus的“已经聚焦而返回false”误作失败，改为检查实际焦点及其后代，并保留真实UMG事件路径与权限断言。原生回退曾在Playing状态已变而定时器尚未更新输入时失败，已由既有房间通知立即同步界面／输入并加入重入保护，原测试不降断言；最终两Target、原生／UMG、129项及混合界面双端均在该修正后验证。

测试启动仅在自身进程使用`-NoGamepad -DisablePlugins=XInputDevice`隔离外设干扰，不修改正常游戏插件／输入配置。**内存UMG夹具不等同于实际新编译Widget Blueprint资产验收**；实际新建父类、Designer布局、事件／按钮接线、动画／图片、嵌套导航／子面板返回、真人键鼠／手柄／按住键／失焦、多窗口、打包及四机仍由团队按本文手动验证。3D预览／技能展示数据及阻塞加载幕另按需求建设。

当前为GF-5接口补充的代码／构建／程序验证完成，团队可开始制作正式Widget Blueprint。下一开发环节仍为GF-6，原F4／VP-4C／FX余项独立记账。
