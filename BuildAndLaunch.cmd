@echo off
setlocal
rem Standalone launcher: copy this file beside a .uproject. No Tools folder needed.
set "UE_LAUNCH_SELF=%~f0"
set "UE_LAUNCH_PAYLOAD=%TEMP%\UE-BuildAndLaunch-%RANDOM%-%RANDOM%.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText($env:UE_LAUNCH_SELF); $parts=$text -split '(?m)^# POWERSHELL_PAYLOAD\r?$',2; if($parts.Count -ne 2){exit 1}; [IO.File]::WriteAllText($env:UE_LAUNCH_PAYLOAD,$parts[1],(New-Object Text.UTF8Encoding($true)))"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File "%UE_LAUNCH_PAYLOAD%" -ProjectDirectory "%~dp0." %*
set "UE_LAUNCH_EXIT=%ERRORLEVEL%"
powershell.exe -NoProfile -Command "Remove-Item -LiteralPath $env:UE_LAUNCH_PAYLOAD -Force -ErrorAction SilentlyContinue"
exit /b %UE_LAUNCH_EXIT%
# POWERSHELL_PAYLOAD
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ProjectDirectory,
    [string]$EngineRoot,
    [string]$MSBuildPath,
    [string]$Platform = 'Win64',
    [switch]$CheckOnly,
    [switch]$NoLaunch,
    [switch]$NoPopup
)

$ErrorActionPreference = 'Stop'

function Find-EngineRoot([string]$Association) {
    $customBuilds = Get-ItemProperty 'HKCU:\Software\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
    if ($customBuilds) {
        $entry = $customBuilds.PSObject.Properties[$Association]
        if ($entry -and (Test-Path -LiteralPath ([string]$entry.Value))) { return [string]$entry.Value }
    }
    foreach ($key in @(
        "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association",
        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$Association"
    )) {
        $entry = Get-ItemProperty $key -ErrorAction SilentlyContinue
        if ($entry -and $entry.InstalledDirectory -and (Test-Path -LiteralPath $entry.InstalledDirectory)) {
            return [string]$entry.InstalledDirectory
        }
    }
    $manifestPath = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path -LiteralPath $manifestPath) {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        $entry = $manifest.InstallationList | Where-Object { $_.AppName -eq "UE_$Association" } | Select-Object -First 1
        if ($entry -and (Test-Path -LiteralPath $entry.InstallLocation)) { return [string]$entry.InstallLocation }
    }
    throw "EngineAssociation '$Association' に対応する Unreal Engine が見つかりません。-EngineRoot でこの PC のエンジンのインストール先を指定してください。"
}

function Find-MSBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $found = @(& $vswhere -latest -prerelease -products '*' -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\**\Bin\MSBuild.exe')
        if ($LASTEXITCODE -eq 0 -and $found.Count -gt 0) { return [string]$found[0] }
    }
    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    throw 'MSBuild が見つかりません。Visual Studio または Build Tools に「C++ によるデスクトップ開発」をインストールするか、-MSBuildPath で実行ファイルを指定してください。'
}

try {
    $projectRoot = (Resolve-Path -LiteralPath $ProjectDirectory).Path
    $projects = @(Get-ChildItem -LiteralPath $projectRoot -Filter '*.uproject' -File)
    if ($projects.Count -eq 0) {
        throw "このフォルダーに .uproject ファイルがありません。BuildAndLaunch.cmd をプロジェクトのルートフォルダーに配置してください。`n$projectRoot"
    }
    if ($projects.Count -gt 1) {
        throw "複数の .uproject ファイルが見つかりました。対象を特定できないため、このフォルダーには .uproject ファイルを 1 つだけ配置してください。`n$projectRoot"
    }
    $projectPath = $projects[0].FullName
    $projectName = $projects[0].BaseName
    $solutionPath = Join-Path $projectRoot ($projectName + '.sln')
    $project = Get-Content -LiteralPath $projectPath -Raw | ConvertFrom-Json
    if (-not $EngineRoot) { $EngineRoot = Find-EngineRoot ([string]$project.EngineAssociation) }
    $EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
    $buildScript = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
    $editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    if (-not $MSBuildPath) { $MSBuildPath = Find-MSBuild }
    $MSBuildPath = (Resolve-Path -LiteralPath $MSBuildPath).Path
    foreach ($required in @($buildScript, $editorPath, $MSBuildPath)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "必要なファイルが見つかりません。`n$required" }
    }
    $version = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
    Write-Host "Project:  $projectPath"
    Write-Host "Solution: $solutionPath"
    Write-Host "Engine:   $($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion) ($EngineRoot)"
    Write-Host "MSBuild:  $MSBuildPath"
    Write-Host "Config:   Development Editor | $Platform"
    $needsGeneration = -not (Test-Path -LiteralPath $solutionPath -PathType Leaf)
    Write-Host "Generate solution: $needsGeneration"
    if ($CheckOnly) {
        Write-Host 'Check only. No files generated, build executed, or Editor launched.'
        exit 0
    }
    if (Get-Process -Name UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw 'Unreal Editor または UnrealEditor-Cmd が実行中です。終了してから、もう一度実行してください。'
    }

    Push-Location -LiteralPath $projectRoot
    try {
        if ($needsGeneration) {
            Write-Host 'Generating Visual Studio solution...'
            & $buildScript '-ProjectFiles' "-Project=$projectPath" '-Game' '-Engine' '-VisualStudio'
            if ($LASTEXITCODE -ne 0) { throw "ソリューションの生成に失敗しました（終了コード: $LASTEXITCODE）。上のログを確認してください。" }
            if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) {
                throw "生成処理は終了しましたが、必要な .sln ファイルが作成されていません。`n$solutionPath"
            }
        }
        # Reproduce Build Solution in Visual Studio without opening the IDE.
        Write-Host 'Building solution...'
        & $MSBuildPath $solutionPath '/t:Build' '/p:Configuration=Development Editor' "/p:Platform=$Platform" '/m' '/nologo'
        if ($LASTEXITCODE -ne 0) { throw "ビルドに失敗しました（終了コード: $LASTEXITCODE）。上のログを確認してください。エディターは起動していません。" }
    }
    finally {
        Pop-Location
    }
    Write-Host 'Build succeeded.'
    if (-not $NoLaunch) {
        Start-Process -FilePath $editorPath -ArgumentList ('"{0}"' -f $projectPath) -WorkingDirectory $projectRoot
        Write-Host 'Editor launched.'
    }
    exit 0
}
catch {
    $message = "処理を中止しました。`n`n$($_.Exception.Message)"
    Write-Host "エラー: $message" -ForegroundColor Red
    if (-not $NoPopup) {
        try {
            Add-Type -AssemblyName System.Windows.Forms
            [void][System.Windows.Forms.MessageBox]::Show($message, 'ビルド・起動エラー', 'OK', 'Error')
        }
        catch { Write-Host 'エラーダイアログを表示できませんでした。上のエラーメッセージを確認してください。' }
    }
    exit 1
}
