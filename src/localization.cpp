// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "localization.hpp"
#include "plugin_paths.hpp"

#include <windows.h>

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSettings>

namespace {

QHash<QString, QString> translations;

QString module_locale_directory()
{
	const QDir directory(module_installation_directory());
	const QString standard_directory = directory.filePath(
		QStringLiteral("data/obs-plugins/tiktok-live-obs/locale"));
	if (QDir(standard_directory).exists())
		return standard_directory;

	// Older global installations stored locale files next to their own bin folder.
	return directory.filePath(QStringLiteral("data/locale"));
}

} // namespace

QString text(const char *key)
{
	return translations.value(QString::fromUtf8(key), QString::fromUtf8(key));
}

QString translated_or(const char *key, const QString &fallback)
{
	const QString value = text(key);
	return value == QString::fromUtf8(key) ? fallback : value;
}

QString obs_language()
{
	using GetLocaleFunction = const char *(*)();
	const HMODULE libobs = GetModuleHandleW(L"obs.dll");
	const auto get_locale = libobs ? reinterpret_cast<GetLocaleFunction>(
		GetProcAddress(libobs, "obs_get_locale")) : nullptr;
	const char *locale = get_locale ? get_locale() : nullptr;
	return locale ? QString::fromUtf8(locale) : QStringLiteral("en-US");
}

void load_translations()
{
	translations.clear();
	const QDir locale_directory(module_locale_directory());
	QSettings english_settings(locale_directory.filePath(QStringLiteral("en-US.ini")),
		QSettings::IniFormat);
	for (const QString &key : english_settings.allKeys())
		translations.insert(key, english_settings.value(key).toString());

	QString locale = obs_language();
	QString locale_path = locale_directory.filePath(locale + QStringLiteral(".ini"));
	if (!QFileInfo::exists(locale_path)) {
		const QString language = locale.section('-', 0, 0);
		const QString language_path = locale_directory.filePath(language + QStringLiteral(".ini"));
		locale_path = QFileInfo::exists(language_path)
			? language_path
			: locale_directory.filePath(QStringLiteral("en-US.ini"));
	}

	if (locale_path.endsWith(QStringLiteral("en-US.ini"), Qt::CaseInsensitive))
		return;

	QSettings settings(locale_path, QSettings::IniFormat);
	for (const QString &key : settings.allKeys())
		translations.insert(key, settings.value(key).toString());
}
