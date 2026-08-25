// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "profile.hpp"
#include "provider_contract.hpp"

// Pure persisted-session transitions shared by every credential provider.
// UI cleanup (OBS output callbacks, timers, widgets) intentionally remains in
// BridgeDock; this module owns only the state that is safe to persist.
namespace ProfileLiveSession {

void reserve(Profile &profile, const PreparedLive &live);
void reserve(Profile &profile, const QString &session_id, const QString &stream_id,
	const QString &server, const QString &key);
void mark_uncertain(Profile &profile, const PreparedLive &live);
void clear(Profile &profile);
// Remove the selected Aitum output and any output-specific signing attachment.
// Use this whenever a profile returns to account setup, so a stale output is
// never displayed as still linked to a profile that can no longer use it.
void clear_output_assignment(Profile &profile);

} // namespace ProfileLiveSession
