param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory,
    [string]$ObsDirectory = 'C:\Program Files\obs-studio'
)

$sourceDll = Join-Path $SourceDirectory 'dist\tiktok-live-obs.dll'
$targetDll = Join-Path $ObsDirectory 'obs-plugins\64bit\tiktok-live-obs.dll'
$targetData = Join-Path $ObsDirectory 'data\obs-plugins\tiktok-live-obs'

if (-not (Test-Path -LiteralPath $sourceDll)) {
    throw "Build output was not found: $sourceDll"
}
if (-not (Test-Path -LiteralPath (Join-Path $ObsDirectory 'bin\64bit\obs64.exe'))) {
    throw "OBS Studio was not found: $ObsDirectory"
}
if (Get-Process obs64 -ErrorAction SilentlyContinue) {
    throw 'Close OBS Studio before installing the plugin.'
}

Copy-Item -LiteralPath $sourceDll -Destination $targetDll -Force
New-Item -ItemType Directory -Path $targetData -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'data\locale') -Destination $targetData -Recurse -Force

Write-Output "Installed: $targetDll"
