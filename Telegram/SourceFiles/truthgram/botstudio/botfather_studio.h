// botfather_studio.h
//
// TruthGram Visual BotFather Builder. Detects when the user opens @BotFather
// or clicks a "Bot Studio" button in the top bar, then presents an interactive
// visual overlay that translates natural-language prompts or drag-and-drop
// inline-keyboard layouts into exact @BotFather commands (/newbot,
// /setcommands, /setmenubutton, /setdescription) and generates ready-to-copy
// JSON / Python / Node.js webhook templates.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

#include <memory>
#include <vector>

namespace tg::truthgram::botstudio {

// One row of inline keyboard buttons in the visual builder.
struct KbButton {
    QString text;
    QString callbackData;     // OR
    QString url;              // OR
    QString switchInlineQuery;
    enum class Style : std::uint8_t { Default, Lime, Ice } style{Style::Default};
};
using KbRow = std::vector<KbButton>;

struct BotSpec {
    QString name;             // display name
    QString username;         // must end in "bot"
    QString description;      // /setdescription text
    QString aboutText;        // /setabouttext
    QString commandsJson;     // /setcommands payload (JSON array)
    QString menuButtonLabel;
    QString menuButtonUrl;
    std::vector<KbRow> inlineKeyboard;
    QString webhookUrl;       // for template generation
};

Q_NAMESPACE

// =============================================================================
// BotFatherStudio
// =============================================================================
class BotFatherStudio : public QObject {
    Q_OBJECT
public:
    static BotFatherStudio& instance();

    // Open the visual builder overlay for the given bot spec (edit existing
    // or start a new one).
    void openFor(const BotSpec& spec);

    // Translate a natural-language prompt ("a bot that posts daily weather")
    // into a starter BotSpec. This is a local heuristic generator; no LLM
    // network call is made.
    [[nodiscard]] BotSpec specFromPrompt(const QString& prompt) const;

    // Emit the exact @BotFather command sequence as a list of ready-to-send
    // strings. The UI shows them as copy buttons.
    [[nodiscard]] std::vector<QString> botfatherCommands(const BotSpec& spec) const;

    // Generate a ready-to-copy webhook handler template in the requested
    // language. The template uses python-telegram-bot / Telegraf idioms.
    [[nodiscard]] QString webhookTemplate(const BotSpec& spec,
        const QString& language /* "python" | "node" | "json" */) const;

    // Serialize the inline keyboard as JSON (Telegram Bot API format).
    [[nodiscard]] QJsonArray inlineKeyboardJson(const std::vector<KbRow>& kb) const;

Q_SIGNALS:
    void builderOpened(qint64 botPeerId);
    void commandsReady(const std::vector<QString>& commands);
    void templateReady(const QString& language, const QString& code);

private:
    BotFatherStudio();
    ~BotFatherStudio() override;
    BotFatherStudio(const BotFatherStudio&) = delete;
    BotFatherStudio& operator=(const BotFatherStudio&) = delete;

    struct State;
    std::unique_ptr<State> _s;
};

inline BotFatherStudio& botStudio() { return BotFatherStudio::instance(); }

} // namespace tg::truthgram::botstudio
