// ABOUTME: Monsters that exist on the disc: how many costumes (files MON\<NAME><costume>.NGP/.TEX/.RTX/.PTR) each game index has.
// ABOUTME: Used by WOTM_MATCH (wotm_overrides.cpp) and wotm_server's JOIN: a monster with no files hung the stage load.
#pragma once

namespace wotm_monsters
{
    // Index into the game's MonsterLongNames (0x6EF860, char[16][8]): null, monkey, robot, lizard, shogun, mantis, spider, energy,
    // lava, dragon, rock, alien, ogre, assboss, jelly, final. spider and ogre were cut (no files in MON); assboss has 2 costumes,
    // jelly and final 1. A missing file is read as 0 bytes, its fileStatus slot gets the next file's address, the same .RTX goes in
    // twice and texmResRelocate links its packet list to itself (endless loop during the load).
    constexpr int kCount = 16;
    constexpr int kCostumes[kCount] = {0, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 0, 2, 1, 1};

    inline int costumes(int monster) { return monster >= 0 && monster < kCount ? kCostumes[monster] : 0; }

    // First automatic costume: assboss (Goliath) costume 0 is a prototype (wotm_custom.inc)
    inline int firstCostume(int monster) { return monster == 13 ? 1 : 0; }

    constexpr int kMaxList = 16;

    // Does the list fit what's on the disc, given the costumes that WOTM_MATCH and lobby matches hand out (userintMainDirect,
    // wotm_overrides.cpp)? `asked[i]` is the requested costume (-1, or asked null: automatic); requests are placed first (the first
    // to ask gets it), then each automatic one takes the lowest free costume of the same monster starting at firstCostume, and with
    // more than 4 of the same monster it wraps (c & 3). Mod costumes (4 and up) come from mods/ and aren't checked here. If it
    // doesn't fit, *bad gets the index of the first entry with no files.
    inline bool fits(const int *monsters, const int *asked, int n, int *bad = nullptr)
    {
        if (n > kMaxList)
            n = kMaxList;
        int given[kMaxList];
        auto taken = [&](int i, int c) {
            for (int j = 0; j < n; ++j)
                if (j != i && given[j] == c && monsters[j] == monsters[i])
                    return true;
            return false;
        };
        for (int i = 0; i < n; ++i)
            given[i] = -1;
        for (int i = 0; i < n; ++i)
            if (asked && asked[i] >= 0 && !taken(i, asked[i]))
                given[i] = asked[i];
        for (int i = 0; i < n; ++i)
            for (int c = firstCostume(monsters[i]); given[i] < 0; ++c)
                if (c > 3 || !taken(i, c))
                    given[i] = c & 3;
        for (int i = 0; i < n; ++i)
            if (given[i] < 4 && given[i] >= costumes(monsters[i]))
            {
                if (bad)
                    *bad = i;
                return false;
            }
        return true;
    }
}
