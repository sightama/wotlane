// ABOUTME: Network match state shared between the game thread (wotm_sync.inc writes) and the window thread (wotm_live.inc shows the
// ABOUTME: message; online, the debug keys that affect the game are off: F1 cheats, F5 free camera, F6/F7/F8 pause and snapshots).
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace wotm_netstatus
{
    inline std::atomic<bool> g_active{false};    // network match (WOTM_NET)
    inline std::atomic<bool> g_waiting{false};   // the game thread is stopped waiting on the network (the watchdog doesn't count it as a hang)
    inline std::atomic<uint32_t> g_pingMs{0};     // round trip to the server
    inline std::atomic<uint32_t> g_delay{0};      // the match's input delay, in frames
    inline std::mutex g_mx;
    inline std::string g_message;                // line in the center of the screen; empty means nothing

    inline void setMessage(const std::string &m)
    {
        std::lock_guard<std::mutex> lk(g_mx);
        g_message = m;
    }
    inline std::string message()
    {
        std::lock_guard<std::mutex> lk(g_mx);
        return g_message;
    }
}
