# Dev deploy: stage the built opengl32.dll proxy, plus a copy of the system
# opengl32.dll as opengl32_real.dll, next to stormworks64.exe in every install
# on this machine. Mirrors launcher-manifest.json's files entry and its
# system-file-copy runtime_requirement.
[CmdletBinding()]
param(
    [string]$GamePath
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

Import-Module (Join-Path $root 'cameraunlock-core\powershell\GamePathDetection.psm1')
Import-Module (Join-Path $root 'cameraunlock-core\powershell\DevDeploy.psm1')

# Throws with the standard not-found diagnostic when no install resolves, so the
# loop below always has at least one target.
Invoke-DevDeployShim `
    -GameId 'stormworks' `
    -GameDisplayName 'Stormworks: Build and Rescue' `
    -BuildOutputPath (Join-Path $root 'build\Release') `
    -ModDllName 'opengl32.dll' `
    -ShimMarker 'StormworksHeadTracking attached' `
    -GivenPath $GamePath | Out-Null

# Refreshed on every deploy, as install.cmd and the manifest's system-file-copy
# do: a copy captured before a Windows update is not the opengl32.dll the rest
# of System32 now expects. Invoke-DevDeployShim's ExtraDlls cannot carry it,
# because it would back an existing copy up as a user original.
$realSrc = Join-Path $env:WINDIR 'System32\opengl32.dll'
$targets = if ($GamePath) { @($GamePath) } else { @(Find-AllGamePaths -GameId 'stormworks') }
foreach ($target in $targets) {
    $exeDir = Resolve-DevExeDir -GamePath $target -GameId 'stormworks'
    Copy-Item -LiteralPath $realSrc -Destination (Join-Path $exeDir 'opengl32_real.dll') -Force
    Write-Host "Deployed opengl32.dll and opengl32_real.dll to $exeDir" -ForegroundColor Green
}
