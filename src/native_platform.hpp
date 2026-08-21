// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 TikTok Live OBS Contributors

#pragma once

#include <QString>

enum class NativeLibrary {
	Obs,
	Frontend,
	Aitum,
};

void *resolve_native_symbol(NativeLibrary library, const char *name);
bool native_library_loaded(NativeLibrary library);
QString current_module_file(const void *address);
