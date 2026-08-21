// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#include "native_platform.hpp"

#include <QFile>
#include <QFileInfo>

#include <iterator>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace {

QString library_fragment(NativeLibrary library)
{
	switch (library) {
	case NativeLibrary::Obs:
		return QStringLiteral("obs");
	case NativeLibrary::Frontend:
		return QStringLiteral("obs-frontend-api");
	case NativeLibrary::Aitum:
		return QStringLiteral("aitum-stream-suite");
	}
	return {};
}

#ifdef _WIN32
const wchar_t *windows_library_name(NativeLibrary library)
{
	switch (library) {
	case NativeLibrary::Obs:
		return L"obs.dll";
	case NativeLibrary::Frontend:
		return L"obs-frontend-api.dll";
	case NativeLibrary::Aitum:
		return L"aitum-stream-suite.dll";
	}
	return L"";
}
#endif

} // namespace

void *resolve_native_symbol(NativeLibrary library, const char *name)
{
#ifdef _WIN32
	const HMODULE module = GetModuleHandleW(windows_library_name(library));
	return module ? reinterpret_cast<void *>(GetProcAddress(module, name)) : nullptr;
#else
	Q_UNUSED(library);
	return dlsym(RTLD_DEFAULT, name);
#endif
}

bool native_library_loaded(NativeLibrary library)
{
#ifdef _WIN32
	return GetModuleHandleW(windows_library_name(library)) != nullptr;
#elif defined(__APPLE__)
	const QString fragment = library_fragment(library);
	for (uint32_t index = 0; index < _dyld_image_count(); ++index) {
		const char *name = _dyld_get_image_name(index);
		if (name && QString::fromUtf8(name).contains(fragment, Qt::CaseInsensitive))
			return true;
	}
	return false;
#else
	QFile maps(QStringLiteral("/proc/self/maps"));
	if (!maps.open(QIODevice::ReadOnly))
		return library != NativeLibrary::Aitum &&
			resolve_native_symbol(library, "obs_get_version") != nullptr;
	const QString content = QString::fromUtf8(maps.readAll());
	return content.contains(library_fragment(library), Qt::CaseInsensitive);
#endif
}

QString current_module_file(const void *address)
{
#ifdef _WIN32
	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(address), &module))
		return {};
	wchar_t path[32768]{};
	const DWORD length = GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)));
	return length ? QString::fromWCharArray(path, length) : QString{};
#else
	Dl_info info{};
	return dladdr(address, &info) != 0 && info.dli_fname
		? QFileInfo(QString::fromUtf8(info.dli_fname)).absoluteFilePath()
		: QString{};
#endif
}
