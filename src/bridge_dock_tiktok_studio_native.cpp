// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "bridge_dock.hpp"

#include "aitum_outputs.hpp"
#include "frame_signing_settings.hpp"
#include "localization.hpp"
#include "profile_live_session.hpp"
#include "tiktok_studio_session.hpp"
#include "tiktok_studio_topics.hpp"
#include "token_store.hpp"

#include <QTimer>

#include <utility>

namespace {

QString with_secure_storage_error(const QString &message)
{
	const QString detail = TokenStore::last_error().trimmed();
	return detail.isEmpty() ? message : message + QStringLiteral("\n\n") + detail;
}

} // namespace


void BridgeDock::start_tiktok_studio_live(const QString &profile_id, bool start_aitum_output)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile || !ProviderRegistry::is_tiktok_studio(profile->provider_id))
			return;
		ProviderLifecycle *provider = provider_sessions_.find(profile->provider_id);
		if (!provider) {
			if (start_aitum_output)
				outputs_preparing_.remove(profile->output_name);
			show_transient_error(text("Error.MissingToken"));
			return;
		}
		if (profile->hashtag_id.trimmed().isEmpty()) {
			if (start_aitum_output)
				outputs_preparing_.remove(profile->output_name);
			show_transient_error(translated_or("Studio.Stream.TopicRequired",
				QStringLiteral("Please select a topic.")));
			return;
		}
		if (tiktok_studio_topic_is_gaming(profile->hashtag_id) && profile->category_id.trimmed().isEmpty()) {
			if (start_aitum_output)
				outputs_preparing_.remove(profile->output_name);
			show_transient_error(translated_or("Studio.Stream.GameRequired",
				QStringLiteral("Choose a game for the Gaming topic.")));
			return;
		}
		profile->preparing = true;
		profile->ending = false;
		profile->session_uncertain = false;
		profile->diagnostic = translated_or("Studio.Recovery.Checking", QStringLiteral(
			"Checking for an existing TikTok LIVE …"));
		profile->diagnostic_error = false;
		const QString output_name = profile->output_name;
		save_profiles();
		refresh_profile_ui(profile_id);

		const ProviderAccountReference provider_account{profile->id, profile->account_id};
		provider->find_continuable_live(provider_account,
			[this, profile_id, output_name, start_aitum_output](PreparedLive live, QString error) mutable {
				Profile *current = find_profile(profile_id);
				if (!current) {
					outputs_preparing_.remove(output_name);
					return;
				}

				if (!live.room_id.isEmpty() && !live.stream_id.isEmpty()) {
					current->preparing = false;
					// A reusable TikTok room is not evidence that OBS is currently
					// streaming. Keep it as an unresolved session until the user
					// resumes or ends it.
					ProfileLiveSession::mark_uncertain(*current, live);
					current->live = false;
					const bool credentials_saved = live.server.isEmpty() || live.key.isEmpty() ||
						TokenStore::save_live_credentials(profile_id, {live.server, live.key});
					if (error.isEmpty() && credentials_saved) {
						current->diagnostic = translated_or("Studio.Recovery.Available", QStringLiteral(
							"TikTok already has an active LIVE for this login. Choose Resume TikTok LIVE to reconnect OBS, or End TikTok LIVE to finish it."));
						current->diagnostic_error = false;
					} else {
						const QString reason = !credentials_saved
							? with_secure_storage_error(translated_or("Studio.Account.SaveFailed",
								QStringLiteral("The existing LIVE credentials could not be saved securely.")))
							: error;
						current->diagnostic = translated_or("Studio.Recovery.Incomplete", QStringLiteral(
							"TikTok reports an existing LIVE, but OBS cannot resume it yet: %1. You can still end it or reset local state.")).arg(reason);
						current->diagnostic_error = true;
					}
					outputs_preparing_.remove(output_name);
					save_profiles();
					refresh_profile_ui(profile_id);
					return;
				}

				if (!error.isEmpty()) {
					current->preparing = false;
					current->diagnostic = text("Diagnostic.Failed").arg(error);
					current->diagnostic_error = true;
					outputs_preparing_.remove(output_name);
					save_profiles();
					refresh_profile_ui(profile_id);
					show_transient_error(error);
					return;
				}
				create_tiktok_studio_live_session(profile_id, start_aitum_output);
			});
	}

void BridgeDock::create_tiktok_studio_live_session(const QString &profile_id,
	bool start_aitum_output)
{
	Profile *profile = find_profile(profile_id);
	if (!profile)
		return;
	const QString output_name = profile->output_name;
	const QString hashtag_id = profile->hashtag_id;
	const QString game_tag_id = tiktok_studio_topic_is_gaming(hashtag_id)
		? profile->category_id : QStringLiteral("0");
	ProviderLifecycle *provider = provider_sessions_.find(profile->provider_id);
	if (!provider) {
		profile->preparing = false;
		profile->diagnostic = text("Diagnostic.Failed").arg(text("Error.MissingToken"));
		profile->diagnostic_error = true;
		outputs_preparing_.remove(output_name);
		save_profiles();
		refresh_profile_ui(profile_id);
		show_transient_error(text("Error.MissingToken"));
		return;
	}
	profile->diagnostic = text("Diagnostic.CreatingSession");
	profile->diagnostic_error = false;
	save_profiles();
	refresh_profile_ui(profile_id);

	const ProviderAccountReference provider_account{profile->id, profile->account_id};
	const LiveRequest request{.title = profile->stream_title, .topic_id = hashtag_id,
		.category_id = game_tag_id, .mature = profile->mature};
	provider->create_live(provider_account, request,
		[this, profile_id, output_name, start_aitum_output](PreparedLive live, QString error) mutable {
			Profile *current = find_profile(profile_id);
			if (!current) {
				outputs_preparing_.remove(output_name);
				return;
			}
			if (!error.isEmpty()) {
				QString visible_error = error;
				if (ProviderLifecycle *error_provider = provider_sessions_.find(current->provider_id)) {
					return_to_account_step_on_live_access_denied(*current, *error_provider, error);
					visible_error = provider_error_message(*error_provider, error);
				}
				if (!live.room_id.isEmpty() && !live.stream_id.isEmpty()) {
					current->live = false;
					ProfileLiveSession::reserve(*current, live);
					current->live = false;
					tiktok_studio_heartbeat_status_.insert(profile_id, 1);
					tiktok_studio_stale_heartbeat_count_.insert(profile_id, 0);
					fail_tiktok_studio_start(profile_id, output_name, start_aitum_output,
						std::move(live), visible_error);
					return;
				}
				current->preparing = false;
				current->diagnostic = text("Diagnostic.Failed").arg(visible_error);
				current->diagnostic_error = true;
				outputs_preparing_.remove(output_name);
				save_profiles();
				refresh_profile_ui(profile_id);
				show_transient_error(visible_error);
				return;
			}
			activate_tiktok_studio_live(profile_id, output_name, start_aitum_output,
				std::move(live));
		});
}

void BridgeDock::resume_tiktok_studio_live(const QString &profile_id, bool start_aitum_output)
{
	Profile *profile = find_profile(profile_id);
	if (!profile || !ProviderRegistry::is_tiktok_studio(profile->provider_id) ||
		(!profile->live && !profile->session_uncertain) || profile->preparing || profile->ending || profile->recovering)
		return;
	ProviderLifecycle *provider = provider_sessions_.find(profile->provider_id);
	if (!provider) {
		show_transient_error(text("Error.MissingToken"));
		return;
	}
	const QString output_name = profile->output_name;
	profile->preparing = true;
	profile->recovering = true;
	profile->diagnostic = translated_or("Studio.Recovery.Resuming",
		QStringLiteral("Reconnecting OBS to the existing TikTok LIVE …"));
	profile->diagnostic_error = false;
	save_profiles();
	refresh_profile_ui(profile_id);

	const ProviderAccountReference provider_account{profile->id, profile->account_id};
	provider->resume_live(provider_account,
		[this, profile_id, output_name, start_aitum_output](PreparedLive live, QString error) mutable {
			Profile *current = find_profile(profile_id);
			if (!current)
				return;
			current->recovering = false;
			if (!error.isEmpty()) {
				current->preparing = false;
				if (!live.room_id.isEmpty()) {
					ProfileLiveSession::mark_uncertain(*current, live);
					current->live = false;
				} else {
					current->session_uncertain = true;
				}
				current->diagnostic = text("Diagnostic.Failed").arg(error);
				current->diagnostic_error = true;
				outputs_preparing_.remove(output_name);
				save_profiles();
				refresh_profile_ui(profile_id);
				show_transient_error(error);
				return;
			}
			activate_tiktok_studio_live(profile_id, output_name, start_aitum_output,
				std::move(live));
		});
}

void BridgeDock::activate_tiktok_studio_live(const QString &profile_id, const QString &output_name,
	bool start_aitum_output, PreparedLive live)
{
	Profile *current = find_profile(profile_id);
	if (!current) {
		outputs_preparing_.remove(output_name);
		return;
	}
	// Creating a TikTok room reserves credentials, but the stream is only live
	// after the selected output reports that its encoder actually started.
	ProfileLiveSession::reserve(*current, live);
	current->live = false;
	current->preparing = true;
	tiktok_studio_heartbeat_status_.insert(profile_id, 1);
	tiktok_studio_stale_heartbeat_count_.insert(profile_id, 0);
	current->diagnostic = output_name.isEmpty() || !aitum_stream_suite_available()
		? translated_or("Signing.Prefetching", QStringLiteral("Prefetching frame signatures …"))
		: text("Diagnostic.UpdatingAitum");
	current->diagnostic_error = false;
	TokenStore::save_live_credentials(profile_id, {live.server, live.key});

	QString signing_error;
	if (!FrameSigningSettings::synchronize_tiktok_studio_account(profile_id,
		current->account_id, &signing_error)) {
		fail_tiktok_studio_start(profile_id, output_name, start_aitum_output,
			std::move(live), translated_or("Studio.Account.SaveFailed",
				QStringLiteral("The updated TikTok session could not be saved securely.")) +
			(signing_error.isEmpty() ? QString{} : QStringLiteral("\n\n") + signing_error));
		return;
	}
	save_profiles();
	refresh_profile_ui(profile_id);

	const bool aitum_available = aitum_stream_suite_available();
	if (output_name.isEmpty()) {
		prepare_tiktok_studio_main_output(profile_id, std::move(live));
		return;
	}
	if (!aitum_available) {
		fail_tiktok_studio_start(profile_id, output_name, start_aitum_output,
			std::move(live), translated_or("Studio.Stream.AitumMissing", QStringLiteral(
				"The selected Aitum output is no longer available. Choose another Aitum output or generate credentials for manual use.")));
		return;
	}
	if (live.server.trimmed().isEmpty() || live.key.trimmed().isEmpty()) {
		// The provider/session layer must return usable RTMP credentials before
		// the provider-neutral Aitum bridge is allowed to update an output.
		fail_tiktok_studio_start(profile_id, output_name, start_aitum_output,
			std::move(live), translated_or("Studio.Stream.CredentialsMissing", QStringLiteral(
				"TikTok did not return a stream URL and key for this LIVE. Try creating the LIVE again.")));
		return;
	}

	// Keep the RTMP pair separate from the move-only callback capture. C++ does
	// not guarantee argument evaluation order, so passing live.server/live.key
	// beside `live = std::move(live)` can hand empty values to Aitum.
	const QString aitum_server = live.server;
	const QString aitum_key = live.key;
	update_aitum_output_for_profile(profile_id, output_name, aitum_server, aitum_key,
		[this, profile_id, output_name, start_aitum_output, live = std::move(live)]
		(BridgeResult result) mutable {
			if (result != BridgeResult::Success) {
				const QString reason = aitum_bridge_result_message(result, output_name);
				fail_tiktok_studio_start(profile_id, output_name, start_aitum_output,
					std::move(live), reason);
				return;
			}
			prepare_tiktok_studio_output(profile_id, output_name,
				start_aitum_output, std::move(live));
		});
}

void BridgeDock::prepare_tiktok_studio_main_output(const QString &profile_id, PreparedLive live)
{
	Profile *profile = find_profile(profile_id);
	if (!profile)
		return;

	// The credential-only target uses the same frame-signing preparation as a
	// named Aitum output, but it does not start or configure an OBS output.
	prepare_output_signing(profile_id, QString{}, live.room_id,
		[this, profile_id, live = std::move(live)](bool attached, QString signing_error) mutable {
			Profile *current = find_profile(profile_id);
			if (!current) {
				output_signing_.detach(QString{});
				return;
			}
			if (!attached) {
				fail_tiktok_studio_start(profile_id, QString{}, false,
					std::move(live), signing_error);
				return;
			}
			// A credential-only target still owns an active TikTok LIVE session.
			// Mark it live once its required signing setup is ready so the dock
			// shows the generated URL/key and exposes the normal End LIVE action.
			current->live = true;
			current->preparing = false;
			current->diagnostic = text("Diagnostic.SessionReady");
			current->diagnostic_error = false;
			save_profiles();
			refresh_profile_ui(profile_id);
		});
}

	void BridgeDock::prepare_tiktok_studio_output(const QString &profile_id, const QString &output_name,
		bool start_aitum_output, PreparedLive live)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile) {
			outputs_preparing_.remove(output_name);
			return;
		}
		// Aitum outputs use the same path as every other provider: credentials
		// have already been stored through the common bridge, so start and verify
		// the selected output directly. Provider-specific preparation does not
		// belong between those two shared operations.
		profile->preparing = start_aitum_output;
		profile->diagnostic = start_aitum_output
			? text("Diagnostic.StartingOutput")
			: text("Diagnostic.SessionReady");
		profile->diagnostic_error = false;
		save_profiles();
		refresh_profile_ui(profile_id);
		if (!start_aitum_output) {
			profile->live = true;
			profile->preparing = false;
			outputs_preparing_.remove(output_name);
			save_profiles();
			refresh_profile_ui(profile_id);
			return;
		}
		start_aitum_output_and_verify(profile_id, output_name,
			[this, profile_id, output_name](const QString &diagnostic) {
				if (Profile *current = find_profile(profile_id)) {
					PreparedLive failed_live{.session_id = current->live_id, .stream_id = current->stream_id,
						.room_id = current->live_id, .server = current->stream_server,
						.key = current->stream_key};
					fail_tiktok_studio_start(profile_id, output_name, true,
						std::move(failed_live), text("OneClick.StartFailed").arg(diagnostic));
				}
			});
	}

	void BridgeDock::fail_tiktok_studio_start(const QString &profile_id, const QString &output_name,
		bool start_aitum_output, PreparedLive live, const QString &reason)
	{
		output_signing_.detach(output_name);
		Profile *profile = find_profile(profile_id);
		if (profile) {
			profile->preparing = false;
			profile->ending = true;
			profile->diagnostic = text("Diagnostic.Failed").arg(reason);
			profile->diagnostic_error = true;
			save_profiles();
			refresh_profile_ui(profile_id);
		}
		show_transient_error(reason);
		if (!profile)
			return;
		ProviderLifecycle *provider = provider_sessions_.find(profile->provider_id);
		if (!provider) {
			profile->ending = false;
			profile->session_uncertain = true;
			save_profiles();
			refresh_profile_ui(profile_id);
			return;
		}
		const ProviderAccountReference account{profile->id, profile->account_id};
		provider->end_live(account, live,
			[this, profile_id, output_name, start_aitum_output, reason](ProviderEndResult result) {
				if (start_aitum_output)
					outputs_preparing_.remove(output_name);
				Profile *current = find_profile(profile_id);
				if (!current)
					return;
				current->ending = false;
				if (result.ended || result.stale_session) {
					clear_live_session(*current);
				} else {
					current->live = false;
					current->session_uncertain = true;
				}
				current->diagnostic = text("Diagnostic.Failed").arg(result.error.isEmpty()
					? reason : QStringLiteral("%1 (%2)").arg(reason, result.error));
				current->diagnostic_error = true;
				save_profiles();
				refresh_profile_ui(profile_id);
			});
	}
