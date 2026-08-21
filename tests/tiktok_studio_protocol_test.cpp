// SPDX-License-Identifier: GPL-3.0-only

#include "tiktok_request_signer.hpp"
#include "tiktok_studio_account.hpp"
#include "tiktok_studio_device.hpp"
#include "tiktok_studio_eligibility.hpp"
#include "tiktok_studio_game_tags.hpp"
#include "tiktok_studio_qr.hpp"
#include "tiktok_studio_session.hpp"
#include "tiktok_studio_topics.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

#include <iostream>

namespace {

bool expect(bool condition, const char *message)
{
	if (condition)
		return true;
	std::cerr << message << '\n';
	return false;
}

} // namespace

int main(int argc, char **argv)
{
	QCoreApplication application(argc, argv);
	bool ok = true;

	TikTokStudioAccountCredentials persisted_identity;
	persisted_identity.device_id = QStringLiteral("7000000000000000001");
	persisted_identity.install_id = QStringLiteral("7000000000000000002");
	TikTokStudioAccountCredentials refreshed_session;
	refreshed_session.device_id = QStringLiteral("8000000000000000001");
	refreshed_session.install_id = QStringLiteral("8000000000000000002");
	refreshed_session.cookie_jar = QByteArrayLiteral("updated-cookie-jar");
	preserve_tiktok_studio_device_identity(persisted_identity, &refreshed_session);
	ok &= expect(refreshed_session.device_id == persisted_identity.device_id &&
		refreshed_session.install_id == persisted_identity.install_id &&
		refreshed_session.cookie_jar == QByteArrayLiteral("updated-cookie-jar"),
		"Persisted device/install identity must survive API session refreshes.");

	// Fixture produced by the tracked public log_encrypt_codec.py implementation
	// at Unix time 1700000000. This guards the device-registration wire format.
	const QByteArray encrypted = encrypt_tiktok_device_registration_payload(
		QByteArrayLiteral("{\"hello\":\"world\"}"), 1700000000);
	const QByteArray expected = QByteArray::fromHex(
		"7463030b0003fbcc2b94447748e36092f9046c26b920e6aa2cf76f8f7165ae48a"
		"1aeb031d51e5a737894ceb12ee32908b1549a79c0b6");
	ok &= expect(encrypted == expected,
		"Device-registration encryption no longer matches the public protocol fixture.");
	ok &= expect(encrypted.startsWith(QByteArray::fromHex("746303")),
		"Device-registration envelope header is invalid.");
	ok &= expect(encrypt_tiktok_device_registration_payload(
		QByteArrayLiteral("{\"hello\":\"world\"}"), 1700000001) != encrypted,
		"Device-registration envelope must bind the generation time.");

	const QString bound_qr = bind_tiktok_qr_client_secret(
		QStringLiteral("https%3A%2F%2Fwww.tiktok.com%2Fqr%3Fnext_url%3Dhttps%253A%252F%252Fwww.tiktok.com%252Fcomplete"),
		QStringLiteral("ABC12345"));
	const QString decoded_qr = QUrl::fromPercentEncoding(bound_qr.toUtf8());
	ok &= expect(decoded_qr.contains(QStringLiteral("client_secret")) &&
		decoded_qr.contains(QStringLiteral("ABC12345")),
		"QR client-secret binding was not preserved in the login URL.");
	const QByteArray qr_png = render_tiktok_qr_png(bound_qr);
	ok &= expect(qr_png.startsWith(QByteArray("\x89PNG\r\n\x1a\n", 8)) && qr_png.size() > 100,
		"The native QR renderer did not produce a PNG image.");

	const TikTokRequestSignatureHeaders nested = RapidApiRequestSigner::parse_response(
		R"({"success":true,"headers":{"x-khronos":"1700000000","x-ladon":"ladon","x-argus":"argus"}})", 200);
	ok &= expect(nested.valid(), "A complete nested RapidAPI response should be accepted.");
	ok &= expect(nested.x_khronos == "1700000000" && nested.x_ladon == "ladon" &&
		nested.x_argus == "argus", "RapidAPI signature headers were parsed incorrectly.");

	const TikTokRequestSignatureHeaders flat = RapidApiRequestSigner::parse_response(
		R"({"X-KHRONOS":"1700000000","X-LADON":"ladon","X-ARGUS":"argus"})", 200);
	ok &= expect(flat.valid(), "A complete flat RapidAPI response should be accepted.");
	ok &= expect(!RapidApiRequestSigner::parse_response(
		R"({"success":false,"message":"rejected"})", 200).error.isEmpty(),
		"An explicitly rejected RapidAPI response must fail closed.");
	ok &= expect(!RapidApiRequestSigner::parse_response(
		R"({"success":true,"headers":{"x-khronos":"1"}})", 200).error.isEmpty(),
		"An incomplete RapidAPI response must fail closed.");
	ok &= expect(!RapidApiRequestSigner::parse_response("not json", 200).error.isEmpty(),
		"Invalid RapidAPI JSON must fail closed.");
	ok &= expect(!RapidApiRequestSigner::parse_response(
		R"({"message":"rate limited"})", 429).error.isEmpty(),
		"A non-success RapidAPI HTTP status must fail closed.");

	const TikTokStudioEligibility ready = parse_tiktok_studio_eligibility(
		{{QStringLiteral("golive_locale_restricted"), 0},
			{QStringLiteral("ban_status"), QJsonObject{{QStringLiteral("is_ban"), 0}}},
			{QStringLiteral("advanced_live_ban_status"),
				QJsonObject{{QStringLiteral("is_ban"), QStringLiteral("")}}},
			{QStringLiteral("block_status"), QStringLiteral("0")}},
		{{QStringLiteral("has_live_studio_login"), 1}});
	ok &= expect(ready.can_go_live && ready.status == QStringLiteral("Ready"),
		"Numeric TikTok eligibility flags should match Python truthiness.");

	const TikTokStudioEligibility restricted = parse_tiktok_studio_eligibility(
		{{QStringLiteral("golive_locale_restricted"), 1},
			{QStringLiteral("ban_status"), QJsonObject{{QStringLiteral("is_ban"), true}}},
			{QStringLiteral("advanced_live_ban_status"),
				QJsonObject{{QStringLiteral("is_ban"), false}}},
			{QStringLiteral("block_status"), QStringLiteral("2")}},
		{{QStringLiteral("has_live_studio_login"), true}});
	ok &= expect(!restricted.can_go_live &&
		restricted.status == QStringLiteral("Restricted / Banned / Blocked 2 / Locale restricted"),
		"Detailed TikTok restriction status no longer matches the public reference app.");

	const QVector<TikTokStudioGameTag> games = parse_tiktok_studio_game_tags({
		{QStringLiteral("data"), QJsonObject{{QStringLiteral("game_tag_list"), QJsonArray{
			QJsonObject{{QStringLiteral("id"), QStringLiteral("101")},
				{QStringLiteral("show_name"), QStringLiteral("Game One")}},
			QJsonObject{{QStringLiteral("id"), 202},
				{QStringLiteral("show_name"), QStringLiteral("Game Two")}},
			QJsonObject{{QStringLiteral("id"), QStringLiteral("303")}}
		}}}}});
	ok &= expect(games.size() == 2 && games.at(0).id == QStringLiteral("101") &&
		games.at(0).name == QStringLiteral("Game One") &&
		games.at(1).id == QStringLiteral("202"),
		"TikTok game names and IDs were not parsed from the public list response.");

	const QVector<TikTokStudioTopic> &topics = tiktok_studio_topics();
	ok &= expect(topics.size() == 9 && topics.at(0).id == QStringLiteral("5") &&
		topics.at(0).name == QStringLiteral("Gaming") &&
		topics.at(2).id == QStringLiteral("42") &&
		topics.at(8).id == QStringLiteral("45"),
		"TikTok LIVE topic names and IDs no longer match the public reference app.");
	ok &= expect(tiktok_studio_topic_is_gaming(QStringLiteral("5")) &&
		!tiktok_studio_topic_is_gaming(QStringLiteral("6")),
		"Gaming topic classification is incorrect.");

	const QByteArray cookie_jar =
		"# Netscape HTTP Cookie File\n"
		".tiktok.com\tTRUE\t/\tTRUE\t0\tsessionid\told\n"
		"#HttpOnly_.tiktokv.com\tTRUE\t/\tTRUE\t0\tuid_tt\tuser-cookie\n"
		"api16-normal-c-alisg.tiktokv.com\tFALSE\t/\tTRUE\t0\tsessionid\tcurrent\n";
	const QByteArray cookie_header = tiktok_session_cookie_header(cookie_jar);
	ok &= expect(cookie_header.contains("sessionid=current") && cookie_header.contains("uid_tt=user-cookie") &&
		!cookie_header.contains("sessionid=old"),
		"Saved TikTok cookies were not converted to domain-independent name/value pairs.");
	ok &= expect(is_tiktok_session_host(QStringLiteral("webcast16-normal-useast5.tiktokv.us")) &&
		is_tiktok_session_host(QStringLiteral("www.tiktok.com")) &&
		!is_tiktok_session_host(QStringLiteral("tiktok.com.example.org")) &&
		!is_tiktok_session_host(QStringLiteral("rapidapi.com")),
		"TikTok cookie forwarding did not enforce the trusted hostname boundary.");
	ok &= expect(tiktok_studio_session_is_stale_error(
		QStringLiteral("TikTok status 30003: room has finished")) &&
		tiktok_studio_session_is_stale_error(QStringLiteral("TikTok status 30003001: LIVE has ended")) &&
		tiktok_studio_session_is_stale_error(
			QStringLiteral("Recover TikTok LIVE failed: This LIVE has ended")) &&
		!tiktok_studio_session_is_stale_error(QStringLiteral("Please login first")),
		"TikTok stale LIVE responses were not classified safely.");
	ok &= expect(tiktok_studio_session_requires_login(QStringLiteral("Please login first")) &&
		!tiktok_studio_session_requires_login(QStringLiteral("Room has finished")),
		"TikTok authentication responses were not classified safely.");

	const QJsonObject nested_account{{QStringLiteral("user"),
		QJsonObject{{QStringLiteral("user_id_str"), QStringLiteral("7000000000000000001")}}}};
	ok &= expect(tiktok_studio_account_user_id(nested_account) == QStringLiteral("7000000000000000001"),
		"Nested TikTok account IDs were not recognized.");
	const QJsonObject nested_room{{QStringLiteral("room"), QJsonObject{
		{QStringLiteral("owner"), QJsonObject{{QStringLiteral("id_str"), QStringLiteral("7000000000000000002")}}}}}};
	ok &= expect(tiktok_studio_room_owner_id(nested_room) == QStringLiteral("7000000000000000002"),
		"Nested TikTok room owner IDs were not recognized.");

	return ok ? 0 : 1;
}
