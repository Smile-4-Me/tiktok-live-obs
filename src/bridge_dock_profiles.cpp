// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "bridge_dock.hpp"
#include "aitum_outputs.hpp"
#include "localization.hpp"
#include "plugin_paths.hpp"
#include "profile_row.hpp"
#include "streamlabs_desktop.hpp"
#include "token_store.hpp"

#include <QAbstractButton>
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
#include <QSettings>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <thread>

Profile BridgeDock::new_profile(const QString &name) const
	{
		Profile profile;
		profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
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
			delete item->widget();
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

		auto *header = new QGroupBox(text("Profile.Title"), detail_container_);
		auto *header_form = new QFormLayout(header);
		auto *profile_name = new QLineEdit(profile->display_name, header);
		header_form->addRow(text("Profile.Name"), profile_name);
		connect(profile_name, &QLineEdit::editingFinished, this, [this, profile_name] {
			if (Profile *current = selected_profile()) {
				const QString name = profile_name->text().trimmed();
				if (!name.isEmpty()) {
					current->display_name = name;
					save_profiles();
					rebuild_profile_list();
				}
			}
		});
		detail_layout_->addWidget(header);

		switch (profile->state()) {
		case ProfileState::NeedsLogin: build_login_step(*profile); break;
		case ProfileState::AwaitingLiveAccess: build_account_step(*profile); break;
		case ProfileState::Ready:
		case ProfileState::SessionUncertain:
		case ProfileState::Live: build_stream_step(*profile); break;
		}

		auto *footer = new QHBoxLayout();
		footer->addStretch();
		auto *delete_button = new QPushButton(text("Profile.Delete"), detail_container_);
		delete_button->setFlat(true);
		delete_button->setStyleSheet(QStringLiteral("QPushButton { color: #e05d5d; padding: 2px; }"));
		connect(delete_button, &QPushButton::clicked, this, [this] { delete_selected_profile(); });
		footer->addWidget(delete_button);
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
		streamlabs_.verify_account(token, [this, profile_id, token](StreamlabsAccount account, QString error) {
			Profile *profile = find_profile(profile_id);
			if (!profile)
				return;
			if (!error.isEmpty()) {
				if (selected_profile() == profile)
					show_transient_error(error);
				return;
			}
			if (!TokenStore::save(profile_id, token)) {
				show_transient_error(text("Error.TokenSaveFailed"));
				return;
			}
			profile->tiktok_username = account.username;
			profile->application_status = account.application_status;
			profile->can_go_live = account.can_go_live;
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
		profile.live = false;
		profile.preparing = false;
		profile.ending = false;
		profile.recovering = false;
		profile.session_uncertain = false;
		profile.live_id.clear();
		profile.stream_server.clear();
		profile.stream_key.clear();
		// Locally entered credentials are long-lived user configuration, not a
		// generated one-time session. Keep them available for the next stream.
		if (!ProviderRegistry::uses_local_credentials(profile.provider_id))
			TokenStore::remove_live_credentials(profile.id);
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
		if (ProviderRegistry::is_research(profile.provider_id)) {
			// The localhost listener is deliberately ephemeral. Never restore its
			// reservation after OBS has restarted.
			clear_live_session(profile);
			profile.diagnostic = text("Research.RecoveryCleared");
			continue;
		}
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

		if (profile.live_id.isEmpty()) {
				clear_live_session(profile);
				profile.diagnostic = text("Diagnostic.RecoveryCleared");
				continue;
			}

			const QString profile_id = profile.id;
			const QString live_id = profile.live_id;
			const QString token = TokenStore::load(profile_id);
			if (token.isEmpty()) {
				profile.recovering = false;
				profile.session_uncertain = true;
				profile.diagnostic = text("Diagnostic.RecoveryFailed").arg(text("Error.MissingToken"));
				profile.diagnostic_error = true;
				continue;
			}

			streamlabs_.end_live(token, live_id, [this, profile_id](StreamlabsEndResult result) {
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
		auto *provider_choice = new QComboBox(group);
		for (const ProviderDefinition &provider : ProviderRegistry::available())
			provider_choice->addItem(provider.display_name, provider.id);
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
				current->tiktok_username.clear();
				current->can_go_live = false;
				current->live = false;
				current->live_id.clear();
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
			});
		}
	});

		if (ProviderRegistry::uses_local_credentials(profile.provider_id)) {
			const bool research_provider = ProviderRegistry::is_research(profile.provider_id);
			layout->addWidget(info_card(text(research_provider ? "Research.Description" : "Manual.Description"), group));
			auto *manual_form = new QFormLayout();
			auto *username = new QLineEdit(group);
			username->setPlaceholderText(text("Manual.UsernamePlaceholder"));
			auto *server = new QLineEdit(group);
			server->setPlaceholderText(text("Manual.ServerPlaceholder"));
			auto *key = new QLineEdit(group);
			key->setPlaceholderText(text("Manual.KeyPlaceholder"));
			key->setEchoMode(QLineEdit::Password);
			manual_form->addRow(text("Manual.Username"), username);
			manual_form->addRow(text("Manual.Server"), server);
			manual_form->addRow(text("Manual.Key"), key);
			layout->addLayout(manual_form);
			auto *save = new QPushButton(text(research_provider ? "Research.Save" : "Manual.Save"), group);
			connect(save, &QPushButton::clicked, this, [this, profile_id, username, server, key] {
				save_local_credentials(profile_id, username->text(), server->text(), key->text());
			});
			layout->addWidget(save);
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
		auto add_readonly = [form, group](const QString &label, const QString &value) {
			auto *field = new QLineEdit(value, group);
			field->setReadOnly(true);
			form->addRow(label, field);
		};
		add_readonly(text("Account.Username"), profile.tiktok_username);
		add_readonly(text("Account.Status"), profile.application_status);
		add_readonly(text("Account.CanGoLive"), text("Common.False"));
		auto *refresh = new QPushButton(text("Account.Refresh"), group);
		connect(refresh, &QPushButton::clicked, this, [this] { refresh_selected_account(); });
		form->addRow(refresh);
		form->addRow(info_card(text("Account.Instructions"), group));
		detail_layout_->addWidget(group);
	}

void BridgeDock::save_local_credentials(const QString &profile_id, const QString &username,
	const QString &server, const QString &key)
{
	Profile *profile = find_profile(profile_id);
	if (!profile || !ProviderRegistry::uses_local_credentials(profile->provider_id))
		return;
	const bool research_provider = ProviderRegistry::is_research(profile->provider_id);
	if (username.trimmed().isEmpty() || server.trimmed().isEmpty() || key.trimmed().isEmpty()) {
		show_transient_error(text(research_provider ? "Research.MissingFields" : "Manual.MissingFields"));
		return;
	}
	if (!TokenStore::save_live_credentials(profile_id, {server.trimmed(), key.trimmed()})) {
		show_transient_error(text(research_provider ? "Research.SaveFailed" : "Manual.SaveFailed"));
		return;
	}
	profile->tiktok_username = username.trimmed();
	profile->can_go_live = true;
	profile->application_status = research_provider ? QStringLiteral("research-local") : QStringLiteral("manual");
	profile->stream_server = server.trimmed();
	profile->stream_key = key.trimmed();
	profile->diagnostic = text(research_provider ? "Research.Saved" : "Manual.Saved");
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
		if (profile->live) {
			QMessageBox::warning(this, text("Profile.ActiveTitle"),
				text("Profile.ActiveDeleteWarning"));
			return;
		}
		if (QMessageBox::question(this, text("Profile.Delete"),
			text("Profile.DeleteConfirmation").arg(profile->display_name),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;
		TokenStore::remove(profile->id);
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
		QSettings settings(profiles_settings_path(), QSettings::IniFormat);
		const int count = settings.beginReadArray(QStringLiteral("profiles"));
		for (int i = 0; i < count; ++i) {
			settings.setArrayIndex(i);
			Profile profile;
			profile.id = settings.value(QStringLiteral("id")).toString();
			profile.provider_id = settings.value(QStringLiteral("provider_id"), ProviderRegistry::streamlabs_id()).toString();
			profile.display_name = settings.value(QStringLiteral("display_name")).toString();
			profile.tiktok_username = settings.value(QStringLiteral("tiktok_username")).toString();
			profile.output_name = settings.value(QStringLiteral("output_name")).toString();
			profile.stream_title = settings.value(QStringLiteral("stream_title")).toString();
			profile.category = settings.value(QStringLiteral("category")).toString();
			profile.category_id = settings.value(QStringLiteral("category_id")).toString();
			profile.mature = settings.value(QStringLiteral("mature"), false).toBool();
			profile.can_go_live = settings.value(QStringLiteral("can_go_live"), false).toBool();
			profile.live = settings.value(QStringLiteral("live"), false).toBool();
			profile.live_id = settings.value(QStringLiteral("live_id")).toString();
			const LiveCredentials credentials = TokenStore::load_live_credentials(profile.id);
			profile.stream_server = credentials.server;
			profile.stream_key = credentials.key;
			profile.application_status = settings.value(QStringLiteral("application_status")).toString();
			if (!profile.id.isEmpty() && !profile.display_name.isEmpty())
				profiles_.push_back(profile);
		}
		settings.endArray();
	}

void BridgeDock::save_profiles() const
	{
		QSettings settings(profiles_settings_path(), QSettings::IniFormat);
		settings.clear();
		settings.beginWriteArray(QStringLiteral("profiles"), static_cast<int>(profiles_.size()));
		for (int i = 0; i < static_cast<int>(profiles_.size()); ++i) {
			const Profile &profile = profiles_.at(i);
			settings.setArrayIndex(i);
			settings.setValue(QStringLiteral("id"), profile.id);
			settings.setValue(QStringLiteral("provider_id"), profile.provider_id);
			settings.setValue(QStringLiteral("display_name"), profile.display_name);
			settings.setValue(QStringLiteral("tiktok_username"), profile.tiktok_username);
			settings.setValue(QStringLiteral("output_name"), profile.output_name);
			settings.setValue(QStringLiteral("stream_title"), profile.stream_title);
			settings.setValue(QStringLiteral("category"), profile.category);
			settings.setValue(QStringLiteral("category_id"), profile.category_id);
			settings.setValue(QStringLiteral("mature"), profile.mature);
			settings.setValue(QStringLiteral("can_go_live"), profile.can_go_live);
			settings.setValue(QStringLiteral("live"), profile.live);
			settings.setValue(QStringLiteral("live_id"), profile.live_id);
			settings.setValue(QStringLiteral("application_status"), profile.application_status);
		}
		settings.endArray();
		settings.sync();
	}
