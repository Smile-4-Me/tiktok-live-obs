#include "bridge_dock.hpp"
#include "localization.hpp"
#include "native_platform.hpp"

#include <QWidget>

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("TikTok Live OBS Contributors")

namespace {

constexpr char dock_id[] = "TikTokLiveObsDock";

using get_main_window_fn = void *(*)();
using add_dock_fn = bool (*)(const char *, const char *, void *);
using remove_dock_fn = void (*)(const char *);

QWidget *bridge_dock = nullptr;

template<typename Function>
Function frontend_function(const char *name)
{
	return reinterpret_cast<Function>(resolve_native_symbol(NativeLibrary::Frontend, name));
}

QWidget *obs_main_window()
{
	const auto get_main_window = frontend_function<get_main_window_fn>("obs_frontend_get_main_window");
	return get_main_window ? static_cast<QWidget *>(get_main_window()) : nullptr;
}

} // namespace

bool obs_module_load(void)
{
	load_translations();
	QWidget *main_window = obs_main_window();
	const auto add_dock = frontend_function<add_dock_fn>("obs_frontend_add_dock_by_id");
	if (!main_window || !add_dock)
		return false;

	bridge_dock = new BridgeDock(main_window);
	const QByteArray dock_title = text("Plugin.Name").toUtf8();
	if (!add_dock(dock_id, dock_title.constData(), bridge_dock)) {
		delete bridge_dock;
		bridge_dock = nullptr;
		return false;
	}
	return true;
}

void obs_module_unload(void)
{
	if (const auto remove_dock = frontend_function<remove_dock_fn>("obs_frontend_remove_dock"))
		remove_dock(dock_id);
	bridge_dock = nullptr;
}
