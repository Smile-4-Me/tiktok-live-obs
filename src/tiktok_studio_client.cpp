// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "tiktok_studio_client.hpp"

#include "frame_signing.hpp"
#include "tiktok_request_signer.hpp"
#include "tiktok_studio_device.hpp"
#include "tiktok_studio_eligibility.hpp"
#include "tiktok_studio_game_tags.hpp"
#include "tiktok_studio_qr.hpp"
#include "tiktok_studio_session.hpp"
#include "tiktok_studio_topics.hpp"

#include <curl/curl.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMetaObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>


#include <algorithm>
#include <atomic>
#include <thread>

namespace {

using Params = QList<QPair<QString, QString>>;
using Headers = QList<QPair<QByteArray, QByteArray>>;

constexpr qsizetype http_response_limit = 16 * 1024 * 1024;
constexpr auto minimum_studio_version = "1.27.0";
constexpr auto passport_sdk_version = "2.1.9";
constexpr auto passport_app_key = "884c28a44b61b78f9d837fc8b0967178";
constexpr auto ticket_guard_public_key =
	"BHTyDu4GfY+Se8QoOlL22ARkOv4aLE5LBCk8eSiJ6K5N5m8Wg3cPeVYWVYNDrJ3hs"
	"RmaVCgNl/AKbMW67VBievw=";

struct HttpResult {
	long status = 0;
	QByteArray body;
	QString error;
	CURLcode curl_code = CURLE_OK;
	bool overflow = false;
};

size_t append_response(char *data, size_t size, size_t count, void *user_data)
{
	auto *result = static_cast<HttpResult *>(user_data);
	const size_t bytes = size * count;
	if (bytes > static_cast<size_t>(http_response_limit) ||
		result->body.size() > http_response_limit - static_cast<qsizetype>(bytes)) {
		result->overflow = true;
		return 0;
	}
	result->body.append(data, static_cast<qsizetype>(bytes));
	return bytes;
}

class CurlSession final {
public:
	explicit CurlSession(const QByteArray &cookie_jar = {}) : handle_(curl_easy_init())
	{
		if (!handle_)
			return;
		curl_easy_setopt(handle_, CURLOPT_COOKIEFILE, "");
		for (const QByteArray &line : cookie_jar.split('\n')) {
			if (!line.trimmed().isEmpty())
				curl_easy_setopt(handle_, CURLOPT_COOKIELIST, line.constData());
		}
	}

	~CurlSession()
	{
		if (handle_)
			curl_easy_cleanup(handle_);
	}

	CurlSession(const CurlSession &) = delete;
	CurlSession &operator=(const CurlSession &) = delete;

	[[nodiscard]] bool valid() const { return handle_ != nullptr; }

	HttpResult request(const QByteArray &method, const QByteArray &url, const Headers &headers,
		const QByteArray &body, const QByteArray &user_agent, long timeout_ms = 20000)
	{
		HttpResult result;
		if (!handle_) {
			result.error = QStringLiteral("libcurl could not create a TikTok request.");
			return result;
		}
		const QUrl parsed = QUrl::fromEncoded(url);
		if (parsed.scheme() != QStringLiteral("https") || parsed.host().isEmpty()) {
			result.error = QStringLiteral("TikTok returned an invalid HTTPS endpoint.");
			return result;
		}

		curl_slist *header_list = nullptr;
		bool has_cookie_header = false;
		for (const auto &[name, value] : headers) {
			has_cookie_header = has_cookie_header || name.compare("Cookie", Qt::CaseInsensitive) == 0;
			header_list = curl_slist_append(header_list, (name + ": " + value).constData());
		}
		if (!has_cookie_header && is_tiktok_session_host(parsed.host())) {
			const QByteArray cookie_header = tiktok_session_cookie_header(cookies());
			if (!cookie_header.isEmpty())
				header_list = curl_slist_append(header_list,
					(QByteArrayLiteral("Cookie: ") + cookie_header).constData());
		}
		curl_easy_setopt(handle_, CURLOPT_URL, url.constData());
		curl_easy_setopt(handle_, CURLOPT_HTTPHEADER, header_list);
		curl_easy_setopt(handle_, CURLOPT_USERAGENT, user_agent.constData());
		curl_easy_setopt(handle_, CURLOPT_FOLLOWLOCATION, 0L);
		curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYHOST, 2L);
		curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
		curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS, timeout_ms);
		curl_easy_setopt(handle_, CURLOPT_ACCEPT_ENCODING, "");
		curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, append_response);
		curl_easy_setopt(handle_, CURLOPT_WRITEDATA, &result);
		curl_easy_setopt(handle_, CURLOPT_NOSIGNAL, 1L);
		if (method == "POST") {
			curl_easy_setopt(handle_, CURLOPT_POST, 1L);
			curl_easy_setopt(handle_, CURLOPT_POSTFIELDS, body.isEmpty() ? "" : body.constData());
			curl_easy_setopt(handle_, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
		} else {
			curl_easy_setopt(handle_, CURLOPT_HTTPGET, 1L);
		}
		const CURLcode code = curl_easy_perform(handle_);
		result.curl_code = code;
		if (code != CURLE_OK)
			result.error = result.overflow ? QStringLiteral("TikTok returned an oversized response.")
				: QString::fromLatin1(curl_easy_strerror(code));
		curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &result.status);
		curl_slist_free_all(header_list);
		return result;
	}

	[[nodiscard]] QByteArray cookies() const
	{
		QByteArray result;
		if (!handle_)
			return result;
		curl_slist *cookies = nullptr;
		if (curl_easy_getinfo(handle_, CURLINFO_COOKIELIST, &cookies) != CURLE_OK)
			return result;
		for (curl_slist *item = cookies; item; item = item->next) {
			if (!item->data)
				continue;
			if (!result.isEmpty())
				result += '\n';
			result += item->data;
		}
		curl_slist_free_all(cookies);
		return result;
	}

private:
	CURL *handle_ = nullptr;
};

QByteArray encode_params(const Params &params)
{
	QUrlQuery query;
	for (const auto &[name, value] : params)
		query.addQueryItem(name, value);
	return query.query(QUrl::FullyEncoded).toUtf8();
}

QByteArray request_url(const QString &base, const QByteArray &query)
{
	QByteArray url = base.toUtf8();
	if (!query.isEmpty())
		url += (url.contains('?') ? '&' : '?') + query;
	return url;
}

QString random_text(int length, const QByteArray &alphabet)
{
	QString result;
	result.reserve(length);
	for (int index = 0; index < length; ++index)
		result += QLatin1Char(alphabet.at(QRandomGenerator::global()->bounded(alphabet.size())));
	return result;
}

QList<int> version_parts(const QString &version)
{
	QList<int> parts;
	for (const QString &part : version.split(QLatin1Char('.')))
		parts.push_back(part.toInt());
	while (parts.size() < 3)
		parts.push_back(0);
	return parts.mid(0, 3);
}

QString normalized_version(const QString &candidate)
{
	const QList<int> found = version_parts(candidate);
	const QList<int> minimum = version_parts(QString::fromLatin1(minimum_studio_version));
	return std::lexicographical_compare(found.begin(), found.end(), minimum.begin(), minimum.end())
		? QString::fromLatin1(minimum_studio_version) : candidate;
}

QString studio_browser_version(const QString &version)
{
	return QStringLiteral("5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
		"(KHTML, like Gecko) TikTokLIVEStudio/%1 Chrome/136.0.7103.59 "
		"Electron/36.4.0-alpha.17 TTElectron/36.4.0-alpha.17 Safari/537.36").arg(version);
}

QByteArray studio_user_agent(const QString &version)
{
	return QByteArray("Mozilla/") + studio_browser_version(version).toUtf8();
}

Headers common_headers(const QString &version)
{
	return {{"Accept", "application/json, text/plain, */*"},
		{"Accept-Language", "en-US"}, {"Sec-CH-UA", "\"Not.A/Brand\";v=\"99\", \"Chromium\";v=\"136\""},
		{"Sec-CH-UA-Mobile", "?0"}, {"Sec-CH-UA-Platform", "\"Windows\""},
		{"Sec-Fetch-Dest", "empty"}, {"Sec-Fetch-Mode", "cors"},
		{"Sec-Fetch-Site", "cross-site"}, {"Sec-Fetch-Storage-Access", "active"},
		{"X-SS-DP", ""}, {"sdk_aid", "8311"}};
}

QJsonObject response_object(const HttpResult &response, const QString &action, QString *error)
{
	QJsonParseError parse_error;
	const QJsonDocument document = QJsonDocument::fromJson(response.body, &parse_error);
	// TikTok occasionally closes a successful response before satisfying the
	// advertised Content-Length. libcurl reports CURLE_PARTIAL_FILE even when
	// the complete JSON object has already arrived; use it only when parsing
	// proves that the body is intact.
	const bool usable_partial_response = response.curl_code == CURLE_PARTIAL_FILE &&
		response.status >= 200 && response.status < 300 && parse_error.error == QJsonParseError::NoError &&
		document.isObject();
	if (!response.error.isEmpty() && !usable_partial_response) {
		*error = QStringLiteral("%1 failed: %2").arg(action, response.error);
		return {};
	}
	if (response.status < 200 || response.status >= 300) {
		QString detail;
		if (document.isObject()) {
			const QJsonObject object = document.object();
			detail = object.value(QStringLiteral("message")).toString();
			if (detail.isEmpty())
				detail = object.value(QStringLiteral("error")).toString();
		}
		*error = detail.isEmpty() ? QStringLiteral("%1 failed (HTTP %2).").arg(action).arg(response.status)
			: QStringLiteral("%1 failed: %2").arg(action, detail.left(500));
		return {};
	}
	if (!document.isObject()) {
		*error = QStringLiteral("%1 returned invalid JSON.").arg(action);
		return {};
	}
	return document.object();
}

QString webcast_error(const QJsonObject &payload, const QString &action)
{
	const QString status = payload.value(QStringLiteral("status_code")).toVariant().toString();
	if (status.isEmpty() || status == QStringLiteral("0") || status == QStringLiteral("4003150"))
		return {};
	const QJsonObject data = payload.value(QStringLiteral("data")).toObject();
	QString message = data.value(QStringLiteral("prompts")).toString();
	if (message.isEmpty())
		message = data.value(QStringLiteral("message")).toString();
	if (message.isEmpty())
		message = payload.value(QStringLiteral("prompts")).toString();
	if (message.isEmpty())
		message = payload.value(QStringLiteral("message")).toString();
	return message.isEmpty() ? QStringLiteral("%1 failed (TikTok status %2).").arg(action, status)
		: QStringLiteral("%1 failed: %2 (TikTok status %3).").arg(action, message, status);
}

QByteArray cookie_value(const QByteArray &jar, const QByteArray &name)
{
	for (const QByteArray &raw_line : jar.split('\n')) {
		const QByteArray line = raw_line.trimmed();
		if (line.isEmpty() || (line.startsWith('#') && !line.startsWith("#HttpOnly_")))
			continue;
		const QList<QByteArray> fields = line.split('\t');
		if (fields.size() >= 7 && fields.at(5) == name)
			return fields.at(6);
	}
	return {};
}

QString idc_segment(const QByteArray &cookie_jar)
{
	QString value = QString::fromUtf8(cookie_value(cookie_jar, "store-idc")).trimmed().toLower();
	if (value == QStringLiteral("alisg"))
		value = QStringLiteral("c-alisg");
	return value;
}

QString idc_suffix(const QString &segment, const QByteArray &cookie_jar)
{
	if (segment == QStringLiteral("c-alisg"))
		return QStringLiteral("tiktokv.com");
	if (segment.startsWith(QStringLiteral("useast")))
		return QStringLiteral("tiktokv.us");
	if (segment.startsWith(QStringLiteral("no")))
		return QStringLiteral("tiktokv.eu");
	const QString target = QString::fromUtf8(cookie_value(cookie_jar, "tt-target-idc")).toLower();
	const QString country = QString::fromUtf8(cookie_value(cookie_jar, "store-country-code")).toLower();
	const QStringList eu{QStringLiteral("at"), QStringLiteral("be"), QStringLiteral("bg"), QStringLiteral("ch"),
		QStringLiteral("cy"), QStringLiteral("cz"), QStringLiteral("de"), QStringLiteral("dk"),
		QStringLiteral("ee"), QStringLiteral("es"), QStringLiteral("fi"), QStringLiteral("fr"),
		QStringLiteral("gb"), QStringLiteral("gr"), QStringLiteral("hr"), QStringLiteral("hu"),
		QStringLiteral("ie"), QStringLiteral("is"), QStringLiteral("it"), QStringLiteral("li"),
		QStringLiteral("lt"), QStringLiteral("lu"), QStringLiteral("lv"), QStringLiteral("mt"),
		QStringLiteral("nl"), QStringLiteral("no"), QStringLiteral("pl"), QStringLiteral("pt"),
		QStringLiteral("ro"), QStringLiteral("se"), QStringLiteral("si"), QStringLiteral("sk")};
	return target.startsWith(QStringLiteral("eu")) || eu.contains(country)
		? QStringLiteral("tiktokv.eu") : QStringLiteral("tiktokv.com");
}

QStringList api_hosts(const QByteArray &cookie_jar)
{
	QStringList hosts;
	const QString segment = idc_segment(cookie_jar);
	if (!segment.isEmpty())
		hosts << QStringLiteral("api16-normal-%1.%2").arg(segment, idc_suffix(segment, cookie_jar));
	hosts << QStringLiteral("api16-normal-c-alisg.tiktokv.com")
		<< QStringLiteral("api16-normal-no1a.tiktokv.eu")
		<< QStringLiteral("api16-normal-useast8.tiktokv.us")
		<< QStringLiteral("api16-normal-useast5.tiktokv.us");
	hosts.removeDuplicates();
	return hosts;
}

QStringList webcast_hosts(const QByteArray &cookie_jar)
{
	QStringList hosts;
	const QString segment = idc_segment(cookie_jar);
	if (!segment.isEmpty()) {
		hosts << QStringLiteral("webcast16-normal-%1.%2").arg(segment, idc_suffix(segment, cookie_jar));
		const QString target = QString::fromUtf8(cookie_value(cookie_jar, "tt-target-idc")).toLower();
		const QString country = QString::fromUtf8(cookie_value(cookie_jar, "store-country-code")).toLower();
		const QStringList eu{QStringLiteral("at"), QStringLiteral("be"), QStringLiteral("bg"),
			QStringLiteral("ch"), QStringLiteral("cy"), QStringLiteral("cz"), QStringLiteral("de"),
			QStringLiteral("dk"), QStringLiteral("ee"), QStringLiteral("es"), QStringLiteral("fi"),
			QStringLiteral("fr"), QStringLiteral("gb"), QStringLiteral("gr"), QStringLiteral("hr"),
			QStringLiteral("hu"), QStringLiteral("ie"), QStringLiteral("is"), QStringLiteral("it"),
			QStringLiteral("li"), QStringLiteral("lt"), QStringLiteral("lu"), QStringLiteral("lv"),
			QStringLiteral("mt"), QStringLiteral("nl"), QStringLiteral("no"), QStringLiteral("pl"),
			QStringLiteral("pt"), QStringLiteral("ro"), QStringLiteral("se"), QStringLiteral("si"),
			QStringLiteral("sk")};
		if (target.startsWith(QStringLiteral("eu")) || eu.contains(country))
			hosts << QStringLiteral("webcast16-normal-no1a.tiktokv.eu");
		else if (country == QStringLiteral("us") || target.startsWith(QStringLiteral("useast")))
			hosts << QStringLiteral("webcast16-normal-useast5.tiktokv.us");
	} else {
		hosts << QStringLiteral("webcast-normal.tiktokv.com")
			<< QStringLiteral("webcast.tiktokv.com")
			<< QStringLiteral("webcast16-normal.tiktokv.com");
	}
	hosts.removeDuplicates();
	return hosts;
}

Params passport_signed(Params params, const Params &body = {})
{
	QStringList keys;
	for (const auto &[key, value] : params) {
		if (!value.isNull() && key != QStringLiteral("sign") && key != QStringLiteral("qs") &&
			key != QStringLiteral("isResend") && key != QStringLiteral("next") &&
			key != QStringLiteral("baseURL") && key != QStringLiteral("extra_params"))
			keys.push_back(key);
	}
	keys.removeDuplicates();
	std::sort(keys.begin(), keys.end());
	if (keys.size() > 10)
		keys = keys.mid(0, 10);
	auto value_for = [](const Params &items, const QString &key) {
		for (const auto &item : items)
			if (item.first == key)
				return item.second;
		return QString{};
	};
	QStringList param_parts;
	for (const QString &key : keys)
		param_parts << key + QLatin1Char('=') + value_for(params, key);
	QStringList body_keys;
	for (const auto &[key, value] : body)
		if (!value.isNull())
			body_keys << key;
	body_keys.removeDuplicates();
	std::sort(body_keys.begin(), body_keys.end());
	QStringList body_parts;
	for (const QString &key : body_keys)
		body_parts << key + QLatin1Char('=') + value_for(body, key);
	const QByteArray base = (param_parts.join(QLatin1Char('&')) + QLatin1Char('&') +
		body_parts.join(QLatin1Char('&')) + QStringLiteral("&app_key=") +
		QString::fromLatin1(passport_app_key)).toUtf8();
	const QString sign = QString::fromLatin1(QCryptographicHash::hash(base, QCryptographicHash::Md5).toHex());
	const QByteArray joined = keys.join(QLatin1Char(',')).toUtf8();
	QByteArray qs;
	qs.reserve(joined.size() * 2);
	for (const char byte : joined)
		qs += QByteArray::number(static_cast<uint8_t>(byte) ^ 5, 16).rightJustified(2, '0');
	params.push_back({QStringLiteral("sign"), sign});
	params.push_back({QStringLiteral("qs"), QString::fromLatin1(qs)});
	return params;
}

Params studio_params(const TikTokStudioAccountCredentials &account)
{
	QString sdk = account.live_studio_version;
	sdk.remove(QLatin1Char('.'));
	const QString country = QString::fromUtf8(cookie_value(account.cookie_jar, "store-country-code")).toLower();
	const QString locale = QLocale::system().name().replace(QLatin1Char('_'), QLatin1Char('-'));
	const QString language = QLocale::system().name().section(QLatin1Char('_'), 0, 0).toLower();
	const QString timezone = QString::fromUtf8(QTimeZone::systemTimeZoneId());
	return {{QStringLiteral("aid"), QStringLiteral("8311")},
		{QStringLiteral("app_name"), QStringLiteral("tiktok_live_studio")},
		{QStringLiteral("device_id"), account.device_id}, {QStringLiteral("install_id"), account.install_id},
		{QStringLiteral("channel"), QStringLiteral("studio")},
		{QStringLiteral("version_code"), account.live_studio_version},
		{QStringLiteral("device_platform"), QStringLiteral("windows")},
		{QStringLiteral("timezone_name"), timezone},
		{QStringLiteral("screen_width"), QStringLiteral("1920")},
		{QStringLiteral("screen_height"), QStringLiteral("1080")},
		{QStringLiteral("browser_language"), locale},
		{QStringLiteral("browser_platform"), QStringLiteral("Win32")},
		{QStringLiteral("browser_name"), QStringLiteral("Mozilla")},
		{QStringLiteral("browser_version"), studio_browser_version(account.live_studio_version)},
		{QStringLiteral("language"), language},
		{QStringLiteral("app_language"), language},
		{QStringLiteral("webcast_language"), language},
		{QStringLiteral("priority_region"), country},
		{QStringLiteral("webcast_sdk_version"), sdk}, {QStringLiteral("live_mode"), QStringLiteral("6")}};
}

struct SignedRequestResult {
	QJsonObject object;
	QString error;
};

struct GameTagsSyncResult {
	QVector<TikTokStudioGameTag> tags;
	TikTokStudioAccountCredentials account;
	QString error;
};

GameTagsSyncResult fetch_game_tags_sync(TikTokStudioAccountCredentials account)
{
	GameTagsSyncResult result;
	result.account = account;
	CurlSession session(account.cookie_jar);
	QStringList hosts{QStringLiteral("webcast-normal.tiktokv.com")};
	hosts += webcast_hosts(account.cookie_jar);
	hosts.removeDuplicates();
	for (const QString &host : hosts) {
		Headers headers = common_headers(account.live_studio_version);
		headers.push_back({"Pragma", "no-cache"});
		headers.push_back({"Cache-Control", "no-cache"});
		const HttpResult response = session.request("GET",
			QStringLiteral("https://%1/webcast/room/hashtag/list/").arg(host).toUtf8(),
			headers, {}, studio_user_agent(account.live_studio_version), 15000);
		const QByteArray updated_cookies = session.cookies();
		if (!updated_cookies.isEmpty())
			account.cookie_jar = updated_cookies;
		result.account = account;
		QString request_error;
		const QJsonObject payload = response_object(response,
			QStringLiteral("Load TikTok games"), &request_error);
		if (!request_error.isEmpty()) {
			result.error = request_error;
			continue;
		}
		const QString response_error = webcast_error(payload, QStringLiteral("Load TikTok games"));
		if (!response_error.isEmpty()) {
			result.error = response_error;
			continue;
		}
		result.tags = parse_tiktok_studio_game_tags(payload);
		if (!result.tags.isEmpty()) {
			result.error.clear();
			return result;
		}
		result.error = QStringLiteral("TikTok returned an empty game list.");
	}
	if (result.error.isEmpty())
		result.error = QStringLiteral("TikTok games could not be loaded.");
	return result;
}

SignedRequestResult signed_request(CurlSession &session, const TikTokStudioAccountCredentials &account,
	const QByteArray &method, const QString &url, const Params &params, const QByteArray &body = {},
	const QByteArray &content_type = "application/x-www-form-urlencoded; charset=UTF-8",
	const Headers &extra_headers = {}, long timeout_ms = 20000)
{
	SignedRequestResult result;
	const QByteArray query = encode_params(params);
	TikTokRequestSignatureInput input;
	input.timestamp_seconds = QDateTime::currentSecsSinceEpoch();
	input.device_id = account.device_id;
	input.encoded_query = query;
	if (!body.isEmpty())
		input.body_stub = QCryptographicHash::hash(body, QCryptographicHash::Md5).toHex();
	FrameSignApiConfig api;
	api.base_url = QUrl(account.signer_api_url);
	api.rapidapi_key = account.rapidapi_key;
	const TikTokRequestSignatureHeaders signatures = RapidApiRequestSigner::fetch(api, input);
	if (!signatures.valid()) {
		result.error = signatures.error;
		return result;
	}
	Headers headers = common_headers(account.live_studio_version);
	headers += extra_headers;
	headers.push_back({"Content-Type", content_type});
	headers.push_back({"Pragma", "no-cache"});
	headers.push_back({"Cache-Control", "no-cache"});
	headers.push_back({"X-Khronos", signatures.x_khronos});
	headers.push_back({"X-Ladon", signatures.x_ladon});
	headers.push_back({"X-Argus", signatures.x_argus});
	if (!input.body_stub.isEmpty())
		headers.push_back({"X-SS-Stub", input.body_stub});
	const QString region = QString::fromUtf8(cookie_value(account.cookie_jar, "store-country-code")).toLower();
	if (!region.isEmpty())
		headers.push_back({"X-TT-Store-Region", region.toUtf8()});
	const HttpResult response = session.request(method, request_url(url, query), headers, body,
		studio_user_agent(account.live_studio_version), timeout_ms);
	result.object = response_object(response, QStringLiteral("TikTok request"), &result.error);
	return result;
}

QString fetch_studio_version(CurlSession &session)
{
	const Params params{{QStringLiteral("pid"), QStringLiteral("7393277106664249610")},
		{QStringLiteral("uid"), QStringLiteral("0")},
		{QStringLiteral("branch"), QStringLiteral("studio/release/stable")},
		{QStringLiteral("buildId"), QStringLiteral("0")}};
	const Headers headers{{"Accept", "application/json"}};
	const HttpResult response = session.request("GET", request_url(
		QStringLiteral("https://tron-sg.bytelemon.com/api/sdk/check_update"), encode_params(params)),
		headers, {}, studio_user_agent(QString::fromLatin1(minimum_studio_version)), 15000);
	QString error;
	const QJsonObject root = response_object(response, QStringLiteral("LIVE Studio version check"), &error);
	const QString version = root.value(QStringLiteral("data")).toObject()
		.value(QStringLiteral("manifest")).toObject().value(QStringLiteral("win32")).toObject()
		.value(QStringLiteral("version")).toString();
	return version.isEmpty() ? QString::fromLatin1(minimum_studio_version) : normalized_version(version);
}

QString register_device(CurlSession &session, TikTokStudioAccountCredentials *account)
{
	account->live_studio_version = fetch_studio_version(session);
	const QByteArray serial_alphabet("abcdefghijklmnopqrstuvwxyz0123456789");
	const QString pc_serial = random_text(20, serial_alphabet);
	const QString pc_uuid = QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('-') +
		random_text(16, serial_alphabet);
	QByteArray mac;
	for (int index = 0; index < 6; ++index) {
		int octet = QRandomGenerator::global()->bounded(256);
		if (index == 0)
			octet = (octet | 0x02) & 0xfe;
		if (!mac.isEmpty())
			mac += ':';
		mac += QByteArray::number(octet, 16).rightJustified(2, '0');
	}
	const QStringList models{QStringLiteral("z790 taichi lite"), QStringLiteral("rog strix b760-f"),
		QStringLiteral("b550 aorus elite"), QStringLiteral("prime z690-p"), QStringLiteral("tomahawk x670e")};
	const QStringList os_versions{QStringLiteral("10.0.19045"), QStringLiteral("10.0.22621"),
		QStringLiteral("10.0.22631"), QStringLiteral("10.0.26100"), QStringLiteral("10.0.26200")};
	const QStringList resolutions{QStringLiteral("1920x1080"), QStringLiteral("2560x1440"),
		QStringLiteral("1600x900"), QStringLiteral("1366x768")};
	const QString os_version = os_versions.at(QRandomGenerator::global()->bounded(os_versions.size()));
	const QString model = models.at(QRandomGenerator::global()->bounded(models.size()));
	const QString resolution = resolutions.at(QRandomGenerator::global()->bounded(resolutions.size()));
	const QString width = resolution.section(QLatin1Char('x'), 0, 0);
	const QString height = resolution.section(QLatin1Char('x'), 1, 1);
	const QString locale = QLocale::system().name().replace(QLatin1Char('_'), QLatin1Char('-'));
	const QString language = QLocale::system().name().section(QLatin1Char('_'), 0, 0).toLower();
	const QString timezone = QString::fromUtf8(QTimeZone::systemTimeZoneId());
	const int timezone_offset = QDateTime::currentDateTime().offsetFromUtc();
	const int offset_minutes = qAbs(timezone_offset) / 60;
	const QString timezone_gmt = QStringLiteral("GMT%1%2%3")
		.arg(timezone_offset < 0 ? QLatin1Char('-') : QLatin1Char('+'))
		.arg(offset_minutes / 60, 2, 10, QLatin1Char('0'))
		.arg(offset_minutes % 60, 2, 10, QLatin1Char('0'));
	const QJsonObject header{{QStringLiteral("device_id"), 0}, {QStringLiteral("install_id"), 0},
		{QStringLiteral("os"), QStringLiteral("Windows")}, {QStringLiteral("device_platform"), QStringLiteral("PC")},
		{QStringLiteral("sdk_version"), QStringLiteral("1.0.2")}, {QStringLiteral("aid"), QStringLiteral("8311")},
		{QStringLiteral("mc"), QString::fromLatin1(mac)}, {QStringLiteral("channel"), QStringLiteral("studio")},
		{QStringLiteral("package"), QStringLiteral("tiktok_live_studio")},
		{QStringLiteral("language"), locale},
		{QStringLiteral("app_version"), account->live_studio_version},
		{QStringLiteral("os_version"), os_version}, {QStringLiteral("device_model"), model},
		{QStringLiteral("time_zone"), timezone_gmt},
		{QStringLiteral("tz_name"), timezone},
		{QStringLiteral("tz_offset"), timezone_offset}, {QStringLiteral("resolution"), resolution},
		{QStringLiteral("app_region"), QString()}, {QStringLiteral("app_language"), QString()},
		{QStringLiteral("display_name"), QStringLiteral("tiktok_live_studio")},
		{QStringLiteral("pc_uuid"), pc_uuid}, {QStringLiteral("pc_serial"), pc_serial}};
	const QJsonObject payload{{QStringLiteral("header"), header}, {QStringLiteral("_gen_time"), 0},
		{QStringLiteral("magic_tag"), QStringLiteral("ss_app_log")}};
	const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
	const QByteArray encrypted = encrypt_tiktok_device_registration_payload(json, QDateTime::currentSecsSinceEpoch());
	if (encrypted.isEmpty())
		return QStringLiteral("The TikTok device-registration payload could not be prepared.");
	const QString browser = studio_browser_version(account->live_studio_version);
	Params params{{QStringLiteral("aid"), QStringLiteral("8311")}, {QStringLiteral("channel"), QStringLiteral("studio")},
		{QStringLiteral("os"), QStringLiteral("Windows")}, {QStringLiteral("os_version"), os_version},
		{QStringLiteral("device_type"), QStringLiteral("PC")}, {QStringLiteral("device_platform"), QStringLiteral("PC")},
		{QStringLiteral("version_code"), account->live_studio_version}, {QStringLiteral("pc_uuid"), pc_uuid},
		{QStringLiteral("pc_serial"), pc_serial}, {QStringLiteral("aid"), QStringLiteral("8311")},
		{QStringLiteral("app_name"), QStringLiteral("tiktok_live_studio")},
		{QStringLiteral("device_id"), QStringLiteral("0")}, {QStringLiteral("install_id"), QStringLiteral("0")},
		{QStringLiteral("channel"), QStringLiteral("studio")},
		{QStringLiteral("version_code"), account->live_studio_version},
		{QStringLiteral("device_platform"), QStringLiteral("windows")},
		{QStringLiteral("timezone_name"), timezone},
		{QStringLiteral("screen_width"), width}, {QStringLiteral("screen_height"), height},
		{QStringLiteral("browser_language"), locale},
		{QStringLiteral("browser_platform"), QStringLiteral("Win32")},
		{QStringLiteral("browser_name"), QStringLiteral("Mozilla")}, {QStringLiteral("browser_version"), browser},
		{QStringLiteral("language"), language}, {QStringLiteral("app_language"), language},
		{QStringLiteral("webcast_language"), language},
		{QStringLiteral("webcast_sdk_version"), QString(account->live_studio_version).remove(QLatin1Char('.'))},
		{QStringLiteral("live_mode"), QStringLiteral("6")}};
	const Headers headers{{"Accept", "application/json, text/plain, */*"}, {"Content-Type", "application/json"},
		{"User-Agent", "TTNetwork PC"}, {"X-SS-DP", ""}, {"sdk_aid", "8311"},
		{"X-SS-Stub", QCryptographicHash::hash(encrypted, QCryptographicHash::Md5).toHex()}};
	const HttpResult response = session.request("POST", request_url(
		QStringLiteral("https://log.tiktokv.com/service/2/desktop/device_register/"), encode_params(params)),
		headers, encrypted, "TTNetwork PC", 25000);
	const QByteArray updated_cookies = session.cookies();
	if (!updated_cookies.isEmpty())
		account->cookie_jar = updated_cookies;
	QString error;
	const QJsonObject root = response_object(response, QStringLiteral("TikTok device registration"), &error);
	if (!error.isEmpty())
		return error;
	const QJsonObject source = root.value(QStringLiteral("data")).isObject()
		? root.value(QStringLiteral("data")).toObject() : root;
	account->device_id = source.value(QStringLiteral("device_id")).toVariant().toString().trimmed();
	account->install_id = source.value(QStringLiteral("install_id")).toVariant().toString().trimmed();
	if (!account->has_device())
		return QStringLiteral("TikTok device registration did not return device and install identifiers.");
	account->cookie_jar = session.cookies();
	return {};
}

Headers passport_headers()
{
	return {{"Accept", "application/json, text/javascript"},
		{"Referer", "https://www.tiktok.com/ucenter_web/live_studio/login"},
		{"TT-Ticket-Guard-Public-Key", ticket_guard_public_key}, {"TT-Ticket-Guard-Version", "2"},
		{"TT-Ticket-Guard-Web-Version", "1"}, {"TT-Ticket-Guard-Iteration-Version", "0"}};
}

struct BeginLoginResult {
	TikTokStudioQrCode code;
	QString token;
	QString host;
	QString client_secret;
	QString error;
};

BeginLoginResult begin_login_sync(TikTokStudioAccountCredentials account)
{
	BeginLoginResult result;
	result.code.account = account;
	CurlSession session(account.cookie_jar);
	if (!session.valid()) {
		result.error = QStringLiteral("libcurl could not initialize TikTok login.");
		return result;
	}
	if (!account.has_device()) {
		result.error = register_device(session, &account);
		result.code.account = account;
		if (!result.error.isEmpty())
			return result;
	}
	const QString verify_fp = QStringLiteral("verify_") + account.device_id;
	const Params params = passport_signed({{QStringLiteral("next"), QStringLiteral("https://www.tiktok.com")},
		{QStringLiteral("device_id"), account.device_id}, {QStringLiteral("aid"), QStringLiteral("8311")},
		{QStringLiteral("account_sdk_source"), QStringLiteral("web")},
		{QStringLiteral("sdk_version"), QString::fromLatin1(passport_sdk_version)},
		{QStringLiteral("verifyFp"), verify_fp}});
	QString last_error;
	for (const QString &host : api_hosts(account.cookie_jar)) {
		const SignedRequestResult response = signed_request(session, account, "GET",
			QStringLiteral("https://%1/passport/web/get_qrcode/").arg(host), params, {},
			"application/x-www-form-urlencoded", passport_headers());
		if (!response.error.isEmpty()) {
			last_error = response.error;
			continue;
		}
		const QJsonObject data = response.object.value(QStringLiteral("data")).toObject();
		const QString token = data.value(QStringLiteral("token")).toString();
		const QString index_url = data.value(QStringLiteral("qrcode_index_url")).toString();
		QByteArray encoded = data.value(QStringLiteral("qrcode")).toString().toLatin1();
		const int comma = encoded.indexOf(',');
		if (encoded.startsWith("data:") && comma >= 0)
			encoded = encoded.mid(comma + 1);
		const QString client_secret = random_text(8, QByteArrayLiteral("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
		const QString patched_url = bind_tiktok_qr_client_secret(index_url, client_secret);
		QByteArray png = patched_url.isEmpty() ? QByteArray{} : render_tiktok_qr_png(patched_url);
		const bool client_secret_bound = !png.isEmpty();
		if (!client_secret_bound)
			png = QByteArray::fromBase64(encoded);
		if (token.isEmpty() || png.isEmpty()) {
			last_error = QStringLiteral("TikTok did not return a usable QR code.");
			continue;
		}
		account.cookie_jar = session.cookies();
		result.code = {png, account};
		result.token = token;
		result.host = host;
		result.client_secret = client_secret_bound ? client_secret : QString{};
		return result;
	}
	const QByteArray updated_cookies = session.cookies();
	if (!updated_cookies.isEmpty())
		account.cookie_jar = updated_cookies;
	result.code.account = account;
	result.error = last_error.isEmpty() ? QStringLiteral("TikTok QR login could not be started.") : last_error;
	return result;
}

TikTokStudioAccountInfo account_info_sync(TikTokStudioAccountCredentials account, QString *error)
{
	TikTokStudioAccountInfo result;
	result.account = account;
	CurlSession session(account.cookie_jar);
	const QString verify_fp = QStringLiteral("verify_") + account.device_id;
	const Params passport_params = passport_signed({{QStringLiteral("device_id"), account.device_id},
		{QStringLiteral("aid"), QStringLiteral("8311")},
		{QStringLiteral("account_sdk_source"), QStringLiteral("web")},
		{QStringLiteral("sdk_version"), QString::fromLatin1(passport_sdk_version)},
		{QStringLiteral("verifyFp"), verify_fp}});
	QJsonObject account_data;
	QString last_error;
	for (const QString &host : api_hosts(account.cookie_jar)) {
		const SignedRequestResult response = signed_request(session, account, "GET",
			QStringLiteral("https://%1/passport/account/info/v2/").arg(host), passport_params, {},
			"application/x-www-form-urlencoded", passport_headers());
		if (!response.error.isEmpty()) {
			last_error = response.error;
			continue;
		}
		account_data = response.object.value(QStringLiteral("data")).toObject();
		if (!account_data.isEmpty())
			break;
	}
	if (account_data.isEmpty()) {
		const QByteArray updated_cookies = session.cookies();
		if (!updated_cookies.isEmpty())
			account.cookie_jar = updated_cookies;
		result.account = account;
		*error = last_error.isEmpty() ? QStringLiteral("TikTok did not return account information.") : last_error;
		return result;
	}
	account.username = account_data.value(QStringLiteral("username")).toString();
	if (account.username.isEmpty())
		account.username = account_data.value(QStringLiteral("screen_name")).toString();
	if (account.username.isEmpty())
		account.username = account_data.value(QStringLiteral("name")).toString();
	const QString returned_user_id = tiktok_studio_account_user_id(account_data);
	if (!returned_user_id.isEmpty())
		account.user_id = returned_user_id;
	if (account.username.isEmpty())
		account.username = QStringLiteral("TikTok account");
	account.cookie_jar = session.cookies();
	result.account = account;
	result.can_go_live = false;
	result.application_status = QStringLiteral("Connected");

	Params create_params = studio_params(account);
	create_params.push_back({QStringLiteral("last_time_hashtag_id"), QStringLiteral("5")});
	create_params.push_back({QStringLiteral("live_studio"), QStringLiteral("1")});
	Params game_params = studio_params(account);
	game_params.push_back({QStringLiteral("scene"), QStringLiteral("2")});
	bool eligibility_checked = false;
	for (const QString &host : webcast_hosts(account.cookie_jar)) {
		const SignedRequestResult create = signed_request(session, account, "GET",
			QStringLiteral("https://%1/webcast/room/create_info/").arg(host), create_params);
		if (!create.error.isEmpty())
			continue;
		const SignedRequestResult game = signed_request(session, account, "GET",
			QStringLiteral("https://%1/webcast/game/basic/create_info/").arg(host), game_params);
		if (!game.error.isEmpty())
			continue;
		const QString create_error = webcast_error(create.object,
			QStringLiteral("TikTok LIVE access check"));
		const QString game_error = webcast_error(game.object,
			QStringLiteral("TikTok LIVE Studio access check"));
		if (!create_error.isEmpty() || !game_error.isEmpty())
			continue;
		const QJsonObject create_data = create.object.value(QStringLiteral("data")).toObject();
		const QJsonObject game_data = game.object.value(QStringLiteral("data")).toObject();
		const TikTokStudioEligibility eligibility =
			parse_tiktok_studio_eligibility(create_data, game_data);
		result.can_go_live = eligibility.can_go_live;
		result.application_status = eligibility.status;
		eligibility_checked = true;
		break;
	}
	if (!eligibility_checked)
		result.application_status = QStringLiteral("Connected — LIVE access not verified");
	result.account.cookie_jar = session.cookies();
	return result;
}

struct PollLoginResult {
	TikTokStudioQrPoll poll;
	QString error;
};

PollLoginResult poll_login_sync(TikTokStudioAccountCredentials account, const QString &token,
	const QString &host, const QString &expected_client_secret)
{
	PollLoginResult result;
	CurlSession session(account.cookie_jar);
	const QString verify_fp = QStringLiteral("verify_") + account.device_id;
	const Params params = passport_signed({{QStringLiteral("next"), QStringLiteral("https://www.tiktok.com")},
		{QStringLiteral("token"), token}, {QStringLiteral("multi_login"), QStringLiteral("1")},
		{QStringLiteral("device_id"), account.device_id}, {QStringLiteral("aid"), QStringLiteral("8311")},
		{QStringLiteral("account_sdk_source"), QStringLiteral("web")},
		{QStringLiteral("sdk_version"), QString::fromLatin1(passport_sdk_version)},
		{QStringLiteral("verifyFp"), verify_fp}});
	QStringList hosts{host};
	hosts += api_hosts(account.cookie_jar);
	hosts.removeAll(QString{});
	hosts.removeDuplicates();
	QJsonObject data;
	QString last_error;
	int best_rank = -1;
	for (const QString &candidate : hosts) {
		const SignedRequestResult response = signed_request(session, account, "GET",
			QStringLiteral("https://%1/passport/web/check_qrconnect/").arg(candidate), params, {},
			"application/x-www-form-urlencoded", passport_headers());
		if (!response.error.isEmpty()) {
			last_error = response.error;
			continue;
		}
		const QJsonObject candidate_data = response.object.value(QStringLiteral("data")).toObject();
		QString candidate_status = candidate_data.value(QStringLiteral("status")).toVariant().toString().toLower();
		if (candidate_status.isEmpty())
			candidate_status = candidate_data.value(QStringLiteral("qr_status")).toVariant().toString().toLower();
		if (candidate_status.isEmpty())
			candidate_status = candidate_data.value(QStringLiteral("state")).toVariant().toString().toLower();
		const QString returned_secret = candidate_data.value(QStringLiteral("client_secret")).toString();
		const bool secret_matches = !expected_client_secret.isEmpty() && returned_secret == expected_client_secret;
		if (candidate_status == QStringLiteral("confirmed") &&
			(expected_client_secret.isEmpty() || secret_matches)) {
			data = candidate_data;
			break;
		}
		const int rank = candidate_status == QStringLiteral("scanned")
			? (secret_matches ? 4 : 3)
			: (candidate_status == QStringLiteral("confirmed")
				? (expected_client_secret.isEmpty() ? 5 : 2)
				: (candidate_status == QStringLiteral("new") ? 1 : 0));
		if (rank > best_rank) {
			best_rank = rank;
			data = candidate_data;
		}
	}
	if (data.isEmpty() && !last_error.isEmpty()) {
		const QByteArray updated_cookies = session.cookies();
		if (!updated_cookies.isEmpty())
			account.cookie_jar = updated_cookies;
		result.poll.account = account;
		result.error = last_error;
		return result;
	}
	QString status = data.value(QStringLiteral("status")).toVariant().toString().toLower();
	if (status.isEmpty())
		status = data.value(QStringLiteral("qr_status")).toVariant().toString().toLower();
	if (status.isEmpty())
		status = data.value(QStringLiteral("state")).toVariant().toString().toLower();
	account.cookie_jar = session.cookies();
	result.poll.account = account;
	if (status == QStringLiteral("scanned")) {
		result.poll.state = TikTokStudioQrState::Scanned;
		return result;
	}
	if (status == QStringLiteral("expired") || status == QStringLiteral("timeout")) {
		result.poll.state = TikTokStudioQrState::Expired;
		return result;
	}
	if (status != QStringLiteral("confirmed")) {
		result.poll.state = TikTokStudioQrState::Waiting;
		return result;
	}
	if (!expected_client_secret.isEmpty() &&
		data.value(QStringLiteral("client_secret")).toString() != expected_client_secret) {
		result.poll.state = TikTokStudioQrState::Waiting;
		return result;
	}
	const QString confirmed_user_id = tiktok_studio_account_user_id(data);
	if (!confirmed_user_id.isEmpty())
		account.user_id = confirmed_user_id;
	QString account_error;
	const TikTokStudioAccountInfo info = account_info_sync(account, &account_error);
	result.poll.state = TikTokStudioQrState::Confirmed;
	// account_info_sync exports its response cookie jar even on a payload error.
	result.poll.account = info.account.has_device() ? info.account : account;
	result.poll.can_go_live = account_error.isEmpty() && info.can_go_live;
	result.poll.application_status = account_error.isEmpty()
		? info.application_status : QStringLiteral("Connected — LIVE access not verified");
	if (result.poll.account.username.isEmpty())
		result.poll.account.username = QStringLiteral("TikTok account");
	return result;
}

struct LiveSyncResult {
	TikTokStudioLive live;
	QString error;
};

QString split_stream_url(const QString &push_url, QString *server, QString *key)
{
	const int slash = push_url.lastIndexOf(QLatin1Char('/'));
	if (slash <= push_url.indexOf(QStringLiteral("://")) + 2 || slash >= push_url.size() - 1)
		return QStringLiteral("TikTok returned an invalid RTMP push URL.");
	*server = push_url.left(slash);
	*key = push_url.mid(slash + 1);
	return {};
}

QString room_id_from(const QJsonObject &room)
{
	const QJsonObject living = room.value(QStringLiteral("living_room_attrs")).toObject();
	QString value = living.value(QStringLiteral("room_id_str")).toString();
	if (value.isEmpty()) value = living.value(QStringLiteral("room_id")).toVariant().toString();
	if (value.isEmpty()) value = room.value(QStringLiteral("id_str")).toString();
	if (value.isEmpty()) value = room.value(QStringLiteral("id")).toVariant().toString();
	return value;
}

QString stream_id_from(const QJsonObject &room)
{
	QString value = room.value(QStringLiteral("stream_id_str")).toString();
	if (value.isEmpty()) value = room.value(QStringLiteral("stream_id")).toVariant().toString();
	return value;
}

QString heartbeat_sync(CurlSession &session, TikTokStudioAccountCredentials *account,
	const QString &room_id, const QString &stream_id, int status, bool *room_is_living = nullptr)
{
	if (room_is_living)
		*room_is_living = false;
	Params params = studio_params(*account);
	const Params data{{QStringLiteral("status"), QString::number(status)},
		{QStringLiteral("room_id"), room_id}, {QStringLiteral("stream_id"), stream_id}};
	const QByteArray body = encode_params(data);
	QString last_error;
	for (const QString &host : webcast_hosts(account->cookie_jar)) {
		const SignedRequestResult response = signed_request(session, *account, "POST",
			QStringLiteral("https://%1/webcast/room/ping/anchor/").arg(host), params, body,
			"application/x-www-form-urlencoded; charset=UTF-8", {}, 2500);
		const QByteArray updated_cookies = session.cookies();
		if (!updated_cookies.isEmpty())
			account->cookie_jar = updated_cookies;
		if (!response.error.isEmpty()) {
			last_error = response.error;
			continue;
		}
		const QString error = webcast_error(response.object, QStringLiteral("TikTok LIVE heartbeat"));
		if (!error.isEmpty()) {
			last_error = error;
			// The first host is the authenticated IDC selected by the saved
			// session. Do not let an unrelated fallback replace a definitive
			// stale-room or authentication response.
			if (tiktok_studio_session_is_stale_error(error) ||
				tiktok_studio_session_requires_login(error))
				return error;
			continue;
		}
		if (room_is_living)
			*room_is_living = response.object.value(QStringLiteral("status_code")).toVariant().toString()
				== QStringLiteral("4003150");
		return {};
	}
	return last_error.isEmpty() ? QStringLiteral("TikTok LIVE heartbeat failed.") : last_error;
}

void pre_finish_sync(CurlSession &session, TikTokStudioAccountCredentials *account,
	const QString &room_id)
{
	const QStringList hosts = webcast_hosts(account->cookie_jar);
	if (hosts.isEmpty())
		return;
	Params params = studio_params(*account);
	params.push_back({QStringLiteral("room_id"), room_id});
	// LIVE Studio sends this advisory request before the two finish pings.
	// Some accounts reject it even though the finish request succeeds.
	signed_request(session, *account, "GET",
		QStringLiteral("https://%1/webcast/room/anchor_pre_finish/").arg(hosts.front()),
		params, {}, "application/x-www-form-urlencoded; charset=UTF-8", {}, 2500);
	const QByteArray updated_cookies = session.cookies();
	if (!updated_cookies.isEmpty())
		account->cookie_jar = updated_cookies;
}

struct ContinuableRoomResult {
	QJsonObject room;
	QString host;
	QString error;
};

void update_session_cookies(CurlSession &session, TikTokStudioAccountCredentials *account)
{
	const QByteArray cookies = session.cookies();
	if (!cookies.isEmpty())
		account->cookie_jar = cookies;
}

ContinuableRoomResult continuable_room_sync(CurlSession &session,
	TikTokStudioAccountCredentials *account)
{
	ContinuableRoomResult result;
	const Params params = studio_params(*account);
	bool confirmed_no_active_room = false;
	for (const QString &host : webcast_hosts(account->cookie_jar)) {
		const SignedRequestResult response = signed_request(session, *account, "GET",
			QStringLiteral("https://%1/webcast/room/continue/").arg(host), params);
		update_session_cookies(session, account);
		if (!response.error.isEmpty()) {
			if (!confirmed_no_active_room)
				result.error = response.error;
			continue;
		}
		const QString response_error = webcast_error(response.object,
			QStringLiteral("Recover TikTok LIVE"));
		if (!response_error.isEmpty()) {
			// /continue/ uses the normal stale-room response when there is no
			// resumable LIVE. That is a successful negative lookup, not a reason
			// to block creation of the next room. Keep checking fallback IDCs in
			// case another region still owns an active continuation.
			if (tiktok_studio_session_is_stale_error(response_error)) {
				confirmed_no_active_room = true;
				result.error.clear();
				continue;
			}
			if (tiktok_studio_session_requires_login(response_error) &&
				!confirmed_no_active_room) {
				result.error = response_error;
				return result;
			}
			if (!confirmed_no_active_room)
				result.error = response_error;
			continue;
		}
		const QJsonObject data = response.object.value(QStringLiteral("data")).toObject();
		const QJsonObject room = data.value(QStringLiteral("room")).toObject();
		if (!room.isEmpty()) {
			result.room = room;
			result.host = host;
			result.error.clear();
			return result;
		}
		confirmed_no_active_room = true;
		result.error.clear();
	}
	if (confirmed_no_active_room)
		result.error.clear();
	return result;
}

LiveSyncResult prepare_live_room_sync(CurlSession &session, TikTokStudioAccountCredentials account,
	const QJsonObject &room_payload, const QString &primary_host, bool send_prepare_heartbeat = true)
{
	LiveSyncResult result;
	const QJsonObject room = tiktok_studio_room_object(room_payload);
	const QJsonObject stream_url = room.value(QStringLiteral("stream_url")).toObject();
	update_session_cookies(session, &account);
	result.live.account = account;
	result.live.room_id = room_id_from(room);
	result.live.stream_id = stream_id_from(room);
	result.live.owner_user_id = tiktok_studio_room_owner_id(room);
	result.error = split_stream_url(stream_url.value(QStringLiteral("rtmp_push_url")).toString(),
		&result.live.server, &result.live.key);
	if (!result.error.isEmpty() || result.live.room_id.isEmpty() || result.live.stream_id.isEmpty()) {
		if (result.error.isEmpty())
			result.error = QStringLiteral("TikTok did not return room and stream identifiers.");
		return result;
	}
	if (result.live.owner_user_id.isEmpty())
		result.live.owner_user_id = account.user_id;
	if (result.live.owner_user_id.isEmpty()) {
		Params room_params = studio_params(account);
		room_params.push_back({QStringLiteral("room_id"), result.live.room_id});
		QStringList room_hosts{primary_host};
		room_hosts += webcast_hosts(account.cookie_jar);
		room_hosts.removeAll(QString{});
		room_hosts.removeDuplicates();
		for (const QString &room_host : room_hosts) {
			const SignedRequestResult room_info = signed_request(session, account, "GET",
				QStringLiteral("https://%1/webcast/room/info/").arg(room_host), room_params);
			update_session_cookies(session, &account);
			if (!room_info.error.isEmpty() ||
				!webcast_error(room_info.object, QStringLiteral("TikTok room info")).isEmpty())
				continue;
			result.live.owner_user_id = tiktok_studio_room_owner_id(
				room_info.object.value(QStringLiteral("data")).toObject());
			if (!result.live.owner_user_id.isEmpty())
				break;
		}
	}
	if (result.live.owner_user_id.isEmpty()) {
		result.error = QStringLiteral("TikTok did not return the account/anchor ID required for frame signing.");
		result.live.account = account;
		return result;
	}
	account.user_id = result.live.owner_user_id;
	if (send_prepare_heartbeat)
		result.error = heartbeat_sync(session, &account, result.live.room_id, result.live.stream_id, 1);
	result.live.account = account;
	return result;
}

LiveSyncResult continuable_live_sync(TikTokStudioAccountCredentials account, bool resume)
{
	LiveSyncResult result;
	CurlSession session(account.cookie_jar);
	ContinuableRoomResult existing = continuable_room_sync(session, &account);
	if (existing.room.isEmpty()) {
		result.live.account = account;
		result.error = existing.error;
		if (resume && result.error.isEmpty())
			result.error = QStringLiteral("TikTok did not return a continuable LIVE session.");
		return result;
	}
	return prepare_live_room_sync(session, account, existing.room, existing.host, resume);
}

LiveSyncResult start_live_sync(TikTokStudioAccountCredentials account, const QString &title,
	const QString &hashtag_id, const QString &game_tag_id, bool mature)
{
	LiveSyncResult result;
	CurlSession session(account.cookie_jar);
	ContinuableRoomResult existing = continuable_room_sync(session, &account);
	if (!existing.room.isEmpty())
		return prepare_live_room_sync(session, account, existing.room, existing.host);
	if (tiktok_studio_session_requires_login(existing.error)) {
		result.error = existing.error;
		result.live.account = account;
		return result;
	}

	const Params params = studio_params(account);
	const Params data{{QStringLiteral("title"), title.trimmed().isEmpty() ? QStringLiteral("Gameplay") : title.trimmed()},
		{QStringLiteral("live_studio"), QStringLiteral("1")}, {QStringLiteral("gen_replay"), QStringLiteral("true")},
		{QStringLiteral("chat_auth"), QStringLiteral("1")},
		{QStringLiteral("age_restricted"), mature ? QStringLiteral("4") : QStringLiteral("0")},
		{QStringLiteral("cover_uri"), QString()}, {QStringLiteral("close_room_when_close_stream"), QStringLiteral("false")},
		{QStringLiteral("hashtag_id"), hashtag_id.trimmed()},
		{QStringLiteral("game_tag_id"), game_tag_id.trimmed().isEmpty() ? QStringLiteral("0") : game_tag_id.trimmed()},
		{QStringLiteral("game_bitrate_type"), QStringLiteral("high")},
		{QStringLiteral("screenshot_cover_status"), QStringLiteral("1")},
		{QStringLiteral("multi_stream_scene"), QStringLiteral("0")}, {QStringLiteral("gift_auth"), QStringLiteral("1")},
		{QStringLiteral("chat_l2"), QStringLiteral("1")}, {QStringLiteral("star_comment_switch"), QStringLiteral("true")},
		{QStringLiteral("multi_stream_source"), QStringLiteral("1")},
		{QStringLiteral("is_group_live_session"), QStringLiteral("false")},
		{QStringLiteral("open_commercial_content_toggle"), QStringLiteral("false")},
		{QStringLiteral("commercial_content_promote_myself"), QStringLiteral("false")},
		{QStringLiteral("commercial_content_promote_third_party"), QStringLiteral("false")},
		{QStringLiteral("rtc_net_enabled"), QStringLiteral("false")}};
	const QByteArray body = encode_params(data);
	QString last_error;
	for (const QString &host : webcast_hosts(account.cookie_jar)) {
		const SignedRequestResult response = signed_request(session, account, "POST",
			QStringLiteral("https://%1/webcast/room/create/").arg(host), params, body);
		update_session_cookies(session, &account);
		QString response_error = response.error;
		if (response_error.isEmpty())
			response_error = webcast_error(response.object, QStringLiteral("Create TikTok LIVE"));
		if (response_error.isEmpty()) {
			const QJsonObject room = tiktok_studio_room_object(
				response.object.value(QStringLiteral("data")).toObject());
			return prepare_live_room_sync(session, account, room, host);
		}

		last_error = response_error;
		// A POST can reach TikTok even when its response is truncated or the
		// connection closes. Recover the resulting room before another create.
		ContinuableRoomResult recovered = continuable_room_sync(session, &account);
		if (!recovered.room.isEmpty())
			return prepare_live_room_sync(session, account, recovered.room, recovered.host);
		if (tiktok_studio_session_requires_login(response_error) ||
			tiktok_studio_session_requires_login(recovered.error))
			break;
	}
	result.live.account = account;
	result.error = last_error.isEmpty() ? QStringLiteral("TikTok LIVE could not be created.") : last_error;
	return result;
}

template<typename Work, typename Done>
void run_async(TikTokStudioClient *owner, Work work, Done done)
{
	QPointer<TikTokStudioClient> guard(owner);
	std::thread([guard, work = std::move(work), done = std::move(done)]() mutable {
		auto value = work();
		if (!guard)
			return;
		QMetaObject::invokeMethod(guard, [guard, done = std::move(done), value = std::move(value)]() mutable {
			if (guard)
				done(std::move(value));
		}, Qt::QueuedConnection);
	}).detach();
}

} // namespace

struct TikTokStudioClient::LoginState {
	TikTokStudioAccountCredentials account;
	QString token;
	QString host;
	QString client_secret;
	quint64 generation = 0;
	std::atomic_bool polling = false;
};

TikTokStudioClient::TikTokStudioClient(QObject *parent) : QObject(parent)
{
	static const CURLcode initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
	(void)initialized;
}

TikTokStudioClient::~TikTokStudioClient()
{
	shutting_down_ = true;
	cancel_qr_login();
}

void TikTokStudioClient::begin_qr_login(TikTokStudioAccountCredentials account, QrCallback completion)
{
	if (account.rapidapi_key.trimmed().isEmpty()) {
		completion({}, QStringLiteral("Enter a RapidAPI key before starting TikTok login."));
		return;
	}
	quint64 generation = 0;
	{
		std::lock_guard lock(login_mutex_);
		generation = ++login_generation_;
		login_.reset();
	}
	run_async(this, [account = std::move(account)]() mutable { return begin_login_sync(std::move(account)); },
		[this, generation, completion = std::move(completion)](BeginLoginResult result) mutable {
			if (shutting_down_)
				return;
			{
				std::lock_guard lock(login_mutex_);
				if (generation != login_generation_)
					return;
				if (result.error.isEmpty()) {
					login_ = std::make_shared<LoginState>();
					login_->account = result.code.account;
					login_->token = result.token;
					login_->host = result.host;
					login_->client_secret = result.client_secret;
					login_->generation = generation;
				}
			}
			completion(std::move(result.code), std::move(result.error));
		});
}

void TikTokStudioClient::poll_qr_login(QrPollCallback completion)
{
	std::shared_ptr<LoginState> state;
	{
		std::lock_guard lock(login_mutex_);
		state = login_;
	}
	if (!state) {
		completion({}, QStringLiteral("No TikTok QR login is active."));
		return;
	}
	if (state->polling.exchange(true))
		return;
	run_async(this, [account = state->account, token = state->token, host = state->host,
		client_secret = state->client_secret] {
		return poll_login_sync(account, token, host, client_secret);
	}, [this, state, completion = std::move(completion)](PollLoginResult result) mutable {
		state->polling = false;
		{
			std::lock_guard lock(login_mutex_);
			if (login_ != state || state->generation != login_generation_)
				return;
			if (result.poll.account.has_device())
				state->account = result.poll.account;
		}
		completion(std::move(result.poll), std::move(result.error));
	});
}

void TikTokStudioClient::cancel_qr_login()
{
	std::lock_guard lock(login_mutex_);
	++login_generation_;
	login_.reset();
}

void TikTokStudioClient::verify_account(TikTokStudioAccountCredentials account, AccountCallback completion)
{
	if (!account.has_login()) {
		completion({}, QStringLiteral("No saved TikTok LIVE Studio login is available."));
		return;
	}
	run_async(this, [account = std::move(account)]() mutable {
		QString error;
		TikTokStudioAccountInfo info = account_info_sync(std::move(account), &error);
		return qMakePair(std::move(info), std::move(error));
	}, [completion = std::move(completion)](QPair<TikTokStudioAccountInfo, QString> result) mutable {
		completion(std::move(result.first), std::move(result.second));
	});
}

void TikTokStudioClient::fetch_game_tags(TikTokStudioAccountCredentials account,
	GameTagsCallback completion)
{
	if (!account.has_login()) {
		completion({}, std::move(account),
			QStringLiteral("No saved TikTok LIVE Studio login is available."));
		return;
	}
	QVector<TikTokStudioGameTag> cached;
	{
		std::lock_guard lock(game_tags_mutex_);
		if (!game_tags_cache_.isEmpty())
			cached = game_tags_cache_;
		else {
			game_tags_callbacks_.push_back({account, std::move(completion)});
			if (game_tags_loading_)
				return;
			game_tags_loading_ = true;
		}
	}
	if (!cached.isEmpty()) {
		completion(std::move(cached), std::move(account), {});
		return;
	}
	run_async(this, [account = std::move(account)] {
		return fetch_game_tags_sync(account);
	}, [this](GameTagsSyncResult result) mutable {
		QVector<PendingGameTagsCallback> callbacks;
		{
			std::lock_guard lock(game_tags_mutex_);
			if (result.error.isEmpty())
				game_tags_cache_ = result.tags;
			game_tags_loading_ = false;
			callbacks.swap(game_tags_callbacks_);
		}
		for (int index = 0; index < callbacks.size(); ++index) {
			TikTokStudioAccountCredentials callback_account = index == 0
				? result.account : std::move(callbacks[index].account);
			callbacks[index].completion(result.tags, std::move(callback_account), result.error);
		}
	});
}

void TikTokStudioClient::find_continuable_live(TikTokStudioAccountCredentials account,
	LiveCallback completion)
{
	if (!account.has_login()) {
		completion({}, QStringLiteral("No saved TikTok LIVE Studio login is available."));
		return;
	}
	run_async(this, [account = std::move(account)]() mutable {
		return continuable_live_sync(std::move(account), false);
	}, [completion = std::move(completion)](LiveSyncResult result) mutable {
		completion(std::move(result.live), std::move(result.error));
	});
}

void TikTokStudioClient::resume_live(TikTokStudioAccountCredentials account,
	LiveCallback completion)
{
	if (!account.has_login()) {
		completion({}, QStringLiteral("No saved TikTok LIVE Studio login is available."));
		return;
	}
	run_async(this, [account = std::move(account)]() mutable {
		return continuable_live_sync(std::move(account), true);
	}, [completion = std::move(completion)](LiveSyncResult result) mutable {
		completion(std::move(result.live), std::move(result.error));
	});
}

void TikTokStudioClient::start_live(TikTokStudioAccountCredentials account, const QString &title,
	const QString &hashtag_id, const QString &game_tag_id, bool mature, LiveCallback completion)
{
	if (!account.has_login()) {
		completion({}, QStringLiteral("Sign in to TikTok LIVE Studio before creating a LIVE session."));
		return;
	}
	if (hashtag_id.trimmed().isEmpty()) {
		completion({}, QStringLiteral("Select a TikTok LIVE topic before creating a LIVE session."));
		return;
	}
	if (tiktok_studio_topic_is_gaming(hashtag_id) && game_tag_id.trimmed().isEmpty()) {
		completion({}, QStringLiteral("Select a game for the Gaming topic before creating a LIVE session."));
		return;
	}
	run_async(this, [account = std::move(account), title, hashtag_id, game_tag_id, mature]() mutable {
		return start_live_sync(std::move(account), title, hashtag_id, game_tag_id, mature);
	}, [completion = std::move(completion)](LiveSyncResult result) mutable {
		completion(std::move(result.live), std::move(result.error));
	});
}

void TikTokStudioClient::heartbeat(TikTokStudioAccountCredentials account, const QString &room_id,
	const QString &stream_id, int status, HeartbeatCallback completion)
{
	run_async(this, [account = std::move(account), room_id, stream_id, status]() mutable {
		TikTokStudioHeartbeatResult result;
		CurlSession session(account.cookie_jar);
		result.error = heartbeat_sync(session, &account, room_id, stream_id, status,
			&result.room_is_living);
		result.account = std::move(account);
		return result;
	}, [completion = std::move(completion)](TikTokStudioHeartbeatResult result) mutable {
		completion(std::move(result));
	});
}

void TikTokStudioClient::end_live(TikTokStudioAccountCredentials account, const QString &room_id,
	const QString &stream_id, EndCallback completion)
{
	TikTokStudioEndResult initial;
	initial.account = account;
	if (!account.has_login() || room_id.isEmpty() || stream_id.isEmpty()) {
		initial.error = QStringLiteral("The saved TikTok LIVE Studio session is incomplete.");
		completion(std::move(initial));
		return;
	}
	run_async(this, [account = std::move(account), room_id, stream_id]() mutable {
		TikTokStudioEndResult result;
		result.account = account;
		CurlSession session(account.cookie_jar);
		pre_finish_sync(session, &account, room_id);
		const QString first_error = heartbeat_sync(session, &account, room_id, stream_id, 4);
		const QString second_error = heartbeat_sync(session, &account, room_id, stream_id, 4);
		QString error;
		if (!first_error.isEmpty() && !second_error.isEmpty()) {
			error = tiktok_studio_session_is_stale_error(first_error) ? first_error : second_error;
			if (tiktok_studio_session_is_stale_error(second_error))
				error = second_error;
		}
		result.account = account;
		result.ended = error.isEmpty();
		result.stale_session = tiktok_studio_session_is_stale_error(error);
		result.error = result.stale_session ? QString{} : error;
		return result;
	}, [completion = std::move(completion)](TikTokStudioEndResult result) mutable {
		completion(std::move(result));
	});
}
