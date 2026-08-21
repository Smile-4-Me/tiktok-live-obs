// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QMap>
#include <QString>
#include <QUrl>

#include <atomic>

struct FrameSignInput {
	QString aid = QStringLiteral("8311");
	QString uid;
	QString device_id;
	QString room_id;
	QString frame_type = QStringLiteral("2");
	qint64 timestamp_seconds = 0;
};

struct FrameSignResult {
	QString frame_type;
	QString license_id;
	QString sign_info;
	QString sign_value;
	QString sign_version;

	[[nodiscard]] bool valid() const;
	[[nodiscard]] QByteArray compact_json() const;
};

struct FrameSignApiConfig {
	QUrl base_url = QUrl(QStringLiteral("https://tiktok-live-studio-api-signer1.p.rapidapi.com/"));
	QString rapidapi_key;

	[[nodiscard]] bool valid() const;
};

struct FrameSignBatch {
	QMap<qint64, FrameSignResult> signatures;
	QString error;

	[[nodiscard]] bool valid() const { return !signatures.isEmpty() && error.isEmpty(); }
	[[nodiscard]] qint64 first_timestamp() const { return signatures.isEmpty() ? 0 : signatures.firstKey(); }
	[[nodiscard]] qint64 last_timestamp() const { return signatures.isEmpty() ? 0 : signatures.lastKey(); }
};

// HTTPS client for the external signing service. It intentionally contains no
// frame-signing algorithm or fallback; blank/rejected signatures fail closed.
class FrameSignClient final {
public:
	static FrameSignBatch fetch_batch(const FrameSignApiConfig &api, const FrameSignInput &input,
		qint64 start_timestamp_seconds, int duration_seconds = 300, int step_seconds = 1,
		const std::atomic_bool *cancelled = nullptr);
	static FrameSignBatch parse_batch_response(const QByteArray &body, long http_status,
		const FrameSignInput &input);
};
