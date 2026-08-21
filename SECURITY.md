# Security Policy

## Supported version

Security fixes are considered for the latest released version only. At the first public release, that version will be `1.0.x`.

## Reporting a vulnerability

Please do **not** open a public issue for a suspected vulnerability, token exposure, account-access issue, or a reproducible way to retrieve another user's credentials.

Until a dedicated private reporting channel is published, contact the repository owner privately through GitHub and include:

- a clear description of the impact;
- minimal reproduction steps;
- affected version and OBS version;
- whether any secrets were exposed.

Never send a real Streamlabs token, stream key, browser callback code, or credential export. Redact them before sharing logs or screenshots.

## Security model and limits

- Tokens and generated stream credentials are stored with the current Windows user in Windows Credential Manager.
- Profile names and non-secret preferences are stored in local INI files scoped to an OBS installation.
- Windows Credential Manager protects secrets from casual file access. It is not a defense against malware or an attacker already running code as the same Windows user.
- The plugin sends authenticated HTTPS requests to Streamlabs endpoints required for the selected feature. It does not provide a general-purpose proxy or remote-control service.
- The plugin is not an official TikTok, Streamlabs, Aitum, or OBS Studio security product. Use it at your own risk and keep Windows, OBS, and Streamlabs Desktop updated.
