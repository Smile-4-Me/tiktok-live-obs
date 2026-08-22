# Release 0.1.2 — Unified Aitum provider flow

## Purpose

Version 0.1.2 makes the Aitum integration deterministic across all current
credential providers. After a provider has produced an RTMP server and stream
key, **TikTok LIVE Studio, Streamlabs, and Manual use the same Aitum handoff,
output-start, and output-verification path**.

## What changed

- Added `update_aitum_output_for_profile`, the provider-neutral boundary for
  validating credentials and updating a selected Aitum output.
- Added `start_aitum_output_and_verify`, the single operation that starts the
  selected Aitum output and waits for Aitum to report an active encoder.
- Routed Streamlabs, TikTok LIVE Studio, and Manual through both operations.
  Providers still differ only in how they obtain or end their own credentials
  and remote LIVE sessions.
- Removed the TikTok LIVE Studio-specific Aitum preparation step that used to
  occur after a successful credential update and before output start.
- Hardened Aitum editor discovery, field selection, save handling, and failure
  reporting. The bridge now recognizes visible and owned modal editors, ignores
  stale hidden dialogs, and reports the precise failed stage.
- Added non-secret diagnostic traces for the Aitum handoff and bridge. The
  traces contain only timestamps, output names, state transitions, and whether
  credentials were present; they never include credential values.
- Made the UI lifecycle safer: profile rows are rebuilt asynchronously after
  edits, signal delivery is stopped before obsolete widgets are destroyed, and
  field handlers retain the profile ID they belong to.
- Improved session accuracy. Creating a remote LIVE room is no longer treated
  as proof that OBS is broadcasting. A profile becomes live only after Aitum
  confirms its output, while unresolved recovered sessions remain reserved
  until resumed or ended.

## Compatibility

No profile migration is required. Existing Streamlabs, TikTok LIVE Studio, and
Manual profiles retain their selected Aitum output and saved provider data.
The patch does not change the supported provider list, secret-store format, or
the public dock workflow.

## Verification performed

- Built the Release configuration with the supported Windows toolchain.
- Ran the six native smoke/regression tests successfully.
- Verified a TikTok LIVE Studio/RapidAPI profile locally: the provider returned
  an RTMP server/key pair, the shared Aitum bridge stored the pair, and Aitum
  started through the same shared output path used by Streamlabs.
- Ran `git diff --check` before publication.

## Security and privacy

The new diagnostics intentionally omit stream URLs, stream keys, cookies,
tokens, API keys, and other secret values. Existing operating-system secret
storage remains unchanged.
