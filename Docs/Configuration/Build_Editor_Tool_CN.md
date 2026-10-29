# 编译、重新构建并启动 Editor 工具

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
