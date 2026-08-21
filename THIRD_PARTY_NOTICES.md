# Third-Party Notices

TikTok Live OBS Bridge is an independent project. Product names and trademarks belong to their respective owners.

## Direct project references

| Project | Relationship | License / terms |
| --- | --- | --- |
| [Loukious/StreamLabsTikTokStreamKeyGenerator](https://github.com/Loukious/StreamLabsTikTokStreamKeyGenerator) | GPL-3.0 reference project. Its observed behavior and architecture informed this implementation. | GPL-3.0; this project is distributed under GPL-3.0-only. |
| [OBS Studio](https://github.com/obsproject/obs-studio) | Host application and frontend/module APIs. OBS Studio is not bundled with this project. | OBS Studio is GPL-2.0. Consult its repository for the current license and notices. |
| [Aitum Stream Suite](https://aitum.tv/) | Optional integration target. No Aitum code, binaries, or assets are bundled. | Subject to Aitum's own license and terms. |
| [curl](https://curl.se/) | Static HTTPS transport dependency built from the revision pinned in `CMakeLists.txt`. | curl license; copyright © Daniel Stenberg and contributors. |
| [Qt](https://www.qt.io/) | UI/runtime dependency supplied by the OBS installation used by the plugin. | Subject to Qt's applicable LGPL/GPL/commercial terms and the distribution used by OBS. |

## Additional notes

- This repository contains no TikTok, Streamlabs, OBS Studio, Aitum, curl, or Qt source code beyond the plugin's own code and build metadata.
- Links are included for attribution and transparency, not to imply endorsement or affiliation.
- When redistributing a build, retain this file, the project [LICENSE](LICENSE), and any notices required by the dependencies you distribute with that build.
