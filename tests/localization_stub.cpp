// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "localization.hpp"

QString text(const char *key)
{
	return QString::fromUtf8(key);
}

QString translated_or(const char * /*key*/, const QString &fallback)
{
	return fallback;
}
