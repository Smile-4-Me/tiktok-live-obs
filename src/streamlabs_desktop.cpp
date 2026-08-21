// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Bridge Contributors

#include "streamlabs_desktop.hpp"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>

QString find_streamlabs_desktop_token()
{
	const wchar_t *app_data = _wgetenv(L"APPDATA");
	if (!app_data)
		return {};
	const std::filesystem::path directory = std::filesystem::path(app_data) /
		"slobs-client" / "Local Storage" / "leveldb";
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
