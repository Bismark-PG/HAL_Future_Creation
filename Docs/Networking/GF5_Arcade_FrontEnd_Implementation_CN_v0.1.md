# GF-5：街机入口、选车表现与本地输入切换

日期：2026-11-03，Asia/Tokyo。基点`main/817e443e48bc8647862ff9ba74eb9c5098f93df9`，开工工作区干净，实际UE **5.8.3-58210709**。依据[GF分段确认基线](Room_VehicleSelection_Practice_Match_Construction_CN_v0.2.md)接续[GF-4](GF4_Round_Travel_Implementation_CN_v0.1.md)。

团队已提交正式`MVPB_Match`地图及两份Room DA更新，用户报告GF-4“初步测试暂时没有问题，后续会进行更多测试”。本轮记录为基础初测正向反馈，不追加未提供的逐项／网络扰动／四机通过结论；GF-4旧验证摘要不改写。本轮不编辑或保存任何BP、DA、地图。

## 1. 交付范围与代码职责

- 新增原生街机待机页、创建／加入页，包含IPv4输入、连接中、取消及错误后的重试入口；直接复用既有GameInstance的Host／Join，无在线服务或第二套连接规则。
- 四席位卡片显示编号、身份色、车型、选择／练习／载入状态及Host。选车主卡显示名称、说明与可替换的静态预览图；预览缺失仍能选车和驾驶。
- 手柄方向键／左摇杆导航、A确认／B返回，兼容键鼠。菜单关闭时清车辆／相机输入，只切换本车IMC，恢复时忽略仍按住的映射键，释放后再接受；不清其他系统的全部映射。
- 显示跨World转场提示、载入进度、服务器倒计时及正式阶段提示；错误信息给出玩家可执行的下一步。阻塞地图加载期间保持静态提示，不承诺持续动画或百分比磁盘进度。
- 名单、选车、出生、载入屏障、倒计时及权限仍由GF-1至4服务器代码决定；Widget只消费状态并提交本人的Controller请求。没有改变物理协议3、伤害／冲击、GA／GE或车型规则。

主要文件：`HALFrontEndConfig`集中品牌／主题；`HALArcadeEntryWidget`负责入口；`HALMenuWidgetBase`处理本地焦点与键位；`HALPracticeRoomWidget`及`HALPracticeRoomPresentation.cpp`输出房间／HUD；`HALNetworkPresentation.cpp`处理跨World静态提示；`TestVehiclePawn::SetMenuInputActive`切换本车输入。

首版车型预览是**静态图片卡片**，不生成可驾驶Pawn、ASC、碰撞或SceneCapture，也不参与权威出生。未配置时提供原生占位；实时旋转3D展示和最终美术可后续独立接入。

## 2. 无新增资产也可运行

完整编译并重开Editor后，从`MVPB_NetEntry`启动，GameInstance新增的 **Use Native Arcade Entry** 默认开启，自动使用原生入口，不需要改现有WBP Event Graph。旧Menu Widget Class保留；关闭该开关可回到旧Blueprint入口。原生选车界面沿用既有PracticeRoom接入路径。

以下引用保持：

| 所属 | 保持的引用／规则 |
|---|---|
| GI → Match Map Path | 继续指向`/Game/Maps/MVPB_PracticeRoom`，Host先进入房间 |
| Room GameMode BP | HALPracticeRoomGameMode父类，原Controller／GameState／BP_HALPlayerState及所测Room DA |
| Room DA → Match Map | 正式`MVPB_Match`，按GF-4启用Round Travel |
| Catalog → Vehicle Class | 既有两种车辆BP；不能以预览图代替该引用 |
| GA／GE／IA／IMC | 不需新建；菜单导航由C++处理，驾驶继续原Enhanced Input |

同机Client仍用127.0.0.1，异机使用Host局域网IPv4；入口只接受数字IPv4，不接受端口、命令或资产路径。

## 3. 可选品牌DA配置

1. Content Browser右键 → Miscellaneous → Data Asset → 选 **HALFrontEndConfig**，例如保存为`Content/Data/Gameflow/DA_FrontEnd_Default`。
2. 配置Title、Subtitle；Background与Logo可留空。背景建议横向16:9图片，Logo使用带透明背景的横向图。
3. Accent为标题／重点提示色，不覆盖服务器分配的四位玩家身份色。
4. Connection Timeout Seconds默认30秒，范围5～120秒，只控制加入房间时本地等待上限；GF-4的Load Timeout Seconds独立控制开局全员就绪等待。
5. 打开`BP_MVPB_NetworkGameInstance` → Class Defaults → Front End，将Front End Config指向此DA；保持Use Native Arcade Entry开启，编译并保存。

未指派DA时使用原生默认标题、配色和30秒连接上限。背景、Logo为DA直接引用；保存后重新进入入口／重新Host读取。首版不增加运行中样式热重载。

## 4. Catalog车型卡片配置

打开`DA_VehicleCatalog_Default`，每个Vehicles条目配置：

| 字段 | 作用 |
|---|---|
| Vehicle Id／Vehicle Class | 既有服务器许可及实际出生类，保持不变 |
| Display Name | 卡片显示名；为空时显示稳定Vehicle Id |
| Description | 新增的简短车型介绍，可留空使用通用说明 |
| Preview Image | 新增可选静态选车预览图，建议16:9、透明或干净背景 |
| Icon | 沿用可选图标；Preview Image为空／文件缺失时尝试此图 |

预览图按选择异步加载，切换车型和退出时撤销旧请求；不在Paint中同步加载。图片都缺失时显示原生占位，不影响Catalog驾驶配置有效性或服务器生成。素材可由美术后续补齐，本轮代理不会改写Catalog保存值；打包资源集合由GF-6核对。

## 5. 操作与团队验收

入口：Enter／手柄A／Start进入创建／加入页；方向键、Tab／Shift+Tab或手柄导航移动焦点，Enter／A激活，Esc／B返回。鼠标可直接操作按钮和地址框。手柄连接时可用系统支持的文本输入；IPv4调试也可先用键盘填入地址。

房间：左右键／手柄左右／肩键切换车型，上下导航按钮，确认后进入练习；驾驶中Tab／Esc／Start打开菜单，菜单内B／Esc／Start恢复，Tab用于焦点导航。返回选车用Change Vehicle，Host开始用Host: Start Match；加载及倒计时阶段只能等待或离房，UI不是权威权限来源。Development正式阶段保留Return To Room，Exhibition仍不提供开发返回按钮。

IPv4使用Slate文本框及平台可用的键盘输入；本阶段没有另建手柄屏幕数字键盘。手柄可完成页面／按钮导航和确认／返回，局域网地址可先用键盘填写；同机默认地址为127.0.0.1。

团队应按以下顺序补充真人测试，程序结果不能替代硬件手感：

1. 从入口仅靠UI完成单Host及双端加入、同／异车型、取消确认、练习、正式跨图、倒计时和退出；两种车型的显示图与实际出生应一致。
2. 在1280×720、1920×1080及项目目标宽高比检查卡片、四席位、提示、按钮焦点／悬停、加载／倒计时和车辆编号；长玩家名及长车型名也要检查。
3. 分别用鼠标、键盘和实际手柄导航／确认／返回；点击背景后恢复导航，不应丢焦点或跳到隐藏、禁用按钮。
4. 持续按油门／手刹／技能／发射时打开菜单，确认车辆不再持续接收原输入；按住确认键恢复驾驶，释放再按才能触发对应动作。检查右摇杆／鼠标相机输入无积累。
5. Alt+Tab失焦、重新激活、持球取消、换车、跨图及回入口检查旧IMC／UI／相机引用清理。联机不会因本地菜单暂停服务器。
6. 输入非法地址、不可达地址、连接中取消、房满、已开始、车型／出生失败和载入超时，检查可读原因与重试；真实断网、四机和打包仍属于GF-6完整验收。

## 6. 本轮验证与交接

最终共享结果及逐文件SHA-256见[验证与保护基线](GF5_Verification_20261103.json)，原始资料在`Saved/GameFlow/GF5_20261103`，不提交Saved／Binaries。

- 最终 **UE5.8.3 Editor／Game Development Win64构建成功**。现有引擎弃用及工具链建议警告保留，不宣称编译零警告；本轮开工Editor关闭，没有要求用户再次关闭。
- 最终 **127/127普通回归，0测试警告／失败／未运行**。新增两项验证可选图片不改变服务器车型许可／驾驶有效性、未知ID拒绝、默认主题无强制素材以及可读连接反馈。旧GF-3锁房夹具使用临时配置隔离新增的Round Travel开关，仍执行原始权限与就绪断言，没有改保存DA。
- 最终 **实际D3D12、1280×720完整UI流程1/1，0测试警告／失败**。通过真正的Slate焦点路径注入键盘方向／Enter及手柄A／B事件，覆盖待机页、非法IPv4、实际Join后取消、随后创建Host、选择及确认实际车辆、练习输入切换、菜单Host开始、实际正式图／倒计时／Playing及返回入口。没有用控制台开局或直接Possess代替该流程。
- 该UI测试仅在自身启动参数中使用`-NoGamepad -DisablePlugins=XInputDevice`隔离额外设备事件；没有改项目插件或正常游戏的手柄设置。这是受控事件／画面验证，不能写成物理手柄／鼠标真人验收。
- 最终 **真实独立Listen Host／Client连续两局，各1/1，0测试警告／失败**。从团队保存的PracticeRoom／BP／Room DA进入已保存的`MVPB_Match`，未用临时RoundMap替换；确认跨图身份／车型、新局状态／物理Epoch、共同倒计时、返回房间再开第二局及最终回入口。Client确认延迟2秒只测试等待屏障，不是磁盘慢机或实测RTT。
- 已检查待机／连接／非法地址、选车及倒计时画面；长名席位改为固定分行和截短，菜单打开时不再叠画世界车辆编号，加载／倒计时切换标题并突出显示等待／剩余时间。预览缺图占位可用。
- **1183份保存资产逐份SHA-256全同**；Config、Build.cs及uproject未改，278份最终Source／Config／uproject哈希留档。本代理没有保存BP／DA／地图，未提交／Push。

早期失败和修复独立保留：首两轮Slate语法／API、测试指针类型编译错误；旧GF-3夹具因保存DA启用跨图而失败，隔离配置后完整通过；额外XInput事件干扰既定测试，`NoGamepad`不足以隔离新插件轮询，故仅在测试启动禁用XInputDevice。取消实际待连接Join后Host曾因残留PendingNetGame导致无GameMode崩溃，已在延迟地图打开前显式CancelPending，并由最终UI流程真实复验；页面切换后隐藏按钮仍有焦点的问题已加可见／启用检查和重试。UI R5通过后看图发现裁切／标识穿透，再修正并重新编译、回归、UI及双端复验；未屏蔽错误或削弱断言。

尚未通过：团队GF-5实际手柄／鼠标／键盘、按住键释放时序、Alt+Tab／多窗口硬件工况、全部分辨率、真实车型图片／背景／Logo素材分支、PIE、打包资源、真实断线／网络扰动及四机。品牌和预览素材可选，真人接线及最终视觉由团队按第2～5节手动完成；初测前优先同步源码、完整重编，不混用旧DLL。

GF-5当前为**代码／构建／程序验证完成，团队硬件与素材验收待补**。下一开发环节为GF-6闭环、打包及四机；原F4／VP-4C／FX余项继续独立记账，不新增正式比赛计时／淘汰／结算。
