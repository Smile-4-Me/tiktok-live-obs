// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QString>

struct SessionDescriptor {
	QString provider_name;
	QString creator_username;
	QString ingest_url;
	QString stream_key;
};

// Provider contract deliberately contains no TikTok-specific protocol or
// transport logic. Official or partner integrations can be added later
// without changing the dock, profile persistence, or Aitum bridge.
class LiveSessionProvider {
public:
	virtual ~LiveSessionProvider() = default;
	virtual QString identifier() const = 0;
	virtual QString display_name() const = 0;
	virtual bool is_configured(const QString &profile_id) const = 0;
	virtual SessionDescriptor session_for(const QString &profile_id, QString *error) const = 0;
};
