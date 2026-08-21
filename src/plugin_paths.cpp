// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "plugin_paths.hpp"
#include "native_platform.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <obs-module.h>

namespace {

QString scoped_settings_path(const QString &name)
{
	const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
	QDir().mkpath(base);
	return QDir(base).filePath(name);
}

} // namespace

QString module_installation_directory()
{
	const QString module_file = current_module_file(
		reinterpret_cast<const void *>(&module_installation_directory));
	QDir directory(QFileInfo(module_file).absolutePath());
#ifdef _WIN32
	directory.cdUp();
	directory.cdUp();
#endif
	return directory.absolutePath();
}

QString module_data_directory()
{
	using GetModuleDataPathFunction = const char *(*)(obs_module_t *);
	const auto get_data_path = reinterpret_cast<GetModuleDataPathFunction>(
		resolve_native_symbol(NativeLibrary::Obs, "obs_get_module_data_path"));
	const char *path = get_data_path ? get_data_path(obs_current_module()) : nullptr;
	return path ? QString::fromUtf8(path) : QString{};
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
	return scoped_settings_path(QStringLiteral("tiktok-live-obs-profiles-%1.ini")
		.arg(installation_storage_scope()));
}

QString plugin_settings_path()
{
	return scoped_settings_path(QStringLiteral("tiktok-live-obs-%1.ini")
		.arg(installation_storage_scope()));
}
