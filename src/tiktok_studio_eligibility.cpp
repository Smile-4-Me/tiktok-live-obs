// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "tiktok_studio_eligibility.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStringList>

#include <cmath>

namespace {

bool python_truthy(const QJsonValue &value)
{
	switch (value.type()) {
	case QJsonValue::Undefined:
	case QJsonValue::Null:
		return false;
	case QJsonValue::Bool:
		return value.toBool();
	case QJsonValue::Double:
		return value.toDouble() != 0.0;
	case QJsonValue::String:
		return !value.toString().isEmpty();
	case QJsonValue::Array:
		return !value.toArray().isEmpty();
	case QJsonValue::Object:
		return !value.toObject().isEmpty();
	}
	return false;
}

bool clear_block_status(const QJsonValue &value)
{
	if (value.isUndefined() || value.isNull())
		return true;
	if (value.isBool())
		return !value.toBool();
	if (value.isDouble())
		return value.toDouble() == 0.0;
	return value.isString() && value.toString() == QStringLiteral("0");
}

QString display_value(const QJsonValue &value)
{
	if (value.isString())
		return value.toString();
	if (value.isBool())
		return value.toBool() ? QStringLiteral("True") : QStringLiteral("False");
	if (value.isDouble()) {
		const double number = value.toDouble();
		if (std::trunc(number) == number)
			return QString::number(static_cast<qint64>(number));
		return QString::number(number, 'g', 15);
	}
	if (value.isArray())
		return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
	if (value.isObject())
		return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
	return QStringLiteral("None");
}

} // namespace

TikTokStudioEligibility parse_tiktok_studio_eligibility(
	const QJsonObject &create_data, const QJsonObject &game_data,
	const QJsonObject &dual_data, const QJsonObject &threshold_data)
{
	const bool locale_restricted = python_truthy(
		create_data.value(QStringLiteral("golive_locale_restricted")));
	const QJsonObject ban = create_data.value(QStringLiteral("ban_status")).toObject();
	const QJsonObject advanced_ban = create_data.value(
		QStringLiteral("advanced_live_ban_status")).toObject();
	const bool banned = python_truthy(ban.value(QStringLiteral("is_ban"))) ||
		python_truthy(advanced_ban.value(QStringLiteral("is_ban")));
	const QJsonValue block_status = create_data.value(QStringLiteral("block_status"));
	const bool blocked = !clear_block_status(block_status);
	const bool studio_login = python_truthy(
		game_data.value(QStringLiteral("has_live_studio_login")));

	TikTokStudioEligibility result;
	const bool explicitly_restricted = locale_restricted || banned || blocked || !studio_login;
	result.can_go_live = !explicitly_restricted;
	QStringList status{explicitly_restricted ? QStringLiteral("Restricted")
		: QStringLiteral("live_access_unknown")};
	if (banned)
		status.push_back(QStringLiteral("Banned"));
	if (blocked)
		status.push_back(QStringLiteral("Blocked %1").arg(display_value(block_status)));
	if (locale_restricted)
		status.push_back(QStringLiteral("Locale restricted"));
	result.status = status.join(QStringLiteral(" / "));
	const bool supports_multi_stream = python_truthy(
		dual_data.value(QStringLiteral("allow_multi_stream")));
	const bool threshold_allowed = python_truthy(threshold_data.value(QStringLiteral("allowed")));
	const bool previously_used = python_truthy(threshold_data.value(QStringLiteral("dual_canvas_used")));
	const QString days_to_reach = display_value(
		threshold_data.value(QStringLiteral("days_to_reach"))).trimmed();
	bool has_remaining_days = false;
	const qint64 remaining_days = days_to_reach.toLongLong(&has_remaining_days);
	has_remaining_days = has_remaining_days && remaining_days > 0;
	// TikTok LIVE Studio unlocks the scene=1 feature only after its threshold
	// permits it, or after the account has already used a dual canvas.
	result.dual_layout_available = supports_multi_stream && (threshold_allowed || previously_used);
	if (result.dual_layout_available) {
		result.dual_layout_status = QStringLiteral("available");
	} else if (has_remaining_days) {
		// `days_to_reach` is TikTok's direct progress value for the LIVE Studio
		// prerequisite. Preserve it even while the scene=1 account gate remains
		// false: the UI can then explain the concrete next step instead of hiding
		// the progress behind a generic account-level status.
		result.dual_layout_status = QStringLiteral("locked:%1").arg(remaining_days);
	} else if (!supports_multi_stream) {
		result.dual_layout_status = QStringLiteral("not_available");
	} else if (!threshold_data.isEmpty()) {
		result.dual_layout_status = QStringLiteral("locked");
	} else {
		result.dual_layout_status = QStringLiteral("unknown");
	}
	return result;
}
