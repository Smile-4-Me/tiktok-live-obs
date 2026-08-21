# Streamlabs TikTok Protocol Notes

Status: observed implementation details, not a public TikTok API contract.

## Stream Creation Request

The original GPL-3.0 reference project sends a multipart `POST` request to:

`https://streamlabs.com/api/v5/slobs/tiktok/stream/start`

The observed fields are:

| Field | Current value | Meaning confirmed by observation |
| --- | --- | --- |
| `title` | User-entered stream title | TikTok LIVE title. |
| `device_platform` | `win32` | Source platform identifier. |
| `category` | `game_mask_id` from the Streamlabs category search response | Selected game category identifier. |
| `audience_type` | `0` when `18+ Stream` is off; `1` when it is on | Mature/audience-control request. |

The response contains the LIVE session id, RTMP server URL, and stream key.

## Session End and Crash Recovery

The observed end request is a `POST` to:

`https://streamlabs.com/api/v5/slobs/tiktok/stream/<live-id>/end`

No documented read-only endpoint for checking the state of a previously
created TikTok LIVE session has been identified. The plugin therefore uses the
end operation as the recovery confirmation after OBS restarts with a persisted
session:

- A confirmed success or a reported missing session releases the stored
  credentials and the output/account reservation.
- A transport or service error does not release the reservation. The dock
  labels the session as unknown and explains that it remains reserved.
- The plugin must never claim that an unverified persisted session is live.

This deliberately favors preventing a second profile from overwriting a
possibly active session over allowing a premature retry.

## Category Behaviour

- Category search uses `GET /api/v5/slobs/tiktok/info?category=<query>`.
- The service returns category display names and `game_mask_id` values. The list
  is dynamic and may differ by account, locale, or current TikTok availability;
  the plugin must not maintain a hard-coded game catalogue.
- The original reference project adds a local `Other` entry with an empty
  `game_mask_id`. Therefore `Other` and a blank category produce the same
  observed request value: an empty `category` field.
- A controlled LIVE test on 2026-08-15 confirmed that an empty `category`
  field produced a LIVE session without a game category. This is the expected
  plugin behaviour for an intentionally blank selection.
- This is an observed behaviour, not a public API guarantee. It should be
  rechecked if Streamlabs or TikTok changes the flow.
- No documented reset value such as `none`, `null`, or `0` has been identified.
  Do not claim that a UI option clears the TikTok category until a controlled
  LIVE test proves it.

## Audience Behaviour

- The reference project explicitly sends `audience_type=0` when mature content
  is disabled and `audience_type=1` when it is enabled.
- There is no public documentation mapping this internal field to every TikTok
  LIVE audience-control state. TikTok can retain or apply room/account-level
  settings independently.
- The UI must describe `18+ Stream` as a request to mark the LIVE as 18+, not
  as a guaranteed reset of every pre-existing TikTok restriction.

## Product Rules

- Send only a category id returned by the Streamlabs category search endpoint.
- Keep an explicit distinction in the UI between an empty selection and a game
  category selected from the results.
- Treat an intentionally blank game-tag selection as `No game category`.
  Preserve the empty `category` field in the stream-start request.
- Do not add unsupported title, tag, cover, description, or audience fields to
  the stream-start request without an observed or documented contract.
