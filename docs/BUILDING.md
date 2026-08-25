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
data\locale\
```

## Local OBS update preflight

Never copy a newly built DLL into OBS by hand. Use the checked-in deployment
script so every local update follows the same preflight checklist:

```powershell
.\tools\install-local-obs.ps1 `
  -ObsRoot "C:\path\to\your\obs-installation" `
  -Configuration RelWithDebInfo `
  -RestartObs
```

Before it changes an OBS installation, the script verifies all of the following:

1. The selected directory contains `bin\64bit\obs64.exe`.
2. OBS itself contains `data\obs-studio\locale\en-US.ini`.
3. The newly built plugin DLL exists.
4. The core catalogue and every bundled provider catalogue contain `en-US.ini`.
5. The copied DLL and catalogue layout exist in the target directory.

When the selected OBS instance is open and `-RestartObs` is supplied, the script
requests a normal OBS shutdown and waits for it. It intentionally never force-kills
OBS: a forced shutdown opens OBS' crash-recovery dialog on the next start, which
prevents any third-party plugin from loading until the user chooses a mode.

When `-RestartObs` is used, OBS is launched from `bin\64bit` as its working
directory. This is essential for portable OBS installations: launching `obs64.exe`
from an unrelated working directory can make OBS fail before loading plugins with
`Failed to find locale/en-US.ini`.

## Windows Smart App Control / Code Integrity troubleshooting

On some Windows installations, Smart App Control or an enterprise Code Integrity
policy can reject a newly compiled native plugin DLL even when the source and OBS
installation are otherwise valid. This is **not** a simple “unsigned means blocked”
rule: Smart App Control can permit one unsigned local build and reject another.
The decision is made for the exact DLL identity, including its hash and cloud
reputation.

To avoid creating a needless new identity from unchanged source, the Windows target
uses MSVC's `/Brepro` linker option and disables incremental linking. This removes
the ordinary PE build timestamp and prior-link state from the normal local build
path. It reduces avoidable binary churn; it does not promise that Windows will
trust a genuinely changed DLL.

Before a release, run the checked-in reproducibility check. It builds twice in the
build directory but never copies anything to OBS:

```powershell
.\tools\verify-reproducible-build.ps1 -BuildDirectory build -Configuration RelWithDebInfo
```

Typical OBS log evidence is one of the following:

```text
LoadLibrary failed for '../../obs-plugins/64bit/tiktok-live-obs.dll' ... (4551)
Module '../../obs-plugins/64bit/tiktok-live-obs.dll' not loaded
```

Windows Event Viewer can additionally show Code Integrity events 3033 or 3077
for `tiktok-live-obs.dll`. Error 4551 means a Windows application-control policy
blocked the file; it is not an OBS locale, provider, or plugin-logic failure.

### Local recovery procedure

1. Build the current source cleanly. Do not restore an older DLL as a workaround.
2. Deploy the fresh DLL and the complete `data\locale` tree to the specific OBS
   instance with `install-local-obs.ps1 -RestartObs`.
3. The deployment script now queries the freshly started OBS process and only
   reports success after `tiktok-live-obs.dll` is present in its loaded-module
   list. OBS buffers its text log during startup, so the process check is more
   reliable than waiting for log output. A successful file copy alone is never
   treated as a successful installation.
4. If Windows prevents the load, the script reports the exact OBS log file instead
   of claiming that the plugin was installed successfully.
5. Never delete user profile/configuration data as part of this procedure.

The local deployment script deliberately stops before it copies an unsigned DLL
when Smart App Control enforcement is enabled. Copying a file, elevating a shell,
or accepting UAC cannot make that DLL trusted, so the script no longer provides a
misleading bypass switch. For an unsigned local-development build, Smart App
Control must first be disabled manually by the developer. A successful deployment
then proves all three required facts: the policy state is off, the full locale
layout was copied, and the freshly started OBS process loaded the DLL.

For distributable releases, a trusted code-signing path is the stable long-term
trust route. It is not the explanation for a build-to-build flip: the immediate
cause is hash-specific application-control evaluation.

## GitHub Actions builds and releases

The root `VERSION` file is the single source of truth for CMake, the Windows
CI artifact names, tags, and release titles. Update and commit that
file before releasing. Then open **Actions → Cross-platform build → Run workflow**
to choose the release type and optional patch notes before any platform build
starts. `build-only` uploads CI artifacts without creating a tag. `draft`,
`prerelease`, and `stable` wait for Windows, Ubuntu, and macOS to pass, create the
corresponding `v<version>` tag and GitHub release, and attach all three platform
archives. When patch notes are empty, GitHub generates notes from the commits.

## Before distributing a build

1. Rebuild from a clean build directory.
2. Confirm the DLL and all 17 locale files exist.
3. Configure with `TIKTOK_LIVE_OBS_BUILD_TESTS=ON`, build all test executables, and run CTest.
4. Confirm that the request/frame signing clients contain no local algorithm or fallback and that [PRIVACY.md](PRIVACY.md) lists every remote field and host. The separately public device-registration fixture must still match.
5. Check the staged file list for local profiles, tokens, cookies, stream keys, API keys, crash dumps, and personal screenshots.

## Run the native tests

The opt-in tests have no external network dependency. `media-signing-pipeline` parses recorded synthetic
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

## macOS and Linux source paths

The CMake project selects macOS Keychain/Secure Transport on Apple builds and
libsecret/OpenSSL on Linux, uses normal `Qt6::*` imported targets, and resolves
OBS runtime symbols without a Windows DLL dependency. GitHub Actions builds and
tests the macOS and Ubuntu archives for every release.

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

All public platform archives are produced by the GitHub Actions release
workflow. See [Platform Support](../PLATFORM_SUPPORT.md) for their runtime
status.
