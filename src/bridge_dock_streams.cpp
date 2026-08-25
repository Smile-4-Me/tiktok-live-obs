// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "bridge_dock.hpp"
#include "frame_signing_settings.hpp"
#include "profile_live_session.hpp"
#include "aitum_outputs.hpp"
#include "localization.hpp"
#include "obs_button_style.hpp"
#include "plugin_paths.hpp"
#include "tiktok_studio_session.hpp"
#include "tiktok_studio_topics.hpp"
#include "token_store.hpp"

#include <QApplication>
#include <QAction>
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
#include <QToolBar>
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

QString live_start_action_text(bool starts_aitum_output)
{
	// Existing language packs remain usable until they receive the two more
	// specific labels. Their already translated "create LIVE" wording is a
	// clearer fallback than exposing an untranslated internal key.
	return starts_aitum_output
		? translated_or("Stream.Start", text("Stream.Create"))
		: translated_or("Stream.GenerateCredentials", text("Stream.Create"));
}

} // namespace

void BridgeDock::build_stream_step(const Profile &profile)
	{
		const bool studio_provider = ProviderRegistry::is_tiktok_studio(profile.provider_id);
		const bool supports_main_obs_output =
			ProviderRegistry::supports_main_obs_output(profile.provider_id);
		const FrameSigningRequirement signing_requirement =
			ProviderRegistry::frame_signing_requirement(profile.provider_id);
		const bool signing_required = signing_requirement == FrameSigningRequirement::Required;
		const bool signing_supported = signing_requirement != FrameSigningRequirement::NotSupported;
		auto *group = new QGroupBox(text("Stream.Title"), detail_container_);
		auto *form = new QFormLayout(group);
		form->setRowWrapPolicy(QFormLayout::WrapLongRows);
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		auto *output_row = new QWidget(group);
		auto *output_layout = new QHBoxLayout(output_row);
		output_layout->setContentsMargins(0, 0, 0, 0);
		auto *output = new QComboBox(output_row);
		output_layout->addWidget(output, 1);
		auto *refresh_outputs = obs_button_style::create_native_button(output_row,
			obs_button_style::obs_theme_icon(QStringLiteral("refresh.svg"), palette()),
			text("Stream.ReloadOutputs"), true);
		output_layout->addWidget(refresh_outputs, 0, Qt::AlignVCenter);
		const QString profile_id = profile.id;
		auto populate_outputs = [this, output, profile_id, supports_main_obs_output] {
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
				// Provider capabilities decide whether an explicit "generate only"
				// target belongs in this list. This keeps provider-specific UX out of
				// the shared Aitum target and LIVE-session code.
				if (supports_main_obs_output)
					output->addItem(text("Stream.MainOutput"), QString());
				for (const QString &name : names)
					output->addItem(name, name);
				const int saved_index = output->findData(saved_name);
				output->setCurrentIndex(saved_index >= 0 ? saved_index : 0);
				if (current && output->currentIndex() >= 0 &&
					saved_name != output->currentData().toString()) {
					if (Profile *editable = find_profile(profile_id)) {
						editable->output_name = output->currentData().toString();
						save_profiles();
					}
				}
				output->blockSignals(false);
				output->setToolTip(!aitum_available ? text("Stream.ManualUsageTooltip")
					: (names.isEmpty() ? diagnostic : QString()));
			};
			populate_outputs();
			connect(refresh_outputs, &QPushButton::clicked, this, populate_outputs);
		auto apply_output_mode = [this, profile_id, output, refresh_outputs] {
				const Profile *current = find_profile(profile_id);
				const bool editable = current && !current->live && !current->preparing && !current->recovering;
				output->setEnabled(editable);
				refresh_outputs->setEnabled(editable);
			};
			apply_output_mode();
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
		const FrameSigningCredentials signing_credentials = FrameSigningSettings::load(profile.id);
		auto *signing_enabled = new QCheckBox(translated_or("Signing.Enable",
			QStringLiteral("Sign video frames inside OBS (RapidAPI)")), group);
		signing_enabled->setChecked(signing_required || profile.frame_signing_enabled);
		signing_enabled->setEnabled(signing_requirement == FrameSigningRequirement::Optional &&
			!profile.live && !profile.preparing);
		signing_enabled->setVisible(signing_supported && !signing_required);
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
		signing_group->setVisible(signing_requirement == FrameSigningRequirement::Optional &&
			profile.frame_signing_enabled);
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
			if (!FrameSigningSettings::save(profile_id, credentials))
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
				studio_game->setEnabled(false);
				studio_game->lineEdit()->setPlaceholderText(translated_or(
					"Studio.Stream.GameLoading", QStringLiteral("Loading games …")));
				QPointer<QComboBox> game_guard(studio_game);
				ProviderLifecycle *provider = provider_sessions_.find(profile.provider_id);
				if (!provider)
					return;
				provider->fetch_game_tags({profile.id, profile.account_id},
					[this, profile_id, game_guard](QVector<ProviderCatalogEntry> games, QString error) {
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
						for (const ProviderCatalogEntry &game : games) {
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
					if (category->text().trimmed().isEmpty()) {
						category_choices->hide();
						return;
					}
					Profile *active_profile = find_profile(profile_id);
					ProviderLifecycle *provider = active_profile
						? provider_sessions_.find(active_profile->provider_id) : nullptr;
					if (!provider) {
						category_choices->hide();
						return;
					}
					provider->search_categories({active_profile->id, active_profile->account_id},
						category->text().trimmed(),
						[this, profile_id, category_choices](QVector<ProviderCatalogEntry> results, QString) {
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

		// Every provider reaches the same user-facing LIVE actions here. The
		// provider determines how a session is created, never how the action is
		// named in the dock.
		auto can_start_live = [this, profile_id, output, supports_main_obs_output] {
			const Profile *current = find_profile(profile_id);
			if (!current || current->preparing || current->live || current->recovering ||
				current->session_uncertain)
				return false;
			const bool selected_target = output->currentIndex() >= 0 &&
				(supports_main_obs_output || !output->currentData().toString().trimmed().isEmpty());
			return selected_target;
		};
		const bool manual_return_to_credentials = ProviderRegistry::is_manual(profile.provider_id) &&
			!profile.live && !profile.preparing && !profile.recovering && !profile.session_uncertain;
		auto *go_live = new QPushButton(live_start_action_text(
			!output->currentData().toString().trimmed().isEmpty()), detail_container_);
		auto *end_live = new QPushButton(manual_return_to_credentials
			? text("Manual.ReturnToCredentials") : text("Stream.End"), detail_container_);
		go_live->setEnabled(can_start_live());
		end_live->setEnabled((manual_return_to_credentials || profile.live || profile.preparing ||
			profile.session_uncertain) && !profile.ending && !profile.recovering);
		connect(go_live, &QPushButton::clicked, this, [this] { start_selected_live(); });
		connect(end_live, &QPushButton::clicked, this, [this] { end_selected_live(); });
		connect(output, &QComboBox::currentIndexChanged, this,
			[this, profile_id, output, go_live, can_start_live] {
			if (Profile *current = find_profile(profile_id)) {
				// The empty data value represents the built-in/main OBS output. Real
				// Aitum output names are stored as item data, so the first real output
				// works for every provider too.
				current->output_name = output->currentData().toString();
				save_profiles();
				go_live->setText(live_start_action_text(!current->output_name.trimmed().isEmpty()));
				go_live->setEnabled(can_start_live());
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
		detail_layout_->addWidget(info_card(text("Stream.Description"), detail_container_));
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
		if (profile.live) {
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
		const FrameSigningCredentials credentials = FrameSigningSettings::load(profile_id);
		const QString configured_room_id = credentials.room_id.trimmed();
		const QString room_id = configured_room_id.isEmpty() ? session_room_id.trimmed() : configured_room_id;
		HostedSigningServiceConfig api;
		api.base_url = QUrl(credentials.api_url);
		api.api_key = credentials.rapidapi_key;
		SignedSeiConfig signing;
		signing.aid = credentials.aid;
		signing.uid = credentials.uid;
		signing.device_id = credentials.device_id;
		signing.room_id = room_id;
		output_signing_.prepare_and_attach(output_name, std::move(api), std::move(signing),
			std::move(completion));
	}

void BridgeDock::start_selected_live()
	{
		Profile *profile = selected_profile();
		if (!profile)
			return;
		// A named Aitum target means the dock action owns the whole start flow:
		// create credentials, hand them to Aitum, start that output, then verify
		// its encoder. The empty target intentionally only generates credentials.
		start_profile_live(profile->id, !profile->output_name.trimmed().isEmpty());
	}

void BridgeDock::return_selected_manual_profile_to_credentials()
{
	Profile *profile = selected_profile();
	if (!profile || !ProviderRegistry::uses_local_credentials(profile->provider_id) ||
		profile->live || profile->preparing || profile->ending || profile->recovering ||
		profile->session_uncertain)
		return;

	// Manual credentials are deliberately single-use. Returning to input clears
	// both the secure credential pair and the selected Aitum output, so the
	// profile cannot be displayed as linked to an expired one-time key.
	clear_live_session(*profile);
	profile->diagnostic.clear();
	profile->diagnostic_error = false;
	save_profiles();
	rebuild_profile_list();
	show_selected_profile();
}

bool BridgeDock::return_to_account_step_on_live_access_denied(Profile &profile,
	const ProviderLifecycle &provider, const QString &error)
{
	if (!provider.is_live_access_denied_error(error))
		return false;
	ProfileLiveSession::clear_output_assignment(profile);
	profile.can_go_live = false;
	profile.application_status = QStringLiteral("live_access_denied");
	return true;
}

QString BridgeDock::provider_error_message(const ProviderLifecycle &provider,
	const QString &error) const
{
	if (provider.is_live_access_denied_error(error))
		return text("Error.LiveAccessDenied");
	return error;
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
		ProviderLifecycle *provider = provider_sessions_.find(profile->provider_id);
		if (!provider) {
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
		const QString provider_id = profile->provider_id;
		const ProviderAccountReference provider_account{profile->id, profile->account_id};
		const LiveRequest request{.title = profile->stream_title, .category_id = profile->category_id,
			.mature = profile->mature};
		save_profiles();
		rebuild_profile_list();
		if (selected_profile() == profile)
			show_selected_profile();
		provider->create_live(provider_account, request,
			[this, active_profile_id, output_name, provider_id, provider_account, start_aitum_output]
			(PreparedLive live, QString error) {
				Profile *current = find_profile(active_profile_id);
				if (!current) {
					if (start_aitum_output)
						outputs_preparing_.remove(output_name);
					return;
				}
				if (!error.isEmpty()) {
					QString visible_error = error;
					if (ProviderLifecycle *error_provider = provider_sessions_.find(current->provider_id)) {
						return_to_account_step_on_live_access_denied(*current, *error_provider, error);
						visible_error = provider_error_message(*error_provider, error);
					}
					current->preparing = false;
					current->diagnostic = text("Diagnostic.Failed").arg(visible_error);
					current->diagnostic_error = true;
					save_profiles();
					rebuild_profile_list();
					if (selected_profile() == current)
						show_selected_profile();
					if (start_aitum_output)
						outputs_preparing_.remove(output_name);
					show_transient_error(visible_error);
					return;
				}
				if (!start_aitum_output && (output_name.isEmpty() || !aitum_stream_suite_available())) {
					const bool aitum_available = aitum_stream_suite_available();
					ProfileLiveSession::reserve(*current, live);
					current->preparing = current->frame_signing_enabled;
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
					prepare_output_signing(active_profile_id, QString{}, live.session_id,
						[this, active_profile_id, provider_id, provider_account, live, aitum_available]
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
							ProviderLifecycle *session_provider = provider_sessions_.find(provider_id);
							if (!session_provider)
								return;
							session_provider->end_live(provider_account, live,
								[this, active_profile_id, live, signing_error]
								(ProviderEndResult end_result) {
									Profile *cleanup = find_profile(active_profile_id);
									if (!cleanup)
										return;
									cleanup->ending = false;
									if (end_result.ended || end_result.stale_session) {
										clear_live_session(*cleanup);
									} else {
										ProfileLiveSession::mark_uncertain(*cleanup, live);
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
					[this, active_profile_id, output_name, provider_id, provider_account, live, start_aitum_output]
					(BridgeResult result) {
					Profile *updated = find_profile(active_profile_id);
					if (!updated) {
						if (start_aitum_output)
							outputs_preparing_.remove(output_name);
						return;
					}
					updated->preparing = false;
					if (result == BridgeResult::Success) {
						ProfileLiveSession::reserve(*updated, live);
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
						prepare_output_signing(active_profile_id, output_name, live.session_id,
							[this, active_profile_id, output_name, provider_id, provider_account, live, start_aitum_output]
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
									ProviderLifecycle *session_provider = provider_sessions_.find(provider_id);
									if (!session_provider)
										return;
									session_provider->end_live(provider_account, live,
										[this, active_profile_id, output_name, live, signing_error]
										(ProviderEndResult end_result) {
											Profile *cleanup = find_profile(active_profile_id);
											outputs_preparing_.remove(output_name);
											if (!cleanup)
												return;
											cleanup->ending = false;
											if (end_result.ended || end_result.stale_session) {
												clear_live_session(*cleanup);
											} else {
												ProfileLiveSession::mark_uncertain(*cleanup, live);
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
					ProviderLifecycle *session_provider = provider_sessions_.find(provider_id);
					if (!session_provider)
						return;
					session_provider->end_live(provider_account, live,
						[this, active_profile_id, output_name, live, start_aitum_output](ProviderEndResult end_result) {
						Profile *cleanup = find_profile(active_profile_id);
						if (!cleanup) {
							if (start_aitum_output)
								outputs_preparing_.remove(output_name);
							return;
						}
						cleanup->preparing = false;
						if (!end_result.ended && !end_result.stale_session) {
							ProfileLiveSession::reserve(*cleanup, live);
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
		if (!profile)
			return;
		if (ProviderRegistry::uses_local_credentials(profile->provider_id) &&
			!profile->live && !profile->preparing && !profile->recovering &&
			!profile->session_uncertain) {
			return_selected_manual_profile_to_credentials();
			return;
		}
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

		ProviderLifecycle *provider = provider_sessions_.find(profile->provider_id);
		if (!provider)
			return;
		const ProviderAccountReference provider_account{profile->id, profile->account_id};
		const PreparedLive active_live{.session_id = profile->live_id, .stream_id = profile->stream_id,
			.room_id = profile->live_id, .server = profile->stream_server, .key = profile->stream_key};
		profile->ending = true;
	profile->diagnostic = text("Diagnostic.EndingSession");
	profile->diagnostic_error = false;
	save_profiles();
	refresh_profile_ui(profile_id);

	// A manual URL/key pair cannot authorize a separate TikTok end request.
	// Stopping its selected Aitum output is therefore the actual disconnect
	// signal that closes the RTMP broadcast at TikTok. We use the same published
	// Aitum vendor action for every provider so the dock's End LIVE button never
	// leaves its own encoder running after the remote session is closed.
	const QString output_name = profile->output_name.trimmed();
	bool output_active = false;
	QString output_diagnostic;
	const bool output_state_known = !output_name.isEmpty() && aitum_stream_suite_available() &&
		aitum_output_is_active(output_name, &output_active, &output_diagnostic);
	if (output_state_known && output_active) {
		if (!aitum_stop_output(output_name, &output_diagnostic)) {
			profile->ending = false;
			profile->diagnostic = text("Diagnostic.Failed").arg(translated_or(
				"Error.AitumStopFailed", QStringLiteral("Aitum could not stop the output \"%1\": %2"))
				.arg(output_name, output_diagnostic));
			profile->diagnostic_error = true;
			save_profiles();
			refresh_profile_ui(profile_id);
			show_transient_error(translated_or("Error.AitumStopFailed", QStringLiteral(
				"Aitum could not stop the output \"%1\": %2")).arg(output_name, output_diagnostic));
			return;
		}
	}
	provider->end_live(provider_account, active_live, [this, profile_id](ProviderEndResult result) {
			Profile *current = find_profile(profile_id);
			if (!current)
				return;
			current->ending = false;
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
			current->diagnostic = result.stale_session ? text("Stream.StaleSessionCleared") : text("Diagnostic.SessionEnded");
			current->diagnostic_error = false;
			save_profiles();
			refresh_profile_ui(profile_id);
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
			ProviderLifecycle *provider = provider_sessions_.find(profile.provider_id);
			if (!provider)
				continue;
			const QString profile_id = profile.id;
			const int heartbeat_status = tiktok_studio_heartbeat_status_.value(profile_id, 1);
			const ProviderAccountReference provider_account{profile.id, profile.account_id};
			const PreparedLive active_live{.session_id = profile.live_id, .stream_id = profile.stream_id,
				.room_id = profile.live_id, .server = profile.stream_server, .key = profile.stream_key};
			tiktok_studio_heartbeat_in_flight_.insert(profile_id);
			provider->heartbeat(provider_account, active_live, heartbeat_status,
				[this, profile_id, heartbeat_status](ProviderHeartbeatResult result) {
					tiktok_studio_heartbeat_in_flight_.remove(profile_id);
					Profile *current = find_profile(profile_id);
					if (!current || !current->live || current->ending || current->recovering ||
						!ProviderRegistry::is_tiktok_studio(current->provider_id))
						return;
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
					if (result.session_is_live && heartbeat_status == 1)
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
