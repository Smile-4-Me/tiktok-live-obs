// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "tiktok_studio_client.hpp"

#include <QDialog>

#include <optional>

class QCloseEvent;
class QLabel;
class QPushButton;
class QTimer;

class TikTokStudioLoginDialog final : public QDialog {
public:
	TikTokStudioLoginDialog(TikTokStudioClient *client,
		TikTokStudioAccountCredentials account, QWidget *parent = nullptr);
	~TikTokStudioLoginDialog() override;

	[[nodiscard]] std::optional<TikTokStudioQrPoll> result() const;
	[[nodiscard]] TikTokStudioAccountCredentials account() const;

protected:
	void closeEvent(QCloseEvent *event) override;

private:
	void start_login();
	void poll_login();
	void cancel_login();
	void show_retry(const QString &message, bool can_generate_new_code = false);

	TikTokStudioClient *client_ = nullptr;
	TikTokStudioAccountCredentials account_;
	QLabel *instructions_ = nullptr;
	QLabel *qr_code_ = nullptr;
	QLabel *status_ = nullptr;
	QPushButton *retry_ = nullptr;
	QPushButton *close_button_ = nullptr;
	QTimer *poll_timer_ = nullptr;
	bool poll_in_flight_ = false;
	bool closing_ = false;
	std::optional<TikTokStudioQrPoll> result_;
};
