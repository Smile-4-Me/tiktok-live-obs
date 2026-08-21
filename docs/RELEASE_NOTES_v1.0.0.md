# TikTok Live OBS Bridge v1.0.0

## First release

TikTok Live OBS Bridge brings the TikTok LIVE session workflow into an OBS dock. It can manage multiple local profiles, prepare a LIVE session through Streamlabs, and optionally update an Aitum Stream Suite output with the generated stream URL and key.

### Highlights

- Multiple TikTok profiles in one OBS installation.
- Browser login, Streamlabs Desktop token import, or manual token verification.
- Optional Aitum output selection and automatic URL/key handoff.
- Manual mode for streaming workflows without Aitum.
- Protection against output and TikTok-account conflicts across profiles.
- Output-start verification and cautious session recovery after OBS closes unexpectedly.
- Localized dock UI: Arabic, Brazilian Portuguese, Chinese (Simplified and Traditional), English, French, German, Hindi, Indonesian, Italian, Japanese, Korean, Russian, Spanish, Thai, Turkish, and Vietnamese. Other OBS languages fall back to English.
- Windows installer with regular and portable OBS support.

### Requirements

- Windows 10 or 11 x64.
- OBS Studio compatible with the v1.0.0 build.
- Aitum Stream Suite is optional.
- A Streamlabs/TikTok account that is eligible for the features you want to use.

### Before you install

- Close OBS Studio.
- Run the installer as administrator.
- Select the correct root folder of the OBS installation you want to extend.
- Restart OBS when setup finishes, then open **Docks → TikTok Live OBS Bridge**.

### Important

This is an independent community project. It is not affiliated with or supported by TikTok, Streamlabs, Aitum, or OBS Studio. The Streamlabs/TikTok flow is not a public TikTok API contract and may change or stop working. Use the plugin only in accordance with the relevant service terms and at your own risk.

### Security and privacy

Tokens and generated stream credentials are stored in Windows Credential Manager. Profile preferences are stored locally per OBS installation. See [PRIVACY.md](PRIVACY.md) and [SECURITY.md](../SECURITY.md) for details.

### Thanks

Thank you to Loukious and the [StreamLabsTikTokStreamKeyGenerator](https://github.com/Loukious/StreamLabsTikTokStreamKeyGenerator) project for the GPL-3.0 reference work that helped make this project possible. Thank you as well to the OBS Studio, Aitum, Qt, and curl communities.
