// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "providers/provider_lifecycle.hpp"

class TikTokStudioClient;

class TikTokStudioProvider final : public ProviderLifecycle {
public:
	explicit TikTokStudioProvider(TikTokStudioClient &client);

	[[nodiscard]] QString id() const override;
	void refresh_account(const ProviderAccountReference &account, AccountCallback completion) override;
	void create_live(const ProviderAccountReference &account, const LiveRequest &request,
		LiveCallback completion) override;
	[[nodiscard]] bool is_live_access_denied_error(const QString &error) const override;
	void end_live(const ProviderAccountReference &account, const PreparedLive &live,
		EndCallback completion) override;
	void fetch_game_tags(const ProviderAccountReference &account, CatalogCallback completion) override;
	void find_continuable_live(const ProviderAccountReference &account, LiveCallback completion) override;
	void resume_live(const ProviderAccountReference &account, LiveCallback completion) override;
	void heartbeat(const ProviderAccountReference &account, const PreparedLive &live, int status,
		HeartbeatCallback completion) override;

private:
	[[nodiscard]] static QString missing_login_error();
	bool save_account(const QString &account_id, const class TikTokStudioAccountCredentials &credentials,
		QString *error) const;
	TikTokStudioClient &client_;
};
