# Installing TikTok Live OBS

TikTok Live OBS v0.2.0 is distributed as platform archives. It does not modify
OBS profiles, scene collections, Aitum configuration, or any other plugin.

## Before you start

- Use OBS Studio **31.0 or newer**.
- Close OBS completely before replacing plugin files.
- Download the archive for your platform from the matching GitHub release.
- Optionally verify the archive with the supplied `SHA256SUMS` file.

## Windows x64

Extract `tiktok-live-obs-<version>-windows-x64.zip` at your OBS root—the folder
that contains `bin\64bit\obs64.exe`. Allow the archive's `obs-plugins` and
`data` folders to merge with the existing ones.

After extraction, the important paths are:

```text
<OBS root>\obs-plugins\64bit\tiktok-live-obs.dll
<OBS root>\data\obs-plugins\tiktok-live-obs\locale\
<OBS root>\data\obs-plugins\tiktok-live-obs\assets\
```

Start OBS and open **Docks → TikTok Live OBS**.

## Ubuntu x86_64 and macOS

Extract the matching `.tar.gz` archive. It contains a self-contained
`tiktok-live-obs` directory with `bin`, `data`, and `LICENSE`. Copy that
directory to the plugin location used by your OBS installation, preserving the
directory structure. See [Platform Support](../PLATFORM_SUPPORT.md) for the
exact Aitum integration availability on your platform.

## Updating

Replace the module and complete `data` directory with the newer archive. Do not
copy an archive's configuration because release archives intentionally contain
none. Your local non-secret profile choices and per-user secrets remain scoped
to the current OBS installation.

## Removing the plugin

Close OBS, then remove only the `tiktok-live-obs` module and its matching data
directory. Local configuration and secrets are not included in the plugin data
directory; remove them only if you explicitly want to clear your local profiles
and account connections.
