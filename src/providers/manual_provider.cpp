// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "providers/manual_provider.hpp"

#include "provider_registry.hpp"
#include "token_store.hpp"

namespace {

QString missing_manual_credentials_error()
{
	return QStringLiteral("Manual stream URL and stream key are missing.");
}

} // namespace

QString ManualProvider::id() const
{
	return ProviderRegistry::manual_id();
}

void ManualProvider::refresh_account(const ProviderAccountReference &account, AccountCallback completion)
{
	const LiveCredentials credentials = TokenStore::load_live_credentials(account.profile_id);
	ProviderAccountStatus status;
	status.username = account.profile_id;
	status.status = credentials.server.trimmed().isEmpty() || credentials.key.trimmed().isEmpty()
		? QStringLiteral("credentials_missing") : QStringLiteral("credentials_ready");
	status.can_go_live = status.status == QStringLiteral("credentials_ready");
	status.live_access_is_confirmed = status.can_go_live;
	completion(status, {});
}

void ManualProvider::create_live(const ProviderAccountReference &account, const LiveRequest & /*request*/,
	LiveCallback completion)
{
	const LiveCredentials credentials = TokenStore::load_live_credentials(account.profile_id);
	if (credentials.server.trimmed().isEmpty() || credentials.key.trimmed().isEmpty()) {
		completion({}, missing_manual_credentials_error());
		return;
	}
	PreparedLive live;
	live.server = credentials.server;
	live.key = credentials.key;
	completion(std::move(live), {});
}

void ManualProvider::end_live(const ProviderAccountReference & /*account*/, const PreparedLive & /*live*/,
	EndCallback completion)
{
	completion({.ended = true});
}
