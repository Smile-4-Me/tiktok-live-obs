// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "profile_account_status.hpp"

#include <iostream>

int main()
{
	Profile profile;
	const ProviderAccountStatus status{
		.username = QStringLiteral("creator"),
		.status = QStringLiteral("approved"),
		.can_go_live = true,
		.live_access_is_confirmed = true,
	};
	ProfileAccountStatus::apply(profile, status);

	if (profile.tiktok_username != status.username ||
		profile.application_status != status.status ||
		profile.can_go_live != status.can_go_live) {
		std::cerr << "Provider account status was not applied consistently.\n";
		return 1;
	}

	profile.application_status = QStringLiteral("live_access_denied");
	profile.can_go_live = false;
	const ProviderAccountStatus unknown_status{
		.username = QStringLiteral("creator"),
		.status = QStringLiteral("live_access_unknown"),
		.can_go_live = true,
		.live_access_is_confirmed = false,
	};
	ProfileAccountStatus::apply(profile, unknown_status);
	if (profile.application_status != QStringLiteral("live_access_denied") || profile.can_go_live) {
		std::cerr << "An inconclusive refresh overwrote a confirmed LIVE-access denial.\n";
		return 1;
	}

	profile.output_name = QStringLiteral("TikTok Vertical");
	ProfileAccountStatus::apply(profile, {
		.username = QStringLiteral("creator"),
		.status = QStringLiteral("live_access_denied"),
		.can_go_live = false,
		.live_access_is_confirmed = true,
	});
	if (!profile.output_name.isEmpty()) {
		std::cerr << "A confirmed LIVE-access denial retained a stale output assignment.\n";
		return 2;
	}

	std::cout << "Profile account status mapping passed.\n";
	return 0;
}
