// truthgram_pch.h
//
// Precompiled header for the tg_truthgram target. Includes the heavy,
// stable headers that nearly every TruthGram module pulls in. Keeping this
// small and stable makes incremental rebuilds cheap.
#pragma once

// Standard library — the bits every TruthGram TU ends up needing.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// Qt Core/Gui/Widgets — included by every TruthGram UI module.
#include <QObject>
#include <QString>
#include <QColor>
#include <QPalette>
#include <QGuiApplication>
#include <QWidget>
#include <QHash>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QPointer>

// TruthGram config + version (always available).
#include "truthgram_config.h"
#include "truthgram_version.h"
