// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "provider_registry.hpp"

const QString &ProviderRegistry::streamlabs_id()
{
	static const QString id = QStringLiteral("streamlabs");
	return id;
}

const QString &ProviderRegistry::tiktok_studio_id()
{
	static const QString id = QStringLiteral("tiktok-studio");
	return id;
}

const QString &ProviderRegistry::manual_id()
{
	static const QString id = QStringLiteral("manual");
	return id;
}

const QList<ProviderDefinition> &ProviderRegistry::available()
{
	static const QList<ProviderDefinition> providers{
		{tiktok_studio_id(), QStringLiteral("Provider.TikTokStudio"), QStringLiteral("RapidAPI"),
			{ProviderAuthenticationKind::QrCode, ProviderSessionKind::RemoteSession,
				true, true, true, true, true, FrameSigningRequirement::Required}},
		{streamlabs_id(), QStringLiteral("Provider.Streamlabs"), QStringLiteral("Streamlabs"),
			{ProviderAuthenticationKind::BrowserToken, ProviderSessionKind::RemoteSession,
				true, true, false, false, true, FrameSigningRequirement::Optional}},
		{manual_id(), QStringLiteral("Provider.Manual"), QStringLiteral("Manual"),
			{ProviderAuthenticationKind::ManualCredentials, ProviderSessionKind::LocalCredentials,
				false, false, false, false, false, FrameSigningRequirement::Optional}},
	};
	return providers;
}

const ProviderDefinition *ProviderRegistry::find(const QString &provider_id)
{
	for (const ProviderDefinition &provider : available()) {
		if (provider.id == provider_id)
			return &provider;
	}
	return nullptr;
}

bool ProviderRegistry::is_known(const QString &provider_id)
{
	return find(provider_id) != nullptr;
}

bool ProviderRegistry::is_tiktok_studio(const QString &provider_id)
{
	return provider_id == tiktok_studio_id();
}

bool ProviderRegistry::is_manual(const QString &provider_id)
{
	return provider_id == manual_id();
}

bool ProviderRegistry::uses_local_credentials(const QString &provider_id)
{
	const ProviderDefinition *provider = find(provider_id);
	return provider && provider->capabilities.session == ProviderSessionKind::LocalCredentials;
}

bool ProviderRegistry::supports_main_obs_output(const QString &provider_id)
{
	const ProviderDefinition *provider = find(provider_id);
	return provider && provider->capabilities.supports_main_obs_output;
}

FrameSigningRequirement ProviderRegistry::frame_signing_requirement(const QString &provider_id)
{
	const ProviderDefinition *provider = find(provider_id);
	return provider ? provider->capabilities.frame_signing : FrameSigningRequirement::NotSupported;
}
