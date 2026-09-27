#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Run the characterization tests and the config differential test.
.DESCRIPTION
    `pixi run build` compiles both test executables in the build tree. This checks
    that every file the differential test compiles still has the hash
    tests/config_differential/provenance.txt records, then runs both.

    Non-interactive: exits 0 when every test passes, non-zero on the first failure.
.NOTES
    Run via: pixi run test
#>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot

# The differential test is only as good as its claim about what it compiled. SHA256 through
# .NET, because Get-FileHash is not found when a runner's pwsh runs this under Windows
# PowerShell.
$provenance = Join-Path $projectRoot 'tests/config_differential/provenance.txt'
$sha256 = [System.Security.Cryptography.SHA256]::Create()
foreach ($line in Get-Content $provenance) {
    if ($line -match '^\s*(#|$)') { continue }
    $hash, $path = ($line -split '\s+', 3)[0, 1]
    $bytes = [System.IO.File]::ReadAllBytes((Join-Path $projectRoot $path))
    $actual = -join ($sha256.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') })
    if ($actual -ne $hash) { throw "$path has changed: sha256 $actual, provenance.txt records $hash" }
}

$testDir = Join-Path $projectRoot 'build/tests/Release'
foreach ($exe in 'StormworksHeadTrackingTests.exe', 'StormworksConfigDifferentialTests.exe') {
    & (Join-Path $testDir $exe)
    if ($LASTEXITCODE -ne 0) { Write-Host "ERROR: $exe failed" -ForegroundColor Red; exit 1 }
}

Write-Host 'All tests passed.' -ForegroundColor Green
