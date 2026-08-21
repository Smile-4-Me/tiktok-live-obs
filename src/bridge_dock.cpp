// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "bridge_dock.hpp"
#include "aitum_outputs.hpp"
#include "localization.hpp"
#include "plugin_paths.hpp"
#include "token_store.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTimer>
#include <QToolBar>

namespace {

constexpr char aitum_output_button_object_name[] = "canvasOutput";

} // namespace

BridgeDock::BridgeDock(QWidget *obs_main_window)
		: QWidget(obs_main_window), bridge_(obs_main_window, this), streamlabs_(this)
	{
		setObjectName(QStringLiteral("TikTokLiveObsDockContents"));
		TokenStore::set_storage_scope(installation_storage_scope());
		build_ui();
		load_profiles();
		if (profiles_.empty())
			profiles_.push_back(new_profile(text("Profile.DefaultName")));
		selected_profile_ = 0;
		reconcile_previous_sessions();
		rebuild_profile_list();
		show_selected_profile();
		refresh_selected_account();
		qApp->installEventFilter(this);
	}

bool BridgeDock::eventFilter(QObject *watched, QEvent *event)
	{
		if (event->type() != QEvent::MouseButtonRelease)
			return QWidget::eventFilter(watched, event);
		auto *button = qobject_cast<QAbstractButton *>(watched);
		const auto *mouse_event = static_cast<QMouseEvent *>(event);
		if (!button || mouse_event->button() != Qt::LeftButton)
			return QWidget::eventFilter(watched, event);

		if (const auto toolbar_action = aitum_toolbar_action_for_button(button)) {
			if (*toolbar_action == AitumToolbarAction::StartAll) {
				show_start_all_menu(button);
				return true;
			}
			if (*toolbar_action == AitumToolbarAction::StopAll) {
				QTimer::singleShot(0, this, [this] { end_all_live_profiles(); });
				return QWidget::eventFilter(watched, event);
			}
		}

		if (button->objectName() != QString::fromUtf8(aitum_output_button_object_name))
			return QWidget::eventFilter(watched, event);

		const QString output_name = output_name_for_aitum_button(button);
		if (output_name.isEmpty())
			return QWidget::eventFilter(watched, event);
		if (button->isChecked()) {
			QTimer::singleShot(0, this, [this, output_name] { end_live_for_output(output_name); });
			return QWidget::eventFilter(watched, event);
		}
		if (live_profile_for_output(output_name))
			return QWidget::eventFilter(watched, event);
		const std::vector<Profile *> linked_profiles = profiles_for_output(output_name, false);
		if (linked_profiles.empty())
			return QWidget::eventFilter(watched, event);
		const std::vector<Profile *> ready_profiles = profiles_for_output(output_name, true);
		if (ready_profiles.empty()) {
			for (const Profile *linked : linked_profiles) {
				if (const Profile *active = active_profile_for_tiktok_account(*linked)) {
					show_transient_error(tiktok_account_conflict_message(*linked, *active));
					return true;
				}
			}
			show_transient_error(text("OneClick.NoReadyProfile"));
			return true;
		}
		if (outputs_preparing_.contains(output_name)) {
			show_transient_error(text("OneClick.Preparing"));
			return true;
		}

		Profile *profile = ready_profiles.size() == 1
			? ready_profiles.front()
			: choose_profile_for_output(output_name, ready_profiles);
		if (!profile)
			return true;
		if (const Profile *active = active_profile_for_tiktok_account(*profile)) {
			show_transient_error(tiktok_account_conflict_message(*profile, *active));
			return true;
		}
		outputs_preparing_.insert(output_name);
		start_profile_live(profile->id, true);
		return true;
	}

std::optional<BridgeDock::AitumToolbarAction> BridgeDock::aitum_toolbar_action_for_button(const QAbstractButton *button) const
	{
		const QToolBar *toolbar = nullptr;
		for (const QWidget *candidate = button; candidate; candidate = candidate->parentWidget()) {
			if ((toolbar = qobject_cast<const QToolBar *>(candidate)))
				break;
		}
		if (!toolbar || toolbar->objectName() != QStringLiteral("outputsToolbar"))
			return std::nullopt;
		const QString css_class = button->property("class").toString();
		if (css_class == QStringLiteral("icon-media-play"))
			return AitumToolbarAction::StartAll;
		if (css_class == QStringLiteral("icon-media-stop"))
			return AitumToolbarAction::StopAll;
		return std::nullopt;
	}

void BridgeDock::show_start_all_menu(QWidget *anchor)
	{
		QMenu menu(this);
		menu.addAction(text("OneClick.StartAllOutputs"), this, [this] { start_all_outputs(false); });
		menu.addAction(text("OneClick.StartAllStreams"), this, [this] { start_all_outputs(true); });
		menu.addAction(text("OneClick.StartAllRecordings"), this, [this] { start_all_recordings(); });
		menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
	}

void BridgeDock::start_all_outputs(bool stream_only)
	{
		QString diagnostic;
		QHash<QString, QString> types;
		const QStringList all_outputs = aitum_output_names(&diagnostic, &types);
		QStringList outputs;
		for (const QString &output_name : all_outputs) {
			const QString type = types.value(output_name);
			if (!stream_only || type == QStringLiteral("stream") || type == QStringLiteral("ffmpeg"))
				outputs.push_back(output_name);
		}
		if (outputs.isEmpty()) {
			show_transient_error(diagnostic.isEmpty() ? text("OneClick.NoOutputs") : diagnostic);
			return;
		}

		for (const QString &output_name : outputs) {
			const auto linked = profiles_for_output(output_name, false);
			if (linked.empty() || live_profile_for_output(output_name)) {
				QString ignored;
				aitum_start_output(output_name, &ignored);
			}
		}
		start_all_linked_outputs(outputs);
	}

void BridgeDock::start_all_recordings()
	{
		QString diagnostic;
		QHash<QString, QString> types;
		const QStringList all_outputs = aitum_output_names(&diagnostic, &types);
		for (const QString &output_name : all_outputs) {
			const QString type = types.value(output_name);
			if (type != QStringLiteral("record") && type != QStringLiteral("backtrack"))
				continue;
			QString ignored;
			aitum_start_output(output_name, &ignored);
		}
	}

void BridgeDock::start_all_linked_outputs(const QStringList &outputs)
	{
		struct OutputChoice {
			QString output_name;
			std::vector<Profile *> profiles;
		};
		std::vector<OutputChoice> choices;
		for (const QString &output_name : outputs) {
			if (live_profile_for_output(output_name) || profiles_for_output(output_name, false).empty())
				continue;
			const auto ready = profiles_for_output(output_name, true);
			if (ready.empty() || outputs_preparing_.contains(output_name))
				continue;
			choices.push_back({output_name, ready});
		}
		if (choices.empty())
			return;

		if (choices.size() == 1) {
			if (Profile *profile = choose_profile_for_output(choices.front().output_name, choices.front().profiles)) {
				outputs_preparing_.insert(choices.front().output_name);
				start_profile_live(profile->id, true);
			}
			return;
		}

		QDialog dialog(this);
		dialog.setWindowTitle(text("OneClick.BatchTitle"));
		dialog.setMinimumWidth(440);
		auto *layout = new QVBoxLayout(&dialog);
		layout->setContentsMargins(18, 16, 18, 14);
		layout->setSpacing(10);
		auto *description = new QLabel(text("OneClick.BatchDescription"), &dialog);
		description->setWordWrap(true);
		layout->addWidget(description);
		auto *form = new QFormLayout;
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
		layout->addLayout(form);

		std::vector<QComboBox *> selectors;
		for (const OutputChoice &choice : choices) {
			auto *selector = new QComboBox(&dialog);
			selector->addItem(text("OneClick.SkipOutput"), QString());
			for (const Profile *profile : choice.profiles) {
				QString label = profile->display_name;
				if (!profile->tiktok_username.isEmpty())
					label += QStringLiteral(" (@%1)").arg(profile->tiktok_username);
				selector->addItem(label, profile->id);
			}
			selector->setCurrentIndex(choice.profiles.size() == 1 ? 1 : 0);
			form->addRow(choice.output_name, selector);
			selectors.push_back(selector);
		}

		// A TikTok account may only appear in one selected output at a time,
		// even when it has been saved under a different local profile name.
		auto update_account_availability = [this, &choices, &selectors] {
			QSet<QString> claimed_accounts;
			for (const Profile &profile : profiles_) {
				if ((profile.live || profile.preparing) && !profile.tiktok_username.isEmpty())
					claimed_accounts.insert(tiktok_account_id(profile));
			}
			for (size_t row = 0; row < choices.size(); ++row) {
				QComboBox *selector = selectors.at(row);
				QString selected_id = selector->currentData().toString();
				const Profile *selected_profile = find_profile(selected_id);
				QString selected_account = selected_profile ? tiktok_account_id(*selected_profile) : QString();
				if (!selected_account.isEmpty() && claimed_accounts.contains(selected_account)) {
					const QSignalBlocker blocked(selector);
					selector->setCurrentIndex(0);
					selected_id.clear();
					selected_account.clear();
				}
				auto *model = qobject_cast<QStandardItemModel *>(selectors.at(row)->model());
				if (!model)
					continue;
				for (int item = 1; item < selectors.at(row)->count(); ++item) {
					const QString profile_id = selectors.at(row)->itemData(item).toString();
					const Profile *profile = find_profile(profile_id);
					if (!profile)
						continue;
					const QString account = tiktok_account_id(*profile);
					const bool selected_here = profile_id == selected_id;
					const bool enabled = account.isEmpty() || selected_here || !claimed_accounts.contains(account);
					if (QStandardItem *entry = model->item(item))
						entry->setEnabled(enabled);
				}
				if (!selected_account.isEmpty())
					claimed_accounts.insert(selected_account);
			}
		};
		for (QComboBox *selector : selectors)
			connect(selector, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, update_account_availability);
		update_account_availability();

		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dialog);
		buttons->button(QDialogButtonBox::Ok)->setText(text("OneClick.BatchStart"));
		buttons->button(QDialogButtonBox::Cancel)->setText(text("OneClick.BatchCancel"));
		connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
		layout->addWidget(buttons);
		if (dialog.exec() != QDialog::Accepted)
			return;

		for (size_t index = 0; index < choices.size(); ++index) {
			const QString profile_id = selectors.at(index)->currentData().toString();
			if (profile_id.isEmpty())
				continue;
			outputs_preparing_.insert(choices.at(index).output_name);
			start_profile_live(profile_id, true);
		}
	}

void BridgeDock::end_all_live_profiles()
	{
		std::vector<QString> active_ids;
		for (const Profile &profile : profiles_)
			if (profile.live && !profile.ending)
				active_ids.push_back(profile.id);
		for (const QString &id : active_ids)
			end_profile_live(id);
	}

QString BridgeDock::output_name_for_aitum_button(const QAbstractButton *button) const
	{
		for (const QWidget *candidate = button->parentWidget(); candidate; candidate = candidate->parentWidget()) {
			const QString name = candidate->objectName();
			if (!name.isEmpty() && name != QString::fromUtf8(aitum_output_button_object_name))
				return name;
		}
		return {};
	}

std::vector<Profile *> BridgeDock::profiles_for_output(const QString &output_name, bool ready_only)
	{
		std::vector<Profile *> matches;
		for (Profile &profile : profiles_) {
			if (profile.output_name != output_name)
				continue;
			if (ready_only && (!profile.can_go_live || profile.live || profile.preparing ||
				tiktok_account_in_use_by_another_profile(profile)))
				continue;
			matches.push_back(&profile);
		}
		return matches;
	}

Profile *BridgeDock::live_profile_for_output(const QString &output_name)
	{
		for (Profile &profile : profiles_)
			if (profile.output_name == output_name && profile.live)
				return &profile;
		return nullptr;
	}

QString BridgeDock::tiktok_account_id(const Profile &profile) const
	{
		return profile.tiktok_username.trimmed().toCaseFolded();
	}

const Profile *BridgeDock::active_profile_for_tiktok_account(const Profile &profile) const
	{
		const QString account = tiktok_account_id(profile);
		if (account.isEmpty())
			return nullptr;
		for (const Profile &other : profiles_) {
			if (other.id != profile.id && (other.live || other.preparing) &&
				tiktok_account_id(other) == account)
				return &other;
		}
		return nullptr;
	}

QString BridgeDock::tiktok_account_conflict_message(const Profile &profile, const Profile &active) const
	{
		return text("Error.TikTokAccountAlreadyActive").arg(
			profile.display_name, profile.tiktok_username, active.display_name, active.output_name);
	}

bool BridgeDock::tiktok_account_in_use_by_another_profile(const Profile &profile) const
	{
		return active_profile_for_tiktok_account(profile) != nullptr;
	}

Profile *BridgeDock::choose_profile_for_output(const QString &output_name, const std::vector<Profile *> &profiles)
	{
		QStringList choices;
		for (const Profile *profile : profiles) {
			QString choice = profile->display_name;
			if (!profile->tiktok_username.isEmpty())
				choice += QStringLiteral(" (@%1)").arg(profile->tiktok_username);
			choices.push_back(choice);
		}
		bool accepted = false;
		const bool german = obs_language().startsWith(QStringLiteral("de"), Qt::CaseInsensitive);
		const QString title = translated_or("OneClick.ProfileChoiceTitle",
			german ? QStringLiteral("TikTok-Profil für %1") : QStringLiteral("TikTok profile for %1")).arg(output_name);
		const QString prompt = translated_or("OneClick.ProfileChoicePrompt",
			german ? QStringLiteral("Aitum-Stream-Ausgabe: %1\n\nMit welchem Profil möchtest du live gehen?")
			       : QStringLiteral("Aitum stream output: %1\n\nWhich profile would you like to go live with?")).arg(output_name);
		const QString selected = QInputDialog::getItem(this, title, prompt, choices, 0, false, &accepted);
		if (!accepted)
			return nullptr;
		const int index = choices.indexOf(selected);
		return index >= 0 ? profiles.at(static_cast<size_t>(index)) : nullptr;
	}
