# Changelog

All notable changes are documented here. This project follows the spirit of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and uses semantic versioning where practical.

## [1.0.0] - 2026-08-16

### Added

- Multi-profile TikTok session management in an OBS dock.
- Streamlabs browser login, Streamlabs Desktop token import, and manual token verification.
- Account access status, stream title, game category, and 18+ LIVE request controls.
- Optional Aitum Stream Suite output discovery, URL/key update, one-click session preparation, and output-start verification.
- Manual mode for users who do not use Aitum.
- Output and TikTok-account reservation rules to prevent conflicting concurrent sessions.
- Session reconciliation after OBS closes during a LIVE session.
- OBS language detection with localized dock UI for Arabic, Brazilian Portuguese, Chinese (Simplified and Traditional), English, French, German, Hindi, Indonesian, Italian, Japanese, Korean, Russian, Spanish, Thai, Turkish, and Vietnamese. English is the fallback for other OBS languages.
- Windows installer with selectable OBS root folder, portable OBS support, instance-specific uninstall entries, configuration retention option, and license page.
- Per-OBS-installation configuration scoping and migration from earlier local configuration names.

### Security

- Streamlabs tokens and generated stream credentials are stored in Windows Credential Manager.
- Repository ignores build outputs, local configuration, and common secret-file patterns.
