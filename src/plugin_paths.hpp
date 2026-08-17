// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include <QString>

// Files are scoped to the physical OBS installation. This keeps portable OBS
// instances independent while allowing an update at the same path to retain data.
QString module_installation_directory();
QString installation_storage_scope();
QString profiles_settings_path();
QString plugin_settings_path();
