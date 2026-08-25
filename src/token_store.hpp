// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include "tiktok_studio_account.hpp"
#include "frame_signing_credentials.hpp"

#include <QString>

struct LiveCredentials {
	QString server;
	QString key;
};

class TokenStore final {
public:
	static void set_storage_scope(const QString &scope);
	static bool save(const QString &profile_id, const QString &token);
	static QString load(const QString &profile_id);
	static bool save_live_credentials(const QString &profile_id, const LiveCredentials &credentials);
	static LiveCredentials load_live_credentials(const QString &profile_id);
	static void remove_live_credentials(const QString &profile_id);
	static bool save_frame_signing_credentials(const QString &profile_id,
		const FrameSigningCredentials &credentials);
	static FrameSigningCredentials load_frame_signing_credentials(const QString &profile_id);
	static void remove_frame_signing_credentials(const QString &profile_id);
	static bool save_tiktok_studio_account(const QString &account_id,
		const TikTokStudioAccountCredentials &credentials);
	static TikTokStudioAccountCredentials load_tiktok_studio_account(const QString &account_id);
	static void remove_tiktok_studio_account(const QString &account_id);
	static QString last_error();
	static void remove(const QString &profile_id);
};
