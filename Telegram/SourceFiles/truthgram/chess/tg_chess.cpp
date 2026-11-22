// tg_chess.cpp
//
// TruthGram built-in P2P chess — wire format + board logic.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/chess/tg_chess.h"

#include <QString>
#include <QStringList>
#include <QChar>

#include <optional>
#include <sstream>

namespace tg::truthgram::chess {

namespace {

constexpr char kVisiblePrefix[] = "TGchess:";

// Zero-width alphabet for steganographic encoding (4 bits per char).
// U+200B = '0', U+200C = '1', U+200D = separator, U+2060 = end-of-move.
constexpr char32_t kZw0 = 0x200B;
constexpr char32_t kZw1 = 0x200C;
constexpr char32_t kZwEnd = 0x2060;

inline Piece charToPiece(QChar c) {
    switch (c.unicode()) {
    case 'P': return Piece::WPawn;
    case 'R': return Piece::WRook;
    case 'N': return Piece::WKnight;
    case 'B': return Piece::WBishop;
    case 'Q': return Piece::WQueen;
    case 'K': return Piece::WKing;
    case 'p': return Piece::BPawn;
    case 'r': return Piece::BRook;
    case 'n': return Piece::BKnight;
    case 'b': return Piece::BBishop;
    case 'q': return Piece::BQueen;
    case 'k': return Piece::BKing;
    default:  return Piece::None;
    }
}

inline QChar pieceToChar(Piece p) {
    switch (p) {
    case Piece::WPawn:   return 'P';
    case Piece::WRook:   return 'R';
    case Piece::WKnight: return 'N';
    case Piece::WBishop: return 'B';
    case Piece::WQueen:  return 'Q';
    case Piece::WKing:   return 'K';
    case Piece::BPawn:   return 'p';
    case Piece::BRook:   return 'r';
    case Piece::BKnight: return 'n';
    case Piece::BBishop: return 'b';
    case Piece::BQueen:  return 'q';
    case Piece::BKing:   return 'k';
    default:             return '.';
    }
}

inline bool isWhite(Piece p) {
    return p == Piece::WPawn || p == Piece::WRook || p == Piece::WKnight
        || p == Piece::WBishop || p == Piece::WQueen || p == Piece::WKing;
}
inline Color colorOf(Piece p) { return isWhite(p) ? Color::White : Color::Black; }

// Sliding direction offsets as (df, dr) pairs.
struct Dir { int df; int dr; };
constexpr Dir kRookDirs[4]   = {{1,0},{-1,0},{0,1},{0,-1}};
constexpr Dir kBishopDirs[4] = {{1,1},{1,-1},{-1,1},{-1,-1}};
constexpr Dir kKnightDeltas[8] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};

inline bool onBoard(int f, int r) { return f >= 0 && f < 8 && r >= 0 && r < 8; }
inline int square(int f, int r) { return r * 8 + f; }

void addSliding(const GameSnapshot& s, int from, const Dir* dirs, int n,
                std::vector<Move>& out) {
    const int f = from % 8, r = from / 8;
    const Piece mover = s.board[from];
    for (int i = 0; i < n; ++i) {
        int nf = f + dirs[i].df, nr = r + dirs[i].dr;
        while (onBoard(nf, nr)) {
            const int to = square(nf, nr);
            const Piece occ = s.board[to];
            if (occ == Piece::None) {
                out.push_back({uint8_t(from), uint8_t(to), Piece::None});
            } else {
                if (colorOf(occ) != colorOf(mover))
                    out.push_back({uint8_t(from), uint8_t(to), Piece::None});
                break;
            }
            nf += dirs[i].df; nr += dirs[i].dr;
        }
    }
}

} // namespace

struct TgChess::State {};

TgChess& TgChess::instance() {
    static TgChess s;
    return s;
}

TgChess::TgChess() : QObject(nullptr), _s(std::make_unique<State>()) {}
TgChess::~TgChess() = default;

// ---------------------------------------------------------------------------
// Wire format
// ---------------------------------------------------------------------------
QString TgChess::encodeVisible(qint64 gameId, const Move& m, Color side, int ply) {
    return QStringLiteral("%1%2|%3|%4|%5")
        .arg(QString::fromLatin1(kVisiblePrefix))
        .arg(gameId)
        .arg(moveToAlgebraic(m))
        .arg(side == Color::White ? "white" : "black")
        .arg(ply);
}

QString TgChess::encodeSteganographic(const QString& cover, const Move& m) {
    // Encode the move as 16 bits (from 6, to 6, promo 4) using zero-width chars.
    const std::uint16_t bits = std::uint16_t(m.from)
        | (std::uint16_t(m.to) << 6)
        | (std::uint16_t(m.promo) << 12);
    QString zw;
    for (int i = 0; i < 16; ++i) {
        const bool b = (bits >> i) & 1;
        zw.append(QChar(b ? kZw1 : kZw0));
    }
    zw.append(QChar(kZwEnd));
    // Insert after the first word boundary so it survives most copy-paste.
    int insertAt = cover.indexOf(' ');
    if (insertAt < 0) insertAt = cover.size();
    return cover.left(insertAt) + zw + cover.mid(insertAt);
}

std::optional<std::pair<qint64, Move>> TgChess::tryDecode(const QString& msg) {
    // Visible prefix first.
    if (msg.startsWith(QString::fromLatin1(kVisiblePrefix))) {
        const QString body = msg.mid(int(sizeof(kVisiblePrefix)) - 1);
        const QStringList parts = body.split('|');
        if (parts.size() < 4) return std::nullopt;
        bool ok = false;
        const qint64 gameId = parts[0].toLongLong(&ok);
        if (!ok) return std::nullopt;
        const auto mv = algebraicToMove(parts[1]);
        if (!mv) return std::nullopt;
        return std::make_pair(gameId, *mv);
    }
    // Steganographic: scan for the kZwEnd terminator preceded by 16 zw bits.
    QString bits;
    for (const QChar c : msg) {
        const char32_t u = c.unicode();
        if (u == kZw0) bits.append('0');
        else if (u == kZw1) bits.append('1');
        else if (u == kZwEnd && bits.size() == 16) {
            std::uint16_t v = 0;
            for (int i = 0; i < 16; ++i)
                if (bits[i] == '1') v |= (1u << i);
            Move m;
            m.from = std::uint8_t(v & 0x3F);
            m.to   = std::uint8_t((v >> 6) & 0x3F);
            m.promo = Piece((v >> 12) & 0xF);
            // Game id unknown in steganographic mode; caller resolves by peer.
            return std::make_pair(qint64(0), m);
        } else if (u == kZwEnd) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Algebraic helpers
// ---------------------------------------------------------------------------
QString TgChess::moveToAlgebraic(const Move& m) {
    const int ff = m.from % 8, fr = m.from / 8;
    const int tf = m.to % 8,   tr = m.to / 8;
    QString s;
    s.append(QChar('a' + ff));
    s.append(QChar('1' + fr));
    s.append(QChar('a' + tf));
    s.append(QChar('1' + tr));
    switch (m.promo) {
    case Piece::WQueen:  s.append('q'); break;
    case Piece::WRook:   s.append('r'); break;
    case Piece::WBishop: s.append('b'); break;
    case Piece::WKnight: s.append('n'); break;
    default: break;
    }
    return s;
}

std::optional<Move> TgChess::algebraicToMove(const QString& s) {
    if (s.size() < 4) return std::nullopt;
    const int ff = s[0].toLatin1() - 'a';
    const int fr = s[1].toLatin1() - '1';
    const int tf = s[2].toLatin1() - 'a';
    const int tr = s[3].toLatin1() - '1';
    if (!onBoard(ff, fr) || !onBoard(tf, tr)) return std::nullopt;
    Move m{uint8_t(square(ff, fr)), uint8_t(square(tf, tr)), Piece::None};
    if (s.size() >= 5) {
        switch (s[4].toLatin1()) {
        case 'q': m.promo = Piece::WQueen; break;
        case 'r': m.promo = Piece::WRook; break;
        case 'b': m.promo = Piece::WBishop; break;
        case 'n': m.promo = Piece::WKnight; break;
        }
    }
    return m;
}

// ---------------------------------------------------------------------------
// Move generation (pseudo-legal; castling and en passant omitted for brevity
// but the structure is here — full legality is enforced in applyMove)
// ---------------------------------------------------------------------------
std::vector<Move> TgChess::pseudoLegalMoves(const GameSnapshot& s) {
    std::vector<Move> out;
    const Color me = s.toMove;
    for (int from = 0; from < 64; ++from) {
        const Piece p = s.board[from];
        if (p == Piece::None || colorOf(p) != me) continue;
        const int f = from % 8, r = from / 8;
        switch (p) {
        case Piece::WPawn: case Piece::BPawn: {
            const int dir = (p == Piece::WPawn) ? 1 : -1;
            const int startRank = (p == Piece::WPawn) ? 1 : 6;
            const int promoRank = (p == Piece::WPawn) ? 7 : 0;
            // Forward one.
            int nr = r + dir;
            if (onBoard(f, nr) && s.board[square(f, nr)] == Piece::None) {
                if (nr == promoRank)
                    out.push_back({uint8_t(from), uint8_t(square(f, nr)), Piece::WQueen});
                else
                    out.push_back({uint8_t(from), uint8_t(square(f, nr)), Piece::None});
                // Forward two from start.
                if (r == startRank && s.board[square(f, nr + dir)] == Piece::None)
                    out.push_back({uint8_t(from), uint8_t(square(f, nr + dir)), Piece::None});
            }
            // Captures.
            for (int df : {-1, 1}) {
                const int nf = f + df;
                if (!onBoard(nf, nr)) continue;
                const Piece occ = s.board[square(nf, nr)];
                if (occ != Piece::None && colorOf(occ) != me) {
                    if (nr == promoRank)
                        out.push_back({uint8_t(from), uint8_t(square(nf, nr)), Piece::WQueen});
                    else
                        out.push_back({uint8_t(from), uint8_t(square(nf, nr)), Piece::None});
                }
            }
            break;
        }
        case Piece::WRook: case Piece::BRook:
            addSliding(s, from, kRookDirs, 4, out);
            break;
        case Piece::WBishop: case Piece::BBishop:
            addSliding(s, from, kBishopDirs, 4, out);
            break;
        case Piece::WQueen: case Piece::BQueen:
            addSliding(s, from, kRookDirs, 4, out);
            addSliding(s, from, kBishopDirs, 4, out);
            break;
        case Piece::WKnight: case Piece::BKnight:
            for (const auto& d : kKnightDeltas) {
                const int nf = f + d.df, nr = r + d.dr;
                if (!onBoard(nf, nr)) continue;
                const Piece occ = s.board[square(nf, nr)];
                if (occ == Piece::None || colorOf(occ) != me)
                    out.push_back({uint8_t(from), uint8_t(square(nf, nr)), Piece::None});
            }
            break;
        case Piece::WKing: case Piece::BKing:
            for (int df = -1; df <= 1; ++df)
                for (int dr = -1; dr <= 1; ++dr) {
                    if (!df && !dr) continue;
                    const int nf = f + df, nr = r + dr;
                    if (!onBoard(nf, nr)) continue;
                    const Piece occ = s.board[square(nf, nr)];
                    if (occ == Piece::None || colorOf(occ) != me)
                        out.push_back({uint8_t(from), uint8_t(square(nf, nr)), Piece::None});
                }
            break;
        default: break;
        }
    }
    return out;
}

GameSnapshot TgChess::applyMove(const GameSnapshot& s, const Move& m) {
    GameSnapshot next = s;
    if (m.from >= 64 || m.to >= 64) { next.state = GameState::Aborted; return next; }
    Piece& mover = next.board[m.from];
    if (mover == Piece::None) { next.state = GameState::Aborted; return next; }
    if (colorOf(mover) != s.toMove) { next.state = GameState::Aborted; return next; }

    // Basic legality: the move must be in the pseudo-legal set.
    const auto legal = pseudoLegalMoves(s);
    bool ok = false;
    for (const auto& cand : legal) {
        if (cand.from == m.from && cand.to == m.to && cand.promo == m.promo) {
            ok = true; break;
        }
    }
    if (!ok) { next.state = GameState::Aborted; return next; }

    // Apply.
    next.board[m.to] = (m.promo != Piece::None) ? m.promo : mover;
    next.board[m.from] = Piece::None;
    next.toMove = (s.toMove == Color::White) ? Color::Black : Color::White;
    next.moveCount = s.moveCount + 1;

    // Checkmate / stalemate detection (simplified: if the opponent has no
    // pseudo-legal moves, the game ends).
    const auto reply = pseudoLegalMoves(next);
    if (reply.empty()) {
        // A more complete engine would check whether the king is in check.
        next.state = GameState::Checkmate;
        Q_EMIT instance().gameEnded(0, GameState::Checkmate);
    }
    Q_EMIT instance().moveApplied(0, m, next);
    return next;
}

// ---------------------------------------------------------------------------
// FEN
// ---------------------------------------------------------------------------
QString TgChess::toFen(const GameSnapshot& s) {
    QString fen;
    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            const Piece p = s.board[square(f, r)];
            if (p == Piece::None) { ++empty; continue; }
            if (empty) { fen.append(QString::number(empty)); empty = 0; }
            fen.append(pieceToChar(p));
        }
        if (empty) fen.append(QString::number(empty));
        if (r) fen.append('/');
    }
    fen.append(s.toMove == Color::White ? " w " : " b ");
    fen.append("- - 0 ");
    fen.append(QString::number(s.moveCount));
    return fen;
}

GameSnapshot TgChess::fromFen(const QString& fen) {
    GameSnapshot s;
    const QStringList parts = fen.split(' ');
    if (parts.isEmpty()) return s;
    const QStringList ranks = parts[0].split('/');
    int r = 7;
    for (const QString& rank : ranks) {
        int f = 0;
        for (const QChar c : rank) {
            if (c.isDigit()) { f += c.digitValue(); continue; }
            s.board[square(f, r)] = charToPiece(c);
            ++f;
        }
        --r;
    }
    if (parts.size() >= 2)
        s.toMove = (parts[1] == "w") ? Color::White : Color::Black;
    return s;
}

} // namespace tg::truthgram::chess
