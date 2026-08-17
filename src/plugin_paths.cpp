// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "plugin_paths.hpp"

#include <windows.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVector>

namespace {

QString registry_string(HKEY root, const wchar_t *subkey, const wchar_t *value_name)
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS)
		return {};

	DWORD type = 0;
	DWORD size = 0;
	const LONG size_result = RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &size);
	if (size_result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
		RegCloseKey(key);
		return {};
	}

	QVector<wchar_t> buffer(static_cast<qsizetype>(size / sizeof(wchar_t)) + 1, L'\0');
	const LONG value_result = RegQueryValueExW(key, value_name, nullptr, &type,
		reinterpret_cast<LPBYTE>(buffer.data()), &size);
	RegCloseKey(key);
	return value_result == ERROR_SUCCESS ? QString::fromWCharArray(buffer.constData()) : QString{};
}

bool uses_legacy_global_plugin_layout()
{
	return QDir(module_installation_directory()).exists(QStringLiteral("data/locale"));
}

bool is_registered_obs_installation()
{
	const QString display_icon = registry_string(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\OBS Studio",
		L"DisplayIcon");
	if (display_icon.isEmpty())
		return false;

	QDir registered_directory(QFileInfo(display_icon).absolutePath());
	registered_directory.cdUp();
	registered_directory.cdUp();
	const QString registered_path = QFileInfo(registered_directory.absolutePath()).canonicalFilePath();
	const QString module_path = QFileInfo(module_installation_directory()).canonicalFilePath();
	return !registered_path.isEmpty() && !module_path.isEmpty()
		&& QString::compare(registered_path, module_path, Qt::CaseInsensitive) == 0;
}

bool should_migrate_previous_settings()
{
	return uses_legacy_global_plugin_layout() || is_registered_obs_installation();
}

QString scoped_settings_path(const QString &current_name, const QString &previous_name,
	const QString &legacy_name)
{
	const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
	QDir().mkpath(base);
	const QString current = QDir(base).filePath(current_name);
	if (!QFile::exists(current) && should_migrate_previous_settings()) {
		const QString previous = QDir(base).filePath(previous_name);
		const QString legacy = QDir(base).filePath(legacy_name);
		if (QFile::exists(previous))
			QFile::copy(previous, current);
		else if (QFile::exists(legacy))
			QFile::copy(legacy, current);
	}
	return current;
}

} // namespace

QString module_installation_directory()
{
	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&module_installation_directory), &module))
		return {};
	wchar_t path[MAX_PATH]{};
	if (!GetModuleFileNameW(module, path, MAX_PATH))
		return {};
	QDir directory(QFileInfo(QString::fromWCharArray(path)).absolutePath());
	directory.cdUp();
	directory.cdUp();
	return directory.absolutePath();
}

QString installation_storage_scope()
{
	QString path = QFileInfo(module_installation_directory()).canonicalFilePath();
	if (path.isEmpty())
		path = module_installation_directory();
	const QByteArray digest = QCryptographicHash::hash(path.toCaseFolded().toUtf8(),
		QCryptographicHash::Sha256).toHex();
	return QString::fromLatin1(digest.left(16));
}

QString profiles_settings_path()
{
	return scoped_settings_path(
		QStringLiteral("tiktok-live-obs-profiles-%1.ini").arg(installation_storage_scope()),
		QStringLiteral("tiktok-live-obs-profiles.ini"),
		QStringLiteral("tiktok-live-obs-bridge-profiles.ini"));
}

QString plugin_settings_path()
{
	return scoped_settings_path(
		QStringLiteral("tiktok-live-obs-%1.ini").arg(installation_storage_scope()),
		QStringLiteral("tiktok-live-obs.ini"),
		QStringLiteral("tiktok-live-obs-bridge.ini"));
}
