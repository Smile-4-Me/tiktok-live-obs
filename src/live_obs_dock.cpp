// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "live_obs_dock.hpp"

#include "aitum_outputs.hpp"
#include "localization.hpp"
#include "plugin_paths.hpp"
#include "token_store.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QInputDialog>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace {

constexpr char aitum_output_button_object_name[] = "canvasOutput";

QLabel *heading(const QString &value, QWidget *parent)
{
	auto *label = new QLabel(value, parent);
	QFont font = label->font();
	font.setBold(true);
	label->setFont(font);
	return label;
}

QLabel *info_card(const QString &content, QWidget *parent)
{
	auto *label = new QLabel(content, parent);
	label->setWordWrap(true);
	label->setOpenExternalLinks(true);
	label->setTextFormat(Qt::RichText);
	label->setFrameShape(QFrame::StyledPanel);
	label->setContentsMargins(10, 8, 10, 8);
	return label;
}

QString state_color(ProfileState state)
{
	switch (state) {
	case ProfileState::NeedsCredentials:
		return QStringLiteral("#e85b63");
	case ProfileState::Ready:
		return QStringLiteral("#f0b34b");
	case ProfileState::AppliedToAitum:
		return QStringLiteral("#65c878");
	}
	return QStringLiteral("#aab3c2");
}

} // namespace

LiveObsDock::LiveObsDock(QWidget *obs_main_window) : aitum_bridge_(obs_main_window, this)
{
	TokenStore::set_storage_scope(installation_storage_scope());
	qApp->installEventFilter(this);
	load_profiles();

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->addWidget(heading(text("Profiles.Title"), this));

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	auto *list_container = new QWidget(scroll);
	profile_list_layout_ = new QVBoxLayout(list_container);
	profile_list_layout_->setContentsMargins(0, 0, 0, 0);
	profile_list_layout_->setSpacing(6);
	scroll->setWidget(list_container);
	root->addWidget(scroll, 0);

	auto *add_button = new QPushButton(text("Profiles.Add"), this);
	connect(add_button, &QPushButton::clicked, this, [this] { add_profile(); });
	root->addWidget(add_button);

	auto *details = new QWidget(this);
	details_layout_ = new QVBoxLayout(details);
	details_layout_->setContentsMargins(0, 12, 0, 0);
	root->addWidget(details, 1);

	if (profiles_.empty())
		add_profile();
	else {
		selected_profile_ = 0;
		rebuild_profile_list();
		show_selected_profile();
	}
}

Profile LiveObsDock::new_profile() const
{
	Profile profile;
	profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	profile.display_name = text("Profile.NewName");
	return profile;
}

Profile *LiveObsDock::selected_profile()
{
	if (selected_profile_ < 0 || selected_profile_ >= static_cast<int>(profiles_.size()))
		return nullptr;
	return &profiles_[static_cast<size_t>(selected_profile_)];
}

Profile *LiveObsDock::find_profile(const QString &id)
{
	for (Profile &profile : profiles_)
		if (profile.id == id)
			return &profile;
	return nullptr;
}

bool LiveObsDock::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() != QEvent::MouseButtonRelease)
		return QWidget::eventFilter(watched, event);
	auto *button = qobject_cast<QAbstractButton *>(watched);
	const auto *mouse_event = static_cast<QMouseEvent *>(event);
	if (!button || mouse_event->button() != Qt::LeftButton ||
		button->objectName() != QString::fromUtf8(aitum_output_button_object_name))
		return QWidget::eventFilter(watched, event);

	const QString output_name = output_name_for_aitum_button(button);
	if (output_name.isEmpty() || button->isChecked())
		return QWidget::eventFilter(watched, event);

	std::vector<Profile *> configured;
	for (Profile &profile : profiles_)
		if (profile.output_name == output_name && manual_provider_.is_configured(profile.id))
			configured.push_back(&profile);
	if (configured.empty())
		return QWidget::eventFilter(watched, event);
	if (outputs_preparing_.contains(output_name)) {
		QMessageBox::information(this, text("Plugin.Name"), text("Aitum.AlreadyPreparing"));
		return true;
	}

	Profile *profile = configured.size() == 1
		? configured.front()
		: choose_profile_for_output(output_name, configured);
	if (!profile)
		return true;
	outputs_preparing_.insert(output_name);
	start_linked_aitum_output(profile->id, output_name);
	return true;
}

QString LiveObsDock::output_name_for_aitum_button(const QAbstractButton *button) const
{
	for (const QWidget *candidate = button->parentWidget(); candidate; candidate = candidate->parentWidget()) {
		const QString name = candidate->objectName();
		if (!name.isEmpty() && name != QString::fromUtf8(aitum_output_button_object_name))
			return name;
	}
	return {};
}

Profile *LiveObsDock::choose_profile_for_output(const QString &output_name,
	const std::vector<Profile *> &profiles)
{
	QStringList choices;
	for (const Profile *profile : profiles)
		choices.push_back(profile->display_name + QStringLiteral(" (@%1)").arg(profile->tiktok_username));
	bool accepted = false;
	const QString choice = QInputDialog::getItem(this, text("Aitum.ProfileChoiceTitle").arg(output_name),
		text("Aitum.ProfileChoicePrompt").arg(output_name), choices, 0, false, &accepted);
	if (!accepted)
		return nullptr;
	const int index = choices.indexOf(choice);
	return index >= 0 ? profiles.at(static_cast<size_t>(index)) : nullptr;
}

void LiveObsDock::load_profiles()
{
	QSettings settings(profiles_settings_path(), QSettings::IniFormat);
	const int count = settings.beginReadArray(QStringLiteral("profiles"));
	for (int index = 0; index < count; ++index) {
		settings.setArrayIndex(index);
		Profile profile;
		profile.id = settings.value(QStringLiteral("id")).toString();
		profile.display_name = settings.value(QStringLiteral("displayName")).toString();
		profile.tiktok_username = settings.value(QStringLiteral("username")).toString();
		profile.output_name = settings.value(QStringLiteral("outputName")).toString();
		profile.stream_title = settings.value(QStringLiteral("streamTitle")).toString();
		profile.category = settings.value(QStringLiteral("category")).toString();
		profile.mature = settings.value(QStringLiteral("mature"), false).toBool();
		profile.credentials_applied = settings.value(QStringLiteral("credentialsApplied"), false).toBool();
		if (!profile.id.isEmpty())
			profiles_.push_back(profile);
	}
	settings.endArray();
}

void LiveObsDock::save_profiles() const
{
	QSettings settings(profiles_settings_path(), QSettings::IniFormat);
	settings.clear();
	settings.beginWriteArray(QStringLiteral("profiles"));
	for (int index = 0; index < static_cast<int>(profiles_.size()); ++index) {
		const Profile &profile = profiles_[static_cast<size_t>(index)];
		settings.setArrayIndex(index);
		settings.setValue(QStringLiteral("id"), profile.id);
		settings.setValue(QStringLiteral("displayName"), profile.display_name);
		settings.setValue(QStringLiteral("username"), profile.tiktok_username);
		settings.setValue(QStringLiteral("outputName"), profile.output_name);
		settings.setValue(QStringLiteral("streamTitle"), profile.stream_title);
		settings.setValue(QStringLiteral("category"), profile.category);
		settings.setValue(QStringLiteral("mature"), profile.mature);
		settings.setValue(QStringLiteral("credentialsApplied"), profile.credentials_applied);
	}
	settings.endArray();
	settings.sync();
}

void LiveObsDock::clear_layout(QLayout *layout)
{
	while (QLayoutItem *item = layout->takeAt(0)) {
		if (QWidget *widget = item->widget())
			widget->deleteLater();
		delete item;
	}
}

void LiveObsDock::rebuild_profile_list()
{
	clear_layout(profile_list_layout_);
	for (int index = 0; index < static_cast<int>(profiles_.size()); ++index) {
		const Profile &profile = profiles_[static_cast<size_t>(index)];
		auto *row = new QWidget(this);
		auto *layout = new QHBoxLayout(row);
		layout->setContentsMargins(0, 0, 0, 0);
		auto *button = new QPushButton(profile.display_name +
			(profile.tiktok_username.isEmpty() ? QString{} : QStringLiteral(" (@%1)").arg(profile.tiktok_username)), row);
		button->setCheckable(true);
		button->setChecked(index == selected_profile_);
		connect(button, &QPushButton::clicked, this, [this, index] {
			selected_profile_ = index;
			rebuild_profile_list();
			show_selected_profile();
		});
		layout->addWidget(button, 1);
		auto *dot = new QLabel(row);
		dot->setFixedSize(14, 14);
		dot->setToolTip(status_text(profile));
		dot->setStyleSheet(QStringLiteral("border-radius: 7px; background: %1; border: 1px solid palette(mid);")
			.arg(state_color(profile.state())));
		layout->addWidget(dot);
		profile_list_layout_->addWidget(row);
	}
	profile_list_layout_->addStretch();
}

QString LiveObsDock::status_text(const Profile &profile) const
{
	switch (profile.state()) {
	case ProfileState::NeedsCredentials:
		return text("Status.NeedsCredentials");
	case ProfileState::Ready:
		return text("Status.Ready");
	case ProfileState::AppliedToAitum:
		return text("Status.AppliedToAitum");
	}
	return {};
}

QStringList LiveObsDock::available_outputs(QString *diagnostic) const
{
	if (!aitum_stream_suite_available()) {
		if (diagnostic)
			*diagnostic = text("Aitum.NotAvailable");
		return {};
	}
	return aitum_output_names(diagnostic);
}

void LiveObsDock::show_selected_profile()
{
	clear_layout(details_layout_);
	Profile *profile = selected_profile();
	if (!profile)
		return;

	details_layout_->addWidget(heading(text("Profile.Title"), this));
	auto *form = new QFormLayout;
	auto *name = new QLineEdit(profile->display_name, this);
	form->addRow(text("Profile.Name"), name);
	connect(name, &QLineEdit::editingFinished, this, [this, profile, name] {
		profile->display_name = name->text().trimmed().isEmpty() ? text("Profile.NewName") : name->text().trimmed();
		save_profiles();
		rebuild_profile_list();
	});
	details_layout_->addLayout(form);

	details_layout_->addWidget(heading(text("Manual.Title"), this));
	details_layout_->addWidget(info_card(text("Manual.Description"), this));
	auto *credentials = new QFormLayout;
	auto *username = new QLineEdit(profile->tiktok_username, this);
	auto *url = new QLineEdit(this);
	auto *key = new QLineEdit(this);
	key->setEchoMode(QLineEdit::Password);
	const LiveCredentials stored = TokenStore::load_live_credentials(profile->id);
	url->setText(stored.server);
	key->setText(stored.key);
	credentials->addRow(text("Manual.Username"), username);
	credentials->addRow(text("Manual.Url"), url);
	credentials->addRow(text("Manual.Key"), key);
	details_layout_->addLayout(credentials);
	auto *save_credentials = new QPushButton(text("Manual.Save"), this);
	connect(save_credentials, &QPushButton::clicked, this, [this, profile, username, url, key] {
		save_manual_credentials(profile->id, username->text(), url->text(), key->text());
	});
	details_layout_->addWidget(save_credentials);

	details_layout_->addWidget(heading(text("Aitum.Title"), this));
	QString output_diagnostic;
	const QStringList outputs = available_outputs(&output_diagnostic);
	auto *output = new QComboBox(this);
	output->addItem(text("Aitum.Manual"), QString{});
	for (const QString &value : outputs)
		output->addItem(value, value);
	const int selected_output = output->findData(profile->output_name);
	output->setCurrentIndex(selected_output >= 0 ? selected_output : 0);
	output->setToolTip(output_diagnostic);
	form = new QFormLayout;
	form->addRow(text("Aitum.Output"), output);
	details_layout_->addLayout(form);
	connect(output, &QComboBox::currentIndexChanged, this, [this, profile, output] {
		profile->output_name = output->currentData().toString();
		profile->credentials_applied = false;
		save_profiles();
		rebuild_profile_list();
	});

	auto *apply = new QPushButton(text("Aitum.Apply"), this);
	apply->setEnabled(!profile->output_name.isEmpty() && manual_provider_.is_configured(profile->id));
	connect(apply, &QPushButton::clicked, this, [this, profile, output] {
		apply_to_aitum(profile->id, output->currentData().toString());
	});
	details_layout_->addWidget(apply);
	if (!profile->diagnostic.isEmpty()) {
		auto *diagnostic = new QLabel(profile->diagnostic, this);
		diagnostic->setWordWrap(true);
		diagnostic->setStyleSheet(profile->diagnostic_error ? QStringLiteral("color: #e85b63;") : QStringLiteral("color: #65c878;"));
		details_layout_->addWidget(diagnostic);
	}

	auto *delete_button = new QPushButton(text("Profile.Delete"), this);
	delete_button->setStyleSheet(QStringLiteral("color: #e85b63;"));
	connect(delete_button, &QPushButton::clicked, this, [this] { delete_selected_profile(); });
	details_layout_->addWidget(delete_button, 0, Qt::AlignRight);
	details_layout_->addStretch();
}

void LiveObsDock::add_profile()
{
	profiles_.push_back(new_profile());
	selected_profile_ = static_cast<int>(profiles_.size()) - 1;
	save_profiles();
	rebuild_profile_list();
	show_selected_profile();
}

void LiveObsDock::delete_selected_profile()
{
	Profile *profile = selected_profile();
	if (!profile)
		return;
	if (QMessageBox::question(this, text("Profile.Delete"), text("Profile.DeleteConfirmation").arg(profile->display_name))
		!= QMessageBox::Yes)
		return;
	manual_provider_.remove(profile->id);
	profiles_.erase(profiles_.begin() + selected_profile_);
	selected_profile_ = profiles_.empty() ? -1 : 0;
	save_profiles();
	rebuild_profile_list();
	show_selected_profile();
}

void LiveObsDock::set_diagnostic(Profile &profile, QString message, bool error)
{
	profile.diagnostic = std::move(message);
	profile.diagnostic_error = error;
}

void LiveObsDock::save_manual_credentials(const QString &profile_id, QString username, QString url, QString key)
{
	Profile *profile = selected_profile();
	if (!profile || profile->id != profile_id)
		return;
	QString error;
	if (!manual_provider_.save(profile_id, username, url, key, &error)) {
		set_diagnostic(*profile, error, true);
	} else {
		profile->tiktok_username = username.trimmed();
		profile->credentials_applied = false;
		set_diagnostic(*profile, text("Manual.Saved"));
	}
	save_profiles();
	rebuild_profile_list();
	show_selected_profile();
}

void LiveObsDock::apply_to_aitum(const QString &profile_id, const QString &output_name)
{
	Profile *profile = selected_profile();
	if (!profile || profile->id != profile_id)
		return;
	QString error;
	const SessionDescriptor session = manual_provider_.session_for(profile_id, &error);
	if (session.ingest_url.isEmpty()) {
		set_diagnostic(*profile, error, true);
		show_selected_profile();
		return;
	}
	set_diagnostic(*profile, text("Aitum.Applying"));
	show_selected_profile();
	aitum_bridge_.update_async({session.ingest_url, session.stream_key, output_name}, [this, profile_id](BridgeResult result) {
		Profile *updated = find_profile(profile_id);
		if (!updated)
			return;
		if (result == BridgeResult::Success) {
			updated->credentials_applied = true;
			set_diagnostic(*updated, text("Aitum.Applied"));
		} else {
			updated->credentials_applied = false;
			set_diagnostic(*updated, text("Aitum.ApplyFailed"), true);
		}
		save_profiles();
		rebuild_profile_list();
		if (selected_profile() == updated)
			show_selected_profile();
	});
}

void LiveObsDock::start_linked_aitum_output(const QString &profile_id, const QString &output_name)
{
	Profile *profile = find_profile(profile_id);
	if (!profile) {
		outputs_preparing_.remove(output_name);
		return;
	}
	QString error;
	const SessionDescriptor session = manual_provider_.session_for(profile_id, &error);
	if (session.ingest_url.isEmpty()) {
		set_diagnostic(*profile, error, true);
		outputs_preparing_.remove(output_name);
		show_selected_profile();
		return;
	}

	set_diagnostic(*profile, text("Aitum.Applying"));
	if (selected_profile() == profile)
		show_selected_profile();
	aitum_bridge_.update_async({session.ingest_url, session.stream_key, output_name},
		[this, profile_id, output_name](BridgeResult result) {
			Profile *updated = find_profile(profile_id);
			if (!updated) {
				outputs_preparing_.remove(output_name);
				return;
			}
			if (result != BridgeResult::Success) {
				updated->credentials_applied = false;
				set_diagnostic(*updated, text("Aitum.ApplyFailed"), true);
				outputs_preparing_.remove(output_name);
				save_profiles();
				rebuild_profile_list();
				if (selected_profile() == updated)
					show_selected_profile();
				return;
			}

			updated->credentials_applied = true;
			set_diagnostic(*updated, text("Aitum.StartingOutput"));
			save_profiles();
			rebuild_profile_list();
			if (selected_profile() == updated)
				show_selected_profile();
			QTimer::singleShot(250, this, [this, profile_id, output_name] {
				QString diagnostic;
				const bool started = aitum_start_output(output_name, &diagnostic);
				outputs_preparing_.remove(output_name);
				if (Profile *current = find_profile(profile_id)) {
					set_diagnostic(*current, started ? text("Aitum.Started")
						: text("Aitum.StartFailed").arg(diagnostic), !started);
					save_profiles();
					rebuild_profile_list();
					if (selected_profile() == current)
						show_selected_profile();
				}
			});
		});
}
