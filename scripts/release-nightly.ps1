[CmdletBinding()]
param(
    [switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $ProjectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force

$pixiFile = Join-Path $ProjectRoot 'pixi.toml'
$versionMatch = Select-String -Path $pixiFile -Pattern '^version\s*=\s*"([^"]+)"'
if (-not $versionMatch) {
    throw "Could not extract version from $pixiFile"
}
$version = $versionMatch.Matches[0].Groups[1].Value

# No Nexus ZIP: package-release.ps1 produces only the installer ZIP (see its
# header comment for why).
Publish-NightlyBuild `
    -ModId 'stormworks' `
    -ModName 'StormworksHeadTracking' `
    -Version $version `
    -ProjectRoot $ProjectRoot `
    -BuildCommand 'pixi run build' `
    -NoNexusZip `
    -AllowDirty:$AllowDirty
