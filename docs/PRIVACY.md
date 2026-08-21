# Data and Privacy

## Local-only design

TikTok Live OBS Bridge has no project-owned backend, telemetry service, analytics endpoint, update service, or account database. The plugin communicates directly with the Streamlabs services needed for the actions you request.

## What is stored locally

### Non-secret configuration

The plugin stores local profile metadata and preferences in the current Windows user's OBS configuration directory, normally:

```text
%LOCALAPPDATA%\obs64\
```

Files are scoped using a hash of the physical OBS installation path, for example:

```text
tiktok-live-obs-bridge-profiles-<installation-id>.ini
tiktok-live-obs-bridge-<installation-id>.ini
```

These INI files are plain text. They can include profile display names, TikTok usernames returned by Streamlabs, selected Aitum output names, titles, category selections, and session state. Treat them as private configuration and do not publish them.

### Secrets

The plugin stores the following in Windows Credential Manager for the current Windows user:

- Streamlabs account token;
- generated stream server URL and stream key while a LIVE session is active.

These values are **not** written to the plugin's INI files. Windows Credential Manager protects them from ordinary file browsing, but software running as the same Windows user may be able to request them. It cannot protect against malware or a compromised Windows account.

## Network activity

When requested through the dock, the plugin makes HTTPS requests to Streamlabs to:

- verify an account and load LIVE eligibility;
- search available game categories;
- create a LIVE session;
- end a LIVE session.

For browser login, it temporarily runs a local callback listener and opens the Streamlabs/TikTok login flow in the user's browser. The browser callback is local to the computer.

The plugin does not upload configuration or credentials to a server controlled by this project.

## Removing data

Use the plugin's profile deletion action to remove a profile and its credentials. During uninstallation, uncheck **Keep plugin configuration** to remove the selected OBS installation's scoped configuration and matching credential entries.

Deleting an OBS folder alone does not automatically remove Windows Credential Manager entries. Use the plugin uninstaller, or remove the relevant entries manually from Windows Credential Manager if needed.
