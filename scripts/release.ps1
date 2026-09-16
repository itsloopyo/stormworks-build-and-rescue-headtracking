#Requires -Version 5.1
# Fully unattended. `pixi run release <major|minor|patch|nightly|X.Y.Z>` is the
# authorization - there is no confirmation gate. Preconditions (on main,
# clean tree, tag absent, valid semver) are the safety net; any failure
# exits non-zero with a one-line diagnostic.
param(
    [string]$Version,
    # Ship a release even when there are no user-facing commits since the
    # last tag (writes a maintenance changelog entry instead of aborting).
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$projectDir = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $projectDir "cameraunlock-core/powershell/ReleaseWorkflow.psm1") -Force

$utf8NoBom = New-Object System.Text.UTF8Encoding $false

# Mirrors New-ChangelogFromCommits' insertion so a -Force maintenance entry
# lands in the same place with the same shape.
function Add-MaintenanceChangelogEntry {
    param([string]$Path, [string]$NewVersion)
    $date = Get-Date -Format 'yyyy-MM-dd'
    $entry = "## [$NewVersion] - $date`n`n### Changed`n`n- Maintenance release (no user-facing changes).`n`n"
    $changelog = [System.IO.File]::ReadAllText($Path)
    if ($changelog -match '(?s)(# Changelog.*?)(## \[)') {
        $changelog = $changelog -replace '(?s)(# Changelog.*?\n\n)', "`$1$entry"
    } else {
        $changelog = $changelog -replace '(?s)(# Changelog.*?\n)', "`$1$entry"
    }
    [System.IO.File]::WriteAllText($Path, $changelog.TrimEnd() + "`n", $utf8NoBom)
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Error "Usage: pixi run release <major|minor|patch|nightly|X.Y.Z>"
    exit 1
}

if ($Version -eq 'nightly') {
    & (Join-Path $PSScriptRoot 'release-nightly.ps1')
    exit $LASTEXITCODE
}

$pixiPath = Join-Path $projectDir "pixi.toml"
$changelogPath = Join-Path $projectDir "CHANGELOG.md"
$cmakePath = Join-Path $projectDir "CMakeLists.txt"
$manifestPath = Join-Path $projectDir "launcher-manifest.json"
$installCmdPath = Join-Path $PSScriptRoot "install.cmd"

# 1. Resolve + validate version against the canonical source (pixi.toml).
$pixiContent = Get-Content $pixiPath -Raw
if ($pixiContent -notmatch '(?m)^version\s*=\s*"([^"]+)"') {
    throw "No version field found in $pixiPath"
}
$currentVersion = $matches[1]
$newVersion = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $currentVersion
if (-not (Test-SemanticVersion -Version $newVersion)) {
    throw "Resolved version '$newVersion' is not valid semver (X.Y.Z)."
}
Write-Host "Releasing v$newVersion (current v$currentVersion)" -ForegroundColor Cyan

# 2. Preconditions - fail fast, never prompt. Checked before anything below
#    commits, so a wrong branch or a dirty tree leaves no trace.
$branch = (git -C $projectDir rev-parse --abbrev-ref HEAD).Trim()
if ($branch -ne "main") { throw "Releases must run on 'main' (currently on '$branch')." }
if (-not (Test-CleanGitStatus)) { throw "Working tree is not clean. Commit or stash first." }
if (Test-GitTagExists -Tag "v$newVersion") { throw "Tag v$newVersion already exists." }

# 3. Changelog. This is the gate that aborts when there is nothing to release,
#    so it runs BEFORE any version file is touched - a failure here leaves a
#    clean tree instead of a half-applied version bump with no tag.
Write-Host "Generating CHANGELOG..." -ForegroundColor Cyan
$hasVersionTags = git -C $projectDir tag -l 'v[0-9]*'
if (-not $hasVersionTags) {
    # First release. CHANGELOG.md is hand-written and carries its entries under
    # [Unreleased]; generating from commits would put every commit since the
    # repo began above that section and leave [Unreleased] in the release. So
    # promote the heading instead.
    $changelog = [System.IO.File]::ReadAllText($changelogPath)
    if ($changelog -match '(?m)^## \[Unreleased\]\s*$') {
        $date = Get-Date -Format 'yyyy-MM-dd'
        $changelog = $changelog -replace '(?m)^## \[Unreleased\][ \t]*$', "## [$newVersion] - $date"
        [System.IO.File]::WriteAllText($changelogPath, $changelog, $utf8NoBom)
        Write-Host "  Promoted [Unreleased] to [$newVersion]" -ForegroundColor Gray
    } elseif ($changelog -notmatch [regex]::Escape("## [$newVersion]")) {
        throw "First release: CHANGELOG.md has neither an [Unreleased] section nor a [$newVersion] section. Write the release notes by hand first."
    }
} else {
    try {
        New-ChangelogFromCommits -ChangelogPath $changelogPath -Version $newVersion | Out-Null
    } catch {
        if (-not $Force) {
            Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
            Write-Host "No user-facing changes to release. Re-run with -Force for a maintenance release." -ForegroundColor Yellow
            exit 1
        }
        Write-Host "No user-facing commits since last tag - writing maintenance entry (-Force)." -ForegroundColor Yellow
        Add-MaintenanceChangelogEntry -Path $changelogPath -NewVersion $newVersion
    }
}

# THIRD-PARTY-NOTICES.md names the cameraunlock-core commit compiled into the
# release ZIP, and bumping the submodule does not touch it. Packaging refuses
# to ship that mismatch, so re-sync it here and let this release carry the
# correction instead of failing at the package step. After the changelog gate,
# so an aborted release leaves no commit the author did not ask for.
& (Join-Path $projectDir 'cameraunlock-core\scripts\sync-core-notices.ps1') -Repo $projectDir
if ($LASTEXITCODE -ne 0) { throw "sync-core-notices.ps1 exited $LASTEXITCODE - fix THIRD-PARTY-NOTICES.md before releasing." }
& git -C $projectDir diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) {
    & git -C $projectDir commit -q -m 'chore: record the cameraunlock-core commit this build compiles' -- THIRD-PARTY-NOTICES.md
    if ($LASTEXITCODE -ne 0) { throw "Could not commit the re-synced THIRD-PARTY-NOTICES.md." }
    Write-Host 'THIRD-PARTY-NOTICES.md re-synced to the pinned cameraunlock-core commit.' -ForegroundColor Yellow
}

# 4. Bump the version in the canonical source and its copies. The manifest
#    and install.cmd ship in the installer ZIP (install.cmd writes its
#    MOD_VERSION into the install state file), and package-release.ps1 refuses
#    to package when any of them disagree with pixi.toml.
$pixiContent = $pixiContent -replace '(?m)^(version\s*=\s*")[^"]+(")', "`${1}$newVersion`${2}"
[System.IO.File]::WriteAllText($pixiPath, $pixiContent, $utf8NoBom)

$cmakeContent = Get-Content $cmakePath -Raw
if ($cmakeContent -notmatch '(?m)^project\(StormworksHeadTracking VERSION \d+\.\d+\.\d+') {
    throw "No project() VERSION line found in $cmakePath"
}
$cmakeContent = $cmakeContent -replace '(?m)^project\(StormworksHeadTracking VERSION \d+\.\d+\.\d+', "project(StormworksHeadTracking VERSION $newVersion"
[System.IO.File]::WriteAllText($cmakePath, $cmakeContent, $utf8NoBom)

$manifestContent = Get-Content $manifestPath -Raw
if ($manifestContent -notmatch '(?m)^\s*"version"\s*:\s*"[0-9]+\.[0-9]+\.[0-9]+"') {
    throw "No version field found in $manifestPath"
}
$manifestContent = $manifestContent -replace '(?m)^(\s*"version"\s*:\s*")[0-9]+\.[0-9]+\.[0-9]+(")', "`${1}$newVersion`${2}"
[System.IO.File]::WriteAllText($manifestPath, $manifestContent, $utf8NoBom)

# ReadAllText/WriteAllText keep install.cmd's CRLF line endings byte for byte.
$installCmd = [System.IO.File]::ReadAllText($installCmdPath)
if ($installCmd -notmatch 'set "MOD_VERSION=[^"]*"') {
    throw "No MOD_VERSION line found in $installCmdPath"
}
$installCmd = $installCmd -replace 'set "MOD_VERSION=[^"]*"', "set `"MOD_VERSION=$newVersion`""
[System.IO.File]::WriteAllText($installCmdPath, $installCmd, $utf8NoBom)

# 5. Release-config build.
Write-Host "Building release..." -ForegroundColor Cyan
pixi run build
if ($LASTEXITCODE -ne 0) { throw "Build failed; aborting release." }

# 6. Package and validate the manifest before anything is committed or
#    tagged - a manifest naming a file the ZIP does not contain must not
#    reach a tag.
Write-Host "Packaging..." -ForegroundColor Cyan
pixi run package
if ($LASTEXITCODE -ne 0) { throw "Package failed; aborting release." }
pixi run validate-manifest
if ($LASTEXITCODE -ne 0) { throw "launcher-manifest.json validation failed; aborting release." }

# 7. Commit the version bump + changelog. "Release v..." matches the
#    build.yml skip guard so CI doesn't double-build this commit.
git -C $projectDir add -- $pixiPath $changelogPath $cmakePath $manifestPath $installCmdPath
if ($LASTEXITCODE -ne 0) { throw "git add failed." }
git -C $projectDir commit -m "Release v$newVersion"
if ($LASTEXITCODE -ne 0) { throw "git commit failed." }

# 8. Annotated tag, then push commits + tag (triggers release.yml).
New-ReleaseTag -Version $newVersion -Message "Release v$newVersion" -Branch "main"

Write-Host "Released v$newVersion." -ForegroundColor Green
