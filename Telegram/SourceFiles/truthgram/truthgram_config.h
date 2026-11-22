// truthgram_config.h
//
// Compile-time + runtime feature flags for TruthGram. Every TruthGram module
// is gated by one of these so distributors can ship slimmed binaries.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
#pragma once

#include "truthgram_version.h"   // generated

namespace tg::truthgram {

// ---- Compile-time feature flags (mirrors CMake options) --------------------
#if !defined(TRUTHGRAM_ENABLE_IOS_UI)
#  define TRUTHGRAM_ENABLE_IOS_UI 1
#endif
#if !defined(TRUTHGRAM_ENABLE_MINI_PLAYER)
#  define TRUTHGRAM_ENABLE_MINI_PLAYER 1
#endif
#if !defined(TRUTHGRAM_ENABLE_PLUGINS)
#  define TRUTHGRAM_ENABLE_PLUGINS 1
#endif
#if !defined(TRUTHGRAM_ENABLE_LUA_PLUGINS)
#  define TRUTHGRAM_ENABLE_LUA_PLUGINS 1
#endif
#if !defined(TRUTHGRAM_ENABLE_PYTHON_PLUGINS)
#  define TRUTHGRAM_ENABLE_PYTHON_PLUGINS 0
#endif
#if !defined(TRUTHGRAM_ENABLE_CLOUD_SYNC)
#  define TRUTHGRAM_ENABLE_CLOUD_SYNC 1
#endif
#if !defined(TRUTHGRAM_ENABLE_VOICE_CHANGER)
#  define TRUTHGRAM_ENABLE_VOICE_CHANGER 1
#endif
#if !defined(TRUTHGRAM_ENABLE_GHOST)
#  define TRUTHGRAM_ENABLE_GHOST 1
#endif
#if !defined(TRUTHGRAM_ENABLE_PHISHING)
#  define TRUTHGRAM_ENABLE_PHISHING 1
#endif
#if !defined(TRUTHGRAM_ENABLE_WEBAPP_AUDIT)
#  define TRUTHGRAM_ENABLE_WEBAPP_AUDIT 1
#endif
#if !defined(TRUTHGRAM_ENABLE_OSINT)
#  define TRUTHGRAM_ENABLE_OSINT 1
#endif
#if !defined(TRUTHGRAM_ENABLE_SCREEN_SHIELD)
#  define TRUTHGRAM_ENABLE_SCREEN_SHIELD 1
#endif
#if !defined(TRUTHGRAM_ENABLE_CHESS)
#  define TRUTHGRAM_ENABLE_CHESS 1
#endif
#if !defined(TRUTHGRAM_ENABLE_BOT_STUDIO)
#  define TRUTHGRAM_ENABLE_BOT_STUDIO 1
#endif
#if !defined(TRUTHGRAM_ENABLE_TERMUX_BRIDGE)
#  define TRUTHGRAM_ENABLE_TERMUX_BRIDGE 1
#endif

// ---- Platform identification ------------------------------------------------
#if defined(_WIN32)
#  define TRUTHGRAM_ON_WINDOWS 1
#elif defined(__APPLE__)
#  define TRUTHGRAM_ON_MACOS  1
#else
#  define TRUTHGRAM_ON_LINUX  1
#endif

// ---- Canonical Cyber-Truth palette (RGB triplet struct) -------------------
// Kept here so any module can reference the brand colors even before the
// theme engine has parsed the .tdesktop-theme file.
struct RgbColor {
    std::uint8_t r = 0, g = 0, b = 0;
    constexpr std::uint32_t packed() const noexcept {
        return (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | b;
    }
    constexpr bool operator==(RgbColor other) const noexcept {
        return r == other.r && g == other.g && b == other.b;
    }
};

namespace brand {
    // Primary background
    constexpr RgbColor kObsidian       { 0x0A, 0x0C, 0x0E };
    constexpr RgbColor kObsidianAlt    { 0x11, 0x14, 0x18 };

    // Primary accent — Electric Lime Green
    constexpr RgbColor kLime           { 0x39, 0xFF, 0x14 };
    constexpr RgbColor kLimeDim        { 0x84, 0xCC, 0x16 };

    // Secondary accent — Ice Light Blue
    constexpr RgbColor kIce            { 0x38, 0xBD, 0xF8 };
    constexpr RgbColor kIceDim         { 0x7D, 0xD3, 0xFC };
} // namespace brand

// ---- Runtime config (loaded from SQLCipher + cloud_sync) -------------------
// A single immutable-once-loaded view of the user's TruthGram toggles. Reads
// are cheap; writes go through truthgram::Config::set() which persists to the
// DB and triggers a cloud_sync publish.
struct RuntimeConfig {
    bool ghostSuppressReadHistory   = true;
    bool ghostSuppressReadContents  = true;
    bool ghostSuppressTyping        = true;
    bool ghostSuppressPresence      = true;
    bool screenShieldEnabled        = false;
    bool screenShieldStreamerMode   = false;
    bool phishingGuardConfirm       = true;
    bool phishingGuardVirusTotal    = false;   // requires opt-in API key
    bool osintOverTor               = true;
    bool cloudSyncEnabled           = true;
    bool frostedGlass               = true;
    bool springScroll               = true;
    bool miniPlayerDetached         = false;
    bool voiceChangerLoopback       = false;
    RgbColor themeAccent            = brand::kLime;
    RgbColor themeLink              = brand::kIce;
};

// ---- Path helpers (where the user's TruthGram data lives) -----------------
const char* userPluginsDir();        // ~/.local/share/TruthGram/plugins
const char* userDataDir();           // ~/.local/share/TruthGram
const char* userCacheDir();          // ~/.cache/TruthGram

} // namespace tg::truthgram
