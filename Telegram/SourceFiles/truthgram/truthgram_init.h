// truthgram_init.h
//
// TruthGram startup bootstrap. Called once from tdesktop's MainWidget
// constructor (or, in a standalone build, from main()). Wires every TruthGram
// subsystem together in the correct order and applies the saved RuntimeConfig.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

class QWidget;

namespace tg::truthgram {

// One-shot initialization. Safe to call exactly once per process.
//   mainWindow: the tdesktop MainWindow; TruthGram installs the iOS UI
//   engine + screen shield onto it.
//   authKey: 32-byte MTProto auth key, used to derive the cloud_sync AES key
//   and unlock the SQLCipher DB.
void initialize(QWidget* mainWindow, const unsigned char authKey[32]);

// Persist dirty state, flush cloud_sync, unload plugins.
void shutdown();

} // namespace tg::truthgram
