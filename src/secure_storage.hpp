// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QByteArray>
#include <QString>

namespace SecureStorage {

// Windows Credential Manager is the strictest supported backend. Keep the
// shared contract at its documented CRED_MAX_CREDENTIAL_BLOB_SIZE so callers
// behave identically on Windows, macOS, and Linux.
inline constexpr qsizetype maximum_entry_bytes = 5 * 512;

bool save(const QString &target, const QByteArray &value, const QString &label,
	QString *error = nullptr);
QByteArray load(const QString &target, QString *error = nullptr);
bool remove(const QString &target, QString *error = nullptr);

} // namespace SecureStorage
