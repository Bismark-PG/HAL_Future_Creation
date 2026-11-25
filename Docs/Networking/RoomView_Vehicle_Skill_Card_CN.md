# 选车 UI 速查 · 从房间引脚接到主技能卡

**改选车页，打开 `WBP_GF_VehicleSelection → ApplyView`。** 房间根控件 `WBP_GF_Room` 已把最新的 `HALRoom View` 传到这里。画车辆列表用 `Vehicles`，查当前车型用 `Selected Vehicle Id`，按钮是否可点直接接对应的 `Can…`。现有 3D 预览也用这个 ID 调用 `BP_VehiclePreview → SetPreviewVehicle`。

## 截图里的引脚

左侧输入 `HALRoom View` 是**当前本地玩家**看到的一份只读房间快照。右侧各引脚可这样接到 UI。 [视图定义](../../Source/HAL_Future_Creation/HALFrontEndViewTypes.h) · [取值逻辑](../../Source/HAL_Future_Creation/HALRoomWidgetInterface.cpp)

| 引脚 | 含义与用法 |
|---|---|
| `Room Available`、`Catalog Loaded` | 房间组件和本地玩家数据已就绪、车辆目录已载入。目录就绪时可以画列表，图片仍可异步加载。 |
| `Menu Visible`、`Host`、`Request Pending` | 本地菜单是否打开、自己是否为房主、上次房间请求是否还在等服务器回复。 |
| `Can Select`、`Can Confirm`、`Can Cancel` | 分别接上一辆／下一辆、确认选车、撤回确认并重新选车的按钮。 |
| `Can Start`、`Can Resume`、`Can Return To Room`、`Can Leave` | 分别控制房主开赛、本地收起菜单继续驾驶、房主带全员返房、本人离房。 |
| `Phase`、`Local Phase` | 整个房间的阶段（选车开放、加载、倒数、比赛、结算等），以及本人的阶段（选车中、正在生成车辆、已就绪等）。 |
| `Local Number`、`Local Color` | 本人的 P1～P4 编号和标识色。编号 0 表示尚未分配。 |
| `Selected Vehicle Id`、`Selected Vehicle Name`、`Description` | 服务器已接受的本地选车 ID（如 `Car_03`）、显示标题、说明文字。选车时读车型文案，加载或结算时文字可随页面变化。 |
| `Loaded Preview`、`Status` | 已载入的静态预览图，优先 `PreviewImage`，再用车辆 `Icon`，两者都空时可显示占位图。`Status` 放操作提示、等待或拒绝原因。 |
| `Minimum Players`、`Loaded Players`、`Expected Players`、`Round Id` | 开赛最低人数、本局已回报加载完成的人数、应回报人数、本局编号。加载页用“已加载／应加载”。 |
| `Countdown Remaining`、`Match Remaining`、`Results Remaining` | 分别为开赛倒数、比赛剩余、结算后自动返房的剩余秒数，均按服务器时间计算。 |
| `Match`、`Show Results`、`Result Summary` | `Match` 可继续拆出每位参赛者的 HP、淘汰和胜者信息。`Show Results` 控制结算页显隐，`Result Summary` 可直接接结算文本。 |
| `Spectating`、`Can Cycle Spectator`、`Spectated Number` | 自己是否在观战、能否切换目标、正在看的玩家 P 编号。0 表示当前无目标车辆。 |
| `Seats`、`Vehicles` | `Seats` 是四个玩家座位，供参与者列表使用。`Vehicles` 是可选车型的展示条目，含 ID、车名、车辆简介、车辆图标和静态预览图。 |

## 给每辆车加主技能卡

**目前能直接填的是车辆资料。** 在 `/Game/Data/Gameflow/DA_VehicleCatalog_Default → Vehicles` 里，车辆 BP 旁边已有 `DisplayName`、`Description`、`Icon` 和 `PreviewImage`。2026-11-25 在 Editor 读回的正式目录含 `Car_01` 至 `Car_04`。技能纹理已可单独使用，按当前选车自动切换技能名、图标和简介仍需独立的展示字段。[目录结构](../../Source/HAL_Future_Creation/HALVehicleCatalog.h) · [车型与技能图标对照](../Presentation/Vehicle_Skill_Icons_CN.md)

| 谁来做 | 操作 |
|---|---|
| 程序 · 待开发 | 新增可拆分的 `FHALPrimarySkillDisplay`，含 `Name`（文本）、`Icon`（Texture2D 软引用）、`Summary`（多行文本）。在目录条目 `FHALVehicleCatalogEntry` 加可选的 `PrimarySkill`，在 `FHALVehicleDisplayView` 暴露同名只读字段，并在 `GetRoomView()` 组装每辆车时复制该字段。原生回退和自定义 WBP 都从同一份视图取值。 |
| 策划／美术 · 字段上线后 | 打开同一目录，找到对应 `VehicleId → Primary Skill`，填入技能名、图标、一句话简介。例如 `Car_03` 可填「战斗陀螺」和「旋转时撞开接触的车辆，碰到球时把球高速甩出。」这里管理展示资料，实际授予哪个技能仍由玩法配置决定。 |
| UI · 接在现有 `ApplyView` | 新增两个 Text 和一个 Image。遍历 `Vehicles`，用 Name 相等节点比对条目的 `Vehicle Id` 与 `Selected Vehicle Id`。命中后拆开 `Primary Skill`，两个文本接 `Set Text`，图标接 `Set Brush from Soft Texture`，随后结束遍历。 |

**验收**　连续切换两辆车，车名、3D 模型和技能卡应对应同一个 ID。切到未填资料的车时清空旧文案并隐藏技能卡，单独缺图时显示占位图。若自写异步加载回调，回填图标前再核对当前 ID，避免慢一拍的旧图覆盖新车。
