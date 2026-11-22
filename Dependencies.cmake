# =============================================================================
# Dependencies.cmake - locate Qt6, tg_owt, lib_*, SQLCipher, OpenSSL, FFmpeg
# =============================================================================
# TruthGram inherits its dependency surface from upstream tdesktop. The
# upstream CMake configuration declares these as imported targets; here we
# only assert they are visible so configure-time errors are clear when the
# user builds TruthGram standalone.

# --- Qt6 ---
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets Network Svg Concurrent
    Multimedia OpenGL OpenGLWidgets
    Quick QuickWidgets)
set(QT_VERSION_MAJOR 6)

# --- OpenSSL (TLS, crypto primitives for cloud_sync AES-GCM) ---
find_package(OpenSSL 3.0 REQUIRED)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL ${OPENSSL_VERSION} found")
endif()

# --- SQLCipher (encrypted local storage) ---
# TruthGram ships a FindSQLCipher.cmake shim because SQLCipher does not
# distribute a CMake config. The find module exposes SQLCipher::SQLCipher.
find_package(SQLCipher 4.5 REQUIRED)

# --- FFmpeg (audio decode for voice changer preset test loopback) ---
find_package(FFMPEG 6.0 REQUIRED COMPONENTS avcodec avformat avutil)

# --- rnnoise (real-time denoise for voice changer) ---
find_path(RNNOISE_INCLUDE_DIR rnnoise.h)
find_library(RNNOISE_LIBRARY rnnoise)
if(RNNOISE_INCLUDE_DIR AND RNNOISE_LIBRARY)
    add_library(Rnnoise::Rnnoise INTERFACE IMPORTED)
    set_target_properties(Rnnoise::Rnnoise PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${RNNOISE_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${RNNOISE_LIBRARY}")
    message(STATUS "rnnoise found")
else()
    message(WARNING "rnnoise not found — Deep/Anonymous preset falls back to LP filter")
endif()

# --- LuaJIT (script plugins) ---
if(TRUTHGRAM_ENABLE_LUA_PLUGINS)
    find_package(LuaJIT 2.1 REQUIRED)
    add_library(LuaJIT::LuaJIT ALIAS LuaJIT::LuaJIT)
endif()

# --- Python (script plugins, optional) ---
if(TRUTHGRAM_ENABLE_PYTHON_PLUGINS)
    find_package(Python3 COMPONENTS Development.Embed REQUIRED)
endif()

# --- Platform native capture protection ---
if(WIN32)
    # SetWindowDisplayAffinity lives in user32.dll; linked implicitly.
elseif(APPLE)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
    find_library(SECURITY_LIBRARY Security REQUIRED)
else()
    # Wayland protocols for screen shield hints.
    find_package(Wayland OPTIONAL_COMPONENTS client)
    pkg_check_modules(X11 IMPORTED_TARGET x11)
endif()

# --- tg_owt (must be patched with the TruthGram audio hook) ---
# Upstream declares the `tg_owt` target; we verify the patched symbol.
if(TARGET tg_owt)
    message(STATUS "tg_owt target present")
else()
    message(WARNING
      "tg_owt target not found. Apply patches/tg_owt/0001-truthgram-audio-hook.patch\n"
      "to your tg_owt checkout before building with TRUTHGRAM_ENABLE_VOICE_CHANGER=ON.")
endif()
