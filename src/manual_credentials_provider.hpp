// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "live_session_provider.hpp"

class ManualCredentialsProvider final : public LiveSessionProvider {
public:
	QString identifier() const override;
	QString display_name() const override;
	bool is_configured(const QString &profile_id) const override;
	SessionDescriptor session_for(const QString &profile_id, QString *error) const override;

	bool save(const QString &profile_id, const QString &username, const QString &url,
		const QString &key, QString *error) const;
	void remove(const QString &profile_id) const;
};
