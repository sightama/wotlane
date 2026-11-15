// ABOUTME: The environment variables that change a network match's game state, the text of them that each PC puts in its config
// ABOUTME: hash (runtime/wotm_sync.inc), and which part differs when the server refuses a player whose hash differs from the others'.
#pragma once

#include <string>

namespace wotm_net::confighash
{
    // The movie switches (PS2X_FMV, PS2X_SKIP_MOVIES, PS2X_SKIP_CMOVIE) are left out: a match boots straight into the fight and no movie
    // plays in its frames (the state is identical with either, through two wins and the win screen: tools/determinism_check.py
    // --env-b), and the browser version always skips them
    inline constexpr const char *kVars[] = {"WOTM_MATCH",    "WOTM_PLAYERS", "WOTM_AIVSAI",    "WOTM_RATEMIN", "WOTM_FIXEDSTEP",
                                            "WOTM_SEED",     "WOTM_HIGHRAM", "WOTM_ENEMYBARS", "WOTM_CHEATS",  "WOTM_CLOAK",
                                            "WOTM_SAVEJSON", "WOTM_FREECAM", "WOTM_KILLTARGET", "WOTM_30HZ",   "WOTM_MODE",
                                            "PS2X_NOOP"};

    // "NAME=value;" for every variable in kVars, in order (unset = empty); `get` looks a variable up (std::getenv in the game)
    template <typename Getenv>
    std::string envText(Getenv get)
    {
        std::string all;
        for (const char *name : kVars)
        {
            const char *value = get(name);
            all += std::string(name) + "=" + (value ? value : "") + ";";
        }
        return all;
    }

    // The hash a PC sends: "<settings hash>:<mods digest>". The server compares the whole text; the two parts only say what to fix
    inline std::string compose(const std::string &settings, const std::string &mods) { return settings + ":" + mods; }

    // What differs between two hashes ("" when either lacks the two parts, as an older client's does), for the refusal message
    inline std::string mismatch(const std::string &a, const std::string &b)
    {
        const size_t ca = a.find(':'), cb = b.find(':');
        if (ca == std::string::npos || cb == std::string::npos)
            return {};
        const bool settings = a.compare(0, ca, b, 0, cb) != 0, mods = a.compare(ca + 1, std::string::npos, b, cb + 1) != 0;
        if (settings && mods)
            return "the game settings and the mods (creations) differ";
        if (settings)
            return "the game settings differ";
        return mods ? "the mods (creations) differ" : std::string();
    }
}
