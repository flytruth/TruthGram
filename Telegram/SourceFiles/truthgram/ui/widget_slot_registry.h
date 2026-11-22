// widget_slot_registry.h
//
// Header-only registry that maps a WidgetSlot enum to a QWidget container
// owned by the main UI. Plugins inject widgets through HostApi::inject_widget,
// which calls into this registry to reparent and place the widget.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram/plugins/plugin_sdk.h"

#include <QWidget>
#include <QHash>
#include <QPointer>
#include <QBoxLayout>

#include <mutex>

namespace tg::truthgram::ui {

class WidgetSlotRegistry {
public:
    static WidgetSlotRegistry& instance() {
        static WidgetSlotRegistry s;
        return s;
    }

    // Bind a container widget to a slot. The registry takes no ownership of
    // the container; the main UI retains it. Subsequent inject_widget calls
    // reparent into this container.
    void bind(plugin::WidgetSlot slot, QWidget* container, QBoxLayout* layout) {
        std::lock_guard lk(_mu);
        _containers[int(slot)] = container;
        _layouts[int(slot)]    = layout;
    }

    // Inject a plugin widget into a bound slot. Returns false if the slot is
    // unbound. The plugin retains C++ ownership of the widget; we reparent it
    // into the container but do not delete it on removeWidget.
    bool inject(plugin::WidgetSlot slot, QWidget* widget) {
        if (!widget) return false;
        std::lock_guard lk(_mu);
        auto it = _containers.find(int(slot));
        if (it == _containers.end() || !*it) return false;
        widget->setParent(*it);
        auto lit = _layouts.find(int(slot));
        if (lit != _layouts.end() && *lit) {
            (*lit)->addWidget(widget);
        } else {
            widget->move(0, 0);
        }
        widget->show();
        return true;
    }

    void remove(QWidget* widget) {
        if (!widget) return;
        std::lock_guard lk(_mu);
        widget->setParent(nullptr);
        widget->hide();
    }

private:
    WidgetSlotRegistry() = default;
    std::mutex _mu;
    QHash<int, QPointer<QWidget>>   _containers;
    QHash<int, QPointer<QBoxLayout>> _layouts;
};

} // namespace tg::truthgram::ui
