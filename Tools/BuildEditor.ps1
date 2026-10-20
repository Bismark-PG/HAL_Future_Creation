# Windows PowerShell 5.1 compatible. Keep console messages ASCII for any locale.
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [switch]$CheckOnly
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'HAL_Future_Creation.uproject'
$target = 'HAL_Future_CreationEditor'

function Find-ProjectEngine {
    param([string]$Association, [string]$OverrideRoot)

    $candidates = @()
    if ($OverrideRoot) {
        $candidates += $OverrideRoot
    }
    else {
        # Source builds registered with UnrealVersionSelector.
        $registered = Get-ItemProperty 'HKCU:\Software\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
        if ($registered) {
            $entry = $registered.PSObject.Properties[$Association]
            if ($entry) { $candidates += [string]$entry.Value }
        }

        foreach ($key in @(
            "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association",
            "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$Association"
        )) {
            $installed = Get-ItemProperty $key -ErrorAction SilentlyContinue
            if ($installed.InstalledDirectory) { $candidates += $installed.InstalledDirectory }
        }

        if ($env:ProgramData) {
            $launcherFile = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
            if (Test-Path -LiteralPath $launcherFile -PathType Leaf) {
                try {
                    $launcher = Get-Content -LiteralPath $launcherFile -Raw -Encoding UTF8 | ConvertFrom-Json
                    foreach ($installation in $launcher.InstallationList) {
                        if ($installation.AppName -eq "UE_$Association") {
                            $candidates += $installation.InstallLocation
                        }
                    }
                }
                catch { Write-Warning "Could not read Epic Launcher installations: $($_.Exception.Message)" }
            }
        }

        if ($env:ProgramFiles) {
            $candidates += Join-Path $env:ProgramFiles "Epic Games\UE_$Association"
        }
    }

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        $batchFile = Join-Path $candidate 'Engine\Build\BatchFiles\Build.bat'
        $versionFile = Join-Path $candidate 'Engine\Build\Build.version'
        if (!(Test-Path -LiteralPath $batchFile -PathType Leaf) -or
            !(Test-Path -LiteralPath $versionFile -PathType Leaf)) { continue }

        $version = Get-Content -LiteralPath $versionFile -Raw -Encoding UTF8 | ConvertFrom-Json
        $majorMinor = '{0}.{1}' -f $version.MajorVersion, $version.MinorVersion
        if ($Association -match '^\d+\.\d+$' -and $majorMinor -ne $Association) {
            Write-Warning "Skipping engine $candidate : expected $Association, found $majorMinor."
            continue
        }
        return [PSCustomObject]@{
            Root = (Resolve-Path -LiteralPath $candidate).ProviderPath
            Version = $version
        }
    }

    throw "No usable engine found for '$Association'. Run Build_Editor.cmd -EngineRoot `"C:\path\to\UE_5.8`" with your local engine folder."
}

$exitCode = 1
$transcriptStarted = $false
try {
    if (!$CheckOnly) {
        $logDirectory = Join-Path $projectRoot 'Saved\BuildTool'
        New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
        $logFile = Join-Path $logDirectory ('BuildEditor_{0}.log' -f (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
        Start-Transcript -LiteralPath $logFile | Out-Null
        $transcriptStarted = $true
        Write-Host "Log: $logFile"
    }

    $project = Get-Content -LiteralPath $projectFile -Raw -Encoding UTF8 | ConvertFrom-Json
    $association = ([string]$project.EngineAssociation).Trim()
    if (!$association -and !$EngineRoot) {
        throw 'The project has no EngineAssociation. Specify -EngineRoot explicitly.'
    }
    if (!(Test-Path -LiteralPath (Join-Path $projectRoot "Source\$target.Target.cs") -PathType Leaf)) {
        throw "Missing Editor target: Source\$target.Target.cs"
    }

    $engine = Find-ProjectEngine -Association $association -OverrideRoot $EngineRoot
    $versionLabel = '{0}.{1}.{2} (CL {3})' -f $engine.Version.MajorVersion, $engine.Version.MinorVersion, $engine.Version.PatchVersion, $engine.Version.Changelist
    Write-Host "Project: $projectFile"
    Write-Host "Engine:  $($engine.Root)"
    Write-Host "Version: $versionLabel"
    Write-Host "Target:  $target Win64 Development"
    Write-Host 'Use the same engine patch version as your teammates.' -ForegroundColor Yellow

    # Conservative guard: do not kill processes or compile into a loaded module.
    $blocking = @(Get-Process | Where-Object {
        $_.ProcessName -like 'UnrealEditor*' -or
        $_.ProcessName -eq 'LiveCodingConsole' -or
        $_.ProcessName -like 'HAL_Future_Creation*'
    })
    if ($blocking.Count -gt 0) {
        $blocking | Select-Object ProcessName, Id | Format-Table -AutoSize | Out-Host
        throw 'Save your work and close Unreal Editor, game windows and Live Coding, then run this tool again. No build was started.'
    }

    if ($CheckOnly) {
        Write-Host 'Preflight passed. CheckOnly: no build was started and no log was created.' -ForegroundColor Green
        $exitCode = 0
    }
    else {
        $batchFile = Join-Path $engine.Root 'Engine\Build\BatchFiles\Build.bat'
        $buildArguments = @($target, 'Win64', 'Development', "-Project=$projectFile", '-WaitMutex', '-NoHotReload')
        Write-Host 'Building Editor target. This may take several minutes...'
        Push-Location -LiteralPath $projectRoot
        try {
            # Native stderr must remain visible without replacing UBT's exit code.
            $ErrorActionPreference = 'Continue'
            $LASTEXITCODE = $null
            & $batchFile @buildArguments 2>&1 | Out-Host
            if ($null -eq $LASTEXITCODE) { throw 'The Unreal build process did not return an exit code.' }
            $exitCode = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = 'Stop'
            Pop-Location
        }

        if ($exitCode -eq 0) {
            Write-Host 'BUILD SUCCEEDED. You can now reopen the project.' -ForegroundColor Green
        }
        else {
            Write-Host "BUILD FAILED (exit code $exitCode). See errors above and the saved log." -ForegroundColor Red
        }
        Write-Host "Log: $logFile"
    }
}
catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    $exitCode = 1
}
finally {
    if ($transcriptStarted) { Stop-Transcript | Out-Null }
}

exit $exitCode
