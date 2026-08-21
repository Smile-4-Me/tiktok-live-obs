// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "bridge_dock.hpp"
#include "aitum_outputs.hpp"
#include "localization.hpp"
#include "plugin_paths.hpp"
#include "token_store.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

void BridgeDock::build_stream_step(const Profile &profile)
	{
		auto *group = new QGroupBox(text("Stream.Title"), detail_container_);
		auto *form = new QFormLayout(group);
		auto *output_row = new QWidget(group);
			auto *output_layout = new QHBoxLayout(output_row);
			output_layout->setContentsMargins(0, 0, 0, 0);
			auto *output = new QComboBox(output_row);
			output_layout->addWidget(output, 1);
			auto *refresh_outputs = new QToolButton(output_row);
			refresh_outputs->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
			refresh_outputs->setToolTip(text("Stream.ReloadOutputs"));
			refresh_outputs->setFixedSize(30, 30);
			output_layout->addWidget(refresh_outputs);
			auto populate_outputs = [this, output, profile] {
				const QString saved_name = selected_profile() ? selected_profile()->output_name : profile.output_name;
				QString diagnostic;
				const QStringList names = aitum_output_names(&diagnostic);
				const bool aitum_available = aitum_stream_suite_available();
				output->blockSignals(true);
				output->clear();
				output->addItem(aitum_available ? text("Stream.OutputPlaceholder") : text("Stream.ManualUsage"), QString());
				output->addItems(names);
				const int saved_index = output->findText(saved_name);
				output->setCurrentIndex(saved_index >= 0 ? saved_index : 0);
				output->blockSignals(false);
				output->setToolTip(!aitum_available ? text("Stream.ManualUsageTooltip")
					: (names.isEmpty() ? diagnostic : QString()));
			};
			populate_outputs();
			connect(refresh_outputs, &QPushButton::clicked, this, populate_outputs);
			form->addRow(text("Stream.Output"), output_row);
		auto *title = new QLineEdit(profile.stream_title, group);
		form->addRow(text("Stream.StreamTitle"), title);
		auto *category = new QLineEdit(profile.category, group);
		form->addRow(text("Stream.GameCategory"), category);
		auto *category_choices = new QListWidget(group);
		category_choices->setMaximumHeight(100);
		category_choices->hide();
		form->addRow(category_choices);
		auto *mature = new QCheckBox(text("Stream.Mature"), group);
		mature->setChecked(profile.mature);
		form->addRow(mature);
		connect(title, &QLineEdit::textEdited, this, [this, title] {
			if (Profile *current = selected_profile()) {
				current->stream_title = title->text();
				save_profiles();
			}
		});
		const QString profile_id = profile.id;
		connect(category, &QLineEdit::textEdited, this, [this, profile_id, category, category_choices] {
			if (Profile *current = selected_profile()) {
				current->category = category->text();
				current->category_id.clear();
				save_profiles();
			}
			const QString token = TokenStore::load(profile_id);
			if (token.isEmpty() || category->text().trimmed().isEmpty()) { category_choices->hide(); return; }
			streamlabs_.search_categories(token, category->text().trimmed(), [this, profile_id, category_choices](QVector<StreamlabsCategory> results, QString) {
				if (!selected_profile() || selected_profile()->id != profile_id) return;
				category_choices->clear();
				for (const auto &result : results) {
					auto *item = new QListWidgetItem(result.name, category_choices);
					item->setData(Qt::UserRole, result.id);
				}
				category_choices->setVisible(category_choices->count() > 0);
			});
		});
		connect(category_choices, &QListWidget::itemClicked, this, [this, category, category_choices] (QListWidgetItem *item) {
			if (Profile *current = selected_profile()) {
				current->category = item->text();
				current->category_id = item->data(Qt::UserRole).toString();
				category->setText(current->category);
				save_profiles();
			}
			category_choices->hide();
		});
		connect(mature, &QCheckBox::toggled, this, [this](bool checked) {
			if (Profile *current = selected_profile()) { current->mature = checked; save_profiles(); }
		});
		add_live_status(form, profile, group);
		detail_layout_->addWidget(group);

		const bool manual_provider = ProviderRegistry::is_manual(profile.provider_id);
		auto *go_live = new QPushButton(
			text(manual_provider ? "Manual.Start" : "Stream.Generate"), detail_container_);
		auto *end_live = new QPushButton(
			text(manual_provider ? "Manual.End" : "Stream.End"), detail_container_);
		go_live->setEnabled(!profile.preparing && !profile.live && !profile.recovering);
		end_live->setEnabled(profile.live && !profile.ending && !profile.recovering);
		connect(go_live, &QPushButton::clicked, this, [this] { start_selected_live(); });
		connect(end_live, &QPushButton::clicked, this, [this] { end_selected_live(); });
		connect(output, &QComboBox::currentIndexChanged, this, [this, output, go_live] {
			if (Profile *current = selected_profile()) {
				current->output_name = output->currentIndex() > 0 ? output->currentText() : QString{};
				save_profiles();
				go_live->setEnabled(!current->preparing && !current->live && !current->recovering);
				QTimer::singleShot(0, this, [this] { rebuild_profile_list(); });
			}
		});
		detail_layout_->addWidget(go_live);
		detail_layout_->addWidget(end_live);
		detail_layout_->addWidget(info_card(text("Stream.Description"), detail_container_));
	}

void BridgeDock::add_live_status(QFormLayout *form, const Profile &profile, QWidget *parent)
	{
		auto *container = new QWidget(parent);
		auto *layout = new QVBoxLayout(container);
		layout->setContentsMargins(0, 5, 0, 3);
		layout->setSpacing(5);
		QString status_text = text("Stream.StatusNotLive");
		if (profile.recovering)
			status_text = text("Stream.StatusRecovering");
		else if (profile.session_uncertain)
			status_text = text("Stream.StatusSessionUncertain");
		else if (profile.live)
			status_text = text("Stream.StatusLive");
		auto *status = new QLabel(status_text, container);
		status->setStyleSheet(QStringLiteral("QLabel { color: %1; font-weight: 600; }")
			.arg((profile.recovering || profile.session_uncertain) ? QStringLiteral("#e7af4b")
				: (profile.live ? QStringLiteral("#62c370") : QStringLiteral("#e05d5d"))));
		layout->addWidget(status);
		if (!profile.diagnostic.isEmpty()) {
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
		QMessageBox notice(QMessageBox::Information, text("Plugin.Name"), text("Manual.AitumMissing"),
			QMessageBox::Ok, this);
		QCheckBox dont_show(text("Manual.AitumMissingDontShow"), &notice);
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

bool BridgeDock::output_in_use_by_another_profile(const Profile &profile) const
	{
		if (profile.output_name.isEmpty())
			return false;
		for (const Profile &candidate : profiles_)
			if (candidate.id != profile.id && candidate.output_name == profile.output_name && (candidate.live || candidate.preparing))
				return true;
		return false;
	}

void BridgeDock::end_unstarted_aitum_session(const QString &profile_id, const QString &output_name)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile || !profile->live ||
			(!ProviderRegistry::is_manual(profile->provider_id) && profile->live_id.isEmpty())) {
			outputs_preparing_.remove(output_name);
			return;
		}
		if (ProviderRegistry::is_manual(profile->provider_id)) {
			clear_live_session(*profile);
			profile->diagnostic = text("Diagnostic.OutputNotActiveEnded");
			profile->diagnostic_error = true;
			outputs_preparing_.remove(output_name);
			save_profiles();
			refresh_profile_ui(profile_id);
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
		if (!profile || !profile->live || profile->ending) {
			outputs_preparing_.remove(output_name);
			return;
		}

		bool active = false;
		QString diagnostic;
		const bool status_available = aitum_output_is_active(output_name, &active, &diagnostic);
		if (status_available && active) {
			outputs_preparing_.remove(output_name);
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

void BridgeDock::start_selected_live()
	{
		Profile *profile = selected_profile();
		if (!profile)
			return;
		start_profile_live(profile->id, false);
	}

void BridgeDock::start_profile_live(const QString &profile_id, bool start_aitum_output)
	{
		Profile *profile = find_profile(profile_id);
		if (!profile || (start_aitum_output && profile->output_name.isEmpty())) {
			if (start_aitum_output && profile)
				outputs_preparing_.remove(profile->output_name);
			return;
		}
		if (profile->recovering)
			return;
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
		if (ProviderRegistry::is_manual(profile->provider_id)) {
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
			profile->diagnostic = start_aitum_output ? text("Diagnostic.UpdatingAitum") : text("Manual.SessionReady");
			profile->diagnostic_error = false;
			const QString manual_profile_id = profile->id;
			const QString manual_output_name = profile->output_name;
			save_profiles();
			rebuild_profile_list();
			if (selected_profile() == profile)
				show_selected_profile();
			if (!start_aitum_output &&
				(manual_output_name.isEmpty() || !aitum_stream_suite_available())) {
				profile->preparing = false;
				save_profiles();
				refresh_profile_ui(manual_profile_id);
				if (!aitum_stream_suite_available())
					show_aitum_missing_notice();
				return;
			}
			bridge_.update_async({credentials.server, credentials.key, manual_output_name},
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
					current->diagnostic = text("Diagnostic.StartingOutput");
					save_profiles();
					refresh_profile_ui(manual_profile_id);
					if (!start_aitum_output)
						return;
					QTimer::singleShot(250, this, [this, manual_profile_id, manual_output_name] {
						QString diagnostic;
						const bool started = aitum_start_output(manual_output_name, &diagnostic);
						if (!started) {
							outputs_preparing_.remove(manual_output_name);
							if (Profile *failed = find_profile(manual_profile_id)) {
								clear_live_session(*failed);
								failed->diagnostic = text("OneClick.StartFailed").arg(diagnostic);
								failed->diagnostic_error = true;
								save_profiles();
								refresh_profile_ui(manual_profile_id);
							}
							return;
						}
						verify_aitum_output_started(manual_profile_id, manual_output_name, 0);
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
					current->preparing = false;
					current->live = true;
					current->live_id = live.id;
					current->stream_server = live.server;
					current->stream_key = live.key;
					current->diagnostic = text("Diagnostic.SessionReady");
					current->diagnostic_error = false;
					TokenStore::save_live_credentials(active_profile_id, {live.server, live.key});
					save_profiles();
					rebuild_profile_list();
					if (selected_profile() == current)
						show_selected_profile();
					if (!aitum_stream_suite_available())
						show_aitum_missing_notice();
					return;
				}
				current->diagnostic = text("Diagnostic.UpdatingAitum");
				current->diagnostic_error = false;
				rebuild_profile_list();
				if (selected_profile() == current)
					show_selected_profile();
				bridge_.update_async({live.server, live.key, output_name}, [this, active_profile_id, output_name, token, live, start_aitum_output](BridgeResult result) {
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
						updated->diagnostic = start_aitum_output ? text("Diagnostic.StartingOutput") : text("Diagnostic.SessionReady");
						updated->diagnostic_error = false;
						TokenStore::save_live_credentials(active_profile_id, {live.server, live.key});
						save_profiles();
						rebuild_profile_list();
						if (selected_profile() == updated)
							show_selected_profile();
						if (start_aitum_output) {
							QTimer::singleShot(250, this, [this, active_profile_id, output_name] {
								QString diagnostic;
								const bool started = aitum_start_output(output_name, &diagnostic);
								if (Profile *current = find_profile(active_profile_id)) {
									current->diagnostic = started ? text("Diagnostic.VerifyingOutput")
										: text("Diagnostic.Failed").arg(text("OneClick.StartFailed").arg(diagnostic));
									current->diagnostic_error = !started;
									save_profiles();
									refresh_profile_ui(active_profile_id);
								}
								if (!started) {
									outputs_preparing_.remove(output_name);
									show_transient_error(text("OneClick.StartFailed").arg(diagnostic));
									return;
								}
								verify_aitum_output_started(active_profile_id, output_name, 0);
							});
						}
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
		if (!profile || !profile->live || profile->ending || profile->recovering)
			return;
		if (ProviderRegistry::is_manual(profile->provider_id)) {
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
