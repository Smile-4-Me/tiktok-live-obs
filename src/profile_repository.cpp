// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "profile_repository.hpp"

#include "plugin_paths.hpp"
#include "provider_registry.hpp"
#include "token_store.hpp"

#include <QSettings>
#include <QUuid>

namespace {

Profile read_profile(QSettings &settings, bool &migrated_account_ids, bool &migrated_topic_ids,
	bool &migrated_signing_policy, bool &removed_legacy_native_output_mode)
{
	Profile profile;
	profile.id = settings.value(QStringLiteral("id")).toString();
	profile.account_id = settings.value(QStringLiteral("account_id")).toString();
	if (profile.account_id.isEmpty()) {
		profile.account_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
		migrated_account_ids = true;
	}

	profile.provider_id = settings.value(QStringLiteral("provider_id"),
		ProviderRegistry::tiktok_studio_id()).toString();
	// Providers may disappear in a later build. Preserve a usable profile by
	// falling back to the established Streamlabs provider rather than passing an
	// orphaned identifier into the live-session flow.
	if (!ProviderRegistry::is_known(profile.provider_id))
		profile.provider_id = ProviderRegistry::streamlabs_id();

	profile.display_name = settings.value(QStringLiteral("display_name")).toString();
	profile.tiktok_username = settings.value(QStringLiteral("tiktok_username")).toString();
	profile.output_name = settings.value(QStringLiteral("output_name")).toString();
	profile.dual_layout_enabled = settings.value(QStringLiteral("dual_layout_enabled"), false).toBool();
	profile.dual_output_name = settings.value(QStringLiteral("dual_output_name")).toString();
	// Version 1.1 removed the private OBS RTMP-output mode. The retained
	// output_name still describes either the Aitum target or the explicit
	// credential-only target, so the old flag can be discarded safely.
	if (settings.contains(QStringLiteral("use_native_output")))
		removed_legacy_native_output_mode = true;
	profile.stream_title = settings.value(QStringLiteral("stream_title")).toString();
	profile.hashtag_id = settings.value(QStringLiteral("hashtag_id")).toString();
	profile.category = settings.value(QStringLiteral("category")).toString();
	profile.category_id = settings.value(QStringLiteral("category_id")).toString();
	if (ProviderRegistry::is_tiktok_studio(profile.provider_id) && profile.hashtag_id.isEmpty() &&
		!profile.category_id.isEmpty()) {
		profile.hashtag_id = QStringLiteral("5");
		migrated_topic_ids = true;
	}
	profile.mature = settings.value(QStringLiteral("mature"), false).toBool();
	profile.frame_signing_enabled = settings.value(QStringLiteral("frame_signing_enabled"), false).toBool();
	if (ProviderRegistry::frame_signing_requirement(profile.provider_id) ==
		FrameSigningRequirement::Required && !profile.frame_signing_enabled) {
		// The requirement belongs to the provider contract, not to a particular
		// UI build. Upgrade profiles saved before the explicit signing policy so
		// a required output cannot silently skip its media pipeline.
		profile.frame_signing_enabled = true;
		migrated_signing_policy = true;
	}
	profile.can_go_live = settings.value(QStringLiteral("can_go_live"), false).toBool();
	profile.live = settings.value(QStringLiteral("live"), false).toBool();
	profile.live_id = settings.value(QStringLiteral("live_id")).toString();
	profile.stream_id = settings.value(QStringLiteral("stream_id")).toString();
	const LiveCredentials credentials = TokenStore::load_live_credentials(profile.id);
	profile.stream_server = credentials.server;
	profile.stream_key = credentials.key;
	profile.application_status = settings.value(QStringLiteral("application_status")).toString();
	profile.dual_layout_available = settings.value(QStringLiteral("dual_layout_available"), false).toBool();
	profile.dual_layout_status = settings.value(QStringLiteral("dual_layout_status")).toString();
	profile.rapidapi_quota.limit = settings.value(QStringLiteral("rapidapi_quota_limit"), -1).toLongLong();
	profile.rapidapi_quota.remaining = settings.value(QStringLiteral("rapidapi_quota_remaining"), -1).toLongLong();
	profile.rapidapi_quota.reset_epoch_seconds = settings.value(QStringLiteral("rapidapi_quota_reset"), 0).toLongLong();
	profile.rapidapi_quota.observed_epoch_seconds = settings.value(QStringLiteral("rapidapi_quota_observed"), 0).toLongLong();
	return profile;
}

void write_profile(QSettings &settings, const Profile &profile)
{
	settings.setValue(QStringLiteral("id"), profile.id);
	settings.setValue(QStringLiteral("account_id"), profile.account_id);
	settings.setValue(QStringLiteral("provider_id"), profile.provider_id);
	settings.setValue(QStringLiteral("display_name"), profile.display_name);
	settings.setValue(QStringLiteral("tiktok_username"), profile.tiktok_username);
	settings.setValue(QStringLiteral("output_name"), profile.output_name);
	settings.setValue(QStringLiteral("dual_layout_enabled"), profile.dual_layout_enabled);
	settings.setValue(QStringLiteral("dual_output_name"), profile.dual_output_name);
	settings.setValue(QStringLiteral("stream_title"), profile.stream_title);
	settings.setValue(QStringLiteral("hashtag_id"), profile.hashtag_id);
	settings.setValue(QStringLiteral("category"), profile.category);
	settings.setValue(QStringLiteral("category_id"), profile.category_id);
	settings.setValue(QStringLiteral("mature"), profile.mature);
	settings.setValue(QStringLiteral("frame_signing_enabled"), profile.frame_signing_enabled);
	settings.setValue(QStringLiteral("can_go_live"), profile.can_go_live);
	settings.setValue(QStringLiteral("live"), profile.live);
	settings.setValue(QStringLiteral("live_id"), profile.live_id);
	settings.setValue(QStringLiteral("stream_id"), profile.stream_id);
	settings.setValue(QStringLiteral("application_status"), profile.application_status);
	settings.setValue(QStringLiteral("dual_layout_available"), profile.dual_layout_available);
	settings.setValue(QStringLiteral("dual_layout_status"), profile.dual_layout_status);
	settings.setValue(QStringLiteral("rapidapi_quota_limit"), profile.rapidapi_quota.limit);
	settings.setValue(QStringLiteral("rapidapi_quota_remaining"), profile.rapidapi_quota.remaining);
	settings.setValue(QStringLiteral("rapidapi_quota_reset"), profile.rapidapi_quota.reset_epoch_seconds);
	settings.setValue(QStringLiteral("rapidapi_quota_observed"), profile.rapidapi_quota.observed_epoch_seconds);
}

} // namespace

std::vector<Profile> ProfileRepository::load()
{
	QSettings settings(profiles_settings_path(), QSettings::IniFormat);
	bool migrated_account_ids = false;
	bool migrated_topic_ids = false;
	bool migrated_signing_policy = false;
	bool removed_legacy_native_output_mode = false;
	std::vector<Profile> profiles;

	const int count = settings.beginReadArray(QStringLiteral("profiles"));
	for (int index = 0; index < count; ++index) {
		settings.setArrayIndex(index);
		Profile profile = read_profile(settings, migrated_account_ids, migrated_topic_ids,
			migrated_signing_policy, removed_legacy_native_output_mode);
		if (!profile.id.isEmpty() && !profile.display_name.isEmpty())
			profiles.push_back(std::move(profile));
	}
	settings.endArray();

	if (migrated_account_ids || migrated_topic_ids || migrated_signing_policy || removed_legacy_native_output_mode)
		save(profiles);
	return profiles;
}

void ProfileRepository::save(const std::vector<Profile> &profiles)
{
	QSettings settings(profiles_settings_path(), QSettings::IniFormat);
	settings.clear();
	settings.beginWriteArray(QStringLiteral("profiles"), static_cast<int>(profiles.size()));
	for (int index = 0; index < static_cast<int>(profiles.size()); ++index) {
		settings.setArrayIndex(index);
		write_profile(settings, profiles.at(index));
	}
	settings.endArray();
	settings.sync();
}
