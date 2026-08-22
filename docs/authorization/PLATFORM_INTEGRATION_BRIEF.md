# Platform Integration Brief

## Project

**TikTok Live OBS** is a private OBS Studio plugin project that helps creators
manage multiple streaming profiles and optionally pass provider-issued ingest
credentials to an Aitum Stream Suite output. It is not affiliated with TikTok,
OBS Studio, Aitum, or Streamlabs.

## Purpose of this brief

This is a request for guidance on an officially supported integration. The
project does not request access to undocumented endpoints, private request
formats, client impersonation, or media-stream modification. The goal is to
use a documented authorization and LIVE-session workflow that a platform is
willing to support.

## Creator workflow

1. The creator selects a local profile in OBS.
2. The creator authorizes the profile using the platform's approved method.
3. The plugin requests a LIVE session through the documented API.
4. The plugin receives the session's authorized ingest target and short-lived
   credential, if the API is designed to expose them.
5. The creator starts their chosen OBS/Aitum output.
6. The plugin observes documented session/output status and releases local
   reservations when the stream ends.

The plugin can also operate without Aitum by displaying platform-issued
credentials to the creator for manual use.

## Requested official capabilities

| Capability | Why it is needed | Minimum acceptable form |
| --- | --- | --- |
| Creator authorization | Connect a creator-owned account without storing a password. | OAuth or a documented token grant with revocation. |
| Account eligibility | Explain whether the account may create a PC/OBS LIVE session. | Read-only eligibility/status endpoint. |
| Create LIVE session | Prepare a stream before OBS starts sending video. | Idempotent create endpoint returning a session identifier and supported ingest configuration. |
| Read LIVE session status | Recover correctly after OBS or PC restart. | Endpoint or signed webhook exposing active/ended/failed state. |
| End LIVE session | Release a creator's session deliberately. | Idempotent end endpoint. |
| Metadata | Apply title/category/audience setting where a platform permits it. | Documented optional fields with an explicit supported-value list. |
| Rate limits and errors | Prevent accidental retries and give users clear feedback. | Published limits and structured error codes. |

## Security model

- The plugin stores user secrets in the operating system credential store.
- Stream keys and access tokens are not placed in the plugin's configuration
  files or user-facing diagnostics.
- Each local profile is scoped to its physical OBS installation.
- One platform account and one Aitum output can be reserved by only one active
  local profile at a time.
- A failed output start causes the plugin to call the documented cleanup path
  rather than leaving a stale session reserved.
- A platform-backed provider will be implemented only after written approval
  and documented requirements are available.

## Technical boundaries

- The project has no project-owned production backend today.
- The project will not implement undocumented signing, integrity behavior,
  encoder impersonation, or media-frame modification.

## Information requested from the platform

1. Is there a supported creator-facing API or partner program for this flow?
2. Which OAuth grant, scopes, and redirect URI rules apply to a desktop OBS
   plugin?
3. May an approved integration receive an ingest URL/key, and what are the
   rotation/expiry rules?
4. Is an Aitum/OBS output start observable through a supported API or webhook?
5. Which title, category, age/audience, thumbnail, and region settings are
   officially supported?
6. What encoder, codec, bitrate, keyframe, and protocol requirements should a
   creator be shown?
7. Is a direct desktop client permitted, or must an approved server backend
   broker requests?
8. What development, testing, review, branding, privacy, and support process
   applies?

## What we can provide for review

- source code of the provider boundary and local state machine;
- dependency and privacy inventory;
- automated local lifecycle tests;
- an architecture diagram;
- test accounts and reproducible OBS/Aitum steps where authorized; and
- a security review of local secret storage and logging.

## Contact-ready short version

> We are building a private OBS Studio integration for creators who manage
> TikTok LIVE alongside Aitum outputs. We would like to use an officially
> supported authorization and LIVE-session API. The project does not implement
> or request undocumented protocols, client impersonation, or media-stream
> modifications. Could you advise whether a creator/partner integration exists
> for account authorization, LIVE session lifecycle, supported metadata, and
> authorized ingest configuration?
