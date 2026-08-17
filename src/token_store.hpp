// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include <QString>

struct LiveCredentials {
	QString server;
	QString key;
};

class TokenStore final {
public:
	static void set_storage_scope(const QString &scope);
	static bool save(const QString &profile_id, const QString &token);
	static QString load(const QString &profile_id);
	static bool save_live_credentials(const QString &profile_id, const LiveCredentials &credentials);
	static LiveCredentials load_live_credentials(const QString &profile_id);
	static void remove_live_credentials(const QString &profile_id);
	static void remove(const QString &profile_id);
};
