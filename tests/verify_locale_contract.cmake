# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 TikTok Live OBS Contributors

# This contract protects the two guarantees the UI needs from localization:
# every supported OBS language ships each catalog, and every literal key used
# by C++ exists in an English catalog. Runtime overlays a selected language on
# that English schema, so an incomplete translation cleanly falls back to
# English rather than exposing a raw key to the creator.

if(NOT DEFINED TIKTOK_SOURCE_DIR)
	message(FATAL_ERROR "TIKTOK_SOURCE_DIR must point to the plugin source tree.")
endif()

set(LOCALE_ROOT "${TIKTOK_SOURCE_DIR}/data/locale")
set(CATALOG_DIRECTORIES
	"${LOCALE_ROOT}/core"
	"${LOCALE_ROOT}/providers/manual"
	"${LOCALE_ROOT}/providers/streamlabs"
	"${LOCALE_ROOT}/providers/tiktok-studio")

function(read_catalog_keys catalog_path output)
	if(NOT EXISTS "${catalog_path}")
		message(FATAL_ERROR "Missing locale catalog: ${catalog_path}")
	endif()
	file(STRINGS "${catalog_path}" catalog_lines REGEX "^[^#; 	][^=]*=.*$")
	set(keys)
	foreach(catalog_line IN LISTS catalog_lines)
		string(REGEX REPLACE "^([^=]+)=.*$" "\\1" key "${catalog_line}")
		string(STRIP "${key}" key)
		if(NOT key STREQUAL "")
			list(APPEND keys "${key}")
		endif()
	endforeach()
	list(REMOVE_DUPLICATES keys)
	set(${output} "${keys}" PARENT_SCOPE)
endfunction()

file(GLOB core_catalogs "${LOCALE_ROOT}/core/*.ini")
set(SUPPORTED_LOCALES)
foreach(catalog_path IN LISTS core_catalogs)
	get_filename_component(locale "${catalog_path}" NAME_WE)
	list(APPEND SUPPORTED_LOCALES "${locale}")
endforeach()
list(SORT SUPPORTED_LOCALES)
if(NOT SUPPORTED_LOCALES)
	message(FATAL_ERROR "No supported locale files were found in ${LOCALE_ROOT}/core.")
endif()

set(ENGLISH_KEYS)
foreach(catalog_directory IN LISTS CATALOG_DIRECTORIES)
	read_catalog_keys("${catalog_directory}/en-US.ini" catalog_keys)
	list(APPEND ENGLISH_KEYS ${catalog_keys})
	foreach(locale IN LISTS SUPPORTED_LOCALES)
		if(NOT EXISTS "${catalog_directory}/${locale}.ini")
			message(FATAL_ERROR "${catalog_directory} is missing the ${locale} locale catalog.")
		endif()
	endforeach()
endforeach()
list(REMOVE_DUPLICATES ENGLISH_KEYS)

# The Dual Layout readiness message is intentionally translated directly for
# every shipped language because creators see it as the primary remediation
# instruction, not as an incidental fallback string.
set(DIRECT_TIKTOK_STUDIO_KEYS
	"Studio.Stream.DualLayoutLockedHint"
	"Studio.Stream.DualLayoutLockedHintSingular")
foreach(locale IN LISTS SUPPORTED_LOCALES)
	read_catalog_keys("${LOCALE_ROOT}/providers/tiktok-studio/${locale}.ini" localized_keys)
	foreach(required_key IN LISTS DIRECT_TIKTOK_STUDIO_KEYS)
		list(FIND localized_keys "${required_key}" required_key_index)
		if(required_key_index EQUAL -1)
			message(FATAL_ERROR "TikTok Studio locale ${locale} is missing direct translation key ${required_key}.")
		endif()
	endforeach()
endforeach()

file(GLOB_RECURSE SOURCE_FILES
	"${TIKTOK_SOURCE_DIR}/src/*.cpp"
	"${TIKTOK_SOURCE_DIR}/src/*.hpp")
foreach(source_path IN LISTS SOURCE_FILES)
	file(READ "${source_path}" source_contents)
	string(REGEX MATCHALL
		"(text|translated_or)[ \\t\\r\\n]*\\([ \\t\\r\\n]*\"[^\"]+\""
		localized_calls "${source_contents}")
	foreach(localized_call IN LISTS localized_calls)
		string(REGEX REPLACE ".*\"([^\"]+)\"$" "\\1" literal_key "${localized_call}")
		list(FIND ENGLISH_KEYS "${literal_key}" key_index)
		if(key_index EQUAL -1)
			message(FATAL_ERROR
				"Localization key ${literal_key} in ${source_path} is absent from all English catalogs.")
		endif()
	endforeach()
endforeach()

list(LENGTH SUPPORTED_LOCALES locale_count)
list(LENGTH ENGLISH_KEYS english_key_count)
message(STATUS "Locale contract passed: ${locale_count} locales, ${english_key_count} English fallback keys.")
