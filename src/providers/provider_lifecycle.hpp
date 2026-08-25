// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "provider_contract.hpp"

#include <functional>
#include <QVector>

// Provider adapters own provider-specific authentication, request payloads,
// and secret lookups. They never touch QWidget, Aitum, or OBS outputs.
class ProviderLifecycle {
public:
	using AccountCallback = std::function<void(ProviderAccountStatus status, QString error)>;
	using CatalogCallback = std::function<void(QVector<ProviderCatalogEntry> entries, QString error)>;
	using LiveCallback = std::function<void(PreparedLive live, QString error)>;
	using EndCallback = std::function<void(ProviderEndResult result)>;
	using HeartbeatCallback = std::function<void(ProviderHeartbeatResult result)>;

	virtual ~ProviderLifecycle() = default;
	[[nodiscard]] virtual QString id() const = 0;
	virtual void refresh_account(const ProviderAccountReference &account, AccountCallback completion) = 0;
	virtual void create_live(const ProviderAccountReference &account, const LiveRequest &request,
		LiveCallback completion) = 0;
	// Providers translate their own failed create-LIVE response into this
	// provider-neutral outcome. The dock then returns the profile to the shared
	// account-status step instead of leaving it on stream details.
	[[nodiscard]] virtual bool is_live_access_denied_error(const QString &error) const;
	virtual void end_live(const ProviderAccountReference &account, const PreparedLive &live,
		EndCallback completion) = 0;
	virtual void fetch_game_tags(const ProviderAccountReference &account, CatalogCallback completion);
	virtual void search_categories(const ProviderAccountReference &account, const QString &query,
		CatalogCallback completion);

	// Only providers that declare supports_session_recovery implement these
	// hooks. Defaults make the unsupported path explicit and fail closed.
	virtual void find_continuable_live(const ProviderAccountReference &account, LiveCallback completion);
	virtual void resume_live(const ProviderAccountReference &account, LiveCallback completion);
	virtual void heartbeat(const ProviderAccountReference &account, const PreparedLive &live, int status,
		HeartbeatCallback completion);
};
