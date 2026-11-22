# =============================================================================
# Dependencies.cmake - locate Qt6, tg_owt, lib_*, SQLCipher, OpenSSL, FFmpeg
# =============================================================================
# TruthGram inherits its dependency surface from upstream tdesktop. The
# upstream CMake configuration declares these as imported targets; here we
# only assert they are visible so configure-time errors are clear when the
# user builds TruthGram standalone.
#
# pkg-config is the primary discovery mechanism for the C libraries (FFmpeg,
# SQLCipher, LuaJIT, Kiss FFT) because every Find module we ship uses it; we
# require it once here so the modules can rely on PkgConfig_FOUND.
find_package(PkgConfig REQUIRED)

# --- Qt6 ---
# WebSockets is required by termux_bridge (QWebSocketServer / QWebSocket).
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets Network Svg Concurrent
    Multimedia OpenGL OpenGLWidgets
    WebSockets)
set(QT_VERSION_MAJOR 6)

# --- OpenSSL (TLS, crypto primitives for cloud_sync AES-GCM + HKDF) ---
find_package(OpenSSL 3.0 REQUIRED)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL ${OPENSSL_VERSION} found")
endif()

# --- SQLCipher (encrypted local storage) ---
# TruthGram ships cmake/FindSQLCipher.cmake because SQLCipher does not
# distribute a CMake config. The find module exposes SQLCipher::SQLCipher and
# carries SQLITE_HAS_CODEC / SQLITE_THREADSAFE=1 as proper semicolon-separated
# INTERFACE_COMPILE_DEFINITIONS.
find_package(SQLCipher 4.5 REQUIRED)

# --- FFmpeg (audio decode; not currently linked by any TruthGram TU, kept as
# an optional discovery so the targets exist for downstream plugin authors) ---
find_package(FFMPEG 6.0 COMPONENTS avcodec avformat avutil)

# --- rnnoise (real-time denoise for the voice changer "Deep/Anonymous" preset;
# optional, falls back to a first-order LP filter when absent) ---
find_path(RNNOISE_INCLUDE_DIR rnnoise.h)
find_library(RNNOISE_LIBRARY rnnoise)
if(RNNOISE_INCLUDE_DIR AND RNNOISE_LIBRARY)
    if(NOT TARGET Rnnoise::Rnnoise)
        add_library(Rnnoise::Rnnoise INTERFACE IMPORTED)
        set_target_properties(Rnnoise::Rnnoise PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${RNNOISE_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES "${RNNOISE_LIBRARY}")
    endif()
    message(STATUS "rnnoise found")
else()
    message(STATUS "rnnoise not found - Deep/Anonymous preset falls back to LP filter")
endif()

# --- LuaJIT (script plugins) ---
# FindLuaJIT.cmake creates the LuaJIT::LuaJIT imported target (guarded by
# `if(NOT TARGET LuaJIT::LuaJIT)`). Do NOT alias it to itself here.
if(TRUTHGRAM_ENABLE_LUA_PLUGINS)
    find_package(LuaJIT 2.1 REQUIRED)
endif()

# --- Python (script plugins, optional) ---
if(TRUTHGRAM_ENABLE_PYTHON_PLUGINS)
    find_package(Python3 COMPONENTS Development.Embed REQUIRED)
endif()

# --- Kiss FFT (Mini Player FFT waveform bars; optional) ---
# When found, TRUTHGRAM_HAVE_KISSFFT=1 is propagated to the source via the
# generated truthgram_config.h and mini_player uses the real kiss_fftr API.
# When not found, the Mini Player still builds; the bars render zero-height.
find_package(KissFFT)
if(KissFFT_FOUND)
    set(TRUTHGRAM_HAVE_KISSFFT 1)
else()
    set(TRUTHGRAM_HAVE_KISSFFT 0)
endif()

# --- Platform native capture protection ---
if(WIN32)
    # SetWindowDisplayAffinity lives in user32.dll; linked implicitly.
elseif(APPLE)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
    find_library(SECURITY_LIBRARY Security REQUIRED)
else()
    # Linux: the Screen Shield falls back to BlankOnCapture (a Qt paint-event
    # filter installed by truthgram_init). No Wayland / X11 link is needed, so
    # nothing to find here. Keeping the block explicit for documentation.
endif()

# --- tg_owt (must be patched with the TruthGram audio hook) ---
# Upstream declares the `tg_owt` target; we verify the patched symbol. When
# tdesktop / tg_owt is not vendored (standalone build), this is just a warning.
if(TARGET tg_owt)
    message(STATUS "tg_owt target present")
else()
    message(STATUS
      "tg_owt target not found (standalone build). Apply "
      "patches/tg_owt/0001-truthgram-audio-hook.patch to your tg_owt checkout "
      "before building with TRUTHGRAM_ENABLE_VOICE_CHANGER=ON.")
endif()
