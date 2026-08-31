// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QByteArray>
#include <QString>

#include <optional>

// Platform adapter seam for an explicitly user-initiated browser-session
// handoff. This module deliberately contains no browser-profile discovery,
// decryption, or process interaction. A product-specific collector may be
// supplied later; the dock owns consent, validation, and secure persistence.
struct TikTokBrowserSessionImport {
	QByteArray netscape_cookie_jar;
	QString source_label;
};

[[nodiscard]] std::optional<TikTokBrowserSessionImport>
collect_tiktok_browser_session(QString *error);
