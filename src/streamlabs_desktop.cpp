// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "streamlabs_desktop.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>

#include <QDir>
#include <QFile>
#include <QStandardPaths>

QString find_streamlabs_desktop_token()
{
	QString application_data;
#ifdef Q_OS_WIN
	application_data = qEnvironmentVariable("APPDATA");
#elif defined(Q_OS_MACOS)
	application_data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#else
	application_data = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif
	const QString directory_name = QDir(application_data).filePath(
		QStringLiteral("slobs-client/Local Storage/leveldb"));
#ifdef Q_OS_WIN
	const std::filesystem::path directory(directory_name.toStdWString());
#else
	const QByteArray native_directory = QFile::encodeName(directory_name);
	const std::filesystem::path directory(native_directory.constData());
#endif
	if (!std::filesystem::is_directory(directory))
		return {};

	std::vector<std::filesystem::directory_entry> files;
	for (const auto &entry : std::filesystem::directory_iterator(directory))
		if (entry.is_regular_file() && entry.path().extension() == ".log")
			files.push_back(entry);
	std::sort(files.begin(), files.end(), [](const auto &left, const auto &right) {
		return left.last_write_time() > right.last_write_time();
	});

	const std::regex token_pattern(R"token("apiToken":"([a-fA-F0-9]+)")token");
	for (const auto &entry : files) {
		std::ifstream input(entry.path(), std::ios::binary);
		std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
		content.erase(std::remove(content.begin(), content.end(), '\0'), content.end());
		std::sregex_iterator last;
		for (std::sregex_iterator it(content.begin(), content.end(), token_pattern), end;
			it != end; ++it)
			last = it;
		if (last != std::sregex_iterator())
			return QString::fromStdString((*last)[1].str());
	}
	return {};
}
