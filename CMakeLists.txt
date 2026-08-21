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
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

# Prefer the Qt imported targets supplied by normal macOS/Linux development
# environments. The Windows OBS dependency bundle used by the local release
# build exposes only headers and import-library directories, so retain that
# explicit path as a supported fallback.
if(WIN32 AND QT_HEADERS_DIR AND QT_IMPORT_LIB_DIR)
  set(TIKTOK_QT_INCLUDE_DIRS
    "${QT_HEADERS_DIR}"
    "${QT_HEADERS_DIR}/QtCore"
    "${QT_HEADERS_DIR}/QtGui"
    "${QT_HEADERS_DIR}/QtWidgets"
    "${QT_HEADERS_DIR}/QtNetwork"
  )
  set(TIKTOK_QT_CORE "${QT_IMPORT_LIB_DIR}/Qt6Core.lib")
  set(TIKTOK_QT_GUI "${QT_IMPORT_LIB_DIR}/Qt6Gui.lib")
  set(TIKTOK_QT_WIDGETS "${QT_IMPORT_LIB_DIR}/Qt6Widgets.lib")
  set(TIKTOK_QT_NETWORK "${QT_IMPORT_LIB_DIR}/Qt6Network.lib")
else()
  find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
  set(TIKTOK_QT_CORE Qt6::Core)
  set(TIKTOK_QT_GUI Qt6::Gui)
  set(TIKTOK_QT_WIDGETS Qt6::Widgets)
  set(TIKTOK_QT_NETWORK Qt6::Network)
endif()

# OBS' Windows dependency bundle already ships Nayuki's MIT-licensed static QR
# generator. Link it into the plugin so QR login never depends on an external
# image service or a separate helper process.
if(WIN32)
  file(GLOB QRCODEGENCPP_CONFIG_DIRS
    "${OBS_SOURCE_DIR}/.deps/obs-deps-*/lib/cmake/qrcodegencpp"
  )
  if(QRCODEGENCPP_CONFIG_DIRS)
    find_package(qrcodegencpp CONFIG QUIET PATHS ${QRCODEGENCPP_CONFIG_DIRS} NO_DEFAULT_PATH)
  endif()
endif()

if(NOT TARGET qrcodegencpp::qrcodegencpp)
  find_package(qrcodegencpp CONFIG QUIET)
endif()
if(NOT TARGET qrcodegencpp::qrcodegencpp)
  FetchContent_Declare(qrcodegen
    GIT_REPOSITORY https://github.com/nayuki/QR-Code-generator.git
    GIT_TAG v1.8.0
    GIT_SHALLOW TRUE
  )
  FetchContent_GetProperties(qrcodegen)
  if(NOT qrcodegen_POPULATED)
    FetchContent_Populate(qrcodegen)
  endif()
  add_library(tiktok-live-obs-qrcodegen STATIC
    "${qrcodegen_SOURCE_DIR}/cpp/qrcodegen.cpp"
  )
  target_include_directories(tiktok-live-obs-qrcodegen PUBLIC
    "${qrcodegen_SOURCE_DIR}/cpp"
  )
  add_library(qrcodegencpp::qrcodegencpp ALIAS tiktok-live-obs-qrcodegen)
endif()

set(TIKTOK_SECURE_STORAGE_SOURCE)
set(TIKTOK_PLATFORM_LIBRARIES ${CMAKE_DL_LIBS})
if(WIN32)
  set(TIKTOK_SECURE_STORAGE_SOURCE src/secure_storage_win.cpp)
  list(APPEND TIKTOK_PLATFORM_LIBRARIES advapi32)
elseif(APPLE)
  set(TIKTOK_SECURE_STORAGE_SOURCE src/secure_storage_macos.cpp)
  find_library(SECURITY_FRAMEWORK Security REQUIRED)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  list(APPEND TIKTOK_PLATFORM_LIBRARIES ${SECURITY_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
else()
  set(TIKTOK_SECURE_STORAGE_SOURCE src/secure_storage_linux.cpp)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
  list(APPEND TIKTOK_PLATFORM_LIBRARIES PkgConfig::LIBSECRET)
endif()

add_library(tiktok-live-obs MODULE
  src/aitum_bridge.cpp
  src/aitum_contract.hpp
	 src/aitum_outputs.cpp
	 src/aitum_outputs.hpp
	 src/bridge_dock.cpp
	 src/bridge_dock.hpp
	 src/bridge_dock_profiles.cpp
	 src/bridge_dock_streams.cpp
	 src/frame_signing.cpp
	 src/frame_signing.hpp
	 src/localization.cpp
	 src/localization.hpp
	 src/native_platform.cpp
	 src/native_platform.hpp
	 src/native_output_manager.cpp
	 src/native_output_manager.hpp
	 src/output_signing_manager.cpp
	 src/output_signing_manager.hpp
	 src/plugin_paths.cpp
	 src/plugin_paths.hpp
	 src/provider_registry.cpp
	 src/provider_registry.hpp
	 src/profile.hpp
	 src/profile_row.cpp
	 src/profile_row.hpp
	 src/research_lab.cpp
	 src/research_lab.hpp
	 src/credential_chunks.hpp
	 src/secure_storage.hpp
	 ${TIKTOK_SECURE_STORAGE_SOURCE}
	 src/sei_metadata.cpp
	 src/sei_metadata.hpp
  src/streamlabs_client.cpp
  src/streamlabs_client.hpp
	 src/streamlabs_desktop.cpp
	 src/streamlabs_desktop.hpp
	 src/tiktok_request_signer.cpp
	 src/tiktok_request_signer.hpp
	 src/tiktok_studio_account.hpp
	 src/tiktok_studio_client.cpp
	 src/tiktok_studio_client.hpp
	 src/tiktok_studio_device.cpp
	 src/tiktok_studio_device.hpp
	 src/tiktok_studio_eligibility.cpp
	 src/tiktok_studio_eligibility.hpp
	 src/tiktok_studio_game_tags.cpp
	 src/tiktok_studio_game_tags.hpp
	 src/tiktok_studio_topics.cpp
	 src/tiktok_studio_topics.hpp
	 src/tiktok_studio_login_dialog.cpp
	 src/tiktok_studio_login_dialog.hpp
	 src/tiktok_studio_qr.cpp
	 src/tiktok_studio_qr.hpp
	 src/tiktok_studio_session.cpp
	 src/tiktok_studio_session.hpp
  src/token_store.cpp
  src/token_store.hpp
  src/module.cpp
)

target_include_directories(tiktok-live-obs PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/generated"
	${TIKTOK_QT_INCLUDE_DIRS}
  "${OBS_SOURCE_DIR}/libobs"
  "${OBS_SOURCE_DIR}/frontend/api"
  "${OBS_SOURCE_DIR}/UI/obs-frontend-api"
)

target_link_libraries(tiktok-live-obs PRIVATE
	${TIKTOK_QT_WIDGETS}
	${TIKTOK_QT_GUI}
	${TIKTOK_QT_CORE}
	${TIKTOK_QT_NETWORK}
	libcurl
	qrcodegencpp::qrcodegencpp
	${TIKTOK_PLATFORM_LIBRARIES}
)

if(WIN32)
  target_compile_definitions(tiktok-live-obs PRIVATE UNICODE _UNICODE)
endif()
if(MSVC)
  target_compile_options(tiktok-live-obs PRIVATE /Zc:__cplusplus /utf-8)
endif()

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
		${TIKTOK_QT_INCLUDE_DIRS}
  )
  target_link_libraries(tiktok-live-obs-research-smoke PRIVATE
		${TIKTOK_QT_CORE}
		${TIKTOK_QT_NETWORK}
  )
	if(WIN32)
		target_compile_definitions(tiktok-live-obs-research-smoke PRIVATE UNICODE _UNICODE)
	endif()
	if(MSVC)
		target_compile_options(tiktok-live-obs-research-smoke PRIVATE /Zc:__cplusplus /utf-8)
	endif()
  add_test(NAME local-research-harness COMMAND tiktok-live-obs-research-smoke)

  add_executable(tiktok-live-obs-provider-registry-smoke
    tests/provider_registry_smoke.cpp
    src/provider_registry.cpp
    src/provider_registry.hpp
  )
  target_include_directories(tiktok-live-obs-provider-registry-smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${TIKTOK_QT_INCLUDE_DIRS}
  )
  target_link_libraries(tiktok-live-obs-provider-registry-smoke PRIVATE
    ${TIKTOK_QT_CORE}
  )
  if(WIN32)
    target_compile_definitions(tiktok-live-obs-provider-registry-smoke PRIVATE UNICODE _UNICODE)
  endif()
  if(MSVC)
    target_compile_options(tiktok-live-obs-provider-registry-smoke PRIVATE /Zc:__cplusplus /utf-8)
  endif()
  add_test(NAME provider-registry-harness COMMAND tiktok-live-obs-provider-registry-smoke)

	add_executable(tiktok-live-obs-media-signing-test
		tests/media_signing_test.cpp
		src/frame_signing.cpp
		src/frame_signing.hpp
		src/sei_metadata.cpp
		src/sei_metadata.hpp
	)
	target_include_directories(tiktok-live-obs-media-signing-test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		${TIKTOK_QT_INCLUDE_DIRS}
	)
	target_link_libraries(tiktok-live-obs-media-signing-test PRIVATE
		${TIKTOK_QT_CORE}
		libcurl
	)
	if(WIN32)
		target_compile_definitions(tiktok-live-obs-media-signing-test PRIVATE UNICODE _UNICODE)
	endif()
	if(MSVC)
		target_compile_options(tiktok-live-obs-media-signing-test PRIVATE /Zc:__cplusplus /utf-8)
	endif()
	add_test(NAME media-signing-pipeline COMMAND tiktok-live-obs-media-signing-test)

	add_executable(tiktok-live-obs-studio-protocol-test
		tests/tiktok_studio_protocol_test.cpp
		src/frame_signing.cpp
		src/frame_signing.hpp
		src/tiktok_request_signer.cpp
		src/tiktok_request_signer.hpp
		src/tiktok_studio_device.cpp
		src/tiktok_studio_device.hpp
		src/tiktok_studio_eligibility.cpp
		src/tiktok_studio_eligibility.hpp
		src/tiktok_studio_game_tags.cpp
		src/tiktok_studio_game_tags.hpp
		src/tiktok_studio_topics.cpp
		src/tiktok_studio_topics.hpp
		src/tiktok_studio_qr.cpp
		src/tiktok_studio_qr.hpp
		src/tiktok_studio_session.cpp
		src/tiktok_studio_session.hpp
	)
	target_include_directories(tiktok-live-obs-studio-protocol-test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		${TIKTOK_QT_INCLUDE_DIRS}
	)
	target_link_libraries(tiktok-live-obs-studio-protocol-test PRIVATE
		${TIKTOK_QT_CORE}
		${TIKTOK_QT_GUI}
		libcurl
		qrcodegencpp::qrcodegencpp
	)
	if(WIN32)
		target_compile_definitions(tiktok-live-obs-studio-protocol-test PRIVATE UNICODE _UNICODE)
	endif()
	if(MSVC)
		target_compile_options(tiktok-live-obs-studio-protocol-test PRIVATE /Zc:__cplusplus /utf-8)
	endif()
	add_test(NAME tiktok-studio-protocol COMMAND tiktok-live-obs-studio-protocol-test)

	add_executable(tiktok-live-obs-credential-chunk-test
		tests/credential_chunks_test.cpp
		src/credential_chunks.hpp
		src/secure_storage.hpp
	)
	target_include_directories(tiktok-live-obs-credential-chunk-test PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/src"
		${TIKTOK_QT_INCLUDE_DIRS}
	)
	target_link_libraries(tiktok-live-obs-credential-chunk-test PRIVATE ${TIKTOK_QT_CORE})
	if(MSVC)
		target_compile_options(tiktok-live-obs-credential-chunk-test PRIVATE /Zc:__cplusplus /utf-8)
	endif()
	add_test(NAME credential-chunk-boundary COMMAND tiktok-live-obs-credential-chunk-test)

	set(TIKTOK_TEST_NAMES local-research-harness provider-registry-harness
		media-signing-pipeline tiktok-studio-protocol credential-chunk-boundary)
	if(WIN32)
		add_executable(tiktok-live-obs-token-store-smoke
			tests/token_store_smoke.cpp
			src/token_store.cpp
			src/token_store.hpp
			src/credential_chunks.hpp
			src/secure_storage.hpp
			src/secure_storage_win.cpp
		)
		target_include_directories(tiktok-live-obs-token-store-smoke PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}/src"
			${TIKTOK_QT_INCLUDE_DIRS}
		)
		target_link_libraries(tiktok-live-obs-token-store-smoke PRIVATE
			${TIKTOK_QT_CORE} advapi32)
		target_compile_definitions(tiktok-live-obs-token-store-smoke PRIVATE UNICODE _UNICODE)
		if(MSVC)
			target_compile_options(tiktok-live-obs-token-store-smoke PRIVATE /Zc:__cplusplus /utf-8)
		endif()
		add_test(NAME token-store-harness COMMAND tiktok-live-obs-token-store-smoke)

		add_executable(tiktok-live-obs-secure-storage-test
			tests/secure_storage_test.cpp
			src/secure_storage.hpp
			src/secure_storage_win.cpp
		)
		target_include_directories(tiktok-live-obs-secure-storage-test PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}/src"
			${TIKTOK_QT_INCLUDE_DIRS}
		)
		target_link_libraries(tiktok-live-obs-secure-storage-test PRIVATE
			${TIKTOK_QT_CORE} advapi32)
		target_compile_definitions(tiktok-live-obs-secure-storage-test PRIVATE UNICODE _UNICODE)
		if(MSVC)
			target_compile_options(tiktok-live-obs-secure-storage-test PRIVATE /Zc:__cplusplus /utf-8)
		endif()
		add_test(NAME secure-storage-roundtrip COMMAND tiktok-live-obs-secure-storage-test)
		list(APPEND TIKTOK_TEST_NAMES token-store-harness secure-storage-roundtrip)
		set_tests_properties(${TIKTOK_TEST_NAMES} PROPERTIES
			ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${QT_IMPORT_LIB_DIR}/../bin")
	endif()
endif()
