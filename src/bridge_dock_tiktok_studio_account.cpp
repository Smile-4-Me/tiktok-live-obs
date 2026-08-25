// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "bridge_dock.hpp"

#include "frame_signing_settings.hpp"
#include "localization.hpp"
#include "profile_account_status.hpp"
#include "profile_live_session.hpp"
#include "tiktok_studio_login_dialog.hpp"
#include "token_store.hpp"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

namespace {

QString with_secure_storage_error(const QString &message)
{
	const QString detail = TokenStore::last_error().trimmed();
	return detail.isEmpty() ? message : message + QStringLiteral("\n\n") + detail;
}

QUrl tiktok_live_studio_access_url()
{
	// This is TikTok's public LIVE Studio access route. The route identifier is
	// shared by TikTok's own LIVE Studio client, not tied to a Streamlabs flow.
	QUrl url(QStringLiteral(
		"https://www.tiktok.com/falcon/live_g/live_studio_access_routing/index.html"));
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("id"), QStringLiteral("AT7395808210055386129"));
	query.addQueryItem(QStringLiteral("__live_platform__"), QStringLiteral("webcast"));
	query.addQueryItem(QStringLiteral("hide_nav_bar"), QStringLiteral("1"));
	query.addQueryItem(QStringLiteral("target_handler"), QStringLiteral("webcast"));
	query.addQueryItem(QStringLiteral("h5_from"), QStringLiteral("live_studio"));
	query.addQueryItem(QStringLiteral("lang"), obs_language());
	url.setQuery(query);
	return url;
}

QString tiktok_live_studio_access_link()
{
	return tiktok_live_studio_access_url().toString(QUrl::FullyEncoded);
}

} // namespace


void BridgeDock::begin_tiktok_studio_login(const QString &profile_id, const QString &rapidapi_key)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile || !ProviderRegistry::is_tiktok_studio(profile->provider_id) ||
			rapidapi_key.trimmed().isEmpty())
			return;
		if (profile->account_id.isEmpty())
			profile->account_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

		TikTokStudioAccountCredentials account =
			TokenStore::load_tiktok_studio_account(profile->account_id);
		account.rapidapi_key = rapidapi_key.trimmed();
		if (!TokenStore::save_tiktok_studio_account(profile->account_id, account)) {
			show_transient_error(with_secure_storage_error(translated_or("Studio.Account.SaveFailed",
				QStringLiteral("The TikTok login could not be saved securely."))));
			return;
		}
		++tiktok_studio_account_generation_[profile_id];

		TikTokStudioLoginDialog dialog(&tiktok_studio_, account, this);
		dialog.exec();
		account = dialog.account();
		const std::optional<TikTokStudioQrPoll> login = dialog.result();
		if (!login) {
			// Preserve a successfully registered device even when the user closes an
			// otherwise incomplete QR session. The next attempt can reuse it.
			if (!TokenStore::save_tiktok_studio_account(profile->account_id, account) && account.has_device())
				show_transient_error(with_secure_storage_error(translated_or("Studio.Account.SaveFailed",
					QStringLiteral("The TikTok device registration could not be saved securely."))));
			return;
		}

		account = login->account;
		if (!account.has_login() || (account.username.trimmed().isEmpty() &&
			account.user_id.trimmed().isEmpty())) {
			show_transient_error(translated_or("Studio.Login.Incomplete",
				QStringLiteral("TikTok confirmed the QR code but did not return a reusable account session.")));
			return;
		}
		if (!TokenStore::save_tiktok_studio_account(profile->account_id, account)) {
			show_transient_error(with_secure_storage_error(translated_or("Studio.Account.SaveFailed",
				QStringLiteral("The TikTok login could not be saved securely."))));
			return;
		}

		const FrameSigningCredentials signing = FrameSigningSettings::for_tiktok_studio_account(
			FrameSigningSettings::load(profile_id), account);
		if (!FrameSigningSettings::save(profile_id, signing)) {
			profile = find_profile(profile_id);
			if (profile) {
				profile->tiktok_username = account.username.trimmed().isEmpty()
					? account.user_id : account.username;
				profile->application_status = login->application_status;
				ProfileLiveSession::clear_output_assignment(*profile);
				profile->can_go_live = false;
				profile->frame_signing_enabled = false;
				profile->diagnostic = translated_or("Signing.SaveFailed",
					QStringLiteral("The frame-signing credentials could not be saved securely."));
				profile->diagnostic_error = true;
				save_profiles();
				refresh_profile_ui(profile_id);
			}
			show_transient_error(with_secure_storage_error(translated_or("Signing.SaveFailed",
				QStringLiteral("The frame-signing credentials could not be saved securely."))));
			return;
		}

		profile = find_profile(profile_id);
		if (!profile)
			return;
		profile->tiktok_username = account.username.trimmed().isEmpty()
			? account.user_id : account.username;
		profile->application_status = login->application_status;
		profile->can_go_live = login->can_go_live;
		if (!profile->can_go_live)
			ProfileLiveSession::clear_output_assignment(*profile);
		profile->frame_signing_enabled = true;
		profile->diagnostic = translated_or("Studio.Login.Saved",
			QStringLiteral("TikTok login and device registration saved securely."));
		profile->diagnostic_error = false;
		save_profiles();
		rebuild_profile_list();
		if (selected_profile() == profile)
			show_selected_profile();

		// The QR callback establishes a profile-owned TikTok LIVE Studio session,
		// but does not always include a complete entitlement result. Refresh it
		// immediately through that saved account session. This is deliberately
		// independent of the user's browser state or any other TikTok profile.
		refresh_tiktok_studio_account(profile_id, false);
	}

void BridgeDock::refresh_tiktok_studio_account(const QString &profile_id, bool report_error)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile || !ProviderRegistry::is_tiktok_studio(profile->provider_id))
			return;
		ProviderLifecycle *provider = provider_sessions_.find(profile->provider_id);
		if (!provider)
			return;
		const quint64 generation = tiktok_studio_account_generation_.value(profile_id) + 1;
		tiktok_studio_account_generation_.insert(profile_id, generation);

		provider->refresh_account({profile->id, profile->account_id},
			[this, profile_id, report_error, generation](ProviderAccountStatus status, QString error) {
				if (tiktok_studio_account_generation_.value(profile_id) != generation)
					return;
				Profile *current = find_profile(profile_id);
				if (!current || !ProviderRegistry::is_tiktok_studio(current->provider_id))
					return;
				if (!error.isEmpty()) {
					current->diagnostic = translated_or("Studio.Account.RefreshFailed",
						QStringLiteral("TikTok account refresh failed: %1")).arg(error);
					current->diagnostic_error = true;
					save_profiles();
					refresh_profile_ui(profile_id);
					if (report_error)
						show_transient_error(error);
					return;
				}
				QString signing_error;
				if (!FrameSigningSettings::synchronize_tiktok_studio_account(profile_id,
					current->account_id, &signing_error)) {
					current->tiktok_username = status.username;
					ProfileLiveSession::clear_output_assignment(*current);
					current->can_go_live = false;
					current->frame_signing_enabled = false;
					current->diagnostic = signing_error.isEmpty()
						? translated_or("Signing.SaveFailed",
							QStringLiteral("The frame-signing credentials could not be saved securely."))
						: signing_error;
					current->diagnostic_error = true;
					save_profiles();
					refresh_profile_ui(profile_id);
					if (report_error)
						show_transient_error(current->diagnostic);
					return;
				}
				ProfileAccountStatus::apply(*current, status);
				current->frame_signing_enabled = true;
				current->diagnostic = translated_or("Studio.Account.Refreshed",
					QStringLiteral("TikTok account refreshed."));
				current->diagnostic_error = false;
				save_profiles();
				refresh_profile_ui(profile_id);
			});
	}

void BridgeDock::add_tiktok_studio_account_controls(QFormLayout *form,
	const Profile &profile, QWidget *parent)
	{
		if (!ProviderRegistry::is_tiktok_studio(profile.provider_id))
			return;
		auto add_readonly = [form, parent](const QString &label, const QString &value) {
			auto *field = new QLineEdit(value, parent);
			field->setReadOnly(true);
			form->addRow(label, field);
	};
	add_readonly(text("Account.Username"), profile.tiktok_username);
	add_readonly(text("Account.Status"), account_status_text(profile));
	const QString live_access = profile.application_status == QStringLiteral("live_access_unknown")
		? translated_or("Studio.Account.LiveAccessUnknownState",
			QStringLiteral("LIVE access will be confirmed when you create your first LIVE."))
		: text(profile.can_go_live ? "Common.True" : "Common.False");
	add_readonly(text("Account.CanGoLive"), live_access);
		auto *refresh = new QPushButton(text("Account.Refresh"), parent);
		const bool account_busy = profile.live || profile.preparing || profile.ending ||
			profile.recovering || profile.session_uncertain;
		refresh->setEnabled(!account_busy);
		connect(refresh, &QPushButton::clicked, this,
			[this, profile_id = profile.id] { refresh_tiktok_studio_account(profile_id, true); });
	form->addRow(refresh);

	if (!profile.can_go_live) {
		const QString card = translated_or("Studio.Account.ApplyAccessCard",
			QStringLiteral("<b>This TikTok account does not have LIVE access yet.</b><br/><br/>"
				"<a href=\"%1\">👉 Apply for TikTok LIVE access directly through TikTok.</a><br/><br/>"
				"After approval, select <b>Refresh account</b>.")).arg(tiktok_live_studio_access_link());
		form->addRow(info_card(card, parent));
	}
}

QString BridgeDock::account_status_text(const Profile &profile) const
{
	if (profile.application_status == QStringLiteral("live_access_denied") ||
		profile.application_status == QStringLiteral("never_applied") ||
		profile.application_status == QStringLiteral("tiktok_live_authorization_missing") ||
		profile.application_status == QStringLiteral("TikTok LIVE authorization missing"))
		return text("Status.AwaitingLiveAccess");
	if (!ProviderRegistry::is_tiktok_studio(profile.provider_id))
		return profile.application_status;
	if (profile.application_status == QStringLiteral("live_access_unknown"))
		return translated_or("Studio.Account.LoginSuccessful", QStringLiteral("Sign-in successful"));
	return profile.application_status;
}
