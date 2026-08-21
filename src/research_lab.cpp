// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "research_lab.hpp"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace {

constexpr int heartbeat_interval_ms = 2000;

QByteArray json_response(const QByteArray &body)
{
	return "HTTP/1.1 200 OK\r\n"
		"Content-Type: application/json\r\n"
		"Connection: close\r\n"
		"Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body;
}

} // namespace

ResearchLab::ResearchLab(QObject *parent) : QObject(parent)
{
	server_ = new QTcpServer(this);
	network_ = new QNetworkAccessManager(this);
	heartbeat_timer_ = new QTimer(this);
	heartbeat_timer_->setInterval(heartbeat_interval_ms);
	connect(heartbeat_timer_, &QTimer::timeout, this, &ResearchLab::send_heartbeat);
	connect(server_, &QTcpServer::newConnection, this, &ResearchLab::accept_connections);
}

ResearchLab::~ResearchLab()
{
	stop();
}

bool ResearchLab::start(const QString &session_id, QString *error)
{
	stop();
	if (session_id.trimmed().isEmpty()) {
		if (error)
			*error = QStringLiteral("A local research session needs an identifier.");
		return false;
	}

	if (!server_->listen(QHostAddress::LocalHost, 0)) {
		if (error)
			*error = QStringLiteral("The local research service could not start: %1").arg(server_->errorString());
		return false;
	}

	session_id_ = session_id;
	heartbeat_sequence_ = 0;
	initial_heartbeat_confirmed_ = false;
	heartbeat_timer_->start();
	send_heartbeat();
	return true;
}

void ResearchLab::stop()
{
	heartbeat_timer_->stop();
	if (server_->isListening())
		server_->close();
	session_id_.clear();
	heartbeat_sequence_ = 0;
	initial_heartbeat_confirmed_ = false;
}

bool ResearchLab::active() const
{
	return !session_id_.isEmpty() && server_->isListening();
}

QString ResearchLab::session_id() const
{
	return session_id_;
}

void ResearchLab::set_status_callback(StatusCallback callback)
{
	status_callback_ = std::move(callback);
}

void ResearchLab::send_heartbeat()
{
	if (!active())
		return;

	QJsonObject payload;
	payload.insert(QStringLiteral("session"), session_id_);
	payload.insert(QStringLiteral("sequence"), static_cast<qint64>(++heartbeat_sequence_));

	QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1/heartbeat").arg(server_->serverPort())));
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	QNetworkReply *reply = network_->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, this, [this, reply] {
		const bool succeeded = reply->error() == QNetworkReply::NoError;
		const QString error = reply->errorString();
		reply->deleteLater();
		if (!active())
			return;
		if (!succeeded) {
			report_status(false, error);
			return;
		}
		if (!initial_heartbeat_confirmed_) {
			initial_heartbeat_confirmed_ = true;
			report_status(true);
		}
	});
}

void ResearchLab::accept_connections()
{
	while (QTcpSocket *socket = server_->nextPendingConnection()) {
		connect(socket, &QTcpSocket::readyRead, socket, [socket] {
			const QByteArray request = socket->readAll();
			if (!request.contains("\r\n\r\n"))
				return;
			const QByteArray body = QByteArrayLiteral("{\"ok\":true,\"service\":\"research-lab\"}");
			socket->write(json_response(body));
			socket->disconnectFromHost();
		});
		connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
	}
}

void ResearchLab::report_status(bool healthy, const QString &detail)
{
	if (status_callback_)
		status_callback_(session_id_, healthy, detail);
}
