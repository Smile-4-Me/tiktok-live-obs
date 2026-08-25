# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 TikTok Live OBS Contributors

<#
.SYNOPSIS
Verifies that two clean local builds of the same plugin source produce the same DLL.

.DESCRIPTION
This never touches an OBS installation. It is a build-integrity check for the
Windows target: Smart App Control evaluates each DLL artifact independently, so
unchanged source should not create avoidable new artifact identities.
#>

[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$BuildDirectory = 'build',

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $repositoryRoot $BuildDirectory
$pluginBinaryCandidates = @(
    # The current CMake target writes the module directly into dist/ for every
    # multi-config build. Keep the legacy configuration directory as a
    # fallback so this verifier remains useful for older local build trees.
    (Join-Path $repositoryRoot 'dist\tiktok-live-obs.dll'),
    (Join-Path $repositoryRoot "dist\$Configuration\tiktok-live-obs.dll")
)

if (-not (Test-Path -LiteralPath $buildPath)) {
    throw "Build directory was not found: $buildPath"
}

function Get-Sha256Hash {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $hasher = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString($hasher.ComputeHash($stream))).Replace('-', '')
    }
    finally {
        $hasher.Dispose()
        $stream.Dispose()
    }
}

function Build-CleanPlugin {
    # Send build output to the host explicitly. Otherwise PowerShell captures
    # it together with the SHA-256 string returned by this function.
    & cmake --build $buildPath --config $Configuration --target clean -- /m:1 /v:q | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw 'CMake clean failed.'
    }

    & cmake --build $buildPath --config $Configuration --target tiktok-live-obs -- /m:1 /v:q | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw 'CMake plugin build failed.'
    }
    $pluginBinary = $pluginBinaryCandidates | Where-Object {
        Test-Path -LiteralPath $_
    } | Select-Object -First 1
    if (-not $pluginBinary) {
        throw "The build completed without the expected DLL. Checked: $($pluginBinaryCandidates -join ', ')"
    }

    return Get-Sha256Hash -Path $pluginBinary
}

$firstHash = Build-CleanPlugin
$secondHash = Build-CleanPlugin

if ($firstHash -ne $secondHash) {
    throw "Reproducibility check failed. First hash: $firstHash; second hash: $secondHash"
}

Write-Host "Reproducible plugin DLL verified: $firstHash"
