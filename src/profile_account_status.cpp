// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "profile_account_status.hpp"

#include "profile_live_session.hpp"

namespace ProfileAccountStatus {

void apply(Profile &profile, const ProviderAccountStatus &status)
{
	profile.tiktok_username = status.username;
	// An account refresh can confirm the login without being able to inspect
	// LIVE entitlement. Never let that inconclusive result overwrite TikTok's
	// previously explicit denial from a failed create-LIVE request.
	if (!status.live_access_is_confirmed &&
		(profile.application_status == QStringLiteral("live_access_denied") ||
			profile.application_status == QStringLiteral("tiktok_live_authorization_missing"))) {
		ProfileLiveSession::clear_output_assignment(profile);
		return;
	}
	profile.application_status = status.status;
	profile.can_go_live = status.can_go_live;
	if (status.live_access_is_confirmed && !profile.can_go_live)
		ProfileLiveSession::clear_output_assignment(profile);
}

} // namespace ProfileAccountStatus
