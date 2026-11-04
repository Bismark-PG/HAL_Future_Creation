# Run with Windows PowerShell 5.1 and PowerShell 7. Uses real redirected child processes,
# synthetic Automation reports, and a fake engine directory; never launches Unreal.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testRoot = Join-Path $projectRoot ('Saved\GameFlow\LoopToolFixture_' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$utf8 = New-Object Text.UTF8Encoding($true)
function Write-Fixture([string]$Path, [string]$Content) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    [IO.File]::WriteAllText($Path, $Content, $utf8)
}
$engine = Join-Path $testRoot 'Engine Space'
Write-Fixture (Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') 'Fixture placeholder, never executed.'
$worker = Join-Path $testRoot 'Worker.ps1'
Write-Fixture $worker @'
param([string]$LogPath, [string]$ReportPath, [string]$Scenario, [int]$Port)
$ErrorActionPreference = 'Stop'
Set-Content -LiteralPath $LogPath -Value "Fixture listening on port $Port" -Encoding utf8
Start-Sleep -Milliseconds 400
if ($Scenario -ne 'Missing') {
    New-Item -ItemType Directory -Path $ReportPath -Force | Out-Null
    if ($Scenario -eq 'Malformed') { Set-Content -LiteralPath "$ReportPath\index.json" -Value '{' -Encoding utf8 }
    else {
        $report = [pscustomobject]@{succeeded=1;succeededWithWarnings=0;failed=0;notRun=0;inProcess=0}
        if ($Scenario -eq 'Failed') { $report.succeeded=0; $report.failed=1 }
        if ($Scenario -eq 'Warnings') { $report.succeeded=0; $report.succeededWithWarnings=1 }
        $report | ConvertTo-Json | Set-Content -LiteralPath "$ReportPath\index.json" -Encoding utf8
    }
}
if ($Scenario -eq 'ExitCode') { exit 9 }
exit 0
'@
$driver = Join-Path $testRoot 'Driver.ps1'
Write-Fixture $driver @'
param([string]$Runner,[string]$EngineRoot,[string]$ReportDirectory,[string]$Scenario,[string]$Worker,[string]$Shell)
$ErrorActionPreference='Stop'
function Start-Process {
    param($FilePath,$ArgumentList,$WorkingDirectory,$WindowStyle,[switch]$PassThru,$RedirectStandardOutput,$RedirectStandardError)
    $logPath = ($ArgumentList | Where-Object { $_ -like '-abslog=*' }).Substring(8).Trim('"')
    $reportPath = ($ArgumentList | Where-Object { $_ -like '-ReportExportPath=*' }).Substring(18).Trim('"')
    $fixtureScenario = if ([IO.Path]::GetFileNameWithoutExtension($logPath) -eq 'Host') { $Scenario } else { 'Pass' }
    Microsoft.PowerShell.Management\Start-Process -FilePath $Shell -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$Worker`"",'-LogPath',"`"$logPath`"",'-ReportPath',"`"$reportPath`"",'-Scenario',$fixtureScenario,'-Port',7955) -WindowStyle Hidden -PassThru -RedirectStandardOutput $RedirectStandardOutput -RedirectStandardError $RedirectStandardError
}
& $Runner -EngineRoot $EngineRoot -Profile Development -Port 7955 -ReportDirectory $ReportDirectory
'@
$shell = (Get-Process -Id $PID).Path
$runner = Join-Path $projectRoot 'Tools\TestGameFlowLoop.ps1'
$results = foreach ($scenario in @('Pass','ExitCode','Failed','Warnings','Missing','Malformed')) {
    $reports = Join-Path $testRoot $scenario
    $console = Join-Path $testRoot ($scenario + '_Console.log')
    # Invoke a child shell so a terminating script error has the same exit behavior as the user's command.
    $driverArguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',"`"$driver`"",'-Runner',"`"$runner`"",'-EngineRoot',"`"$engine`"",'-ReportDirectory',"`"$reports`"",'-Scenario',$scenario,'-Worker',"`"$worker`"",'-Shell',"`"$shell`"")
    $driverProcess = Start-Process -FilePath $shell -ArgumentList $driverArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $console -RedirectStandardError (Join-Path $testRoot ($scenario + '_Stderr.log'))
    $null = $driverProcess.Handle
    $driverProcess.WaitForExit()
    $exitCode = $driverProcess.ExitCode
    $summary = Get-Content -LiteralPath "$reports\Summary.json" -Raw | ConvertFrom-Json
    if ($summary.Count -ne 2) { throw "Expected both role results: $scenario" }
    $hostResult = $summary | Where-Object { $_.Role -eq 'Host' }
    if ($scenario -eq 'Pass') {
        if ($exitCode -ne 0 -or $hostResult.ExitCode -ne 0 -or $hostResult.Error) { throw 'Successful processes/reports were misclassified.' }
    } else {
        if ($exitCode -eq 0 -or -not $hostResult.Error) { throw "Failure was incorrectly accepted: $scenario" }
        if ($scenario -eq 'ExitCode' -and $hostResult.ExitCode -ne 9) { throw 'Real nonzero child exit code was lost.' }
        $expected = @{ExitCode='Process exit code 9';Failed='failed=1';Warnings='warnings=1';Missing='Missing report';Malformed='Unreadable report'}[$scenario]
        if (-not $hostResult.Error.Contains($expected)) { throw "Missing actionable reason for $scenario" }
    }
    [pscustomobject]@{Scenario=$scenario;Verified=$true;Shell=$PSVersionTable.PSVersion.ToString();RunnerExit=$exitCode;HostExit=$hostResult.ExitCode;Reports=$reports}
}
$results | ConvertTo-Json | Set-Content -LiteralPath "$testRoot\Results.json" -Encoding utf8
$results | Format-Table Scenario,Verified,Shell,RunnerExit,HostExit
Write-Output "Fixture results: $testRoot\Results.json"
