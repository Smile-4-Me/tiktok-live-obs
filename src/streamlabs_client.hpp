// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QVector>

class QTcpServer;

struct StreamlabsAccount {
	QString username;
	QString application_status;
	bool can_go_live = false;
};

struct StreamlabsLive {
	QString id;
	QString server;
	QString key;
};

struct StreamlabsEndResult {
	bool ended = false;
	// Streamlabs explicitly returned success: false. This normally means that
	// the saved session no longer exists on the service.
	bool stale_session = false;
	QString error;
};

struct StreamlabsCategory {
	QString name;
	QString id;
};

class StreamlabsClient final : public QObject {
public:
	using TokenCallback = std::function<void(QString token, QString error)>;
	using AccountCallback = std::function<void(StreamlabsAccount account, QString error)>;
	using LiveCallback = std::function<void(StreamlabsLive live, QString error)>;
	using EndCallback = std::function<void(StreamlabsEndResult result)>;
	using CategoriesCallback = std::function<void(QVector<StreamlabsCategory> categories, QString error)>;

	explicit StreamlabsClient(QObject *parent = nullptr);
	void verify_account(const QString &token, AccountCallback completion);
	void start_live(const QString &token, const QString &title, const QString &category, bool mature, LiveCallback completion);
	void end_live(const QString &token, const QString &live_id, EndCallback completion);
	void search_categories(const QString &token, const QString &query, CategoriesCallback completion);
	void login_in_browser(TokenCallback completion);
	void cancel_login();

private:
	void exchange_code(const QString &code);
	void finish_login(const QString &token, const QString &error);

	QTcpServer *callback_server_ = nullptr;
	QString verifier_;
	TokenCallback token_callback_;
};
