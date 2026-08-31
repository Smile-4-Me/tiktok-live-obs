// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "bridge_dock.hpp"

#include "frame_signing_settings.hpp"
#include "localization.hpp"
#include "profile_account_status.hpp"
#include "profile_live_session.hpp"
#include "tiktok_browser_session_adapter.hpp"
#include "tiktok_studio_login_dialog.hpp"
#include "token_store.hpp"

#include <QColor>
#include <QFormLayout>
#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

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

		complete_tiktok_studio_login(profile_id, login->account, login->can_go_live,
			login->application_status);
	}

void BridgeDock::complete_tiktok_studio_login(const QString &profile_id,
	const TikTokStudioAccountCredentials &account, bool can_go_live,
	const QString &application_status)
{
	Profile *profile = find_profile(profile_id);
	if (!profile || !account.has_login() || (account.username.trimmed().isEmpty() &&
		account.user_id.trimmed().isEmpty())) {
		show_transient_error(translated_or("Studio.Login.Incomplete",
			QStringLiteral("TikTok did not return a reusable account session.")));
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
		profile->tiktok_username = account.username.trimmed().isEmpty() ? account.user_id : account.username;
		profile->application_status = application_status;
		ProfileLiveSession::clear_output_assignment(*profile);
		profile->can_go_live = false;
		profile->frame_signing_enabled = false;
		profile->diagnostic = translated_or("Signing.SaveFailed",
			QStringLiteral("The frame-signing credentials could not be saved securely."));
		profile->diagnostic_error = true;
		save_profiles();
		refresh_profile_ui(profile_id);
		show_transient_error(with_secure_storage_error(profile->diagnostic));
		return;
	}
	profile->tiktok_username = account.username.trimmed().isEmpty() ? account.user_id : account.username;
	profile->application_status = application_status;
	profile->can_go_live = can_go_live;
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
	refresh_tiktok_studio_account(profile_id, false);
}

void BridgeDock::begin_tiktok_browser_session_import(const QString &profile_id,
	const QString &rapidapi_key)
{
	Profile *profile = find_profile(profile_id);
	if (!profile || !ProviderRegistry::is_tiktok_studio(profile->provider_id) ||
		rapidapi_key.trimmed().isEmpty())
		return;
	if (QMessageBox::question(this, text("Plugin.Name"), translated_or(
		"Studio.Login.BrowserImportConfirm", QStringLiteral(
			"Import the TikTok session selected in your browser now? The session is stored only in Windows Credential Manager.")),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		return;
	QString collector_error;
	const std::optional<TikTokBrowserSessionImport> imported =
		collect_tiktok_browser_session(&collector_error);
	if (!imported) {
		show_transient_error(collector_error);
		return;
	}
	if (profile->account_id.isEmpty())
		profile->account_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	TikTokStudioAccountCredentials account = TokenStore::load_tiktok_studio_account(profile->account_id);
	account.rapidapi_key = rapidapi_key.trimmed();
	account.cookie_jar = imported->netscape_cookie_jar;
	const quint64 generation = tiktok_studio_account_generation_.value(profile_id) + 1;
	tiktok_studio_account_generation_.insert(profile_id, generation);
	tiktok_studio_.import_browser_session(std::move(account),
		[this, profile_id, generation](TikTokStudioAccountInfo info, QString error) {
			if (tiktok_studio_account_generation_.value(profile_id) != generation)
				return;
			if (!error.isEmpty()) {
				show_transient_error(error);
				return;
			}
			complete_tiktok_studio_login(profile_id, info.account, info.can_go_live,
				info.application_status);
		});
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

QString BridgeDock::rapidapi_quota_text(const RapidApiQuota &quota) const
{
	QString value = translated_or("Studio.Account.QuotaUnknown",
		QStringLiteral("Not reported by RapidAPI yet"));
	if (quota.limit >= 0 && quota.remaining >= 0) {
		value = translated_or("Studio.Account.Quota", QStringLiteral("%1 / %2 requests remaining"))
			.arg(quota.remaining).arg(quota.limit);
	} else if (quota.remaining >= 0) {
		value = translated_or("Studio.Account.QuotaRemaining", QStringLiteral("%1 requests remaining"))
			.arg(quota.remaining);
	}

	if (quota.limit >= 0 && quota.has_calendar_reset()) {
		const QDateTime reset = QDateTime::fromSecsSinceEpoch(quota.reset_epoch_seconds).toLocalTime();
		if (reset.isValid())
			value += translated_or("Studio.Account.QuotaDailyResetAt",
				QStringLiteral(" · Daily limit · resets %1"))
				.arg(QLocale().toString(reset.date(), QLocale::ShortFormat));
	} else if (quota.limit >= 0) {
		// The Basic plan is a daily allowance. Some RapidAPI responses expose a
		// relative reset value rather than a timestamp, so state the dependable
		// daily reset without inventing a calendar date.
		value += translated_or("Studio.Account.QuotaDailyReset",
			QStringLiteral(" · Daily limit · resets daily"));
	}
	return value;
}

QLabel *BridgeDock::create_rapidapi_quota_field(const RapidApiQuota &quota, QWidget *parent) const
{
	auto *field = new QLabel(parent);
	field->setWordWrap(true);
	field->setTextFormat(Qt::PlainText);
	field->setTextInteractionFlags(Qt::TextSelectableByMouse);
	field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	field->setAccessibleName(translated_or("Studio.Account.QuotaLabel", QStringLiteral("RapidAPI limit")));
	update_rapidapi_quota_field(field, quota);
	return field;
}

void BridgeDock::update_rapidapi_quota_field(QLabel *field, const RapidApiQuota &quota) const
{
	if (!field)
		return;

	const QString value = rapidapi_quota_text(quota);
	field->setText(value);
	field->setToolTip(value);

	const QColor empty = field->palette().color(QPalette::Base);
	QString background = empty.name();
	if (quota.limit > 0 && quota.remaining >= 0) {
		const double fraction = std::clamp(static_cast<double>(quota.remaining) /
			static_cast<double>(quota.limit), 0.0, 1.0);
		// Green for a full daily limit, yellow halfway, red near exhaustion. A
		// shaded fill preserves readable text while still making the amount clear.
		const QColor current = QColor::fromHsvF(fraction / 3.0, 0.58, 0.54);
		const QColor fill_start = current.lighter(115);
		const QColor fill_end = current.darker(118);
		if (fraction >= 0.999) {
			background = QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 %2)")
				.arg(fill_start.name(), fill_end.name());
		} else {
			// A zero balance still leaves a thin red marker rather than looking like
			// a missing value. The unfilled area stays the native field surface.
			const double visible_fraction = std::max(0.015, fraction);
			const QString edge = QString::number(visible_fraction, 'f', 4);
			background = QStringLiteral(
				"qlineargradient(x1:0, y1:0, x2:1, y2:0, "
				"stop:0 %1, stop:%2 %3, stop:%2 %4, stop:1 %4)")
				.arg(fill_start.name(), edge, fill_end.name(), empty.name());
		}
	}
	field->setStyleSheet(QStringLiteral(
		"QLabel { color: palette(text); border: 1px solid palette(mid); border-radius: 4px; "
		"padding: 4px 6px; background: %1; }").arg(background));
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
	// The quota copy must stay complete even in a narrow dock. It therefore owns
	// one full-width row rather than competing with the label column.
	auto *quota_row = new QWidget(parent);
	auto *quota_layout = new QVBoxLayout(quota_row);
	quota_layout->setContentsMargins(0, 0, 0, 0);
	quota_layout->setSpacing(3);
	auto *quota_label = new QLabel(translated_or("Studio.Account.QuotaLabel",
		QStringLiteral("RapidAPI limit")), quota_row);
	quota_layout->addWidget(quota_label);
	quota_layout->addWidget(create_rapidapi_quota_field(profile.rapidapi_quota, quota_row));
	form->addRow(quota_row);
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
