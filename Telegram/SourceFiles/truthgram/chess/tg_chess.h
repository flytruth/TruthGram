// tg_chess.h
//
// TruthGram built-in P2P chess. A lightweight, serverless board lives in a
// collapsible side-drawer. Moves are serialized as compact, human-readable
// algebraic strings OR zero-width-encoded move tags and sent directly in the
// chat, so two TruthGram users can play over standard Telegram messages with
// no central game server.
//
// Wire format (sent as a normal text message):
//   Visible mode:   "TGchess:e2e4|black|2"
//   Steganographic: "Hi! " + ZWSP-encoded "e2e4" + " how are you?"
//     Zero-width chars: U+200B=0, U+200C=1, U+200D=sep, U+2060=EOL
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#pragma once

#include "truthgram_config.h"

#include <QObject>
#include <QString>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace tg::truthgram::chess {

enum class Color : std::uint8_t { White, Black };

enum class Piece : std::uint8_t {
    None,
    WPawn, WRook, WKnight, WBishop, WQueen, WKing,
    BPawn, BRook, BKnight, BBishop, BQueen, BKing,
};

// 8x8 board. Index 0 = a1, 63 = h8. (file = idx % 8, rank = idx / 8).
using Board = std::array<Piece, 64>;

struct Move {
    std::uint8_t from{};
    std::uint8_t to{};
    // Promotion piece if the move is a pawn reaching the last rank.
    Piece promo{Piece::None};
};

enum class GameState {
    AwaitingOpponent,
    InProgress,
    Checkmate,
    Stalemate,
    Draw,
    Aborted,
};

struct GameSnapshot {
    Board board;
    Color toMove{Color::White};
    GameState state{GameState::AwaitingOpponent};
    int moveCount{0};
    qint64 whitePeerId{0};
    qint64 blackPeerId{0};
};

Q_NAMESPACE

// =============================================================================
// TgChess
// =============================================================================
class TgChess : public QObject {
    Q_OBJECT
public:
    static TgChess& instance();

    // ---- Wire format ----
    // Encode a move into the visible "TGchess:..." packet.
    [[nodiscard]] static QString encodeVisible(qint64 gameId, const Move& m, Color side, int ply);
    // Encode a move steganographically into a cover text using zero-width
    // characters. Returns the cover text with the move bits embedded.
    [[nodiscard]] static QString encodeSteganographic(const QString& cover, const Move& m);
    // Detect + decode either format from an incoming chat message. Returns
    // std::nullopt if the message is not a chess packet.
    [[nodiscard]] static std::optional<std::pair<qint64, Move>> tryDecode(const QString& msg);

    // ---- Board logic ----
    // Apply a move to the given snapshot, returning the new state. Rejects
    // illegal moves (returns the snapshot unchanged with state=Aborted).
    [[nodiscard]] static GameSnapshot applyMove(const GameSnapshot& s, const Move& m);

    // Generate all pseudo-legal moves for the side to move. The UI uses this
    // to highlight destination squares when the user picks up a piece.
    [[nodiscard]] static std::vector<Move> pseudoLegalMoves(const GameSnapshot& s);

    // FEN serialization for persistence + cloud_sync.
    [[nodiscard]] static QString toFen(const GameSnapshot& s);
    [[nodiscard]] static GameSnapshot fromFen(const QString& fen);

    // Algebraic helpers used by the UI / wire format.
    [[nodiscard]] static QString moveToAlgebraic(const Move& m);
    [[nodiscard]] static std::optional<Move> algebraicToMove(const QString& s);

Q_SIGNALS:
    void moveApplied(qint64 gameId, const Move& m, const GameSnapshot& s);
    void gameEnded(qint64 gameId, GameState finalState);

private:
    TgChess();
    ~TgChess() override;
    TgChess(const TgChess&) = delete;
    TgChess& operator=(const TgChess&) = delete;

    struct State;
    std::unique_ptr<State> _s;
};

inline TgChess& chess() { return TgChess::instance(); }

} // namespace tg::truthgram::chess
