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

const QString &ProviderRegistry::research_id()
{
	static const QString id = QStringLiteral("research-local");
	return id;
}

QList<ProviderDefinition> ProviderRegistry::available()
{
	return {{streamlabs_id(), QStringLiteral("Streamlabs"), true},
		{manual_id(), QStringLiteral("Manuell"), false},
		{research_id(), QStringLiteral("Research Lab (localhost)"), false}};
}

bool ProviderRegistry::is_manual(const QString &provider_id)
{
	return provider_id == manual_id();
}

bool ProviderRegistry::is_research(const QString &provider_id)
{
	return provider_id == research_id();
}

bool ProviderRegistry::uses_local_credentials(const QString &provider_id)
{
	return is_manual(provider_id) || is_research(provider_id);
}

QString ProviderRegistry::display_name(const QString &provider_id)
{
	for (const ProviderDefinition &provider : available())
		if (provider.id == provider_id)
			return provider.display_name;
	return QStringLiteral("Streamlabs");
}
