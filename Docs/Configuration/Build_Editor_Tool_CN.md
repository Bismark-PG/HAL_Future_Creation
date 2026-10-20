# 双击编译 Editor 工具

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
