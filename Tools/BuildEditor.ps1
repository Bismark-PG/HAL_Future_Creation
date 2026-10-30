# Legacy build-only adapter. The standalone root CMD owns all checks and build logic.
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [switch]$CheckOnly
)

$launcher = Join-Path (Split-Path -Parent $PSScriptRoot) 'BuildAndLaunch.cmd'
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) { Write-Error "Missing launcher: $launcher"; exit 1 }
$launcherArguments = @('-NoLaunch')
if ($EngineRoot) { $launcherArguments += @('-EngineRoot', $EngineRoot) }
if ($CheckOnly) { $launcherArguments += '-CheckOnly' }
& $launcher @launcherArguments
exit $LASTEXITCODE
