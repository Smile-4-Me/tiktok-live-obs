# Releasing TikTok Live OBS

This is the short release checklist for maintainers. It applies to the exact
commit that will become a GitHub tag.

## 1. Prepare the source

- Set a semantic version in `VERSION` (`MAJOR.MINOR.PATCH`).
- Add a concise entry to `CHANGELOG.md` and a reader-friendly release note in
  `docs/RELEASE_<version>.md`.
- Confirm documentation reflects the shipped workflow and platform status.
- Check that no profile INI, token, cookie, stream key, API key, crash dump, or
  personal screenshot is staged.

## 2. Verify locally

- Configure a **clean** Release build with `TIKTOK_LIVE_OBS_BUILD_TESTS=ON`.
- Build the module and run all CTest tests.
- Run `tools/verify-reproducible-build.ps1` on Windows when distributing a new
  native DLL.
- Install only with `tools/install-local-obs.ps1`; it validates the full locale
  tree and checks that OBS really loads the copied DLL.

## 3. Publish the release

1. Push the verified commit to `main`.
2. In **Actions → Cross-platform build**, run the workflow with `draft`.
3. Wait for Windows, Ubuntu, and macOS builds and tests to succeed.
4. Verify that the draft release contains three platform archives and the
   generated `SHA256SUMS` file.
5. Replace the draft notes with `docs/RELEASE_<version>.md`, then publish it.

The workflow creates the `v<version>` tag from the commit it built. Do not create
the tag manually first.

## 4. After publishing

- Download one archive and inspect its layout: module, complete `data` tree,
  `LICENSE`, and no configuration files.
- Verify the checksum file against every release archive.
- Check the release page, changelog, platform-support statement, and download
  links once as a signed-out visitor.
