// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "provider_registry.hpp"

#include <QSet>

#include <iostream>

int main()
{
	const QList<ProviderDefinition> &providers = ProviderRegistry::available();
	const QSet<QString> ids = [&providers] {
		QSet<QString> result;
		for (const ProviderDefinition &provider : providers)
			result.insert(provider.id);
		return result;
	}();

	if (providers.size() != ids.size() || ids.size() != 3) {
		std::cerr << "Provider IDs must be unique and complete.\n";
		return 1;
	}
	if (!ids.contains(ProviderRegistry::tiktok_studio_id()) ||
		!ids.contains(ProviderRegistry::streamlabs_id()) ||
		!ids.contains(ProviderRegistry::manual_id())) {
		std::cerr << "A required provider is missing from the registry.\n";
		return 2;
	}
	if (!ProviderRegistry::is_known(ProviderRegistry::tiktok_studio_id()) ||
		!ProviderRegistry::is_known(ProviderRegistry::streamlabs_id()) ||
		!ProviderRegistry::is_known(ProviderRegistry::manual_id()) ||
		ProviderRegistry::is_known(QStringLiteral("not-a-provider"))) {
		std::cerr << "Provider recognition is inconsistent.\n";
		return 6;
	}
	if (ProviderRegistry::uses_local_credentials(ProviderRegistry::tiktok_studio_id()) ||
		ProviderRegistry::uses_local_credentials(ProviderRegistry::streamlabs_id()) ||
		!ProviderRegistry::uses_local_credentials(ProviderRegistry::manual_id())) {
		std::cerr << "Credential ownership policy is inconsistent.\n";
		return 3;
	}
	if (!ProviderRegistry::is_tiktok_studio(ProviderRegistry::tiktok_studio_id()) ||
		ProviderRegistry::is_tiktok_studio(ProviderRegistry::streamlabs_id()) ||
		!ProviderRegistry::is_manual(ProviderRegistry::manual_id()) ||
		ProviderRegistry::is_manual(ProviderRegistry::streamlabs_id())) {
		std::cerr << "Provider classification is inconsistent.\n";
		return 4;
	}

	for (const ProviderDefinition &provider : providers) {
		if (provider.id.isEmpty() || provider.display_name_key.isEmpty() ||
			provider.fallback_display_name.isEmpty()) {
			std::cerr << "Provider definitions require a complete UI identity.\n";
			return 5;
		}
	}

	const ProviderDefinition *manual = ProviderRegistry::find(ProviderRegistry::manual_id());
	const ProviderDefinition *streamlabs = ProviderRegistry::find(ProviderRegistry::streamlabs_id());
	const ProviderDefinition *studio = ProviderRegistry::find(ProviderRegistry::tiktok_studio_id());
	if (!manual || !streamlabs || !studio ||
		manual->capabilities.session != ProviderSessionKind::LocalCredentials ||
		streamlabs->capabilities.session != ProviderSessionKind::RemoteSession ||
		studio->capabilities.authentication != ProviderAuthenticationKind::QrCode ||
		studio->capabilities.frame_signing != FrameSigningRequirement::Required ||
		streamlabs->capabilities.frame_signing != FrameSigningRequirement::Optional ||
		manual->capabilities.frame_signing != FrameSigningRequirement::Optional ||
		ProviderRegistry::frame_signing_requirement(QStringLiteral("not-a-provider")) !=
			FrameSigningRequirement::NotSupported) {
		std::cerr << "Provider capabilities do not describe the active implementations.\n";
		return 7;
	}

	std::cout << "Provider Registry smoke test passed.\n";
	return 0;
}
