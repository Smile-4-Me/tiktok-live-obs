$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$profilesSource = Join-Path $root 'src\bridge_dock_profiles.cpp'
$tokenStoreSource = Join-Path $root 'src\token_store.cpp'

foreach ($path in @($profilesSource, $tokenStoreSource)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required source file is missing: $path"
    }
}

$profiles = Get-Content -LiteralPath $profilesSource -Raw
$tokenStore = Get-Content -LiteralPath $tokenStoreSource -Raw

# Profiles are deliberately non-secret metadata. Credentials must only be
# persisted through the Windows Credential Manager implementation.
foreach ($forbiddenKey in @('stream_server', 'stream_key', 'server', 'key')) {
    $pattern = 'settings\.setValue\(QStringLiteral\("' + $forbiddenKey + '"\)'
    if ($profiles -match $pattern) {
        throw "Profile INI persistence must not contain credential field '$forbiddenKey'."
    }
}

foreach ($requiredMarker in @('CredWriteW', 'CredReadW', 'save_live_credentials', 'load_live_credentials')) {
    if ($tokenStore -notmatch [regex]::Escape($requiredMarker)) {
        throw "Credential Manager storage marker is missing: $requiredMarker"
    }
}

Write-Host 'Secret-storage verification passed: profile INI remains metadata-only.'
