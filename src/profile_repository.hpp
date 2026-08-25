// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "profile.hpp"

#include <vector>

// Owns the persisted, non-secret profile document. Runtime UI state remains in
// BridgeDock; credentials continue to live in TokenStore rather than the INI.
class ProfileRepository final {
public:
	[[nodiscard]] static std::vector<Profile> load();
	static void save(const std::vector<Profile> &profiles);
};
