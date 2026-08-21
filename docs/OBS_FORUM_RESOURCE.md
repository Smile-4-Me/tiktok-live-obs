# OBS Forum Resource Draft

## Title

TikTok Live OBS Bridge — TikTok LIVE sessions for OBS and Aitum

## Short description

Create TikTok LIVE sessions from an OBS dock, optionally send the generated stream URL and key directly to an Aitum Stream Suite output, and manage multiple local TikTok profiles.

## Full description

TikTok Live OBS Bridge is a free, open-source OBS Studio plugin for Windows. It keeps the TikTok LIVE session workflow close to OBS: connect a Streamlabs TikTok account, select an Aitum output if you use Aitum, create the session, and start the output as usual.

It can also be used without Aitum. In manual mode, the dock shows the generated stream URL and key so you can use them in another compatible streaming setup.

### Features

- Multiple TikTok profiles in one OBS installation.
- Browser login, Streamlabs Desktop token import, or manual token verification.
- Account access status and a direct TikTok PC LIVE access link when approval is still needed.
- Optional stream title, game category, and 18+ request.
- Aitum output selection, automatic credential handoff, and one-click preparation when starting an Aitum output.
- Safeguards against two profiles using the same TikTok account or output at the same time.
- Manual mode without Aitum.
- German and English UI.

### Requirements

- Windows 10 or 11 x64.
- OBS Studio compatible with the current plugin release.
- Aitum Stream Suite is optional, not required.
- A Streamlabs/TikTok account that is eligible for PC LIVE.

### Installation

1. Close OBS Studio.
2. Run the installer as administrator.
3. Choose the root folder of the OBS installation you want to extend. Portable installations are supported.
4. Restart OBS Studio.
5. Open **Docks → TikTok Live OBS Bridge**.

### Notes and limitations

- This is an independent project and is not affiliated with TikTok, Streamlabs, Aitum, or OBS Studio.
- The used Streamlabs/TikTok flow is not a public TikTok API contract. It may change, be restricted, or stop working.
- Please follow the applicable platform rules and use the plugin at your own risk.
- v1.0.0 supports Windows only.

### Support and source

Source code, installation details, privacy information, and issue reporting guidance are available in the GitHub repository.

### Credits

Special thanks to Loukious and [StreamLabsTikTokStreamKeyGenerator](https://github.com/Loukious/StreamLabsTikTokStreamKeyGenerator), whose GPL-3.0 reference work informed this project. Thanks to the OBS Studio, Aitum, Qt, and curl communities as well.
