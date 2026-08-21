# Platform Support

Status: Windows v1.0.0 release candidate. This document is the authoritative list of
known platform dependencies. An item is marked complete only after it has been
built and tested on the relevant target platform.

| Component | Current implementation | Windows | macOS | Linux | Multi-OS path |
| --- | --- | --- | --- | --- | --- |
| OBS dock and plugin UI | Qt 6 / OBS frontend | tested | pending | pending | Build and test against the supported OBS version on all three systems. |
| Read Aitum outputs | Aitum obs-websocket vendor request | tested | pending | pending | The API is not Windows-specific; add Aitum integration tests on macOS and Linux. |
| Write Aitum outputs | Aitum Qt settings dialog | tested | pending | pending | No mouse or keyboard automation. Test widget and dialog discovery on the other platforms. |
| Streamlabs HTTPS | Pinned libcurl build owned by the plugin. Qt Network was evaluated and rejected because the tested OBS runtime did not provide a usable TLS backend to the plugin. | implemented with Schannel | pending | pending | Use Secure Transport or a supported equivalent on macOS, and a maintained TLS backend on Linux. Do not depend on OBS' bundled Qt TLS plugins or OBS' own libcurl binary. |
| Token storage | Windows Credential Manager | implemented | not implemented | not implemented | Add platform backends: macOS Keychain and Linux Secret Service/KWallet. Never store tokens in OBS or plugin configuration. |
| Build and distribution | Windows DLL | implemented | not implemented | not implemented | Add CMake presets, macOS signing/notarization, and a Linux packaging/dependency strategy. |

## Rules for New Features

- New Streamlabs and Aitum logic must remain platform-neutral; native calls are
  isolated behind a small interface.
- Document every Windows-specific shortcut here immediately.
- Do not claim macOS or Linux support until each platform has a native credential
  backend, a distributable HTTP transport, and an end-to-end test covering login,
  token storage, start/end LIVE, and the Aitum update.

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
