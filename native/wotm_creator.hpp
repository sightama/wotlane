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
    while (!out.empty() && out.back() == '_') out.pop_back();   // the cut can land right after a _
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
    return json::dump(v);
}

// Alpha of a texture whose raw GS alpha goes above 0x80 (TexInfo::hiAlpha) before it is written to PNG. Such a PNG is read back
// raw (texmods::loadPng, "255 = 0xFF", as tools/tex.py export writes it): the fight takes its alpha as the GS's, the model path
// doubles it (min(255, 2a)). The decoded pixels hold fixAlpha's 0..255: 2 * raw below 0x80 (halved: exact), and 255 for any raw
// from 0x80 up, which stays 255 (raw 0xFF: what such textures hold, e.g. every texel of Togera's t0004; 0x80 would halve their
// alpha in the fight). Model path round trip: exact for even values and 255, an odd value comes back 1 lower.
inline void halveAlpha(std::vector<uint32_t> &rgba) {
    for (uint32_t &px : rgba) {
        const uint32_t a = px >> 24;
        px = (px & 0x00FFFFFFu) | (a >= 255u ? 255u : a / 2u) << 24;
    }
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

// One entry of Model::textures() as the creator sees it (Model::TexInfo without raylib)
struct TexKey { uint16_t id = 0; bool rtx = false; };
// Indices of textures() per distinct (rtx, id), in first-seen order: the TEXTURE row, the tints and the PNGs are per group
inline std::vector<std::vector<int>> groupTextures(const std::vector<TexKey> &keys) {
    std::vector<std::vector<int>> out;
    std::vector<TexKey> seen;
    for (int i = 0; i < int(keys.size()); ++i) {
        size_t g = 0;
        while (g < seen.size() && !(seen[g].id == keys[size_t(i)].id && seen[g].rtx == keys[size_t(i)].rtx)) ++g;
        if (g == seen.size()) { seen.push_back(keys[size_t(i)]); out.emplace_back(); }
        out[g].push_back(i);
    }
    return out;
}

} // namespace wotm::creator
