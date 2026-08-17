// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include <functional>

#include <QObject>
#include <QPointer>
#include <QString>

class QDialog;
class QWidget;

struct StreamCredentials {
	QString server;
	QString key;
	QString target_output;
};

enum class BridgeResult {
	Success,
	Busy,
	AitumNotAvailable,
	TargetOutputMissing,
	OutputTabMissing,
	OutputActionMissing,
	SaveFailed,
	SettingsAlreadyOpen,
};

// A GUI-thread state machine. It never injects mouse or keyboard input.
// Aitum actions are queued as separate Qt events so its own event loop can
// complete each dialog operation before the bridge observes the next state.
class AitumBridge final : public QObject {
public:
	using Completion = std::function<void(BridgeResult)>;

	explicit AitumBridge(QWidget *obs_main_window, QObject *parent = nullptr);
	// The outer Aitum settings dialog is a transaction: only its OK button
	// commits a saved output to Aitum's live profile configuration.
	void update_async(StreamCredentials credentials, Completion completion);

private:
	enum class Phase { Idle, FindSettings, FindTargetOutput, FindEditDialog, WaitAfterSave };

	void schedule_next(int milliseconds = 0);
	void advance();
	void find_settings();
	void start_settings();
	void open_target_editor();
	void find_target_output();
	void find_editor();
	void wait_after_save();
	void finish(BridgeResult result);

	QWidget *obs_main_window_ = nullptr;
	QPointer<QDialog> settings_dialog_;
	QPointer<QDialog> stream_dialog_;
	StreamCredentials credentials_;
	Completion completion_;
	Phase phase_ = Phase::Idle;
	bool opened_settings_ = false;
	int retries_ = 0;
};
