# TruthGram — Privacy-First Telegram Business Bot

> Catch what gets deleted. Block what shouldn't be said. Keep it all on your machine open-source and free with privacy focused system.
>
> A self-hosted **Telegram Business Bot** that:

- 🔍 **Filters English & Russian profanity** *before* any AI sees the message
- 🗑️ **Logs messages the sender later deletes** — encrypted at rest, owner-only
- 🤖 **Optionally replies with a local LLM** (Ollama) — no cloud calls by default
- 🔐 **Privacy-first by design** — SQLite + Fernet encryption, structured redacted logs

[![Python 3.11+](https://img.shields.io/badge/python-3.11%2B-blue.svg)](https://www.python.org/)
[![python-telegram-bot v21](https://img.shields.io/badge/python--telegram--bot-v21-26A5E4.svg)](https://github.com/python-telegram-bot/python-telegram-bot)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Docker](https://img.shields.io/badge/docker-ready-2496ED.svg)](docker-compose.yml)
[![Privacy: local-first](https://img.shields.io/badge/privacy-local--first-success.svg)](#-privacy-model)

## Why

Telegram's ordinary Bot API **cannot see deleted messages** — there is no
"message deleted" update for regular bots. Only the **Business Bot API**
exposes `updateBotDeleteBusinessMessage`. This project uses that path, so the
log is real and not a hack.

The trade-off: the bot must be connected to your account as a Business Bot.
Everything else — filtering, storage, inference — stays on your hardware.

## Features

| Feature | Status | Notes |
|---|---|---|
| EN/RU profanity filter | ✅ | Normalizer handles lookalikes, leetspeak, zero-width chars |
| Deleted-message log | ✅ | Encrypted (Fernet), owner-only, auto-purge on retention |
| Owner commands | ✅ | `/deleted` `/purge` `/blocklist` `/status` `/pause` `/resume` |
| Local LLM replies | ⚙️ Optional | Ollama, `ENABLE_AI=true`, 20s timeout |
| Cloud AI | ❌ Never | Outbound calls restricted to Telegram API + local Ollama |
| Redaction before inference | ✅ | Phones, emails, card-like numbers stripped |
| Docker | ✅ | `docker-compose up` and go |
| Tests | ✅ | Normalizer, encryption round-trip, owner guard |

## Privacy model (read this)

- **What is stored:** deleted-message text (encrypted), filter-hit events
  (category only — never the raw word), timestamps, chat IDs.
- **Where:** local SQLite file on your machine / your volume.
- **Who can read it:** only `OWNER_ID` via `/deleted`.
- **Encryption key:** `LOG_ENCRYPTION_KEY` env var — **back it up**. Lose it,
  lose the log.
- **Retention:** `RETENTION_DAYS` (default 7). `/purge` enforces it.
- **Outbound traffic:** Telegram API only, plus `OLLAMA_URL` if AI is enabled.
  A test fails the build if any other host appears in source.

⚠️ **Not a spy tool.** This logs messages *sent to the owner* and later
deleted *in chats the owner is part of*. It does not intercept third-party
chats. Use it on accounts you own.


To create a Telegram bot token, message @BotFather on Telegram, use the command /newbot, and follow the prompts to choose a name and username for your bot. After completing these steps, BotFather will provide you with a unique bot token that you can use to authenticate your bot

## Quick start

```bash
git clone https://github.com/flytruth/TruthGram
cd TruthGram
cp .env.example .env      # fill BOT_TOKEN, OWNER_ID; keep the generated key
docker compose up -d
