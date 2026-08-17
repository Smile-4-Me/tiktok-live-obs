// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QString>

enum class ProfileState { NeedsCredentials, Ready, AppliedToAitum };

// Persisted profile state. Stream credentials intentionally remain in the
// operating-system credential store and are never saved to this object.
struct Profile {
	QString id;
	QString display_name;
	QString tiktok_username;
	QString output_name;
	QString stream_title;
	QString category;
	bool mature = false;
	bool credentials_applied = false;
	QString diagnostic;
	bool diagnostic_error = false;

	[[nodiscard]] ProfileState state() const
	{
		if (tiktok_username.isEmpty())
			return ProfileState::NeedsCredentials;
		return credentials_applied ? ProfileState::AppliedToAitum : ProfileState::Ready;
	}
};
