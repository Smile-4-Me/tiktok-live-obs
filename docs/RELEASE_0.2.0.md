# TikTok Live OBS v0.2.0

Version 0.2.0 turns the project into a clearer provider-based OBS workflow:
choose how a profile receives its credentials, then use the same reliable Aitum
handoff and session safeguards for every provider.

## Highlights

- **Three provider options:** RapidAPI, Streamlabs, and Manual credentials.
- **One shared streaming path:** provider credentials are handled by the same
  Aitum update, output start verification, and cleanup logic.
- **Multi-profile safeguards:** an Aitum output and a TikTok identity cannot be
  used by conflicting LIVE sessions at the same time.
- **RapidAPI workflow:** QR sign-in, account refresh, LIVE-access guidance,
  topic/game metadata, and secure per-profile credential storage.
- **More reliable recovery:** failed output starts, missing account access, and
  reset manual credentials clear stale output links automatically.
- **Local language support:** core and provider catalogues load independently,
  with English as the safe fallback.

## Download and install

Release assets are available for Windows x64, Ubuntu x86_64, and macOS. Each
archive includes the module, every locale catalogue, UI assets, and `LICENSE`.
Use the archive that matches your system and follow the concise instructions in
[Installing](INSTALLING.md). The `SHA256SUMS` asset lets you verify every
download before extracting it.

All three platform archives are built and tested in the release workflow. See
[Platform Support](../PLATFORM_SUPPORT.md) for Aitum integration availability
on each platform.

## Requirements and boundaries

- OBS Studio 31.0 or newer.
- Aitum Stream Suite is optional. Select an Aitum output to have the plugin
  insert the provider-issued URL/key and start that output. Without Aitum, use
  the displayed credentials in a compatible setup.
- RapidAPI requires a user-owned RapidAPI key and TikTok's QR sign-in flow.
- Manual mode accepts credentials obtained through an authorized source.

## Privacy

This release contains no user data. Profile preferences stay local to each OBS
installation. Tokens, stream keys, RapidAPI keys, cookies, and device IDs are
kept in the operating system's per-user secret store and are not written to the
repository, archive, or diagnostic messages.

For every change in this release, see [CHANGELOG.md](../CHANGELOG.md).
