// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include "aitum_bridge.hpp"
#include "native_output_manager.hpp"
#include "output_signing_manager.hpp"
#include "profile.hpp"
#include "provider_registry.hpp"
#include "research_lab.hpp"
#include "streamlabs_client.hpp"
#include "tiktok_studio_client.hpp"

#include <QSet>
#include <QHash>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <optional>
#include <vector>

class QAbstractButton;
class QEvent;
class QFormLayout;
class QLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;
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
	void begin_tiktok_studio_login(const QString &profile_id, const QString &rapidapi_key);
	void refresh_tiktok_studio_account(const QString &profile_id, bool report_error = false);
	void disconnect_tiktok_studio_account(const QString &profile_id);
	void add_tiktok_studio_account_controls(QFormLayout *form, const Profile &profile, QWidget *parent);
	void save_local_credentials(const QString &profile_id, const QString &username, const QString &server,
		const QString &key);
	void refresh_selected_account();
	void show_transient_error(const QString &message);
	void set_diagnostic(Profile &profile, const QString &message, bool is_error = false);
	void clear_live_session(Profile &profile);
	void prepare_output_signing(const QString &profile_id, const QString &output_name,
		const QString &session_room_id, OutputSigningManager::Completion completion);
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
	void start_tiktok_studio_live(const QString &profile_id, bool start_aitum_output);
	void create_tiktok_studio_live_session(const QString &profile_id, bool start_aitum_output,
		TikTokStudioAccountCredentials account);
	void resume_tiktok_studio_live(const QString &profile_id, bool start_aitum_output = false);
	void activate_tiktok_studio_live(const QString &profile_id, const QString &output_name,
		bool start_aitum_output, TikTokStudioLive live);
	void prepare_tiktok_studio_output(const QString &profile_id, const QString &output_name,
		bool start_aitum_output, TikTokStudioLive live, bool aitum_available);
	void prepare_tiktok_studio_native_output(const QString &profile_id, TikTokStudioLive live);
	void verify_tiktok_studio_native_output(const QString &profile_id, int attempt);
	void fail_tiktok_studio_start(const QString &profile_id, const QString &output_name,
		bool start_aitum_output, TikTokStudioLive live, const QString &reason);
	void run_tiktok_studio_heartbeats();
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
	TikTokStudioClient tiktok_studio_;
	ResearchLab research_lab_;
	NativeOutputManager native_output_;
	OutputSigningManager output_signing_;
	QTimer *tiktok_studio_heartbeat_timer_ = nullptr;
	QSet<QString> tiktok_studio_heartbeat_in_flight_;
	QSet<QString> tiktok_studio_heartbeat_failed_;
	QHash<QString, int> tiktok_studio_heartbeat_status_;
	QHash<QString, int> tiktok_studio_stale_heartbeat_count_;
	QHash<QString, quint64> tiktok_studio_account_generation_;
};
