// ABOUTME: Picks the next lock-on target to the left or right on screen (lockCycleTarget in wotm_overrides.cpp), apart from guest RAM so
// ABOUTME: native/tests can cycle targets. The pick is game state, so its angles come from wotm_detmath: the same bits on every machine.
#pragma once
#include <cstddef>
#include <cstdint>

#include "wotm_detmath.hpp"

namespace wotm::lockon
{
    struct Candidate
    {
        uint32_t id;   // the monster (guest address); never 0
        float pos[3];
    };

    // Horizontal angle of p seen from me on the player's camera (right and fwd: the camera's axes in the XY plane). 0 = straight ahead,
    // positive to the right.
    inline float screenAngle(const float me[3], const float p[3], const float right[2], const float fwd[2])
    {
        const float dx = p[0] - me[0], dy = p[1] - me[1];
        return wotm::detmath::atan2f(dx * right[0] + dy * right[1], dx * fwd[0] + dy * fwd[1]);
    }

    // The candidate nearest to the left (dir < 0) or right (dir > 0) of the current target's angle curA, wrapping around to the farthest
    // on the opposite side; 0 with no candidates. On a tie the earlier candidate wins.
    inline uint32_t nextTarget(const float me[3], float curA, const Candidate *c, size_t n, const float right[2], const float fwd[2], int dir)
    {
        uint32_t best = 0, wrap = 0;
        float bestD = 1e9f, wrapA = 0.f;
        for (size_t i = 0; i < n; ++i)
        {
            const float a = screenAngle(me, c[i].pos, right, fwd), d = (a - curA) * static_cast<float>(dir);
            if (d > 0.f && d < bestD)
            {
                bestD = d;
                best = c[i].id;
            }
            if (!wrap || a * -static_cast<float>(dir) > wrapA * -static_cast<float>(dir))
            {
                wrap = c[i].id;   // the farthest on the opposite side: the wraparound
                wrapA = a;
            }
        }
        return best ? best : wrap;
    }
}
