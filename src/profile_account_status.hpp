// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "profile.hpp"
#include "provider_contract.hpp"

// Maps the provider-neutral account contract to non-secret profile state.
// It has no UI, transport, or persistence policy and is safe to test alone.
namespace ProfileAccountStatus {

void apply(Profile &profile, const ProviderAccountStatus &status);

} // namespace ProfileAccountStatus
