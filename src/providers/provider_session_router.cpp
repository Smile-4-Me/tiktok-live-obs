// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "providers/provider_session_router.hpp"

#include "provider_registry.hpp"

#include <utility>

ProviderSessionRouter::ProviderSessionRouter(StreamlabsClient &streamlabs, TikTokStudioClient &studio)
	: streamlabs_(streamlabs), studio_(studio)
{
}

ProviderLifecycle *ProviderSessionRouter::find(const QString &provider_id)
{
	return const_cast<ProviderLifecycle *>(std::as_const(*this).find(provider_id));
}

const ProviderLifecycle *ProviderSessionRouter::find(const QString &provider_id) const
{
	if (provider_id == ProviderRegistry::manual_id())
		return &manual_;
	if (provider_id == ProviderRegistry::streamlabs_id())
		return &streamlabs_;
	if (provider_id == ProviderRegistry::tiktok_studio_id())
		return &studio_;
	return nullptr;
}
