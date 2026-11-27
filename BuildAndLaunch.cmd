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
    [switch]$ShowAllWarnings,
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
    $suppressed = New-Object 'System.Collections.Generic.List[string]'
    $suppressedWarningCount = 0
    $suppressContext = $false
    try {
        $ErrorActionPreference = 'Continue'
        $global:LASTEXITCODE = $null
        & $Entry @Arguments 2>&1 | ForEach-Object {
            # Render native stderr as text, not PowerShell NativeCommandError records.
            foreach ($line in ([string]$_ -split '\r?\n')) {
                if (-not $ShowAllWarnings -and $line -match '(?i)^\s*(?:.+\(\d+(?:,\d+)?\)\s*:\s*)?warning\s+C4305\s*:') {
                    $suppressed.Add($line)
                    $suppressedWarningCount++
                    $suppressContext = $true
                    continue
                }
                if ($suppressContext -and $line -match '^\s' -and $line -notmatch '(?i)\b(?:error|warning)\b(?:\s|:)') {
                    $suppressed.Add($line)
                    continue
                }
                $suppressContext = $false
                Write-Host $line
            }
        }
        if ($null -eq $LASTEXITCODE) { throw 'The Unreal build tool did not return an exit code.' }
        $nativeExitCode = [int]$LASTEXITCODE
        if ($suppressedWarningCount -gt 0) {
            if ($nativeExitCode -ne 0) {
                Write-Host "Build failed; showing $suppressedWarningCount suppressed C4305 warning(s):"
                foreach ($line in $suppressed) { Write-Host $line }
            }
            else {
                Write-Host "Suppressed $suppressedWarningCount C4305 warning(s). Full diagnostics remain in the UBT log; use -ShowAllWarnings to display them."
            }
        }
        return $nativeExitCode
    }
    finally { $ErrorActionPreference = $previousPreference }
}

function Get-BuildSummary([string]$LogPath, [int]$BuildExitCode, [bool]$WasRebuild) {
    $summary = [ordered]@{ Kind = 'Unknown'; ActionCount = $null; UbtSeconds = $null }
    $upToDate = $false
    try {
        if (Test-Path -LiteralPath $LogPath -PathType Leaf) {
            $log = [IO.File]::ReadAllText($LogPath)
            $upToDate = [regex]::IsMatch($log, '(?im)^\s*Target is up to date\s*$')
            $actions = [regex]::Matches($log, '(?im)^\s*(?:Using .+ executor to run|Building)\s+(\d+)\s+action')
            if ($actions.Count -gt 0) {
                $summary.ActionCount = [int]$actions[$actions.Count - 1].Groups[1].Value
            }
            else {
                # Other executors may only report progress as [completed/total].
                foreach ($progress in [regex]::Matches($log, '(?m)^\s*\[\d+/(\d+)\]\s+')) {
                    $total = [int]$progress.Groups[1].Value
                    if ($null -eq $summary.ActionCount -or $total -gt $summary.ActionCount) { $summary.ActionCount = $total }
                }
                if ($null -eq $summary.ActionCount -and $upToDate) { $summary.ActionCount = 0 }
            }
            $times = [regex]::Matches($log, '(?im)^\s*Total execution time:\s*(\d+(?:[.,]\d+)?)\s+seconds')
            if ($times.Count -gt 0) {
                $summary.UbtSeconds = [double]::Parse($times[$times.Count - 1].Groups[1].Value.Replace(',', '.'), [Globalization.CultureInfo]::InvariantCulture)
            }
        }
    }
    catch { Write-Warning "Build statistics are unavailable: $($_.Exception.Message)" }
    # Statistics never override the native build exit code or an explicit Rebuild.
    if ($BuildExitCode -ne 0) { $summary.Kind = 'Failed' }
    elseif ($WasRebuild) { $summary.Kind = 'Rebuild' }
    elseif ($null -ne $summary.ActionCount) {
        if ($summary.ActionCount -eq 0) { $summary.Kind = 'UpToDate' }
        else { $summary.Kind = 'Incremental' }
    }
    return [PSCustomObject]$summary
}

$exitCode = 1
$transcriptStarted = $false
$runDirectory = $null
$result = $null
try {
    $projectRoot = (Resolve-Path -LiteralPath $ProjectDirectory).Path
    $projects = @(Get-ChildItem -LiteralPath $projectRoot -Filter '*.uproject' -File)
    if ($projects.Count -eq 0) {
        throw "No .uproject file was found in this folder. Place BuildAndLaunch.cmd in the project root folder.`n$projectRoot"
    }
    if ($projects.Count -gt 1) {
        throw "Multiple .uproject files were found. Keep exactly one .uproject file in this folder so the target project can be identified.`n$projectRoot"
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
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "A required file was not found.`n$required" }
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
            BuildKind = 'NotRun'; ActionCount = $null; BuildSeconds = $null; UbtSeconds = $null
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
            $buildLogPath = Join-Path $runDirectory 'UnrealBuildTool.log'
            $buildTimer = [Diagnostics.Stopwatch]::StartNew()
            try {
                $exitCode = Invoke-UnrealBuild $buildEntry @($EditorTarget, $Platform, 'Development', "-Project=$projectPath", '-WaitMutex', '-NoHotReload', '-NoLiveCoding', "-Log=$buildLogPath")
            }
            finally {
                $buildTimer.Stop()
                $result.BuildSeconds = [Math]::Round($buildTimer.Elapsed.TotalSeconds, 2)
            }
            $result.BuildExitCode = $exitCode
            $summary = Get-BuildSummary $buildLogPath $exitCode $Rebuild.IsPresent
            $result.BuildKind = $summary.Kind
            $result.ActionCount = $summary.ActionCount
            $result.UbtSeconds = $summary.UbtSeconds
            $kindLabels = @{
                UpToDate = 'Up to date (no compilation needed)'; Incremental = 'Incremental build completed'; Rebuild = 'Explicit rebuild completed'
                Failed = 'Build failed'; Unknown = 'Build succeeded (action statistics unknown)'
            }
            Write-Host ("Build result: {0}" -f $kindLabels[$result.BuildKind])
            Write-Host ("Build invocation time: {0:F2} seconds (including tool startup and waiting)" -f $result.BuildSeconds)
            $ubtTime = if ($null -eq $result.UbtSeconds) { 'Unknown' } else { '{0:F2} seconds' -f $result.UbtSeconds }
            $actionCount = if ($null -eq $result.ActionCount) { 'Unknown' } else { [string]$result.ActionCount }
            Write-Host "UBT time: $ubtTime"
            Write-Host "Build actions: $actionCount"
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
    if ($result) {
        if ($result.Status -eq 'Building' -and $null -eq $result.BuildExitCode) { $result.BuildKind = 'Failed' }
        $result.Error = $_.Exception.Message; $result.Status = 'Failed'
    }
    $message = "The operation was stopped.`n`n$($_.Exception.Message)"
    Write-Host "Error: $message" -ForegroundColor Red
    if (-not $NoPopup) {
        try {
            Add-Type -AssemblyName System.Windows.Forms
            [void][System.Windows.Forms.MessageBox]::Show($message, 'Build / Launch Error', 'OK', 'Error')
        }
        catch { Write-Host 'The error dialog could not be displayed. Check the error message above.' }
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
