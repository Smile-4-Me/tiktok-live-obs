// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "tiktok_request_signer.hpp"

#include <curl/curl.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace {

constexpr qsizetype response_limit = 1024 * 1024;

struct HttpResult {
	long status = 0;
	QByteArray body;
	QString error;
	bool overflow = false;
};

size_t append_response(char *data, size_t size, size_t count, void *user_data)
{
	auto *result = static_cast<HttpResult *>(user_data);
	const size_t bytes = size * count;
	if (bytes > static_cast<size_t>(response_limit) ||
		result->body.size() > response_limit - static_cast<qsizetype>(bytes)) {
		result->overflow = true;
		return 0;
	}
	result->body.append(data, static_cast<qsizetype>(bytes));
	return bytes;
}

QString response_message(const QJsonObject &object)
{
	for (const QString &key : {QStringLiteral("error"), QStringLiteral("message"),
		QStringLiteral("detail")}) {
		const QString value = object.value(key).toString().trimmed();
		if (!value.isEmpty())
			return value.left(500);
	}
	return {};
}

QByteArray header_value(const QJsonObject &object, const QString &name)
{
	QJsonValue value = object.value(name);
	if (value.isUndefined())
		value = object.value(name.toUpper());
	return value.toVariant().toString().trimmed().toUtf8();
}

} // namespace

TikTokRequestSignatureHeaders RapidApiRequestSigner::fetch(const FrameSignApiConfig &api,
	const TikTokRequestSignatureInput &input)
{
	TikTokRequestSignatureHeaders signatures;
	if (!api.valid()) {
		signatures.error = QStringLiteral("A valid RapidAPI signer key and HTTPS endpoint are required.");
		return signatures;
	}
	if (input.timestamp_seconds <= 0 || input.encoded_query.size() > 128 * 1024 ||
		input.body_stub.size() > 256) {
		signatures.error = QStringLiteral("The TikTok request-signing input is invalid.");
		return signatures;
	}

	QUrl endpoint = api.base_url;
	QString path = endpoint.path();
	if (!path.endsWith(QLatin1Char('/')))
		path += QLatin1Char('/');
	endpoint.setPath(path + QStringLiteral("signatures"));
	const QJsonObject payload{
		{QStringLiteral("timestamp"), input.timestamp_seconds},
		{QStringLiteral("aid"), input.aid},
		{QStringLiteral("device_id"), input.device_id},
		{QStringLiteral("license_id"), 1877999593},
		{QStringLiteral("params"), QString::fromUtf8(input.encoded_query)},
		{QStringLiteral("stub"), QString::fromLatin1(input.body_stub)},
	};
	const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

	HttpResult result;
	CURL *handle = curl_easy_init();
	if (!handle) {
		signatures.error = QStringLiteral("libcurl could not create a RapidAPI request.");
		return signatures;
	}
	curl_slist *headers = nullptr;
	const QByteArray host = endpoint.host().toUtf8();
	const QByteArray rapidapi_key = api.rapidapi_key.toUtf8();
	headers = curl_slist_append(headers, "Accept: application/json");
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, (QByteArray("X-RapidAPI-Key: ") + rapidapi_key).constData());
	headers = curl_slist_append(headers, (QByteArray("X-RapidAPI-Host: ") + host).constData());
	const QByteArray encoded_url = endpoint.toEncoded();
	curl_easy_setopt(handle, CURLOPT_URL, encoded_url.constData());
	curl_easy_setopt(handle, CURLOPT_POST, 1L);
	curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.constData());
	curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(handle, CURLOPT_USERAGENT, "TikTokLiveOBS/0.1");
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
	curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 20000L);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, append_response);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &result);
	const CURLcode code = curl_easy_perform(handle);
	if (code != CURLE_OK)
		result.error = result.overflow ? QStringLiteral("RapidAPI returned an oversized response.")
			: QString::fromLatin1(curl_easy_strerror(code));
	curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &result.status);
	curl_slist_free_all(headers);
	curl_easy_cleanup(handle);
	if (!result.error.isEmpty()) {
		signatures.error = QStringLiteral("The RapidAPI request signer could not be reached: %1")
			.arg(result.error);
		return signatures;
	}
	signatures = parse_response(result.body, result.status);
	if (!signatures.error.isEmpty())
		signatures.error.replace(api.rapidapi_key, QStringLiteral("<redacted>"), Qt::CaseSensitive);
	return signatures;
}

TikTokRequestSignatureHeaders RapidApiRequestSigner::parse_response(const QByteArray &body,
	long http_status)
{
	TikTokRequestSignatureHeaders signatures;
	const QJsonDocument document = QJsonDocument::fromJson(body);
	if (!document.isObject()) {
		signatures.error = QStringLiteral("RapidAPI returned an invalid request-signing response.");
		return signatures;
	}
	const QJsonObject root = document.object();
	const QJsonValue success = root.value(QStringLiteral("success"));
	if (http_status < 200 || http_status >= 300 ||
		(!success.isUndefined() && !success.toBool())) {
		const QString detail = response_message(root);
		signatures.error = detail.isEmpty()
			? QStringLiteral("RapidAPI rejected the request-signing request (HTTP %1).").arg(http_status)
			: QStringLiteral("RapidAPI rejected the request-signing request: %1").arg(detail);
		return signatures;
	}
	const QJsonObject headers = root.value(QStringLiteral("headers")).isObject()
		? root.value(QStringLiteral("headers")).toObject() : root;
	signatures.x_khronos = header_value(headers, QStringLiteral("x-khronos"));
	signatures.x_ladon = header_value(headers, QStringLiteral("x-ladon"));
	signatures.x_argus = header_value(headers, QStringLiteral("x-argus"));
	if (!signatures.valid())
		signatures.error = QStringLiteral("RapidAPI returned incomplete TikTok request signatures.");
	return signatures;
}
