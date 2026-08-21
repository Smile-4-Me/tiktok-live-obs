# Loukious TikTokStreamKeyGenerator: Architecture Research Record

## Purpose and scope

This record preserves the architectural research performed for the private
TikTok Live OBS project. It is intentionally detailed so that future work can
resume without repeating discovery.

It is **not** an implementation guide for TikTok-specific authentication,
request signing, integrity metadata, encoder impersonation, or bypassing a
platform control. Those areas are out of scope for this project unless an
official, documented, and authorized integration becomes available.

The reusable findings are limited to general software architecture:

- provider boundaries;
- UI and asynchronous state management;
- local media transport separation;
- health checking and recovery;
- secret handling;
- observability; and
- a local-only research harness.

## Source provenance

| Field | Value |
| --- | --- |
| Upstream repository | <https://github.com/Loukious/TikTokStreamKeyGenerator> |
| Inspected branch | `main` |
| Pinned revision | `31a6906df4b67e5281697a1310884a80719a7cd6` |
| Revision date | 2026-07-10T22:45:02+01:00 |
| Research date | 2026-08-21 |
| Local read-only research checkout | `research/Loukious-TikTokStreamKeyGenerator-20260821` (not part of this repository) |
| License finding | No license file was present in the inspected repository root. Do not copy source code from it without obtaining permission or confirming a license. |

This document is an independent description of observed architecture. No
upstream signing or protocol implementation is imported into this repository.

## Executive summary

The upstream application is a desktop Python application that coordinates four
distinct concerns:

1. account/login state;
2. remote LIVE-room lifecycle;
3. a local media relay process; and
4. recurring operational monitoring.

Its important engineering lesson is not any platform-specific request detail.
It is the explicit separation between a **control plane** (login, session
state, metadata, start/end actions) and a **media plane** (a local RTMP relay
process that moves encoded media).

The application delegates its platform-specific signing to a hosted service.
RapidAPI is the marketplace/gateway used for the default endpoint; it is not a
technical requirement of the GUI or the local proxy. Replacing it would still
require an independently authorized implementation of the external signing
service. This project will not implement or emulate that service.

## Repository inventory

### Root files

| Path | Role |
| --- | --- |
| `TiktokStreamKeyGenerator.py` | Main application, remote-session client, UI, state timers, configuration, and proxy process orchestration. |
| `Updater.py` | Version-checking helper. |
| `_version.py` | Version metadata. |
| `requirements.txt` | Python runtime dependencies. |
| `README.md` | User-facing architecture and operating instructions. |

### `Libs/` modules

| Module | Observed responsibility | Reusable lesson |
| --- | --- | --- |
| `domain_routing.py` | Host discovery, region candidate selection, common headers, and time-offset handling. | Isolate endpoint resolution and clock handling behind a transport boundary. |
| `device_gen.py` | Desktop-environment identity generation and version discovery. | Treat client capability information as an explicit provider concern; do not scatter it through UI code. |
| `signers.py` | Client for an external signing service. | Keep sensitive/remote signing work behind one interface and fail closed if it is unavailable. |
| `ffmpeg_sei_proxy.py` | Local receiver/relay orchestration, media-stream transformation pipeline, process restart, logging, and relay health. | Keep media transport in an independently monitored component with a clear lifecycle. |
| `log_encrypt_codec.py` | Encoded telemetry helper. | Keep telemetry formatting/encoding separate from product state. Do not reuse platform-specific formats. |
| `XArgus.py`, `XLadon.py`, `XFrameSign.py` | Platform-specific signing/integrity helpers. | Explicitly excluded. No code, behavior, or algorithm is adopted. |

## Main application decomposition

### 1. `Stream`: remote control-plane client

The `Stream` class centralizes most remote operations. Its public lifecycle is
conceptually:

```text
authenticate -> discover/capability setup -> create or resume room
             -> prepare -> live heartbeat -> end
```

It also owns separate operations for room information, account information,
metadata, basic statistics, audience/safety views, thumbnails, and recovery.

The valuable pattern is that all remote calls pass through a small group of
request helpers. Those helpers establish common headers, timeout handling,
retry selection, structured error conversion, and endpoint fallbacks before
business methods consume the results.

**Adoption in TikTok Live OBS:** provider-specific remote operations remain
behind a provider/session interface. The existing Streamlabs client remains a
provider implementation; the Manual provider intentionally has no remote
control-plane client.

### 2. Login client

The upstream repository has a dedicated browser/QR login component rather than
putting browser flow logic into the main stream class. It owns state, polling,
cleanup, and cookie persistence separately.

**Adoption:** keep login mechanisms provider-owned. The dock only asks a
provider to connect or validate an account; it should not know how a provider
obtains authorization.

### 3. Local transport/relay process

The upstream proxy module has a process-oriented design:

```text
OBS -> local listener -> relay worker -> destination
```

The UI starts the helper process, waits for its listener to be reachable,
monitors whether it is alive, captures logs, and stops it during shutdown. The
relay retries a local listener after a completed or failed session instead of
leaving the UI in a permanently dead state.

**Adoption:** our research harness will use the same *shape* but only for
localhost. The plugin must model a local service as a separately startable,
health-checked component. It must never silently assume that a requested
output actually started.

### 4. Timers and state reconciliation

The upstream application uses separate timers for:

- remote anchor/lifecycle heartbeat;
- realtime statistics;
- audience and safety information; and
- proxy health.

This is a useful operational pattern, but each timer must be independently
rate-limited, cancellable, and tied to an explicit session state. A timer must
stop before the owning session/UI is destroyed.

**Adoption:** the local Research provider will use a two-second localhost
heartbeat to validate scheduling, cancellation, failure display, and restart
recovery. It will not contact TikTok or any third-party service.

### 5. Desktop UI

The UI is a single application window with focused subviews for account,
session, output credentials, proxy status, monitoring, and configuration.
Long-running work is moved out of direct click handlers, then its result is
returned to the UI thread.

**Adoption:** our OBS dock already separates profile UI, stream UI, Aitum
bridge logic, Streamlabs client, credential store, and provider registry. A
recent provider-switch crash confirmed why UI reconstruction must be deferred
until a Qt signal handler has returned.

## Inferred state model

The upstream application makes a useful distinction between the following
states, even if their presentation is application-specific:

```text
Disconnected
  -> Connected
  -> Room prepared
  -> Transport ready
  -> Active
  -> Ending
  -> Ended / recovery required
```

The project should preserve this separation. In particular:

- receiving a stream URL/key is not proof that a media transport is running;
- starting a local helper is not proof that the destination received media;
- a successful control-plane response is not proof that an OBS/Aitum output
  actually became active; and
- shutdown or OBS termination must release local reservations safely.

The existing Aitum verification loop already applies this principle: it checks
the observable Aitum output state and cleans up an unstarted session instead
of assuming a successful button action was sufficient.

## RapidAPI assessment

### What RapidAPI is doing in the upstream project

The upstream signer client defaults to a RapidAPI-hosted endpoint and sends a
RapidAPI consumer key when that host is selected. RapidAPI provides the
marketplace proxy, consumer authentication, quotas/rate limits, metering, and
billing around the API. RapidAPI documents that proxy consumers use its host
and key headers, and that an API can instead be reached directly when the
provider chooses not to use the Rapid proxy. [RapidAPI authentication
documentation](https://docs.rapidapi.com/v2.0.0/docs/configuring-api-security)

The inspected client also supports a configurable signer base URL. Therefore
the *application architecture* is not hard-coupled to RapidAPI.

### What is and is not replaceable

| Component | Replaceable? | Conditions |
| --- | --- | --- |
| RapidAPI gateway | Yes | A provider can host a normal HTTPS API with its own authentication, quota, logs, and operations. |
| GUI client call site | Yes | It already follows a service-boundary design. |
| Generic local test signer | Yes | We can implement one for localhost using our own test keys and an intentionally unrelated protocol. |
| TikTok-specific signer behavior | Not in this project | Requires official authorization and a documented integration. Reimplementing it would cross the project's research boundary. |

### Implications for a future authorized service

If an official provider agreement later authorizes a signing or session API,
our own backend should include:

- OAuth or short-lived scoped credentials rather than embedded shared secrets;
- per-user and per-IP rate limits;
- request schema validation and size limits;
- structured audit logs that never retain tokens or stream keys;
- explicit timeout, retry, and circuit-breaker behavior;
- regional deployment and health monitoring;
- key rotation and server-side secret storage; and
- a versioned, documented API contract.

RapidAPI itself adds gateway authentication, usage tracking, quotas, and a
provider-side proxy secret capability. Replacing the marketplace means owning
those operational duties. [RapidAPI provider request-header
documentation](https://docs.rapidapi.com/v2.0/docs/additional-request-headers)

## Dependency assessment

| Upstream dependency | Purpose | Our position |
| --- | --- | --- |
| Python / PySide6 | Standalone desktop UI runtime. | Not needed: our product is a native OBS/Qt plugin. |
| `requests` / `curl_cffi` | HTTP transport and browser-like session behavior. | Reuse only the generic lesson: isolate HTTP transport and timeouts. Our plugin uses its existing native HTTP stack. |
| FFmpeg | Local receiving/relaying process. | Useful as an optional local research transport only. Do not bundle or modify it for platform-specific metadata injection. |
| RapidAPI | Hosted signer marketplace/gateway. | Not needed for our local research provider. |
| `pycryptodome` and signing helper files | Platform-specific transformations. | Explicitly excluded. |
| `truststore` | Certificate-store integration. | The native plugin already uses OS-supported transport on Windows. Multi-platform strategy remains documented separately. |

## Useful design principles adopted by this project

1. **Provider boundary first.** A provider is selected by stable identifier,
   never by a translated UI label.
2. **Separate control plane from transport plane.** Session creation, stream
   credentials, output configuration, and media delivery are different facts.
3. **Fail closed.** Missing credentials, unavailable local services, or
   failed health checks must not be presented as a successful live state.
4. **Observable confirmation.** Verify an Aitum output after start rather
   than trusting a button click.
5. **Explicit recovery.** After OBS restarts, do not keep local state merely
   because an old process said it was live.
6. **Minimal secret surface.** Persist secrets separately from profile
   metadata and avoid logs containing them.
7. **Lifecycle ownership.** Every timer, worker, and child process needs an
   owner and an idempotent stop path.
8. **Health is not telemetry.** A local service health check can run without
   copying, altering, signing, or forwarding any media.

## Explicitly excluded material

The following parts of the upstream repository are not ported, summarized as
algorithms, or called by TikTok Live OBS:

- request-signing algorithms and their inputs;
- frame-signing algorithms;
- H.264/HEVC metadata insertion routines;
- client/device impersonation;
- private endpoint reconstruction;
- cookie-import automation for an undocumented client flow; and
- any attempt to disguise encoder identity or bypass an integrity check.

This exclusion protects the project boundary and keeps the local research
harness useful independently of TikTok.

## Local Research Provider specification

The `research-local` provider is implemented as a private development aid. It
has no TikTok integration and no external network dependency.

### Inputs

- a human-readable test account label;
- a locally supplied RTMP target, if an external local RTMP test server is
  available;
- a local test stream key; and
- an automatically generated per-session research identifier.

### Local-only services

| Service | Bind address | Purpose |
| --- | --- | --- |
| Research heartbeat mock | `127.0.0.1` only | Receives a generic heartbeat every two seconds and reports scheduling/health status. |
| Optional RTMP test server | User-selected local service | Receives a test stream; it is not a TikTok proxy and does not alter media. |

### Required behavior

1. Starting a research session starts the local heartbeat service.
2. The plugin sends generic, non-platform-specific test heartbeats every two
   seconds.
3. Any local failure is visible in the profile diagnostic and never reported
   as a successful external LIVE.
4. Stopping a session stops timers and local listeners idempotently.
5. OBS restart clears the local research reservation. The listener is
   intentionally ephemeral and is never restored as a presumed external LIVE.
6. No packets are sent to TikTok by this provider.

## Validation matrix

| Test | Expected result |
| --- | --- |
| Start Research provider with no Aitum output | Local heartbeat starts; profile shows a local research session, not a TikTok LIVE. |
| Bind collision on localhost port | Start fails with an explicit local-service diagnostic. |
| Stop twice | Second stop is safe and does not crash. |
| Close OBS during active research session | No crash; next start clears the stale local reservation. |
| Aitum output fails to start | Existing Aitum verification clears the prepared profile state. |
| Five concurrent profile rows | UI remains compact; profile conflicts are still enforced. |
| Provider switch during view rebuild | No Qt use-after-free; rebuild is deferred to the next event-loop turn. |

## Current implementation record

The private implementation corresponding to this record is intentionally
small and auditable:

| Component | Repository path | Responsibility | Boundary evidence |
| --- | --- | --- | --- |
| Provider registry | `src/provider_registry.*` | Stable provider identifiers and capability selection. | `research-local` is distinct from Streamlabs and Manual. |
| Local harness | `src/research_lab.*` | Starts a loopback listener on an ephemeral port and sends a generic JSON heartbeat to it every two seconds. | The listener binds to `QHostAddress::LocalHost`; the request target is `127.0.0.1`; no TikTok hostname, request format, or media code exists in this component. |
| Dock integration | `src/bridge_dock.*` | Shows provider-specific UI, stores local test credentials in the existing secret store, and tears down the harness. | Research sessions are labelled as local research, never as confirmed TikTok LIVE sessions. |
| Restart recovery | `src/bridge_dock_profiles.cpp` | Clears an old `research-local` reservation on the next OBS start. | A listener is not assumed to survive an OBS restart. |

The harness also uses a monotonically increasing generation number. A reply
from a stopped session is ignored if a later session has already started. This
is required because local network callbacks, like any asynchronous callback,
can outlive the UI event that initiated them.

### Build verification record

| Date | Check | Result |
| --- | --- | --- |
| 2026-08-21 | Windows Release build with MSVC/NMake | Passed. |
| 2026-08-21 | Release DLL dependency inspection | Passed; release C++ runtime dependencies only, with OBS-provided Qt runtime libraries. |
| 2026-08-21 | Launch OBS with the private plugin installed | Passed; `tiktok-live-obs.dll` was present in the OBS plugin load list and no plugin-specific load failure was logged. |

These checks prove only the local software boundary and loadability. They do
not validate a TikTok integration, and they must never be presented as such.

The repository also contains `tools/verify-research-boundary.ps1`. It verifies
the loopback-only endpoint, the two-second heartbeat interval, and the absence
of platform/media/signing categories from the Research Lab source. Run it from
the repository root before merging changes to `src/research_lab.*`.

## Open questions for an authorized future integration

These questions must be answered by official documentation or a written
provider agreement, not reverse engineering:

1. Which session lifecycle API is officially supported?
2. Is a server-to-server integration offered, and what OAuth scopes apply?
3. Are custom encoder labels or metadata fields supported?
4. Is there an official ingest health/status callback?
5. What credential retention, revocation, and audit requirements apply?
6. Are there platform-specific bitrate, codec, keyframe, or region rules?
7. What support and deprecation commitments exist for the API?

## Re-research checklist

Repeat or amend this record when any of the following changes:

- upstream revision or license;
- an official TikTok developer/LIVE integration is published;
- Streamlabs changes its authorization or session behavior;
- OBS changes the relevant frontend, output, or Qt API;
- Aitum changes its output UI contract; or
- this project considers any non-local transport provider.
