#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
  Compare the installed stormworks64.exe against the build profiles in
  src/StormworksHeadTracking/builds/steam_offsets.cpp.
.DESCRIPTION
  The same three-field PE check (TimeDateStamp / SizeOfImage / CheckSum) that
  builds::SelectProfile runs at mod load. Run it after a Steam patch to find
  out whether the cull-frustum hook's addresses need rederiving: a buildid can
  move without the executable changing at all.

  Exit codes:
    0 = matches a committed profile, nothing to do
    1 = matches none of them, rederive and ADD a profile
    2 = the EXE could not be found
.PARAMETER ExePath
  Path to stormworks64.exe. Without it, every install on this machine is
  checked.
#>
param(
    [Parameter(Position = 0)]
    [string]$ExePath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectDir = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $projectDir 'cameraunlock-core/powershell/GamePathDetection.psm1') -Force

function Read-PeFingerprint {
    param([string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        $stream.Position = 0x3c
        $peOffset = $reader.ReadUInt32()
        $stream.Position = $peOffset
        $signature = $reader.ReadUInt32()
        if ($signature -ne 0x00004550) {
            throw ("Not a PE file: signature 0x{0:x} at 0x{1:x}" -f $signature, $peOffset)
        }
        $stream.Position = $peOffset + 8
        $timeDateStamp = $reader.ReadUInt32()
        $stream.Position = $peOffset + 4 + 20 + 0x38
        $sizeOfImage = $reader.ReadUInt32()
        $stream.Position = $peOffset + 4 + 20 + 0x40
        $checkSum = $reader.ReadUInt32()
        return [pscustomobject]@{
            TimeDateStamp = $timeDateStamp
            SizeOfImage   = $sizeOfImage
            CheckSum      = $checkSum
        }
    } finally {
        $stream.Dispose()
    }
}

function Read-CommittedProfiles {
    $profiles = @()
    $offsetFiles = Get-ChildItem (Join-Path $projectDir 'src/StormworksHeadTracking/builds') -Filter '*_offsets.cpp'
    foreach ($file in $offsetFiles) {
        $cpp = Get-Content -Raw $file.FullName
        $pattern = '(?s)name\s*\*/\s*"([^"]+)".*?fingerprint\s*\*/\s*\{\s*0x([0-9a-fA-F]+)u?\s*,\s*0x([0-9a-fA-F]+)u?\s*,\s*0x([0-9a-fA-F]+)u?\s*\}'
        foreach ($m in [regex]::Matches($cpp, $pattern)) {
            $profiles += [pscustomobject]@{
                Name          = $m.Groups[1].Value
                TimeDateStamp = [Convert]::ToUInt32($m.Groups[2].Value, 16)
                SizeOfImage   = [Convert]::ToUInt32($m.Groups[3].Value, 16)
                CheckSum      = [Convert]::ToUInt32($m.Groups[4].Value, 16)
            }
        }
    }
    if ($profiles.Count -eq 0) {
        throw "No build profiles found in src/StormworksHeadTracking/builds"
    }
    return $profiles
}

# Every install on this machine, not just the first one found: a player can own
# the game on more than one store, and each is its own build.
$exes = @()
if ($ExePath) {
    if (-not (Test-Path $ExePath)) {
        Write-Host "ERROR: no EXE at $ExePath" -ForegroundColor Red
        exit 2
    }
    $exes += (Resolve-Path $ExePath).Path
} else {
    foreach ($root in @(Find-AllGamePaths -GameId 'stormworks')) {
        $candidate = Join-Path $root 'stormworks64.exe'
        if (Test-Path $candidate) { $exes += $candidate }
    }
    if ($exes.Count -eq 0) {
        Write-Host "ERROR: no Stormworks install found. Pass the path to stormworks64.exe." -ForegroundColor Red
        exit 2
    }
}

$profiles = Read-CommittedProfiles
foreach ($p in $profiles) {
    Write-Host ("Profile:  ts=0x{0:x8} size=0x{1:x8} csum=0x{2:x8}  {3}" -f $p.TimeDateStamp, $p.SizeOfImage, $p.CheckSum, $p.Name)
}

$allMatched = $true
foreach ($exe in $exes) {
    Write-Host ""
    Write-Host "EXE:      $exe"
    $running = Read-PeFingerprint -Path $exe
    Write-Host ("Running:  ts=0x{0:x8} size=0x{1:x8} csum=0x{2:x8}" -f $running.TimeDateStamp, $running.SizeOfImage, $running.CheckSum)
    $match = $profiles | Where-Object {
        $running.TimeDateStamp -eq $_.TimeDateStamp -and
        $running.SizeOfImage -eq $_.SizeOfImage -and
        $running.CheckSum -eq $_.CheckSum
    } | Select-Object -First 1
    if ($match) {
        Write-Host ("MATCH - profile {0}, nothing to rederive." -f $match.Name) -ForegroundColor Green
        continue
    }
    $allMatched = $false
    Write-Host "MISMATCH - this EXE matches no committed profile." -ForegroundColor Yellow
    Write-Host ("Paste-ready stub for steam_offsets.cpp (fill in the RVA and offsets):")
    Write-Host ("    const BuildProfile kSteamProfile_{0} = {{" -f ([DateTimeOffset]::FromUnixTimeSeconds($running.TimeDateStamp).UtcDateTime.ToString('yyyyMMdd')))
    Write-Host ("        /* name        */ `"steam-win64-{0}`"," -f ([DateTimeOffset]::FromUnixTimeSeconds($running.TimeDateStamp).UtcDateTime.ToString('yyyyMMdd')))
    Write-Host ("        /* fingerprint */ {{0x{0:x8}u, 0x{1:x8}u, 0x{2:x8}u}}," -f $running.TimeDateStamp, $running.SizeOfImage, $running.CheckSum)
    Write-Host ("        /* offsets     */ {{ ... }},")
    Write-Host ("    };")
    Write-Host "Add it to the TOP of kKnownProfiles. Never edit an existing profile: players who"
    Write-Host "have not taken the patch still match it."
}

if ($allMatched) { exit 0 } else { exit 1 }
