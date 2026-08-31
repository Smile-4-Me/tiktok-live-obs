# Platform Support

The v0.2.1 source tree is configured to package Windows, Ubuntu x86_64, and
macOS archives. Every release archive is built and tested by the same GitHub
Actions workflow.

| Area | Windows x64 | Ubuntu x86_64 | macOS |
| --- | --- | --- | --- |
| Build and automated tests | Supported | Supported | Supported |
| Plugin runtime in OBS | Supported | Supported | Supported |
| Secure secret storage | Windows Credential Manager | Secret Service / libsecret | Keychain |
| Streamlabs provider | Supported | Supported | Supported |
| RapidAPI provider | Supported | Supported | Supported |
| Aitum integration | Supported when Aitum is installed | Depends on Aitum availability | Depends on Aitum availability |

## Aitum availability

The plugin itself supports all three release platforms. Aitum Stream Suite is
optional; its own platform availability determines whether the automatic Aitum
handoff can be used. Without Aitum, the plugin displays the prepared stream
URL and key for a compatible streaming setup.

## Installation model

There is no installer in v0.2.1. Download the archive matching your operating
system, close OBS, and install its plugin files into the appropriate OBS plugin
location. The archive always includes the module, all locale catalogues, the UI
assets, and `LICENSE`.

Windows users can extract the ZIP at the OBS root so that `obs-plugins` and
`data` merge with the existing folders. For Linux and macOS, use the packaged
`tiktok-live-obs` directory with the plugin location appropriate for that OBS
installation. See [docs/INSTALLING.md](docs/INSTALLING.md) for details.

## Compatibility

- OBS Studio **31.0 or newer** is required.
- Aitum Stream Suite is optional. When it is present, a selected Aitum output
  can receive stream credentials and be started by the plugin.
- Manual mode works without Aitum and shows the generated URL/key for use in a
  compatible streaming setup.

## Security boundary

The public archive never contains user profiles, tokens, API keys, stream keys,
cookies, or local configuration. Those values are created locally after the
plugin is installed and are stored through the operating system's per-user
secret-storage facility.
