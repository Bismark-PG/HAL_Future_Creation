[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$EngineRoot,
    [ValidateRange(1024, 65535)][int]$Port = 7941,
    [switch]$SkipNetwork,
    [string]$ReportDirectory
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'HAL_Future_Creation.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Editor not found: $editor" }
if (-not $ReportDirectory) { $ReportDirectory = Join-Path $projectRoot ('Saved\HealingArea\Tests_' + (Get-Date -Format 'yyyyMMdd_HHmmss')) }
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
if (Test-Path -LiteralPath $ReportDirectory) { throw 'Use a fresh report directory; prior evidence is retained.' }
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
$common = @('-nullrhi', '-unattended', '-nop4', '-nosound', '-nosplash', '-NoGamepad', '-DisablePlugins=XInputDevice',
    '-UDPMESSAGING_TRANSPORT_ENABLE=0', '-TestExit="Automation Test Queue Empty"')
$owned = @()
$summary = @()

function Start-Probe([string]$Role, [string]$Url, [string]$Filter, [bool]$Game) {
    $arguments = @("`"$project`"")
    if ($Url) { $arguments += $Url }
    $arguments += $common
    if ($Game) { $arguments += @('-game', "-port=$Port") }
    $arguments += @("-ExecCmds=`"t.MaxFPS 60,Automation RunTests $Filter`"", "-abslog=`"$ReportDirectory\$Role.log`"", "-ReportExportPath=`"$ReportDirectory\$Role`"")
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportDirectory\${Role}_Console.log" -RedirectStandardError "$ReportDirectory\${Role}_Stderr.log"
    $null = $process.Handle
    Write-Output $process
}
function Wait-Probe([Diagnostics.Process]$Process, [int]$Seconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while (-not $Process.HasExited -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 500 }
    if (-not $Process.HasExited) { throw "Test process timed out: $($Process.Id)" }
    $Process.WaitForExit()
}
function Read-Probe([string]$Role, [Diagnostics.Process]$Process, [int]$Minimum) {
    $path = Join-Path $ReportDirectory "$Role\index.json"
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing report: $path" }
    $report = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    $clean = $Process.ExitCode -eq 0 -and $report.succeeded -ge $Minimum -and $report.succeededWithWarnings -eq 0 -and $report.failed -eq 0 -and $report.notRun -eq 0 -and $report.inProcess -eq 0
    if ($Role -eq 'Native') {
        # Missing new tests must fail even when an old DLL still contains the pre-existing area suites.
        $healing = @($report.tests | Where-Object { $_.fullTestPath -like 'HAL.FutureCreation.HealingAreas.*' })
        $clean = $clean -and $healing.Count -ge 6
    }
    [pscustomobject]@{ Role=$Role; ExitCode=$Process.ExitCode; Passed=$report.succeeded; Warnings=$report.succeededWithWarnings; Failed=$report.failed; Clean=$clean }
}
try {
    Write-Output "Reports: $ReportDirectory. Build the current sources with BuildAndLaunch.cmd -NoLaunch first."
    $native = Start-Probe 'Native' '' 'HAL.FutureCreation.HealingAreas+HAL.FutureCreation.MovementAreas+HAL.FutureCreation.ArenaGenerator' $false
    $owned += $native
    Wait-Probe $native 240
    $result = Read-Probe 'Native' $native 18
    $summary += $result
    if (-not $result.Clean) { throw 'Native healing/area regression failed; network probes were not started.' }
    if (-not $SkipNetwork) {
        $hostProcess = Start-Probe 'Host' '/Game/Maps/MVPB_NetTest?listen' 'HAL.NetworkSmoke.HealingAreas' $true
        $owned += $hostProcess
        $deadline = [DateTime]::UtcNow.AddSeconds(120)
        $listening = $false
        while (-not $hostProcess.HasExited -and [DateTime]::UtcNow -lt $deadline) {
            if ((Test-Path -LiteralPath "$ReportDirectory\Host.log") -and (Select-String -LiteralPath "$ReportDirectory\Host.log" -SimpleMatch "listening on port $Port" -Quiet)) { $listening = $true; break }
            Start-Sleep -Milliseconds 500
        }
        if (-not $listening) { throw 'Host did not start listening; inspect Host.log.' }
        $clientProcess = Start-Probe 'Client' "127.0.0.1:$Port" 'HAL.NetworkSmoke.HealingAreas' $true
        $owned += $clientProcess
        Wait-Probe $hostProcess 180
        Wait-Probe $clientProcess 30
        $summary += Read-Probe 'Host' $hostProcess 1
        $summary += Read-Probe 'Client' $clientProcess 1
    }
    if (@($summary | Where-Object { -not $_.Clean }).Count) { throw 'Healing area verification failed; inspect reports.' }
} finally {
    foreach ($process in $owned) { if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force } }
    $summary | ConvertTo-Json | Set-Content -LiteralPath "$ReportDirectory\Summary.json" -Encoding utf8
    $summary | Format-Table -AutoSize
}
