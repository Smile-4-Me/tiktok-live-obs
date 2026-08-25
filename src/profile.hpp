// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include <QString>

// Profile is the persisted, non-secret state for one TikTok account connection.
// Tokens and active stream credentials are deliberately stored outside this type.
enum class ProfileState { NeedsLogin, AwaitingLiveAccess, Ready, SessionUncertain, Live };

struct Profile {
	QString id;
	QString account_id;
	QString provider_id = QStringLiteral("tiktok-studio");
	QString display_name;
	QString tiktok_username;
	// Providers can discover an account name only while a LIVE session is
	// active. It is intentionally runtime-only and is cleared when the session
	// ends; manually entered profile names remain in tiktok_username.
	QString live_tiktok_username;
	QString output_name;
	QString stream_title;
	QString hashtag_id;
	QString category;
	QString category_id;
	bool mature = false;
	bool frame_signing_enabled = false;
	// Runtime-only: the current callback is attached to OBS' main stream rather
	// than the named Aitum output. No callback survives an OBS restart.
	bool frame_signing_uses_main_output = false;
	QString frame_signing_output_name;
	bool can_go_live = false;
	bool live = false;
	bool preparing = false;
	bool ending = false;
	bool recovering = false;
	bool session_uncertain = false;
	QString live_id;
	QString stream_id;
	QString stream_server;
	QString stream_key;
	QString application_status;
	QString diagnostic;
	bool diagnostic_error = false;

	[[nodiscard]] ProfileState state() const
	{
		if (session_uncertain)
			return ProfileState::SessionUncertain;
		if (live)
			return ProfileState::Live;
		if (provider_id == QStringLiteral("manual") && !can_go_live)
			return ProfileState::NeedsLogin;
		// Manual RTMP credentials have no trustworthy username field. Their stream
		// URL and key are sufficient to move to the next step once saved.
		if (tiktok_username.isEmpty() && provider_id != QStringLiteral("manual"))
			return ProfileState::NeedsLogin;
		if (!can_go_live)
			return ProfileState::AwaitingLiveAccess;
		return ProfileState::Ready;
	}
};
