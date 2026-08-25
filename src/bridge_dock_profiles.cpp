// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "bridge_dock.hpp"
#include "aitum_outputs.hpp"
#include "frame_signing_settings.hpp"
#include "localization.hpp"
#include "obs_button_style.hpp"
#include "profile_repository.hpp"
#include "profile_account_status.hpp"
#include "profile_live_session.hpp"
#include "profile_row.hpp"
#include "streamlabs_desktop.hpp"
#include "token_store.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <thread>

namespace {

QString with_secure_storage_error(const QString &message)
{
	const QString detail = TokenStore::last_error().trimmed();
	return detail.isEmpty() ? message : message + QStringLiteral("\n\n") + detail;
}

QString provider_display_name(const QString &provider_id)
{
	for (const ProviderDefinition &definition : ProviderRegistry::available()) {
		if (definition.id != provider_id)
			continue;
		const QByteArray key = definition.display_name_key.toUtf8();
		return translated_or(key.constData(), definition.fallback_display_name);
	}
	return provider_id;
}

bool has_manual_credentials(const Profile &profile)
{
	const LiveCredentials credentials = TokenStore::load_live_credentials(profile.id);
	return !credentials.server.trimmed().isEmpty() && !credentials.key.trimmed().isEmpty();
}

QString visible_tiktok_username(const Profile &profile)
{
	QString username = profile.tiktok_username.trimmed();
	if (username.isEmpty())
		username = profile.live_tiktok_username.trimmed();
	while (username.startsWith(QLatin1Char('@')))
		username.remove(0, 1);
	if (username.isEmpty())
		username = translated_or("Profile.UsernameUnknown", QStringLiteral("Unknown"));
	return QStringLiteral("@%1").arg(username);
}

QString profile_heading(const Profile &profile)
{
	return QStringLiteral("%1 (%2 | %3)")
		.arg(profile.display_name, provider_display_name(profile.provider_id), visible_tiktok_username(profile));
}

} // namespace

Profile BridgeDock::new_profile(const QString &name) const
	{
		Profile profile;
		profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
		profile.account_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
		profile.display_name = name;
		return profile;
	}

QLabel *BridgeDock::info_card(const QString &content, QWidget *parent) const
	{
		auto *card = new QLabel(content, parent);
		card->setWordWrap(true);
		card->setTextFormat(Qt::RichText);
		card->setOpenExternalLinks(true);
		card->setTextInteractionFlags(Qt::TextBrowserInteraction);
		card->setStyleSheet(QStringLiteral(
			"QLabel { background: rgba(90, 120, 160, 0.13); border: 1px solid rgba(130, 160, 205, 0.35); "
			"border-radius: 4px; padding: 8px; }"
			"QLabel a { color: palette(highlight); font-weight: 600; text-decoration: none; }"
			"QLabel a:hover { text-decoration: underline; }"));
		return card;
	}

void BridgeDock::build_ui()
	{
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(10, 10, 10, 10);
		layout->setSpacing(8);

		auto *profiles_label = new QLabel(text("Profiles.Title"), this);
		profiles_label->setStyleSheet(QStringLiteral("font-weight: 600;"));
		layout->addWidget(profiles_label);

		auto *profile_list_container = new QWidget(this);
		profile_list_layout_ = new QVBoxLayout(profile_list_container);
		profile_list_layout_->setContentsMargins(0, 0, 0, 0);
		profile_list_layout_->setSpacing(2);
		profile_list_layout_->setAlignment(Qt::AlignTop);
		profile_scroll_ = new QScrollArea(this);
		profile_scroll_->setWidget(profile_list_container);
		profile_scroll_->setWidgetResizable(true);
		profile_scroll_->setFrameShape(QFrame::NoFrame);
		layout->addWidget(profile_scroll_);

		add_profile_button_ = new QPushButton(text("Profiles.Add"), this);
		connect(add_profile_button_, &QPushButton::clicked, this, [this] {
			profiles_.push_back(new_profile(text("Profile.NewName")));
			selected_profile_ = static_cast<int>(profiles_.size()) - 1;
			save_profiles();
			rebuild_profile_list();
			show_selected_profile();
		});
		layout->addWidget(add_profile_button_);

		auto *separator = new QFrame(this);
		separator->setFrameShape(QFrame::HLine);
		separator->setFrameShadow(QFrame::Sunken);
		layout->addWidget(separator);

		detail_container_ = new QWidget(this);
		detail_layout_ = new QVBoxLayout(detail_container_);
		detail_layout_->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(detail_container_, 1);
	}

void BridgeDock::clear_layout(QLayout *layout)
	{
	while (QLayoutItem *item = layout->takeAt(0)) {
		if (QLayout *child_layout = item->layout())
			clear_layout(child_layout);
		// A focused editor can emit editingFinished while Qt tears it down. The
		// detail/profile rebuild paths must never run recursively from that
		// destruction event, otherwise Qt may still be dispatching the editor's
		// original input event while its sibling widgets are deleted.
		if (QWidget *widget = item->widget()) {
			widget->blockSignals(true);
			delete widget;
		}
		delete item;
	}
	}

void BridgeDock::rebuild_profile_list()
{
	clear_layout(profile_list_layout_);
		for (int index = 0; index < static_cast<int>(profiles_.size()); ++index) {
			auto *row = new ProfileRow(this);
			row->set_profile(profiles_.at(index), index == selected_profile_);
			row->on_clicked = [this, index] {
				// The clicked button is owned by the list being rebuilt. Defer the
				// rebuild until Qt has completed delivery of its mouse event.
				QTimer::singleShot(0, this, [this, index] {
					if (index < 0 || index >= static_cast<int>(profiles_.size()))
						return;
					selected_profile_ = index;
					rebuild_profile_list();
					show_selected_profile();
					refresh_selected_account();
				});
			};
			profile_list_layout_->addWidget(row);
	}

	// Use only the space needed for one to five profiles. A sixth profile keeps
	// the compact five-row viewport and activates the scroll bar.
	constexpr int visible_profile_rows = 5;
	constexpr int list_spacing = 2;
	const int displayed_rows = std::min(static_cast<int>(profiles_.size()), visible_profile_rows);
	const int profile_list_height = displayed_rows > 0
		? displayed_rows * ProfileRow::kHeight + (displayed_rows - 1) * list_spacing
		: 0;
	profile_scroll_->setFixedHeight(profile_list_height);
}

Profile *BridgeDock::selected_profile()
	{
		if (selected_profile_ < 0 || selected_profile_ >= static_cast<int>(profiles_.size()))
			return nullptr;
		return &profiles_[selected_profile_];
	}

void BridgeDock::show_selected_profile()
	{
		clear_layout(detail_layout_);
		Profile *profile = selected_profile();
		if (!profile)
			return;

		auto *header = new QWidget(detail_container_);
		auto *header_layout = new QHBoxLayout(header);
		header_layout->setContentsMargins(0, 0, 0, 0);
		header_layout->setSpacing(5);
		auto *heading = new QLabel(profile_heading(*profile), header);
		heading->setStyleSheet(QStringLiteral("QLabel { font-weight: 600; }"));
		heading->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
		heading->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		header_layout->addWidget(heading, 1);
		const QString rename_label = text("Profile.EditName");
		// OBS ships no pencil asset. Use its settings icon rather than a
		// plugin-drawn approximation, keeping the action visually native.
		auto *rename = obs_button_style::create_native_button(header,
			obs_button_style::obs_theme_icon(QStringLiteral("cogs.svg"), palette()),
			rename_label, false);
		header_layout->addWidget(rename, 0, Qt::AlignVCenter);
		const QString profile_id = profile->id;
		connect(rename, &QPushButton::clicked, this,
			[this, profile_id, header_layout, heading, rename] {
				Profile *current = find_profile(profile_id);
				if (!current)
					return;
				heading->hide();
				rename->hide();
				auto *editor = new QLineEdit(current->display_name, heading->parentWidget());
				editor->setAccessibleName(text("Profile.Name"));
				auto *context = new QLabel(
					QStringLiteral("(%1 | %2)").arg(provider_display_name(current->provider_id),
						visible_tiktok_username(*current)), heading->parentWidget());
				context->setStyleSheet(QStringLiteral("QLabel { color: palette(mid); }"));
				header_layout->insertWidget(0, editor, 1);
				header_layout->insertWidget(1, context);
				editor->setFocus(Qt::OtherFocusReason);
				editor->selectAll();
				connect(editor, &QLineEdit::editingFinished, this,
					[this, profile_id, editor] {
						Profile *edited = find_profile(profile_id);
						if (edited) {
							const QString name = editor->text().trimmed();
							if (!name.isEmpty() && edited->display_name != name) {
								edited->display_name = name;
								save_profiles();
							}
						}
						// Defer the rebuild until Qt has completed the editor event. This
						// avoids deleting the signal sender while it is still active.
						QTimer::singleShot(0, this, [this] {
							rebuild_profile_list();
							show_selected_profile();
						});
					});
			});
		detail_layout_->addWidget(header);

		// Manual stream credentials are the proof that the user already has LIVE
		// access. They therefore use one direct setup step, without a redundant
		// account-status page between entering the URL/key and streaming.
		if (ProviderRegistry::is_manual(profile->provider_id)) {
			if (has_manual_credentials(*profile))
				build_stream_step(*profile);
			else
				build_login_step(*profile);
		} else switch (profile->state()) {
		case ProfileState::NeedsLogin: build_login_step(*profile); break;
		case ProfileState::AwaitingLiveAccess: build_account_step(*profile); break;
		case ProfileState::Ready:
		case ProfileState::SessionUncertain:
		case ProfileState::Live: build_stream_step(*profile); break;
		}

		auto *footer = new QHBoxLayout();
		footer->addStretch();
		const QString delete_label = text("Profile.Delete");
		auto *delete_action = obs_button_style::create_baseline_icon_button(detail_container_,
			obs_button_style::plugin_asset_icon(QStringLiteral("garbage-bin-10428.svg")),
			delete_label);
		// Use the delete action's own native size hint. It remains the same
		// height as the edit control but has a width appropriate for its label.
		delete_action->setFixedSize(delete_action->sizeHint());
		delete_action->setStyleSheet(QStringLiteral("QPushButton { color: #e05d5d; }"));
		connect(delete_action, &QPushButton::clicked, this, [this] { delete_selected_profile(); });
		footer->addWidget(delete_action, 0, Qt::AlignVCenter);
		detail_layout_->addLayout(footer);
		detail_layout_->addStretch();
	}

Profile *BridgeDock::find_profile(const QString &id)
	{
		for (Profile &profile : profiles_)
			if (profile.id == id)
				return &profile;
		return nullptr;
	}

void BridgeDock::verify_token_for_profile(const QString &profile_id, const QString &token)
	{
		if (token.isEmpty())
			return;
		Profile *profile = find_profile(profile_id);
		if (!profile)
			return;
		ProviderLifecycle *provider = provider_sessions_.find(profile->provider_id);
		if (!provider)
			return;

		// ProviderLifecycle reads the temporary candidate through its normal secret
		// boundary. Keep the previous credential so an invalid token cannot replace
		// a working Streamlabs login.
		const QString previous_token = TokenStore::load(profile_id);
		if (!TokenStore::save(profile_id, token)) {
			show_transient_error(with_secure_storage_error(text("Error.TokenSaveFailed")));
			return;
		}
		const ProviderAccountReference account{profile_id, profile->account_id};
		provider->refresh_account(account,
			[this, profile_id, previous_token](ProviderAccountStatus status, QString error) {
			Profile *profile = find_profile(profile_id);
			if (!profile)
				return;
			if (!error.isEmpty()) {
				if (previous_token.isEmpty())
					TokenStore::remove(profile_id);
				else
					TokenStore::save(profile_id, previous_token);
				if (selected_profile() == profile)
					show_transient_error(error);
				return;
			}
			ProfileAccountStatus::apply(*profile, status);
			save_profiles();
			rebuild_profile_list();
			if (selected_profile() == profile)
				show_selected_profile();
		});
}

void BridgeDock::refresh_selected_account()
	{
		Profile *profile = selected_profile();
		if (!profile)
			return;
		if (profile->live || profile->preparing || profile->ending || profile->recovering)
			return;
		if (ProviderRegistry::is_tiktok_studio(profile->provider_id)) {
			refresh_tiktok_studio_account(profile->id);
			return;
		}
		if (ProviderRegistry::uses_local_credentials(profile->provider_id))
			return;
		const QString token = TokenStore::load(profile->id);
		if (!token.isEmpty())
			verify_token_for_profile(profile->id, token);
	}

void BridgeDock::show_transient_error(const QString &message)
	{
		QMessageBox::warning(this, text("Plugin.Name"), message);
	}

void BridgeDock::set_diagnostic(Profile &profile, const QString &message, bool is_error)
	{
		profile.diagnostic = message;
		profile.diagnostic_error = is_error;
		rebuild_profile_list();
		if (selected_profile() && selected_profile()->id == profile.id)
			show_selected_profile();
	}

	void BridgeDock::clear_live_session(Profile &profile)
	{
		tiktok_studio_heartbeat_in_flight_.remove(profile.id);
		tiktok_studio_heartbeat_failed_.remove(profile.id);
		tiktok_studio_heartbeat_status_.remove(profile.id);
		tiktok_studio_stale_heartbeat_count_.remove(profile.id);
		const QString signing_output = profile.frame_signing_output_name.isEmpty()
			? (profile.frame_signing_uses_main_output ? QString{} : profile.output_name)
			: profile.frame_signing_output_name;
		output_signing_.detach(signing_output);
		ProfileLiveSession::clear(profile);
		TokenStore::remove_live_credentials(profile.id);
		if (ProviderRegistry::uses_local_credentials(profile.provider_id)) {
			// A manually supplied RTMP pair is valid for one LIVE only. Clearing a
			// completed or failed session deliberately returns this profile to the
			// direct credential-entry step instead of reusing old access data.
			ProfileLiveSession::clear_output_assignment(profile);
			profile.can_go_live = false;
			profile.application_status.clear();
		}
}

void BridgeDock::refresh_profile_ui(const QString &profile_id)
	{
		rebuild_profile_list();
		if (Profile *current = selected_profile(); current && current->id == profile_id)
			show_selected_profile();
	}

void BridgeDock::reconcile_previous_sessions()
	{
		bool changed = false;
		for (Profile &profile : profiles_) {
			if (!profile.live)
				continue;

			changed = true;
			profile.preparing = false;
			profile.ending = false;
			profile.recovering = true;
			profile.session_uncertain = false;
		profile.diagnostic = text("Diagnostic.RecoveryChecking");
		profile.diagnostic_error = false;
		if (ProviderRegistry::is_manual(profile.provider_id)) {
			// Manual credentials do not expose a remote session API. After an OBS restart,
			// use Aitum's observable output state as the source of truth and never keep a
			// stale local reservation merely because OBS was closed.
			QString diagnostic;
			bool output_active = false;
			const bool state_available = !profile.output_name.isEmpty()
				&& aitum_output_is_active(profile.output_name, &output_active, &diagnostic);
			if (state_available && output_active) {
				profile.recovering = false;
				profile.diagnostic = text("Manual.SessionReady");
			} else {
				clear_live_session(profile);
				profile.diagnostic = text("Diagnostic.RecoveryCleared");
			}
			continue;
		}
		if (ProviderRegistry::is_tiktok_studio(profile.provider_id)) {
			ProviderLifecycle *provider = provider_sessions_.find(profile.provider_id);
			if (!provider) {
				profile.recovering = false;
				profile.session_uncertain = true;
				profile.diagnostic = text("Diagnostic.RecoveryFailed").arg(
					text("Error.MissingToken"));
				profile.diagnostic_error = true;
				continue;
			}
			const QString profile_id = profile.id;
			const ProviderAccountReference account{profile.id, profile.account_id};
			provider->find_continuable_live(account,
				[this, profile_id](PreparedLive live, QString error) mutable {
					Profile *current = find_profile(profile_id);
					if (!current || !ProviderRegistry::is_tiktok_studio(current->provider_id))
						return;

					current->recovering = false;
					current->preparing = false;
					if (!live.room_id.isEmpty() && !live.stream_id.isEmpty()) {
						// TikTok returned a reusable room, not proof that OBS is
						// currently sending video. Keep it as an unresolved session.
						ProfileLiveSession::mark_uncertain(*current, live);
						current->live = false;
						const bool credentials_saved = live.server.isEmpty() || live.key.isEmpty() ||
							TokenStore::save_live_credentials(profile_id, {live.server, live.key});
						QString signing_error;
						const bool signing_saved = FrameSigningSettings::synchronize_tiktok_studio_account(
							profile_id, current->account_id, &signing_error);
						if (error.isEmpty() && credentials_saved && signing_saved) {
							current->diagnostic = translated_or("Studio.Recovery.Available", QStringLiteral(
								"TikTok kept the previous LIVE active. Choose Resume TikTok LIVE to reconnect OBS, or End TikTok LIVE to finish it."));
							current->diagnostic_error = false;
						} else {
							QString reason = error;
							if (!credentials_saved || !signing_saved)
								reason = with_secure_storage_error(translated_or("Studio.Account.SaveFailed",
									QStringLiteral("The refreshed TikTok session could not be saved securely."))) +
									(signing_error.isEmpty() ? QString{} : QStringLiteral(" ") + signing_error);
							current->diagnostic = translated_or("Studio.Recovery.Incomplete", QStringLiteral(
								"TikTok reports an existing LIVE, but OBS cannot resume it yet: %1. You can still end it or reset local state.")).arg(reason);
							current->diagnostic_error = true;
						}
					} else if (error.isEmpty()) {
						clear_live_session(*current);
						current->diagnostic = text("Diagnostic.RecoveryCleared");
						current->diagnostic_error = false;
					} else {
						// The remote state could not be confirmed. Do not present an
						// uncertain reservation as a confirmed LIVE stream.
						current->live = false;
						current->session_uncertain = true;
						current->diagnostic = text("Diagnostic.RecoveryFailed").arg(error);
						current->diagnostic_error = true;
					}
					save_profiles();
					refresh_profile_ui(profile_id);
				});
			continue;
		}

		if (profile.live_id.isEmpty()) {
				clear_live_session(profile);
				profile.diagnostic = text("Diagnostic.RecoveryCleared");
				continue;
			}

			ProviderLifecycle *provider = provider_sessions_.find(profile.provider_id);
			if (!provider) {
				profile.recovering = false;
				profile.session_uncertain = true;
				profile.diagnostic = text("Diagnostic.RecoveryFailed").arg(text("Error.MissingToken"));
				profile.diagnostic_error = true;
				continue;
			}

			const QString profile_id = profile.id;
			const ProviderAccountReference account{profile.id, profile.account_id};
			const PreparedLive live{.session_id = profile.live_id, .stream_id = profile.stream_id,
				.room_id = profile.live_id, .server = profile.stream_server, .key = profile.stream_key};
			provider->end_live(account, live, [this, profile_id](ProviderEndResult result) {
				Profile *current = find_profile(profile_id);
				if (!current)
					return;
				if (result.ended || result.stale_session) {
					clear_live_session(*current);
					current->diagnostic = text("Diagnostic.RecoveryCleared");
					current->diagnostic_error = false;
				} else {
					// Keep the reservation until Streamlabs confirms that the old session is gone.
					current->recovering = false;
					current->session_uncertain = true;
					current->diagnostic = text("Diagnostic.RecoveryFailed").arg(result.error);
					current->diagnostic_error = true;
				}
				save_profiles();
				refresh_profile_ui(profile_id);
			});
		}
		if (changed)
			save_profiles();
	}

void BridgeDock::build_login_step(const Profile &profile)
	{
		auto *group = new QGroupBox(text("Login.Title"), detail_container_);
		auto *layout = new QVBoxLayout(group);
		auto *provider_form = new QFormLayout();
		provider_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
		provider_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		auto *provider_choice = new QComboBox(group);
		for (const ProviderDefinition &provider : ProviderRegistry::available()) {
			const QByteArray key = provider.display_name_key.toUtf8();
			provider_choice->addItem(translated_or(key.constData(), provider.fallback_display_name), provider.id);
		}
		const int provider_index = provider_choice->findData(profile.provider_id);
		provider_choice->setCurrentIndex(provider_index >= 0 ? provider_index : 0);
		provider_form->addRow(text("Provider.Label"), provider_choice);
		layout->addLayout(provider_form);
		const QString profile_id = profile.id;
		connect(provider_choice, &QComboBox::currentIndexChanged, this, [this, profile_id, provider_choice] {
			if (Profile *current = find_profile(profile_id)) {
				const QString provider_id = provider_choice->currentData().toString();
				if (current->provider_id == provider_id)
					return;
				current->provider_id = provider_id;
				if (current->account_id.isEmpty())
					current->account_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
				current->tiktok_username.clear();
				ProfileLiveSession::clear_output_assignment(*current);
				current->can_go_live = false;
				current->live = false;
				current->live_id.clear();
				current->stream_id.clear();
			current->application_status.clear();
			current->diagnostic.clear();
			save_profiles();
			// This handler runs from the combo box that belongs to the current
			// detail view. Defer rebuilding the view until Qt has returned from
			// currentIndexChanged; deleting it here would destroy the signal sender
			// while Qt is still dispatching its event.
			QTimer::singleShot(0, this, [this] {
				rebuild_profile_list();
				show_selected_profile();
				refresh_selected_account();
			});
		}
	});

		if (ProviderRegistry::uses_local_credentials(profile.provider_id)) {
			layout->addWidget(info_card(text("Manual.Description"), group));
			auto *manual_form = new QFormLayout();
			manual_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
			manual_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
			auto *username = new QLineEdit(profile.tiktok_username, group);
			username->setPlaceholderText(text("Manual.UsernamePlaceholder"));
			auto *server = new QLineEdit(group);
			server->setPlaceholderText(text("Manual.ServerPlaceholder"));
			auto *key = new QLineEdit(group);
			key->setPlaceholderText(text("Manual.KeyPlaceholder"));
			key->setEchoMode(QLineEdit::Password);
			manual_form->addRow(text("Manual.UsernameOptional"), username);
			manual_form->addRow(text("Manual.Server"), server);
			manual_form->addRow(text("Manual.Key"), key);
			layout->addLayout(manual_form);
			auto *save = new QPushButton(text("Manual.Save"), group);
			connect(save, &QPushButton::clicked, this, [this, profile_id, username, server, key] {
				save_local_credentials(profile_id, username->text(), server->text(), key->text());
			});
			layout->addWidget(save);
			detail_layout_->addWidget(group);
			return;
		}
		if (ProviderRegistry::is_tiktok_studio(profile.provider_id)) {
			layout->addWidget(info_card(translated_or("Studio.Login.Description",
				QStringLiteral("TikTok LIVE Studio uses a QR-code login. Request and video-frame signatures are supplied by RapidAPI; your login stays in Windows Credential Manager.")), group));
			const TikTokStudioAccountCredentials saved =
				TokenStore::load_tiktok_studio_account(profile.account_id);
			auto *studio_form = new QFormLayout();
			studio_form->setRowWrapPolicy(QFormLayout::WrapLongRows);
			studio_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
			auto *rapidapi_key = new QLineEdit(saved.rapidapi_key, group);
			rapidapi_key->setEchoMode(QLineEdit::Password);
			rapidapi_key->setPlaceholderText(translated_or("Studio.RapidApiKey.Placeholder",
				QStringLiteral("Paste your RapidAPI key")));
			studio_form->addRow(translated_or("Studio.RapidApiKey", QStringLiteral("RapidAPI key")), rapidapi_key);
			layout->addLayout(studio_form);
			layout->addWidget(info_card(translated_or("Studio.RapidApiKey.Help",
				QStringLiteral("<b>You need a free RapidAPI key before signing in.</b><br/><br/>"
					"<a href=\"https://rapidapi.com/Loukious/api/tiktok-live-studio-api-signer1\">"
					"👉 Set up free Basic access on RapidAPI.</a><br/><br/>"
					"For a normal individual stream, the free Basic plan is usually enough. "
					"If you manage several TikTok profiles, you can store the appropriate RapidAPI key "
					"with each profile to keep them organised.")), group));
			auto *login = new QPushButton(translated_or("Studio.Login.Button",
				QStringLiteral("Log in with TikTok QR code")), group);
			login->setEnabled(!rapidapi_key->text().trimmed().isEmpty());
			connect(rapidapi_key, &QLineEdit::textChanged, login,
				[login](const QString &value) { login->setEnabled(!value.trimmed().isEmpty()); });
			connect(login, &QPushButton::clicked, this,
				[this, profile_id, rapidapi_key] {
					begin_tiktok_studio_login(profile_id, rapidapi_key->text().trimmed());
				});
			layout->addWidget(login);
			detail_layout_->addWidget(group);
			return;
		}
		layout->addWidget(info_card(text("Login.Description"), group));
		auto *token = new QLineEdit(group);
		token->setEchoMode(QLineEdit::Password);
		token->setPlaceholderText(text("Login.TokenPlaceholder"));
		layout->addWidget(token);
		auto *actions = new QHBoxLayout();
		auto *verify = new QPushButton(text("Login.VerifyToken"), group);
		auto *from_pc = new QPushButton(text("Login.StreamlabsDesktop"), group);
		auto *from_web = new QPushButton(text("Login.WebLogin"), group);
		actions->addWidget(verify);
		actions->addWidget(from_pc);
		actions->addWidget(from_web);
		layout->addLayout(actions);
		layout->addWidget(info_card(text("Login.Instructions"), group));
		connect(verify, &QPushButton::clicked, this, [this, profile_id, token] {
			verify_token_for_profile(profile_id, token->text().trimmed());
		});
		connect(from_pc, &QPushButton::clicked, this, [this, profile_id, token, from_pc] {
			from_pc->setEnabled(false);
			from_pc->setText(text("Login.Searching"));
			std::thread([this, profile_id, token, from_pc] {
				const QString found = find_streamlabs_desktop_token();
				QMetaObject::invokeMethod(this, [this, profile_id, token, from_pc, found] {
					if (token) token->setText(found);
			if (from_pc) { from_pc->setEnabled(true); from_pc->setText(text("Login.StreamlabsDesktop")); }
					if (found.isEmpty()) show_transient_error(text("Error.TokenNotFound"));
					else verify_token_for_profile(profile_id, found);
				}, Qt::QueuedConnection);
			}).detach();
		});
		connect(from_web, &QPushButton::clicked, this, [this, profile_id, from_web] {
			from_web->setEnabled(false);
			from_web->setText(text("Login.WaitingForLogin"));
			streamlabs_.login_in_browser([this, profile_id, from_web](QString token, QString error) {
			if (from_web) { from_web->setEnabled(true); from_web->setText(text("Login.WebLogin")); }
				if (!error.isEmpty()) show_transient_error(error);
				else verify_token_for_profile(profile_id, token);
			});
		});
		detail_layout_->addWidget(group);
	}

void BridgeDock::build_account_step(const Profile &profile)
	{
		auto *group = new QGroupBox(text("Account.Title"), detail_container_);
		auto *form = new QFormLayout(group);
		form->setRowWrapPolicy(QFormLayout::WrapLongRows);
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		if (ProviderRegistry::is_tiktok_studio(profile.provider_id)) {
			add_tiktok_studio_account_controls(form, profile, group);
		} else {
			auto add_readonly = [form, group](const QString &label, const QString &value) {
				auto *field = new QLineEdit(value, group);
				field->setReadOnly(true);
				form->addRow(label, field);
			};
			add_readonly(text("Account.Username"), profile.tiktok_username);
			add_readonly(text("Account.Status"), account_status_text(profile));
			add_readonly(text("Account.CanGoLive"),
				text(profile.can_go_live ? "Common.True" : "Common.False"));
			auto *refresh = new QPushButton(text("Account.Refresh"), group);
			connect(refresh, &QPushButton::clicked, this, [this] { refresh_selected_account(); });
			form->addRow(refresh);
			if (!profile.can_go_live)
				form->addRow(info_card(text("Account.Instructions"), group));
		}
		detail_layout_->addWidget(group);
	}

void BridgeDock::save_local_credentials(const QString &profile_id, const QString &username,
	const QString &server, const QString &key)
{
	Profile *profile = find_profile(profile_id);
	if (!profile || !ProviderRegistry::uses_local_credentials(profile->provider_id))
		return;
	if (server.trimmed().isEmpty() || key.trimmed().isEmpty()) {
		show_transient_error(text("Manual.MissingFields"));
		return;
	}
	if (!TokenStore::save_live_credentials(profile_id, {server.trimmed(), key.trimmed()})) {
		show_transient_error(text("Manual.SaveFailed"));
		return;
	}
	// Manual RTMP credentials do not reliably expose a TikTok username. Keep the
	// field optional instead of inventing account identity from a stream key.
	profile->tiktok_username = username.trimmed();
	profile->can_go_live = true;
	profile->application_status = QStringLiteral("manual");
	profile->stream_server = server.trimmed();
	profile->stream_key = key.trimmed();
	profile->diagnostic = text("Manual.Saved");
	profile->diagnostic_error = false;
	save_profiles();
	rebuild_profile_list();
	show_selected_profile();
}

void BridgeDock::delete_selected_profile()
	{
		Profile *profile = selected_profile();
		if (!profile)
			return;
		if (profile->live || profile->preparing || profile->ending || profile->recovering) {
			QMessageBox::warning(this, text("Profile.ActiveTitle"),
				text("Profile.ActiveDeleteWarning"));
			return;
		}
		if (QMessageBox::question(this, text("Profile.Delete"),
			text("Profile.DeleteConfirmation").arg(profile->display_name),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;
		++tiktok_studio_account_generation_[profile->id];
		TokenStore::remove(profile->id);
		const QString account_id = profile->account_id;
		const bool account_used_elsewhere = std::any_of(profiles_.cbegin(), profiles_.cend(),
			[profile, &account_id](const Profile &candidate) {
				return candidate.id != profile->id && candidate.account_id == account_id;
			});
		if (!account_used_elsewhere)
			TokenStore::remove_tiktok_studio_account(account_id);
		profiles_.erase(profiles_.begin() + selected_profile_);
		if (profiles_.empty())
			profiles_.push_back(new_profile(text("Profile.DefaultName")));
		selected_profile_ = qMin(selected_profile_, static_cast<int>(profiles_.size()) - 1);
		save_profiles();
		rebuild_profile_list();
		show_selected_profile();
	}

void BridgeDock::load_profiles()
	{
	profiles_ = ProfileRepository::load();
	bool changed = false;
	for (Profile &profile : profiles_) {
		const ProfileState state = profile.state();
		if (state != ProfileState::NeedsLogin && state != ProfileState::AwaitingLiveAccess)
			continue;
		if (profile.output_name.isEmpty() && !profile.frame_signing_uses_main_output &&
			profile.frame_signing_output_name.isEmpty())
			continue;
		// Older versions could leave an Aitum selection attached after a profile
		// returned to setup. Enforce the same invariant during migration so the
		// profile list cannot advertise a stale output link after an upgrade.
		ProfileLiveSession::clear_output_assignment(profile);
		changed = true;
	}
	if (changed)
		ProfileRepository::save(profiles_);
	}

void BridgeDock::save_profiles() const
	{
	ProfileRepository::save(profiles_);
	}
