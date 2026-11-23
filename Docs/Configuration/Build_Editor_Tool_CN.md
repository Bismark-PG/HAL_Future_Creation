# 编译、重新构建并启动 Editor 工具

## 2026-11-22：构建显示统一为英语

按用户要求，BuildAndLaunch.cmd 自有的构建结果、耗时、动作数、未知统计、工程／文件检查错误及错误对话框标题均使用英语。结果示例为 `Build result: Up to date (no compilation needed)`；其余分类为 `Incremental build completed`、`Explicit rebuild completed`、`Build failed` 和 `Build succeeded (action statistics unknown)`。统计标签为 `Build invocation time`、`UBT time`、`Build actions`，时间单位为 `seconds`，缺失统计显示 `Unknown`。JSON 字段与取值、构建参数及行为不变；底层工具自行输出的诊断和路径按原样保留。

本轮同步更新原有输出断言，21/21 假引擎／进程检查通过；启动器 ASCII 显示文本检查与 git diff --check 通过。结果位于本机 `Saved/BuildTool/LauncherIntegration_97e1daf285004d04915ba202a4cf21e9/Results.json`。本轮仅改提示文本，没有再次执行真实 Unreal 编译或启动 Editor；下节中文输出保留为 P1 初次实施、英文化前的实测记录。

## 2026-11-22：P1 构建结果与耗时摘要

根目录 CMD 在 UBT 返回后读取本次独立的 `UnrealBuildTool.log`，在控制台和 `Console.log` 显示构建结果、构建调用耗时、UBT 耗时及动作数，并将统计追加到原有 `Result.json`。使用方式及 P0 构建参数不变。

| Result.json 新字段 | 含义 |
|---|---|
| BuildKind | `UpToDate`＝已是最新；`Incremental`＝增量编译完成；`Rebuild`＝显式重建完成；`Failed`＝构建失败；`Unknown`＝构建成功但动作统计未知；`NotRun`＝尚未进入构建（例如生成解决方案失败） |
| ActionCount | UBT 本轮调度的构建动作数，包括编译、链接、元数据等；从 executor 动作数或进度总数读取，无法识别时为 null。失败时的数量不代表全部动作已完成 |
| BuildSeconds | 调用 Build.bat／Rebuild.bat 的墙钟耗时，含工具启动与锁等待，不含生成解决方案、结果解析或 Editor 启动 |
| UbtSeconds | UBT 日志中的 Total execution time；无法识别时为 null |

分类以构建退出码为先；非零退出仍失败、不启动 Editor，显式 `-Rebuild` 成功单列为重建。普通 Build 的 0 动作显示“已是最新”，正动作显示“增量编译完成”。日志缺失或格式不识别时显示未知，不把未知当作 0，也不改变原生退出码或原有启动条件。后续 Editor 启动失败仍由原有 Status／ExitCode／Error 表示，BuildKind 保留已经完成的构建结果。

本轮在 UE5.8.3 CL58210709 运行 `BuildAndLaunch.cmd -NoLaunch -NoPopup`，真实输出为：

```text
构建结果: 已是最新（无需重新编译）
构建调用耗时: 1.97 秒（含工具启动与等待）
UBT 耗时: 1.65 秒
构建动作数: 0
```

退出 0，Result.json 为 BuildSucceeded／BuildKind=UpToDate／ActionCount=0／BuildSeconds=1.97／UbtSeconds=1.65，未发生 makefile 参数失效或 PCH 重编。本机日志为 `Saved/BuildTool/BuildAndLaunch_20261122_043248_695_298004`。

现有假引擎／进程夹具扩展为 **21/21 通过**，包含原有 13 项及零动作、进度回退、逗号小数、缺日志、未知格式、失败码优先、编译成功后启动早退、重建缺统计等检查。本机结果为 `Saved/BuildTool/LauncherIntegration_0f5392f9aa834661bcc36b4678aae45e/Results.json`。首轮新增中文输出断言因测试脚本无 UTF-8 BOM、被 Windows PowerShell5.1 按本地编码读取而失败；保留失败夹具，修正测试文件编码后完整复跑通过，CMD 的实际中文输出正常。

本轮真实验证覆盖当前产物的增量检查；正动作编译和显式 Rebuild 的摘要由假引擎夹具验证，没有为提示功能触发真实全量重建或修改玩法源码。未启动 Editor／PIE／联机或运行玩法自动化。P2 内存／并行度和 P3 源码依赖优化仍未实施。

## 2026-11-22：P0 统一代码验证与日常启动的构建参数

日常启动与代码代理的 Editor 编译验证必须使用同一个入口：

```powershell
# 代码验证／只编译；无人值守时可加 -NoPopup
.\BuildAndLaunch.cmd -NoLaunch

# 日常编译并打开 Editor
.\BuildAndLaunch.cmd
```

两条命令均由 CMD 构建 `HAL_Future_CreationEditor Win64 Development`，使用 `-WaitMutex -NoHotReload -NoLiveCoding`；`-NoLaunch` 仅控制成功后是否打开 Editor。保留现有关闭 Editor／游戏／Live Coding 后再构建的约束，不使用热更新。

本机已有日志确认，之前代码验证直接调用 `Build.bat` 时未带 `-NoLiveCoding`，而 CMD 带该参数。UBT 在两种入口切换时记录 `Old/New command line arguments` 在空值与 `-NoLiveCoding` 间变化；Core、CoreUObject、Engine 的 SharedDefinitions 新旧文件差异为 `WITH_LIVE_CODING=1/0`，随后 PCH 与大量项目文件重编。2026-11-22 三次 CMD 构建分别耗时 399.01、346.00、375.45 秒；2026-11-21 一次正常缓存命中仅 2.39 秒，零编译动作。这些是实施前的历史证据，不是本轮性能复测结果。

P0 不需要改写已有 CMD 或两个兼容适配器。已将入口与参数约束写入根目录 [AGENTS.md 第 14.1 节](../../AGENTS.md#141-本地-editor-构建统一入口)，供后续代理与换机接续执行。日常验证不再单独拼接 Editor 的 UBT 命令；需要独立 Game Target 或 UBT 诊断时单列用途和结果，直接构建相同 Editor Target 仍须保持上述参数及相同引擎／配置。

普通运行保留 UBT 自身的增量检查，不新增文件时间戳／Git 状态判断，不清理缓存、不默认 Rebuild，也不调整内存、并行度、Unity 或源码依赖。`-CheckOnly` 只是环境预检，不能判断产物是否最新。

验收时在无源码／配置变更、相同引擎与参数下，连续运行两次 `-NoLaunch -NoPopup`：后一次应显示 `Target is up to date`、零编译动作，且不再因 `-NoLiveCoding` 切换重建 PCH。首次若原产物来自其他参数，允许先做一次必要构建；实际耗时和本轮检查结果另记，不以历史数值作为承诺。

### 本轮真实增量构建验证

从干净的 `main/56e2223` 接续，本机 UE 5.8.3 CL58210709。只修改构建约定与说明，CMD、两个兼容适配器、Target／模块规则、玩法源码及资产均未修改。

| 验证 | 命令 | UBT 耗时 | 结果与本机日志目录（位于 Saved/BuildTool） |
|---|---|---|---|
| 第一次增量检查 | `BuildAndLaunch.cmd -NoLaunch -NoPopup` | 2.49 秒 | 退出 0，`Target is up to date`，0 actions；`BuildAndLaunch_20261122_042639_397_aa03e6` |
| 第二次增量检查 | 同上 | 1.68 秒 | 退出 0，`Target is up to date`，0 actions；`BuildAndLaunch_20261122_042655_184_01b812` |

两次均使用相同 Target／参数，没有参数变更导致的 makefile 重建或 PCH 编译，也未启动 Editor。表中为 UBT 的 `Total execution time`，不是包含 PowerShell 启动等开销的命令总耗时。本轮证明当前产物能连续复用，不代表任意后续源码改动均可零编译。

首次在受限沙箱内执行时停在引擎 Build.bat 锁文件等待，未进入 UBT；已中止该次进程，再在沙箱外完成上述两次验证，不把中止记录算作成功。

### 本轮脚本检查

运行现有 `Tools/Tests/TestBuildAndLaunch.ps1`，13/13 假引擎／进程夹具通过，覆盖构建参数、生成与构建失败码、占用阻止、Rebuild、模拟启动及早退。本机结果为 `Saved/BuildTool/LauncherIntegration_a86e9322845a48c7b8036db86f9c93f5/Results.json`。该检查不代表真实 Editor 启动或玩法自动化；本轮未运行 PIE、联机、独立 Game Target 或修改源码后的重编测试。日志属于本机生成证据，不作为工程运行依赖纳入 Git。

<a id="local-editor-mcp"></a>

## 本机编译并接入 MCP

在已配置的开发机上，双击工程里的 `Saved/LocalMCP/BuildAndLaunchWithMCP.cmd`。首次启动会调用根目录的 `BuildAndLaunch.cmd` 编译 Editor Target，成功后打开可交互的 Unreal Editor。当控制台显示 `MCP ready`，启动器已经通过 `http://127.0.0.1:8000/mcp` 成功读到当前关卡。Niagara、GAS、UMG、材质和场景工具都可从这条本机连接访问。

- **日常使用**　再次双击会复用已经接通 MCP 的项目 Editor。改过 C++ 需要重新编译时，先保存并关闭当前 Editor 和游戏窗口，再双击启动器。
- **只检查连接**　在工程根目录的 PowerShell 运行 `.\Saved\LocalMCP\BuildAndLaunchWithMCP.cmd -ProbeOnly`，可查看编辑器进程、工具集数量和当前关卡。
- **Codex 端**　本机已登记 `unreal_hal`。换设备时可用 `codex mcp add unreal_hal --url http://127.0.0.1:8000/mcp` 登记该机的服务，`codex mcp get unreal_hal` 可查看配置。两份启动器文件随本机保存在 `Saved/LocalMCP/`，Git 会忽略该目录。[Codex MCP 设置](https://developers.openai.com/codex/mcp/)
- **验证记录**　2026-11-19，UE 5.8.2（CL 56702186）编译成功。冷启动读到 `/Game/Maps/MVPB_NetTest` 和 28 个工具集，再次双击复用同一进程。新的 Codex 客户端发现 `list_toolsets`、`describe_toolset` 和 `call_tool`。本次日志在 `Saved/BuildTool/BuildAndLaunch_20261119_145708_386_45c41d/`，其中 `Result.json` 记录编译结果，`MCP.json` 记录关卡、进程与工具集，`EditorWithMCP.log` 记录实际调用。

## 2026-11-17：合并 Car08 后再次暴露的 Unity 私有函数重名

用户拉新到`main/deee273`，经本机`Saved/LocalMCP/BuildAndLaunchWithMCP.cmd`触发真实构建。日志`Saved/BuildTool/BuildAndLaunch_20261117_202045_072_501ca0/UnrealBuildTool.log`的首个错误为C2084：新增`HALSmokeScreenTypes.cpp`和既有`HALTornado.cpp`都在匿名命名空间中定义了`Range(float,float,float)`，并被包含到同一个`Module.HAL_Future_Creation.4.cpp`，后续C4551／C2568为连带错误。编译工具正确返回6，本机包装器因此没有启动Editor。

修复将四份运行时CPP中的同名范围校验函数分别改为`IsSmokeScreenValueInRange`、`IsTornadoValueInRange`、`IsPursuitValueInRange`和`IsArrestChainValueInRange`，避免另外两份旧实现随Unity分组变化再次冲突。逐文件反向替换核对：除辅助函数名称外内容一致（仅忽略换行格式），没有修改数值、校验边界、反射字段或技能逻辑；共享BuildAndLaunch、uproject及本机启动器不需要改动。

这也解释了开发设备先前的增量编译为何可能通过：Adaptive Unity会把正在编辑的源码拆出Unity文件；提交或拉新后重新组合，会暴露之前未一起编译的同名函数。本页顶部的合并前测试不能替代当前合并结果的验证。新增辅助函数继续使用明确的文件／系统相关名称，验证时覆盖`-DisableAdaptiveUnity`，不要依靠禁用整个运行时模块的Unity来回避问题。构建机制参考[Epic Build Configuration](https://dev.epicgames.com/documentation/en-us/unreal-engine/build-configuration-for-unreal-engine)。

本轮UE5.8.3-58210709已完成：Editor Target普通Unity编译通过（`-DisableAdaptiveUnity`，exit 0；实读确认黑烟和龙卷风在同一合并CPP）；黑烟3项、龙卷风2项、通缉与铁链5项原生专项共**10/10通过**，0测试警告／失败／未运行。使用NullRHI，没有重新验收视觉／多人或打包，保留既有MSVC优选版本、引擎弃用和浮点转换警告。默认一键启动的实测结果及文件保持核对见[本轮验证留档](Build_Unity_Range_Fix_Verification_20261117.json)。

## 2026-11-15：本次报错是项目的 Unity 函数重名，已修复

用户常规入口日志`BuildAndLaunch_20261115_194627_009_688359`中，首个错误为C2084：`HALDamageFeedbackConfig.cpp`与`HALPolicePursuit.cpp`在匿名命名空间内各自定义了`Range(float,float,float)`。Unreal为加快编译将两份CPP合并到`Module.HAL_Future_Creation.3.cpp`，两者在这个编译单元内不再隔离，产生重复定义，随后连带出现C4551／C2568。BuildAndLaunch正确保留UBT退出6，并停止启动Editor；本次无需修改工具。

修复将新受击配置的三个辅助函数改为`DamageFeedbackInRange`、`DamageFeedbackColorValid`、`DamageFeedbackFade`，不更改函数计算、字段或参数。截图中的C4864是另一个非致命警告：警车HUD的泛型Lambda内调用模板成员；将Lambda参数明确写为`const FHALPursuitView&`，消除依赖类型歧义，筛选行为保持。

本次UE5.8.3-58210709真实验证：

- 原入口`.\BuildAndLaunch.cmd -NoLaunch -NoPopup`成功，`Result.json`为BuildSucceeded／BuildExitCode=0／ExitCode=0，没有启动交互Editor。
- 额外对Editor及Game Target使用`-DisableAdaptiveUnity`构建，均Succeeded／退出0。运行时模块保留普通Unity构建，受击配置、警车逻辑、HUD明确在同一合并CPP中；没有用整个运行时模块禁用Unity来绕过冲突。Editor工具模块保留其此前的非Unity设置。
- 在上述Unity Editor模块执行相关原生＋真实D3D12专项 **11/11通过**，测试警告／失败／未执行均0，覆盖受击配方／脉冲／实际扣血／遮挡／清理、通缉／六星／HUD及铁链。
- 本轮开工1337份Content／Config／uproject和BuildAndLaunch文件 **1337/1337哈希一致**。没有代改BP、地图、技能／表现DA或工具脚本。保留MSVC优选版本及既有UE弃用警告，不把构建成功写作零编译警告。

之前受击初版的95项／双端结果是各自历史证据，本轮未重跑完整95项、多人、Cook或打包，也未重新验收交互Editor启动。详细留档见[Unity兼容修复验证](Build_Unity_Fix_Verification_20261115.json)。

日常继续使用根目录`BuildAndLaunch.cmd`。今后新增CPP内辅助函数采用明确的文件／模块相关命名；仅通过自适应增量构建可能漏检Unity重名，开发收尾需要同时覆盖关闭自适应拆分的普通Unity组合。`-DisableAdaptiveUnity`是本轮直接传给UBT的验证参数，没有加入启动器日常默认参数或改动全局构建配置。

## 2026-10-30：重新整合后的当前入口

同日车辆蓝图引用显示补充：Input预览的Editor C++现已加入当前源码，由本入口构建的现有Editor模块注册；旧Build_Editor.cmd已由用户删除，无需恢复。运行`BuildAndLaunch.cmd`即可按需编译这些新源文件；`-Rebuild`仍可用于官方完整重新构建。已有Car_02与新生成车型使用同一套只读读取，详见[车辆输入引用显示](Vehicle_Input_Preview_CN_v0.1.md)。主工程新版DLL已完成编译／链接；UBA启用时收尾阻塞，最终以`-NoUBA`原生构建核对返回Succeeded／0。脚本隔离检查重新13/13通过，六项UE自动化及蓝图界面验收仍待完成；详细证据见引用显示说明的主工程收尾核对，不将隔离夹具或编译成功记作界面验收。

当前统一使用根目录 **BuildAndLaunch.cmd**。此前在干净的`main/945e616`重新核对，实际文件仍为旧的整份解决方案MSBuild流程，已恢复独立整合版并重新验证。旧`Build_Editor.cmd`现已删除；根目录统一入口不引用它或Tools目录。`Tools/BuildEditor.ps1`现在仅转发到统一入口的`-NoLaunch`模式。

完整构建参数现明确包含`-NoHotReload -NoLiveCoding`，保留旧入口禁用Hot Reload的行为；默认编译和`-Rebuild`都生成正常Editor模块。生成器和Input预览同属该模块，普通运行本入口即可编译新增源码并在下次Editor启动时注册，无需单独的编辑器重构脚本。

| 操作 | 统一入口 |
|---|---|
| 编译成功后打开Editor | `BuildAndLaunch.cmd` |
| 只编译 | `BuildAndLaunch.cmd -NoLaunch` |
| 只检查工程／引擎／目标／进程占用 | `BuildAndLaunch.cmd -CheckOnly` |
| 官方重新构建 | `BuildAndLaunch.cmd -Rebuild -NoLaunch` |
| 结果窗口等待按键 | 添加`-Pause` |
| 指定引擎或Editor Target | 添加`-EngineRoot`或`-EditorTarget` |

整合内容包括引擎注册／Launcher／默认路径定位、实际版本与CL显示、Source目标定位、Editor／游戏／Live Coding占用检查、逐次日志／Result.json、真实构建退出码保留、失败不启动，以及成功后自动打开Editor和0～30秒启动早退观察。保留缺少同名解决方案时的生成能力；实际编译直接调用目标引擎Build.bat构建项目Editor Target。`-MSBuildPath`只作兼容提示，不再编译整份解决方案。

每次实际生成／编译的日志位于`Saved/BuildTool/BuildAndLaunch_日期时间_短ID`，包含Console.log、Result.json和由Unreal写入的构建／启动日志。CheckOnly与前置失败不建立构建日志目录。`-NoPopup`仅输出控制台错误；`-Pause`为可选交互行为，默认不阻塞终端。显式选择与EngineAssociation不同主次版本时显示警告，由UnrealBuildTool检查兼容性。

本机实际UE5.8.3-58210709。本次Windows PowerShell5.1语法检查、**13/13独立假引擎／进程检查**及真实路径CheckOnly通过。测试覆盖单文件便携、生成／编译及其失败码、已有解决方案、空目标、三类占用、Rebuild和启动成功／早退。启动进程为模拟句柄，宿主进程在工具沙箱中不可完整枚举，不能把CheckOnly返回0当作宿主Editor已退出。**本轮未执行真实Unreal编译、Editor启动或玩法／界面验收。** 可复测入口为`Tools/Tests/TestBuildAndLaunch.ps1`，本轮证据见[重新整合验证](Build_And_Launch_Reintegration_Verification_20261030.json)。

以下2026-10-20／28内容保留各自历史含义。当前入口及命令以上节为准，历史构建通过不代替本次真实构建验证。

## 2026-10-28：BuildAndLaunch.cmd修复与用法

推荐入口为工程根目录 **BuildAndLaunch.cmd**，可以单独复制到其他同名C++ Editor Target工程的`.uproject`旁；不依赖Tools目录、Visual Studio界面、已有`.sln`或`.slnx`。保留原有Build_Editor.cmd作为只编译的旧入口，两者行为不同。

旧BuildAndLaunch已在本机复现：它调用整份解决方案MSBuild，把安装版引擎的AutomationTool、规则与插件C#工程一起构建，因缺少NuGet `project.assets.json`报NETSDK1004，共57个错误。修复改为引擎自带的Build.bat，仅构建本工程`HAL_Future_CreationEditor Win64 Development`；不为绕开错误去恢复或重编无关引擎C#工程。

### 推荐命令

先保存并关闭Editor、独立游戏、Live Coding。在工程根目录打开PowerShell：

```powershell
# 按需编译成功后打开Editor
.\BuildAndLaunch.cmd

# 官方Rebuild，成功后打开Editor
.\BuildAndLaunch.cmd -Rebuild

# 只编译，成功后不启动
.\BuildAndLaunch.cmd -NoLaunch

# 强制重新构建，但暂不启动
.\BuildAndLaunch.cmd -Rebuild -NoLaunch

# 只检查工程、引擎和占用情况，不构建、不启动
.\BuildAndLaunch.cmd -CheckOnly
```

找不到引擎或需要选定本机安装时：

```powershell
.\BuildAndLaunch.cmd -EngineRoot 'D:\Program Files\Epic Games\UE_5.8'
```

cmd.exe中将路径改用双引号。可以从其他工作目录调用脚本的绝对路径；工程位置始终取自脚本所在目录。工具读取`.uproject`的EngineAssociation，按注册表／Launcher记录查找本机引擎，并输出实际补丁版本；显式路径也须与工程主次版本匹配。`5.8`不锁定5.8.2或5.8.3，联机仍须团队统一实际补丁与程序。

普通模式使用Build.bat按需更新；`-Rebuild`调用同一引擎的官方Rebuild.bat（UE5.8转交UBT `-Rebuild`），重建本Editor Target的编译产物。工具不手工删除Content、Saved、DDC或.vs，不保存蓝图／DA／地图，不打包或生成独立Game Target。

### 失败处理与日志

- 前置检查会列出仍运行的UnrealEditor、LiveCodingConsole和本项目游戏进程；返回1，不强制关闭进程，不在已加载DLL上构建。检查保守覆盖其他工程Editor。
- Native构建失败时保留真实退出码，例如UBT返回6则脚本也返回6；绝不启动Editor。修复后默认只在终端显示错误，`-NoPopup`保留兼容。
- 每次实际构建写入`Saved/BuildTool/BuildAndLaunch_日期时间_短ID/`，包含`Console.log`、`UnrealBuildTool.log`、`Result.json`，启动时另有`Editor.log`。前置失败／CheckOnly不创建项目日志目录。
- 启动后默认等待5秒；如果Editor已退出，返回1并提示Editor.log。进程仍运行会记录PID、返回0；这只是成功发起启动，不保证地图／资产／玩法验收。可用`-LaunchWaitSeconds 10`调整0～30秒的早退观察窗口。
- 不再需要MSBuildPath；旧命令传入`-MSBuildPath`会提示弃用并忽略。真正的Visual Studio C++工具链与Windows SDK仍由UnrealBuildTool选择和检查。

建议在已有终端运行以保留前置错误。工具不额外pause阻塞命令行；双击成功后会打开Editor，构建失败时可查上述日志。

### 本轮实际验证

本机UE5.8.3-58210709、Windows PowerShell5.1：从工程外目录预检及普通Build通过；真实`-Rebuild -NoLaunch`成功（116.20秒），随后默认入口再次Build成功并启动交互式Editor PID49912，日志确认Engine初始化与Startup complete，主窗口标题为HAL_Future_Creation。工具在该Editor／LiveCoding仍打开时能正确拒绝再次编译、返回1；没有关闭用户窗口。

独立假引擎夹具在含中文、空格、`&`的路径，且没有Tools目录／.sln的情况下检查了5项行为：预检0、构建失败保留6且不启动、NoLaunch成功0、Rebuild选择正确入口成功0、Editor立即退出时返回1。夹具不代表真实C++构建，真实构建证据单列。

1167份保存资产和197份Source／Config／uproject相对本次开工基线全同；只修改工具／说明／进度留档。保留现有MSVC优选版本及引擎启动的可选DLL／widget等警告，没有将整份日志写成零警告。没有重跑85项玩法回归、进入PIE、双端／四机或验收新Car_02。完整结果见[本轮验证与进度核对](Build_And_Launch_Verification_20261028.json)。本轮未提交／Push。

## 原有 Build_Editor.cmd（只编译）

更新：2026-10-20。入口为工程根目录 `Build_Editor.cmd`，实现为 `Tools/BuildEditor.ps1`；两份脚本及本说明应随 Git 同步。

## 日常操作

1. 保存需要保留的工作，关闭 Unreal Editor、PIE／Standalone／独立游戏窗口。Live Coding 窗口也应退出；脚本不会替你关闭或强制结束进程。
2. 在资源管理器中双击根目录 **Build_Editor.cmd**。通常不需要管理员权限。
3. 检查窗口显示的工程位置、引擎位置和实际版本。工具编译 `HAL_Future_CreationEditor / Win64 / Development`，不通过 Live Coding。
4. 等待 **BUILD SUCCEEDED**、退出码 **0**，再打开 `.uproject`。如果原先 ABP 因原生父类缺失而无法加载，此时重新打开并检查；编译成功本身不代表资产／运行验收通过。
5. 编译失败时，窗口会保留错误，日志在 `Saved/BuildTool/BuildEditor_时间.log`。把该次日志交给开发者排查，不要在缺少父类时重建或重新指定 ABP 的父类。

正常 Build 会按需要重编变更的 C++／反射代码及依赖；工具不执行 Clean，不删除缓存，不打包游戏，不自动启动 Editor，也不修改蓝图／DA／地图。它只生成 Editor Target；独立 Game 可执行文件需要另行构建 Game Target。

## 换电脑和引擎定位

工程位置取自脚本所在目录，不固定盘符。引擎依次从 `.uproject` 的 EngineAssociation 对应的用户源码构建注册、系统安装注册、Epic Launcher 安装记录和默认安装目录查找；找到后读取 `Engine/Build/Build.version` 并显示实际补丁版本。

`EngineAssociation="5.8"` 不锁定 5.8.2／5.8.3。工具不会升级引擎或替团队决定补丁，所有成员仍应统一实际版本。Visual Studio C++ 工具链和 Windows SDK 等编译环境也须由本机安装；本工具不负责安装。

如果自动定位失败或需要明确选择另一份安装，在工程根目录打开 PowerShell，执行以下命令；将示例路径替换为本机引擎根目录（该目录中包含 `Engine`）：

```powershell
.\Build_Editor.cmd -EngineRoot 'D:\Program Files\Epic Games\UE_5.8'
```

仅检查工程、引擎位置和进程前置条件，不编译、不写编译日志：

```powershell
.\Build_Editor.cmd -CheckOnly
```

脚本保守检查所有 `UnrealEditor*`、`LiveCodingConsole` 和 `HAL_Future_Creation*` 进程；其他工程的 Editor 也会阻止本次构建。先正常保存退出，再运行工具。成功时退出码为 0，前置条件失败为 1，实际编译失败保留 Unreal 构建返回码；双击入口在结束后等待按键，便于阅读结果。

## 本次验证边界

本机已用 Windows PowerShell 5.1 执行只检查模式，正确定位 UE 5.8.2 CL56702186，并在 Editor 仍打开时拒绝构建、返回 1；语法检查通过。批处理统一为 CRLF 后，从工程以外目录启动、传入带空格的引擎路径也得到相同结果，最终双击入口正确保留退出码 1；无效引擎路径同样明确报错、返回 1。

本轮未执行真实 C++ 编译，未验证编译分支的日志／成功返回码，也未确认 ABP_Car01 已恢复。关闭 Editor 后首次成功构建和资产重新加载结果需另记。

后续同日更新：用户反馈ABP已能打开；随后独立凸包生成工具开发中，代理使用Build.bat完成本机Editor／Game构建，相关日志和验证见[凸包工具说明](Vehicle_Collision_Generator_CN_v0.1.md)。这更新了项目构建状态，但不作为本双击脚本完整成功分支已直接测试的证据。
