// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "bridge_dock.hpp"

#include "aitum_outputs.hpp"
#include "localization.hpp"
#include "profile_live_session.hpp"

#include <QTimer>

#include <utility>

// This file contains the provider-neutral Aitum output lifecycle. Providers
// only supply a PreparedLive; all output reservation, verification, and safe
// rollback rules remain identical regardless of its origin.

bool BridgeDock::output_in_use_by_another_profile(const Profile &profile) const
{
	const bool credential_only_target =
		profile.output_name.isEmpty() || !aitum_stream_suite_available();
	for (const Profile &candidate : profiles_) {
		if (candidate.id == profile.id ||
			(!candidate.live && !candidate.preparing && !candidate.session_uncertain))
			continue;
		const bool candidate_credential_only_target =
			candidate.output_name.isEmpty() || !aitum_stream_suite_available();
		if ((credential_only_target && candidate_credential_only_target) ||
			(!credential_only_target && !candidate_credential_only_target &&
				candidate.output_name == profile.output_name))
			return true;
	}
	return false;
}

void BridgeDock::end_unstarted_aitum_session(const QString &profile_id, const QString &output_name)
{
	Profile *profile = find_profile(profile_id);
	if (!profile || (!profile->live && !profile->preparing) ||
		(!ProviderRegistry::uses_local_credentials(profile->provider_id) && profile->live_id.isEmpty())) {
		outputs_preparing_.remove(output_name);
		return;
	}
	ProviderLifecycle *provider = provider_sessions_.find(profile->provider_id);
	if (!provider) {
		outputs_preparing_.remove(output_name);
		show_transient_error(text("Error.MissingToken"));
		return;
	}
	const ProviderAccountReference account{profile->id, profile->account_id};
	const PreparedLive live{.session_id = profile->live_id, .stream_id = profile->stream_id,
		.room_id = profile->live_id, .server = profile->stream_server, .key = profile->stream_key};
	profile->ending = true;
	profile->diagnostic = text("Diagnostic.OutputNotActiveEnding");
	profile->diagnostic_error = true;
	save_profiles();
	refresh_profile_ui(profile_id);
	provider->end_live(account, live, [this, profile_id, output_name](ProviderEndResult result) {
		outputs_preparing_.remove(output_name);
		Profile *current = find_profile(profile_id);
		if (!current)
			return;
		current->ending = false;
		if (result.ended || result.stale_session) {
			clear_live_session(*current);
			current->diagnostic = text("Diagnostic.OutputNotActiveEnded");
			current->diagnostic_error = true;
			save_profiles();
			refresh_profile_ui(profile_id);
			show_transient_error(text("Error.AitumOutputNotActive").arg(output_name));
			return;
		}
		current->diagnostic = text("Diagnostic.Failed").arg(
			text("Error.AitumOutputNotActiveEndFailed").arg(output_name, result.error));
		current->diagnostic_error = true;
		save_profiles();
		refresh_profile_ui(profile_id);
		show_transient_error(text("Error.AitumOutputNotActiveEndFailed").arg(output_name, result.error));
	});
}

void BridgeDock::verify_aitum_output_started(const QString &profile_id, const QString &output_name, int attempt)
{
	Profile *profile = find_profile(profile_id);
	// A TikTok room can be prepared before Aitum has actually started its
	// encoder. Treat that as pending, never as a confirmed LIVE stream.
	if (!profile || (!profile->live && !profile->preparing) || profile->ending) {
		outputs_preparing_.remove(output_name);
		return;
	}

	bool active = false;
	QString diagnostic;
	const bool status_available = aitum_output_is_active(output_name, &active, &diagnostic);
	if (status_available && active) {
		outputs_preparing_.remove(output_name);
		profile->live = true;
		profile->preparing = false;
		profile->session_uncertain = false;
		profile->diagnostic = text("Diagnostic.OutputStarted");
		profile->diagnostic_error = false;
		save_profiles();
		refresh_profile_ui(profile_id);
		return;
	}

	constexpr int maximum_attempts = 20;
	if (attempt + 1 < maximum_attempts) {
		QTimer::singleShot(500, this, [this, profile_id, output_name, attempt] {
			verify_aitum_output_started(profile_id, output_name, attempt + 1);
		});
		return;
	}

	// An accepted vendor request is not proof that the encoder started. Do not
	// leave a TikTok LIVE reservation behind when Aitum/OBS never became active.
	end_unstarted_aitum_session(profile_id, output_name);
}

void BridgeDock::start_aitum_output_and_verify(const QString &profile_id, const QString &output_name,
	std::function<void(const QString &)> on_start_failure)
{
	QTimer::singleShot(250, this, [this, profile_id, output_name,
		on_start_failure = std::move(on_start_failure)]() mutable {
		QString diagnostic;
		if (!aitum_start_output(output_name, &diagnostic)) {
			outputs_preparing_.remove(output_name);
			if (on_start_failure)
				on_start_failure(diagnostic);
			return;
		}
		if (Profile *profile = find_profile(profile_id)) {
			profile->diagnostic = text("Diagnostic.VerifyingOutput");
			profile->diagnostic_error = false;
			save_profiles();
			refresh_profile_ui(profile_id);
		}
		verify_aitum_output_started(profile_id, output_name, 0);
	});
}
