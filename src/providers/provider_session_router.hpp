// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "providers/manual_provider.hpp"
#include "providers/streamlabs_provider.hpp"
#include "providers/tiktok_studio_provider.hpp"

class StreamlabsClient;
class TikTokStudioClient;

// Resolves a stable provider id once. BridgeDock uses this router rather than
// branching on concrete provider clients throughout its session flow.
class ProviderSessionRouter final {
public:
	ProviderSessionRouter(StreamlabsClient &streamlabs, TikTokStudioClient &studio);

	[[nodiscard]] ProviderLifecycle *find(const QString &provider_id);
	[[nodiscard]] const ProviderLifecycle *find(const QString &provider_id) const;

private:
	ManualProvider manual_;
	StreamlabsProvider streamlabs_;
	TikTokStudioProvider studio_;
};
