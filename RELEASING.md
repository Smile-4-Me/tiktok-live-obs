# Release Checklist

This checklist is deliberately conservative. A release is ready only when every applicable item is checked against the exact commit that will be tagged.

## Source and legal

- [ ] `LICENSE` is present and unchanged from GNU GPL v3.0.
- [ ] `README.md`, `CHANGELOG.md`, `THIRD_PARTY_NOTICES.md`, `SECURITY.md`, and `docs/` match the release behavior.
- [ ] No real profile data, tokens, stream keys, callback codes, credentials, logs, crash reports, or private screenshots are staged.
- [ ] The release binary is built from the same Git commit published as source.
- [ ] The installer includes the GPL license page and ships only the plugin DLL and locale files.

## Build and installer

- [ ] Release build succeeds from a clean build directory.
- [ ] Installer compilation succeeds.
- [ ] Fresh installation works in a regular OBS installation.
- [ ] Fresh installation works in a portable OBS installation.
- [ ] Updating the same OBS installation preserves scoped configuration.
- [ ] Installing into a second OBS installation starts with independent configuration.
- [ ] Uninstalling with configuration retention checked preserves configuration.
- [ ] Uninstalling with configuration retention unchecked removes scoped configuration and credentials.
- [ ] OBS is closed or Restart Manager handles it before installation updates files.

## Functional smoke test

- [ ] Dock loads in German OBS and English OBS.
- [ ] Manual mode works without Aitum.
- [ ] Aitum output list refreshes and a selected output receives generated credentials.
- [ ] A new LIVE session can be created and ended.
- [ ] A failed Aitum output start triggers the session cleanup path.
- [ ] One-click output start shows a selection only when multiple eligible profiles are linked.
- [ ] The same TikTok account cannot be selected for two active outputs.
- [ ] Restart recovery does not falsely claim an unknown session is live.

## Publishing

- [ ] Private GitHub repository contains only intended source and documentation.
- [ ] A signed or clearly identified release artifact is attached to the matching GitHub tag.
- [ ] SHA-256 checksum is published for the installer.
- [ ] Release notes include supported platform, known boundaries, installation steps, and credit links.
- [ ] OBS Forum resource text has been reviewed against the exact release.
