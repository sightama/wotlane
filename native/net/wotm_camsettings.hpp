// ABOUTME: A player's modern-camera settings (camera style, recenter, invert, sensitivity) packed into 16 bits of their per-frame input,
// ABOUTME: so every PC turns that player's camera with that player's own settings (wotm_orbit.inc). No dependency on the game.
#pragma once

#include <algorithm>
#include <cstdint>

namespace wotm::camsettings
{
struct Settings
{
    bool modern = false;     // Camera: the modern orbit camera (else the game's classic one)
    bool recenter = false;   // CamRecenter
    bool invertY = false;    // CamInvertY
    int sens = 5;            // CamSens, 1..10
};

// bit 0: present (0 = an input recorded before the settings travelled); 1 modern; 2 recenter; 3 invertY; 4..7 sens; 8..15 zero
inline uint16_t pack(const Settings &s)
{
    const unsigned sens = static_cast<unsigned>(std::clamp(s.sens, 1, 10));
    return static_cast<uint16_t>(1u | (s.modern ? 2u : 0u) | (s.recenter ? 4u : 0u) | (s.invertY ? 8u : 0u) | sens << 4);
}

// false (and `s` untouched) when the input carries no settings
inline bool unpack(uint16_t packed, Settings &s)
{
    if (!(packed & 1u))
        return false;
    s.modern = (packed & 2u) != 0u;
    s.recenter = (packed & 4u) != 0u;
    s.invertY = (packed & 8u) != 0u;
    s.sens = std::clamp(static_cast<int>(packed >> 4 & 15u), 1, 10);
    return true;
}

// The settings that turn a player's camera: the ones packed in their input when present, else `local` (this PC's)
inline Settings resolve(uint16_t packed, const Settings &local)
{
    Settings s = local;
    unpack(packed, s);
    return s;
}
} // namespace wotm::camsettings
