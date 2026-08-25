// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "providers/provider_lifecycle.hpp"

namespace {

QString unsupported_operation_error()
{
	return QStringLiteral("This provider does not support this operation.");
}

} // namespace

bool ProviderLifecycle::is_live_access_denied_error(const QString &error) const
{
	// Keep the shared fallback useful for future provider adapters as well. A
	// provider can still override this with a stricter, protocol-aware check.
	const QString message = error.trimmed().toLower();
	return message.contains(QStringLiteral("no live auth")) ||
		message.contains(QStringLiteral("live authorization missing")) ||
		message.contains(QStringLiteral("live access denied")) ||
		message.contains(QStringLiteral("live permission denied")) ||
		message.contains(QStringLiteral("not authorized to go live")) ||
		message.contains(QStringLiteral("tiktok status 20800"));
}

void ProviderLifecycle::find_continuable_live(const ProviderAccountReference & /*account*/,
	LiveCallback completion)
{
	completion({}, unsupported_operation_error());
}

void ProviderLifecycle::resume_live(const ProviderAccountReference & /*account*/, LiveCallback completion)
{
	completion({}, unsupported_operation_error());
}

void ProviderLifecycle::fetch_game_tags(const ProviderAccountReference & /*account*/,
	CatalogCallback completion)
{
	completion({}, unsupported_operation_error());
}

void ProviderLifecycle::search_categories(const ProviderAccountReference & /*account*/,
	const QString & /*query*/, CatalogCallback completion)
{
	completion({}, unsupported_operation_error());
}

void ProviderLifecycle::heartbeat(const ProviderAccountReference & /*account*/, const PreparedLive & /*live*/,
	int /*status*/, HeartbeatCallback completion)
{
	completion({.error = unsupported_operation_error()});
}
