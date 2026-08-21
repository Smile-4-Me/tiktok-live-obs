// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QByteArray>
#include <QString>

// Secret and account-scoped state for one TikTok LIVE Studio login. The
// profile INI stores only the stable account_id; this object is persisted in
// Windows Credential Manager so additional profiles/accounts can be added
// without coupling credentials to the dock UI.
struct TikTokStudioAccountCredentials {
	QString rapidapi_key;
	QString signer_api_url = QStringLiteral("https://tiktok-live-studio-api-signer1.p.rapidapi.com/");
	QString device_id;
	QString install_id;
	QString live_studio_version = QStringLiteral("1.27.0");
	QString username;
	QString user_id;
	QByteArray cookie_jar;

	[[nodiscard]] bool has_device() const
	{
		return !device_id.trimmed().isEmpty() && device_id != QStringLiteral("0") &&
			!install_id.trimmed().isEmpty() && install_id != QStringLiteral("0");
	}

	[[nodiscard]] bool has_login() const
	{
		return has_device() && !cookie_jar.isEmpty();
	}
};

// A device registration is the stable identity of one saved LIVE Studio
// account. API responses are allowed to refresh cookies and account metadata,
// but they must never accidentally clear or rotate a registration that is
// already persisted. Deleting the saved account remains the explicit way to
// generate a new identity.
inline void preserve_tiktok_studio_device_identity(
	const TikTokStudioAccountCredentials &persisted,
	TikTokStudioAccountCredentials *updated)
{
	if (updated && persisted.has_device()) {
		updated->device_id = persisted.device_id;
		updated->install_id = persisted.install_id;
	}
}
