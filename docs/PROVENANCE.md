# Code provenance

TikTok Live OBS reuses selected general-purpose components from the GPL-3.0-only project [TikTok Live OBS Bridge](https://github.com/Smile-4-Me/tiktok-live-obs-bridge):

- OBS dock integration patterns
- Aitum output bridge
- Aitum output discovery
- installation-scoped settings paths
- Windows Credential Manager storage
- localization loader

No Streamlabs client, Streamlabs authentication logic, non-public TikTok protocol implementation, signing algorithm, device emulation, or code from `Loukious/TikTokStreamKeyGenerator` is included in this repository.

Because the reused components are GPL-3.0-only, this repository is also licensed under GPL-3.0-only.
