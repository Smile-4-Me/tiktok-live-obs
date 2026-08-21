// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QString>
#include <QList>

struct ProviderDefinition {
	QString id;
	// The registry owns stable identifiers and translation keys. Rendering a
	// provider name is deliberately left to the dock's localization boundary.
	QString display_name_key;
	QString fallback_display_name;
	bool creates_live_session = false;
};

// The dock depends on provider identifiers, never on a provider's UI label.
// Adding a future authorized provider starts with one definition here, then a
// provider implementation behind the existing session lifecycle methods.
class ProviderRegistry final {
public:
	static const QString &streamlabs_id();
	static const QString &tiktok_studio_id();
	static const QString &manual_id();
	static const QString &research_id();
	static QList<ProviderDefinition> available();
	static bool is_known(const QString &provider_id);
	static bool is_manual(const QString &provider_id);
	static bool is_tiktok_studio(const QString &provider_id);
	static bool is_research(const QString &provider_id);
	static bool uses_local_credentials(const QString &provider_id);
};
