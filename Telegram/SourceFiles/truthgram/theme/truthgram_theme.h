// truthgram_theme.h
//
// TruthGram "Cyber-Truth" theme engine: loads/parses .tdesktop-theme files,
// overrides QPalette with the deep-obsidian / lime / ice palette, and exposes
// a live color-accent customizer used by Settings > TruthGram Appearance.
//
// The engine stays 100% hot-swappable: replace the theme at runtime by calling
// TruthGramTheme::load(path) — every widget that subscribes to the
// accentChanged / paletteChanged signals repaints automatically.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QColor>
#include <QPalette>
#include <QString>
#include <QHash>
#include <QPixmap>
#include <QRecursiveMutex>

#include <atomic>
#include <memory>
#include <optional>

namespace tg::truthgram::theme {

// A single key/value entry from a .tdesktop-theme file. Values are stored as
// strings (the raw "#RRGGBB" or integer) so we can re-export the theme verbatim.
struct ThemeEntry {
    QString key;
    QString raw;        // raw value string ("#39ff14", "14", etc.)
    QColor  color;      // parsed color if raw was "#RRGGBB[AA]"
    int     integer{};  // parsed integer if raw was a number
};

// TruthGram-specific extras (keys with the "tg_" prefix in the theme file).
// These drive the iOS UI engine, mini player FFT, ghost bar, etc.
struct TruthGramExtras {
    QColor frostBlurColor     = QColor(0x11, 0x14, 0x18, 0xC0);
    QColor ghostBarBg         = QColor(0x16, 0x1A, 0x1F);
    QColor ghostBarBorder     = QColor(0x1C, 0x21, 0x28);
    QColor osintMetaBg         = QColor(0x0F, 0x12, 0x16);
    QColor osintMetaBorder     = QColor(0x1C, 0x21, 0x28);
    QColor miniPlayerFftLow    = QColor(0x39, 0xFF, 0x14);
    QColor miniPlayerFftHigh   = QColor(0x38, 0xBD, 0xF8);
    int    frostBlurRadius     = 24;
    int    bubbleRadius        = 14;
    double springDamping       = 0.825;
    double springStiffness     = 0.45;
};

// Accent override set by the live customizer. Empty optional = use theme value.
struct AccentOverride {
    std::optional<QColor> accent;       // primary active elements
    std::optional<QColor> accentDim;
    std::optional<QColor> link;         // secondary accent / links
    std::optional<QColor> linkDim;
};

// Error reasons surfaced to the UI when a theme fails to load.
enum class LoadError {
    None,
    FileNotFound,
    ReadFailed,
    MalformedVariable,
    FontBlockMalformed,
};

Q_NAMESPACE
Q_ENUM_NS(LoadError)

// =============================================================================
// TruthGramTheme
// =============================================================================
// Singleton theme engine. Thread-safe reads via cachedSnapshot(); writes
// (load / setAccent) take the internal mutex and broadcast change signals.
class TruthGramTheme : public QObject {
    Q_OBJECT
public:
    static TruthGramTheme& instance();

    // Load (or reload) a .tdesktop-theme file. Returns the error reason and,
    // on success, applies the palette to qApp and emits paletteChanged.
    LoadError load(const QString& path);
    LoadError loadBuiltinCyberTruth();   // uses the qrc resource

    // Apply the current palette to a specific QGuiApplication instance.
    // Called automatically by load(), but exported for tests.
    void applyToApplication() const;

    // Live color-accent customizer. Persists to SQLCipher + cloud_sync.
    void setAccentOverride(const AccentOverride& o);
    [[nodiscard]] AccentOverride accentOverride() const;

    // Quick lookups used by widgets that want a specific role color without
    // paying for a QPalette lookup.
    [[nodiscard]] QColor windowBg() const;
    [[nodiscard]] QColor accent() const;       // lime
    [[nodiscard]] QColor accentDim() const;    // lime dim
    [[nodiscard]] QColor link() const;         // ice
    [[nodiscard]] QColor linkDim() const;      // ice dim
    [[nodiscard]] QColor textFg() const;
    [[nodiscard]] QColor subTextFg() const;

    [[nodiscard]] const TruthGramExtras& extras() const;

    // The full QPalette we want every Qt widget to see. This is the snapshot
    // actually applied to qApp; widgets that paint custom roles read from here.
    [[nodiscard]] QPalette palette() const;

    // True if a .tdesktop-theme is currently loaded and applied.
    [[nodiscard]] bool loaded() const;

    // Re-derive the accent color in HSV from a user-supplied hue (the
    // customizer slider). Keeps S/V at the Lime brand defaults so the result
    // always looks on-brand.
    static QColor limeFromHue(int hue);        // hue 0..359
    static QColor iceFromHue(int hue);

Q_SIGNALS:
    void paletteChanged(const QPalette& p);
    void accentChanged(const AccentOverride& o);
    void themeLoaded(const QString& path);
    void loadFailed(LoadError reason, const QString& detail);

private:
    TruthGramTheme();
    ~TruthGramTheme() override;
    TruthGramTheme(const TruthGramTheme&) = delete;
    TruthGramTheme& operator=(const TruthGramTheme&) = delete;

    struct State;
    std::unique_ptr<State> _s;
};

// Convenience free functions.
[[nodiscard]] inline TruthGramTheme& theme() { return TruthGramTheme::instance(); }

} // namespace tg::truthgram::theme
