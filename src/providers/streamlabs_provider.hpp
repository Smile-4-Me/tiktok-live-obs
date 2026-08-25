// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "providers/provider_lifecycle.hpp"

class StreamlabsClient;

class StreamlabsProvider final : public ProviderLifecycle {
public:
	explicit StreamlabsProvider(StreamlabsClient &client);

	[[nodiscard]] QString id() const override;
	void refresh_account(const ProviderAccountReference &account, AccountCallback completion) override;
	void create_live(const ProviderAccountReference &account, const LiveRequest &request,
		LiveCallback completion) override;
	[[nodiscard]] bool is_live_access_denied_error(const QString &error) const override;
	void end_live(const ProviderAccountReference &account, const PreparedLive &live,
		EndCallback completion) override;
	void search_categories(const ProviderAccountReference &account, const QString &query,
		CatalogCallback completion) override;

private:
	StreamlabsClient &client_;
};
