// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "frame_signing.hpp"

#include <QByteArray>
#include <QString>

struct TikTokRequestSignatureInput {
	qint64 timestamp_seconds = 0;
	QString aid = QStringLiteral("8311");
	QString device_id;
	QByteArray encoded_query;
	QByteArray body_stub;
};

struct TikTokRequestSignatureHeaders {
	QByteArray x_khronos;
	QByteArray x_ladon;
	QByteArray x_argus;
	QString error;

	[[nodiscard]] bool valid() const
	{
		return !x_khronos.isEmpty() && !x_ladon.isEmpty() && !x_argus.isEmpty() && error.isEmpty();
	}
};

class RapidApiRequestSigner final {
public:
	static TikTokRequestSignatureHeaders fetch(const FrameSignApiConfig &api,
		const TikTokRequestSignatureInput &input);
	static TikTokRequestSignatureHeaders parse_response(const QByteArray &body, long http_status);
};
