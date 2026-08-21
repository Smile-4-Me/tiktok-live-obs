# Building from Source on Windows

This is the current developer build path for the private Windows development build. It deliberately does not require a globally installed Qt runtime: the plugin targets the Qt headers and import libraries compatible with the OBS installation ABI.

## Requirements

- Windows 10 or 11 x64.
- Visual Studio 2022 Build Tools with the C++ desktop workload.
- CMake 3.26 or newer.
- Git.
- An OBS Studio 31.0-or-newer source checkout matching the ABI of the target OBS build, with its matching `obs-deps` bundle available under `.deps`. The bundle supplies the static `qrcodegencpp` library used for local QR rendering. In-process signing depends on the encoded-packet callback introduced in OBS 31.
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
2. Confirm the DLL and all 17 locale files exist.
3. Build the installer from those artifacts.
4. Configure with `TIKTOK_LIVE_OBS_BUILD_TESTS=ON`, build all test executables, and run CTest.
5. Run the research boundary check: `& .\tools\verify-research-boundary.ps1`.
6. Run the secret-storage check: `& .\tools\verify-secret-storage.ps1`.
7. Confirm that the request/frame signing clients contain no local algorithm or fallback and that [PRIVACY.md](PRIVACY.md) lists every remote field and host. The separately public device-registration fixture must still match.
8. Do not distribute a build containing an experimental provider without an authorized integration and a completed [Future Provider Contract](FUTURE_PROVIDER_CONTRACT.md) review.

## Run the native tests

The opt-in tests have no external network dependency. `local-research-harness`
starts the loopback-only Research Lab, confirms its first heartbeat, then
confirms clean shutdown. `media-signing-pipeline` parses recorded synthetic
RapidAPI response fixtures and validates H.264/HEVC insertion, cadence, cache
expiry, and preservation of the original encoded access unit.
`tiktok-studio-protocol` validates the public device-registration envelope and
fail-closed hosted request-signature response parsing. `credential-chunk-boundary`
guards the Windows 2,560-byte secret limit, and `secure-storage-roundtrip` writes,
reads, and removes a non-secret 2,048-byte Windows Credential Manager test entry.
`provider-registry-harness` verifies provider identity and credential ownership,
while `token-store-harness` checks isolated Token Store credential lifecycles on
Windows. Fixture values are intentionally fake; no request/frame signing
algorithm or live API key is present.

```powershell
cmake -S . -B build-tests -G "Visual Studio 17 2022" -A x64 `
  -DTIKTOK_LIVE_OBS_BUILD_TESTS=ON `
  -DOBS_SOURCE_DIR="C:\path\to\obs-studio" `
  -DQT_HEADERS_DIR="C:\path\to\qt\include" `
  -DQT_IMPORT_LIB_DIR="C:\path\to\qt\lib"
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
```

## Best-effort macOS and Linux source paths

Windows is the supported release target. The CMake project also selects macOS
Keychain/Secure Transport on Apple builds and libsecret/OpenSSL on Linux, uses
normal `Qt6::*` imported targets, and resolves OBS runtime symbols without a
Windows DLL dependency. These paths are intentionally described as best effort
until native OBS builds complete login, LIVE lifecycle, frame-signing, and
secret-store tests.

Provide `OBS_SOURCE_DIR`, Qt 6, CMake 3.26+, a C++20 compiler, and the platform
development dependencies. Linux additionally requires `pkg-config`,
`libsecret-1`, and OpenSSL development files. A normal configure shape is:

```sh
cmake -S . -B build -G Ninja \
  -DOBS_SOURCE_DIR=/path/to/obs-studio \
  -DTIKTOK_LIVE_OBS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

No macOS/Linux release package is produced by the Windows installer.
