// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "streamlabs_client.hpp"

#include <curl/curl.h>

#include <thread>

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

namespace {

constexpr auto account_url = "https://streamlabs.com/api/v5/slobs/tiktok/info";
constexpr auto token_url = "https://streamlabs.com/api/v5/slobs/auth/data";
constexpr auto start_url = "https://streamlabs.com/api/v5/slobs/tiktok/stream/start";

struct HttpResult {
	long status = 0;
	QByteArray body;
	QString error;
};

QByteArray desktop_user_agent()
{
	return "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
	       "StreamlabsDesktop/1.20.4 Chrome/122.0.6261.156 Electron/29.3.1 Safari/537.36";
}

size_t append_response(char *data, size_t size, size_t count, void *user_data)
{
	auto *body = static_cast<QByteArray *>(user_data);
	const size_t bytes = size * count;
	body->append(data, static_cast<int>(bytes));
	return bytes;
}

HttpResult https_request(const QByteArray &method, const QUrl &url,
	                     const QList<QPair<QByteArray, QByteArray>> &headers = {},
	                     const QByteArray &body = {})
{
	HttpResult result;
	if (url.scheme() != QStringLiteral("https") || url.host().isEmpty()) {
		result.error = QStringLiteral("Invalid HTTPS address.");
		return result;
	}
	CURL *handle = curl_easy_init();
	if (!handle) {
		result.error = QStringLiteral("libcurl could not create an HTTP request.");
		return result;
	}
	curl_slist *header_list = nullptr;
	for (const auto &[name, value] : headers)
		header_list = curl_slist_append(header_list, (name + ": " + value).constData());
	const QByteArray encoded_url = url.toEncoded();
	curl_easy_setopt(handle, CURLOPT_URL, encoded_url.constData());
	curl_easy_setopt(handle, CURLOPT_USERAGENT, desktop_user_agent().constData());
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, append_response);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &result.body);
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header_list);
	if (method == "POST") {
		curl_easy_setopt(handle, CURLOPT_POST, 1L);
		curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.isEmpty() ? "" : body.constData());
		curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
	} else if (method != "GET") {
		curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method.constData());
	}
	const CURLcode code = curl_easy_perform(handle);
	if (code != CURLE_OK)
		result.error = QString::fromLatin1(curl_easy_strerror(code));
	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &result.status);
	curl_slist_free_all(header_list);
	curl_easy_cleanup(handle);
	return result;
}

QList<QPair<QByteArray, QByteArray>> authenticated_headers(const QString &token)
{
	return {{"Authorization", QByteArray("Bearer ") + token.toUtf8()}};
}

QByteArray multipart_body(const QList<QPair<QByteArray, QString>> &fields, QByteArray *content_type)
{
	const QByteArray boundary = "----TikTokLiveObs" + QByteArray::number(QRandomGenerator::global()->generate64(), 16);
	QByteArray body;
	for (const auto &[name, value] : fields) {
		body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + name + "\"\r\n\r\n";
		body += value.toUtf8() + "\r\n";
	}
	body += "--" + boundary + "--\r\n";
	*content_type = "multipart/form-data; boundary=" + boundary;
	return body;
}

template<typename Work, typename Done>
void run_async(StreamlabsClient *owner, Work work, Done done)
{
	QPointer<StreamlabsClient> guard(owner);
	std::thread([guard, work = std::move(work), done = std::move(done)]() mutable {
		auto value = work();
		if (!guard) return;
		QMetaObject::invokeMethod(guard, [guard, done = std::move(done), value = std::move(value)]() mutable {
			if (guard) done(std::move(value));
		}, Qt::QueuedConnection);
	}).detach();
}

} // namespace

StreamlabsClient::StreamlabsClient(QObject *parent) : QObject(parent)
{
	static const CURLcode initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
	callback_server_ = new QTcpServer(this);
	connect(callback_server_, &QTcpServer::newConnection, this, [this] {
		auto *socket = callback_server_->nextPendingConnection();
		connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
			const QByteArray request = socket->readAll();
			const QList<QByteArray> parts = request.split('\n').value(0).trimmed().split(' ');
			QUrl url(QStringLiteral("http://127.0.0.1") + QString::fromUtf8(parts.value(1)));
			QUrlQuery query(url);
			const QString code = query.queryItemValue(QStringLiteral("code"));
			const bool success = query.queryItemValue(QStringLiteral("success")) == QStringLiteral("true");
			const QByteArray body = success && !code.isEmpty() ? "<h2>Authentication successful. You can close this tab.</h2>"
			                                                    : "<h2>Authentication failed. Return to OBS and try again.</h2>";
			socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
			socket->disconnectFromHost();
			callback_server_->close();
			if (initialized != CURLE_OK) finish_login({}, QStringLiteral("libcurl initialization failed."));
			else if (success && !code.isEmpty()) exchange_code(code);
			else finish_login({}, QStringLiteral("Streamlabs login was not confirmed."));
		});
	});
}

void StreamlabsClient::verify_account(const QString &token, AccountCallback completion)
{
	run_async(this, [token] { return https_request("GET", QUrl(QString::fromLatin1(account_url)), authenticated_headers(token)); },
	          [completion = std::move(completion)](HttpResult result) mutable {
			const QJsonDocument document = QJsonDocument::fromJson(result.body);
			if (result.status != 200 || !document.isObject()) { completion({}, QStringLiteral("The token could not be verified with Streamlabs.")); return; }
			const QJsonObject object = document.object();
			StreamlabsAccount account {object.value("user").toObject().value("username").toString(), object.value("application_status").toObject().value("status").toString(), object.value("can_be_live").toBool(false)};
			if (account.username.isEmpty()) { completion({}, QStringLiteral("Streamlabs did not return TikTok account information.")); return; }
			completion(account, {});
		});
}

void StreamlabsClient::start_live(const QString &token, const QString &title, const QString &category, bool mature, LiveCallback completion)
{
	run_async(this, [token, title, category, mature] {
		QByteArray content_type;
		const QByteArray body = multipart_body({{"title", title}, {"device_platform", "win32"}, {"category", category}, {"audience_type", mature ? "1" : "0"}}, &content_type);
		auto headers = authenticated_headers(token); headers.push_back({"Content-Type", content_type});
		return https_request("POST", QUrl(QString::fromLatin1(start_url)), headers, body);
	}, [completion = std::move(completion)](HttpResult result) mutable {
		const QJsonObject object = QJsonDocument::fromJson(result.body).object();
		StreamlabsLive live {object.value("id").toVariant().toString(), object.value("rtmp").toString(), object.value("key").toString()};
		if (result.status < 200 || result.status >= 300 || live.id.isEmpty() || live.server.isEmpty() || live.key.isEmpty()) { completion({}, QStringLiteral("TikTok LIVE could not be created through Streamlabs.")); return; }
		completion(live, {});
	});
}

void StreamlabsClient::end_live(const QString &token, const QString &live_id, EndCallback completion)
{
	if (live_id.isEmpty()) { completion({false, false, QStringLiteral("No active stream ID is available.")}); return; }
	run_async(this, [token, live_id] { return https_request("POST", QUrl(QStringLiteral("https://streamlabs.com/api/v5/slobs/tiktok/stream/%1/end").arg(live_id)), authenticated_headers(token)); },
	          [completion = std::move(completion)](HttpResult result) mutable {
			const QJsonObject response = QJsonDocument::fromJson(result.body).object();
			const bool explicitly_failed = response.contains(QStringLiteral("success")) &&
				!response.value(QStringLiteral("success")).toBool();
			const bool ended = (result.status >= 200 && result.status < 300 && !explicitly_failed) || result.status == 404;
			if (ended) {
				completion({true, false, {}});
				return;
			}
			if (result.status >= 200 && result.status < 300 && explicitly_failed) {
				completion({false, true, {}});
				return;
			}
			const QString reason = result.status > 0 ? QStringLiteral("HTTP %1").arg(result.status) : result.error;
			QString detail = response.value(QStringLiteral("message")).toString();
			if (detail.isEmpty())
				detail = response.value(QStringLiteral("error")).toString();
			if (detail.isEmpty())
				detail = response.value(QStringLiteral("detail")).toString();
			completion({false, false, detail.isEmpty()
				? QStringLiteral("TikTok LIVE could not be ended (%1).").arg(reason)
				: QStringLiteral("TikTok LIVE could not be ended (%1: %2).").arg(reason, detail)});
		});
}

void StreamlabsClient::search_categories(const QString &token, const QString &query, CategoriesCallback completion)
{
	if (query.isEmpty()) { completion({}, {}); return; }
	run_async(this, [token, query] { QUrl url(QString::fromLatin1(account_url)); QUrlQuery q; q.addQueryItem("category", query.left(25)); url.setQuery(q); return https_request("GET", url, authenticated_headers(token)); },
	          [completion = std::move(completion)](HttpResult result) mutable {
			const QJsonDocument document = QJsonDocument::fromJson(result.body);
			if (result.status != 200 || !document.isObject()) { completion({}, QStringLiteral("Categories could not be loaded.")); return; }
			QVector<StreamlabsCategory> categories;
			for (const QJsonValue &value : document.object().value("categories").toArray()) { const QJsonObject item = value.toObject(); const QString name = item.value("full_name").toString(); if (!name.isEmpty()) categories.push_back({name, item.value("game_mask_id").toString()}); }
			categories.push_back({QStringLiteral("Other"), {}}); completion(categories, {});
		});
}

void StreamlabsClient::login_in_browser(TokenCallback completion)
{
	cancel_login(); token_callback_ = std::move(completion);
	QByteArray random(64, Qt::Uninitialized); for (char &byte : random) byte = static_cast<char>(QRandomGenerator::global()->bounded(256));
	verifier_ = random.toHex();
	const QByteArray challenge = QCryptographicHash::hash(verifier_.toUtf8(), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
	if (!callback_server_->listen(QHostAddress::LocalHost, 0)) { finish_login({}, QStringLiteral("The local login callback could not be started.")); return; }
	QUrl url(QStringLiteral("https://streamlabs.com/slobs/login")); QUrlQuery query;
	query.addQueryItem("skip_splash", "true"); query.addQueryItem("external", "electron"); query.addQueryItem("tiktok", {}); query.addQueryItem("force_verify", {}); query.addQueryItem("origin", "slobs"); query.addQueryItem("port", QString::number(callback_server_->serverPort())); query.addQueryItem("code_challenge", QString::fromLatin1(challenge)); query.addQueryItem("code_flow", "true"); url.setQuery(query);
	QDesktopServices::openUrl(url);
}

void StreamlabsClient::cancel_login()
{
	if (callback_server_->isListening()) callback_server_->close();
	token_callback_ = {}; verifier_.clear();
}

void StreamlabsClient::exchange_code(const QString &code)
{
	const QString verifier = verifier_;
	run_async(this, [code, verifier] { QUrl url(QString::fromLatin1(token_url)); QUrlQuery query; query.addQueryItem("code_verifier", verifier); query.addQueryItem("code", code); url.setQuery(query); return https_request("GET", url, {{"Accept", "*/*"}, {"Accept-Language", "en-US"}, {"Sec-Fetch-Site", "cross-site"}, {"Sec-Fetch-Mode", "cors"}, {"Sec-Fetch-Dest", "empty"}}); },
	          [this](HttpResult result) {
			const QJsonObject object = QJsonDocument::fromJson(result.body).object();
			const QString token = object.value("data").toObject().value("oauth_token").toString();
			if (result.status == 200 && object.value("success").toBool() && !token.isEmpty()) { finish_login(token, {}); return; }
			const QString reason = result.status > 0 ? QStringLiteral("HTTP %1").arg(result.status) : result.error;
			finish_login({}, QStringLiteral("Streamlabs login could not be completed (%1).").arg(reason));
		});
}

void StreamlabsClient::finish_login(const QString &token, const QString &error)
{
	TokenCallback callback = std::move(token_callback_); token_callback_ = {}; verifier_.clear();
	if (callback) callback(token, error);
}
