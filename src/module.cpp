// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "live_obs_dock.hpp"
#include "localization.hpp"

#include <windows.h>

#include <QWidget>

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("TikTok Live OBS Contributors")

namespace {

constexpr char dock_id[] = "TikTokLiveObsDock";

using GetMainWindowFunction = void *(*)();
using AddDockFunction = bool (*)(const char *, const char *, void *);
using RemoveDockFunction = void (*)(const char *);

QWidget *dock = nullptr;

template<typename Function>
Function frontend_function(const char *name)
{
	const HMODULE frontend = GetModuleHandleW(L"obs-frontend-api.dll");
	return frontend ? reinterpret_cast<Function>(GetProcAddress(frontend, name)) : nullptr;
}

} // namespace

bool obs_module_load(void)
{
	load_translations();
	const auto main_window = frontend_function<GetMainWindowFunction>("obs_frontend_get_main_window");
	const auto add_dock = frontend_function<AddDockFunction>("obs_frontend_add_dock_by_id");
	if (!main_window || !add_dock)
		return false;

	dock = new LiveObsDock(static_cast<QWidget *>(main_window()));
	const QByteArray title = text("Plugin.Name").toUtf8();
	if (!add_dock(dock_id, title.constData(), dock)) {
		delete dock;
		dock = nullptr;
		return false;
	}
	return true;
}

void obs_module_unload(void)
{
	if (const auto remove_dock = frontend_function<RemoveDockFunction>("obs_frontend_remove_dock"))
		remove_dock(dock_id);
	dock = nullptr;
}
