#Requires -Version 5.1
# Package release/StormworksHeadTracking-v<version>-installer.zip: install.cmd,
# uninstall.cmd, the shared/ bundle their bodies live in, opengl32.dll under
# plugins/, launcher-manifest.json at the root, and the docs every binary
# distribution has to carry.
#
# Installer-only: there is deliberately no -nexus.zip stage. The proxy forwards
# every GL export it does not intercept to opengl32_real.dll, a copy of the
# user's own System32\opengl32.dll that is not redistributable and so cannot be
# in any ZIP. Extracting an archive into the game folder can only drop
# opengl32.dll next to stormworks64.exe with nothing to forward to, and the game
# then has no OpenGL. Capturing that copy is install-time work, done by
# install.cmd and by the manifest's system-file-copy runtime requirement.
# release-nightly.ps1 passes -NoNexusZip for the same reason.
$ErrorActionPreference = "Stop"

$projectDir = Split-Path -Parent $PSScriptRoot
$modName = "StormworksHeadTracking"

Import-Module (Join-Path $projectDir "cameraunlock-core/powershell/ReleaseWorkflow.psm1") -Force

# pixi.toml is the canonical version (release.yml reads it too). The other
# copies ship inside this ZIP, so a mismatch fails here rather than producing
# an archive whose name, manifest and installer state disagree.
$version = (Select-String -Path (Join-Path $projectDir "pixi.toml") -Pattern '^version\s*=\s*"([^"]+)"').Matches[0].Groups[1].Value
$copies = [ordered]@{
    "CMakeLists.txt"         = '(?m)^project\(StormworksHeadTracking VERSION (\d+\.\d+\.\d+)'
    "launcher-manifest.json" = '(?m)^\s*"version"\s*:\s*"([^"]+)"'
    "scripts/install.cmd"    = 'set "MOD_VERSION=([^"]*)"'
}
foreach ($file in $copies.Keys) {
    $text = Get-Content (Join-Path $projectDir $file) -Raw
    if ($text -notmatch $copies[$file]) { throw "No version found in $file." }
    if ($matches[1] -ne $version) {
        throw "$file carries version $($matches[1]) but pixi.toml says $version. scripts/release.ps1 keeps them in step; fix $file by hand."
    }
}

$proxy = Join-Path $projectDir "build/Release/opengl32.dll"
if (-not (Test-Path $proxy)) {
    throw "Release build not found at $proxy. Run 'pixi run build' first."
}

$releaseDir = Join-Path $projectDir "release"
$staging = Join-Path $releaseDir "staging-installer"

if (Test-Path $releaseDir) { Remove-Item $releaseDir -Recurse -Force }
New-Item -ItemType Directory -Path $releaseDir | Out-Null
New-Item -ItemType Directory -Path $staging | Out-Null
New-Item -ItemType Directory -Path (Join-Path $staging "plugins") | Out-Null
Copy-Item $proxy -Destination (Join-Path $staging "plugins")

# Copy-SharedBundle also asserts THIRD-PARTY-NOTICES.md names the pinned
# cameraunlock-core commit - do not hand-roll this staging.
Copy-SharedBundle -StagingDir $staging

# A .cmd with LF line endings fails in cmd.exe without a useful error, and a
# wrapper whose body is not in shared/ fails the same way on the user's machine
# only. Both are checked against what is actually staged.
foreach ($wrapper in "install.cmd", "uninstall.cmd") {
    $src = Join-Path $PSScriptRoot $wrapper
    $text = [System.IO.File]::ReadAllText($src)
    if ($text -match '(?<!\r)\n') {
        throw "scripts/$wrapper has LF line endings. Run: unix2dos scripts/$wrapper"
    }
    $bodies = @([regex]::Matches($text, 'shared\\([\w.-]+\.cmd)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    if ($bodies.Count -eq 0) { throw "scripts/$wrapper names no body under shared\." }
    foreach ($body in $bodies) {
        if (-not (Test-Path (Join-Path $staging "shared/$body"))) {
            throw "scripts/$wrapper dispatches to shared\$body, which the pinned cameraunlock-core does not provide. Bump the submodule to a core commit that has scripts/$body, then re-run."
        }
    }
    Copy-Item $src -Destination $staging
}

# The launcher reads exactly this filename from the installer ZIP root.
$manifest = Join-Path $projectDir "launcher-manifest.json"
if (-not (Test-Path $manifest)) {
    throw "launcher-manifest.json not found at project root. The launcher manifest must ship in the installer ZIP."
}
Copy-Item $manifest -Destination $staging

Copy-LicenceNotices -StagingDir $staging -ProjectRoot $projectDir -Additional @("README.md", "CHANGELOG.md")

$installerZip = Join-Path $releaseDir "$modName-v$version-installer.zip"
Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $installerZip -Force
Remove-Item $staging -Recurse -Force
Write-Host "Wrote $installerZip" -ForegroundColor Green
