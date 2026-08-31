// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QJsonObject>
#include <QString>

struct TikTokStudioEligibility {
	bool can_go_live = false;
	QString status;
	bool dual_layout_available = false;
	QString dual_layout_status;
};

[[nodiscard]] TikTokStudioEligibility parse_tiktok_studio_eligibility(
	const QJsonObject &create_data, const QJsonObject &game_data,
	const QJsonObject &dual_data = {}, const QJsonObject &threshold_data = {});
