// truthgram_main.cpp
//
// Standalone entry point for `truthgram_app` when the upstream tdesktop
// checkout is NOT vendored. It builds a minimal Qt main window, hands it to
// tg::truthgram::initialize() (which wires every TruthGram subsystem - theme,
// iOS UI engine, screen shield, ghost / phishing / webapp / osint singletons,
// voice changer, chess, BotFather studio, Termux bridge, plugin discovery),
// runs the Qt event loop, and shuts down cleanly on exit.
//
// In the real overlay this role is played by tdesktop's Core::Launcher /
// MainWidget; this file exists so `cmake --build . --target truthgram_app`
// produces a runnable binary on Arch Linux without cloning the whole of
// tdesktop.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/truthgram_init.h"
#include "truthgram/theme/truthgram_theme.h"

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QFont>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TruthGram"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("TruthGram"));

    // Apply the built-in Cyber-Truth palette to qApp BEFORE any widget is
    // constructed, so the very first paint is on-brand. initialize() will
    // re-load it (idempotent) once the theme engine is up.
    tg::truthgram::theme::TruthGramTheme::instance().loadBuiltinCyberTruth();

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("TruthGram"));
    window.resize(960, 640);

    auto* central = new QWidget(&window);
    central->setStyleSheet(QStringLiteral(
        "background:#0A0C0E;"
        "color:#E6EDF3;"
        "font-size:15px;"));
    auto* layout = new QVBoxLayout(central);
    auto* label = new QLabel(QStringLiteral(
        "TruthGram - standalone shell\n"
        "Every subsystem is initialized; "
        "drop plugins into ~/.local/share/TruthGram/plugins/"));
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label, 0, Qt::AlignCenter);
    window.setCentralWidget(central);

    // Derive a deterministic 32-byte auth key for the standalone shell. In the
    // real overlay this comes from the MTProto auth_key; here we use a fixed
    // seed so the SQLCipher DB key + cloud_sync AES key are stable across
    // runs of the demo (so your saved state survives restarts).
    unsigned char authKey[32] = {0};
    for (int i = 0; i < 32; ++i)
        authKey[i] = static_cast<unsigned char>((i * 7 + 1) & 0xFF);

    tg::truthgram::initialize(&window, authKey);
    window.show();

    const int rc = app.exec();
    tg::truthgram::shutdown();
    return rc;
}
