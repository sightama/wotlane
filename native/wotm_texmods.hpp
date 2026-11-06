// PNG mod textures (MODS.md): the mod ships only the image, and the native renderer uses it in place of the game's texture.
// The game still loads the .TEX/.RTX from disc (VRAM, ids and palettes untouched); only what the window draws changes.
// File name, in any folder of the mod, as tools/tex.py export (and tools/modkit.py) write them:
//   <file>_t<texId>.png       texture from <file>.TEX       (mods/meu_mod/MON/lizard0_t0004.png)
//   <file>_rtx_t<texId>.png   resident image from <file>.RTX (portraits, screens and menu fonts)
// texId is the one in the file on disc (before the load relocates it). Any resolution works: draw UVs are normalized (u / 4096) and
// screen quads (font, portraits) scale their region by the ratio between the PNG and the original texture. Applies to every palette
// that uses the texture. PNG alpha: 255 = opaque (GS 0x80); if the original texture goes above 0x80, 255 = 0xFF (loadPng). Doesn't apply to the image from ps2EntryRunner's emulated GS (the
// reference for the VU1 check) nor to the composited dust (assembled in VRAM). Particles (Scene::modParticle) and the screens' 3D monster (wotm_model.hpp) use the PNG.
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "raylib.h"
#include "wotm_mods.hpp"

namespace wotm::texmods {

struct Png {
    std::string host;   // file on the PC
    std::string mod;    // mod folder
    std::string rel;    // path inside the mod (for the log)
};

class Index {
public:
    static Index &get() { static Index i; return i; }

    // (FILE, rtx, texId) -> PNG. Built on the first lookup, after the mods/ scan (Registry::scan).
    const Png *find(const std::string &stem, bool rtx, uint32_t texId) {
        build();
        const auto it = m_png.find(Key{upper(stem), rtx, texId});
        return it == m_png.end() ? nullptr : &it->second;
    }
    bool empty() { build(); return m_png.empty(); }

    // "lizard0_t0004.png" -> (LIZARD0, false, 4); "shell_rtx_t0012.png" -> (SHELL, true, 12)
    static bool parseName(const std::string &file, std::string &stem, bool &rtx, uint32_t &id) {
        std::string n = upper(file);
        if (n.size() < 7 || n.compare(n.size() - 4, 4, ".PNG") != 0) return false;
        n.resize(n.size() - 4);
        static const std::string kGray = "_SEMPALETA";   // tools/tex.py export without the palette found (comes out gray); the name works the same
        if (n.size() > kGray.size() && n.compare(n.size() - kGray.size(), kGray.size(), kGray) == 0) n.resize(n.size() - kGray.size());
        const size_t t = n.rfind("_T");
        if (t == std::string::npos || t == 0 || t + 2 >= n.size()) return false;
        uint32_t v = 0;
        for (size_t i = t + 2; i < n.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(n[i])) || v > 99999) return false;
            v = v * 10 + uint32_t(n[i] - '0');
        }
        stem = n.substr(0, t);
        rtx = stem.size() > 4 && stem.compare(stem.size() - 4, 4, "_RTX") == 0;
        if (rtx) stem.resize(stem.size() - 4);
        id = v;
        return !stem.empty();
    }

private:
    struct Key {
        std::string stem; bool rtx; uint32_t id;
        bool operator<(const Key &o) const { return stem != o.stem ? stem < o.stem : rtx != o.rtx ? rtx < o.rtx : id < o.id; }
    };
    std::map<Key, Png> m_png;
    bool m_built = false;

    static std::string upper(std::string s) {
        for (char &c : s) c = char(std::toupper(static_cast<unsigned char>(c)));
        return s;
    }
    void build() {
        const mods::Registry &reg = mods::Registry::get();
        if (m_built || !reg.ready()) return;
        m_built = true;
        for (const auto &[rel, f] : reg.files()) {
            const std::string base = rel.substr(rel.find_last_of('/') + 1);
            std::string stem; bool rtx = false; uint32_t id = 0;
            if (!parseName(base, stem, rtx, id)) continue;
            Png &p = m_png[Key{stem, rtx, id}];
            if (!p.host.empty())
                std::fprintf(stderr, "[mods] png %s: %s/%s overrides %s/%s\n", base.c_str(), f.mod.c_str(), rel.c_str(), p.mod.c_str(), p.rel.c_str());
            p.host = f.host.string();
            p.mod = f.mod;
            p.rel = rel;
        }
        // Skin PNGs (mods/<mod>/skins/<folder>/): the number in the name counts, the file is the skin's costume's. Accepts the name from
        // the base costume's export ("lizard1_t0004.png", "lizard1_rtx_t0011.png") or just "t0004.png"/"rtx_t0011.png".
        for (const mods::SkinPng &sp : reg.skinPngs()) {
            const std::string base = sp.host.filename().string();
            std::string stem; bool rtx = false; uint32_t id = 0;
            if (!parseName(base, stem, rtx, id) && !parseName("X_" + base, stem, rtx, id)) {
                std::fprintf(stderr, "[mods] png %s: name without _tNNNN, ignored\n", sp.rel.c_str());
                continue;
            }
            if (upper(base).rfind("RTX_T", 0) == 0) rtx = true;
            Png &p = m_png[Key{sp.stem, rtx, id}];
            p.host = sp.host.string();
            p.mod = sp.mod;
            p.rel = sp.rel;
        }
        // Portrait of custom monsters ("portrait" in monster.json): the image in the .RTX of the costume they use
        for (const mods::Custom &cu : reg.customs()) {
            if (cu.portrait.empty() || cu.portraitTex < 0) continue;
            Png &p = m_png[Key{upper(cu.prefix + std::to_string(cu.costume)), true, uint32_t(cu.portraitTex)}];
            p.host = cu.portrait.string();
            p.mod = cu.mod;
            std::error_code ec;   // path inside the mod, for the log
            p.rel = std::filesystem::relative(cu.portrait, cu.dir.parent_path().parent_path(), ec).generic_string();
            if (ec || p.rel.empty()) p.rel = cu.portrait.filename().string();
        }
        if (!m_png.empty()) std::fprintf(stderr, "[mods] %zu PNG texture(s)\n", m_png.size());
    }
};

// Reads the PNG as RGBA (r | g << 8 | b << 16 | a << 24). rawAlpha: alpha as the GS sees it (255 -> 0x80, the rest / 2), for what Scene::draw
// blends like the GS; otherwise 0..255. tcc = false: RGB only (TEX0.TCC = 0), alpha always 1.0. Logs the first load of each PNG.
// hiAlpha: the original texture uses alpha above 0x80 (most of the game's go from 0 to 0xFF; the title logo is drawn with the
// vertex at half opacity and relies on the 0xFF). The PNG is then taken raw: 255 = 0xFF, as tools/tex.py export writes it (fix_alpha).
inline bool loadPng(const Png &png, const std::string &what, uint32_t origW, uint32_t origH, bool rawAlpha, bool tcc,
                    std::vector<uint32_t> &px, uint32_t &w, uint32_t &h, bool hiAlpha = false) {
    Image img = LoadImage(png.host.c_str());
    if (!img.data || img.width <= 0 || img.height <= 0) {
        std::fprintf(stderr, "[mods] png %s (%s): failed to open\n", png.rel.c_str(), png.mod.c_str());
        if (img.data) UnloadImage(img);
        return false;
    }
    if (img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    w = uint32_t(img.width); h = uint32_t(img.height);
    px.resize(size_t(w) * h);
    std::memcpy(px.data(), img.data, px.size() * 4);
    UnloadImage(img);
    for (uint32_t &c : px) {
        uint32_t a = tcc ? c >> 24 : (hiAlpha && rawAlpha ? 0x80 : 255);
        if (hiAlpha) { if (tcc && !rawAlpha) a = std::min(255u, a * 2); }   // raw; 0..255 with 0x80 saturated, like gs::fixAlpha
        else if (rawAlpha) a = a >= 255 ? 0x80 : a / 2;
        c = (c & 0x00FFFFFFu) | (a << 24);
    }
    const bool ratio = uint64_t(w) * origH == uint64_t(h) * origW;
    std::fprintf(stderr, "[mods] %s <- %s/%s (%ux%u; game %ux%u%s)%s\n", what.c_str(), png.mod.c_str(), png.rel.c_str(), w, h, origW, origH,
                 hiAlpha ? "; raw alpha, 255 = 0xFF" : "", ratio ? "" : " WARNING: aspect ratio differs from the original, the image comes out stretched");
    return true;
}

// Packets of a .TEX/.RTX (already decompressed): packet offset in the file -> texId on disc. Same loop as tools/tex.py parse.
inline std::map<uint32_t, uint32_t> packetIds(const std::vector<uint8_t> &d) {
    std::map<uint32_t, uint32_t> out;
    if (d.size() < 8) return out;
    uint32_t headerQw = 0;
    std::memcpy(&headerQw, d.data() + 4, 4);
    size_t off = size_t(headerQw) * 16;
    for (int n = 0; off + 16 <= d.size() && n < 4096; ++n) {
        uint32_t size = 0, idTbp = 0;
        std::memcpy(&size, d.data() + off, 4);
        std::memcpy(&idTbp, d.data() + off + 8, 4);
        out[uint32_t(off)] = idTbp & 0xFFFF;
        if (!size) break;
        const size_t next = off + size_t(size >> 2) * 16;
        if (next <= off) break;
        off = next;
    }
    return out;
}

// Decompressed file from disc (zip with the magic "IE\3\4", as in Scene::loadRtx). Empty if it failed to open.
inline std::vector<uint8_t> readUnzipped(const std::string &path) {
    std::vector<uint8_t> raw;
    if (FILE *f = std::fopen(path.c_str(), "rb")) {
        std::fseek(f, 0, SEEK_END);
        raw.resize(size_t(std::max(0L, std::ftell(f))));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(raw.data(), 1, raw.size(), f) != raw.size()) raw.clear();
        std::fclose(f);
    }
    static const uint8_t kMagic[4] = {'I', 'E', 3, 4};
    if (raw.size() < 34 || std::memcmp(raw.data(), kMagic, 4) != 0) return raw;
    uint32_t csize = 0;
    uint16_t nlen = 0, xlen = 0;
    std::memcpy(&csize, raw.data() + 18, 4);
    std::memcpy(&nlen, raw.data() + 26, 2);
    std::memcpy(&xlen, raw.data() + 28, 2);
    const size_t off = 30 + size_t(nlen) + xlen;
    if (off + csize > raw.size()) return {};
    int outSize = 0;
    unsigned char *d = DecompressData(raw.data() + off, int(csize), &outSize);
    std::vector<uint8_t> out;
    if (d && outSize > 0) out.assign(d, d + outSize);
    if (d) MemFree(d);
    return out;
}

} // namespace wotm::texmods
