// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <functional>
#include <utility>

class QNetworkAccessManager;
class QTcpServer;
class QTimer;

// Local-only research harness. It validates the scheduling, health-check, and
// teardown shape of a transport companion without connecting to TikTok or
// altering any media stream.
class ResearchLab final : public QObject {
public:
	// The callback carries a health result and an optional diagnostic only. The
	// dock owns user-facing text so the Research Lab stays fully localized.
	using StatusCallback = std::function<void(const QString &session_id, bool healthy, const QString &detail)>;

	explicit ResearchLab(QObject *parent = nullptr);
	~ResearchLab() override;

	bool start(const QString &session_id, QString *error = nullptr);
	void stop();
	[[nodiscard]] bool active() const;
	[[nodiscard]] QString session_id() const;
	void set_status_callback(StatusCallback callback);

private:
	void send_heartbeat();
	void accept_connections();
	void report_status(bool healthy, const QString &detail = {});

	QTcpServer *server_ = nullptr;
	QNetworkAccessManager *network_ = nullptr;
	QTimer *heartbeat_timer_ = nullptr;
	StatusCallback status_callback_;
	QString session_id_;
	quint64 heartbeat_sequence_ = 0;
	bool initial_heartbeat_confirmed_ = false;
};
