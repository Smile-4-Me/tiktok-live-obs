# TikTok Live OBS v0.2.1

Version 0.2.1 adds the current TikTok LIVE Studio provider improvements while
keeping the plugin-native OBS and Aitum architecture intact. It is a source
and documentation update; it introduces no helper executable, local RTMP
server, FFmpeg relay, or project-operated backend.

## Highlights

- **Dual Layout via Aitum:** eligible RapidAPI/TikTok LIVE Studio profiles can
  create a portrait-and-landscape LIVE and send each canvas through a selected
  Aitum output. The plugin starts and verifies both outputs directly.
- **Clearer eligibility:** a locked Dual Layout option explains the reported
  number of remaining TikTok LIVE Studio days and refreshes after account and
  LIVE lifecycle operations.
- **RapidAPI usage at a glance:** the account's existing API response headers
  update a green/yellow/red remaining-quota meter in account and stream setup.
  Showing the meter does not make an additional RapidAPI request.
- **Reliable profile layout:** the dock always keeps at least three profiles
  available. It expands the list only when the complete configuration area is
  already visible, then uses the remaining room for further rows.
- **Cleaner provider flow:** Manual profiles use one credentials step;
  unsuccessful access checks and ended/reset sessions clear obsolete Aitum
  output links before returning to setup.

## Browser-session import status

The source includes a consented browser-session import boundary, but no
browser-specific collector is shipped. The intentionally empty adapter accepts
only a caller-supplied Netscape-format TikTok cookie jar; the existing client
then validates it, registers a device if needed, and persists the successful
account session in the operating-system secret store.

This release does **not** implement a TikTok browser redirect login. The
tested redirect flow was rejected by TikTok with "Incorrect parameters" before
the plugin received a reusable session. A future official TikTok OAuth client
registration could use an in-plugin loopback callback listener without a
separate executable, but such a server-side client/redirect authorization is
not part of this release.

## Boundaries retained in v0.2.1

- No local TikTok request- or frame-signing algorithm.
- No FFmpeg process, local RTMP listener, remux layer, or additional helper
  executable. OBS packet callbacks and Aitum outputs remain the media path.
- No browser-cookie collection, browser database decryption, or plaintext
  cookie persistence.
- No project-operated backend, telemetry, or account database.

## Install and verify

Follow [Installing](INSTALLING.md). A release archive contains the module,
complete locale tree, assets, and license only; it contains no profile,
cookie, RapidAPI key, stream URL, or stream key.

For detailed behavior and migration notes, see [CHANGELOG.md](../CHANGELOG.md)
and [Data and Privacy](PRIVACY.md).
