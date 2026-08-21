// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include <QString>

// Reads only the locally stored Streamlabs Desktop API token when the user
// explicitly requests an import from this machine.
QString find_streamlabs_desktop_token();
