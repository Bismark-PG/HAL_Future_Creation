[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$EngineRoot,
    [ValidateRange(1024, 65535)][int]$Port = 7933,
    [ValidateRange(0, 500)][int]$HostLagMilliseconds = 0,
    [switch]$NetTrace,
    [string]$ReportDirectory
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $projectRoot 'HAL_Future_Creation.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) { throw "Editor not found: $editor" }
if (-not $ReportDirectory) { $ReportDirectory = Join-Path $projectRoot ('Saved\HookThrow\Network_' + (Get-Date -Format 'yyyyMMdd_HHmmss')) }
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
if (Test-Path -LiteralPath $ReportDirectory) { throw 'Use a new report directory; earlier evidence is retained.' }
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
$common = @('-game', '-nullrhi', '-unattended', '-nop4', '-nosound', '-nosplash', '-NoGamepad', '-DisablePlugins=XInputDevice',
    '-HALHookFixture', '-UDPMESSAGING_TRANSPORT_ENABLE=0', "-port=$Port", '-TestExit="Automation Test Queue Empty"')
if ($NetTrace) { $common += '-HALHookNetTrace' }
$owned = @()
$roles = @('Host', 'Client')
Write-Output "Reports: $ReportDirectory"
try {
    foreach ($role in $roles) {
        $hostRole = $role -eq 'Host'
        $url = if ($hostRole) { '/Game/Maps/MVPB_NetTest?listen' } else { "127.0.0.1:$Port" }
        $commands = 't.MaxFPS 60,hal.HookLog 1,hal.HookPhysicsLog 1,hal.MovementEffectLog 1,'
        if ($hostRole -and $HostLagMilliseconds -gt 0) { $commands += "NetEmulation.PktLag $HostLagMilliseconds," }
        $commands += 'Automation RunTests HAL.NetworkSmoke.HookThrow.ClientDirectionAuthorityThrowAndInterrupt'
        $arguments = @("`"$project`"", $url) + $common + @("-ExecCmds=`"$commands`"", "-abslog=`"$ReportDirectory\$role.log`"", "-ReportExportPath=`"$ReportDirectory\$role`"")
        $process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput "$ReportDirectory\${role}_Console.log" -RedirectStandardError "$ReportDirectory\${role}_Stderr.log"
        $owned += $process
        $null = $process.Handle
        Write-Output "$role PID=$($process.Id)"
        if ($hostRole) {
            $deadline = [DateTime]::UtcNow.AddSeconds(120)
            $listening = $false
            while (-not $process.HasExited -and [DateTime]::UtcNow -lt $deadline) {
                if ((Test-Path -LiteralPath "$ReportDirectory\Host.log") -and (Select-String -LiteralPath "$ReportDirectory\Host.log" -SimpleMatch "listening on port $Port" -Quiet)) { $listening = $true; break }
                Start-Sleep -Milliseconds 500
            }
            if (-not $listening) { throw 'Host did not begin listening; inspect Host.log.' }
        }
    }
    $deadline = [DateTime]::UtcNow.AddSeconds(180)
    while ([DateTime]::UtcNow -lt $deadline -and @($owned | Where-Object { -not $_.HasExited }).Count) { Start-Sleep -Seconds 1 }
    if (@($owned | Where-Object { -not $_.HasExited }).Count) { throw 'Owned hook test processes timed out.' }
    $results = for ($i = 0; $i -lt $roles.Count; ++$i) {
        $owned[$i].WaitForExit()
        $reportPath = Join-Path $ReportDirectory "$($roles[$i])\index.json"
        $report = if (Test-Path -LiteralPath $reportPath) { Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json } else { $null }
        $passed = $null -ne $report -and $owned[$i].ExitCode -eq 0 -and $report.succeeded -eq 1 -and $report.succeededWithWarnings -eq 0 -and $report.failed -eq 0 -and $report.notRun -eq 0 -and $report.inProcess -eq 0
        [pscustomobject]@{Role=$roles[$i];Pid=$owned[$i].Id;ExitCode=$owned[$i].ExitCode;Passed=$report.succeeded;Warnings=$report.succeededWithWarnings;Failed=$report.failed;Clean=$passed;HostInjectedOutgoingLagMs=$HostLagMilliseconds}
    }
    $results | ConvertTo-Json | Set-Content -LiteralPath "$ReportDirectory\Summary.json" -Encoding utf8
    $results | Format-Table -AutoSize
    if (@($results | Where-Object { -not $_.Clean }).Count) { throw "Hook regression failed. Reports: $ReportDirectory" }
} finally {
    foreach ($process in $owned) { if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force } }
}
