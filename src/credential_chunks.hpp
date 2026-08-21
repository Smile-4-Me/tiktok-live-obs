// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include "secure_storage.hpp"

#include <QByteArray>

namespace CredentialChunks {

// Leave room below the strictest native-keyring limit. A 4,000-byte value is
// rejected by Windows Credential Manager, whose maximum is 2,560 bytes.
inline constexpr qsizetype chunk_bytes = 2048;
inline constexpr int maximum_chunks = 128;
static_assert(chunk_bytes <= SecureStorage::maximum_entry_bytes);

inline int count(qsizetype size)
{
	return size <= 0 ? 0 : static_cast<int>((size + chunk_bytes - 1) / chunk_bytes);
}

inline QByteArray at(const QByteArray &value, int index)
{
	return value.mid(static_cast<qsizetype>(index) * chunk_bytes, chunk_bytes);
}

} // namespace CredentialChunks
