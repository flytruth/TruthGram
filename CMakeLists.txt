# =============================================================================
# TruthGram - privacy-first, zero-telemetry Telegram Desktop client
# Root CMake build definition
#
# This file is the TruthGram overlay that extends telegramdesktop/tdesktop.
# It builds the `tg_truthgram` static library and links it into the
# `truthgram_app` executable. Upstream tdesktop is expected at TDESKTOP_DIR.
# =============================================================================
cmake_minimum_required(VERSION 3.25)

# ---- Project metadata -------------------------------------------------------
project(TruthGram
    VERSION 1.0.0
    DESCRIPTION "Privacy-first, zero-telemetry Telegram Desktop client"
    LANGUAGES CXX C
)

# ---- Module option flags (every subsystem can be compiled out) --------------
option(TRUTHGRAM_ENABLE_IOS_UI          "iOS-inspired desktop UI"                 ON)
option(TRUTHGRAM_ENABLE_MINI_PLAYER     "Detachable FFT mini media player"        ON)
option(TRUTHGRAM_ENABLE_PLUGINS         "Plugin engine (native + Lua/Python)"     ON)
option(TRUTHGRAM_ENABLE_CLOUD_SYNC      "Saved Messages cross-device sync"        ON)
option(TRUTHGRAM_ENABLE_VOICE_CHANGER   "tg_owt voice changer DSP"                ON)
option(TRUTHGRAM_ENABLE_GHOST           "Ghost Mode read/unread controls"         ON)
option(TRUTHGRAM_ENABLE_PHISHING        "Link Guardian phishing analyzer"         ON)
option(TRUTHGRAM_ENABLE_WEBAPP_AUDIT    "Mini App domain/TLS inspector"           ON)
option(TRUTHGRAM_ENABLE_OSINT           "OSINT/Sherlock local runner"             ON)
option(TRUTHGRAM_ENABLE_SCREEN_SHIELD   "Screenshot/capture protection"           ON)
option(TRUTHGRAM_ENABLE_CHESS           "Built-in P2P chess"                      ON)
option(TRUTHGRAM_ENABLE_BOT_STUDIO      "BotFather visual builder"                ON)
option(TRUTHGRAM_ENABLE_TERMUX_BRIDGE   "Termux/local CLI IPC bridge"             ON)
option(TRUTHGRAM_ENABLE_LUA_PLUGINS     "Embed LuaJIT for script plugins"         ON)
option(TRUTHGRAM_ENABLE_PYTHON_PLUGINS  "Embed CPython for script plugins"        OFF)
option(TRUTHGRAM_ENABLE_TESTS           "Build TruthGram unit tests"              OFF)
option(TRUTHGRAM_ENABLE_ASAN            "Enable AddressSanitizer (debug)"         OFF)
option(TRUTHGRAM_ENABLE_LTO             "Link-time optimization (Release)"        ON)

# ---- Locate upstream tdesktop ----------------------------------------------
set(TRUTHGRAM_TDESKTOP_DIR "" CACHE PATH
    "Path to the telegramdesktop/tdesktop checkout to extend")
if(NOT TRUTHGRAM_TDESKTOP_DIR)
    set(TRUTHGRAM_TDESKTOP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tdesktop")
endif()
set(TDESKTOP_DIR ${TRUTHGRAM_TDESKTOP_DIR} CACHE PATH "tdesktop root" FORCE)

# ---- C++20 standard, enforced everywhere -----------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)   # tdesktop does not use C++23 modules

# Qt MOC/RCC/UIC — required because several TruthGram classes (SpringAnimation
# in floating_action_bar.cpp, SpringScroller in ios_ui_engine.cpp) declare
# Q_OBJECT inside .cpp files and include the generated *.moc.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---- Helper modules ---------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Platform)        # WIN32/LINUX/APPLE guards, output dir helpers
include(CompilerFlags)   # -Wall -Wextra, /permissive-, LTO, ASAN
include(Dependencies)    # Qt6, tg_owt, lib_*, SQLCipher, OpenSSL, FFmpeg

# ---- TruthGram version header ----------------------------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Telegram/SourceFiles/truthgram/truthgram_version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/truthgram_version.h"
    @ONLY
)

# ---- TruthGram feature-flag header (generated) -----------------------------
# truthgram_config.h is GENERATED into ${CMAKE_BINARY_DIR}/generated/ from the
# truthgram_config.h.in template. The template uses @TRUTHGRAM_ENABLE_*@
# substitution so every CMake option becomes a concrete `#define ... 1|0` in
# the generated header — the source never has to rely on -D being passed on
# the command line. We normalize ON/OFF -> 1/0 here (a bool option's cache
# value is "ON"/"OFF", which the preprocessor would treat as 0).
foreach(_opt IN ITEMS
        IOS_UI MINI_PLAYER PLUGINS CLOUD_SYNC VOICE_CHANGER GHOST PHISHING
        WEBAPP_AUDIT OSINT SCREEN_SHIELD CHESS BOT_STUDIO TERMUX_BRIDGE
        LUA_PLUGINS PYTHON_PLUGINS)
    if(TRUTHGRAM_ENABLE_${_opt})
        set(TRUTHGRAM_ENABLE_${_opt} 1)
    else()
        set(TRUTHGRAM_ENABLE_${_opt} 0)
    endif()
endforeach()
# TRUTHGRAM_HAVE_KISSFFT is set by Dependencies.cmake (1 if Kiss FFT found).
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Telegram/SourceFiles/truthgram/truthgram_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/truthgram_config.h"
    @ONLY
)

# ---- The tg_truthgram static library ---------------------------------------
set(TG_TRUTHGRAM_SOURCES
    "Telegram/SourceFiles/truthgram/truthgram_init.cpp"
    "Telegram/SourceFiles/truthgram/theme/truthgram_theme.cpp"
    "Telegram/SourceFiles/truthgram/storage/truthgram_database.cpp"
    "Telegram/SourceFiles/truthgram/ui/floating_action_bar.cpp"
)
set(TG_TRUTHGRAM_HEADERS
    "Telegram/SourceFiles/truthgram/truthgram_pch.h"
    "Telegram/SourceFiles/truthgram/truthgram_init.h"
    "Telegram/SourceFiles/truthgram/theme/truthgram_theme.h"
    "Telegram/SourceFiles/truthgram/storage/truthgram_database.h"
    "Telegram/SourceFiles/truthgram/ui/floating_action_bar.h"
    "Telegram/SourceFiles/truthgram/ui/widget_slot_registry.h"
)

if(TRUTHGRAM_ENABLE_IOS_UI)
    list(APPEND TG_TRUTHGRAM_SOURCES
        "Telegram/SourceFiles/truthgram/ios_ui/ios_ui_engine.cpp"
        "Telegram/SourceFiles/truthgram/ios_ui/frosted_sidebar.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS
        "Telegram/SourceFiles/truthgram/ios_ui/ios_ui_engine.h"
        "Telegram/SourceFiles/truthgram/ios_ui/frosted_sidebar.h")
endif()
if(TRUTHGRAM_ENABLE_MINI_PLAYER)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/ios_ui/mini_player.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/ios_ui/mini_player.h")
endif()
if(TRUTHGRAM_ENABLE_PLUGINS)
    list(APPEND TG_TRUTHGRAM_SOURCES
        "Telegram/SourceFiles/truthgram/plugins/plugin_manager.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS
        "Telegram/SourceFiles/truthgram/plugins/plugin_sdk.h"
        "Telegram/SourceFiles/truthgram/plugins/plugin_host.h"
        "Telegram/SourceFiles/truthgram/plugins/plugin_manager.h")
endif()
if(TRUTHGRAM_ENABLE_CLOUD_SYNC)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/sync/cloud_sync.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/sync/cloud_sync.h")
endif()
if(TRUTHGRAM_ENABLE_VOICE_CHANGER)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/voice/voice_changer.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/voice/voice_changer.h")
endif()
if(TRUTHGRAM_ENABLE_GHOST)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/ghost/ghost_controls.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/ghost/ghost_controls.h")
endif()
if(TRUTHGRAM_ENABLE_PHISHING)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/phishing/phishing_guard.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/phishing/phishing_guard.h")
endif()
if(TRUTHGRAM_ENABLE_WEBAPP_AUDIT)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/webapp/webapp_inspector.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/webapp/webapp_inspector.h")
endif()
if(TRUTHGRAM_ENABLE_OSINT)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/osint/osint_runner.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/osint/osint_runner.h")
endif()
if(TRUTHGRAM_ENABLE_SCREEN_SHIELD)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/screen/screen_shield.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/screen/screen_shield.h")
endif()
if(TRUTHGRAM_ENABLE_CHESS)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/chess/tg_chess.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/chess/tg_chess.h")
endif()
if(TRUTHGRAM_ENABLE_BOT_STUDIO)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/botstudio/botfather_studio.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/botstudio/botfather_studio.h")
endif()
if(TRUTHGRAM_ENABLE_TERMUX_BRIDGE)
    list(APPEND TG_TRUTHGRAM_SOURCES "Telegram/SourceFiles/truthgram/termux/termux_bridge.cpp")
    list(APPEND TG_TRUTHGRAM_HEADERS "Telegram/SourceFiles/truthgram/termux/termux_bridge.h")
endif()

add_library(tg_truthgram STATIC ${TG_TRUTHGRAM_SOURCES} ${TG_TRUTHGRAM_HEADERS})
target_include_directories(tg_truthgram PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Telegram/SourceFiles"
    "${CMAKE_CURRENT_SOURCE_DIR}/Telegram/SourceFiles/truthgram"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_compile_features(tg_truthgram PUBLIC cxx_std_20)

# Precompiled header for fast iteration.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.22")
    target_precompile_headers(tg_truthgram PRIVATE
        "Telegram/SourceFiles/truthgram/truthgram_pch.h")
endif()

# Public defines derived from options, so downstream consumers see them.
truthgram_apply_config_defines(tg_truthgram)

# Link Qt6 + the tdesktop lib_* helpers. WebSockets backs the Termux bridge
# (QWebSocketServer / QWebSocket).
target_link_libraries(tg_truthgram PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::WebSockets
)
if(TARGET lib_ui)
    target_link_libraries(tg_truthgram PUBLIC lib_ui lib_base lib_tl)
endif()
if(TARGET tg_owt)
    target_link_libraries(tg_truthgram PUBLIC tg_owt)
endif()

# SQLCipher for local encrypted storage (found by Dependencies.cmake).
target_link_libraries(tg_truthgram PUBLIC SQLCipher::SQLCipher)

# OpenSSL for HKDF / AES-256-GCM in truthgram_init + cloud_sync.
target_link_libraries(tg_truthgram PUBLIC OpenSSL::SSL OpenSSL::Crypto)

# Kiss FFT is optional: when present, the Mini Player draws real FFT bars;
# when absent, mini_player stubs the FFT and the bars render zero-height.
if(TRUTHGRAM_HAVE_KISSFFT AND TARGET KissFFT::KissFFT)
    target_link_libraries(tg_truthgram PRIVATE KissFFT::KissFFT)
endif()

# Resource bundle: theme + icons.
qt_add_resources(tg_truthgram "truthgram_themes"
    PREFIX "/truthgram/themes"
    FILES "resources/themes/cyber-truth.tdesktop-theme")

# ---- The application executable (wraps upstream Telegram) -------------------
if(EXISTS "${TDESKTOP_DIR}/CMakeLists.txt")
    # Defer to upstream's Telegram target; we just attach our library.
    add_subdirectory(${TDESKTOP_DIR} ${CMAKE_BINARY_DIR}/tdesktop-build EXCLUDE_FROM_ALL)
    if(TARGET Telegram)
        target_link_libraries(Telegram PRIVATE tg_truthgram)
        set_target_properties(Telegram PROPERTIES OUTPUT_NAME "TruthGram")
        add_custom_target(truthgram_app ALL DEPENDS Telegram)
    endif()
endif()

# Standalone build (no vendored tdesktop, or tdesktop without a Telegram
# target): a real Qt launcher with main() that exercises every TruthGram
# subsystem against a minimal main window. This is what produces a runnable
# `truthgram_app` binary on Arch Linux without cloning the whole of tdesktop.
# truthgram_init.cpp already lives inside the tg_truthgram static library, so
# the executable only needs the launcher TU (no duplicate symbols).
if(NOT TARGET truthgram_app)
    add_executable(truthgram_app
        "Telegram/SourceFiles/truthgram/truthgram_main.cpp")
    target_link_libraries(truthgram_app PRIVATE tg_truthgram)
    truthgram_apply_config_defines(truthgram_app)
    truthgram_set_output_dirs(truthgram_app)
endif()

# ---- Optional tests --------------------------------------------------------
if(TRUTHGRAM_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- Install rules (used by packaging scripts) -----------------------------
include(GNUInstallDirs)
install(TARGETS tg_truthgram
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY "Telegram/SourceFiles/truthgram/"
    DESTINATION "include/truthgram"
    FILES_MATCHING PATTERN "*.h")
install(FILES "resources/themes/cyber-truth.tdesktop-theme"
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/truthgram/themes")
install(FILES "LICENSE" "README.md" "ARCHITECTURE.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}/truthgram")

message(STATUS "TruthGram ${PROJECT_VERSION} configured")
message(STATUS "  tdesktop root : ${TDESKTOP_DIR}")
message(STATUS "  C++ standard  : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type    : ${CMAKE_BUILD_TYPE}")
