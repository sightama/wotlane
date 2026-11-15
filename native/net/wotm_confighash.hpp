// ABOUTME: The environment variables that change a network match's game state, and the text of them that each PC puts in its config
// ABOUTME: hash (runtime/wotm_sync.inc): the server refuses a player whose hash differs from the others'.
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
}
