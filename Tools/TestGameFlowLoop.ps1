# Deterministic editor-runtime smoke test. Not a packaged or four-physical-PC acceptance test.
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [ValidateSet('Development', 'Exhibition')][string]$Profile = 'Exhibition',
    [ValidateRange(1024, 65535)][int]$Port = 7909,
    [string]$ReportDirectory
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'HAL_Future_Creation.uproject'
if (-not $EngineRoot) {
    $association = (Get-Content -LiteralPath $projectFile -Raw | ConvertFrom-Json).EngineAssociation
    $installed = Get-ItemProperty -LiteralPath "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$association" -ErrorAction SilentlyContinue
    if ($installed) { $EngineRoot = $installed.InstalledDirectory }
    if (-not $EngineRoot) { throw 'Pass -EngineRoot with the UE installation directory.' }
}
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Editor missing: $editor" }
if (-not $ReportDirectory) { $ReportDirectory = Join-Path $projectRoot ('Saved\GameFlow\Loop_' + (Get-Date -Format 'yyyyMMdd_HHmmss')) }
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
if (Test-Path -LiteralPath $ReportDirectory) { throw 'Use a new report directory to retain earlier evidence.' }
New-Item -ItemType Directory -Path $ReportDirectory | Out-Null
Write-Output "Reports: $ReportDirectory"
if ($Profile -eq 'Exhibition') {
    $players = 4
    $test = 'HAL.NetworkSmoke.GameFlow.GF6.ExhibitionFourProcessesTwoRounds'
    # Native server mode allows selecting the saved Exhibition DA without editing the team's BP.
    $map = '/Game/Maps/MVPB_PracticeRoom?listen?game=/Script/HAL_Future_Creation.HALPracticeRoomGameMode?RoomConfig=/Game/Data/Gameflow/DA_Room_Exhibition.DA_Room_Exhibition'
} else {
    $players = 2
    $test = 'HAL.NetworkSmoke.GameFlow.GF4.TwoRounds'
    $map = '/Game/Maps/MVPB_PracticeRoom?listen'
}
$common = @('-game', '-nullrhi', '-unattended', '-NoGamepad', '-DisablePlugins=XInputDevice', '-nop4', '-nosplash', '-nosound',
    '-UDPMESSAGING_TRANSPORT_ENABLE=0', "-port=$Port", '-stdout', '-FullStdOutLogOutput', '-TestExit="Automation Test Queue Empty"')
$owned = @()
$roles = @('Host') + @(1..($players - 1) | ForEach-Object { "Client$_" })
try {
    foreach ($role in $roles) {
        $hostRole = $role -eq 'Host'
        $url = if ($hostRole) { $map } else { "127.0.0.1:$Port" }
        $commands = if ($hostRole) { "t.MaxFPS 60,Automation RunTests $test" } else { "t.MaxFPS 60,hal.RoundReadyDelay 2,Automation RunTests $test" }
        $arguments = @("`"$projectFile`"", $url) + $common + @("-ExecCmds=`"$commands`"", "-abslog=`"$ReportDirectory\$role.log`"", "-ReportExportPath=`"$ReportDirectory\$role`"")
        if ($hostRole -and $Profile -eq 'Exhibition') { $arguments += '-HALAllowTestRoundEnd' }
        $process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportDirectory\${role}_Console.log" -RedirectStandardError "$ReportDirectory\${role}_Stderr.log"
        $owned += $process
        # Windows PowerShell 5.1 otherwise loses ExitCode after a redirected process exits.
        $null = $process.Handle
        Write-Output "$role PID=$($process.Id)"
        if ($hostRole) {
            $deadline = [DateTime]::UtcNow.AddSeconds(120)
            $listening = $false
            while (-not $process.HasExited -and [DateTime]::UtcNow -lt $deadline) {
                if ((Test-Path -LiteralPath "$ReportDirectory\Host.log") -and (Select-String -LiteralPath "$ReportDirectory\Host.log" -SimpleMatch "listening on port $Port" -Quiet)) { $listening = $true; break }
                Start-Sleep -Milliseconds 500
            }
            if (-not $listening) { throw 'Host did not open the expected port.' }
        }
    }
    $deadline = [DateTime]::UtcNow.AddSeconds(240)
    while ([DateTime]::UtcNow -lt $deadline -and @($owned | Where-Object { -not $_.HasExited }).Count) { Start-Sleep -Seconds 1 }
    if (@($owned | Where-Object { -not $_.HasExited }).Count) { throw 'Smoke test timed out; owned processes will be stopped.' }
    $results = for ($i = 0; $i -lt $roles.Count; ++$i) {
        $owned[$i].WaitForExit() # Ensure redirected output has drained before reading artifacts.
        $exitCode = $owned[$i].ExitCode
        $problems = @()
        if ($null -eq $exitCode) { $problems += 'Process exit code unavailable' }
        elseif ($exitCode -ne 0) { $problems += "Process exit code $exitCode" }
        $report = $null
        $reportPath = Join-Path $ReportDirectory "$($roles[$i])\index.json"
        if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { $problems += "Missing report: $reportPath" }
        else {
            try { $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json }
            catch { $problems += "Unreadable report: $($_.Exception.Message)" }
            if ($null -ne $report -and ($report.succeeded -ne 1 -or $report.succeededWithWarnings -ne 0 -or $report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0)) {
                $problems += "Automation counts: passed=$($report.succeeded), warnings=$($report.succeededWithWarnings), failed=$($report.failed), notRun=$($report.notRun), inProcess=$($report.inProcess)"
            }
            if ($null -eq $report -and -not $problems.Count) { $problems += 'Empty automation report' }
        }
        [pscustomobject]@{ Role = $roles[$i]; Pid = $owned[$i].Id; ExitCode = $exitCode; Passed = $report.succeeded; Warnings = $report.succeededWithWarnings; Failed = $report.failed; NotRun = $report.notRun; InProcess = $report.inProcess; Error = ($problems -join '; ') }
    }
    $results | ConvertTo-Json | Set-Content -LiteralPath "$ReportDirectory\Summary.json" -Encoding utf8
    $results | Format-Table Role, Pid, ExitCode, Passed, Warnings, Failed
    $badResults = @($results | Where-Object { $_.Error })
    if ($badResults.Count) { throw ("GameFlow check failed. Reports: $ReportDirectory. " + (($badResults | ForEach-Object { "$($_.Role): $($_.Error)" }) -join ' | ')) }
} finally {
    foreach ($process in $owned) { if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force } }
}
