// hello_plugin.cpp
//
// TruthGram Plugin SDK example. A minimal native plugin that:
//   * registers one Setting row in Settings > TruthGram > Plugins,
//   * subscribes to the ChatOpened event,
//   * injects a tiny QLabel into the chat header left slot.
//
// Build:
//   cmake -S . -B build && cmake --build build
// Install:
//   cp build/libhello_plugin.so ~/.local/share/TruthGram/plugins/hello_plugin/
//   cp manifest.json            ~/.local/share/TruthGram/plugins/hello_plugin/
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/plugins/plugin_sdk.h"

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>

#include <cstdio>

using namespace tg::truthgram::plugin;

namespace {

// The HostApi pointer is captured here at entry time. It is stable for the
// lifetime of the loaded library (the host guarantees this).
const HostApi* g_api = nullptr;

// A small Qt widget we inject into the chat header. We own it; the host
// reparents it into the ChatHeaderLeft container.
class HelloPill : public QWidget {
public:
    explicit HelloPill(const HostApi* api) : QWidget(nullptr), _api(api) {
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(8, 2, 8, 2);
        _label = new QLabel(QStringLiteral("\xF0\x9F\x91\x8B Hello from a plugin!"), this);
        _label->setStyleSheet("color:#39FF14; font-weight:600;");
        lay->addWidget(_label);
        // Re-render the chat title every time ChatOpened fires.
        _blink = new QTimer(this);
        _blink->setInterval(1200);
        _blink->start();
        connect(_blink, &QTimer::timeout, this, [this] {
            _on = !_on;
            _label->setVisible(_on);
        });
    }
private:
    const HostApi* _api;
    QLabel* _label{nullptr};
    QTimer* _blink{nullptr};
    bool _on{true};
};

HelloPill* g_pill = nullptr;

} // namespace

// ---------------------------------------------------------------------------
// Plugin C entry points
// ---------------------------------------------------------------------------
extern "C" {

std::uint32_t truthgram_plugin_api_version() {
    return kApiVersion;
}

int truthgram_plugin_entry(const HostApi* api) {
    g_api = api;
    if (!api || api->api_version != kApiVersion) {
        // ABI mismatch; refuse to load.
        return 1;
    }
    api->log(LogLevel::Info, "hello_plugin: entry");

    // Register one setting the user can toggle.
    Setting s{};
    s.key = "show_pill";
    s.label = "Show hello pill in chat header";
    s.tooltip = nullptr;
    s.kind = Setting::Kind::Toggle;
    s.intValue = 1;
    api->register_setting(&s);

    // Subscribe to ChatOpened so we can refresh the greeting.
    auto cb = [](void* /*subscriber*/, const EventPayload* ev) {
        if (!g_api) return;
        if (ev->type == EventType::ChatOpened) {
            g_api->log(LogLevel::Verbose, "hello_plugin: chat opened");
        }
    };
    api->subscribe(EventType::ChatOpened, cb, nullptr);
    return 0;
}

void truthgram_plugin_enable() {
    if (!g_api) return;
    g_api->log(LogLevel::Info, "hello_plugin: enabled");
    if (!g_pill) {
        g_pill = new HelloPill(g_api);
        g_api->inject_widget(WidgetSlot::ChatHeaderLeft, g_pill);
    }
}

void truthgram_plugin_disable() {
    if (!g_api) return;
    g_api->log(LogLevel::Info, "hello_plugin: disabled");
    if (g_pill) {
        g_api->remove_widget(g_pill);
        delete g_pill;
        g_pill = nullptr;
    }
}

void truthgram_plugin_shutdown() {
    if (!g_api) return;
    if (g_pill) {
        g_api->remove_widget(g_pill);
        delete g_pill;
        g_pill = nullptr;
    }
    g_api->log(LogLevel::Info, "hello_plugin: shutdown");
    g_api = nullptr;
}

} // extern "C"
