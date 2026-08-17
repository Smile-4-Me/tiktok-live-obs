# TikTok Live OBS

Private development repository for a future official TikTok LIVE integration for OBS Studio.

## Status

This project is in private architecture and feasibility development. Its first compiled foundation is an OBS dock with profile management, secure manual credentials, and an optional Aitum output bridge. It does not implement or distribute non-public TikTok protocols, signature algorithms, or platform-integrity workarounds.

## Product direction

- Native OBS Studio dock and multi-profile workflow
- Optional Aitum Stream Suite output bridge
- Provider-neutral live-session architecture
- Manual credentials support as a safe baseline
- Future support for an officially authorized TikTok or agency provider

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the initial design.

## Current foundation

- Provider-neutral `LiveSessionProvider` contract
- `ManualCredentialsProvider` for credentials obtained through an authorized workflow
- Per-OBS-installation settings scope
- Windows Credential Manager storage for stream credentials
- Optional Aitum output discovery and update bridge

The first build is intentionally not a LIVE session creator. An official TikTok or authorized partner provider will be added only after access and terms are confirmed.
