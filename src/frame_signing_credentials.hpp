// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QString>

// Account-derived values required only by the encoded-frame metadata pipeline.
// They are never written to profile INI files; FrameSigningSettings persists
// them through TokenStore's encrypted platform backend.
struct FrameSigningCredentials {
	QString api_url = QStringLiteral("https://tiktok-live-studio-api-signer1.p.rapidapi.com/");
	QString rapidapi_key;
	QString uid;
	QString device_id;
	QString room_id;
	QString aid = QStringLiteral("8311");
};
