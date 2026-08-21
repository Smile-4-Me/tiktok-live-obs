// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include <QString>

// Profile is the persisted, non-secret state for one TikTok account connection.
// Tokens and active stream credentials are deliberately stored outside this type.
enum class ProfileState { NeedsLogin, AwaitingLiveAccess, Ready, SessionUncertain, Live };

struct Profile {
	QString id;
	QString provider_id = QStringLiteral("streamlabs");
	QString display_name;
	QString tiktok_username;
	QString output_name;
	QString stream_title;
	QString category;
	QString category_id;
	bool mature = false;
	bool can_go_live = false;
	bool live = false;
	bool preparing = false;
	bool ending = false;
	bool recovering = false;
	bool session_uncertain = false;
	QString live_id;
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
		if (tiktok_username.isEmpty())
			return ProfileState::NeedsLogin;
		if (!can_go_live)
			return ProfileState::AwaitingLiveAccess;
		return ProfileState::Ready;
	}
};
