// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "provider_registry.hpp"

const QString &ProviderRegistry::streamlabs_id()
{
	static const QString id = QStringLiteral("streamlabs");
	return id;
}

const QString &ProviderRegistry::manual_id()
{
	static const QString id = QStringLiteral("manual");
	return id;
}

QList<ProviderDefinition> ProviderRegistry::available()
{
	return {{streamlabs_id(), QStringLiteral("Streamlabs"), true},
		{manual_id(), QStringLiteral("Manuell"), false}};
}

bool ProviderRegistry::is_manual(const QString &provider_id)
{
	return provider_id == manual_id();
}

QString ProviderRegistry::display_name(const QString &provider_id)
{
	for (const ProviderDefinition &provider : available())
		if (provider.id == provider_id)
			return provider.display_name;
	return QStringLiteral("Streamlabs");
}
