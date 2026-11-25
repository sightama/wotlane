// ABOUTME: Read-only view of the match's score for the page's result card: each fighter's monster type and rounds won, the wins needed
// ABOUTME: and rounds played, read from guest RAM and published for the page (wotm_web_result). Never writes game state.
#pragma once

#include <atomic>
#include <cstdint>

#include "wotm_syms.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace wotm::matchresult
{
constexpr int kMaxFighters = 8;
constexpr int kCells = 5 + 2 * kMaxFighters;   // see cell()

// TheGame (shellGame) and Shell (shellInfo) fields, as declared in include/game/game.h and shell.h
constexpr uint32_t kGame = wotm::sym::shellGame, kShell = wotm::sym::shellInfo;
constexpr uint32_t kMonsters = kGame + 0x120380;   // Monster *[]: one pointer per slot, players first, then the AIs
constexpr uint32_t kNumMonsters = kGame + 0x1203D8;
constexpr uint32_t kTypeBits = 0x14, kWinsThisGame = 0x3C;   // Monster: type in bits 5 and up, rounds won this match
constexpr uint32_t kKillTarget = kShell + 0x2A44, kRoundsPlayed = kShell + 0x2B5C, kInSession = kShell + 0x2BCC;

struct Fighter
{
    int type = 0;   // the monster's type (1 Congar ... 11 Zorgulon, the lobby's monster ids), 0 when there is none
    int wins = 0;   // rounds won this match
};

struct Result
{
    bool session = false;   // a play session is running; false in menus (nothing else is meaningful then)
    int count = 0;          // fighters
    int target = 0;         // wins needed to take the match, 0 for endless
    int rounds = 0;         // rounds played
    Fighter fighter[kMaxFighters];

    int totalWins() const
    {
        int n = 0;
        for (int i = 0; i < count; ++i) n += fighter[i].wins;
        return n;
    }

    // the fighter that has the wins target, -1 for none (also an endless match), -2 for a tie at the target
    int winner() const
    {
        if (target <= 0) return -1;
        int best = -1;
        for (int i = 0; i < count; ++i)
        {
            if (fighter[i].wins < target) continue;
            if (best == -1 || fighter[i].wins > fighter[best].wins) best = i;
            else if (fighter[i].wins == fighter[best].wins) best = -2;
            if (best == -2) break;
        }
        return best;
    }
};

// `R` is wotm::Ram (u32, ok). Pointers are KSEG-masked like the game's own code does; the list ends at the first slot whose pointer is
// not in RAM
template <class R> Result read(const R &ram)
{
    Result r;
    r.session = ram.u32(kInSession) != 0;
    if (!r.session) return r;
    const uint32_t n = ram.u32(kNumMonsters);
    r.target = int(ram.u32(kKillTarget));
    r.rounds = int(ram.u32(kRoundsPlayed));
    for (uint32_t i = 0; i < n && i < uint32_t(kMaxFighters); ++i)
    {
        const uint32_t m = ram.u32(kMonsters + 4 * i) & 0x1FFFFFFFu;
        if (!m || !ram.ok(m + kWinsThisGame)) break;
        r.fighter[r.count].type = int(ram.u32(m + kTypeBits) >> 5);
        r.fighter[r.count].wins = int(ram.u32(m + kWinsThisGame));
        ++r.count;
    }
    return r;
}

// The cells the page reads (wotm_web_result): 0 session (0 or 1), 1 fighters, 2 wins target, 3 rounds played, 4 winner (see
// Result::winner), then per fighter its type and wins (5 + 2 * i, 6 + 2 * i)
inline std::atomic<int32_t> &cell(int i)
{
    static std::atomic<int32_t> cells[kCells];
    return cells[i];
}

inline void publish(const Result &r)
{
    cell(0) = r.session ? 1 : 0;
    cell(1) = r.count;
    cell(2) = r.target;
    cell(3) = r.rounds;
    cell(4) = r.winner();
    for (int i = 0; i < kMaxFighters; ++i)
    {
        cell(5 + 2 * i) = i < r.count ? r.fighter[i].type : 0;
        cell(6 + 2 * i) = i < r.count ? r.fighter[i].wins : 0;
    }
}
}

#ifdef __EMSCRIPTEN__
// called by the page (web/boot.js): one cell of the last published score, 0 for an index outside them
extern "C" EMSCRIPTEN_KEEPALIVE inline double wotm_web_result(int which)
{
    return which >= 0 && which < wotm::matchresult::kCells ? wotm::matchresult::cell(which).load() : 0;
}
#endif
