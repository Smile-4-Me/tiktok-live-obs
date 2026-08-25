# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 TikTok Live OBS Contributors

<#
.SYNOPSIS
Installs a locally built TikTok Live OBS plugin into one explicit OBS installation.

.DESCRIPTION
The script is deliberately strict. It verifies the OBS executable, OBS' own locale
catalogue, the plugin binary, and the complete plugin locale layout before copying
anything. OBS is always launched from its binary directory so its core data files
are resolved correctly in portable installations as well.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$ObsRoot,

    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'RelWithDebInfo',

    [switch]$RestartObs,

    [ValidateRange(3, 60)]
    [int]$LoadVerificationTimeoutSeconds = 15
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$obsRootPath = (Resolve-Path -LiteralPath $ObsRoot).Path
$obsBinaryDirectory = Join-Path $obsRootPath 'bin\64bit'
$obsExecutable = Join-Path $obsBinaryDirectory 'obs64.exe'
$obsLocale = Join-Path $obsRootPath 'data\obs-studio\locale\en-US.ini'
$pluginBinary = Join-Path $repositoryRoot "dist\$Configuration\tiktok-live-obs.dll"
$sourceLocale = Join-Path $repositoryRoot 'data\locale'
$sourceAssets = Join-Path $repositoryRoot 'data\assets'
$pluginDataDirectory = Join-Path $obsRootPath 'data\obs-plugins\tiktok-live-obs'
$targetLocale = Join-Path $pluginDataDirectory 'locale'
$targetAssets = Join-Path $pluginDataDirectory 'assets'
$targetBinary = Join-Path $obsRootPath 'obs-plugins\64bit\tiktok-live-obs.dll'
$obsLogsDirectory = Join-Path $obsRootPath 'config\obs-studio\logs'

$requiredSourceFiles = @(
    $pluginBinary,
    (Join-Path $sourceLocale 'en-US.ini'),
    (Join-Path $sourceLocale 'core\en-US.ini'),
    (Join-Path $sourceLocale 'providers\manual\en-US.ini'),
    (Join-Path $sourceLocale 'providers\streamlabs\en-US.ini'),
    (Join-Path $sourceLocale 'providers\tiktok-studio\en-US.ini'),
    (Join-Path $sourceAssets 'garbage-bin-10428.svg')
)

if (-not (Test-Path -LiteralPath $obsExecutable)) {
    throw "The selected directory is not an OBS installation: $obsExecutable was not found."
}
if (-not (Test-Path -LiteralPath $obsLocale)) {
    throw "OBS core locale verification failed: $obsLocale was not found."
}
foreach ($file in $requiredSourceFiles) {
    if (-not (Test-Path -LiteralPath $file)) {
        throw "Plugin preflight failed: required build or locale file is missing: $file"
    }
}

$smartAppControlPolicy = Get-ItemProperty `
    -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' `
    -ErrorAction SilentlyContinue
if ($smartAppControlPolicy.VerifiedAndReputablePolicyState -eq 1) {
    $signature = Get-AuthenticodeSignature -FilePath $pluginBinary
    if ($signature.Status -ne 'Valid') {
        throw @"
Smart App Control is enforcing application trust on this PC and will block this unsigned plugin DLL.
No files were copied, so the currently installed plugin remains untouched.

Use a DLL signed with a trusted code-signing certificate, or disable Smart App
Control manually before a local unsigned-development deployment. The installer
does not offer a bypass flag because copying a DLL cannot make Windows trust it.
"@
    }
}

$runningObs = Get-Process obs64 -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -eq $obsExecutable }
if ($runningObs) {
    if (-not $RestartObs) {
        throw 'The selected OBS instance is open. Close it first, or run this script with -RestartObs.'
    }
    # A forced termination leaves OBS in its crash-recovery state. Its recovery
    # dialog prevents every plugin from loading and made the old verifier report
    # a misleading failure. Request a normal close and refuse to overwrite a
    # still-running instance rather than killing it.
    foreach ($process in $runningObs) {
        [void]$process.CloseMainWindow()
    }
    $closeDeadline = (Get-Date).AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 250
        $runningObs = Get-Process obs64 -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -eq $obsExecutable }
    } while ($runningObs -and (Get-Date) -lt $closeDeadline)

    if ($runningObs) {
        throw 'OBS did not close normally. Close the selected OBS instance and run the installer again; no files were changed.'
    }
}

New-Item -ItemType Directory -Path (Split-Path -Parent $targetBinary) -Force | Out-Null
New-Item -ItemType Directory -Path $targetLocale -Force | Out-Null
New-Item -ItemType Directory -Path $targetAssets -Force | Out-Null
Copy-Item -LiteralPath $pluginBinary -Destination $targetBinary -Force
Get-ChildItem -LiteralPath $sourceLocale -Force |
    Copy-Item -Destination $targetLocale -Recurse -Force
Get-ChildItem -LiteralPath $sourceAssets -Force |
    Copy-Item -Destination $targetAssets -Recurse -Force

$requiredInstalledFiles = @(
    $targetBinary,
    (Join-Path $targetLocale 'en-US.ini'),
    (Join-Path $targetLocale 'core\en-US.ini'),
    (Join-Path $targetLocale 'providers\manual\en-US.ini'),
    (Join-Path $targetLocale 'providers\streamlabs\en-US.ini'),
    (Join-Path $targetLocale 'providers\tiktok-studio\en-US.ini'),
    (Join-Path $targetAssets 'garbage-bin-10428.svg')
)
foreach ($file in $requiredInstalledFiles) {
    if (-not (Test-Path -LiteralPath $file)) {
        throw "Installation verification failed: expected file is missing: $file"
    }
}

Write-Host "TikTok Live OBS installed successfully in: $obsRootPath"
if ($RestartObs) {
    $launchTime = Get-Date
    $startedObs = Start-Process -FilePath $obsExecutable -ArgumentList '--portable' `
        -WorkingDirectory $obsBinaryDirectory -PassThru
    Write-Host 'OBS started with its binary directory as the working directory. Verifying plugin load...'

    $deadline = $launchTime.AddSeconds($LoadVerificationTimeoutSeconds)
    $pluginLoaded = $false
    while ((Get-Date) -lt $deadline -and -not $pluginLoaded) {
        Start-Sleep -Milliseconds 500
        try {
            $startedObs.Refresh()
            if ($startedObs.HasExited) {
                break
            }

            # OBS buffers its text log during startup. The module list belongs to
            # the running process, so it is the immediate and authoritative check.
            $pluginModule = $startedObs.Modules | Where-Object {
                $_.FileName -ieq $targetBinary
            } | Select-Object -First 1
            $pluginLoaded = $null -ne $pluginModule
        } catch {
            # Module enumeration can briefly fail while the process is starting.
            # Keep waiting until the bounded deadline, then use the text log as
            # supplemental diagnostics below.
            continue
        }
    }

    if ($pluginLoaded) {
        Write-Host "Plugin load verified in OBS process $($startedObs.Id)."
        return
    }

    $candidate = $null
    if (Test-Path -LiteralPath $obsLogsDirectory) {
        $candidate = Get-ChildItem -LiteralPath $obsLogsDirectory -Filter '*.txt' -File |
            Where-Object { $_.LastWriteTime -ge $launchTime.AddSeconds(-2) } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
    }
    $logText = if ($candidate) {
        Get-Content -LiteralPath $candidate.FullName -Raw -ErrorAction SilentlyContinue
    } else {
        ''
    }
    if ($logText -match 'LoadLibrary failed.*tiktok-live-obs\.dll|tiktok-live-obs\.dll.*not loaded') {
        throw @"
OBS started but Windows prevented TikTok Live OBS from loading.
The copied files and locale catalogues are present; this is a Windows application-control decision for this exact DLL build.
Fresh OBS log: $($candidate.FullName)
"@
    }
    throw "OBS did not load TikTok Live OBS within $LoadVerificationTimeoutSeconds seconds. Check the process and the current OBS log under: $obsLogsDirectory"
}
