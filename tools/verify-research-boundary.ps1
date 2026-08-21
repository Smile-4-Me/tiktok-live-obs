# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 TikTok Live OBS Contributors

[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

$researchFiles = @(
    (Join-Path $RepositoryRoot 'src\research_lab.cpp'),
    (Join-Path $RepositoryRoot 'src\research_lab.hpp')
)

foreach ($file in $researchFiles) {
    if (-not (Test-Path -LiteralPath $file)) {
        throw "Research boundary check cannot find $file."
    }
}

$source = ($researchFiles | ForEach-Object {
    Get-Content -LiteralPath $_ -Raw
}) -join "`n"

if ($source -notmatch 'QHostAddress::LocalHost' -or
    $source -notmatch '127\.0\.0\.1') {
    throw 'Research Lab must bind and send requests only through localhost.'
}

if ($source -notmatch 'heartbeat_interval_ms\s*=\s*2000') {
    throw 'Research Lab must retain its documented two-second local heartbeat.'
}

# These terms intentionally cover categories, not implementations. If a future
# change legitimately needs one, it must first update the research record and
# pass the Future Provider Contract review.
$forbiddenPatterns = @(
    'tiktok\.com',
    'tiktokcdn',
    'ffmpeg',
    'encoder',
    'signer',
    'signature',
    'integrity',
    'metadata injection',
    'frame sign'
)

foreach ($pattern in $forbiddenPatterns) {
    if ($source -match $pattern) {
        throw "Research boundary violation: '$pattern' was found in Research Lab source."
    }
}

Write-Host 'Research boundary verification passed: localhost-only, generic two-second heartbeat.'
