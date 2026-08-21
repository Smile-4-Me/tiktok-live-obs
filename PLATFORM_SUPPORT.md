# Platform Support

Status: Windows v1.0.0 release candidate. This document is the authoritative list of
known platform dependencies. An item is marked complete only after it has been
built and tested on the relevant target platform.

| Component | Current implementation | Windows | macOS | Linux | Multi-OS path |
| --- | --- | --- | --- | --- | --- |
| OBS dock and plugin UI | Qt 6 / OBS frontend with platform-neutral runtime symbol lookup | tested | best-effort source path; untested | WSL Ubuntu 22.04 compile/link tested; native OBS load untested | Build and test against the supported OBS version on the target system. |
| Read Aitum outputs | Aitum obs-websocket vendor request | tested | pending | pending | The API is not Windows-specific; add Aitum integration tests on macOS and Linux. |
| Write Aitum outputs | Aitum Qt settings dialog | tested | pending | pending | No mouse or keyboard automation. Test widget and dialog discovery on the other platforms. |
| Streamlabs HTTPS | Version-pinned libcurl owned by the plugin | implemented/tested with Schannel | Secure Transport build path; untested | OpenSSL compile/link tested in WSL; network flow untested | Do not depend on OBS' Qt TLS plugins or OBS' private libcurl binary. |
| TikTok LIVE Studio provider | Native libcurl control plane, public device-registration codec, locally rendered QR, hosted RapidAPI request signatures, and five-second LIVE heartbeat | implemented; account validation in progress | best-effort source path; untested | WSL compile and protocol tests pass; login/LIVE runtime untested | Run an authorized end-to-end login/start/end test on each platform before promoting support. |
| In-process frame signing | RapidAPI batch client plus OBS 31 encoded-packet callback; OBS/frontend symbols use a platform-neutral resolver | implemented and native-tested | best-effort source path; untested | WSL compile and media-signing tests pass; OBS runtime untested | Add end-to-end encoded-output tests on macOS and Linux. |
| Token/account storage | Native secret-store interface with 2,048-byte cookie chunks and transactional A/B cookie sets | Windows Credential Manager round-trip tested | macOS Keychain backend implemented; untested | Secret Service/libsecret backend implemented; untested | Never silently fall back to plaintext profile JSON. |
| Build and distribution | Conditional CMake dependencies and Windows installer | implemented | no signed/notarized package | Ubuntu 22.04 WSL build and four local tests pass; no package | Add release packaging only after native verification. |

## Rules for New Features

- New Streamlabs and Aitum logic must remain platform-neutral; native calls are
  isolated behind a small interface.
- Document every Windows-specific shortcut here immediately.
- Treat macOS and Linux as best-effort source paths until each has a distributable
  build and an end-to-end test covering login, token storage, start/end LIVE, frame
  signing, and the Aitum update.

## HTTPS Decision (2026-08-14)

Qt Network is not the selected transport for this plugin. Although Qt supports
native TLS backends such as Schannel on Windows and Secure Transport on macOS,
those backends are loaded as Qt plugins and their availability is controlled by
the OBS installation. The tested OBS runtime did not provide a usable backend
to this plugin.

The selected release architecture is a thin `HttpTransport` interface and a
version-pinned libcurl distribution that is built and shipped with the plugin.
The platform builds will use a maintained TLS backend appropriate to the target
platform: Schannel on Windows, Secure Transport or a supported equivalent on
macOS, and a maintained TLS backend on Linux. Certificate verification remains
enabled in every build. This adds one consciously maintained dependency, but
removes dependency on OBS' internal Qt packaging and keeps the Streamlabs API
implementation identical across platforms.
