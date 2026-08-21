# Installing TikTok Live OBS Bridge

## Supported installation types

The Windows installer supports both:

- a regular OBS Studio installation, commonly under `C:\Program Files\obs-studio`;
- a portable OBS installation, where `obs64.exe --portable` is used from a custom folder.

The installer always displays the OBS folder selection page. Choose the **root OBS folder**: it must contain `bin\64bit\obs64.exe`.

## What the installer changes

For the chosen OBS root, the installer adds only:

```text
obs-plugins\64bit\tiktok-live-obs-bridge.dll
data\obs-plugins\tiktok-live-obs-bridge\locale\de-DE.ini
data\obs-plugins\tiktok-live-obs-bridge\locale\en-US.ini
```

It does not edit OBS profiles, scene collections, Aitum configuration files, or other plugins.

Administrator rights are required because regular OBS installations are commonly located under `Program Files`. Portable OBS folders outside protected locations still use the same installer behavior for consistency.

## Multiple OBS installations

Each selected OBS folder receives its own Windows Apps entry and its own plugin configuration scope. Installing into a second portable OBS folder does not copy profiles or credentials from the first one.

Installing a newer version into the same selected OBS folder updates the plugin files and keeps that installation's configuration by default.

## Uninstalling

The uninstaller removes the plugin DLL and its locale files from the selected OBS installation. It asks whether to retain plugin configuration:

- **Keep checked:** profiles, account connections, and preferences remain available for a future installation at the same OBS path.
- **Keep unchecked:** scoped profile and preference files are removed and the matching Windows Credential Manager entries are deleted.

If the OBS folder or plugin files were manually deleted first, use the entry in **Windows Settings → Apps → Installed apps** to run the retained uninstaller. If that entry is also unavailable, reinstall to the same OBS path and then uninstall normally.

## Manual installation

Manual installation is intended for advanced users. Close OBS, then copy:

```text
tiktok-live-obs-bridge.dll
  → <OBS root>\obs-plugins\64bit\

data\locale\de-DE.ini
  → <OBS root>\data\obs-plugins\tiktok-live-obs-bridge\locale\de-DE.ini

data\locale\en-US.ini
  → <OBS root>\data\obs-plugins\tiktok-live-obs-bridge\locale\en-US.ini
```

Restart OBS and open **Docks → TikTok Live OBS Bridge**.
