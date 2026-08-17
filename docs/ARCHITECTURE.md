# Architecture

## Goal

TikTok Live OBS is a lightweight, provider-neutral OBS Studio plugin for managing creator profiles, preparing LIVE sessions, and optionally updating Aitum stream outputs.

The project is designed for an officially authorized TikTok LIVE integration. It must not rely on reverse-engineered platform protocols or imitate platform integrity controls.

## Core components

```text
OBS dock
  |- Profile and session UI
  |- Aitum output bridge
  |- Session conflict protection
  |- Recovery and user-facing status
  |
  +-- LiveSessionProvider
        |- ManualCredentialsProvider
        |- OfficialTikTokProvider (future)
        `- AuthorizedPartnerProvider (future)
```

## Security principles

- Keep credentials in the operating-system credential store where available.
- Never write access tokens, stream keys, or complete authorization headers to logs.
- Keep provider contracts minimal and versioned.
- Make all network communication explicit, authenticated, and observable.
- Bind any future local service to loopback only and require an authenticated local session.

## Intended provider contract

Providers return an authorized session descriptor only:

- creator identity and display name
- LIVE eligibility / authorization state
- session identifier and lifecycle state
- approved ingest URL and stream key, if the provider is authorized to issue them
- optional title, category, and audience settings

The OBS plugin owns profile storage, output mapping, conflict prevention, and Aitum updates. Providers do not control OBS UI state directly.
