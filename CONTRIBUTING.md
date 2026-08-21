# Contributing

Thank you for considering a contribution. Small, focused changes are easier to review and safer for a plugin that can create live sessions.

## Before opening an issue

- Search existing issues first.
- Use the latest release or current `main` build.
- Remove or redact tokens, stream keys, usernames, callback codes, and personal file paths.
- Include OBS version, Aitum version when relevant, plugin version, and exact reproduction steps.

## Development principles

- Keep new code platform-neutral unless a native boundary is unavoidable.
- Put Windows-specific code behind a narrow interface and update [PLATFORM_SUPPORT.md](PLATFORM_SUPPORT.md).
- Never write tokens or generated stream credentials to logs, settings files, test fixtures, or screenshots.
- Do not use mouse or keyboard automation to control Aitum.
- Preserve the one-account/one-active-session and one-output/one-active-session safety rules.
- Keep UI strings in `data/locale/en-US.ini` and `data/locale/de-DE.ini`; English is the source language.
- Use English for source, comments, commit messages, documentation, and pull requests.

## Pull requests

1. Fork the repository and create a focused branch.
2. Build the Release configuration locally.
3. Test the affected workflow without exposing real secrets.
4. Update user-facing documentation and the changelog when behavior changes.
5. Explain the user impact, test evidence, and compatibility impact in the pull request.

Large refactors should be discussed in an issue first. The project deliberately favors clear boundaries and small source files over clever abstractions.
