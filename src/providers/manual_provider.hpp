// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "providers/provider_lifecycle.hpp"

class ManualProvider final : public ProviderLifecycle {
public:
	[[nodiscard]] QString id() const override;
	void refresh_account(const ProviderAccountReference &account, AccountCallback completion) override;
	void create_live(const ProviderAccountReference &account, const LiveRequest &request,
		LiveCallback completion) override;
	void end_live(const ProviderAccountReference &account, const PreparedLive &live,
		EndCallback completion) override;
};
