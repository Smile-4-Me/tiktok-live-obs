// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "bridge_dock.hpp"
#include "aitum_outputs.hpp"
#include "localization.hpp"
#include "plugin_paths.hpp"
#include "tiktok_studio_topics.hpp"
#include "token_store.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

namespace {

QString with_secure_storage_error(const QString &message)
{
	const QString detail = TokenStore::last_error().trimmed();
	return detail.isEmpty() ? message : message + QStringLiteral("\n\n") + detail;
}

} // namespace

void BridgeDock::build_stream_step(const Profile &profile)
	{
		const bool local_provider = ProviderRegistry::uses_local_credentials(profile.provider_id);
		const bool studio_provider = ProviderRegistry::is_tiktok_studio(profile.provider_id);
		if (studio_provider) {
			auto *account_group = new QGroupBox(text("Account.Title"), detail_container_);
			auto *account_form = new QFormLayout(account_group);
			add_tiktok_studio_account_controls(account_form, profile, account_group);
			detail_layout_->addWidget(account_group);
		}
		auto *group = new QGroupBox(text("Stream.Title"), detail_container_);
		auto *form = new QFormLayout(group);
		auto *output_row = new QWidget(group);
			auto *output_layout = new QHBoxLayout(output_row);
			output_layout->setContentsMargins(0, 0, 0, 0);
			auto *output = new QComboBox(output_row);
			output->setEnabled(!profile.live && !profile.preparing);
			output_layout->addWidget(output, 1);
			auto *refresh_outputs = new QToolButton(output_row);
			refresh_outputs->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
			refresh_outputs->setToolTip(text("Stream.ReloadOutputs"));
			refresh_outputs->setFixedSize(30, 30);
			output_layout->addWidget(refresh_outputs);
			const QString profile_id = profile.id;
			auto populate_outputs = [this, output, profile_id, studio_provider] {
				// This editor can outlive a profile-list refresh. Always resolve the
				// profile by its stable ID rather than whichever row happens to be
				// selected when the refresh is processed.
				const Profile *current = find_profile(profile_id);
				const QString saved_name = current ? current->output_name : QString{};
				QString diagnostic;
				const QStringList names = aitum_output_names(&diagnostic);
				const bool aitum_available = aitum_stream_suite_available();
				output->blockSignals(true);
				output->clear();
				output->addItem(studio_provider
					? translated_or("Studio.Stream.NativeOutput", QStringLiteral("TikTok output (inside OBS)"))
					: text("Stream.MainOutput"), QString());
				for (const QString &name : names)
					output->addItem(name, name);
				const int saved_index = output->findData(saved_name);
				output->setCurrentIndex(saved_index >= 0 ? saved_index : 0);
				output->blockSignals(false);
				output->setToolTip(studio_provider
					? translated_or("Studio.Stream.NativeOutputHelp", QStringLiteral(
						"Streams directly from OBS to TikTok without changing the main OBS streaming service."))
					: (!aitum_available ? text("Stream.ManualUsageTooltip")
						: (names.isEmpty() ? diagnostic : QString())));
			};
			populate_outputs();
			connect(refresh_outputs, &QPushButton::clicked, this, populate_outputs);
			form->addRow(text("Stream.Output"), output_row);
		auto *title = new QLineEdit(profile.stream_title, group);
		title->setEnabled(!profile.live && !profile.preparing && !profile.recovering);
		form->addRow(text("Stream.StreamTitle"), title);
		QLineEdit *category = nullptr;
		QComboBox *studio_topic = nullptr;
		QComboBox *studio_game = nullptr;
		QLabel *studio_game_label = nullptr;
		QListWidget *category_choices = nullptr;
		if (studio_provider) {
			studio_topic = new QComboBox(group);
			studio_topic->addItem(translated_or("Studio.Stream.TopicPlaceholder",
				QStringLiteral("Select a topic …")), QString{});
			for (const TikTokStudioTopic &topic : tiktok_studio_topics())
				studio_topic->addItem(topic.name, topic.id);
			const int topic_index = studio_topic->findData(profile.hashtag_id);
			studio_topic->setCurrentIndex(topic_index >= 0 ? topic_index : 0);
			studio_topic->setEnabled(!profile.live && !profile.preparing && !profile.recovering);
			form->addRow(translated_or("Studio.Stream.Topic", QStringLiteral("Topic")), studio_topic);

			studio_game = new QComboBox(group);
			studio_game->setEditable(true);
			studio_game->setInsertPolicy(QComboBox::NoInsert);
			studio_game->addItem(QString{}, QString{});
			if (!profile.category.trimmed().isEmpty()) {
				studio_game->addItem(profile.category, profile.category_id);
				studio_game->setCurrentIndex(1);
			}
			studio_game->lineEdit()->setPlaceholderText(translated_or(
				"Studio.Stream.GamePlaceholder", QStringLiteral("Search games …")));
			if (QCompleter *completer = studio_game->completer()) {
				completer->setCaseSensitivity(Qt::CaseInsensitive);
				completer->setFilterMode(Qt::MatchContains);
				completer->setCompletionMode(QCompleter::PopupCompletion);
			}
			studio_game->setEnabled(!profile.live && !profile.preparing && !profile.recovering);
			category = studio_game->lineEdit();
			studio_game_label = new QLabel(translated_or("Studio.Stream.GameTag", QStringLiteral("Game")), group);
			form->addRow(studio_game_label, studio_game);
			const bool gaming_topic = tiktok_studio_topic_is_gaming(profile.hashtag_id);
			studio_game_label->setVisible(gaming_topic);
			studio_game->setVisible(gaming_topic);
		} else {
			category = new QLineEdit(profile.category, group);
			category->setEnabled(!profile.live && !profile.preparing && !profile.recovering);
			form->addRow(text("Stream.GameCategory"), category);
			category_choices = new QListWidget(group);
			category_choices->setMaximumHeight(100);
			category_choices->hide();
			form->addRow(category_choices);
		}
		auto *mature = new QCheckBox(text("Stream.Mature"), group);
		mature->setChecked(profile.mature);
		mature->setEnabled(!profile.live && !profile.preparing && !profile.recovering);
		form->addRow(mature);
		const FrameSigningCredentials signing_credentials =
			TokenStore::load_frame_signing_credentials(profile.id);
		auto *signing_enabled = new QCheckBox(translated_or("Signing.Enable",
			QStringLiteral("Sign video frames inside OBS (RapidAPI)")), group);
		signing_enabled->setChecked(studio_provider || profile.frame_signing_enabled);
		signing_enabled->setEnabled(!studio_provider && !profile.live && !profile.preparing);
		signing_enabled->setVisible(!studio_provider);
		form->addRow(signing_enabled);
		auto *signing_group = new QGroupBox(translated_or("Signing.Title",
			QStringLiteral("In-process frame signing")), group);
		auto *signing_form = new QFormLayout(signing_group);
		auto *signing_description = new QLabel(translated_or("Signing.Description",
			QStringLiteral("RapidAPI supplies the signatures; OBS inserts them directly into encoded H.264/HEVC packets.")),
			signing_group);
		signing_description->setWordWrap(true);
		signing_form->addRow(signing_description);
		auto *rapidapi_key = new QLineEdit(signing_credentials.rapidapi_key, signing_group);
		rapidapi_key->setEchoMode(QLineEdit::Password);
		auto *tiktok_uid = new QLineEdit(signing_credentials.uid, signing_group);
		auto *device_id = new QLineEdit(signing_credentials.device_id, signing_group);
		auto *room_id = new QLineEdit(signing_credentials.room_id, signing_group);
		auto *api_url = new QLineEdit(signing_credentials.api_url, signing_group);
		signing_form->addRow(translated_or("Signing.RapidApiKey", QStringLiteral("RapidAPI key")), rapidapi_key);
		signing_form->addRow(translated_or("Signing.TikTokUid", QStringLiteral("TikTok UID / anchor ID")), tiktok_uid);
		signing_form->addRow(translated_or("Signing.DeviceId", QStringLiteral("Device ID")), device_id);
		signing_form->addRow(translated_or("Signing.RoomId", QStringLiteral("Room ID override")), room_id);
		signing_form->addRow(translated_or("Signing.ApiUrl", QStringLiteral("Signer API URL")), api_url);
		signing_group->setVisible(!studio_provider && profile.frame_signing_enabled);
		signing_group->setEnabled(!profile.live && !profile.preparing);
		form->addRow(signing_group);
		auto save_signing_credentials = [this, profile_id = profile.id,
			signing_aid = signing_credentials.aid, rapidapi_key, tiktok_uid, device_id, room_id, api_url] {
			FrameSigningCredentials credentials;
			credentials.aid = signing_aid;
			credentials.api_url = api_url->text().trimmed();
			credentials.rapidapi_key = rapidapi_key->text().trimmed();
			credentials.uid = tiktok_uid->text().trimmed();
			credentials.device_id = device_id->text().trimmed();
			credentials.room_id = room_id->text().trimmed();
			if (!TokenStore::save_frame_signing_credentials(profile_id, credentials))
				show_transient_error(translated_or("Signing.SaveFailed",
					QStringLiteral("The frame-signing credentials could not be saved securely.")));
		};
		for (QLineEdit *field : {rapidapi_key, tiktok_uid, device_id, room_id, api_url})
			connect(field, &QLineEdit::editingFinished, this, save_signing_credentials);
		connect(signing_enabled, &QCheckBox::toggled, this,
			[this, profile_id, signing_group, save_signing_credentials](bool enabled) {
				if (Profile *current = find_profile(profile_id)) {
					current->frame_signing_enabled = enabled;
					save_profiles();
					if (enabled)
						save_signing_credentials();
					else {
						output_signing_.detach(current->frame_signing_uses_main_output
							? QString{} : current->output_name);
						current->frame_signing_uses_main_output = false;
					}
				}
				signing_group->setVisible(enabled);
			});
		connect(title, &QLineEdit::textEdited, this, [this, profile_id, title] {
			if (Profile *current = find_profile(profile_id)) {
				current->stream_title = title->text();
				save_profiles();
			}
		});
		if (studio_provider) {
			connect(studio_topic, &QComboBox::currentIndexChanged, this,
				[this, profile_id, studio_topic, studio_game, studio_game_label](int) {
					Profile *current = find_profile(profile_id);
					if (!current || !ProviderRegistry::is_tiktok_studio(current->provider_id))
						return;
					current->hashtag_id = studio_topic->currentData().toString();
					const bool gaming = tiktok_studio_topic_is_gaming(current->hashtag_id);
					studio_game_label->setVisible(gaming);
					studio_game->setVisible(gaming);
					save_profiles();
				});
			connect(studio_game, &QComboBox::currentTextChanged, this,
				[this, profile_id, studio_game](const QString &value) {
					Profile *current = find_profile(profile_id);
					if (!current || !ProviderRegistry::is_tiktok_studio(current->provider_id))
						return;
					current->category = value.trimmed();
					current->category_id.clear();
					for (int index = 1; index < studio_game->count(); ++index) {
						if (studio_game->itemText(index).compare(current->category,
							Qt::CaseInsensitive) == 0) {
							current->category = studio_game->itemText(index);
							current->category_id = studio_game->itemData(index).toString();
							break;
						}
					}
					save_profiles();
				});

			if (!profile.live && !profile.preparing && !profile.recovering) {
				const TikTokStudioAccountCredentials account =
					TokenStore::load_tiktok_studio_account(profile.account_id);
				studio_game->setEnabled(false);
				studio_game->lineEdit()->setPlaceholderText(translated_or(
					"Studio.Stream.GameLoading", QStringLiteral("Loading games …")));
				QPointer<QComboBox> game_guard(studio_game);
				tiktok_studio_.fetch_game_tags(account,
					[this, profile_id, game_guard](QVector<TikTokStudioGameTag> games,
						TikTokStudioAccountCredentials updated_account, QString error) {
						if (Profile *account_profile = find_profile(profile_id);
							account_profile && updated_account.has_device() &&
							!TokenStore::save_tiktok_studio_account(account_profile->account_id, updated_account)) {
							const QString save_error = with_secure_storage_error(translated_or(
								"Studio.Account.SaveFailed",
								QStringLiteral("The refreshed TikTok session could not be saved securely.")));
							error = error.isEmpty() ? save_error : QStringLiteral("%1 (%2)").arg(error, save_error);
						}
						if (!game_guard)
							return;
						Profile *current = find_profile(profile_id);
						if (!current || !selected_profile() || selected_profile()->id != profile_id)
							return;
						game_guard->setEnabled(!current->live && !current->preparing && !current->recovering);
						if (!error.isEmpty() || games.isEmpty()) {
							game_guard->setToolTip(error);
							game_guard->lineEdit()->setPlaceholderText(translated_or(
								"Studio.Stream.GameLoadFailed", QStringLiteral("Games could not be loaded")));
							return;
						}

						const QString saved_name = current->category.trimmed();
						const QString saved_id = current->category_id.trimmed();
						game_guard->blockSignals(true);
						game_guard->clear();
						game_guard->addItem(QString{}, QString{});
						int selected_index = -1;
						for (const TikTokStudioGameTag &game : games) {
							game_guard->addItem(game.name, game.id);
							const int index = game_guard->count() - 1;
							if ((!saved_id.isEmpty() && game.id == saved_id) ||
								(saved_id.isEmpty() && game.name.compare(saved_name,
									Qt::CaseInsensitive) == 0))
								selected_index = index;
						}
						if (selected_index >= 0) {
							game_guard->setCurrentIndex(selected_index);
							current->category = game_guard->itemText(selected_index);
							current->category_id = game_guard->itemData(selected_index).toString();
							save_profiles();
						} else if (!saved_name.isEmpty()) {
							game_guard->setEditText(saved_name);
							current->category_id.clear();
						} else {
							game_guard->setCurrentIndex(0);
						}
						game_guard->blockSignals(false);
						game_guard->setToolTip(QString{});
						game_guard->lineEdit()->setPlaceholderText(translated_or(
							"Studio.Stream.GamePlaceholder", QStringLiteral("Search games …")));
					});
			}
		} else {
			connect(category, &QLineEdit::textEdited, this,
				[this, profile_id, category, category_choices] {
					if (Profile *current = find_profile(profile_id)) {
						current->category = category->text();
						current->category_id.clear();
						save_profiles();
					}
					const QString token = TokenStore::load(profile_id);
					if (token.isEmpty() || category->text().trimmed().isEmpty()) {
						category_choices->hide();
						return;
					}
					streamlabs_.search_categories(token, category->text().trimmed(),
						[this, profile_id, category_choices](QVector<StreamlabsCategory> results, QString) {
							if (!selected_profile() || selected_profile()->id != profile_id)
								return;
							category_choices->clear();
							for (const auto &result : results) {
								auto *item = new QListWidgetItem(result.name, category_choices);
								item->setData(Qt::UserRole, result.id);
							}
							category_choices->setVisible(category_choices->count() > 0);
						});
				});
			connect(category_choices, &QListWidget::itemClicked, this,
				[this, profile_id, category, category_choices](QListWidgetItem *item) {
					if (Profile *current = find_profile(profile_id)) {
						current->category = item->text();
						current->category_id = item->data(Qt::UserRole).toString();
						category->setText(current->category);
						save_profiles();
					}
					category_choices->hide();
				});
		}
		connect(mature, &QCheckBox::toggled, this, [this, profile_id](bool checked) {
			if (Profile *current = find_profile(profile_id)) { current->mature = checked; save_profiles(); }
		});
		add_live_status(form, profile, group);
		detail_layout_->addWidget(group);

		auto *go_live = new QPushButton(studio_provider
			? translated_or("Studio.Stream.Start", QStringLiteral("Create TikTok LIVE"))
			: text(local_provider ? "Manual.Start" : "Stream.Generate"), detail_container_);
		auto *end_live = new QPushButton(studio_provider
			? translated_or("Studio.Stream.End", QStringLiteral("End TikTok LIVE"))
			: text(local_provider ? "Manual.End" : "Stream.End"), detail_container_);
		go_live->setEnabled(!profile.preparing && !profile.live && !profile.recovering &&
			!profile.session_uncertain);
		end_live->setEnabled((profile.live || profile.preparing || profile.session_uncertain) &&
			!profile.ending && !profile.recovering);
		connect(go_live, &QPushButton::clicked, this, [this] { start_selected_live(); });
		connect(end_live, &QPushButton::clicked, this, [this] { end_selected_live(); });
		connect(output, &QComboBox::currentIndexChanged, this, [this, profile_id, output, go_live] {
			if (Profile *current = find_profile(profile_id)) {
				// The empty data value represents the built-in/main OBS output. Real
				// Aitum output names are stored as item data, so the first real output
				// works for every provider too.
				current->output_name = output->currentData().toString();
				save_profiles();
				go_live->setEnabled(!current->preparing && !current->live && !current->recovering &&
					!current->session_uncertain);
				QTimer::singleShot(0, this, [this] { rebuild_profile_list(); });
			}
		});
		detail_layout_->addWidget(go_live);
		if (studio_provider && profile.session_uncertain &&
			!profile.live_id.isEmpty() && !profile.stream_id.isEmpty() &&
			!profile.stream_server.isEmpty() && !profile.stream_key.isEmpty()) {
			auto *resume_live = new QPushButton(translated_or("Studio.Stream.Resume",
				QStringLiteral("Resume TikTok LIVE")), detail_container_);
			resume_live->setEnabled(!profile.preparing && !profile.ending && !profile.recovering);
			connect(resume_live, &QPushButton::clicked, this, [this] {
				if (Profile *current = selected_profile())
					resume_tiktok_studio_live(current->id);
			});
			detail_layout_->addWidget(resume_live);
		}
		detail_layout_->addWidget(end_live);
		if (profile.session_uncertain) {
			auto *reset_session = new QPushButton(translated_or("Studio.Stream.ResetLocalSession",
				QStringLiteral("Reset local LIVE state")), detail_container_);
			connect(reset_session, &QPushButton::clicked, this, [this] {
				Profile *current = selected_profile();
				if (!current || !current->session_uncertain)
					return;
				const QString warning = translated_or("Studio.Stream.ResetLocalSessionConfirm", QStringLiteral(
					"TikTok could not confirm whether this LIVE ended. Resetting only clears OBS's saved state; "
					"it does not end a LIVE that is still active on TikTok. Continue only after checking TikTok."));
				if (QMessageBox::warning(this, text("Plugin.Name"), warning,
					QMessageBox::Reset | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Reset)
					return;
				const QString profile_id = current->id;
				clear_live_session(*current);
				current->diagnostic = translated_or("Studio.Stream.LocalSessionReset",
					QStringLiteral("The saved local LIVE state was reset."));
				current->diagnostic_error = false;
				save_profiles();
				refresh_profile_ui(profile_id);
			});
			detail_layout_->addWidget(reset_session);
		}
		detail_layout_->addWidget(info_card(studio_provider
			? translated_or("Studio.Stream.Description", QStringLiteral(
				"<p><b>Ready to go live.</b></p><p>The built-in TikTok output streams directly from OBS and does not replace your Twitch, YouTube, or other main service.</p>"))
			: text("Stream.Description"), detail_container_));
	}

void BridgeDock::add_live_status(QFormLayout *form, const Profile &profile, QWidget *parent)
	{
		const bool show_status = profile.live || profile.recovering || profile.session_uncertain;
		const bool show_diagnostic = !profile.diagnostic.isEmpty() &&
			(profile.diagnostic_error || profile.preparing || profile.ending || show_status);
		if (!show_status && !show_diagnostic)
			return;
		auto *container = new QWidget(parent);
		auto *layout = new QVBoxLayout(container);
		layout->setContentsMargins(0, 5, 0, 3);
		layout->setSpacing(5);
		if (show_status) {
			QString status_text = text("Stream.StatusLive");
			if (profile.recovering)
				status_text = text("Stream.StatusRecovering");
			else if (profile.session_uncertain)
				status_text = text("Stream.StatusSessionUncertain");
			auto *status = new QLabel(status_text, container);
			status->setStyleSheet(QStringLiteral("QLabel { color: %1; font-weight: 600; }")
				.arg((profile.recovering || profile.session_uncertain)
					? QStringLiteral("#e7af4b") : QStringLiteral("#62c370")));
			layout->addWidget(status);
		}
		if (show_diagnostic) {
			auto *diagnostic = new QLabel(profile.diagnostic, container);
			diagnostic->setWordWrap(true);
			diagnostic->setStyleSheet(QStringLiteral(
				"QLabel { color: %1; background: rgba(90, 120, 160, 0.13); "
				"border: 1px solid rgba(130, 160, 205, 0.35); border-radius: 4px; padding: 6px; }")
				.arg(profile.diagnostic_error ? QStringLiteral("#e05d5d") : QStringLiteral("#8fb4e8")));
			layout->addWidget(diagnostic);
		}
		// LIVE Studio owns its private native output, so exposing the generated
		// RTMP credentials in the dock is unnecessary. Keep them available for
		// providers whose explicit/manual workflow still depends on them.
		if (profile.live && !ProviderRegistry::is_tiktok_studio(profile.provider_id)) {
			layout->addWidget(live_credential_field(text("Stream.Key"), profile.stream_key, true, container));
			layout->addWidget(live_credential_field(text("Stream.Url"), profile.stream_server, false, container));
		}
		form->addRow(container);
	}

void BridgeDock::show_aitum_missing_notice()
	{
		QSettings settings(plugin_settings_path(), QSettings::IniFormat);
		if (settings.value(QStringLiteral("manual/hide_aitum_missing_notice"), false).toBool())
			return;
		QMessageBox notice(QMessageBox::Information, text("Plugin.Name"),
			translated_or("Manual.AitumMissing", QStringLiteral(
				"Aitum Stream Suite was not found. This LIVE is ready; start the main OBS stream when you are ready to broadcast.")),
			QMessageBox::Ok, this);
		QCheckBox dont_show(translated_or("Manual.AitumMissingDontShow",
			QStringLiteral("Don't show this message again")), &notice);
		notice.setCheckBox(&dont_show);
		notice.exec();
		if (dont_show.isChecked()) {
			settings.setValue(QStringLiteral("manual/hide_aitum_missing_notice"), true);
			settings.sync();
		}
	}

QWidget *BridgeDock::live_credential_field(const QString &label, const QString &value, bool concealed, QWidget *parent)
	{
		auto *row = new QWidget(parent);
		auto *layout = new QHBoxLayout(row);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(5);
		auto *caption = new QLabel(label, row);
		caption->setMinimumWidth(78);
		layout->addWidget(caption);
		auto *field = new QLineEdit(value, row);
		field->setReadOnly(true);
		field->setEchoMode(concealed ? QLineEdit::Password : QLineEdit::Normal);
		field->setStyleSheet(QStringLiteral("QLineEdit { padding-left: 8px; }"));
		layout->addWidget(field, 1);
		if (concealed) {
			auto *reveal = new QToolButton(row);
			reveal->setText(text("Stream.Show"));
			reveal->setToolTip(text("Stream.Show"));
			connect(reveal, &QToolButton::clicked, this, [field, reveal] {
				const bool hidden = field->echoMode() == QLineEdit::Password;
				field->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
				reveal->setText(hidden ? text("Stream.Hide") : text("Stream.Show"));
				reveal->setToolTip(reveal->text());
			});
			layout->addWidget(reveal);
		}
		auto *copy = new QToolButton(row);
		copy->setText(text("Stream.Copy"));
		copy->setToolTip(text("Stream.Copy"));
		connect(copy, &QToolButton::clicked, this, [field] {
			QGuiApplication::clipboard()->setText(field->text());
		});
		layout->addWidget(copy);
		return row;
	}

void BridgeDock::prepare_output_signing(const QString &profile_id, const QString &output_name,
	const QString &session_room_id, OutputSigningManager::Completion completion)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile) {
			completion(false, QStringLiteral("The selected profile no longer exists."));
			return;
		}
		if (!profile->frame_signing_enabled) {
			const QString previous_output = profile->frame_signing_output_name.isEmpty()
				? (profile->frame_signing_uses_main_output ? QString{} : output_name)
				: profile->frame_signing_output_name;
			output_signing_.detach(previous_output);
			profile->frame_signing_uses_main_output = false;
			profile->frame_signing_output_name.clear();
			completion(true, {});
			return;
		}
		profile->frame_signing_uses_main_output = output_name.trimmed().isEmpty();
		profile->frame_signing_output_name = output_name.trimmed();
		const FrameSigningCredentials credentials = TokenStore::load_frame_signing_credentials(profile_id);
		const QString configured_room_id = credentials.room_id.trimmed();
		const QString room_id = configured_room_id.isEmpty() ? session_room_id.trimmed() : configured_room_id;
		FrameSignApiConfig api;
		api.base_url = QUrl(credentials.api_url);
		api.rapidapi_key = credentials.rapidapi_key;
		SignedSeiConfig signing;
		signing.aid = credentials.aid;
		signing.uid = credentials.uid;
		signing.device_id = credentials.device_id;
		signing.room_id = room_id;
		output_signing_.prepare_and_attach(output_name, std::move(api), std::move(signing),
			std::move(completion));
	}

bool BridgeDock::output_in_use_by_another_profile(const Profile &profile) const
	{
		const bool main_output = profile.output_name.isEmpty() || !aitum_stream_suite_available();
		for (const Profile &candidate : profiles_) {
			if (candidate.id == profile.id ||
				(!candidate.live && !candidate.preparing && !candidate.session_uncertain))
				continue;
			const bool candidate_main_output = candidate.output_name.isEmpty() || !aitum_stream_suite_available();
			if ((main_output && candidate_main_output) ||
				(!main_output && !candidate_main_output && candidate.output_name == profile.output_name))
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
		if (ProviderRegistry::uses_local_credentials(profile->provider_id)) {
			clear_live_session(*profile);
			profile->diagnostic = text("Diagnostic.OutputNotActiveEnded");
			profile->diagnostic_error = true;
			outputs_preparing_.remove(output_name);
			save_profiles();
			refresh_profile_ui(profile_id);
			return;
		}
		if (ProviderRegistry::is_tiktok_studio(profile->provider_id)) {
			TikTokStudioLive live;
			live.account = TokenStore::load_tiktok_studio_account(profile->account_id);
			live.room_id = profile->live_id;
			live.stream_id = profile->stream_id;
			live.server = profile->stream_server;
			live.key = profile->stream_key;
			fail_tiktok_studio_start(profile_id, output_name, true, std::move(live),
				text("Error.AitumOutputNotActive").arg(output_name));
			return;
		}
		const QString token = TokenStore::load(profile->id);
		const QString live_id = profile->live_id;
		profile->ending = true;
		profile->diagnostic = text("Diagnostic.OutputNotActiveEnding");
		profile->diagnostic_error = true;
		save_profiles();
		refresh_profile_ui(profile_id);
		streamlabs_.end_live(token, live_id, [this, profile_id, output_name](StreamlabsEndResult result) {
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

void BridgeDock::start_selected_live()
	{
		Profile *profile = selected_profile();
		if (!profile)
			return;
		start_profile_live(profile->id, false);
	}

void BridgeDock::start_tiktok_studio_live(const QString &profile_id, bool start_aitum_output)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile || !ProviderRegistry::is_tiktok_studio(profile->provider_id))
			return;
		const TikTokStudioAccountCredentials account =
			TokenStore::load_tiktok_studio_account(profile->account_id);
		if (!account.has_login()) {
			if (start_aitum_output)
				outputs_preparing_.remove(profile->output_name);
			show_transient_error(translated_or("Studio.Error.MissingLogin",
				QStringLiteral("The saved TikTok login is missing. Log in again with a QR code.")));
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
		if (profile->output_name.trimmed().isEmpty()) {
			const QString codec = native_output_.configured_video_codec();
			if (!codec.isEmpty() && codec != QStringLiteral("h264") && codec != QStringLiteral("avc") &&
				codec != QStringLiteral("hevc") && codec != QStringLiteral("h265")) {
				if (start_aitum_output)
					outputs_preparing_.remove(profile->output_name);
				show_transient_error(translated_or("Studio.Stream.VideoCodecUnsupported",
					QStringLiteral("TikTok LIVE requires an H.264 or HEVC streaming encoder. Change it in OBS Settings → Output before creating the LIVE.")));
				return;
			}
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

		tiktok_studio_.find_continuable_live(account,
			[this, profile_id, output_name, start_aitum_output](TikTokStudioLive live, QString error) mutable {
				Profile *current = find_profile(profile_id);
				if (!current) {
					outputs_preparing_.remove(output_name);
					return;
				}

				const bool account_saved = !live.account.has_device() ||
					TokenStore::save_tiktok_studio_account(current->account_id, live.account);
				if (!account_saved)
					error = with_secure_storage_error(translated_or("Studio.Account.SaveFailed",
						QStringLiteral("The refreshed TikTok session could not be saved securely.")));

				if (!live.room_id.isEmpty() && !live.stream_id.isEmpty()) {
					current->preparing = false;
					// A reusable TikTok room is not evidence that OBS is currently
					// streaming. Keep it as an unresolved session until the user
					// resumes or ends it.
					current->live = false;
					current->session_uncertain = true;
					current->live_id = live.room_id;
					current->stream_id = live.stream_id;
					current->stream_server = live.server;
					current->stream_key = live.key;
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
				create_tiktok_studio_live_session(profile_id, start_aitum_output,
					std::move(live.account));
			});
	}

void BridgeDock::create_tiktok_studio_live_session(const QString &profile_id,
	bool start_aitum_output, TikTokStudioAccountCredentials account)
{
	Profile *profile = find_profile(profile_id);
	if (!profile)
		return;
	const QString output_name = profile->output_name;
	const QString hashtag_id = profile->hashtag_id;
	const QString game_tag_id = tiktok_studio_topic_is_gaming(hashtag_id)
		? profile->category_id : QStringLiteral("0");
	profile->diagnostic = text("Diagnostic.CreatingSession");
	profile->diagnostic_error = false;
	save_profiles();
	refresh_profile_ui(profile_id);

	tiktok_studio_.start_live(std::move(account), profile->stream_title, hashtag_id,
		game_tag_id, profile->mature,
		[this, profile_id, output_name, start_aitum_output](TikTokStudioLive live, QString error) mutable {
			Profile *current = find_profile(profile_id);
			if (!current) {
				if (!live.room_id.isEmpty())
					tiktok_studio_.end_live(live.account, live.room_id, live.stream_id,
						[](TikTokStudioEndResult) {});
				outputs_preparing_.remove(output_name);
				return;
			}
			if (live.account.has_device() &&
				!TokenStore::save_tiktok_studio_account(current->account_id, live.account)) {
				const QString save_error = with_secure_storage_error(translated_or(
					"Studio.Account.SaveFailed",
					QStringLiteral("The updated TikTok session could not be saved securely.")));
				error = error.isEmpty() ? save_error : QStringLiteral("%1 (%2)").arg(error, save_error);
			}
			if (!error.isEmpty()) {
				if (!live.room_id.isEmpty() && !live.stream_id.isEmpty() && live.account.has_login()) {
					current->live = false;
					current->live_id = live.room_id;
					current->stream_id = live.stream_id;
					current->stream_server = live.server;
					current->stream_key = live.key;
					tiktok_studio_heartbeat_status_.insert(profile_id, 1);
					tiktok_studio_stale_heartbeat_count_.insert(profile_id, 0);
					fail_tiktok_studio_start(profile_id, output_name, start_aitum_output,
						std::move(live), error);
					return;
				}
				current->preparing = false;
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

void BridgeDock::resume_tiktok_studio_live(const QString &profile_id, bool start_aitum_output)
{
	Profile *profile = find_profile(profile_id);
	if (!profile || !ProviderRegistry::is_tiktok_studio(profile->provider_id) ||
		(!profile->live && !profile->session_uncertain) || profile->preparing || profile->ending || profile->recovering)
		return;
	const TikTokStudioAccountCredentials account =
		TokenStore::load_tiktok_studio_account(profile->account_id);
	if (!account.has_login()) {
		show_transient_error(translated_or("Studio.Error.MissingLogin",
			QStringLiteral("The saved TikTok login is missing. Log in again with a QR code.")));
		return;
	}
	if (profile->output_name.trimmed().isEmpty()) {
		const QString codec = native_output_.configured_video_codec();
		if (!codec.isEmpty() && codec != QStringLiteral("h264") && codec != QStringLiteral("avc") &&
			codec != QStringLiteral("hevc") && codec != QStringLiteral("h265")) {
			show_transient_error(translated_or("Studio.Stream.VideoCodecUnsupported", QStringLiteral(
				"TikTok LIVE requires an H.264 or HEVC streaming encoder. Change it in OBS Settings → Output before resuming the LIVE.")));
			return;
		}
	}

	const QString output_name = profile->output_name;
	profile->preparing = true;
	profile->recovering = true;
	profile->diagnostic = translated_or("Studio.Recovery.Resuming",
		QStringLiteral("Reconnecting OBS to the existing TikTok LIVE …"));
	profile->diagnostic_error = false;
	save_profiles();
	refresh_profile_ui(profile_id);

	tiktok_studio_.resume_live(account,
		[this, profile_id, output_name, start_aitum_output](TikTokStudioLive live, QString error) mutable {
			Profile *current = find_profile(profile_id);
			if (!current)
				return;
			current->recovering = false;
			if (live.account.has_device() &&
				!TokenStore::save_tiktok_studio_account(current->account_id, live.account)) {
				const QString save_error = with_secure_storage_error(translated_or(
					"Studio.Account.SaveFailed",
					QStringLiteral("The updated TikTok session could not be saved securely.")));
				error = error.isEmpty() ? save_error : QStringLiteral("%1 (%2)").arg(error, save_error);
			}
			if (!error.isEmpty()) {
				current->preparing = false;
				current->session_uncertain = true;
				if (!live.room_id.isEmpty()) {
					current->live_id = live.room_id;
					current->stream_id = live.stream_id;
					current->stream_server = live.server;
					current->stream_key = live.key;
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
	bool start_aitum_output, TikTokStudioLive live)
{
	Profile *current = find_profile(profile_id);
	if (!current) {
		if (!live.room_id.isEmpty())
			tiktok_studio_.end_live(live.account, live.room_id, live.stream_id,
				[](TikTokStudioEndResult) {});
		outputs_preparing_.remove(output_name);
		return;
	}
	// Creating a TikTok room reserves credentials, but the stream is only live
	// after the selected output reports that its encoder actually started.
	current->live = false;
	current->preparing = true;
	current->recovering = false;
	current->session_uncertain = false;
	current->live_id = live.room_id;
	current->stream_id = live.stream_id;
	current->stream_server = live.server;
	current->stream_key = live.key;
	tiktok_studio_heartbeat_status_.insert(profile_id, 1);
	tiktok_studio_stale_heartbeat_count_.insert(profile_id, 0);
	current->diagnostic = output_name.isEmpty() || !aitum_stream_suite_available()
		? translated_or("Signing.Prefetching", QStringLiteral("Prefetching frame signatures …"))
		: text("Diagnostic.UpdatingAitum");
	current->diagnostic_error = false;
	TokenStore::save_live_credentials(profile_id, {live.server, live.key});

	FrameSigningCredentials signing = TokenStore::load_frame_signing_credentials(profile_id);
	signing.api_url = live.account.signer_api_url;
	signing.rapidapi_key = live.account.rapidapi_key;
	signing.uid = live.owner_user_id.trimmed().isEmpty()
		? live.account.user_id : live.owner_user_id;
	signing.device_id = live.account.device_id;
	signing.room_id.clear();
	if (!TokenStore::save_tiktok_studio_account(current->account_id, live.account) ||
		!TokenStore::save_frame_signing_credentials(profile_id, signing)) {
		fail_tiktok_studio_start(profile_id, output_name, start_aitum_output,
			std::move(live), translated_or("Studio.Account.SaveFailed",
				QStringLiteral("The updated TikTok session could not be saved securely.")));
		return;
	}
	save_profiles();
	refresh_profile_ui(profile_id);

	const bool aitum_available = aitum_stream_suite_available();
	if (output_name.isEmpty()) {
		prepare_tiktok_studio_native_output(profile_id, std::move(live));
		return;
	}
	if (!aitum_available) {
		fail_tiktok_studio_start(profile_id, output_name, start_aitum_output,
			std::move(live), translated_or("Studio.Stream.AitumMissing", QStringLiteral(
				"The selected Aitum output is no longer available. Choose the built-in TikTok output and try again.")));
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

	void BridgeDock::prepare_tiktok_studio_native_output(const QString &profile_id, TikTokStudioLive live)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile) {
			tiktok_studio_.end_live(live.account, live.room_id, live.stream_id,
				[](TikTokStudioEndResult) {});
			return;
		}
		const NativeOutputManager::CreateResult created = native_output_.create(
			profile_id, live.server, live.key);
		if (!created.error.isEmpty()) {
			fail_tiktok_studio_start(profile_id, created.output_name, false,
				std::move(live), created.error);
			return;
		}
		profile->preparing = true;
		profile->diagnostic = translated_or("Signing.Prefetching",
			QStringLiteral("Prefetching frame signatures …"));
		profile->diagnostic_error = false;
		save_profiles();
		refresh_profile_ui(profile_id);

		const QString signing_room_id = live.room_id;
		prepare_output_signing(profile_id, created.output_name, signing_room_id,
			[this, profile_id, output_name = created.output_name, live = std::move(live)]
			(bool attached, QString signing_error) mutable {
				Profile *current = find_profile(profile_id);
				if (!current) {
					output_signing_.detach(output_name);
					native_output_.remove(profile_id);
					if (!live.room_id.isEmpty())
						tiktok_studio_.end_live(live.account, live.room_id, live.stream_id,
							[](TikTokStudioEndResult) {});
					return;
				}
				if (!attached) {
					fail_tiktok_studio_start(profile_id, output_name, false,
						std::move(live), signing_error);
					return;
				}
				QString start_error;
				if (!native_output_.start(profile_id, &start_error)) {
					fail_tiktok_studio_start(profile_id, output_name, false,
						std::move(live), start_error);
					return;
				}
				current->diagnostic = translated_or("Studio.Stream.NativeStarting",
					QStringLiteral("Connecting OBS directly to TikTok …"));
				current->diagnostic_error = false;
				save_profiles();
				refresh_profile_ui(profile_id);
				QTimer::singleShot(500, this, [this, profile_id] {
					verify_tiktok_studio_native_output(profile_id, 0);
				});
			});
	}

	void BridgeDock::verify_tiktok_studio_native_output(const QString &profile_id, int attempt)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile || !profile->live || profile->ending)
			return;
		if (native_output_.active(profile_id)) {
			profile->preparing = false;
			profile->diagnostic = translated_or("Studio.Stream.NativeLive",
				QStringLiteral("OBS is streaming directly to TikTok."));
			profile->diagnostic_error = false;
			save_profiles();
			refresh_profile_ui(profile_id);
			return;
		}
		constexpr int maximum_attempts = 20;
		if (attempt < maximum_attempts) {
			QTimer::singleShot(500, this, [this, profile_id, attempt] {
				verify_tiktok_studio_native_output(profile_id, attempt + 1);
			});
			return;
		}

		QString output_error = native_output_.last_error(profile_id);
		if (output_error.isEmpty())
			output_error = translated_or("Studio.Stream.NativeStartFailed",
				QStringLiteral("OBS could not connect the native TikTok output."));
		TikTokStudioLive live;
		live.account = TokenStore::load_tiktok_studio_account(profile->account_id);
		live.room_id = profile->live_id;
		live.stream_id = profile->stream_id;
		live.server = profile->stream_server;
		live.key = profile->stream_key;
		fail_tiktok_studio_start(profile_id, native_output_.output_name(profile_id), false,
			std::move(live), output_error);
	}

	void BridgeDock::prepare_tiktok_studio_output(const QString &profile_id, const QString &output_name,
		bool start_aitum_output, TikTokStudioLive live)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile) {
			outputs_preparing_.remove(output_name);
			tiktok_studio_.end_live(live.account, live.room_id, live.stream_id,
				[](TikTokStudioEndResult) {});
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
			profile->preparing = false;
			outputs_preparing_.remove(output_name);
			save_profiles();
			refresh_profile_ui(profile_id);
			return;
		}
		start_aitum_output_and_verify(profile_id, output_name,
			[this, profile_id, output_name](const QString &diagnostic) {
				if (Profile *current = find_profile(profile_id)) {
					TikTokStudioLive failed_live;
					failed_live.account = TokenStore::load_tiktok_studio_account(current->account_id);
					failed_live.room_id = current->live_id;
					failed_live.stream_id = current->stream_id;
					failed_live.server = current->stream_server;
					failed_live.key = current->stream_key;
					fail_tiktok_studio_start(profile_id, output_name, true,
						std::move(failed_live), text("OneClick.StartFailed").arg(diagnostic));
				}
			});
	}

	void BridgeDock::fail_tiktok_studio_start(const QString &profile_id, const QString &output_name,
		bool start_aitum_output, TikTokStudioLive live, const QString &reason)
	{
		output_signing_.detach(output_name);
		native_output_.remove(profile_id);
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
		tiktok_studio_.end_live(live.account, live.room_id, live.stream_id,
			[this, profile_id, output_name, start_aitum_output, reason](TikTokStudioEndResult result) {
				if (start_aitum_output)
					outputs_preparing_.remove(output_name);
				Profile *current = find_profile(profile_id);
				if (!current)
					return;
				current->ending = false;
				TokenStore::save_tiktok_studio_account(current->account_id, result.account);
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

void BridgeDock::start_profile_live(const QString &profile_id, bool start_aitum_output)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile || (start_aitum_output && profile->output_name.isEmpty())) {
			if (start_aitum_output && profile)
				outputs_preparing_.remove(profile->output_name);
			return;
		}
		if (profile->recovering) {
			if (start_aitum_output)
				outputs_preparing_.remove(profile->output_name);
			return;
		}
		if (output_in_use_by_another_profile(*profile)) {
			show_transient_error(text("Error.OutputInUse"));
			if (start_aitum_output)
				outputs_preparing_.remove(profile->output_name);
			return;
		}
		if (const Profile *active = active_profile_for_tiktok_account(*profile)) {
			show_transient_error(tiktok_account_conflict_message(*profile, *active));
			if (start_aitum_output)
				outputs_preparing_.remove(profile->output_name);
			return;
		}
		if (ProviderRegistry::is_tiktok_studio(profile->provider_id)) {
			start_tiktok_studio_live(profile_id, start_aitum_output);
			return;
		}
		if (ProviderRegistry::uses_local_credentials(profile->provider_id)) {
			const LiveCredentials credentials = TokenStore::load_live_credentials(profile->id);
			if (credentials.server.isEmpty() || credentials.key.isEmpty()) {
				show_transient_error(text("Manual.MissingCredentials"));
				if (start_aitum_output)
					outputs_preparing_.remove(profile->output_name);
				return;
			}
			profile->preparing = true;
			profile->live = true;
			profile->stream_server = credentials.server;
			profile->stream_key = credentials.key;
			profile->diagnostic = start_aitum_output ? text("Diagnostic.UpdatingAitum")
				: text("Manual.SessionReady");
			profile->diagnostic_error = false;
			const QString manual_profile_id = profile->id;
			const QString manual_output_name = profile->output_name;
			save_profiles();
			rebuild_profile_list();
			if (selected_profile() == profile)
				show_selected_profile();
			if (!start_aitum_output &&
				(manual_output_name.isEmpty() || !aitum_stream_suite_available())) {
				const bool aitum_available = aitum_stream_suite_available();
				if (!profile->frame_signing_enabled) {
					profile->preparing = false;
					save_profiles();
					refresh_profile_ui(manual_profile_id);
					if (!aitum_available)
						show_aitum_missing_notice();
					return;
				}
				profile->diagnostic = translated_or("Signing.Prefetching",
					QStringLiteral("Prefetching frame signatures …"));
				save_profiles();
				refresh_profile_ui(manual_profile_id);
				prepare_output_signing(manual_profile_id, QString{}, QString{},
					[this, manual_profile_id, aitum_available](bool attached, QString signing_error) {
						Profile *prepared = find_profile(manual_profile_id);
						if (!prepared) {
							output_signing_.detach(QString{});
							return;
						}
						prepared->preparing = false;
						if (!attached) {
							clear_live_session(*prepared);
							prepared->diagnostic = text("Diagnostic.Failed").arg(signing_error);
							prepared->diagnostic_error = true;
							show_transient_error(signing_error);
						} else {
							prepared->diagnostic = text("Manual.SessionReady");
							prepared->diagnostic_error = false;
							if (!aitum_available)
								show_aitum_missing_notice();
						}
						save_profiles();
						refresh_profile_ui(manual_profile_id);
					});
				return;
			}
			update_aitum_output_for_profile(manual_profile_id, manual_output_name,
				credentials.server, credentials.key,
				[this, manual_profile_id, manual_output_name, start_aitum_output](BridgeResult result) {
					Profile *current = find_profile(manual_profile_id);
					if (!current) {
						outputs_preparing_.remove(manual_output_name);
						return;
					}
					current->preparing = false;
					if (result != BridgeResult::Success) {
						clear_live_session(*current);
						current->diagnostic = text("Diagnostic.Failed").arg(text("Error.AitumUpdateFailed"));
						current->diagnostic_error = true;
						outputs_preparing_.remove(manual_output_name);
						save_profiles();
						refresh_profile_ui(manual_profile_id);
						return;
					}
					current->preparing = current->frame_signing_enabled;
					current->diagnostic = current->frame_signing_enabled
						? translated_or("Signing.Prefetching", QStringLiteral("Prefetching frame signatures …"))
						: text("Diagnostic.StartingOutput");
					save_profiles();
					refresh_profile_ui(manual_profile_id);
					prepare_output_signing(manual_profile_id, manual_output_name, {},
						[this, manual_profile_id, manual_output_name, start_aitum_output](bool attached, QString signing_error) {
							Profile *prepared = find_profile(manual_profile_id);
							if (!prepared) {
								outputs_preparing_.remove(manual_output_name);
								return;
							}
							prepared->preparing = false;
							if (!attached) {
								clear_live_session(*prepared);
								prepared->diagnostic = text("Diagnostic.Failed").arg(signing_error);
								prepared->diagnostic_error = true;
								outputs_preparing_.remove(manual_output_name);
								save_profiles();
								refresh_profile_ui(manual_profile_id);
								show_transient_error(signing_error);
								return;
							}
							prepared->diagnostic = start_aitum_output ? text("Diagnostic.StartingOutput")
								: text("Manual.SessionReady");
							prepared->diagnostic_error = false;
							save_profiles();
							refresh_profile_ui(manual_profile_id);
							if (!start_aitum_output)
								return;
							start_aitum_output_and_verify(manual_profile_id, manual_output_name,
								[this, manual_profile_id](const QString &diagnostic) {
									if (Profile *failed = find_profile(manual_profile_id)) {
										clear_live_session(*failed);
										failed->diagnostic = text("OneClick.StartFailed").arg(diagnostic);
										failed->diagnostic_error = true;
										save_profiles();
										refresh_profile_ui(manual_profile_id);
									}
								});
						});
				});
			return;
		}
		const QString token = TokenStore::load(profile->id);
		if (token.isEmpty()) {
			show_transient_error(text("Error.MissingToken"));
			if (start_aitum_output)
				outputs_preparing_.remove(profile->output_name);
			return;
		}
		profile->preparing = true;
		profile->ending = false;
		profile->session_uncertain = false;
		profile->diagnostic = text("Diagnostic.CreatingSession");
		profile->diagnostic_error = false;
		const QString active_profile_id = profile->id;
		const QString output_name = profile->output_name;
		save_profiles();
		rebuild_profile_list();
		if (selected_profile() == profile)
			show_selected_profile();
		streamlabs_.start_live(token, profile->stream_title, profile->category_id, profile->mature,
			[this, active_profile_id, output_name, token, start_aitum_output](StreamlabsLive live, QString error) {
				Profile *current = find_profile(active_profile_id);
				if (!current) {
					if (start_aitum_output)
						outputs_preparing_.remove(output_name);
					return;
				}
				if (!error.isEmpty()) {
					current->preparing = false;
					current->diagnostic = text("Diagnostic.Failed").arg(error);
					current->diagnostic_error = true;
					save_profiles();
					rebuild_profile_list();
					if (selected_profile() == current)
						show_selected_profile();
					if (start_aitum_output)
						outputs_preparing_.remove(output_name);
					show_transient_error(error);
					return;
				}
				if (!start_aitum_output && (output_name.isEmpty() || !aitum_stream_suite_available())) {
					const bool aitum_available = aitum_stream_suite_available();
					current->preparing = current->frame_signing_enabled;
					current->live = true;
					current->live_id = live.id;
					current->stream_server = live.server;
					current->stream_key = live.key;
					current->diagnostic = current->frame_signing_enabled
						? translated_or("Signing.Prefetching", QStringLiteral("Prefetching frame signatures …"))
						: text("Diagnostic.SessionReady");
					current->diagnostic_error = false;
					TokenStore::save_live_credentials(active_profile_id, {live.server, live.key});
					save_profiles();
					rebuild_profile_list();
					if (selected_profile() == current)
						show_selected_profile();
					if (!current->frame_signing_enabled) {
						if (!aitum_available)
							show_aitum_missing_notice();
						return;
					}
					prepare_output_signing(active_profile_id, QString{}, live.id,
						[this, active_profile_id, token, live, aitum_available]
						(bool attached, QString signing_error) {
							Profile *prepared = find_profile(active_profile_id);
							if (!prepared) {
								output_signing_.detach(QString{});
								return;
							}
							prepared->preparing = false;
							if (attached) {
								prepared->diagnostic = text("Diagnostic.SessionReady");
								prepared->diagnostic_error = false;
								save_profiles();
								refresh_profile_ui(active_profile_id);
								if (!aitum_available)
									show_aitum_missing_notice();
								return;
							}
							prepared->ending = true;
							prepared->diagnostic = text("Diagnostic.Failed").arg(signing_error);
							prepared->diagnostic_error = true;
							save_profiles();
							refresh_profile_ui(active_profile_id);
							show_transient_error(signing_error);
							streamlabs_.end_live(token, live.id,
								[this, active_profile_id, live, signing_error]
								(StreamlabsEndResult end_result) {
									Profile *cleanup = find_profile(active_profile_id);
									if (!cleanup)
										return;
									cleanup->ending = false;
									if (end_result.ended || end_result.stale_session) {
										clear_live_session(*cleanup);
									} else {
										cleanup->live = true;
										cleanup->live_id = live.id;
										cleanup->stream_server = live.server;
										cleanup->stream_key = live.key;
										cleanup->session_uncertain = true;
									}
									cleanup->diagnostic = text("Diagnostic.Failed").arg(signing_error);
									cleanup->diagnostic_error = true;
									save_profiles();
									refresh_profile_ui(active_profile_id);
								});
						});
					return;
				}
				current->diagnostic = text("Diagnostic.UpdatingAitum");
				current->diagnostic_error = false;
				rebuild_profile_list();
				if (selected_profile() == current)
					show_selected_profile();
				update_aitum_output_for_profile(active_profile_id, output_name, live.server, live.key,
					[this, active_profile_id, output_name, token, live, start_aitum_output](BridgeResult result) {
					Profile *updated = find_profile(active_profile_id);
					if (!updated) {
						if (start_aitum_output)
							outputs_preparing_.remove(output_name);
						return;
					}
					updated->preparing = false;
					if (result == BridgeResult::Success) {
						updated->live = true;
						updated->live_id = live.id;
						updated->stream_server = live.server;
						updated->stream_key = live.key;
						updated->preparing = updated->frame_signing_enabled;
						updated->diagnostic = updated->frame_signing_enabled
							? translated_or("Signing.Prefetching", QStringLiteral("Prefetching frame signatures …"))
							: (start_aitum_output ? text("Diagnostic.StartingOutput") : text("Diagnostic.SessionReady"));
						updated->diagnostic_error = false;
						TokenStore::save_live_credentials(active_profile_id, {live.server, live.key});
						save_profiles();
						rebuild_profile_list();
						if (selected_profile() == updated)
							show_selected_profile();
						prepare_output_signing(active_profile_id, output_name, live.id,
							[this, active_profile_id, output_name, token, live, start_aitum_output]
							(bool attached, QString signing_error) {
								Profile *prepared = find_profile(active_profile_id);
								if (!prepared) {
									outputs_preparing_.remove(output_name);
									return;
								}
								prepared->preparing = false;
								if (!attached) {
									prepared->ending = true;
									prepared->diagnostic = text("Diagnostic.Failed").arg(signing_error);
									prepared->diagnostic_error = true;
									save_profiles();
									refresh_profile_ui(active_profile_id);
									show_transient_error(signing_error);
									streamlabs_.end_live(token, live.id,
										[this, active_profile_id, output_name, live, signing_error]
										(StreamlabsEndResult end_result) {
											Profile *cleanup = find_profile(active_profile_id);
											outputs_preparing_.remove(output_name);
											if (!cleanup)
												return;
											cleanup->ending = false;
											if (end_result.ended || end_result.stale_session) {
												clear_live_session(*cleanup);
											} else {
												cleanup->live = true;
												cleanup->live_id = live.id;
												cleanup->stream_server = live.server;
												cleanup->stream_key = live.key;
												cleanup->session_uncertain = true;
											}
											cleanup->diagnostic = text("Diagnostic.Failed").arg(signing_error);
											cleanup->diagnostic_error = true;
											save_profiles();
											refresh_profile_ui(active_profile_id);
										});
									return;
								}
								prepared->diagnostic = start_aitum_output ? text("Diagnostic.StartingOutput")
									: text("Diagnostic.SessionReady");
								prepared->diagnostic_error = false;
								save_profiles();
								refresh_profile_ui(active_profile_id);
								if (!start_aitum_output)
									return;
								start_aitum_output_and_verify(active_profile_id, output_name,
									[this, active_profile_id, output_name](const QString &diagnostic) {
										if (Profile *current = find_profile(active_profile_id)) {
											current->diagnostic = text("Diagnostic.Failed").arg(
												text("OneClick.StartFailed").arg(diagnostic));
											current->diagnostic_error = true;
											save_profiles();
											refresh_profile_ui(active_profile_id);
										}
										output_signing_.detach(output_name);
										show_transient_error(text("OneClick.StartFailed").arg(diagnostic));
									});
							});
						return;
					}
					// A key exists now, so retain the reservation until Streamlabs has
					// positively confirmed the cleanup. Otherwise a second profile could
					// overwrite the same Aitum output while this LIVE still exists.
					streamlabs_.end_live(token, live.id, [this, active_profile_id, output_name, live, start_aitum_output](StreamlabsEndResult end_result) {
						Profile *cleanup = find_profile(active_profile_id);
						if (!cleanup) {
							if (start_aitum_output)
								outputs_preparing_.remove(output_name);
							return;
						}
						cleanup->preparing = false;
						if (!end_result.ended && !end_result.stale_session) {
							cleanup->live = true;
							cleanup->live_id = live.id;
							cleanup->stream_server = live.server;
							cleanup->stream_key = live.key;
							cleanup->diagnostic = text("Diagnostic.Failed").arg(text("Error.AitumUpdateEndFailed"));
							cleanup->diagnostic_error = true;
							TokenStore::save_live_credentials(active_profile_id, {live.server, live.key});
							save_profiles();
							rebuild_profile_list();
							if (selected_profile() == cleanup)
								show_selected_profile();
							if (start_aitum_output)
								outputs_preparing_.remove(output_name);
							show_transient_error(text("Error.AitumUpdateEndFailed"));
							return;
						}
						if (start_aitum_output)
							outputs_preparing_.remove(output_name);
						cleanup->diagnostic = text("Diagnostic.Failed").arg(text("Error.AitumUpdateFailed"));
						cleanup->diagnostic_error = true;
						save_profiles();
						refresh_profile_ui(active_profile_id);
						show_transient_error(text("Error.AitumUpdateFailed"));
					});
				});
			});
	}

void BridgeDock::end_selected_live()
	{
		Profile *profile = selected_profile();
		if (profile)
			end_profile_live(profile->id);
	}

void BridgeDock::end_live_for_output(const QString &output_name)
	{
		if (Profile *profile = live_profile_for_output(output_name))
			end_profile_live(profile->id);
	}

void BridgeDock::end_profile_live(const QString &profile_id)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile || (!profile->live && !profile->preparing && !profile->session_uncertain) ||
			profile->ending || profile->recovering)
			return;
		if (ProviderRegistry::is_tiktok_studio(profile->provider_id)) {
			if (native_output_.contains(profile_id)) {
				output_signing_.detach(profile->frame_signing_output_name);
				native_output_.remove(profile_id);
			}
			const TikTokStudioAccountCredentials account =
				TokenStore::load_tiktok_studio_account(profile->account_id);
			profile->ending = true;
			profile->diagnostic = text("Diagnostic.EndingSession");
			profile->diagnostic_error = false;
			save_profiles();
			refresh_profile_ui(profile_id);
			tiktok_studio_.end_live(account, profile->live_id, profile->stream_id,
				[this, profile_id](TikTokStudioEndResult result) {
					Profile *current = find_profile(profile_id);
					if (!current)
						return;
					current->ending = false;
					TokenStore::save_tiktok_studio_account(current->account_id, result.account);
					if (!result.ended && !result.stale_session) {
						current->session_uncertain = true;
						current->diagnostic = text("Diagnostic.Failed").arg(result.error);
						current->diagnostic_error = true;
						save_profiles();
						refresh_profile_ui(profile_id);
						show_transient_error(result.error);
						return;
					}
					clear_live_session(*current);
					current->diagnostic = result.stale_session
						? text("Stream.StaleSessionCleared") : text("Diagnostic.SessionEnded");
					current->diagnostic_error = false;
					save_profiles();
					refresh_profile_ui(profile_id);
				});
			return;
		}
		if (ProviderRegistry::uses_local_credentials(profile->provider_id)) {
			clear_live_session(*profile);
			profile->diagnostic = text("Manual.SessionEnded");
			profile->diagnostic_error = false;
			save_profiles();
			refresh_profile_ui(profile_id);
			return;
		}
		const QString token = TokenStore::load(profile->id);
		profile->ending = true;
		profile->diagnostic = text("Diagnostic.EndingSession");
		profile->diagnostic_error = false;
		save_profiles();
		rebuild_profile_list();
		if (selected_profile() == profile)
			show_selected_profile();
		streamlabs_.end_live(token, profile->live_id, [this, profile_id](StreamlabsEndResult result) {
			Profile *current = find_profile(profile_id);
			if (!current) return;
			current->ending = false;
			if (!result.ended && !result.stale_session) {
				current->diagnostic = text("Diagnostic.Failed").arg(result.error);
				current->diagnostic_error = true;
				save_profiles();
				rebuild_profile_list();
				if (selected_profile() == current)
					show_selected_profile();
				show_transient_error(result.error);
				return;
			}
			clear_live_session(*current);
			current->diagnostic = result.stale_session ? text("Stream.StaleSessionCleared") : text("Diagnostic.SessionEnded");
			current->diagnostic_error = false;
			save_profiles(); rebuild_profile_list(); show_selected_profile();
			if (result.stale_session) {
				const bool german = obs_language().startsWith(QStringLiteral("de"), Qt::CaseInsensitive);
				QMessageBox::information(this, text("Plugin.Name"), translated_or("Stream.StaleSessionCleared",
					german ? QStringLiteral("Diese Live-Session ist auf TikTok nicht mehr aktiv. Der Status wurde zurückgesetzt.")
					       : QStringLiteral("This LIVE session is no longer active on TikTok. Its status has been reset.")));
			}
		});
	}

	void BridgeDock::run_tiktok_studio_heartbeats()
	{
		for (const Profile &profile : profiles_) {
			if (!ProviderRegistry::is_tiktok_studio(profile.provider_id) || !profile.live ||
				profile.preparing || profile.ending || profile.recovering || profile.session_uncertain ||
				profile.live_id.isEmpty() ||
				profile.stream_id.isEmpty() || tiktok_studio_heartbeat_in_flight_.contains(profile.id))
				continue;
			if (native_output_.contains(profile.id) && !native_output_.active(profile.id)) {
				if (Profile *current = find_profile(profile.id)) {
					QString reason = native_output_.last_error(profile.id);
					if (reason.isEmpty())
						reason = translated_or("Studio.Stream.NativeStopped",
							QStringLiteral("The native TikTok output stopped."));
					current->diagnostic = text("Diagnostic.Failed").arg(reason);
					current->diagnostic_error = true;
					save_profiles();
					refresh_profile_ui(profile.id);
					end_profile_live(profile.id);
				}
				continue;
			}
			const TikTokStudioAccountCredentials account =
				TokenStore::load_tiktok_studio_account(profile.account_id);
			if (!account.has_login())
				continue;
			const QString profile_id = profile.id;
			const int heartbeat_status = tiktok_studio_heartbeat_status_.value(profile_id, 1);
			const QByteArray previous_cookies = account.cookie_jar;
			tiktok_studio_heartbeat_in_flight_.insert(profile_id);
			tiktok_studio_.heartbeat(account, profile.live_id, profile.stream_id, heartbeat_status,
				[this, profile_id, previous_cookies, heartbeat_status](TikTokStudioHeartbeatResult result) {
					tiktok_studio_heartbeat_in_flight_.remove(profile_id);
					Profile *current = find_profile(profile_id);
					if (!current || !current->live || current->ending || current->recovering ||
						!ProviderRegistry::is_tiktok_studio(current->provider_id))
						return;
					if (result.account.cookie_jar != previous_cookies &&
						!TokenStore::save_tiktok_studio_account(current->account_id, result.account)) {
						const QString save_error = with_secure_storage_error(translated_or(
							"Studio.Account.SaveFailed",
							QStringLiteral("The refreshed TikTok session could not be saved securely.")));
						result.error = result.error.isEmpty()
							? save_error : QStringLiteral("%1 (%2)").arg(result.error, save_error);
					}
					if (!result.error.isEmpty()) {
						const int stale_count = result.error.contains(QStringLiteral("30003"))
							? tiktok_studio_stale_heartbeat_count_.value(profile_id) + 1 : 0;
						tiktok_studio_stale_heartbeat_count_.insert(profile_id, stale_count);
						if (stale_count >= 3) {
							clear_live_session(*current);
							current->diagnostic = translated_or("Studio.Heartbeat.RemoteEnded",
								QStringLiteral("TikTok reports that this LIVE has already ended."));
							current->diagnostic_error = false;
							save_profiles();
							refresh_profile_ui(profile_id);
							return;
						}
						tiktok_studio_heartbeat_failed_.insert(profile_id);
						current->diagnostic = translated_or("Studio.Heartbeat.Failed",
							QStringLiteral("TikTok LIVE heartbeat failed: %1")).arg(result.error);
						current->diagnostic_error = true;
						save_profiles();
						refresh_profile_ui(profile_id);
						return;
					}
					tiktok_studio_stale_heartbeat_count_.insert(profile_id, 0);
					if (result.room_is_living && heartbeat_status == 1)
						tiktok_studio_heartbeat_status_.insert(profile_id, 2);
					if (tiktok_studio_heartbeat_failed_.remove(profile_id)) {
						current->diagnostic = text("Diagnostic.SessionReady");
						current->diagnostic_error = false;
						save_profiles();
						refresh_profile_ui(profile_id);
					}
				});
		}
	}
