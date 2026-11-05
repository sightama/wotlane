// ABOUTME: Pure parts of the character creator (CREATE in the menu, WOTM_CREATOR): the skin id from the typed name, the
// ABOUTME: WOTM_CREATOR script, the skin.json text and the PNG names the registry accepts. No raylib, no game: unit tested.
#pragma once
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "wotm_json.hpp"
#include "wotm_mods.hpp"
#include "net/wotm_monsters.hpp"

namespace wotm::creator {

constexpr int kNameLen = 11;   // the HUD's letters
constexpr const char *kMod = "Creator";   // mods/Creator/skins/<id>/

// "TOGERA 2" -> "togera_2": lowercase, runs of anything outside a-z0-9 become one _, edges trimmed, up to 32 (skin.json's rule);
// empty -> "skin"
inline std::string idFromName(const std::string &name) {
    std::string out;
    bool gap = false;
    for (char ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) {
            if (gap && !out.empty()) out += '_';
            gap = false;
            out += char(std::tolower(c));
        } else gap = true;
    }
    if (out.size() > 32) out.resize(32);
    return out.empty() ? "skin" : out;
}

inline std::string pngName(uint32_t texId, bool rtx) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%st%04u.png", rtx ? "rtx_" : "", texId);
    return buf;
}

inline std::string skinJson(const std::string &monster, const std::string &id, const std::string &name, int base) {
    json::Value v = json::Value::object();
    v.at("monster") = json::Value::string(monster);
    v.at("id") = json::Value::string(id);
    v.at("name") = json::Value::string(name);
    v.at("base") = json::Value::integer(base);
    v.at("ai") = json::Value::boolean(false);
    return json::dump(v) + "\n";
}

// WOTM_CREATOR=<monster>:<costume>:<id>[:<hue>[:<sat>[:<bright>]]]
struct Script { int monster = 0; int costume = 0; std::string id; float hue = 0.f, sat = 0.f, bright = 0.f; };
inline bool parseScript(const std::string &text, Script &out, std::string &err) {
    std::vector<std::string> f;
    for (size_t i = 0;;) {
        const size_t j = text.find(':', i);
        f.push_back(text.substr(i, j == std::string::npos ? std::string::npos : j - i));
        if (j == std::string::npos) break;
        i = j + 1;
    }
    if (f.size() < 3 || f.size() > 6) { err = "expected <monster>:<costume>:<id>[:<hue>[:<sat>[:<bright>]]]"; return false; }
    const mods::MonsterInfo *mi = mods::monsterInfo(f[0]);
    if (!mi) { err = "unknown monster \"" + f[0] + "\""; return false; }
    out.monster = mi->type;
    char *end = nullptr;
    const long c = std::strtol(f[1].c_str(), &end, 10);
    if (f[1].empty() || *end || c < 0 || c >= wotm_monsters::costumes(mi->type)) { err = "costume \"" + f[1] + "\" is not on the disc"; return false; }
    out.costume = int(c);
    if (f[2].empty() || f[2].size() > 32 || f[2].find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos) {
        err = "id must be lowercase letters, digits and _ (up to 32)"; return false;
    }
    out.id = f[2];
    float *vals[3] = {&out.hue, &out.sat, &out.bright};
    out.hue = out.sat = out.bright = 0.f;
    for (size_t k = 3; k < f.size(); ++k) {
        const float x = std::strtof(f[k].c_str(), &end);
        if (f[k].empty() || *end || !std::isfinite(x)) { err = "number expected at field " + std::to_string(k + 1); return false; }
        *vals[k - 3] = x;
    }
    return true;
}

} // namespace wotm::creator
