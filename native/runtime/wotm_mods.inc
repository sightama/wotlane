// wotm_mods.inc: the mods/ folder (MODS.md; registry in native/wotm_mods.hpp). Included by wotm_overrides.cpp.
// Two hooks serve every file the game reads from disc:
// - fileCdSearchFile (src/common/file.cpp): stages, monsters, SHELL, sound banks, music and movies locate their file through it
//   (sector and size). When the file is in mods/, the original still runs (so the log can compare sizes) and the sceCdlFILE comes
//   back with the fake LSN and the mod's size.
// - sceCdRead (runtime stub): fileReadf/fileReada/fileReads and snd_StreamSafeCdRead read through it; the fake LSN is served from the mod.
// Native sound (wotm_snd.inc, readLsnRaw) reads banks and music by LSN and checks the registry before the disc.
// Mod file larger than the disc's: monsters (MON/*) and stage textures load back to back above 32 MB (high RAM), so they only
// push back what comes after; the level-load log (the "[wotm]" lines at the end of the load, wotm_overrides.cpp) shows the spare RAM
// and the texIds used. With WOTM_HIGHRAM=0, and for the other files (stage, SHELL, sound banks), the game loads into fixed-size regions
// and a larger file may overwrite what comes after (the log warns). WOTM_MODS=<folder> changes the folder (default: mods/ next to the
// ELF, otherwise the repo's); =0 turns mods off.
#include "wotm_mods.hpp"
#include <set>

namespace wotm_mods_hooks
{
using wotm::mods::Registry;

PS2Runtime::RecompiledFunction g_origSearch = nullptr, g_origCdRead = nullptr;

inline uint32_t rd32(const uint8_t *rdram, uint32_t a) { uint32_t v; std::memcpy(&v, rdram + (a & PS2_RAM_MASK), 4); return v; }
inline void wr32(uint8_t *rdram, uint32_t a, uint32_t v) { std::memcpy(rdram + (a & PS2_RAM_MASK), &v, 4); }

// int fileCdSearchFile(sceCdlFILE *f, char *path); sceCdlFILE = {u32 lsn, u32 size, char name[16], u8 date[8]}
void searchFile(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    const uint32_t fAddr = getRegU32(ctx, 4), pAddr = getRegU32(ctx, 5);
    const char *p = reinterpret_cast<const char *>(rdram + (pAddr & PS2_RAM_MASK));
    const std::string rel = Registry::keyFromGame(std::string(p, strnlen(p, 256)));
    const std::string target = Registry::get().aliasOf(rel);
    const wotm::mods::File *m = Registry::get().find(target);
    if (target != rel)
    {
        // Mod costume without this file in the skin folder: looks up the base costume's ("LIZARD4" -> "LIZARD1"), swapping the name
        // in the game's own string during the call (the base name is never longer: the base costume number has one digit)
        char *s = reinterpret_cast<char *>(rdram + (pAddr & PS2_RAM_MASK));
        const size_t len = strnlen(s, 256);
        const std::string saved(s, len + 1);
        const std::string from = rel.substr(rel.find_last_of('/') + 1), to = target.substr(target.find_last_of('/') + 1);
        std::string up(saved);
        for (char &c : up) c = char(std::toupper(static_cast<unsigned char>(c)));
        if (const size_t at = up.find(from); at != std::string::npos && to.size() <= from.size())
        {
            std::string repl = saved.substr(0, at) + to + saved.substr(at + from.size());
            std::memcpy(s, repl.c_str(), repl.size() + 1);
        }
        g_origSearch(rdram, ctx, rt);
        std::memcpy(s, saved.data(), saved.size());
        static std::set<std::string> logged;
        if (logged.insert(rel).second)
            std::fprintf(stderr, "[mods] %s = %s (skin: base costume's file)%s\n", rel.c_str(), target.c_str(), getRegU32(ctx, 2) ? "" : " NOT FOUND");
    }
    else
        g_origSearch(rdram, ctx, rt);
    if (!m)
        return;
    const bool onDisc = getRegU32(ctx, 2) != 0;
    const uint32_t discSize = onDisc ? rd32(rdram, fAddr + 4) : 0;
    wr32(rdram, fAddr + 0, m->lsn);
    wr32(rdram, fAddr + 4, uint32_t(m->size));
    if (!onDisc)   // new file: the sceCdlFILE name comes from the mod (the original found nothing to copy)
    {
        std::string base = m->rel.substr(m->rel.find_last_of('/') + 1);
        base.resize(std::min<size_t>(base.size(), 15));
        std::memset(rdram + ((fAddr + 8) & PS2_RAM_MASK), 0, 16 + 8);
        std::memcpy(rdram + ((fAddr + 8) & PS2_RAM_MASK), base.data(), base.size());
    }
    setReturnS32(ctx, 1);
    if (!m->logged)
    {
        m->logged = true;
        if (!onDisc)
            std::fprintf(stderr, "[mods] %s <- %s (%llu bytes; new file, not on the disc)\n", m->rel.c_str(), m->mod.c_str(),
                         (unsigned long long)m->size);
        else
            std::fprintf(stderr, "[mods] %s <- %s (%llu bytes; disc %u)%s\n", m->rel.c_str(), m->mod.c_str(), (unsigned long long)m->size,
                         discSize, m->size <= discSize ? ""
                                   : m->rel.rfind("MON/", 0) == 0 ? " (larger than the disc's: check the spare RAM in the level-load log)"
                                                                   : " WARNING: larger than the disc's, may overwrite what the game loads after it");
    }
}

// int sceCdRead(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode)
void cdRead(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    const uint32_t lsn = getRegU32(ctx, 4);
    if (!Registry::isModLsn(lsn))
    {
        g_origCdRead(rdram, ctx, rt);
        return;
    }
    const uint32_t sectors = getRegU32(ctx, 5), offset = getRegU32(ctx, 6) & PS2_RAM_MASK;
    const size_t bytes = size_t(std::min<uint64_t>(uint64_t(sectors) * 2048, uint64_t(PS2_RAM_SIZE) - offset));
    if (!Registry::get().read(lsn, sectors, rdram + offset, bytes))
    {
        static int warned = 0;
        if (warned++ < 16)
            std::fprintf(stderr, "[mods] sceCdRead: LSN %#x (%u sectors) doesn't belong to any mod file; returning zeros\n", lsn, sectors);
        std::memset(rdram + offset, 0, bytes);
    }
    setReturnS32(ctx, 1);   // the stub also reads synchronously: sceCdSync returns "done"
}
} // namespace wotm_mods_hooks

void wotmModsInstall(PS2Runtime &runtime, const std::string &elfPath)
{
    using namespace wotm_mods_hooks;
    namespace fs = std::filesystem;
    fs::path dir;
    if (const char *e = std::getenv("WOTM_MODS"))
    {
        if (std::strcmp(e, "0") == 0)
        {
            std::fprintf(stderr, "[mods] off (WOTM_MODS=0)\n");
            return;
        }
        dir = e;
    }
    else
    {
        // mods/ with that exact name: on Windows the path "mods" next to the ELF in the disc folder resolves to the game's MOD/ (the IRXs)
        auto modsIn = [](const fs::path &parent) {
            std::error_code ec;
            for (fs::directory_iterator it(parent, ec), end; !ec && it != end; it.increment(ec))
                if (it->path().filename() == "mods" && it->is_directory(ec))
                    return true;
            return false;
        };
        dir = fs::path(elfPath).parent_path() / "mods";
        // without it, the repo's (WOTM_REPO, which run_live.bat and the launcher pass): the mods shipped in the repo (Goliath Prime,
        // Cerebulon) work without copying anything
        const char *repo = std::getenv("WOTM_REPO");
        if (!modsIn(fs::path(elfPath).parent_path()) && repo && *repo && modsIn(fs::path(repo)))
            dir = fs::path(repo) / "mods";
    }
    const size_t n = Registry::get().scan(dir);
    if (n == 0)
        return;   // no mod files: no hooks, the game reads the disc as always
    g_origSearch = runtime.lookupFunction(wotm::fn::fileCdSearchFile__FP10sceCdlFILEPc);
    g_origCdRead = runtime.lookupFunction(wotm::fn::sceCdRead);
    if (!g_origSearch || !g_origCdRead)
    {
        std::fprintf(stderr, "[mods] fileCdSearchFile/sceCdRead not found: mods off\n");
        return;
    }
    runtime.registerFunction(wotm::fn::fileCdSearchFile__FP10sceCdlFILEPc, hostLeaf<searchFile>);
    runtime.registerFunction(wotm::fn::sceCdRead, hostLeaf<cdRead>);
    std::string names;
    for (const std::string &m : Registry::get().modNames())
        names += (names.empty() ? "" : ", ") + m;
    std::fprintf(stderr, "[mods] %zu file(s) from %s in %s\n", n, names.c_str(), dir.string().c_str());
}
