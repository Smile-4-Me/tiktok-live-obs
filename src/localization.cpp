// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "localization.hpp"
#include "native_platform.hpp"
#include "plugin_paths.hpp"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSettings>

namespace {

QHash<QString, QString> translations;

QString module_locale_directory()
{
	const QString data_directory = module_data_directory();
	const QString native_locale_directory = QDir(data_directory).filePath(QStringLiteral("locale"));
	if (!data_directory.isEmpty() && QDir(native_locale_directory).exists())
		return native_locale_directory;

	const QDir directory(module_installation_directory());
	const QString standard_directory = directory.filePath(
		QStringLiteral("data/obs-plugins/tiktok-live-obs/locale"));
	if (QDir(standard_directory).exists())
		return standard_directory;

	// Older global installations stored locale files next to their own bin folder.
	return directory.filePath(QStringLiteral("data/locale"));
}

QString locale_file(const QDir &catalog_directory, const QString &locale)
{
	const QString exact_path = catalog_directory.filePath(locale + QStringLiteral(".ini"));
	if (QFileInfo::exists(exact_path))
		return exact_path;

	const QString language = locale.section('-', 0, 0);
	const QString language_path = catalog_directory.filePath(language + QStringLiteral(".ini"));
	if (QFileInfo::exists(language_path))
		return language_path;

	return catalog_directory.filePath(QStringLiteral("en-US.ini"));
}

void merge_catalog(const QDir &catalog_directory, const QString &locale)
{
	const QString english_path = catalog_directory.filePath(QStringLiteral("en-US.ini"));
	if (!QFileInfo::exists(english_path))
		return;

	QSettings english_settings(english_path, QSettings::IniFormat);
	// English is the complete schema for every catalog. It is loaded first and
	// the selected language only overlays translated values, so a newly added
	// key can never render as a raw internal identifier in a partial language
	// pack. The locale-catalog-contract test verifies that every literal UI key
	// used by the plugin is present in this English schema.
	for (const QString &key : english_settings.allKeys())
		translations.insert(key, english_settings.value(key).toString());

	const QString selected_path = locale_file(catalog_directory, locale);
	if (selected_path.compare(english_path, Qt::CaseInsensitive) == 0)
		return;

	QSettings selected_settings(selected_path, QSettings::IniFormat);
	for (const QString &key : selected_settings.allKeys())
		translations.insert(key, selected_settings.value(key).toString());
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
	const auto get_locale = reinterpret_cast<GetLocaleFunction>(
		resolve_native_symbol(NativeLibrary::Obs, "obs_get_locale"));
	const char *locale = get_locale ? get_locale() : nullptr;
	return locale ? QString::fromUtf8(locale) : QStringLiteral("en-US");
}

void load_translations()
{
	translations.clear();
	const QString locale = obs_language();
	const QDir locale_root(module_locale_directory());
	const QDir core_directory(locale_root.filePath(QStringLiteral("core")));

	// Pre-0.1.3 builds shipped one flat locale directory. Retain it as a
	// compatibility fallback so an in-place DLL update never renders raw keys.
	merge_catalog(core_directory.exists() ? core_directory : locale_root, locale);

	// Provider catalogs are discovered from disk instead of constructing the
	// provider registry during OBS module initialization. This keeps locale
	// startup independent from provider static initialization and lets packaged
	// providers be added or removed without touching the core language pack.
	const QDir providers_directory(locale_root.filePath(QStringLiteral("providers")));
	const QStringList provider_ids = providers_directory.entryList(
		QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
	for (const QString &provider_id : provider_ids) {
		const QDir provider_directory(providers_directory.filePath(provider_id));
		merge_catalog(provider_directory, locale);
	}
}
