// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "tiktok_browser_session_adapter.hpp"

std::optional<TikTokBrowserSessionImport>
collect_tiktok_browser_session(QString *error)
{
	// Intentionally empty integration seam. A later platform-specific module
	// should return a Netscape-format jar containing only the user-approved
	// TikTok cookies. Do not put UI, persistence, or TikTok API calls here;
	// those are completed by BridgeDock after this function returns.
	if (error) {
		*error = QStringLiteral(
			"The browser-session collector has not been added to this build yet.");
	}
	return std::nullopt;
}
