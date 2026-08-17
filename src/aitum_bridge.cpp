// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "aitum_bridge.hpp"
#include "aitum_contract.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

namespace {

constexpr int poll_interval_ms = 25;
constexpr int maximum_polls = 200;

QDialog *find_settings_dialog()
{
	for (QWidget *widget : QApplication::topLevelWidgets()) {
		// Aitum keeps one OBSBasicSettings instance alive after it is closed.
		// It remains a top-level widget, so title matching alone mistakes that
		// hidden instance for a dialog the user is actively editing.
		if (widget->isVisible() && widget->windowTitle() == QString::fromUtf8(aitum_contract::settings_window_title))
			return qobject_cast<QDialog *>(widget);
	}
	return nullptr;
}

QDialog *find_stream_output_dialog()
{
	for (QWidget *widget : QApplication::topLevelWidgets()) {
		auto *dialog = qobject_cast<QDialog *>(widget);
		if (dialog && dialog->windowTitle().contains("Aitum Stream Suite:") &&
		    dialog->windowTitle().contains("Stream Output"))
			return dialog;
	}
	return nullptr;
}

QGroupBox *find_output_group(QAbstractButton *output_button)
{
	for (QWidget *candidate = output_button; candidate; candidate = candidate->parentWidget()) {
		auto *group = qobject_cast<QGroupBox *>(candidate);
		if (!group)
			continue;
		for (QAbstractButton *button : group->findChildren<QAbstractButton *>()) {
			if (button->text() == QString::fromUtf8(aitum_contract::output_settings_button_text))
				return group;
		}
	}
	return nullptr;
}

QGroupBox *find_output_group_by_name(QDialog *settings, const QString &output_name)
{
	// The title is checkable in Aitum and is currently a QToolButton. Use the
	// base class rather than that implementation detail: different Aitum builds
	// can expose the same UI as another QAbstractButton subclass.
	for (QAbstractButton *button : settings->findChildren<QAbstractButton *>()) {
		if (button->text() == output_name) {
			if (QGroupBox *group = find_output_group(button))
				return group;
		}
	}
	return nullptr;
}

QGroupBox *find_target_group(QDialog *settings, const QString &output_name)
{
	return find_output_group_by_name(settings, output_name);
}

QAbstractButton *find_button(QWidget *parent, const char *text)
{
	for (QAbstractButton *button : parent->findChildren<QAbstractButton *>()) {
		if (button->text() == QString::fromUtf8(text))
			return button;
	}
	return nullptr;
}

bool queue_click(QAbstractButton *button)
{
	return button && QMetaObject::invokeMethod(button, "click", Qt::QueuedConnection);
}

bool set_line_edit_value(QLineEdit *field, const QString &value)
{
	if (!field)
		return false;
	field->setText(value);
	// Aitum intentionally stores these fields through textEdited rather than
	// textChanged. There is no keyboard/mouse injection: this invokes only the
	// same Qt signal handler Aitum registered for the field.
	return QMetaObject::invokeMethod(field, "textEdited", Qt::DirectConnection, Q_ARG(QString, value));
}

} // namespace

AitumBridge::AitumBridge(QWidget *obs_main_window, QObject *parent)
	: QObject(parent), obs_main_window_(obs_main_window)
{
}

void AitumBridge::update_async(StreamCredentials credentials, Completion completion)
{
	if (phase_ != Phase::Idle) {
		completion(BridgeResult::Busy);
		return;
	}
	credentials_ = std::move(credentials);
	completion_ = std::move(completion);
	opened_settings_ = false;
	retries_ = 0;
	phase_ = Phase::FindSettings;
	schedule_next();
}

void AitumBridge::schedule_next(int milliseconds)
{
	QTimer::singleShot(milliseconds, this, [this] { advance(); });
}

void AitumBridge::advance()
{
	switch (phase_) {
	case Phase::FindSettings: find_settings(); break;
	case Phase::FindTargetOutput: find_target_output(); break;
	case Phase::FindEditDialog: find_editor(); break;
	case Phase::WaitAfterSave: wait_after_save(); break;
	case Phase::Idle: break;
	}
}

void AitumBridge::find_settings()
{
	if (QDialog *settings = find_settings_dialog()) {
		settings_dialog_ = settings;
		// Do not attach to a dialog the user already has open. Apart from being
		// surprising, that dialog is an uncommitted transaction and its X button
		// discards the pending values.
		if (!opened_settings_) {
			finish(BridgeResult::SettingsAlreadyOpen);
			return;
		}
		retries_ = 0;
		open_target_editor();
		return;
	}
	if (retries_++ >= maximum_polls) {
		finish(BridgeResult::AitumNotAvailable);
		return;
	}
	if (retries_ == 1)
		start_settings();
	schedule_next(poll_interval_ms);
}

void AitumBridge::start_settings()
{
	if (!obs_main_window_) {
		finish(BridgeResult::AitumNotAvailable);
		return;
	}
	auto *button = obs_main_window_->findChild<QToolButton *>(
		QString::fromUtf8(aitum_contract::settings_button_object_name));
	if (!button || !QMetaObject::invokeMethod(button, "click", Qt::QueuedConnection)) {
		finish(BridgeResult::AitumNotAvailable);
		return;
	}
	opened_settings_ = true;
}

void AitumBridge::open_target_editor()
{
	QDialog *settings = settings_dialog_.data();
	if (!settings) {
		finish(BridgeResult::AitumNotAvailable);
		return;
	}
	auto *navigation = settings->findChild<QListWidget *>();
	if (!navigation) {
		finish(BridgeResult::OutputTabMissing);
		return;
	}
	const auto items = navigation->findItems(QString::fromUtf8(aitum_contract::output_tab_text), Qt::MatchExactly);
	if (items.size() != 1) {
		finish(BridgeResult::OutputTabMissing);
		return;
	}
	navigation->setCurrentItem(items.front());
	// Aitum constructs the Output page lazily. The settings dialog may already
	// be visible while the output QGroupBoxes do not exist yet, especially after
	// a fresh OBS start. Wait for those widgets rather than treating that brief
	// construction window as a missing TikTok output.
	retries_ = 0;
	phase_ = Phase::FindTargetOutput;
	schedule_next(poll_interval_ms);
}


void AitumBridge::find_target_output()
{
	QDialog *settings = settings_dialog_.data();
	if (!settings) {
		finish(BridgeResult::AitumNotAvailable);
		return;
	}
	QGroupBox *target = find_target_group(settings, credentials_.target_output);
	if (!target) {
		if (retries_++ >= maximum_polls)
			finish(BridgeResult::TargetOutputMissing);
		else
			schedule_next(poll_interval_ms);
		return;
	}
	auto *button = find_button(target, aitum_contract::output_settings_button_text);
	if (!queue_click(button)) {
		finish(BridgeResult::OutputActionMissing);
		return;
	}
	retries_ = 0;
	phase_ = Phase::FindEditDialog;
	schedule_next(poll_interval_ms);
}

void AitumBridge::find_editor()
{
	QDialog *dialog = find_stream_output_dialog();
	if (!dialog) {
		if (retries_++ >= maximum_polls)
			finish(BridgeResult::SaveFailed);
		else
			schedule_next(poll_interval_ms);
		return;
	}
	stream_dialog_ = dialog;
	const auto fields = dialog->findChildren<QLineEdit *>();
	if (fields.size() != 3) {
		finish(BridgeResult::SaveFailed);
		return;
	}
	if (!set_line_edit_value(fields.at(1), credentials_.server) ||
	    !set_line_edit_value(fields.at(2), credentials_.key)) {
		finish(BridgeResult::SaveFailed);
		return;
	}
	if (!queue_click(find_button(dialog, aitum_contract::save_output_button_text))) {
		finish(BridgeResult::SaveFailed);
		return;
	}
	phase_ = Phase::WaitAfterSave;
	schedule_next(poll_interval_ms);
}

void AitumBridge::wait_after_save()
{
	if (find_stream_output_dialog()) {
		if (retries_++ >= maximum_polls)
			finish(BridgeResult::SaveFailed);
		else
			schedule_next(poll_interval_ms);
		return;
	}
	retries_ = 0;
	// Save Output has updated Aitum's temporary settings object. Finish with
	// OK on the outer dialog so Aitum commits it to the live profile and reloads
	// outputs. Do not reopen for verification: that re-rendered page can lag and
	// previously produced a false "not found" after a successful write.
	finish(BridgeResult::Success);
}

void AitumBridge::finish(BridgeResult result)
{
	if (phase_ == Phase::Idle)
		return;
	phase_ = Phase::Idle;
	if (opened_settings_ && settings_dialog_) {
		if (result == BridgeResult::Success)
			settings_dialog_->accept();
		else
			settings_dialog_->reject();
	}
	Completion completion = std::move(completion_);
	completion_ = {};
	if (completion)
		completion(result);
}
