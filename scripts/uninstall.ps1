# Dev uninstall: run the shipped scripts/uninstall.cmd against every install on
# this machine, the same set scripts/deploy.ps1 writes to. uninstall.cmd on its
# own resolves a single install, because the launcher passes one and means it.
[CmdletBinding()]
param(
    [string]$GamePath
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

Import-Module (Join-Path $root 'cameraunlock-core\powershell\GamePathDetection.psm1')

$targets = if ($GamePath) { @($GamePath) } else { @(Find-AllGamePaths -GameId 'stormworks') }
if ($targets.Count -eq 0) { throw "Stormworks not found. Pass -GamePath or set STORMWORKS_PATH." }

foreach ($target in $targets) {
    & (Join-Path $PSScriptRoot 'uninstall.cmd') $target /y
    if ($LASTEXITCODE -ne 0) { throw "uninstall.cmd exited $LASTEXITCODE for $target" }
    Write-Host "Uninstalled from $target" -ForegroundColor Green
}
