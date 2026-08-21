# Building from Source on Windows

This is the current developer build path for the private Windows development build. It deliberately does not require a globally installed Qt runtime: the plugin targets the Qt headers and import libraries compatible with the OBS installation ABI.

## Requirements

- Windows 10 or 11 x64.
- Visual Studio 2022 Build Tools with the C++ desktop workload.
- CMake 3.26 or newer.
- Git.
- An OBS Studio source checkout matching the ABI of the target OBS build.
- Compatible Qt 6 headers and import libraries for `Qt6Core`, `Qt6Gui`, `Qt6Widgets`, and `Qt6Network`.
- Internet access during the initial configure step; CMake fetches the pinned curl revision declared in `CMakeLists.txt`.

## Configure

From the repository root, provide the three local SDK paths explicitly:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DOBS_SOURCE_DIR="C:\path\to\obs-studio" `
  -DQT_HEADERS_DIR="C:\path\to\qt\include" `
  -DQT_IMPORT_LIB_DIR="C:\path\to\qt\lib"
```

## Build

```powershell
cmake --build build --config Release --target tiktok-live-obs
```

The result is written to:

```text
dist\tiktok-live-obs.dll
dist\data\locale\de-DE.ini
dist\data\locale\en-US.ini
```

## Build the installer

Install Inno Setup 6, then run:

```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" installer\TikTokLiveObsBridge.iss
```

Adjust the executable path if Inno Setup is installed elsewhere. The output is written to `installer\Output\` and is intentionally ignored by Git.

## Before distributing a build

1. Rebuild from a clean build directory.
2. Confirm the DLL and both locale files exist.
3. Build the installer from those artifacts.
4. Run the research boundary check: `& .\tools\verify-research-boundary.ps1`.
5. Run the secret-storage check: `& .\tools\verify-secret-storage.ps1`.
6. Do not distribute a build containing an experimental provider without an authorized integration and a completed [Future Provider Contract](FUTURE_PROVIDER_CONTRACT.md) review.

## Run the local research smoke test

The smoke test is opt-in and has no external network dependency. It starts the
loopback-only Research Lab, confirms its first heartbeat, then confirms clean
shutdown:

```powershell
cmake -S . -B build-tests -G "Visual Studio 17 2022" -A x64 `
  -DTIKTOK_LIVE_OBS_BUILD_TESTS=ON `
  -DOBS_SOURCE_DIR="C:\path\to\obs-studio" `
  -DQT_HEADERS_DIR="C:\path\to\qt\include" `
  -DQT_IMPORT_LIB_DIR="C:\path\to\qt\lib"
cmake --build build-tests --config Release --target tiktok-live-obs-research-smoke
ctest --test-dir build-tests -C Release --output-on-failure
```
