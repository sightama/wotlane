// Game disc files for the native layer: the extracted disc folder (cdRoot) or an ISO9660 image (PS2X_CD_IMAGE).
// Used by the movies (wotm_fmv.hpp) and the save icon (wotm_saveicon.hpp). Read-only; doesn't depend on the runtime.
// Files in mods/ (wotm_mods.hpp) override the disc's in readFile and in the movies' locate.
#pragma once
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include "wotm_mods.hpp"   // readFile looks in mods/ first

namespace wotm::disc {

// Where to read a movie from: a loose file (extracted disc) or a range of an ISO image
struct Source {
    std::string path;
    uint64_t offset = 0;
    uint64_t size = UINT64_MAX;
    bool ok() const { return !path.empty(); }
};

inline int seek64(FILE *f, uint64_t off) {
#ifdef _WIN32
    return _fseeki64(f, int64_t(off), SEEK_SET);
#else
    return fseeko(f, off_t(off), SEEK_SET);
#endif
}

// File inside an ISO9660 ("MOV/INTRO_M.PSS", case-insensitive; the ";1" version is ignored)
inline Source isoFind(const std::string &iso, const std::string &name) {
    FILE *f = std::fopen(iso.c_str(), "rb");
    if (!f) return {};
    auto readAt = [&](uint64_t off, uint8_t *dst, size_t n) { return seek64(f, off) == 0 && std::fread(dst, 1, n, f) == n; };
    auto le32 = [](const uint8_t *p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); };
    auto upper = [](std::string s) { for (char &c : s) c = char(std::toupper(static_cast<unsigned char>(c))); return s; };
    Source out;
    uint8_t pvd[2048];
    if (readAt(16 * 2048ull, pvd, sizeof pvd) && pvd[0] == 1 && std::memcmp(pvd + 1, "CD001", 5) == 0) {
        uint32_t ext = le32(pvd + 156 + 2), size = le32(pvd + 156 + 10);
        bool isDir = true, found = true;
        size_t start = 0;
        const std::string n = upper(name);
        while (found && start < n.size()) {
            const size_t slash = n.find('/', start);
            const std::string part = n.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
            start = slash == std::string::npos ? n.size() : slash + 1;
            if (part.empty()) continue;
            if (!isDir || size > (64u << 20)) { found = false; break; }
            std::vector<uint8_t> dir(size);
            if (!readAt(uint64_t(ext) * 2048, dir.data(), dir.size())) { found = false; break; }
            found = false;
            for (size_t o = 0; o < dir.size();) {
                const uint8_t len = dir[o];
                if (len == 0) { o = (o / 2048 + 1) * 2048; continue; }   // records don't cross sectors
                if (o + len > dir.size() || len < 34) break;
                std::string id(reinterpret_cast<const char *>(&dir[o + 33]), dir[o + 32]);
                if (const auto sc = id.find(';'); sc != std::string::npos) id.resize(sc);
                if (upper(id) == part) {
                    ext = le32(&dir[o + 2]);
                    size = le32(&dir[o + 10]);
                    isDir = (dir[o + 25] & 2) != 0;
                    found = true;
                    break;
                }
                o += len;
            }
        }
        if (found && !isDir) out = Source{iso, uint64_t(ext) * 2048, size};
    }
    std::fclose(f);
    return out;
}

// Game's file name ("mov/intro_m.pss", "cdrom0:\MOV\INTRO_M.PSS;1") -> relative path "mov/intro_m.pss"
inline std::string discRelative(std::string name) {
    if (const auto c = name.find(':'); c != std::string::npos) name = name.substr(c + 1);
    if (const auto sc = name.find(';'); sc != std::string::npos) name = name.substr(0, sc);
    std::replace(name.begin(), name.end(), '\\', '/');
    while (!name.empty() && name[0] == '/') name.erase(0, 1);
    return name;
}

// Relative path resolved against the root of an extracted disc, case-insensitive; empty if it doesn't exist
inline std::string resolveDiscPath(const std::filesystem::path &root, const std::string &rel) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (root.empty() || !fs::is_directory(root, ec)) return {};
    auto upper = [](std::string s) { for (char &c : s) c = char(std::toupper(static_cast<unsigned char>(c))); return s; };
    fs::path cur = root;
    size_t start = 0;
    while (start < rel.size()) {
        const size_t slash = rel.find('/', start);
        const std::string part = rel.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        start = slash == std::string::npos ? rel.size() : slash + 1;
        if (part.empty()) continue;
        fs::path next = cur / part;
        if (!fs::exists(next, ec)) {
            bool hit = false;
            for (const auto &e : fs::directory_iterator(cur, ec))
                if (upper(e.path().filename().string()) == upper(part)) { next = e.path(); hit = true; break; }
            if (!hit) return {};
        }
        cur = next;
    }
    return fs::is_regular_file(cur, ec) ? cur.string() : std::string{};
}

// Reads a whole file from the disc: first the folder, then the ISO. `rel` like "MEMCRD/ICON.ICO".
inline bool readFile(const std::filesystem::path &root, const std::string &iso, const std::string &rel, std::vector<uint8_t> &out) {
    Source src;
    if (std::string p = wotm::mods::Registry::get().hostPath(rel); !p.empty()) src = Source{p};   // mods/ overrides the disc
    else if (std::string p = resolveDiscPath(root, wotm::mods::Registry::get().aliasOf(rel)); !p.empty()) src = Source{p};   // mod costume: the base's
    else if (!iso.empty()) src = isoFind(iso, wotm::mods::Registry::get().aliasOf(rel));
    if (!src.ok()) return false;
    FILE *f = std::fopen(src.path.c_str(), "rb");
    if (!f) return false;
    bool ok = true;
    if (src.size == UINT64_MAX) {
        std::fseek(f, 0, SEEK_END);
        const long n = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        ok = n >= 0;
        src.size = ok ? uint64_t(n) : 0;
    }
    if (ok && src.size > (64u << 20)) ok = false;
    if (ok) {
        out.resize(size_t(src.size));
        ok = seek64(f, src.offset) == 0 && std::fread(out.data(), 1, out.size(), f) == out.size();
    }
    std::fclose(f);
    return ok;
}

} // namespace wotm::disc
