// ABOUTME: Map editor state shared by the EE thread (session, live preview, save: runtime/wotm_mapedit.inc) and the window thread
// ABOUTME: (keys, markers and panel: runtime/wotm_mapedit_view.inc). Defined in wotm_overrides.cpp.
#pragma once
#include "wotm_mapedit.hpp"

#include <mutex>
#include <string>
#include <vector>

struct WotmMapEdit {
    std::mutex m;
    bool enabled = false;   // WOTM_MAPEDIT names the map to save (set once at boot, read-only afterwards)
    bool ready = false;     // a session exists for the stage on screen
    bool on = false;        // the editor listens to the keys and draws (F10, window thread)
    std::string id, stage;  // the map being made and the stock stage it starts from
    std::vector<wotm::mapedit::ItemView> items;   // as the EE thread last published them
    int selected = -1;
    int edits = 0, copies = 0;
    std::string msg;        // the last result (SAVED ..., MODS ARE OFF, ...)
    std::vector<wotm::mapedit::Command> cmds;     // window -> EE thread, run on the next game frame
    static constexpr int kMaxQueued = 64;         // while the game is held nothing drains the queue: later commands are dropped

    void push(const wotm::mapedit::Command &c)
    {
        std::lock_guard<std::mutex> lk(m);
        if (int(cmds.size()) < kMaxQueued)
            cmds.push_back(c);
    }
};

extern WotmMapEdit g_wotmMapEdit;
