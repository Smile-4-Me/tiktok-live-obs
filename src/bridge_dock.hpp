// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include "aitum_bridge.hpp"
#include "profile.hpp"
#include "provider_registry.hpp"
#include "streamlabs_client.hpp"

#include <QSet>
#include <QStringList>
#include <QWidget>

#include <optional>
#include <vector>

class QAbstractButton;
class QEvent;
class QFormLayout;
class QLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QWidget;
class QVBoxLayout;

// The dock coordinates account state, Streamlabs session lifecycle, and the
// optional Aitum output bridge. UI construction and session logic live in
// separate implementation units to keep reviewable responsibilities small.
class BridgeDock final : public QWidget {
public:
	explicit BridgeDock(QWidget *obs_main_window);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	enum class AitumToolbarAction { StartAll, StopAll };

	std::optional<AitumToolbarAction> aitum_toolbar_action_for_button(const QAbstractButton *button) const;
	void show_start_all_menu(QWidget *anchor);
	void start_all_outputs(bool stream_only);
	void start_all_recordings();
	void start_all_linked_outputs(const QStringList &outputs);
	void end_all_live_profiles();

	Profile new_profile(const QString &name) const;
	QLabel *info_card(const QString &content, QWidget *parent) const;
	QString output_name_for_aitum_button(const QAbstractButton *button) const;
	std::vector<Profile *> profiles_for_output(const QString &output_name, bool ready_only);
	Profile *live_profile_for_output(const QString &output_name);
	QString tiktok_account_id(const Profile &profile) const;
	const Profile *active_profile_for_tiktok_account(const Profile &profile) const;
	QString tiktok_account_conflict_message(const Profile &profile, const Profile &active) const;
	bool tiktok_account_in_use_by_another_profile(const Profile &profile) const;
	Profile *choose_profile_for_output(const QString &output_name, const std::vector<Profile *> &profiles);

	void build_ui();
	void clear_layout(QLayout *layout);
	void rebuild_profile_list();
	Profile *selected_profile();
	void show_selected_profile();
	Profile *find_profile(const QString &id);
	void build_login_step(const Profile &profile);
	void build_account_step(const Profile &profile);
	void build_stream_step(const Profile &profile);
	void add_live_status(QFormLayout *form, const Profile &profile, QWidget *parent);
	QWidget *live_credential_field(const QString &label, const QString &value, bool concealed, QWidget *parent);
	void show_aitum_missing_notice();

	void verify_token_for_profile(const QString &profile_id, const QString &token);
	void save_manual_credentials(const QString &profile_id, const QString &username, const QString &server,
		const QString &key);
	void refresh_selected_account();
	void show_transient_error(const QString &message);
	void set_diagnostic(Profile &profile, const QString &message, bool is_error = false);
	void clear_live_session(Profile &profile);
	void refresh_profile_ui(const QString &profile_id);
	void reconcile_previous_sessions();
	void load_profiles();
	void save_profiles() const;
	void delete_selected_profile();

	bool output_in_use_by_another_profile(const Profile &profile) const;
	void end_unstarted_aitum_session(const QString &profile_id, const QString &output_name);
	void verify_aitum_output_started(const QString &profile_id, const QString &output_name, int attempt);
	void start_selected_live();
	void start_profile_live(const QString &profile_id, bool start_aitum_output);
	void end_selected_live();
	void end_live_for_output(const QString &output_name);
	void end_profile_live(const QString &profile_id);

	QVBoxLayout *profile_list_layout_ = nullptr;
	QScrollArea *profile_scroll_ = nullptr;
	QPushButton *add_profile_button_ = nullptr;
	QWidget *detail_container_ = nullptr;
	QVBoxLayout *detail_layout_ = nullptr;
	std::vector<Profile> profiles_;
	QSet<QString> outputs_preparing_;
	int selected_profile_ = -1;
	AitumBridge bridge_;
	StreamlabsClient streamlabs_;
};
