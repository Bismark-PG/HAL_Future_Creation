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
    [string]$EditorTarget,
    [string]$Platform = 'Win64',
    [switch]$CheckOnly,
    [switch]$NoLaunch,
    [switch]$NoPopup,
    [switch]$Rebuild,
    [switch]$Pause,
    [ValidateRange(0, 30)][int]$LaunchWaitSeconds = 5
)

$ErrorActionPreference = 'Stop'

function Find-EngineRoot([string]$Association, [string]$OverrideRoot) {
    $candidates = @()
    if ($OverrideRoot) { $candidates += $OverrideRoot }
    else {
        if (-not $Association) { throw 'EngineAssociation is empty. Specify -EngineRoot.' }
        $customBuilds = Get-ItemProperty 'HKCU:\Software\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
        if ($customBuilds) {
            $entry = $customBuilds.PSObject.Properties[$Association]
            if ($entry) { $candidates += [string]$entry.Value }
        }
        foreach ($key in @(
            "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association",
            "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$Association"
        )) {
            $entry = Get-ItemProperty $key -ErrorAction SilentlyContinue
            if ($entry -and $entry.InstalledDirectory) { $candidates += [string]$entry.InstalledDirectory }
        }
        if ($env:ProgramData) {
            $manifestPath = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
            if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
                try {
                    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
                    $candidates += @($manifest.InstallationList | Where-Object { $_.AppName -eq "UE_$Association" } | ForEach-Object { $_.InstallLocation })
                }
                catch { Write-Warning "Could not read Launcher installations: $($_.Exception.Message)" }
            }
        }
        if ($env:ProgramFiles) { $candidates += Join-Path $env:ProgramFiles "Epic Games\UE_$Association" }
    }
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        $buildFile = Join-Path $candidate 'Engine\Build\BatchFiles\Build.bat'
        $versionFile = Join-Path $candidate 'Engine\Build\Build.version'
        if (-not (Test-Path -LiteralPath $buildFile -PathType Leaf) -or -not (Test-Path -LiteralPath $versionFile -PathType Leaf)) { continue }
        $version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
        $majorMinor = '{0}.{1}' -f $version.MajorVersion, $version.MinorVersion
        if ($Association -match '^\d+\.\d+$' -and $majorMinor -ne $Association) {
            if (-not $OverrideRoot) { Write-Warning "Skipping engine $candidate : expected $Association, found $majorMinor."; continue }
            Write-Warning "Explicit engine $majorMinor differs from EngineAssociation $Association. UnrealBuildTool will check compatibility."
        }
        return [PSCustomObject]@{ Root = (Resolve-Path -LiteralPath $candidate).Path; Version = $version }
    }
    throw "No usable engine for '$Association'. Specify -EngineRoot with this computer's engine directory."
}

function Find-EditorTarget([string]$Root, [string]$ProjectName, [string]$Requested) {
    $source = Join-Path $Root 'Source'
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "C++ Source directory is missing: $source" }
    $targets = @(Get-ChildItem -LiteralPath $source -Recurse -File -Filter '*.Target.cs' | Where-Object {
        $_.Name -like '*Editor.Target.cs' -or (Select-String -LiteralPath $_.FullName -Pattern 'Type\s*=\s*TargetType\.Editor' -Quiet)
    })
    if ($Requested) {
        if (@($targets | Where-Object { $_.Name -eq ($Requested + '.Target.cs') }).Count -eq 1) { return $Requested }
        throw "Editor Target '$Requested' was not found under Source."
    }
    $conventional = @($targets | Where-Object { $_.Name -eq ($ProjectName + 'Editor.Target.cs') })
    if ($conventional.Count -eq 1) { return $ProjectName + 'Editor' }
    if ($targets.Count -eq 1) { return $targets[0].Name -replace '\.Target\.cs$', '' }
    throw 'Editor Target is missing or ambiguous. Specify -EditorTarget with a target under Source.'
}

function Invoke-UnrealBuild([string]$Entry, [string[]]$Arguments) {
    # PowerShell 5.1 must not turn native stderr warnings into a terminating error.
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $global:LASTEXITCODE = $null
        & $Entry @Arguments 2>&1 | Out-Host
        if ($null -eq $LASTEXITCODE) { throw 'The Unreal build tool did not return an exit code.' }
        return [int]$LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference }
}

$exitCode = 1
$transcriptStarted = $false
$runDirectory = $null
$result = $null
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
    $engine = Find-EngineRoot ([string]$project.EngineAssociation).Trim() $EngineRoot
    $EngineRoot = $engine.Root
    $EditorTarget = Find-EditorTarget $projectRoot $projectName $EditorTarget
    $buildScript = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
    $buildEntry = $buildScript
    if ($Rebuild) { $buildEntry = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Rebuild.bat' }
    $editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    foreach ($required in @($buildEntry, $editorPath)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "必要なファイルが見つかりません。`n$required" }
    }
    $version = $engine.Version
    Write-Host "Project:  $projectPath"
    Write-Host "Solution: $solutionPath"
    Write-Host "Engine:   $($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion) CL$($version.Changelist) ($EngineRoot)"
    Write-Host "Target:   $EditorTarget $Platform Development"
    Write-Host 'Use the same actual engine patch version as teammates for multiplayer.'
    if ($MSBuildPath) { Write-Warning '-MSBuildPath is deprecated and ignored. UnrealBuildTool selects the local C++ toolchain and SDK.' }
    $needsGeneration = -not (Test-Path -LiteralPath $solutionPath -PathType Leaf)
    Write-Host "Generate solution: $needsGeneration"
    $blocking = @(Get-Process | Where-Object {
        $_.ProcessName -like 'UnrealEditor*' -or $_.ProcessName -like 'LiveCodingConsole*' -or $_.ProcessName -like "$projectName*"
    })
    if ($blocking.Count -gt 0) {
        $blocking | Select-Object ProcessName, Id | Format-Table -AutoSize | Out-Host
        throw 'Save your work and close Unreal Editor, project game windows and Live Coding before building. No process was stopped.'
    }
    if ($CheckOnly) {
        Write-Host 'Preflight passed. No files generated, build logs created, build executed, or Editor launched.'
        $exitCode = 0
    }
    else {
        $runName = 'BuildAndLaunch_{0}_{1}' -f (Get-Date -Format 'yyyyMMdd_HHmmss_fff'), ([Guid]::NewGuid().ToString('N').Substring(0, 6))
        $runDirectory = Join-Path $projectRoot ("Saved\BuildTool\$runName")
        New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
        Start-Transcript -LiteralPath (Join-Path $runDirectory 'Console.log') | Out-Null
        $transcriptStarted = $true
        Write-Host "Project: $projectPath"
        Write-Host "Engine: $EngineRoot ($($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion) CL$($version.Changelist))"
        Write-Host "Target: $EditorTarget $Platform Development"
        Write-Host "Logs: $runDirectory"
        $result = [ordered]@{
            Project = $projectPath; Engine = $EngineRoot; EngineVersion = "$($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)"
            Target = $EditorTarget; Platform = $Platform; EngineChangelist = $version.Changelist; BuildEntry = (Split-Path -Leaf $buildEntry)
            Status = 'Starting'; BuildExitCode = $null; ExitCode = 1; EditorProcessId = $null; Error = $null
        }
        Push-Location -LiteralPath $projectRoot
        try {
            if ($needsGeneration) {
                $result.Status = 'GeneratingSolution'
                Write-Host 'Generating Visual Studio solution...'
                $exitCode = Invoke-UnrealBuild $buildScript @('-ProjectFiles', "-Project=$projectPath", '-Game', '-Engine', '-VisualStudio', "-Log=$(Join-Path $runDirectory 'ProjectFiles.log')")
                if ($exitCode -ne 0) { throw "Solution generation failed (exit code $exitCode). Editor was not launched." }
                if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) { throw "Solution generation returned success but the solution is missing: $solutionPath" }
            }
            $result.Status = 'Building'
            Write-Host 'Building the project Editor Target...'
            $exitCode = Invoke-UnrealBuild $buildEntry @($EditorTarget, $Platform, 'Development', "-Project=$projectPath", '-WaitMutex', '-NoHotReload', '-NoLiveCoding', "-Log=$(Join-Path $runDirectory 'UnrealBuildTool.log')")
            $result.BuildExitCode = $exitCode
            if ($exitCode -ne 0) { throw "Build failed (exit code $exitCode). Editor was not launched." }
        }
        finally { Pop-Location }
        $result.Status = 'BuildSucceeded'
        Write-Host 'BUILD SUCCEEDED (exit code 0).'
        if (-not $NoLaunch) {
            $result.Status = 'LaunchingEditor'
            # This is the requested interactive Editor, so its window remains visible.
            $editorProcess = Start-Process -FilePath $editorPath -ArgumentList ('"{0}" -log="{1}"' -f $projectPath, (Join-Path $runDirectory 'Editor.log')) -WorkingDirectory $projectRoot -PassThru
            $result.EditorProcessId = $editorProcess.Id
            $exited = $editorProcess.WaitForExit($LaunchWaitSeconds * 1000)
            if ($exited) { throw "Editor exited during startup. See $(Join-Path $runDirectory 'Editor.log')." }
            $result.Status = 'EditorStarted'
            Write-Host "Editor launched (PID $($editorProcess.Id))."
        }
        $exitCode = 0
    }
}
catch {
    if ($exitCode -eq 0) { $exitCode = 1 }
    if ($result) { $result.Error = $_.Exception.Message; $result.Status = 'Failed' }
    $message = "処理を中止しました。`n`n$($_.Exception.Message)"
    Write-Host "エラー: $message" -ForegroundColor Red
    if (-not $NoPopup) {
        try {
            Add-Type -AssemblyName System.Windows.Forms
            [void][System.Windows.Forms.MessageBox]::Show($message, 'ビルド・起動エラー', 'OK', 'Error')
        }
        catch { Write-Host 'エラーダイアログを表示できませんでした。上のエラーメッセージを確認してください。' }
    }
}
finally {
    if ($result) {
        $result.ExitCode = $exitCode
        try { $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runDirectory 'Result.json') -Encoding UTF8 }
        catch { Write-Warning "Could not save Result.json: $($_.Exception.Message)" }
    }
    if ($transcriptStarted) { Stop-Transcript | Out-Null }
    if ($runDirectory) { Write-Host "Logs: $runDirectory" }
    Write-Host "Exit code: $exitCode"
    if ($Pause -and -not $CheckOnly) { [void](Read-Host 'Press Enter to close') }
}
exit $exitCode
