// ABOUTME: Personal pause in a network match (#16): Start opens this PC's own menu instead of reaching the lockstep, and decides what
// ABOUTME: this PC sends meanwhile (neutral input, or one Start to pause for everyone). Pure; used by wotm_mainmenu.inc, tested in native/tests.
#pragma once

#include <cstdint>

namespace wotm::personalpause
{
inline constexpr uint16_t kStart = 0x0008;   // the PS2 pad's Start bit (active low)
inline constexpr uint16_t kNone = 0xFFFFu;   // nothing pressed

// Closed: the input goes to the match. Open: the menu is up. Releasing: closed, but the buttons that closed it are still held.
struct Menu
{
    enum State { Closed, Open, Releasing };
    State state = Closed;
    bool startAfter = false;     // Releasing: then one frame of Start (PAUSE MATCH FOR EVERYONE)
    uint16_t prev = kNone;       // last frame's buttons, for Start's edge
    bool open() const { return state == Open; }
};

// What this PC sends this frame: neutral (no buttons, centered sticks, no mouse) and, with `start`, Start alone
struct Out
{
    bool neutral = false;
    bool start = false;
};

inline void close(Menu &m)
{
    if (m.state == Menu::Open)
        m.state = Menu::Releasing;
}
inline void pauseEveryone(Menu &m)
{
    close(m);
    m.startAfter = true;
}

// One frame: `buttons` is this PC's sampled pad, `allowed` whether Start may open the menu now (the fight is running and no game
// dialog is up; otherwise Start belongs to the game, and a game dialog closes the menu)
inline Out step(Menu &m, uint16_t buttons, bool allowed)
{
    const bool startEdge = !(buttons & kStart) && (m.prev & kStart);
    m.prev = buttons;
    Out o;
    if (m.state == Menu::Open && !allowed)
        m.state = Menu::Closed;
    if (m.state == Menu::Open)
        o.neutral = true;
    else if (m.state == Menu::Releasing)
    {
        o.neutral = buttons != kNone || m.startAfter;
        if (buttons == kNone)
        {
            o.start = m.startAfter;
            m.startAfter = false;
            m.state = Menu::Closed;
        }
    }
    else if (startEdge && allowed)
    {
        m.state = Menu::Open;
        o.neutral = true;
    }
    return o;
}
}
