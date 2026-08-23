# Localization architecture

The plugin loads its text from independent catalogs:

- `data/locale/core/<locale>.ini` contains dock, profile, Aitum, and shared UI text.
- `data/locale/providers/<provider-id>/<locale>.ini` contains text owned by one provider.

`ProviderRegistry` defines the provider identifier. At startup the localization layer loads the core catalog, then the catalog of every registered provider. Every catalog loads its English file first and overlays the OBS UI language when a matching file is available. A missing language file or a missing string therefore falls back to English instead of exposing a raw translation key.

## Adding a provider

1. Register a stable provider identifier in `src/provider_registry.cpp`.
2. Add `data/locale/providers/<provider-id>/en-US.ini`.
3. Add the supported locale files in the same provider directory.
4. Keep provider-specific keys under a provider namespace, for example `Provider.Example.*`.

The provider catalog is optional at runtime. Removing a provider and its catalog does not affect the core catalog or any other provider.

## Compatibility

Older installations used a single flat `data/locale` directory. The loader still understands that layout as a fallback. New packages use the separated `core` and `providers` directories.
