// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "aitum_bridge.hpp"
#include "aitum_contract.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaObject>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>

namespace {

constexpr int poll_interval_ms = 25;
constexpr int maximum_polls = 200;
constexpr qint64 maximum_trace_size_bytes = 128 * 1024;

QString bridge_trace_path()
{
	// OBS itself writes its runtime logs below APPDATA on Windows. Prefer that
	// known, user-visible location so a production build never loses an
	// important bridge diagnostic behind a Qt application-name variation.
	const QString obs_app_data = qEnvironmentVariable("APPDATA");
	if (!obs_app_data.isEmpty()) {
		const QString log_directory = obs_app_data + QStringLiteral("/obs-studio/logs");
		QDir().mkpath(log_directory);
		return log_directory + QStringLiteral("/tiktok-live-obs-aitum-bridge.log");
	}
	const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	QDir directory(base);
	directory.mkpath(QStringLiteral("tiktok-live-obs"));
	return directory.filePath(QStringLiteral("tiktok-live-obs/aitum-bridge.log"));
}

void trace_bridge_event(const QString &event)
{
	QFile trace(bridge_trace_path());
	if (trace.exists() && trace.size() > maximum_trace_size_bytes)
		trace.remove();
	if (!trace.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
		return;
	QTextStream stream(&trace);
	stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
		<< " " << event << '\n';
}

const char *bridge_result_name(BridgeResult result)
{
	switch (result) {
	case BridgeResult::Success: return "success";
	case BridgeResult::Busy: return "busy";
	case BridgeResult::AitumNotAvailable: return "aitum-not-available";
	case BridgeResult::TargetOutputMissing: return "target-output-missing";
	case BridgeResult::OutputTabMissing: return "output-tab-missing";
	case BridgeResult::OutputActionMissing: return "output-action-missing";
	case BridgeResult::EditorInvalid: return "editor-invalid";
	case BridgeResult::CredentialUpdateFailed: return "credential-update-failed";
	case BridgeResult::SaveActionMissing: return "save-action-missing";
	case BridgeResult::SaveConfirmationTimeout: return "save-confirmation-timeout";
	case BridgeResult::SaveFailed: return "save-failed";
	case BridgeResult::SettingsAlreadyOpen: return "settings-already-open";
	}
	return "unknown";
}

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
		// Aitum may retain a closed editor as a hidden top-level object until the
		// next UI refresh. Only a visible editor represents an unfinished save.
		// Treating that hidden object as open causes a false timeout even though
		// Aitum has already accepted the new server and stream key.
		if (dialog && dialog->isVisible() && dialog->windowTitle().contains("Aitum Stream Suite:") &&
		    dialog->windowTitle().contains("Stream Output"))
			return dialog;
	}
	return nullptr;
}

QDialog *find_owned_stream_output_dialog(QDialog *settings)
{
	if (!settings)
		return nullptr;
	// Aitum creates the editor with its settings dialog as parent. On some Qt
	// builds a window-modal child is temporarily absent from topLevelWidgets()
	// while its exec() loop is starting, although the object is already usable.
	for (QDialog *dialog : settings->findChildren<QDialog *>()) {
		if (dialog->windowTitle().contains("Aitum Stream Suite:") &&
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
	return QMetaObject::invokeMethod(field, "textEdited", Qt::DirectConnection,
		Q_ARG(QString, value));
}

} // namespace

AitumBridge::AitumBridge(QWidget *obs_main_window, QObject *parent)
	: QObject(parent), obs_main_window_(obs_main_window)
{
}

void AitumBridge::update_async(StreamCredentials credentials, Completion completion)
{
	if (phase_ != Phase::Idle) {
		trace_bridge_event(QStringLiteral("result=busy"));
		completion(BridgeResult::Busy);
		return;
	}
	credentials_ = std::move(credentials);
	trace_bridge_event(QStringLiteral("begin output=%1 server-present=%2 key-present=%3")
		.arg(credentials_.target_output, credentials_.server.trimmed().isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
			credentials_.key.trimmed().isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));
	qWarning().noquote() << "[TikTok Live OBS] Aitum bridge: updating output" << credentials_.target_output << ".";
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
		qWarning().noquote() << "[TikTok Live OBS] Aitum bridge: settings dialog found.";
		open_target_editor();
		return;
	}
	if (retries_++ >= maximum_polls) {
		finish(BridgeResult::AitumNotAvailable);
		return;
	}
	if (retries_ == 1)
		qWarning().noquote() << "[TikTok Live OBS] Aitum bridge: opening Aitum settings.";
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
	qWarning().noquote() << "[TikTok Live OBS] Aitum bridge: Output tab selected.";
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
	qWarning().noquote() << "[TikTok Live OBS] Aitum bridge: target output" << credentials_.target_output << "found.";
	auto *button = find_button(target, aitum_contract::output_settings_button_text);
	if (!queue_click(button)) {
		finish(BridgeResult::OutputActionMissing);
		return;
	}
	qWarning().noquote() << "[TikTok Live OBS] Aitum bridge: opening output editor.";
	retries_ = 0;
	phase_ = Phase::FindEditDialog;
	schedule_next(poll_interval_ms);
}

void AitumBridge::find_editor()
{
	QDialog *dialog = find_stream_output_dialog();
	if (!dialog) {
		dialog = find_owned_stream_output_dialog(settings_dialog_.data());
		if (dialog)
			trace_bridge_event(QStringLiteral("editor=owned-child-fallback"));
	}
	if (!dialog) {
		if (retries_ == 0 || retries_ == maximum_polls)
			trace_bridge_event(QStringLiteral("editor=not-found attempt=%1").arg(retries_));
		if (retries_++ >= maximum_polls)
			finish(BridgeResult::SaveActionMissing);
		else
			schedule_next(poll_interval_ms);
		return;
	}
	stream_dialog_ = dialog;
	const auto all_fields = dialog->findChildren<QLineEdit *>();
	QList<QLineEdit *> fields;
	for (QLineEdit *field : all_fields) {
		if (field->isVisible())
			fields.push_back(field);
	}
	// Use visible controls so a helper field from a future Aitum build cannot
	// shift the name/server/key positions. Fall back to the complete set only
	// while the editor is still being shown by Qt.
	if (fields.size() < 3)
		fields = all_fields;
	if (fields.size() < 3) {
		trace_bridge_event(QStringLiteral("editor=invalid-visible-fields count=%1 all=%2")
			.arg(fields.size()).arg(all_fields.size()));
		finish(BridgeResult::EditorInvalid);
		return;
	}
	const bool server_updated = set_line_edit_value(fields.at(1), credentials_.server);
	const bool key_updated = set_line_edit_value(fields.at(2), credentials_.key);
	if (!server_updated || !key_updated) {
		trace_bridge_event(QStringLiteral("editor=field-update-failed server=%1 key=%2")
			.arg(server_updated ? QStringLiteral("ok") : QStringLiteral("failed"),
				key_updated ? QStringLiteral("ok") : QStringLiteral("failed")));
		finish(BridgeResult::CredentialUpdateFailed);
		return;
	}
	QAbstractButton *save_button = find_button(dialog, aitum_contract::save_output_button_text);
	if (!queue_click(save_button)) {
		trace_bridge_event(QStringLiteral("editor=save-button-missing"));
		finish(BridgeResult::SaveActionMissing);
		return;
	}
	trace_bridge_event(QStringLiteral("editor=updated save=queued enabled=%1")
		.arg(save_button->isEnabled() ? QStringLiteral("yes") : QStringLiteral("no")));
	phase_ = Phase::WaitAfterSave;
	schedule_next(poll_interval_ms);
}

void AitumBridge::wait_after_save()
{
	// Only observe the exact editor opened for this transaction. Looking up all
	// top-level dialogs again can find an unrelated retained Aitum editor and
	// report a false failure after this editor has already saved successfully.
	if (stream_dialog_ && stream_dialog_->isVisible()) {
		// Aitum's Save Output button only calls QDialog::accept(). If the queued
		// click has not dismissed the editor after a short grace period, complete
		// that same documented dialog action directly. The fields were already
		// committed to Aitum's dialog model through its textEdited handlers.
		if (retries_ == 8) {
			trace_bridge_event(QStringLiteral("save=force-accept-after-200ms"));
			stream_dialog_->accept();
		}
		if (retries_++ >= maximum_polls)
			finish(BridgeResult::SaveConfirmationTimeout);
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
	trace_bridge_event(QStringLiteral("result=%1").arg(QString::fromUtf8(bridge_result_name(result))));
	qWarning().noquote() << "[TikTok Live OBS] Aitum bridge finished:" << bridge_result_name(result) << ".";
	Completion completion = std::move(completion_);
	completion_ = {};
	if (completion)
		completion(result);
}
