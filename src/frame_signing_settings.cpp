// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "frame_signing_settings.hpp"

#include "tiktok_studio_account.hpp"
#include "token_store.hpp"

FrameSigningCredentials FrameSigningSettings::load(const QString &profile_id)
{
	return TokenStore::load_frame_signing_credentials(profile_id);
}

bool FrameSigningSettings::save(const QString &profile_id, const FrameSigningCredentials &credentials)
{
	return TokenStore::save_frame_signing_credentials(profile_id, credentials);
}

void FrameSigningSettings::remove(const QString &profile_id)
{
	TokenStore::remove_frame_signing_credentials(profile_id);
}

bool FrameSigningSettings::synchronize_tiktok_studio_account(const QString &profile_id,
	const QString &account_id, QString *error)
{
	const TikTokStudioAccountCredentials account = TokenStore::load_tiktok_studio_account(account_id);
	if (!account.has_device()) {
		if (error)
			*error = QStringLiteral("The saved TikTok device registration is missing.");
		return false;
	}
	const FrameSigningCredentials signing = for_tiktok_studio_account(load(profile_id), account);
	if (save(profile_id, signing))
		return true;
	if (error)
		*error = TokenStore::last_error().trimmed();
	return false;
}

FrameSigningCredentials FrameSigningSettings::for_tiktok_studio_account(
	const FrameSigningCredentials &existing, const TikTokStudioAccountCredentials &account)
{
	FrameSigningCredentials signing = existing;
	signing.api_url = account.signer_api_url;
	signing.rapidapi_key = account.rapidapi_key;
	signing.uid = account.user_id;
	signing.device_id = account.device_id;
	return signing;
}
