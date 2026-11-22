// frosted_sidebar.h
//
// TruthGram frosted-glass sidebar. A translucent panel that renders the chat
// list against a blurred backdrop of whatever sits behind the main window.
// On macOS this would defer to NSVisualEffectView via platform native code;
// here we implement the cross-platform fallback using QGraphicsBlurEffect on
// a backing widget that captures the parent's pixels.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QWidget>
#include <QPointer>

#include <memory>

class QGraphicsBlurEffect;

namespace tg::truthgram::ios_ui {

class FrostedSidebar : public QWidget {
    Q_OBJECT
public:
    explicit FrostedSidebar(QWidget* parent);
    ~FrostedSidebar() override;

    void setFrost(bool on);
    [[nodiscard]] bool frost() const;

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    struct Impl;
    std::unique_ptr<Impl> _i;
};

} // namespace tg::truthgram::ios_ui
