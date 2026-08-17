# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 TikTok Live OBS Contributors

cmake_minimum_required(VERSION 3.26)
project(tiktok_live_obs VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tiktok-live-obs MODULE
  src/aitum_bridge.cpp
  src/aitum_contract.hpp
  src/aitum_outputs.cpp
  src/aitum_outputs.hpp
  src/live_obs_dock.cpp
  src/live_obs_dock.hpp
  src/live_session_provider.hpp
  src/manual_credentials_provider.cpp
  src/manual_credentials_provider.hpp
  src/profile.hpp
  src/localization.cpp
  src/localization.hpp
  src/plugin_paths.cpp
  src/plugin_paths.hpp
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
  advapi32
)

target_compile_definitions(tiktok-live-obs PRIVATE UNICODE _UNICODE)
target_compile_options(tiktok-live-obs PRIVATE /Zc:__cplusplus /utf-8)

set_target_properties(tiktok-live-obs PROPERTIES
  PREFIX ""
  OUTPUT_NAME "tiktok-live-obs"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/dist"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/dist"
)

add_custom_command(TARGET tiktok-live-obs POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/data"
    "$<TARGET_FILE_DIR:tiktok-live-obs>/data"
)
