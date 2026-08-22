# Release 0.1.1 — Research Lab removal

## Purpose

Version 0.1.1 removes the discontinued **Research Lab (localhost)** provider.
It was a private development-only option; it did not create a TikTok LIVE or
modify media. The supported provider list is now:

1. TikTok LIVE Studio
2. Streamlabs
3. Manual

## What changed

- Removed `research-local` from `ProviderRegistry` and the account selector.
- Removed the `ResearchLab` QObject, its two-second localhost heartbeat, and
  all start, stop, error, and restart-recovery branches that called it.
- Removed the Research Lab smoke test and CMake target, the boundary-check
  PowerShell script, and all unused `Research.*`/`Provider.Research` locale
  strings.
- Updated architecture, privacy, build, integration, and research documents
  so they no longer describe an available Research Lab provider.

## Compatibility and migration

Manual, Streamlabs, and TikTok LIVE Studio do not depend on the removed code.
Manual remains the only local-credentials provider and retains its optional
frame-signing controls. The pre-existing profile loader protects against a
provider that disappears in a later build: a saved `research-local` profile is
treated as an unknown provider and is loaded as Streamlabs. It may then be
changed to Manual or configured again through the normal UI.

No token, cookie, stream key, or profile secret is moved, exported, or logged
by this release.

## Verification performed

- Searched source, tests, build configuration, locales, documentation, and
  tools for `research-local`, `Research Lab`, and `Research.*`: no active
  references remain.
- Ran `git diff --check`: no whitespace errors.
- Started a CMake test configuration. CMake identified Visual Studio 2022 and
  MSVC successfully, but this workstation lacks the Qt 6 development package,
  so the local build cannot proceed beyond `find_package(Qt6)`. GitHub Actions
  supplies Qt 6 and OBS headers for the release build and tests.

## Release artifact source

The `Cross-platform build` workflow builds and tests this exact commit on
Windows, Ubuntu, and macOS. When dispatched as a stable release it creates tag
`v0.1.1` and attaches the three platform archives to the GitHub release. The
artifacts are the release assets; no unverified local binary is substituted.
