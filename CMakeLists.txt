# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 TikTok Live OBS Contributors

cmake_minimum_required(VERSION 3.26)

project(tiktok_live_obs VERSION 0.1.0 LANGUAGES CXX)

# Single-configuration generators such as NMake do not choose a build type
# automatically. Default to a production-safe Release build while still
# allowing contributors to explicitly request Debug when they need it.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TIKTOK_LIVE_OBS_BUILD_TESTS "Build local developer tests" OFF)

include(FetchContent)

# The plugin owns its HTTPS dependency. Do not rely on TLS plugins or libcurl
# binaries that happen to be shipped by a particular OBS installation.
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(USE_NGTCP2 OFF CACHE BOOL "" FORCE)
set(USE_QUICHE OFF CACHE BOOL "" FORCE)

if(WIN32)
  set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
  set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
elseif(APPLE)
  set(CURL_USE_SECTRANSP ON CACHE BOOL "" FORCE)
  set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
else()
  set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(curl
  GIT_REPOSITORY https://github.com/curl/curl.git
  GIT_TAG 68720b4837284335b2d63cb358f8f6ce65f5bc55
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(curl)

add_library(tiktok-live-obs MODULE
  src/aitum_bridge.cpp
  src/aitum_contract.hpp
	 src/aitum_outputs.cpp
	 src/aitum_outputs.hpp
	 src/bridge_dock.cpp
	 src/bridge_dock.hpp
	 src/bridge_dock_profiles.cpp
	 src/bridge_dock_streams.cpp
	 src/localization.cpp
	 src/localization.hpp
	 src/plugin_paths.cpp
	 src/plugin_paths.hpp
	 src/provider_registry.cpp
	 src/provider_registry.hpp
	 src/profile.hpp
	 src/profile_row.cpp
	 src/profile_row.hpp
	 src/research_lab.cpp
	 src/research_lab.hpp
  src/streamlabs_client.cpp
  src/streamlabs_client.hpp
	 src/streamlabs_desktop.cpp
	 src/streamlabs_desktop.hpp
  src/token_store.cpp
  src/token_store.hpp
  src/module.cpp
)

target_include_directories(tiktok-live-obs PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/generated"
	"${QT_HEADERS_DIR}"
	"${QT_HEADERS_DIR}/QtCore"
	"${QT_HEADERS_DIR}/QtGui"
	"${QT_HEADERS_DIR}/QtWidgets"
	"${QT_HEADERS_DIR}/QtNetwork"
  "${OBS_SOURCE_DIR}/libobs"
  "${OBS_SOURCE_DIR}/UI/obs-frontend-api"
)

target_link_libraries(tiktok-live-obs PRIVATE
	"${QT_IMPORT_LIB_DIR}/Qt6Widgets.lib"
	"${QT_IMPORT_LIB_DIR}/Qt6Gui.lib"
	"${QT_IMPORT_LIB_DIR}/Qt6Core.lib"
	"${QT_IMPORT_LIB_DIR}/Qt6Network.lib"
	libcurl
	advapi32
)

target_compile_definitions(tiktok-live-obs PRIVATE UNICODE _UNICODE)
target_compile_options(tiktok-live-obs PRIVATE /Zc:__cplusplus /utf-8)

set_target_properties(tiktok-live-obs PROPERTIES
  PREFIX ""
  OUTPUT_NAME "tiktok-live-obs"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/dist"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/dist"
)

add_custom_command(TARGET tiktok-live-obs POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/data"
    "$<TARGET_FILE_DIR:tiktok-live-obs>/data"
)

if(TIKTOK_LIVE_OBS_BUILD_TESTS)
  enable_testing()
  add_executable(tiktok-live-obs-research-smoke
    tests/research_lab_smoke.cpp
    src/research_lab.cpp
    src/research_lab.hpp
  )
  target_include_directories(tiktok-live-obs-research-smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${QT_HEADERS_DIR}"
    "${QT_HEADERS_DIR}/QtCore"
    "${QT_HEADERS_DIR}/QtNetwork"
  )
  target_link_libraries(tiktok-live-obs-research-smoke PRIVATE
    "${QT_IMPORT_LIB_DIR}/Qt6Core.lib"
    "${QT_IMPORT_LIB_DIR}/Qt6Network.lib"
  )
  target_compile_definitions(tiktok-live-obs-research-smoke PRIVATE UNICODE _UNICODE)
  target_compile_options(tiktok-live-obs-research-smoke PRIVATE /Zc:__cplusplus /utf-8)
  add_test(NAME local-research-harness COMMAND tiktok-live-obs-research-smoke)

  add_executable(tiktok-live-obs-token-store-smoke
    tests/token_store_smoke.cpp
    src/token_store.cpp
    src/token_store.hpp
  )
  target_include_directories(tiktok-live-obs-token-store-smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${QT_HEADERS_DIR}"
    "${QT_HEADERS_DIR}/QtCore"
  )
  target_link_libraries(tiktok-live-obs-token-store-smoke PRIVATE
    "${QT_IMPORT_LIB_DIR}/Qt6Core.lib"
    advapi32
  )
  target_compile_definitions(tiktok-live-obs-token-store-smoke PRIVATE UNICODE _UNICODE)
  target_compile_options(tiktok-live-obs-token-store-smoke PRIVATE /Zc:__cplusplus /utf-8)
  add_test(NAME token-store-harness COMMAND tiktok-live-obs-token-store-smoke)
endif()
