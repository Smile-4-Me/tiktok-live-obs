# Future Provider Contract

This document is the implementation gate for any provider added after
the experimental `tiktok-studio`, `Streamlabs`, `Manual`, or local-only
`research-local` provider. It is
written so a future contributor can decide whether a proposal belongs in this
project before touching the OBS UI or transport code.

## Non-negotiable entry requirements

A provider that contacts an external platform must have all of the following:

1. Written authorization from the platform, an authorized agency, or the
   credential owner for the intended integration.
2. Public API documentation or a written integration agreement covering the
   calls the provider will make.
3. A supported authentication model, including revocation and credential
   expiry behavior.
4. A documented session lifecycle: create, inspect, end, and failure
   recovery.
5. A privacy review describing every host contacted and every data field sent.
6. A reproducible test environment or an explicit test account process.

Missing any one of these requirements means the provider remains a proposal;
it must not be implemented as an inferred protocol.

## Provider responsibilities

Every provider must map its external behavior to the plugin's common profile
state without leaking transport details into the dock:

| Concern | Provider responsibility | Dock responsibility |
| --- | --- | --- |
| Authorization | Obtain, refresh, revoke, and securely store its credentials. | Display state and errors only. |
| Account state | Return a normalized account identifier and eligibility state. | Select the appropriate onboarding step. |
| Session start | Return a normalized stream server, stream key, and provider session identifier. | Apply credentials to Aitum when selected; reserve conflicts. |
| Session end | Confirm end or report an uncertain state. | Keep the profile/output reserved until confirmation. |
| Recovery | Reconcile sessions after an OBS restart. | Render `recovering`/`uncertain` status. |
| Diagnostics | Return concise, non-secret error detail. | Localize and show the diagnostic. |

## Required lifecycle state machine

The common profile state must remain valid for every provider:

```text
unconfigured
  -> authorized
  -> ready
  -> preparing
  -> active
  -> ending
  -> ready

preparing/active/ending
  -> uncertain (when external state cannot be verified)
  -> recovered or manually resolved
```

Invariants:

- one Aitum output can be reserved by only one preparing/active profile;
- one normalized TikTok account can be reserved by only one preparing/active
  profile, even if it appears under different profile names;
- a provider failure must release a reservation only after its external session
  is known to be absent;
- a local-only provider must never be shown as platform-confirmed LIVE.

## Security and observability requirements

- Credentials belong in the operating system's secure store, not profile INI
  files or logs.
- Diagnostics must never include an access token, stream key, cookie, or full
  authorization URL.
- All remote destinations must be listed in `docs/PRIVACY.md` before merging.
- Timeouts, retry limits, and cancellation must be explicit.
- External requests must have a provider-level integration test or a recorded
  test fixture from an authorized environment.

## Forbidden shortcuts

The following do not qualify as a provider implementation:

- reproducing a TikTok request- or frame-signing algorithm, embedding its secrets, or
  silently falling back to a local implementation when a hosted signer fails;
- pretending to be another encoder, client, or service;
- manufacturing locally signed metadata to satisfy an undocumented platform
  control;
- importing non-licensed upstream source code;
- shipping a provider whose only evidence is reverse-engineered traffic.

The RapidAPI clients are used for both LIVE Studio request signatures and the
optional media-output frame signatures. They consume opaque signed values
returned by the user's RapidAPI subscription and perform no signing locally.
The LIVE Studio provider does implement the separately public device-registration
envelope and Passport parameter formatting; those are documented, fixture-tested,
and are not a local fallback for request/frame signing. None of these distinctions
implies TikTok endorsement or authorization: distribution and use still need the
privacy, terms, provenance, and authorization review described above.

## Review checklist

Before merging a provider, reviewers must answer yes to each item:

- Is the authorization source documented?
- Can a user disconnect or revoke it?
- Are start/end/recovery results independently testable?
- Does it preserve output and account conflict protections?
- Are credentials and diagnostics safe?
- Is platform and license provenance documented?
- Does it introduce no behavior outside the documented authorization scope?
