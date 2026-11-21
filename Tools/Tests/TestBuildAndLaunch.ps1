# Windows PowerShell 5.1. Isolated launcher checks; no real Unreal build or Editor launch.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testRoot = Join-Path $projectRoot ('Saved\BuildTool\LauncherIntegration_' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$utf8 = New-Object Text.UTF8Encoding($true)
function Write-FixtureFile([string]$Path, [string]$Content) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    [IO.File]::WriteAllText($Path, ($Content -replace '\r?\n', "`r`n"), $utf8)
}
$engine = Join-Path $testRoot 'Engine Space'
$batchDirectory = Join-Path $engine 'Engine\Build\BatchFiles'
$fakeBuild = @'
@echo off
echo %~nx0 %*>>"%HAL_LAUNCHER_TEST_TRACE%"
if "%~1"=="-ProjectFiles" goto generate
:scanlog
if "%~1"=="" goto build
set "FixtureArgument=%~1"
if defined HAL_LAUNCHER_TEST_LOG_SOURCE if /i "%FixtureArgument:~0,5%"=="-Log=" copy /y "%HAL_LAUNCHER_TEST_LOG_SOURCE%" "%FixtureArgument:~5%" >nul
shift
goto scanlog
:build
echo fixture native stderr 1>&2
exit /b %HAL_LAUNCHER_TEST_BUILD_EXIT%
:generate
if not "%HAL_LAUNCHER_TEST_GENERATE_EXIT%"=="0" exit /b %HAL_LAUNCHER_TEST_GENERATE_EXIT%
type nul > "%HAL_LAUNCHER_TEST_PROJECT%\Fixture.sln"
exit /b 0
'@
New-Item -ItemType Directory -Path $batchDirectory -Force | Out-Null
foreach ($entry in @('Build.bat', 'Rebuild.bat')) {
    [IO.File]::WriteAllText((Join-Path $batchDirectory $entry), ($fakeBuild -replace '\r?\n', "`r`n"), [Text.Encoding]::ASCII)
}
Write-FixtureFile (Join-Path $engine 'Engine\Build\Build.version') '{"MajorVersion":5,"MinorVersion":8,"PatchVersion":99,"Changelist":123}'
Write-FixtureFile (Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor.exe') 'Fixture placeholder; never executed.'
$launcherText = [IO.File]::ReadAllText((Join-Path $projectRoot 'BuildAndLaunch.cmd'))
$payload = ($launcherText -split '(?m)^# POWERSHELL_PAYLOAD\r?$', 2)[1]
$tokens = $null; $parseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseInput($payload, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
$payloadFile = Join-Path $testRoot 'Payload.ps1'
Write-FixtureFile $payloadFile $payload
$driver = Join-Path $testRoot 'MockProcessDriver.ps1'
Write-FixtureFile $driver @'
param([string]$Payload, [string]$ProjectDirectory, [string]$EngineRoot, [switch]$CheckOnly, [switch]$NoLaunch)
function Get-Process {
    if ($env:HAL_LAUNCHER_TEST_BLOCK) { [pscustomobject]@{ ProcessName = $env:HAL_LAUNCHER_TEST_BLOCK; Id = 99999 } }
}
function Start-Process {
    param($FilePath, $ArgumentList, $WorkingDirectory, [switch]$PassThru)
    Add-Content -LiteralPath $env:HAL_LAUNCHER_TEST_TRACE -Value 'LAUNCH'
    $handle = [pscustomobject]@{ Id = 4242; Exited = ($env:HAL_LAUNCHER_TEST_EARLY_EXIT -eq '1') }
    Add-Member -InputObject $handle -MemberType ScriptMethod -Name WaitForExit -Value { param([int]$Milliseconds) return $this.Exited }
    return $handle
}
& $Payload -ProjectDirectory $ProjectDirectory -EngineRoot $EngineRoot -CheckOnly:$CheckOnly -NoLaunch:$NoLaunch -NoPopup
exit $LASTEXITCODE
'@
$results = @()
$incrementalLog = @'
Using Unreal Build Accelerator local executor to run 3 action(s)
[1/3] Compile [x64] Fixture.cpp
[2/3] Link [x64] Fixture.dll
[3/3] WriteMetadata Fixture.target
Total execution time: 12.50 seconds
'@
$upToDateLog = @'
Target is up to date
Using Unreal Build Accelerator local executor to run 0 action(s)
Total execution time: 1.68 seconds
'@
function Run-Case([string]$Name, [int]$ExpectedExit, [string[]]$Options = @(), [string]$Block = '', [switch]$MockLaunch, [switch]$ExistingSolution, [switch]$MissingTarget, [int]$BuildExit = 0, [int]$GenerateExit = 0, [switch]$EarlyExit,
    [AllowNull()][string]$BuildLog = $incrementalLog, [string]$ExpectedBuildKind = 'Incremental', [Nullable[int]]$ExpectedActions = 3, [Nullable[double]]$ExpectedUbtSeconds = 12.5) {
    $fixture = Join-Path $testRoot ($Name + ' ' + [char]0x4e2d + [char]0x6587 + ' & space')
    New-Item -ItemType Directory -Path $fixture -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot 'BuildAndLaunch.cmd') -Destination (Join-Path $fixture 'BuildAndLaunch.cmd')
    Write-FixtureFile (Join-Path $fixture 'Fixture.uproject') '{"EngineAssociation":"5.8"}'
    if (-not $MissingTarget) { Write-FixtureFile (Join-Path $fixture 'Source\CustomEditor.Target.cs') '// Type = TargetType.Editor' }
    if ($ExistingSolution) { Write-FixtureFile (Join-Path $fixture 'Fixture.sln') 'fixture solution' }
    $env:HAL_LAUNCHER_TEST_LOG_SOURCE = ''
    if ($BuildLog) {
        $env:HAL_LAUNCHER_TEST_LOG_SOURCE = Join-Path $fixture 'NativeLog.txt'
        Write-FixtureFile $env:HAL_LAUNCHER_TEST_LOG_SOURCE $BuildLog
    }
    $env:HAL_LAUNCHER_TEST_TRACE = Join-Path $fixture 'Trace.txt'
    $env:HAL_LAUNCHER_TEST_PROJECT = $fixture
    $env:HAL_LAUNCHER_TEST_BUILD_EXIT = [string]$BuildExit
    $env:HAL_LAUNCHER_TEST_GENERATE_EXIT = [string]$GenerateExit
    $env:HAL_LAUNCHER_TEST_BLOCK = $Block
    $env:HAL_LAUNCHER_TEST_EARLY_EXIT = [string][int]$EarlyExit.IsPresent
    Push-Location -LiteralPath $testRoot
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        if ($Block -or $MockLaunch) {
            $driverOptions = @()
            if ($Options -contains '-CheckOnly') { $driverOptions += '-CheckOnly' }
            if ($Options -contains '-NoLaunch') { $driverOptions += '-NoLaunch' }
            $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $driver -Payload $payloadFile -ProjectDirectory $fixture -EngineRoot $engine @driverOptions 2>&1 | Out-String
        }
        else {
            $output = & (Join-Path $fixture 'BuildAndLaunch.cmd') -EngineRoot $engine -NoPopup @Options 2>&1 | Out-String
        }
        $actualExit = $LASTEXITCODE
    }
    finally { $ErrorActionPreference = $previousPreference; Pop-Location }
    Write-FixtureFile (Join-Path $fixture 'TestConsole.txt') $output
    $trace = if (Test-Path -LiteralPath $env:HAL_LAUNCHER_TEST_TRACE) { Get-Content -LiteralPath $env:HAL_LAUNCHER_TEST_TRACE -Raw } else { '' }
    if ($actualExit -ne $ExpectedExit) { throw "$Name : expected $ExpectedExit, got $actualExit`n$output" }
    $isPreflight = $Options -contains '-CheckOnly' -or $Block -or $MissingTarget
    if ($isPreflight -and (Test-Path -LiteralPath (Join-Path $fixture 'Saved'))) { throw "$Name : preflight wrote build logs" }
    if ($isPreflight -and $trace) { throw "$Name : preflight invoked a build or launch" }
    if (-not $isPreflight) {
        $reports = @(Get-ChildItem -LiteralPath (Join-Path $fixture 'Saved') -Recurse -Filter Result.json)
        if ($reports.Count -ne 1) { throw "$Name : expected one result report" }
        $report = Get-Content -LiteralPath $reports[0].FullName -Raw | ConvertFrom-Json
        if ($report.ExitCode -ne $ExpectedExit) { throw "$Name : report exit code differs" }
        if (-not $MockLaunch -and $report.EditorProcessId) { throw "$Name : unexpectedly launched Editor" }
        if ($GenerateExit -ne 0 -and $trace -match 'CustomEditor') { throw "$Name : generation failure continued into build" }
        if ($GenerateExit -eq 0 -and $trace -notmatch 'CustomEditor Win64 Development') { throw "$Name : did not build the selected Editor Target" }
        if ($GenerateExit -eq 0 -and ($trace -notmatch '-NoHotReload(?:\s|$)' -or $trace -notmatch '-NoLiveCoding(?:\s|$)')) { throw "$Name : full Editor build must disable Hot Reload and Live Coding" }
        if ($ExistingSolution -and $trace -match '-ProjectFiles') { throw "$Name : regenerated an existing solution" }
        if (-not $ExistingSolution -and $trace -notmatch '-ProjectFiles') { throw "$Name : missing solution was not generated" }
        if (($Options -contains '-Rebuild') -and $trace -notmatch 'Rebuild.bat') { throw "$Name : did not use official Rebuild entry" }
        if ($MockLaunch -and $trace -notmatch 'LAUNCH') { throw "$Name : launch path was not reached" }
        $expectedKind = if ($GenerateExit -ne 0) { 'NotRun' } elseif ($BuildExit -ne 0) { 'Failed' } elseif ($Options -contains '-Rebuild') { 'Rebuild' } else { $ExpectedBuildKind }
        if ($report.BuildKind -ne $expectedKind) { throw "$Name : expected summary $expectedKind, got $($report.BuildKind)" }
        if ($GenerateExit -ne 0) {
            if ($null -ne $report.BuildSeconds -or $null -ne $report.UbtSeconds -or $null -ne $report.ActionCount) { throw "$Name : generation failure reported build statistics" }
        }
        else {
            if ($null -eq $report.BuildSeconds -or $report.BuildSeconds -lt 0) { throw "$Name : missing build invocation duration" }
            if ($report.ActionCount -ne $ExpectedActions -or $report.UbtSeconds -ne $ExpectedUbtSeconds) { throw "$Name : incorrect action count or UBT duration" }
            $expectedLabels = @{ UpToDate = 'Up to date (no compilation needed)'; Incremental = 'Incremental build completed'; Rebuild = 'Explicit rebuild completed'; Failed = 'Build failed'; Unknown = 'action statistics unknown' }
            if ($output -notmatch [regex]::Escape($expectedLabels[$expectedKind])) { throw "$Name : missing console summary" }
            if ($BuildExit -ne 0 -and $output -match 'BUILD SUCCEEDED') { throw "$Name : failed build printed success" }
        }
    }
    $script:results += [pscustomobject]@{ Name = $Name; ExitCode = $actualExit; Passed = $true }
}
Run-Case 'PortablePreflight' 0 @('-CheckOnly')
Run-Case 'GenerateAndBuildOnly' 0 @('-NoLaunch')
Run-Case 'ExistingSolution' 0 @('-NoLaunch') -ExistingSolution
Run-Case 'PreserveBuildFailure' 6 -BuildExit 6
Run-Case 'PreserveGenerationFailure' 7 -GenerateExit 7
Run-Case 'MissingTarget' 1 @('-CheckOnly') -MissingTarget
Run-Case 'EditorGuard' 1 @('-CheckOnly') -Block 'UnrealEditor'
Run-Case 'LiveCodingGuard' 1 @('-CheckOnly') -Block 'LiveCodingConsole'
Run-Case 'ProjectGameGuard' 1 @('-CheckOnly') -Block 'Fixture-Win64-Shipping'
Run-Case 'OfficialRebuild' 0 @('-Rebuild', '-NoLaunch')
Run-Case 'DeprecatedMSBuildPath' 0 @('-CheckOnly', '-MSBuildPath', 'Z:\Missing\MSBuild.exe')
Run-Case 'LaunchSuccessMock' 0 -MockLaunch
Run-Case 'LaunchEarlyExitMock' 1 -MockLaunch -EarlyExit
Run-Case 'UpToDateSummary' 0 @('-NoLaunch') -BuildLog $upToDateLog -ExpectedBuildKind 'UpToDate' -ExpectedActions 0 -ExpectedUbtSeconds 1.68
Run-Case 'ZeroActionsSummary' 0 @('-NoLaunch') -BuildLog 'Using Parallel executor to run 0 action(s)' -ExpectedBuildKind 'UpToDate' -ExpectedActions 0 -ExpectedUbtSeconds $null
Run-Case 'ProgressSummary' 0 @('-NoLaunch') -BuildLog (@('[1/4] Compile [x64] Fixture.cpp', '[2/4] Link [x64] Fixture.dll', 'Total execution time: 0,75 seconds') -join [Environment]::NewLine) -ExpectedActions 4 -ExpectedUbtSeconds 0.75
Run-Case 'MissingStatistics' 0 @('-NoLaunch') -BuildLog $null -ExpectedBuildKind 'Unknown' -ExpectedActions $null -ExpectedUbtSeconds $null
Run-Case 'UnknownLogFormat' 0 @('-NoLaunch') -BuildLog 'Build completed without recognized statistics.' -ExpectedBuildKind 'Unknown' -ExpectedActions $null -ExpectedUbtSeconds $null
Run-Case 'FailedUpToDateLog' 6 -BuildExit 6 -BuildLog $upToDateLog -ExpectedActions 0 -ExpectedUbtSeconds 1.68
Run-Case 'LaunchEarlyExitAfterUpToDateMock' 1 -MockLaunch -EarlyExit -BuildLog $upToDateLog -ExpectedBuildKind 'UpToDate' -ExpectedActions 0 -ExpectedUbtSeconds 1.68
Run-Case 'RebuildWithoutStatistics' 0 @('-Rebuild', '-NoLaunch') -BuildLog $null -ExpectedActions $null -ExpectedUbtSeconds $null
$results | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $testRoot 'Results.json') -Encoding UTF8
$results | Format-Table -AutoSize
Write-Host "Fixture reports: $testRoot"
Write-Host 'Fake build/process checks only; no Unreal compilation or real Editor launch was performed.'
