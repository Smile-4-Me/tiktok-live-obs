// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include "aitum_bridge.hpp"
#include "output_signing_manager.hpp"
#include "profile.hpp"
#include "provider_registry.hpp"
#include "providers/provider_session_router.hpp"
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
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QTimer;
class QWidget;
class QVBoxLayout;

// The dock coordinates account state, provider-neutral LIVE sessions, and the
// optional Aitum output bridge. Provider-specific UI and session paths live
// in dedicated implementation units so each responsibility stays small and
// reviewable.
class BridgeDock final : public QWidget {
public:
	explicit BridgeDock(QWidget *obs_main_window);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

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
	void update_detail_viewport_minimum();
	void rebalance_dock_height();
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
	void begin_tiktok_browser_session_import(const QString &profile_id, const QString &rapidapi_key);
	void complete_tiktok_studio_login(const QString &profile_id,
		const class TikTokStudioAccountCredentials &account, bool can_go_live,
		const QString &application_status);
	void refresh_tiktok_studio_account(const QString &profile_id, bool report_error = false);
	void add_tiktok_studio_account_controls(QFormLayout *form, const Profile &profile, QWidget *parent);
	[[nodiscard]] QString account_status_text(const Profile &profile) const;
	[[nodiscard]] QString rapidapi_quota_text(const RapidApiQuota &quota) const;
	QLabel *create_rapidapi_quota_field(const RapidApiQuota &quota, QWidget *parent) const;
	void update_rapidapi_quota_field(QLabel *field, const RapidApiQuota &quota) const;
	void save_local_credentials(const QString &profile_id, const QString &username, const QString &server,
		const QString &key);
	void refresh_selected_account();
	void show_transient_error(const QString &message);
	void set_diagnostic(Profile &profile, const QString &message, bool is_error = false);
	// Common failed-LIVE transition for every remote provider. Adapters decide
	// whether their response means missing LIVE access; the dock owns the UI
	// state change back to step 2.
	bool return_to_account_step_on_live_access_denied(Profile &profile,
		const class ProviderLifecycle &provider, const QString &error);
	[[nodiscard]] QString provider_error_message(const class ProviderLifecycle &provider,
		const QString &error) const;
	void clear_live_session(Profile &profile);
	// Provider-neutral handoff for a ready RTMP credential pair. Providers own
	// session creation; this method owns validation and the Aitum UI update.
	void update_aitum_output_for_profile(const QString &profile_id, const QString &output_name,
		const QString &server, const QString &key, AitumBridge::Completion completion);
	[[nodiscard]] QString aitum_bridge_result_message(BridgeResult result,
		const QString &output_name) const;
	void prepare_output_signing(const QString &profile_id, const QString &output_name,
		const QString &session_room_id, OutputSigningManager::Completion completion);
	// RapidAPI exposes usage through response headers. Keep that account-scoped
	// telemetry alongside the saved Studio login so paired outputs and profiles
	// never need an additional request just to render a quota value.
	void record_rapidapi_quota(const QString &profile_id, const RapidApiQuota &quota);
	void sync_rapidapi_quota_from_account(const QString &profile_id);
	void refresh_profile_ui(const QString &profile_id);
	void reconcile_previous_sessions();
	void load_profiles();
	void save_profiles() const;
	void delete_selected_profile();

	bool output_in_use_by_another_profile(const Profile &profile) const;
	void end_unstarted_aitum_session(const QString &profile_id, const QString &output_name);
	void verify_aitum_output_started(const QString &profile_id, const QString &output_name, int attempt);
	// Shared final Aitum path for every credential provider: start the named
	// output only after the provider-neutral credential handoff has completed,
	// then verify that Aitum reports the encoder as active.
	void start_aitum_output_and_verify(const QString &profile_id, const QString &output_name,
		std::function<void(const QString &)> on_start_failure);
	void start_aitum_output_and_verify_then(const QString &profile_id, const QString &output_name,
		std::function<void()> on_started, std::function<void(const QString &)> on_start_failure);
	void start_selected_live();
	void return_selected_manual_profile_to_credentials();
	void start_profile_live(const QString &profile_id, bool start_aitum_output);
	void start_tiktok_studio_live(const QString &profile_id, bool start_aitum_output);
	void create_tiktok_studio_live_session(const QString &profile_id, bool start_aitum_output);
	void resume_tiktok_studio_live(const QString &profile_id, bool start_aitum_output = false);
	void activate_tiktok_studio_live(const QString &profile_id, const QString &output_name,
		bool start_aitum_output, PreparedLive live);
	void prepare_tiktok_studio_output(const QString &profile_id, const QString &output_name,
		bool start_aitum_output, PreparedLive live);
	void prepare_tiktok_studio_dual_outputs(const QString &profile_id, const QString &primary_output,
		const QString &secondary_output, bool start_aitum_output, PreparedLive live);
	void prepare_tiktok_studio_main_output(const QString &profile_id, PreparedLive live);
	void fail_tiktok_studio_start(const QString &profile_id, const QString &output_name,
		bool start_aitum_output, PreparedLive live, const QString &reason);
	void run_tiktok_studio_heartbeats();
	void end_selected_live();
	void end_live_for_output(const QString &output_name);
	void end_profile_live(const QString &profile_id);

	QVBoxLayout *profile_list_layout_ = nullptr;
	QScrollArea *profile_scroll_ = nullptr;
	QPushButton *add_profile_button_ = nullptr;
	QScrollArea *detail_scroll_ = nullptr;
	QWidget *detail_container_ = nullptr;
	QVBoxLayout *detail_layout_ = nullptr;
	// The lowest control that must remain visible without scrolling. Step 3
	// assigns this to the End LIVE button; supplemental guidance and destructive
	// actions below it scroll instead of being clipped by a short dock.
	QWidget *detail_minimum_boundary_ = nullptr;
	int profile_list_content_height_ = 0;
	int detail_boundary_height_ = 0;
	int detail_content_height_ = 0;
	// Some nested Qt form controls resolve their wrapped height only after the
	// layout pass. This is the last measured profile-list height that still
	// left the complete detail view visible; -1 starts a fresh measurement.
	int measured_profile_height_cap_ = -1;
	// RapidAPI is the longest configuration. Once its Step 3 has been rendered,
	// keep that required height as the provider-neutral dock reference.
	int rapidapi_reference_boundary_height_ = 0;
	std::vector<Profile> profiles_;
	QSet<QString> outputs_preparing_;
	int selected_profile_ = -1;
	AitumBridge bridge_;
	StreamlabsClient streamlabs_;
	TikTokStudioClient tiktok_studio_;
	ProviderSessionRouter provider_sessions_;
	OutputSigningManager output_signing_;
	QTimer *tiktok_studio_heartbeat_timer_ = nullptr;
	QSet<QString> tiktok_studio_heartbeat_in_flight_;
	QSet<QString> tiktok_studio_heartbeat_failed_;
	QHash<QString, int> tiktok_studio_heartbeat_status_;
	QHash<QString, int> tiktok_studio_stale_heartbeat_count_;
	QHash<QString, quint64> tiktok_studio_account_generation_;
	// The currently rendered step 3 owns this field. It allows quota changes to
	// update in place without rebuilding the form while a LIVE is running.
	QHash<QString, QLabel *> rapidapi_quota_fields_;
};
