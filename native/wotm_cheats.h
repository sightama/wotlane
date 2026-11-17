// Runtime debug cheats: bits of g_wotmCheats (defined in native/runtime/wotm_overrides.cpp).
// The window (F1 menu in wotm_live.inc) writes; the EE thread (Monster::update/takeDamage hook) reads and applies them to the players.
#pragma once
#include <atomic>
#include <cstdint>

constexpr uint32_t kCheatInvincible = 1u << 0;    // takeDamage doesn't affect the player
constexpr uint32_t kCheatStamina1 = 1u << 1;      // 2-bit field: 0 normal, 1 full, 2 rage
constexpr uint32_t kCheatStaminaMask = 3u << 1;
constexpr uint32_t kCheatSpecial = 1u << 3;       // special always available
constexpr uint32_t kCheatCloak = 1u << 4;         // CLOAK always on
constexpr uint32_t kCheatGod = 1u << 5;           // the game's own m_godMode

extern std::atomic<uint32_t> g_wotmCheats;
extern std::atomic<bool> g_wotmCheatWin;          // "win the fight" request (consumed on the next frame)
