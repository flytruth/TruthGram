// botfather_studio.cpp
//
// TruthGram Visual BotFather Builder implementation.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/botstudio/botfather_studio.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QDebug>

#include <unordered_map>

namespace tg::truthgram::botstudio {

namespace {

// Keyword -> starter command set for the heuristic prompt generator.
struct PromptTemplate {
    const char* keyword;
    std::vector<std::pair<QString, QString>> commands; // (command, description)
};

const PromptTemplate kPromptTemplates[] = {
    {"weather", {{"weather", "Get the weather for a city"},
                 {"forecast", "Get the 3-day forecast"},
                 {"units", "Toggle C/F units"}}},
    {"reminder", {{"remind", "Set a reminder: /remind <time> <text>"},
                  {"list", "List pending reminders"},
                  {"cancel", "Cancel a reminder by id"}}},
    {"poll",   {{"poll", "Create a poll"},
                {"vote", "Vote on an option"},
                {"results", "Show current results"}}},
    {"todo",   {{"add", "Add a todo item"},
                {"done", "Mark done"},
                {"list", "List todos"}}},
    {"news",   {{"latest", "Latest headlines"},
                {"subscribe", "Subscribe to a topic"},
                {"unsubscribe", "Unsubscribe"}}},
};

} // namespace

struct BotFatherStudio::State {
    BotSpec current;
};

BotFatherStudio& BotFatherStudio::instance() {
    static BotFatherStudio s;
    return s;
}

BotFatherStudio::BotFatherStudio()
    : QObject(nullptr), _s(std::make_unique<State>()) {}
BotFatherStudio::~BotFatherStudio() = default;

void BotFatherStudio::openFor(const BotSpec& spec) {
    _s->current = spec;
    Q_EMIT builderOpened(0);
}

BotSpec BotFatherStudio::specFromPrompt(const QString& prompt) const {
    BotSpec spec;
    spec.name = QStringLiteral("My TruthGram Bot");
    spec.username = QStringLiteral("my_truthgram_bot");
    spec.description = prompt;

    // Match keywords and seed the command list.
    QJsonArray cmds;
    bool matched = false;
    const QString lower = prompt.toLower();
    for (const auto& t : kPromptTemplates) {
        if (lower.contains(QString::fromLatin1(t.keyword))) {
            matched = true;
            for (const auto& [cmd, desc] : t.commands) {
                QJsonObject o;
                o["command"] = cmd;
                o["description"] = desc;
                cmds.append(o);
            }
            break;
        }
    }
    if (!matched) {
        // Generic starter.
        for (const auto& [cmd, desc] : kPromptTemplates[3].commands) {
            QJsonObject o;
            o["command"] = cmd;
            o["description"] = desc;
            cmds.append(o);
        }
    }
    spec.commandsJson = QString::fromUtf8(
        QJsonDocument(cmds).toJson(QJsonDocument::Compact));
    return spec;
}

std::vector<QString> BotFatherStudio::botfatherCommands(const BotSpec& spec) const {
    std::vector<QString> out;
    out.reserve(8);
    out.push_back(QStringLiteral("/newbot"));
    out.push_back(spec.name);
    out.push_back(spec.username);
    if (!spec.description.isEmpty()) {
        out.push_back(QStringLiteral("/setdescription"));
        out.push_back(spec.description);
    }
    if (!spec.commandsJson.isEmpty()) {
        out.push_back(QStringLiteral("/setcommands"));
        out.push_back(spec.commandsJson);
    }
    if (!spec.menuButtonLabel.isEmpty() || !spec.menuButtonUrl.isEmpty()) {
        out.push_back(QStringLiteral("/setmenubutton"));
        QJsonObject btn;
        btn["text"] = spec.menuButtonLabel;
        btn["url"]  = spec.menuButtonUrl;
        out.push_back(QString::fromUtf8(
            QJsonDocument(btn).toJson(QJsonDocument::Compact)));
    }
    if (!spec.aboutText.isEmpty()) {
        out.push_back(QStringLiteral("/setabouttext"));
        out.push_back(spec.aboutText);
    }
    Q_EMIT commandsReady(out);
    return out;
}

QString BotFatherStudio::webhookTemplate(const BotSpec& spec,
                                          const QString& language) const {
    if (language == "python") {
        return QStringLiteral(
            "# TruthGram-generated python-telegram-bot webhook template\n"
            "from telegram import Update, InlineKeyboardButton, InlineKeyboardMarkup\n"
            "from telegram.ext import Application, CommandHandler, CallbackQueryHandler\n"
            "import os\n\n"
            "TOKEN = os.environ[\"BOT_TOKEN\"]\n"
            "WEBHOOK_URL = \"%1\"\n\n"
            "async def start(update, ctx):\n"
            "    await update.message.reply_text(\"%2\")\n\n"
            "async def on_button(update, ctx):\n"
            "    q = update.callback_query\n"
            "    await q.answer()\n"
            "    await q.edit_message_text(f\"You pressed: {q.data}\")\n\n"
            "def main():\n"
            "    app = Application.builder().token(TOKEN).build()\n"
            "    app.add_handler(CommandHandler(\"start\", start))\n"
            "    app.add_handler(CallbackQueryHandler(on_button))\n"
            "    app.run_webhook(listen=\"0.0.0.0\", port=8080,\n"
            "                     webhook_url=WEBHOOK_URL)\n\n"
            "if __name__ == \"__main__\":\n"
            "    main()\n"
        ).arg(spec.webhookUrl, spec.description);
    }
    if (language == "node") {
        return QStringLiteral(
            "// TruthGram-generated Telegraf webhook template\n"
            "const { Telegraf, Markup } = require('telegraf');\n"
            "const bot = new Telegraf(process.env.BOT_TOKEN);\n\n"
            "bot.start((ctx) => ctx.reply('%1'));\n"
            "bot.on('callback_query', (ctx) => ctx.answerCbQuery(`You pressed: ${ctx.callbackQuery.data}`));\n\n"
            "bot.telegram.setWebhook('%2');\n"
            "bot.startWebhook({ port: 8080 });\n"
        ).arg(spec.description, spec.webhookUrl);
    }
    // JSON: just the command + keyboard payload.
    QJsonObject root;
    root["bot"] = spec.username;
    root["commands"] = QJsonDocument::fromJson(spec.commandsJson.toUtf8()).array();
    root["inline_keyboard"] = inlineKeyboardJson(spec.inlineKeyboard);
    root["webhook_url"] = spec.webhookUrl;
    Q_EMIT templateReady(language, QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Indented)));
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QJsonArray BotFatherStudio::inlineKeyboardJson(const std::vector<KbRow>& kb) const {
    QJsonArray rows;
    for (const auto& row : kb) {
        QJsonArray r;
        for (const auto& b : row) {
            QJsonObject o;
            o["text"] = b.text;
            if (!b.callbackData.isEmpty())      o["callback_data"] = b.callbackData;
            else if (!b.url.isEmpty())           o["url"] = b.url;
            else if (!b.switchInlineQuery.isEmpty()) o["switch_inline_query"] = b.switchInlineQuery;
            r.append(o);
        }
        rows.append(r);
    }
    return rows;
}

} // namespace tg::truthgram::botstudio
