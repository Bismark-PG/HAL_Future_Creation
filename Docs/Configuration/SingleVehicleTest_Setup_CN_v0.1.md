# 单车离线测试场景与观察相机

日期：2026-11-10。适用现有 `BP_VehicleTestGameMode` 与 `SingleVehicleTest`。运行逻辑由原生C++提供，蓝图仅配置车型和框架类。代理不修改保存的Blueprint或地图。

## 1. 编译与资产接线

新增反射类需要关闭Editor和Live Coding后完整构建Editor Target。编译后重新打开 `BP_VehicleTestGameMode`：

1. **Class Settings → Parent Class** 选择 `HALVehicleTestGameMode`。
2. **Class Defaults → Classes** 核对下表。复制自联网GameMode的蓝图可能保留旧类覆盖值，重新指定这些类。

| 属性 | 指定值 |
|---|---|
| Player Controller Class | `HALVehicleTestPlayerController` |
| Player State Class | `HALPlayerState` |
| Game State Class | `HALMatchGameState` |
| Default Pawn Class | **`BP_VehiclePawn_Car_02`** |
| Vehicle Test → Test Vehicle Classes | 加入需要切换的车辆BP，例如Car_02、Car_03、Car_04 |

Default Pawn Class自动包含在运行时列表中，无需重复添加。候选类必须继承 `TestVehiclePawn` 并具有有效Definition、输入、Impact及GAS配置。工具没有写死车型资产路径；初始Car_02由本蓝图显式配置。无需Widget Blueprint或Level Blueprint事件。

3. 编译并保存 `BP_VehicleTestGameMode`。
4. 打开 `/Game/Maps/SingleVehicleTest/SingleVehicleTest`，**World Settings → GameMode Override** 选择 `BP_VehicleTestGameMode`，保存地图。
5. 场景保留一个PlayerStart或HALPlayerStart。HALPlayerStart的Spawn Slot可为0；新模式不依赖联网席位分配。起点应位于实际碰撞体可以完整放入的位置，Z至少覆盖 `地面Z + 碰撞体半高 + 适当余量`。
6. Play选项使用 **Number of Players=1、Net Mode=Play Standalone**。可以使用Selected Viewport或独立游戏窗口；相机按钮和F2均不依赖F8。

GameInstance保持现有项目配置。专用Controller跳过联网大厅、房间UI、网络就绪轮询及比赛观战更新；不会执行Host、Join或ServerTravel。测试GameMode直接设置Playing，不创建Room、倒计时、胜负判定或比赛截止时间。测试世界的车辆不启用Network Physics History。Play Standalone可同时避免UE自身创建Listen Server网络驱动。

## 2. 左上角测试面板

Play后自动显示原生面板，包括车型下拉框、Apply selected vehicle、相机切换、侧面聚焦、当前Pawn、GAS就绪和HP。

| 操作 | 行为 |
|---|---|
| 选择车型 → Apply selected vehicle | 生成配置有效的新车型，通过现有GAS生命周期切换，再销毁旧车辆 |
| Switch to observation camera / F2 | 切到独立侧面观察相机，车辆仍由原Controller控制 |
| Switch to vehicle camera / F2 | 返回当前车辆原有Local Camera |
| Focus vehicle from the side | 相机重新定位到当前车辆侧面，俯角约10° |
| 观察模式按住右键 + WASD | 相机前后左右移动，鼠标旋转观察方向 |
| 观察模式右键 + Q / E | 相机降低／升高；Shift加速 |
| 松开右键 | 释放飞行相机输入，车辆恢复通常输入；新加入的映射会等待仍按住的按键释放 |

观察模式没有按住右键时，车辆原有键盘／手柄输入仍可使用，可以从侧面观察车辆运动。右键飞行期间，键盘输入临时交给相机，车辆获得中性输入。此切换不会解除Possess、替换GAS Avatar、回血或清除冷却。车辆相机的鼠标观察在右键按住时生效；面板鼠标可见时手柄观察仍可用，鼠标停留面板上会避免误操作镜头。

已有驾驶、刹车、手刹、发射、主技能、自救及相机Action仍由车型Local Config读取。测试模式跳过房间Ready／网络确认门槛，但保留配置有效性、真实GAS绑定、物理绑定就绪及技能自身冷却／眩晕等玩法条件。测试模式没有赋予未配置的技能，也不会绕过角色配置错误。

## 3. 换车行为与失败处理

- 每次应用车型都在PlayerStart重新生成一辆车，物理状态重新开始。PlayerController、PlayerState和ASC保留。
- 复用 `HALPlayerState::ChangeVehicleAvatar`，清理旧车技能来源、Avatar效果、冷却、球权、区域状态与输入，按新车型种子初始化属性。
- 独立观察相机在换车后继续存在，并重新聚焦新车；车辆相机模式则切到新车的Local Camera。
- 成功切换后，运行时销毁该测试世界中的其他TestVehiclePawn和PassiveTestVehicle，使复制地图中遗留的车辆不继续参与测试。保存地图中的Actor不受修改。
- 配置、候选列表或出生碰撞预检失败时保留当前车辆，原因显示在左上角。出生检查使用实际Chaos碰撞根，并忽略即将被替换的测试车辆。
- 极端情况下GAS提交后失败，状态栏要求重新选择或重开Play，日志保留原因；不把已经Retired的旧Avatar重新接回ASC。

## 4. 程序与验证入口

| 文件 | 职责 |
|---|---|
| `HALVehicleTestGameMode` | 单车生成、Playing初始化、候选车型与GAS换车 |
| `HALVehicleTestPlayerController.h/.cpp` | ViewTarget切换、侧面聚焦、观察相机输入，以及`.cpp`内部的左上角Slate面板 |

左上角面板已合并为Controller实现文件内的私有Slate类，直接挂接本地玩家视口并在EndPlay移除。独立的`HALVehicleTestWidget.h/.cpp`已删除，无需Widget Blueprint或额外可配置Widget类。车型、相机按钮、侧面聚焦和GAS／HP显示保持原接口。

测试Controller保留对`HALPlayerController`的继承，以复用真实GAS换车生命周期和已有调试命令。它在自身的`BeginPlay`、`SetupInputComponent`、`TickActor`与`PostSeamlessTravel`中显式调用UE的`APlayerController`实现，跳过大厅界面、房间事件订阅／就绪轮询、房间菜单按键和比赛观战更新。清理仍经过公共Controller的`EndPlay`，释放继承的调试命令可能创建的资源。

公共`HALPlayerController.h/.cpp`、`HALPlayerControllerRound.cpp`与`HALGameFlowPermissions.h/.cpp`保持仓库原实现。测试世界识别放在`HALVehicleTestGameMode`，Room Ready例外及光标／鼠标策略放在`HALVehicleTestPlayerController`的静态辅助函数中。公共车辆和动态相机保留少量辅助函数调用；例外仅针对专用测试Controller，本地Room Ready例外还要求处于测试GameMode。原有联网Controller继续执行原有门槛，测试功能没有增加公共Controller虚函数或公共流程接口。

新增自动化过滤器 **`HAL.FutureCreation.VehicleTest`**，四项测试覆盖Standalone初始Car_02、Playing／GAS／全部输入权限、真实生命周期换车、侧面镜头保留Possess与冷却、错误配置／阻挡出生时保留旧车。公共Controller隔离调整还覆盖测试启动／跨图后不订阅房间UI、普通Controller仍订阅房间UI并保留Room Ready与光标门槛，以及右键启用／释放车辆鼠标观察。

团队实测可依次检查：Play后左上角出现面板且只有一辆Car_02；驾驶、刹车、手刹、发射、技能及自救可用；按钮和F2在车辆／观察镜头间往返；观察视角可看清车身侧面；右键飞行和释放后驾驶输入正确；Car_02与其他候选往返切换后GAS和镜头仍绑定当前车；运行超过正式比赛时长仍维持Playing，不自动转场。

面板合并前，本机UE5.8.3-58210709的Editor R3与Game R3均构建成功，四项新增测试已编译。早期DLL占用解除后主工程DLL已写入；此前独立模块链接保留原时点。专项启动进程退出0，但没有生成UE日志或自动化报告，因此四项测试、实际面板点击、相机输入及驾驶结果均未记为通过。合并面板后Editor／Game重新构建通过，源码和链接清单不再依赖旧Widget；1224份保存资产及四份Config与合并前基线逐份一致。本轮没有新增测试文件或重跑自动化，实际面板操作仍待团队验证。

构建与自动化的本次实际结果单独记录于 `SingleVehicleTest_Verification_20261110.json`。团队完成第1节接线后可在Session Frontend运行上述过滤器，再执行实际输入和镜头验收。

2026-11-10公共Controller隔离调整后，UE5.8.3-58210709的Editor R3与Game R1均重新编译、链接成功。四项现有测试已扩充房间UI订阅、普通Controller的Room Ready／光标规则及右键观察检查，没有新增测试文件。两次以单车测试与相机过滤器启动的进程均退出0，但没有生成UE日志或自动化报告，因此四项单车测试与九项相机测试的实际执行及结果均未验证。1235份保存资产、Config、公共Controller头文件及构建配置的保护基线保持一致。构建日志与启动结果位于`Saved/VehicleTest/20261110/ControllerIsolation`；初次批处理临时目录和UBA缓存写入问题通过仅针对本次进程指定工程内目录解决，没有修改全局或引擎配置。
