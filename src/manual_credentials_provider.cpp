// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "manual_credentials_provider.hpp"

#include "token_store.hpp"

QString ManualCredentialsProvider::identifier() const
{
	return QStringLiteral("manual-credentials");
}

QString ManualCredentialsProvider::display_name() const
{
	return QStringLiteral("Manual credentials");
}

bool ManualCredentialsProvider::is_configured(const QString &profile_id) const
{
	const LiveCredentials credentials = TokenStore::load_live_credentials(profile_id);
	return !credentials.server.isEmpty() && !credentials.key.isEmpty();
}

SessionDescriptor ManualCredentialsProvider::session_for(const QString &profile_id, QString *error) const
{
	const LiveCredentials credentials = TokenStore::load_live_credentials(profile_id);
	if (credentials.server.isEmpty() || credentials.key.isEmpty()) {
		if (error)
			*error = QStringLiteral("No stream URL or stream key has been saved for this profile.");
		return {};
	}
	return {display_name(), {}, credentials.server, credentials.key};
}

bool ManualCredentialsProvider::save(const QString &profile_id, const QString &username, const QString &url,
	const QString &key, QString *error) const
{
	if (username.trimmed().isEmpty() || url.trimmed().isEmpty() || key.trimmed().isEmpty()) {
		if (error)
			*error = QStringLiteral("Username, stream URL, and stream key are required.");
		return false;
	}
	if (!TokenStore::save_live_credentials(profile_id, {url.trimmed(), key.trimmed()})) {
		if (error)
			*error = QStringLiteral("Credentials could not be saved securely.");
		return false;
	}
	return true;
}

void ManualCredentialsProvider::remove(const QString &profile_id) const
{
	TokenStore::remove_live_credentials(profile_id);
}
