# Changelog

All notable changes are documented here. This project follows the spirit of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and uses semantic versioning where practical.

## [Unreleased]

## [0.1.2] - 2026-08-22

### Added

- Added a provider-neutral Aitum credential handoff. Streamlabs, TikTok LIVE
  Studio, and Manual now submit their ready stream URL/key pair through the
  same validation and Aitum update operation.
- Added a single shared Aitum output start-and-verify operation. Once a
  provider has updated an Aitum output, the same code starts that output and
  confirms that Aitum reports it as active.
- Added bounded, credential-free Aitum bridge diagnostics. The bridge records
  its progress and result without writing stream URLs, stream keys, tokens,
  cookies, or API keys to disk.
- Added specific Aitum bridge results for an unavailable editor, rejected
  credential fields, a missing save action, and a save-confirmation timeout.

### Changed

- TikTok LIVE Studio no longer inserts provider-specific preparation between
  the successful Aitum update and the shared Aitum output start. Its Aitum
  path is now identical to the Streamlabs and Manual paths after credentials
  have been obtained.
- A prepared TikTok LIVE room is now shown as pending rather than as an active
  broadcast until the selected Aitum output confirms that its encoder started.
- Recovered TikTok LIVE rooms are treated as unresolved sessions until they
  are explicitly resumed or ended. They continue to reserve the associated
  TikTok account and output to prevent conflicting sessions.

### Fixed

- Fixed an evaluation-order bug that could pass empty TikTok LIVE Studio RTMP
  credentials to the Aitum bridge while preserving those same credentials in
  the provider callback.
- Fixed Aitum editor detection after opening Output Settings. The bridge now
  recognizes the visible modal editor and its owned-child variant, and ignores
  stale hidden editors retained by Qt/Aitum.
- Fixed false Aitum save failures caused by unrelated retained dialogs after a
  successful editor save.
- Fixed profile-name and stream-detail UI rebuild crashes caused by widgets
  being deleted while Qt was still dispatching an editor signal.
- Fixed output selection persistence by storing the selected output's item
  data instead of relying on list indices. This also supports the first real
  Aitum output consistently.
- Fixed lifecycle cleanup so a session that fails before the Aitum encoder is
  active can still be ended and its output reservation released.

## [0.1.1] - 2026-08-22

### Removed

- Removed the discontinued localhost-only Research Lab provider from the provider selector, session lifecycle, profile recovery, build targets, tests, locale catalogs, and current documentation.
- Removed its private loopback heartbeat harness and the associated boundary-check script. The plugin no longer opens that Research Lab listener.

### Changed

- The provider registry now exposes exactly TikTok LIVE Studio, Streamlabs, and Manual. Manual remains the sole provider using locally supplied stream credentials.
- Existing profiles whose provider identifier is no longer known continue to use the existing safe fallback to Streamlabs when loaded.

### Added

- Optional in-process H.264/HEVC frame-signature metadata insertion using OBS 31's encoded-packet callback.
- Batched RapidAPI signature prefetch and background refresh with no local signing algorithm or FFmpeg proxy.
- Secure per-profile storage for the RapidAPI key and TikTok signing identifiers.
- Native fixture tests for RapidAPI response handling, SEI cadence, codec framing, and cache-expiry failure.
- Experimental TikTok LIVE Studio provider with RapidAPI-key-gated QR login, native public-protocol device registration, account eligibility checks, LIVE create/heartbeat/end, and restart recovery.
- Account-scoped LIVE Studio cookie/device storage with secure reuse and an in-dock delete-login action, structured for additional accounts.
- Fixed QR-login persistence by keeping Credential Manager entries below Windows' 2,560-byte blob limit; cookie saves now use 2,048-byte chunks, transactional A/B sets, and a native round-trip regression test.
- Added best-effort macOS Keychain and Linux Secret Service storage backends plus platform-neutral OBS symbol/path handling. Windows remains the supported release target.
- Locally rendered, client-secret-bound TikTok QR codes using the static QR generator already provided by the OBS dependency bundle.
- Protocol fixtures covering the public device-registration envelope and fail-closed RapidAPI request-signature parsing.
- Built-in LIVE Studio output that sends the signed stream directly from OBS while reusing its configured encoders, without replacing the main streaming service or running an FFmpeg relay.
- A root `VERSION` source of truth plus cross-platform GitHub Actions builds with validated release type, patch notes, and release artifact publishing for Windows, Ubuntu, and macOS.

### Security

- Signer requests are HTTPS-only, restricted to RapidAPI hostnames, non-redirecting, size-limited, cancellable, and kept off the encoder/UI threads.
- Signing-enabled outputs raise an OBS error and stop when signed values expire or encoded packets do not meet the supported H.264/HEVC Annex-B contract; there is no local fallback.
- LIVE Studio request and frame signatures remain RapidAPI-only. Only the publicly tracked device-registration/Passport formatting is implemented locally; cookies and keys remain in the platform's per-user secret store.

### Fixed

- LIVE Studio eligibility now follows the public reference app's truthiness and detailed restriction status, and the account view no longer displays `False` unconditionally.
- PC LIVE access guidance now directs users to apply through the official TikTok mobile app.
- The ready-state UI now keeps LIVE Studio account eligibility visible, hides its mandatory frame-signing switch, and omits idle session/diagnostic clutter.
- LIVE Studio game selection now searches TikTok's public game-name list and persists the corresponding internal tag ID.
- Saved LIVE Studio cookies now follow TikTok's rotating API hosts, and create-LIVE resolves and persists the anchor UID before in-process frame signing starts.
- In-process signing now uses OBS's exported packet ownership API instead of requiring the internal, non-exported packet-copy helper.
- LIVE creation now accepts intact JSON from TikTok responses that libcurl reports as partial and recovers an already-created room through the continuable-room endpoint before another create attempt.
- Linux builds now isolate Qt's keyword macros from libsecret headers, build static dependencies as PIC, and avoid importing QR libraries from a Windows OBS dependency bundle.
- The built-in TikTok output now reuses OBS's pre-created named stream encoders when the frontend streaming output has not been initialized yet.
- LIVE Studio now exposes the reference app's required Topic selector and requests a game only for the Gaming topic instead of hard-coding every LIVE as Gaming.
- LIVE Studio no longer exposes its internally managed RTMP URL/key.
- The built-in LIVE Studio output now rejects unsupported video codecs before creating a TikTok room; AV1 is not offered because TikTok did not accept its incoming video stream.
- LIVE Studio device/install IDs are now immutable for the lifetime of a saved account, while cookies returned by account, game-list, login, lifecycle, and heartbeat responses are persisted for reuse.
- Restart recovery now checks TikTok's continuable-room endpoint and offers an explicit Resume TikTok LIVE action that rebuilds the native OBS output and frame-signing callback without creating a second room.
- Continuation checks now treat TikTok's "This LIVE has ended" response as confirmation that no resumable room exists, allowing a fresh LIVE to be created instead of surfacing a recovery failure.

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
