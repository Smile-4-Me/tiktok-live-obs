// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "providers/streamlabs_provider.hpp"

#include "provider_registry.hpp"
#include "streamlabs_client.hpp"
#include "token_store.hpp"

namespace {

QString missing_streamlabs_token_error()
{
	return QStringLiteral("The saved Streamlabs token is missing.");
}

} // namespace

StreamlabsProvider::StreamlabsProvider(StreamlabsClient &client) : client_(client) {}

QString StreamlabsProvider::id() const
{
	return ProviderRegistry::streamlabs_id();
}

void StreamlabsProvider::refresh_account(const ProviderAccountReference &account, AccountCallback completion)
{
	const QString token = TokenStore::load(account.profile_id);
	if (token.isEmpty()) {
		completion({}, missing_streamlabs_token_error());
		return;
	}
	client_.verify_account(token, [completion = std::move(completion)](StreamlabsAccount response, QString error) {
		ProviderAccountStatus status;
		status.username = response.username;
		status.status = response.application_status;
		status.can_go_live = response.can_go_live;
		status.live_access_is_confirmed = true;
		completion(std::move(status), std::move(error));
	});
}

void StreamlabsProvider::create_live(const ProviderAccountReference &account, const LiveRequest &request,
	LiveCallback completion)
{
	const QString token = TokenStore::load(account.profile_id);
	if (token.isEmpty()) {
		completion({}, missing_streamlabs_token_error());
		return;
	}
	client_.start_live(token, request.title, request.category_id, request.mature,
		[completion = std::move(completion)](StreamlabsLive response, QString error) {
			PreparedLive live;
			live.session_id = response.id;
			live.server = response.server;
			live.key = response.key;
			completion(std::move(live), std::move(error));
		});
}

bool StreamlabsProvider::is_live_access_denied_error(const QString &error) const
{
	const QString message = error.trimmed().toLower();
	return message.contains(QStringLiteral("can_be_live")) ||
		message.contains(QStringLiteral("no live auth")) ||
		message.contains(QStringLiteral("live access")) ||
		message.contains(QStringLiteral("not eligible")) ||
		message.contains(QStringLiteral("not approved")) ||
		message.contains(QStringLiteral("not authorised")) ||
		message.contains(QStringLiteral("not authorized")) ||
		message.contains(QStringLiteral("live permission"));
}

void StreamlabsProvider::end_live(const ProviderAccountReference &account, const PreparedLive &live,
	EndCallback completion)
{
	const QString token = TokenStore::load(account.profile_id);
	if (token.isEmpty()) {
		completion({.error = missing_streamlabs_token_error()});
		return;
	}
	client_.end_live(token, live.session_id, [completion = std::move(completion)](StreamlabsEndResult response) {
		completion({.ended = response.ended, .stale_session = response.stale_session,
			.error = response.error});
	});
}

void StreamlabsProvider::search_categories(const ProviderAccountReference &account, const QString &query,
	CatalogCallback completion)
{
	const QString token = TokenStore::load(account.profile_id);
	if (token.isEmpty()) {
		completion({}, missing_streamlabs_token_error());
		return;
	}
	client_.search_categories(token, query,
		[completion = std::move(completion)](QVector<StreamlabsCategory> response, QString error) {
		QVector<ProviderCatalogEntry> categories;
		categories.reserve(response.size());
		for (const StreamlabsCategory &category : response)
			categories.push_back({category.id, category.name});
		completion(std::move(categories), std::move(error));
	});
}
