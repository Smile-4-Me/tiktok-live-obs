# Data and Privacy

## Direct-service design

TikTok Live OBS has no project-owned backend, telemetry service, analytics endpoint, update service, or account database. Depending on the selected provider, the plugin communicates directly with TikTok/LIVE Studio endpoints, Streamlabs, and the configured RapidAPI signer.

## What is stored locally

### Non-secret configuration

The plugin stores local profile metadata and preferences in the current user's OBS configuration directory. On the supported Windows build this is normally:

```text
%LOCALAPPDATA%\obs64\
```

Files are scoped using a hash of the physical OBS installation path, for example:

```text
tiktok-live-obs-profiles-<installation-id>.ini
tiktok-live-obs-<installation-id>.ini
```

These INI files are plain text. They can include profile display names, TikTok usernames returned by a provider, opaque LIVE Studio account IDs, selected Aitum output names, titles, category selections, and session state. Treat them as private configuration and do not publish them.

### Secrets

The plugin stores the following in the operating system's per-user secret store. The supported Windows build uses Windows Credential Manager; the best-effort macOS and Linux source paths use macOS Keychain and Secret Service/libsecret respectively:

- Streamlabs account token;
- generated stream server URL and stream key while a LIVE session is active;
- RapidAPI key and signer URL; and
- TikTok UID, device ID, room-ID override, and application ID used for frame-signature requests;
- LIVE Studio device/install identifiers and account metadata; and
- the LIVE Studio cookie jar, split into 2,048-byte native-keyring entries when needed.

These values are **not** written to the plugin's INI files. The native secret store protects them from ordinary file browsing, but software running as the same signed-in user may be able to request them. It cannot protect against malware or a compromised operating-system account. The plugin does not silently downgrade to plaintext JSON when a native secret-store write fails.

For a saved LIVE Studio account, the device/install identity remains fixed until **Delete TikTok login** is used. TikTok response cookies are merged by the HTTP session and the refreshed cookie jar is written back to that same account after login, account, game-list, continuation, create, heartbeat, resume, and end requests.

## Network activity

When requested through the dock, the plugin makes HTTPS requests to Streamlabs to:

- verify an account and load LIVE eligibility;
- search available game categories;
- create a LIVE session;
- end a LIVE session.

The experimental **TikTok LIVE Studio** provider contacts these HTTPS destination groups:

- `tron-sg.bytelemon.com` to check the current LIVE Studio version;
- `log.tiktokv.com` to register a public-protocol desktop device;
- regional `api16-normal-*.tiktokv.com`, `*.tiktokv.eu`, or `*.tiktokv.us` hosts to request and poll a QR login and load account information; and
- regional `webcast*-normal-*.tiktokv.com`, `*.tiktokv.eu`, or `*.tiktokv.us` hosts to check LIVE eligibility and create, heartbeat, recover, or end a LIVE session.

Those requests can contain the registered device/install IDs, LIVE Studio/browser metadata, QR token, account cookies, title, optional game tag ID, age-restriction choice, and the active room/stream IDs. The QR image is generated locally; it is not sent to a QR-image service. Cookies are never written to logs or profile INI files.

LIVE Studio requests that require TikTok request signatures send the encoded query, request-body MD5 stub (not the plaintext body), device ID, application ID, and timestamp to the configured RapidAPI signer. The returned `X-Argus`, `X-Ladon`, and `X-Khronos` values are attached to the direct TikTok request. No local request-signing fallback exists.

When **Sign video frames inside OBS** is enabled, the plugin sends HTTPS POST requests to the configured `*.rapidapi.com` signer. Each batch request contains:

- application ID (`aid`), TikTok UID (`uid`), device ID (`did`), room ID (`roomid`), frame type, and current timestamp;
- batch start timestamp, duration, and step interval; and
- the RapidAPI key and host headers required by the subscription.

The plugin requests a five-minute signature window before output start and refreshes it before expiry while the signing session remains attached. It receives signed text values and keeps them in memory for the active output. It does **not** send video frames, audio, stream keys, stream URLs, titles, categories, thumbnails, or encoded packet contents to RapidAPI. Redirects are disabled so the RapidAPI key cannot be forwarded to another host, TLS verification remains enabled, and responses have a 16 MiB limit.

For Streamlabs browser login, the plugin temporarily runs a local callback listener and opens the Streamlabs/TikTok login flow in the user's browser. The LIVE Studio provider instead displays a locally generated QR image inside OBS and polls TikTok directly.

The plugin does not upload configuration or credentials to a server controlled by this project. RapidAPI and the signer publisher process the request fields above under their own terms and privacy policies; subscribing to or using that service is the user's choice.


## Removing data

Use **Delete TikTok login** in the account panel to remove that account's cookies, device registration, RapidAPI key, and signing credentials. Deleting a profile also removes its credentials (and its account credential set when no other profile references it). During uninstallation, uncheck **Keep plugin configuration** to remove the selected OBS installation's scoped configuration and matching credential entries.

Deleting an OBS folder alone does not automatically remove Windows Credential Manager entries. Remove the relevant entries manually from Windows Credential Manager if you want to clear those credentials.
