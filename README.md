# TikTok Live OBS

> A small, independent OBS Studio plugin for preparing TikTok LIVE sessions and optionally handing their generated stream URL and key to an Aitum Stream Suite output.

![Windows](https://img.shields.io/badge/platform-Windows%2010%20%2F%2011-0078D4?logo=windows&logoColor=white)
![License](https://img.shields.io/badge/license-GPL--3.0--only-3DA639)
![Status](https://img.shields.io/badge/status-private%20development-5865F2)

TikTok Live OBS is a provider-based evolution of TikTok Live OBS Bridge. It keeps the proven OBS and Aitum workflow while allowing a profile to choose how its credentials are supplied: Streamlabs today, manual credentials from an authorized source now, and further providers later.

It is a community project, not a company product. Please read the information below before using it.

## How it works

<table width="100%">
  <tr>
    <td width="50%" valign="top" align="center">
      <strong>1. Connect your TikTok account</strong><br><br>
      <img src="docs/assets/how-it-works-01-connect-account.png" alt="Connect a TikTok account through Streamlabs" width="400">
    </td>
    <td width="50%" valign="top" align="center">
      <strong>2. Get your TikTok LIVE access</strong><br><br>
      <img src="docs/assets/how-it-works-02-pc-live-access.png" alt="Apply for TikTok PC LIVE access" width="400">
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top" align="center">
      <strong>3. Go live!</strong><br><br>
      <img src="docs/assets/how-it-works-03-go-live.png" alt="Create a TikTok LIVE session and start streaming" width="400">
    </td>
    <td width="50%" valign="top" align="center">
      <strong>4. Stream with as many TikTok accounts as you like</strong><br><br>
      <img src="docs/assets/how-it-works-04-multiple-accounts.png" alt="Manage multiple TikTok accounts in one OBS installation" width="400">
    </td>
  </tr>
</table>

## What it does

✓ Keeps multiple local TikTok profile configurations in one OBS installation.

✓ Lets every profile choose a credentials provider. Streamlabs retains the full tested workflow; **Manual** accepts a TikTok username, stream URL, and stream key supplied through an authorized source.

✓ Shows whether an account appears ready for PC LIVE access.

✓ Creates and ends TikTok LIVE sessions, including stream title, game category, and 18+ request.

✓ Can update a selected Aitum Stream Suite output with the generated stream URL and key.

✓ Can also be used without Aitum: copy the generated credentials into another streaming workflow.

✓ Prevents two profiles from reserving the same Aitum output or TikTok account at the same time.

✓ Checks whether Aitum actually started an output and cleans up a newly created session when it did not.

✓ Recovers cautiously after OBS is closed during a LIVE session.

✓ Uses OBS' selected language and includes 17 localized dock interfaces: Arabic, Brazilian Portuguese, Chinese (Simplified and Traditional), English, French, German, Hindi, Indonesian, Italian, Japanese, Korean, Russian, Spanish, Thai, Turkish, and Vietnamese.

## Important boundaries

- **Windows only for v1.0.0.** macOS and Linux are not supported yet; see [PLATFORM_SUPPORT.md](PLATFORM_SUPPORT.md).
- **Aitum is optional.** The plugin works in manual mode without it.
- **Nothing here is affiliated with, endorsed by, or supported by TikTok, Streamlabs, Aitum, or OBS Studio.**
- The Streamlabs/TikTok flow used by this plugin is not a public TikTok API contract. Providers may change, restrict, or remove it at any time.
- You are responsible for your account, stream content, and compliance with the terms and rules of every service you use. This project does not promise account eligibility, uninterrupted streaming, or any particular platform outcome.

## Development status

This private repository is under active development. A dedicated installer and public release will follow once the provider architecture is complete.

The private, localhost-only research work is documented in
[docs/research/loukious-tiktok-stream-key-generator-analysis.md](docs/research/loukious-tiktok-stream-key-generator-analysis.md).
It records the external architecture research, the RapidAPI assessment, and
the explicit boundary that this project does not implement platform-specific
signing or integrity-bypass behavior.

Any future external provider must pass the
[Future Provider Contract](docs/FUTURE_PROVIDER_CONTRACT.md) before it is
implemented.

The distinction between RapidAPI, an official direct integration, and a
project-owned authorized backend is documented in
[Authorized Backend Options](docs/research/AUTHORIZED_BACKEND_OPTIONS.md).

## Quick start

1. Open the dock and create or select a profile.
2. On the first page, choose **Streamlabs**, **Manual**, or the private **Research Lab (localhost)** provider.
3. For Streamlabs, connect the TikTok account as before and complete PC LIVE approval if necessary. For Manual, enter credentials supplied through an authorized source. Research Lab is a local-only development tool and does not contact TikTok or alter media.
4. Choose an Aitum output, or leave the output in **Manual usage** mode.
5. Add optional stream metadata. Streamlabs creates the LIVE session; Manual applies the supplied credentials; Research Lab starts only its local heartbeat mock.
6. With Aitum, start the selected output as usual. Without Aitum, copy the configured credentials into your streaming software.
7. End the LIVE session or local manual reservation in the dock when the stream is over.

## Data and privacy

Profile names and non-secret preferences are saved per OBS installation under the current Windows user's local OBS configuration directory. Streamlabs tokens and generated stream credentials are stored in **Windows Credential Manager**, not in the plugin's INI files.

See [docs/PRIVACY.md](docs/PRIVACY.md) for the exact storage model, its limits, and how uninstalling handles configuration.

## Building from source

The source is included so the plugin can be inspected, improved, and built independently. The current build requires Windows, CMake, Visual Studio Build Tools, compatible Qt 6 headers/import libraries, and an OBS source tree matching the target ABI.

Detailed, reproducible instructions are in [docs/BUILDING.md](docs/BUILDING.md).

## Credits and respect

This project exists because other projects made the problem understandable:

- [Loukious/StreamLabsTikTokStreamKeyGenerator](https://github.com/Loukious/StreamLabsTikTokStreamKeyGenerator) — the GPL-3.0 reference project whose observed flow and ideas informed this implementation. Thank you, Loukious.
- [OBS Studio](https://obsproject.com/) — the broadcasting platform this plugin extends.
- [Aitum Stream Suite](https://aitum.tv/) — optional output management integration. This project does not bundle, modify, or represent Aitum.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for licensing and attribution details.

## Contributing and support

Bug reports and improvements are welcome. Please start with [CONTRIBUTING.md](CONTRIBUTING.md), and do not include tokens, stream keys, account identifiers, or crash dumps containing personal data in public issues.

Security-sensitive reports belong in [SECURITY.md](SECURITY.md), not in a public issue.

### Support the project

If TikTok Live OBS Bridge helps your stream and you would like to support its maintenance, you can leave a small tip on Ko-fi. It is completely optional, but always appreciated.

<p align="center">
  <a href="https://ko-fi.com/smile_4_meee">
    <img src="https://media.giphy.com/media/K7gPh3p71iAK8NwkhO/giphy.gif" alt="Thanks for your support" width="160">
  </a>
</p>

## License

Copyright © 2026 TikTok Live OBS Bridge Contributors.

This project is licensed under the [GNU General Public License v3.0 only](LICENSE). It is provided **without warranty**; see the license for the full terms.
