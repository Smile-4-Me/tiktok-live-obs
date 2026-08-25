// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "providers/tiktok_studio_provider.hpp"

#include "provider_registry.hpp"
#include "tiktok_studio_client.hpp"
#include "tiktok_studio_session.hpp"
#include "token_store.hpp"

namespace {

PreparedLive normalize_live(const TikTokStudioLive &source)
{
	PreparedLive live;
	live.session_id = source.room_id;
	live.room_id = source.room_id;
	live.stream_id = source.stream_id;
	live.server = source.server;
	live.key = source.key;
	return live;
}

ProviderAccountStatus normalize_account(const TikTokStudioAccountInfo &source)
{
	ProviderAccountStatus status;
	status.username = source.account.username;
	status.status = source.application_status;
	status.can_go_live = source.can_go_live;
	// TikTok can explicitly report a non-entitled account. When read-only
	// endpoints are incomplete, the client returns `live_access_unknown` and
	// leaves the definitive check to LIVE creation.
	status.live_access_is_confirmed = source.application_status != QStringLiteral("live_access_unknown");
	return status;
}

} // namespace

TikTokStudioProvider::TikTokStudioProvider(TikTokStudioClient &client) : client_(client) {}

QString TikTokStudioProvider::id() const
{
	return ProviderRegistry::tiktok_studio_id();
}

QString TikTokStudioProvider::missing_login_error()
{
	return QStringLiteral("The saved TikTok LIVE Studio login is missing.");
}

bool TikTokStudioProvider::save_account(const QString &account_id,
	const TikTokStudioAccountCredentials &credentials, QString *error) const
{
	// Error responses may contain an empty placeholder account. Never let one
	// overwrite the durable device identity and cookie jar in secure storage.
	if (!credentials.has_device())
		return true;
	if (TokenStore::save_tiktok_studio_account(account_id, credentials))
		return true;
	if (error)
		*error = TokenStore::last_error().trimmed();
	return false;
}

void TikTokStudioProvider::refresh_account(const ProviderAccountReference &account,
	AccountCallback completion)
{
	// Each bridge profile owns a separate account_id. Loading credentials by this
	// identifier ensures an account refresh never depends on Chrome's active
	// TikTok session or another profile's saved TikTok LIVE Studio login.
	const TikTokStudioAccountCredentials credentials = TokenStore::load_tiktok_studio_account(account.account_id);
	if (!credentials.has_login()) {
		completion({}, missing_login_error());
		return;
	}
	client_.verify_account(credentials, [this, account, completion = std::move(completion)]
		(TikTokStudioAccountInfo response, QString error) {
		QString save_error;
		if (!save_account(account.account_id, response.account, &save_error)) {
			completion({}, save_error.isEmpty()
				? QStringLiteral("The refreshed TikTok login could not be saved securely.") : save_error);
			return;
		}
		completion(normalize_account(response), std::move(error));
	});
}

void TikTokStudioProvider::create_live(const ProviderAccountReference &account, const LiveRequest &request,
	LiveCallback completion)
{
	const TikTokStudioAccountCredentials credentials = TokenStore::load_tiktok_studio_account(account.account_id);
	if (!credentials.has_login()) {
		completion({}, missing_login_error());
		return;
	}
	client_.start_live(credentials, request.title, request.topic_id, request.category_id, request.mature,
		[this, account, completion = std::move(completion)](TikTokStudioLive response, QString error) {
			QString save_error;
			if (!save_account(account.account_id, response.account, &save_error)) {
				completion({}, save_error.isEmpty()
					? QStringLiteral("The refreshed TikTok login could not be saved securely.") : save_error);
				return;
			}
			completion(normalize_live(response), std::move(error));
		});
}

bool TikTokStudioProvider::is_live_access_denied_error(const QString &error) const
{
	return tiktok_studio_session_has_no_live_auth(error);
}

void TikTokStudioProvider::end_live(const ProviderAccountReference &account, const PreparedLive &live,
	EndCallback completion)
{
	const TikTokStudioAccountCredentials credentials = TokenStore::load_tiktok_studio_account(account.account_id);
	if (!credentials.has_login()) {
		completion({.error = missing_login_error()});
		return;
	}
	client_.end_live(credentials, live.room_id.isEmpty() ? live.session_id : live.room_id, live.stream_id,
		[this, account, completion = std::move(completion)](TikTokStudioEndResult response) {
			QString save_error;
			if (!save_account(account.account_id, response.account, &save_error)) {
				completion({.error = save_error.isEmpty()
					? QStringLiteral("The refreshed TikTok login could not be saved securely.") : save_error});
				return;
			}
			completion({.ended = response.ended, .stale_session = response.stale_session,
				.error = response.error});
		});
}

void TikTokStudioProvider::fetch_game_tags(const ProviderAccountReference &account,
	CatalogCallback completion)
{
	const TikTokStudioAccountCredentials credentials = TokenStore::load_tiktok_studio_account(account.account_id);
	if (!credentials.has_login()) {
		completion({}, missing_login_error());
		return;
	}
	client_.fetch_game_tags(credentials,
		[this, account, completion = std::move(completion)](QVector<TikTokStudioGameTag> response,
			TikTokStudioAccountCredentials updated_account, QString error) {
		QString save_error;
		if (!save_account(account.account_id, updated_account, &save_error)) {
			completion({}, save_error.isEmpty()
				? QStringLiteral("The refreshed TikTok login could not be saved securely.") : save_error);
			return;
		}
		QVector<ProviderCatalogEntry> tags;
		tags.reserve(response.size());
		for (const TikTokStudioGameTag &tag : response)
			tags.push_back({tag.id, tag.name});
		completion(std::move(tags), std::move(error));
	});
}

void TikTokStudioProvider::find_continuable_live(const ProviderAccountReference &account,
	LiveCallback completion)
{
	const TikTokStudioAccountCredentials credentials = TokenStore::load_tiktok_studio_account(account.account_id);
	if (!credentials.has_login()) {
		completion({}, missing_login_error());
		return;
	}
	client_.find_continuable_live(credentials, [this, account, completion = std::move(completion)]
		(TikTokStudioLive response, QString error) {
		QString save_error;
		if (!save_account(account.account_id, response.account, &save_error)) {
			completion({}, save_error.isEmpty()
				? QStringLiteral("The refreshed TikTok login could not be saved securely.") : save_error);
			return;
		}
		completion(normalize_live(response), std::move(error));
	});
}

void TikTokStudioProvider::resume_live(const ProviderAccountReference &account, LiveCallback completion)
{
	const TikTokStudioAccountCredentials credentials = TokenStore::load_tiktok_studio_account(account.account_id);
	if (!credentials.has_login()) {
		completion({}, missing_login_error());
		return;
	}
	client_.resume_live(credentials, [this, account, completion = std::move(completion)]
		(TikTokStudioLive response, QString error) {
		QString save_error;
		if (!save_account(account.account_id, response.account, &save_error)) {
			completion({}, save_error.isEmpty()
				? QStringLiteral("The refreshed TikTok login could not be saved securely.") : save_error);
			return;
		}
		completion(normalize_live(response), std::move(error));
	});
}

void TikTokStudioProvider::heartbeat(const ProviderAccountReference &account, const PreparedLive &live,
	int status, HeartbeatCallback completion)
{
	const TikTokStudioAccountCredentials credentials = TokenStore::load_tiktok_studio_account(account.account_id);
	if (!credentials.has_login()) {
		completion({.error = missing_login_error()});
		return;
	}
	client_.heartbeat(credentials, live.room_id.isEmpty() ? live.session_id : live.room_id, live.stream_id, status,
		[this, account, completion = std::move(completion)](TikTokStudioHeartbeatResult response) {
			QString save_error;
			if (!save_account(account.account_id, response.account, &save_error)) {
				completion({.error = save_error.isEmpty()
					? QStringLiteral("The refreshed TikTok login could not be saved securely.") : save_error});
				return;
			}
			completion({.session_is_live = response.room_is_living, .error = response.error});
		});
}
