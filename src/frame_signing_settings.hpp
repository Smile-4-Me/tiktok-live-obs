// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "frame_signing_credentials.hpp"

class QString;
struct TikTokStudioAccountCredentials;

// The only bridge between account lifecycle and encoded-frame signing. UI and
// providers use this narrow module instead of directly manipulating signing
// records in the generic secret store.
class FrameSigningSettings final {
public:
	[[nodiscard]] static FrameSigningCredentials load(const QString &profile_id);
	static bool save(const QString &profile_id, const FrameSigningCredentials &credentials);
	static void remove(const QString &profile_id);
	// TikTok LIVE Studio stores its rotating signer inputs with its account
	// session. Keep that provider detail out of the dock while maintaining the
	// separate signing-store boundary.
	static bool synchronize_tiktok_studio_account(const QString &profile_id,
		const QString &account_id, QString *error = nullptr);
	[[nodiscard]] static FrameSigningCredentials for_tiktok_studio_account(
		const FrameSigningCredentials &existing, const TikTokStudioAccountCredentials &account);
};
