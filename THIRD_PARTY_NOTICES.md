# Third-Party Notices

TikTok Live OBS is an independent project. Product names and trademarks belong to their respective owners.

## Direct project references

| Project | Relationship | License / terms |
| --- | --- | --- |
| [Loukious/StreamLabsTikTokStreamKeyGenerator](https://github.com/Loukious/StreamLabsTikTokStreamKeyGenerator) | GPL-3.0 reference project. Its observed behavior and architecture informed this implementation. | GPL-3.0; this project is distributed under GPL-3.0-only. |
| [Loukious/TikTokStreamKeyGenerator](https://github.com/Loukious/TikTokStreamKeyGenerator) | Public behavioral/source reference for LIVE Studio device registration, Passport request formatting, QR/session lifecycle, hosted signing contracts, and encoded metadata cadence. The public device-registration wire codec and Passport formatting are implemented here; request/frame signing algorithms are not. | No license file was present at the inspected revision; confirm redistribution permission before a public binary/source release. |
| [OBS Studio](https://github.com/obsproject/obs-studio) | Host application and frontend/module APIs. OBS Studio is not bundled with this project. | OBS Studio is GPL-2.0. Consult its repository for the current license and notices. |
| [Aitum Stream Suite](https://aitum.tv/) | Optional integration target. No Aitum code, binaries, or assets are bundled. | Subject to Aitum's own license and terms. |
| [curl](https://curl.se/) | Static HTTPS transport dependency built from the revision pinned in `CMakeLists.txt`. | curl license; copyright © Daniel Stenberg and contributors. |
| [QR Code generator](https://www.nayuki.io/page/qr-code-generator-library) | Statically linked from the OBS dependency bundle to render the TikTok login QR locally. | MIT License; copyright © Project Nayuki. |
| [Qt](https://www.qt.io/) | UI/runtime dependency supplied by the OBS installation used by the plugin. | Subject to Qt's applicable LGPL/GPL/commercial terms and the distribution used by OBS. |

## Additional notes

- This repository contains no TikTok, Streamlabs, or Aitum binaries/assets. The device-registration and Passport portions derived from the public TikTokStreamKeyGenerator reference are identified above; opaque request/frame signature generation remains hosted. OBS/Qt headers and the QR generator are supplied by the OBS dependency bundle at build time; curl is fetched from its pinned upstream revision.
- Links are included for attribution and transparency, not to imply endorsement or affiliation.
- When redistributing a build, retain this file, the project [LICENSE](LICENSE), and any notices required by the dependencies you distribute with that build.
