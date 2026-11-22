// truthgram_theme.cpp
//
// Implementation of the TruthGram "Cyber-Truth" theme engine.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/theme/truthgram_theme.h"

#include <QGuiApplication>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QColor>
#include <QPalette>
#include <QSet>

#include <cmath>

namespace tg::truthgram::theme {

namespace {

// Parse a single "#RRGGBB" or "#RRGGBBAA" string into a QColor. Returns
// std::nullopt on malformed input — we never want a partial color.
std::optional<QColor> parseColor(const QString& raw) {
    auto s = raw.trimmed();
    if (!s.startsWith('#')) return std::nullopt;
    s = s.mid(1);
    bool ok = false;
    if (s.length() == 6) {
        const auto rgb = s.toUInt(&ok, 16);
        if (!ok) return std::nullopt;
        return QColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }
    if (s.length() == 8) {
        const auto rgba = s.toULongLong(&ok, 16);
        if (!ok) return std::nullopt;
        return QColor((rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
                      (rgba >> 8) & 0xFF, rgba & 0xFF);
    }
    return std::nullopt;
}

// Parse an integer (used by tg_bubble_radius, tg_spring_damping, etc.).
std::optional<int> parseInteger(const QString& raw) {
    bool ok = false;
    const int v = raw.trimmed().toInt(&ok);
    return ok ? std::optional<int>(v) : std::nullopt;
}

std::optional<double> parseDouble(const QString& raw) {
    bool ok = false;
    const double v = raw.trimmed().toDouble(&ok);
    return ok ? std::optional<double>(v) : std::nullopt;
}

} // namespace

struct TruthGramTheme::State {
    mutable QRecursiveMutex mu;

    QHash<QString, ThemeEntry> entries;
    TruthGramExtras extras{};
    AccentOverride override_;
    QPalette palette;
    QString currentPath;
    std::atomic<bool> loadedFlag{false};

    // Cache of role colors for fast lookup.
    QColor windowBg{QColor(0x0A, 0x0C, 0x0E)};
    QColor accent{QColor(0x39, 0xFF, 0x14)};
    QColor accentDim{QColor(0x84, 0xCC, 0x16)};
    QColor link{QColor(0x38, 0xBD, 0xF8)};
    QColor linkDim{QColor(0x7D, 0xD3, 0xFC)};
    QColor textFg{QColor(0xE6, 0xED, 0xF3)};
    QColor subTextFg{QColor(0x7D, 0x85, 0x90)};

    QColor lookup(const char* key, const QColor& fallback) const {
        const auto it = entries.find(QString::fromLatin1(key));
        if (it == entries.end()) return fallback;
        return it->color.isValid() ? it->color : fallback;
    }

    // Re-derive the public role colors from the loaded entries, applying the
    // AccentOverride on top. Called under mu.
    void recomputeRoleColors() {
        windowBg  = lookup("windowBg",      QColor(0x0A, 0x0C, 0x0E));
        textFg    = lookup("windowFg",      QColor(0xE6, 0xED, 0xF3));
        subTextFg = lookup("windowSubTextFg", QColor(0x7D, 0x85, 0x90));

        QColor baseAccent    = lookup("activeButtonBg", QColor(0x39, 0xFF, 0x14));
        QColor baseAccentDim = lookup("activeButtonBgOver", QColor(0x84, 0xCC, 0x16));
        QColor baseLink      = lookup("lightButtonFg", QColor(0x38, 0xBD, 0xF8));
        QColor baseLinkDim   = lookup("lightButtonFgOver", QColor(0x7D, 0xD3, 0xFC));

        accent    = override_.accent    ? *override_.accent    : baseAccent;
        accentDim = override_.accentDim ? *override_.accentDim : baseAccentDim;
        link      = override_.link      ? *override_.link      : baseLink;
        linkDim   = override_.linkDim   ? *override_.linkDim   : baseLinkDim;
    }

    // Build the QPalette we apply to qApp. Maps the brand colors onto Qt's
    // palette roles so that standard widgets (menus, buttons, scrollbars)
    // render correctly without per-widget code.
    QPalette buildQPalette() const {
        QPalette p;
        const QColor bg = windowBg;
        const QColor bgOver = lookup("windowBgOver", QColor(0x11, 0x14, 0x18));
        const QColor fg = textFg;
        const QColor sub = subTextFg;

        p.setColor(QPalette::Window,          bg);
        p.setColor(QPalette::Base,            bg);
        p.setColor(QPalette::AlternateBase,   bgOver);
        p.setColor(QPalette::WindowText,      fg);
        p.setColor(QPalette::Text,            fg);
        p.setColor(QPalette::Button,          bgOver);
        p.setColor(QPalette::ButtonText,      fg);
        p.setColor(QPalette::BrightText,      accent);
        p.setColor(QPalette::Highlight,       accent);
        p.setColor(QPalette::HighlightedText, bg);     // lime bg + obsidian text
        p.setColor(QPalette::ToolTipBase,     bgOver);
        p.setColor(QPalette::ToolTipText,     fg);
        p.setColor(QPalette::Link,            link);
        p.setColor(QPalette::LinkVisited,     linkDim);
        p.setColor(QPalette::PlaceholderText, sub);
        p.setColor(QPalette::Light,           accent);
        p.setColor(QPalette::Midlight,        accentDim);
        p.setColor(QPalette::Mid,             bgOver);
        p.setColor(QPalette::Dark,            QColor(0x00, 0x00, 0x00));
        p.setColor(QPalette::Shadow,          QColor(0x00, 0x00, 0x00, 0x80));

        // Disabled state — readable but clearly inactive.
        const QColor disabledFg = sub.darker(120);
        p.setColor(QPalette::Disabled, QPalette::WindowText, disabledFg);
        p.setColor(QPalette::Disabled, QPalette::Text,       disabledFg);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledFg);
        return p;
    }

    // Parse TruthGram tg_* extras into the extras struct.
    void applyExtras() {
        extras.frostBlurColor   = lookup("windowBgOver",
            QColor(0x11, 0x14, 0x18, 0xC0));
        extras.ghostBarBg       = lookup("tg_ghost_bar_bg",
            QColor(0x16, 0x1A, 0x1F));
        extras.ghostBarBorder   = lookup("tg_ghost_bar_border",
            QColor(0x1C, 0x21, 0x28));
        extras.osintMetaBg      = lookup("tg_osint_meta_bg",
            QColor(0x0F, 0x12, 0x16));
        extras.osintMetaBorder  = lookup("tg_osint_meta_border",
            QColor(0x1C, 0x21, 0x28));
        extras.miniPlayerFftLow = lookup("tg_miniplayer_fft_low",
            QColor(0x39, 0xFF, 0x14));
        extras.miniPlayerFftHigh= lookup("tg_miniplayer_fft_high",
            QColor(0x38, 0xBD, 0xF8));

        if (auto v = parseInteger(entries.value("tg_frost_blur_radius").raw))
            extras.frostBlurRadius = *v;
        if (auto v = parseInteger(entries.value("tg_bubble_radius").raw))
            extras.bubbleRadius = *v;
        if (auto v = parseDouble(entries.value("tg_spring_damping").raw))
            extras.springDamping = *v;
        if (auto v = parseDouble(entries.value("tg_spring_stiffness").raw))
            extras.springStiffness = *v;
    }
};

TruthGramTheme& TruthGramTheme::instance() {
    static TruthGramTheme s;
    return s;
}

TruthGramTheme::TruthGramTheme()
    : QObject(nullptr)
    , _s(std::make_unique<State>()) {
    // Seed the palette with the built-in Cyber-Truth defaults so widgets are
    // correct even before a .tdesktop-theme is explicitly loaded.
    _s->palette = _s->buildQPalette();
}

TruthGramTheme::~TruthGramTheme() = default;

LoadError TruthGramTheme::loadBuiltinCyberTruth() {
    return load(QStringLiteral(":/truthgram/themes/cyber-truth.tdesktop-theme"));
}

LoadError TruthGramTheme::load(const QString& path) {
    QString detail;
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        detail = QStringLiteral("file not found: %1").arg(path);
        Q_EMIT loadFailed(LoadError::FileNotFound, detail);
        return LoadError::FileNotFound;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        detail = QStringLiteral("cannot read: %1").arg(path);
        Q_EMIT loadFailed(LoadError::ReadFailed, detail);
        return LoadError::ReadFailed;
    }

    QHash<QString, ThemeEntry> next;
    bool inFontBlock = false;

    QTextStream in(&f);
    QString line;
    while (in.readLineInto(&line)) {
        const auto trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(';') || trimmed.startsWith("//"))
            continue;
        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            inFontBlock = (trimmed.compare("[Fonts]", Qt::CaseInsensitive) == 0);
            continue;
        }
        // Split on the first ':'.
        const auto idx = trimmed.indexOf(':');
        if (idx <= 0) {
            detail = QStringLiteral("malformed variable line: %1").arg(line);
            Q_EMIT loadFailed(LoadError::MalformedVariable, detail);
            return LoadError::MalformedVariable;
        }
        const auto key = trimmed.left(idx).trimmed();
        const auto raw = trimmed.mid(idx + 1).trimmed();

        ThemeEntry e;
        e.key = key;
        e.raw = raw;
        if (const auto c = parseColor(raw)) e.color = *c;
        if (const auto i = parseInteger(raw)) e.integer = *i;
        next.insert(key, std::move(e));
    }

    QMutexLocker lock(&_s->mu);
    _s->entries = std::move(next);
    _s->currentPath = path;
    _s->applyExtras();
    _s->recomputeRoleColors();
    _s->palette = _s->buildQPalette();
    _s->loadedFlag.store(true, std::memory_order_release);
    lock.unlock();

    applyToApplication();
    Q_EMIT themeLoaded(path);
    Q_EMIT paletteChanged(_s->palette);
    return LoadError::None;
}

void TruthGramTheme::applyToApplication() const {
    if (auto* app = qGuiApp) {
        QMutexLocker lock(&_s->mu);
        app->setPalette(_s->palette);
    }
}

void TruthGramTheme::setAccentOverride(const AccentOverride& o) {
    QMutexLocker lock(&_s->mu);
    _s->override_ = o;
    _s->recomputeRoleColors();
    _s->palette = _s->buildQPalette();
    lock.unlock();

    applyToApplication();
    Q_EMIT accentChanged(o);
    Q_EMIT paletteChanged(_s->palette);
    // Persist + cloud-sync is performed by cloud_sync, which subscribes to
    // accentChanged(). We do not touch SQLCipher here to keep the theme
    // engine free of storage coupling.
}

AccentOverride TruthGramTheme::accentOverride() const {
    QMutexLocker lock(&_s->mu);
    return _s->override_;
}

QColor TruthGramTheme::windowBg() const  { QMutexLocker l(&_s->mu); return _s->windowBg; }
QColor TruthGramTheme::accent() const    { QMutexLocker l(&_s->mu); return _s->accent; }
QColor TruthGramTheme::accentDim() const { QMutexLocker l(&_s->mu); return _s->accentDim; }
QColor TruthGramTheme::link() const      { QMutexLocker l(&_s->mu); return _s->link; }
QColor TruthGramTheme::linkDim() const   { QMutexLocker l(&_s->mu); return _s->linkDim; }
QColor TruthGramTheme::textFg() const    { QMutexLocker l(&_s->mu); return _s->textFg; }
QColor TruthGramTheme::subTextFg() const { QMutexLocker l(&_s->mu); return _s->subTextFg; }

const TruthGramExtras& TruthGramTheme::extras() const {
    QMutexLocker l(&_s->mu);
    return _s->extras;
}

QPalette TruthGramTheme::palette() const {
    QMutexLocker l(&_s->mu);
    return _s->palette;
}

bool TruthGramTheme::loaded() const {
    return _s->loadedFlag.load(std::memory_order_acquire);
}

QColor TruthGramTheme::limeFromHue(int hue) {
    // Keep the Lime brand S/V (≈ S=100%, V=100%) but shift the hue. Clamp to
    // a band around green/yellow-green so the result stays "lime-ish" even
    // when the slider drifts.
    hue = std::clamp(hue, 70, 160);
    QColor c;
    c.setHsv(hue, 255, 255);
    return c.toRgb();
}

QColor TruthGramTheme::iceFromHue(int hue) {
    // Light blue band: hue 180..220, S≈88%, V≈97%.
    hue = std::clamp(hue, 180, 220);
    QColor c;
    c.setHsv(hue, 224, 247);
    return c.toRgb();
}

} // namespace tg::truthgram::theme
