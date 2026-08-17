// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "aitum_bridge.hpp"
#include "manual_credentials_provider.hpp"
#include "profile.hpp"

#include <QWidget>

#include <vector>

class QComboBox;
class QFormLayout;
class QLabel;
class QVBoxLayout;

class LiveObsDock final : public QWidget {
public:
	explicit LiveObsDock(QWidget *obs_main_window);

private:
	Profile new_profile() const;
	Profile *selected_profile();
	void load_profiles();
	void save_profiles() const;
	void rebuild_profile_list();
	void show_selected_profile();
	void clear_layout(QLayout *layout);
	void add_profile();
	void delete_selected_profile();
	void save_manual_credentials(const QString &profile_id, QString username, QString url, QString key);
	void apply_to_aitum(const QString &profile_id, const QString &output_name);
	void set_diagnostic(Profile &profile, QString message, bool error = false);
	QString status_text(const Profile &profile) const;
	QStringList available_outputs(QString *diagnostic = nullptr) const;

	QVBoxLayout *profile_list_layout_ = nullptr;
	QVBoxLayout *details_layout_ = nullptr;
	std::vector<Profile> profiles_;
	int selected_profile_ = -1;
	AitumBridge aitum_bridge_;
	ManualCredentialsProvider manual_provider_;
};
