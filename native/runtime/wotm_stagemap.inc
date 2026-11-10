// ABOUTME: Map creator, runtime side: hashes the stage image when Shell::LoadLevelDB inflates it and applies the active map's
// ABOUTME: edits (native/wotm_stagemap.cpp) on entry of dbInitDb, before the game copies positions out of it. Included by wotm_overrides.cpp.
//
// Spec: docs/superpowers/specs/2026-11-09-map-creator-design.md ("Loading a map", "PR 1 checks" 1a-1d). EE thread only.
// - LoadLevelDB (0x1A9AB0) inflates lvl/<STAGE>.NGP to 0xA00000 through zipInflateAll, whose return value is the exact image size
//   (1a: the game keeps only the end rounded up); a tap on zipInflateAll takes it while a LoadLevelDB call is in progress. On its
//   completion the image is hashed (FNV-1a 64, as stage_map.py's baseHash; only with a map active) and g_pending is set.
// - A recompiled function can return midway (thread switch, e.g. waiting for the CD) and be resumed later by the dispatcher at
//   the table slot of the resume address, which a hook on the entry alone never sees (wotm_split4.inc, "The resume doesn't go
//   through this hook"). So the LoadLevelDB and zipInflateAll hooks are registered at every slot of their function (wrapAll), keep
//   the call's $ra from the entry and treat "the original came back with pc == that $ra" as the completion, whichever call of
//   the hook it happens in. The result does not depend on where the time slice ran out.
// - dbInitDb (0x12E418) is also called for the shell, the pre-shell and the load screen, all with 0xA00000 (1b): only the call
//   after a LoadLevelDB (g_pending) applies, once per load (1c: REBUILD LEVEL loads again and applies again; PLAY AGAIN and RESTART
//   don't run dbInitDb, the edited image stays).
// - The active map is set where a stage is chosen (setActive: the stage selection's confirm on a map slot, wotm_levelsel.inc; WOTM_MATCH's
//   map id in userintMainDirect) and cleared each time the menus load (Shell::LoadUserintDB entry) and on a confirm of any other
//   stage, so no path that starts a fight without that confirm (adventure, training, minigames) inherits an earlier map. It stays
//   set for the whole session (REBUILD LEVEL reloads without the menus); a map on another base stage applies nothing. The edits take grid list nodes and ROTATEs from the reserve (kReserve, 4 MB under
//   the top-of-RAM slots, 1d), cleared before each apply; if the loaded files reach it (or fileStatus can't say), nothing is applied.
// - Power-up places (the map's {"powerup": i, "from", "at"} edits) are not in the image: TheGame::InitAfterDbLoad, after dbInitDb,
//   calls PowerUpTool::loadPoints, which copies PTS/<STAGE>.PUP into PowerUpTool::instance's points and calls the virtual
//   exportPoints (0x1881D0), which kills the power-ups and creates one per point. A hook on exportPoints (wrapAll, acting on its
//   entry only) rewrites the points' positions first (sm::applyPowerUps), when the map's piece edits were applied to this load
//   (g_pups, set by applyActive, cleared by the next LoadLevelDB) and the map is still the active one. Game mode 8 (Big Shot) is
//   skipped, as InitAfterDbLoad skips loadPoints there. Writes are absolute: another exportPoints on the same points changes nothing.
// - Copies (PR 2, spec 2026-11-09-map-copy-design.md): apply() builds them in the reserve at the same dbInitDb entry, within the
//   Limits passed in (pickup room: MemoryStack::global's free room at that entry minus sm::kGlobalKeep). The map's power-up places
//   added ({"addpowerup": i, ...}) are appended by the exportPoints hook through sm::applyPointEdits, on the first exportPoints of
//   the load only (it sets g_pupStock, the stock count; LoadLevelDB resets it). sanFranUpdate and threeMileUpdateCountDown call
//   exportPoints again mid-fight without loadPoints (spec 2h): those calls get no additions (the points already hold them) and
//   rerun the moves against the stock count, which writes the same values.
// - Interactives::addInteractive (0x147198) is guarded for every load, map or not (amendment A4): the table has 1024 slots and
//   the game never checks. Past the last slot the call writes nothing, leaves s_numInteractives as it is and returns 0, the
//   game's own "no interactive" id (dispatchHit and HitHistory::newHit ignore id 0, Debris skips a node with id2 0, a hit on a
//   node with id 0 reaches nobody); the first refusal of a load is logged. The maps' estimate keeps them under 1000 (decision 7):
//   the guard is for what the estimate cannot see (HomingBugs, the Airplane, the UFO, bosses). Not every user of the id ignores
//   0: doDomino (ActionDispatch.cpp:385-388) calls getInteractive(id2)->ChangeState, and getInteractive(0) returns slot 0,
//   which is NULL, so a domino aimed at a refused destructible makes ChangeState write into guest page 0; sanFranSaveInteractive
//   (Destructible.cpp:163) keeps the 0 for a SanFran save, and the bosses (FinalBoss.cpp:217, PlantBoss.cpp:519, 1037, 1205)
//   store it in their node's id2, with the same NULL ChangeState on their way. All of that is reachable only once the table is
//   past 1024 entries during a load (spec, Known limits): the guard turns a write past the table into those, never more.
// - LevelPickups::createPickup (0x181750) is guarded the same way for MemoryStack::global, whose alloc16 has no bounds check
//   (include/memory_stack.h:58-63): an allocation past hi1 runs over the high end and the end of the 0x80000 pool. When the free
//   room (hi1 - low) is under the pickup's size (sm::levelPickupBytes() of its id1, plus 16 for alignment) the call allocates
//   nothing and returns 0, the value createPickup itself returns for a type that creates nothing (Pickup.cpp:1166-1167): the
//   stage traversal (dbProcInteractive, db.cpp:223; retail 0x12E818 branches to the return) and Debris::update's settled and
//   dying chunks (Debris.cpp:412, 442; retail 0x12FD74, 0x12FE54 go on to csDeactivate) ignore the result. Debris::Activate's
//   rigid chunk (Debris.cpp:294) does not: retail 0x12F768-0x12F774 loads the node's data and calls huck on it with no null
//   test, so that call (return address 0x12F768) is let through and logged, as is any caller not in kPickupTolerantRa; the
//   maps' estimate charges every debris pickup against the room (spec 2g). The first refusal of a load is logged.
// - WOTM_STAGE_BREAK=<seconds> (test only): that long after the fight's first frame, the first applied copy with a destructible
//   goes to its last state through Destructible::ChangeState (integer arguments), and 120 frames later the copy's and the
//   source's INTERACTIVE_STATE whichChild are logged (tick(), called from the Monster::update hook's slot-0 path).
// - WOTM_STAGE_DUMP=<path>: after apply, the image goes to <path> and the reserve to <path>.reserve (tools/stage_map.py check).
// wotm_overrides.cpp is built with strict float rules, but keep float math out of here anyway: it belongs in wotm_stagemap.cpp.
#include <fstream>

namespace wotm_stagemap
{
namespace sm = wotm::stagemap;

constexpr uint32_t kLoadLevelDB = wotm::fn::LoadLevelDB__5Shell, kZipInflateAll = wotm::fn::zipInflateAll__FPcPv,
                   kDbInitDb = wotm::fn::dbInitDb__FP9_dbheader10_vramAddrs;
constexpr uint32_t kFileStatusAddr = wotm::sym::fileStatus, kWhichLevel = wotm::sym::whichLevel;
constexpr uint32_t kLevelNumAddr = wotm::sym::shellInfo + 0x2BB0;   // Shell::m_levelNum
// PowerUpTool::exportPoints (this in $a0: PowerUpTool::instance); the instance's PointToolKit::points[256] (64 bytes each, the
// position first) is at +0 and numPoints at +0x4000 (getPoint(i) is this + i * 64; exportPoints reads numPoints with lw 0x4000($s0))
constexpr uint32_t kExportPoints = wotm::fn::exportPoints__11PowerUpTool, kPowerUpTool = wotm::sym::x_11PowerUpTool_instance,
                   kNumPointsOff = 0x4000;
constexpr uint32_t kGameModeAddr = wotm::sym::shellGame + 0x1203C8;   // TheGame::m_gameMode
// MemoryStack::global {start +0, low +4, mark +8, end +0xC, hi1 +0x10, hi2 +0x14} (include/memory_stack.h): the free room is hi1 - low
constexpr uint32_t kGlobalStack = wotm::sym::x_11MemoryStack_global;
// Interactives: DbInteractive *s_dbInteractive[1024], int s_numInteractives (src/game/Interactives.cpp)
constexpr uint32_t kAddInteractive = wotm::fn::addInteractive__12InteractivesP13DbInteractive,
                   kNumInteractives = wotm::sym::x_12Interactives_s_numInteractives,
                   kDbInteractive = wotm::sym::x_12Interactives_s_dbInteractive, kInteractiveSlots = 1024;
constexpr uint32_t kChangeState = wotm::fn::ChangeState__12DestructibleibT2;   // Destructible::ChangeState(int, bool, bool)
constexpr uint32_t kNoStock = 0xFFFFFFFFu;   // g_pupStock before the load's first exportPoints
// LevelPickups::createPickup(_hierhead *ep, _fvector &pos, float (*m)[4], bool regen, ePickupType type): a0 ep, t0 type
constexpr uint32_t kCreatePickup = wotm::fn::createPickup__12LevelPickupsP9_hierheadR8_fvectorPA3_fb11ePickupType;
// return addresses of the createPickup calls that ignore the result (see the header): dbProcInteractive, Debris::update twice.
// The fourth retail caller, Debris::Activate (0x12F768), uses it and is never refused.
constexpr uint32_t kPickupTolerantRa[3] = {0x12E818, 0x12FD74, 0x12FE54};

PS2Runtime::RecompiledFunction g_origLoadLevelDB = nullptr, g_origZip = nullptr, g_origDbInitDb = nullptr, g_origLoadUserintDB = nullptr,
                               g_origExportPoints = nullptr, g_origAddInteractive = nullptr, g_origCreatePickup = nullptr;

std::string g_active;          // map id (Registry::stages()), or empty: no map
bool g_hashed = false;         // g_hash is the hash of the last LoadLevelDB's image (taken only with a map active)
uint64_t g_hash = 0;
uint32_t g_size = 0;           // that image's exact size (zipInflateAll's return)
std::string g_stage;           // its stage file name, upper case ("CENTRAL"), from whichLevel
bool g_pending = false;        // set when LoadLevelDB completes, cleared at the next dbInitDb
uint32_t g_loadRa = 0;         // $ra of the LoadLevelDB call in progress (0: none); the zipInflateAll tap is live while set
uint32_t g_zipRa = 0;          // $ra of the zipInflateAll call that inflates the stage (0: none)
uint32_t g_zipSize = 0;        // what it returned (0: not yet)
std::string g_pupMap;          // the map whose edits were applied to the loaded stage (empty: none), for its power-up edits
std::vector<sm::PowerUpEdit> g_pups;   // its power-up moves
std::vector<sm::PowerUpEdit> g_adds;   // its power-up places added
bool g_pupLogged = false;      // the "exportPoints on another object" line was logged for this load
uint32_t g_pupStock = kNoStock;   // numPoints at the load's first exportPoints (the stock points), kNoStock before it
uint32_t g_pupTotal = 0;          // numPoints written back by that call (stock plus the places added), for the log
bool g_addFullLogged = false;     // the Interactives guard logged its refusal for this load
bool g_pickupFullLogged = false;  // the createPickup guard logged a refusal, for this load
bool g_pickupRigidLogged = false; // ... and a call it let through for lack of room (a caller that uses the result)
// WOTM_STAGE_BREAK (tick()): the first applied copy with a destructible, its source, and the break's progress for this load
sm::CopyInfo g_copy;
uint32_t g_breakStart = 0, g_breakAt = 0;   // g_frame of the fight's first tick seen (0: none yet), of the break (0: not yet)
bool g_breakDone = false;

void setActive(const std::string &id)
{
    if (id != g_active)
        std::fprintf(stderr, "[stages] active map: %s\n", id.empty() ? "none" : id.c_str());
    g_active = id;
}
const std::string &active() { return g_active; }

std::string upper(std::string s)
{
    for (char &c : s)
        c = char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
std::string lower(std::string s)
{
    for (char &c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// GameLevelNames (char[28][10] at 0x6EF748, the stage's file name by Shell::m_levelNum; wotm_save.inc has the same list). Index
// for a map's base (case-insensitive), -1 for a name with no stage file.
int levelIndex(const std::string &base)
{
    static const char *const kFiles[28] = {"default", "central", "vegas", "canyon2", "metro", "airport", "3-mile", "sanfran", "island",
                                           "tokyo", "ufo", "capitol", "training", "canyon1", "minisf", "volcano", "tokyodm", nullptr,
                                           nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "dball", "bigshot", "crush"};
    const std::string b = lower(base);
    for (int i = 1; i < 28; ++i)
        if (kFiles[i] && b == kFiles[i])
            return i;
    return -1;
}

uint32_t rd32(const uint8_t *rdram, uint32_t a) { uint32_t v; std::memcpy(&v, rdram + (a & PS2_RAM_MASK), 4); return v; }

// Highest address the stage load reached (fileStatus: next load address of the TEX, NGP and RTX lists), 0 when unreadable.
// Also what the end-of-load log prints (wotmCheckLoadEnd).
uint32_t loadEnd(const uint8_t *rdram)
{
    const uint32_t texN = rdram[kFileStatusAddr + 0x38], ngpN = rdram[kFileStatusAddr + 0x74], resN = rdram[kFileStatusAddr + 0xB0];
    if (texN > 13 || ngpN > 13 || resN > 13)
        return 0;
    return std::max({rd32(rdram, kFileStatusAddr + 4 * texN), rd32(rdram, kFileStatusAddr + 0x3C + 4 * ngpN),
                     rd32(rdram, kFileStatusAddr + 0x78 + 4 * resN)});
}

// int zipInflateAll(char *path, void *dest): the inflated size
void zipTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kZipInflateAll && g_loadRa && !g_zipRa && !g_zipSize && GPR_U32(ctx, 5) == sm::kBase)
        g_zipRa = GPR_U32(ctx, 31);
    g_origZip(rdram, ctx, runtime);
    if (g_zipRa && ctx->pc == g_zipRa)
    {
        g_zipSize = GPR_U32(ctx, 2);
        g_zipRa = 0;
    }
}

void loadLevelDBTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kLoadLevelDB)
    {
        g_loadRa = GPR_U32(ctx, 31);
        g_zipRa = 0;
        g_zipSize = 0;
        g_pending = false;
        g_hashed = false;
        g_pupMap.clear();
        g_pups.clear();
        g_adds.clear();
        g_pupStock = kNoStock;
        g_pupTotal = 0;
        g_addFullLogged = false;
        g_pickupFullLogged = g_pickupRigidLogged = false;
        g_copy = sm::CopyInfo{};
        g_breakStart = g_breakAt = 0;
        g_breakDone = false;
    }
    g_origLoadLevelDB(rdram, ctx, runtime);
    if (!g_loadRa || ctx->pc != g_loadRa)
        return;   // returned midway: the resume comes through here again (wrapAll) and finishes
    g_loadRa = 0;
    // ngpAddr[1]: the image end rounded up plus 0x80 (fileOnlyNgpFile), only a sanity bound for the size
    const uint32_t bound = rd32(rdram, kFileStatusAddr + 0x3C + 4);
    if (!g_zipSize || g_zipSize > sm::kMaxNode || bound < sm::kBase || g_zipSize > bound - sm::kBase)
    {
        std::fprintf(stderr, "[stages] stage image size not taken (zipInflateAll returned %u, image end 0x%X): no map applies to this load%s%s\n",
                     g_zipSize, bound, g_active.empty() ? "" : "; active map ", g_active.c_str());
        return;
    }
    g_size = g_zipSize;
    const uint32_t level = rd32(rdram, kLevelNumAddr);
    g_stage.clear();
    if (level < 0x1D)   // LoadLevelDB copied the stage's name to whichLevel (or the command line put it there); 0x1D+: host0:monster.ngp
    {
        const char *w = reinterpret_cast<const char *>(rdram + kWhichLevel);
        g_stage = upper(std::string(w, strnlen(w, 15)));
    }
    g_pending = true;
    if (!g_active.empty())   // the map is chosen before the load (WOTM_MATCH, the stage selection's confirm)
    {
        g_hash = sm::fnv1a(rdram + sm::kBase, g_size);
        g_hashed = true;
        std::fprintf(stderr, "[stages] image %s %u bytes, hash %016llx\n", g_stage.c_str(), g_size, static_cast<unsigned long long>(g_hash));
    }
}

void dump(const uint8_t *rdram)
{
    const char *path = std::getenv("WOTM_STAGE_DUMP");
    if (!path || !*path)
        return;
    std::ofstream img(path, std::ios::binary), res(std::string(path) + ".reserve", std::ios::binary);
    img.write(reinterpret_cast<const char *>(rdram + sm::kBase), std::streamsize(g_size));
    res.write(reinterpret_cast<const char *>(rdram + sm::kReserve), std::streamsize(sm::kReserveSize));
    std::fprintf(stderr, img && res ? "[stages] dump %s\n" : "[stages] dump %s failed\n", path);
}

void applyActive(uint8_t *rdram)
{
    const std::string id = g_active;
    if (!g_hashed)
    {
        std::fprintf(stderr, "[stages] %s: the map became active after the stage loaded, nothing applied\n", id.c_str());
        return;
    }
    const std::vector<wotm::mods::Stage> all = wotm::mods::Registry::get().stages();
    const wotm::mods::Stage *st = nullptr;
    for (const wotm::mods::Stage &s : all)
        if (s.map.id == id)
            st = &s;
    if (!st)
    {
        std::fprintf(stderr, "[stages] %s: map not found in mods/, nothing applied\n", id.c_str());
        return;
    }
    const sm::Map &map = st->map;
    if (upper(map.base) != g_stage)
    {
        std::fprintf(stderr, "[stages] %s: base %s is not the loaded stage (%s), nothing applied\n", id.c_str(), upper(map.base).c_str(),
                     g_stage.empty() ? "none" : g_stage.c_str());
        return;
    }
    if (map.baseHash != g_hash)
    {
        std::fprintf(stderr, "[stages] %s: base %s does not match this disc\n", id.c_str(), upper(map.base).c_str());
        return;
    }
    if (const uint32_t end = loadEnd(rdram); !end || end > sm::kReserve)
    {
        std::fprintf(stderr, "[stages] %s: %s (end 0x%07X, reserve 0x%07X), nothing applied\n", id.c_str(),
                     end ? "the loaded files reach the map reserve" : "fileStatus does not say where the load ends", end, sm::kReserve);
        return;
    }
    std::memset(rdram + sm::kReserve, 0, sm::kReserveSize);
    sm::Mem mem;
    mem.r.push_back({sm::kBase, rdram + sm::kBase, g_size});
    mem.r.push_back({sm::kReserve, rdram + sm::kReserve, sm::kReserveSize});
    sm::Reserve res{sm::kReserve, sm::kReserve + sm::kReserveSize};
    // pickup room for copies: MemoryStack::global's free room now (the level's pickups are allocated from it during this
    // dbInitDb), minus what the rest of the load takes from it (sm::kGlobalKeep, spec 2g)
    const uint32_t low = rd32(rdram, kGlobalStack + 4), hi1 = rd32(rdram, kGlobalStack + 0x10);
    const uint32_t room = hi1 > low ? hi1 - low : 0;
    sm::Limits lim;
    lim.pickupRoom = room > sm::kGlobalKeep ? room - sm::kGlobalKeep : 0;
    const sm::Result r = sm::apply(mem, g_size, map, res, lim);
    std::fprintf(stderr, "[stages] %s: applied %d edits, %d grid nodes relinked (%d from reserve)\n", id.c_str(), r.applied, r.relinked,
                 r.fromReserve);
    for (const std::string &l : r.log)
        std::fprintf(stderr, "[stages] %s: %s\n", id.c_str(), l.c_str());
    if (!map.copies.empty())
    {
        std::fprintf(stderr, "[stages] %s: %s\n", id.c_str(), r.copySummary.c_str());
        std::fprintf(stderr, "[stages] %s: MemoryStack::global %u bytes free at dbInitDb entry (low 0x%X, hi1 0x%X), %u kept for the rest of the load\n",
                     id.c_str(), room, low, hi1, sm::kGlobalKeep);
    }
    for (const sm::CopyInfo &c : r.copies)   // the break hook's copy: the first one with a destructible
        if (c.destructible && c.istate)
        {
            g_copy = c;
            break;
        }
    dump(rdram);
    g_pupMap = id;
    g_pups = map.powerups;
    g_adds = map.addPowerUps;
    g_pupLogged = false;
}

// PowerUpTool::exportPoints(void). Acted on at its entry only (a resumed call comes back with pc inside it). The load's first call
// (from loadPoints, on the stock points) takes the stock count, appends the places added and moves the stock points; a later
// call in the same load (sanFranUpdate, threeMileUpdateCountDown: spec 2h) passes no additions, so it appends nothing whatever
// the point count is, and reruns the moves (absolute writes: the same values). Logs come from the first call only.
void exportPointsTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kExportPoints && (!g_pups.empty() || !g_adds.empty()) && !g_pupMap.empty() && g_pupMap == g_active &&
        static_cast<int32_t>(rd32(rdram, kGameModeAddr)) != 8)
    {
        const uint32_t self = GPR_U32(ctx, 4) & PS2_RAM_MASK;
        if (self != kPowerUpTool)
        {
            if (!g_pupLogged)
                std::fprintf(stderr, "[stages] %s: exportPoints on 0x%X, not PowerUpTool::instance: power-ups not moved or added\n",
                             g_pupMap.c_str(), self);
            g_pupLogged = true;
        }
        else
        {
            const uint32_t n = rd32(rdram, kPowerUpTool + kNumPointsOff);
            const bool first = g_pupStock == kNoStock;
            if (first)
                g_pupStock = n;
            sm::Mem mem;
            mem.r.push_back({kPowerUpTool, rdram + kPowerUpTool, sm::kMaxPoints * sm::kPointSize});
            static const std::vector<sm::PowerUpEdit> kNone;
            uint32_t count = n;
            const sm::PointEditsResult r = sm::applyPointEdits(mem, kPowerUpTool, n, g_pupStock, first ? g_adds : kNone, g_pups, count);
            if (count != n)
                std::memcpy(rdram + kPowerUpTool + kNumPointsOff, &count, 4);
            if (first)
            {
                g_pupTotal = count;
                const std::string &id = g_pupMap;
                if (!g_adds.empty())
                {
                    // past 100 points appendPowerUps() logs its own "added N power-ups (M places: the game creates 100 at most)"
                    if (g_pupTotal <= sm::kMaxPowerUps || r.added.moved == 0)
                        std::fprintf(stderr, "[stages] %s: added %d power-ups (%u places)\n", id.c_str(), r.added.moved, g_pupTotal);
                    for (const std::string &l : r.added.log)
                        std::fprintf(stderr, "[stages] %s: %s\n", id.c_str(), l.c_str());
                }
                if (!g_pups.empty())
                {
                    std::fprintf(stderr, "[stages] %s: moved %d power-ups\n", id.c_str(), r.moved.moved);
                    for (const std::string &l : r.moved.log)
                        std::fprintf(stderr, "[stages] %s: %s\n", id.c_str(), l.c_str());
                }
            }
        }
    }
    g_origExportPoints(rdram, ctx, runtime);
}

// LevelPickups::createPickup: acted on at its entry only (wrapAll: a resumed call comes back with pc inside it). See the header.
void createPickupTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kCreatePickup)
    {
        const uint32_t ep = GPR_U32(ctx, 4), type = GPR_U32(ctx, 8);
        const uint32_t id1 = type ? type + 0x1388 : rd32(rdram, ep) >> 18;
        const uint32_t size = sm::levelPickupBytes(id1);
        const uint32_t low = rd32(rdram, kGlobalStack + 4), hi1 = rd32(rdram, kGlobalStack + 0x10);
        const uint32_t room = hi1 > low ? hi1 - low : 0;
        if (size && room < size + 16)
        {
            const uint32_t ra = GPR_U32(ctx, 31);
            const bool tolerant = ra == kPickupTolerantRa[0] || ra == kPickupTolerantRa[1] || ra == kPickupTolerantRa[2];
            if (!tolerant)
            {
                if (!g_pickupRigidLogged)
                    std::fprintf(stderr, "[stages] MemoryStack::global has %u bytes free, pickup id1 0x%X needs %u: the call from 0x%X "
                                 "cannot be refused (it uses the result), let through\n", room, id1, size, ra);
                g_pickupRigidLogged = true;
            }
            else
            {
                if (!g_pickupFullLogged)
                    std::fprintf(stderr, "[stages] MemoryStack::global has %u bytes free, pickup id1 0x%X needs %u (called from 0x%X): not "
                                 "created; the rest of this load's are refused silently\n", room, id1, size, ra);
                g_pickupFullLogged = true;
                SET_GPR_U32(ctx, 2, 0);
                ctx->pc = ra;
                return;
            }
        }
    }
    g_origCreatePickup(rdram, ctx, runtime);
}

// int Interactives::addInteractive(DbInteractive *p): the table's next slot. A leaf (no call, no thread switch): hooked at its entry.
// Past slot 1023 it writes nothing and returns 0 (see the header).
void addInteractiveTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    const uint32_t n = rd32(rdram, kNumInteractives);
    if (ctx->pc != kAddInteractive || n < kInteractiveSlots)
    {
        g_origAddInteractive(rdram, ctx, runtime);
        return;
    }
    if (!g_addFullLogged)
        std::fprintf(stderr, "[stages] Interactives table full (%u of %u slots): interactive 0x%X (called from 0x%X) gets id 0 and is "
                     "not registered; the rest of this load's are refused silently\n",
                     n, kInteractiveSlots, GPR_U32(ctx, 4), GPR_U32(ctx, 31));
    g_addFullLogged = true;
    SET_GPR_U32(ctx, 2, 0);
    ctx->pc = GPR_U32(ctx, 31);
}

// dbInitDb(_dbheader *db, vramAddrs *v). Acted on at its entry only (a resumed call comes back with pc inside it)
void dbInitDbTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == kDbInitDb && g_pending)
    {
        g_pending = false;
        // the stage's dbInitDb (LoadLevelFiles): the image at 0xA00000 is NGP file 0, and the monsters' NGPs follow it. LoadMonstersDB
        // runs before dbInitDb (Shell.cpp, LoadLevelFiles) and adds at least one monster NGP, so a stage load has 2 or more NGP files;
        // the shell, the pre-shell and the load screen (fileOnlyNgpFile) have exactly one. A load with no monster (the editor PR) must
        // revisit this test.
        const uint32_t ngpN = rdram[kFileStatusAddr + 0x74];
        const bool stage = GPR_U32(ctx, 4) == sm::kBase && ngpN >= 2 && ngpN <= 13 && rd32(rdram, kFileStatusAddr + 0x3C) == sm::kBase;
        if (!stage && !g_active.empty())
            std::fprintf(stderr, "[stages] %s: the dbInitDb after the stage load is not the stage's (db 0x%X, %u NGP files), nothing applied\n",
                         g_active.c_str(), GPR_U32(ctx, 4), ngpN);
        else if (stage && !g_active.empty())
            applyActive(rdram);
    }
    g_origDbInitDb(rdram, ctx, runtime);
}

// Calls a game function with up to 4 integer arguments (a0..a3); the context is restored afterwards (wotm_padsel.inc's callGuest,
// which is in another translation unit)
void callGuest(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    const PS2Runtime::RecompiledFunction f = rt->lookupFunction(fn);
    if (!f)
        return;
    const R5900Context saved = *ctx;
    const uint32_t a[4] = {a0, a1, a2, a3};
    for (int i = 0; i < 4; ++i)
        ctx->r[4 + i] = _mm_set_epi64x(0, static_cast<int64_t>(static_cast<int32_t>(a[i])));
    ctx->r[31] = _mm_set_epi64x(0, 0);
    ctx->pc = fn;
    f(rdram, ctx, rt);
    *ctx = saved;
}

// WOTM_STAGE_BREAK=<seconds> (test only): called once per frame from the Monster::update hook's slot-0 path (wotm_overrides.cpp,
// where vencer runs). `seconds` of game time (g_frame, 60 per second) after the first tick of the fight, the first applied copy
// with a destructible goes to its last state (Destructible::ChangeState(last, false, false), integer arguments); 120 frames later
// the copy's and its source's INTERACTIVE_STATE whichChild (+4) are logged. Once per load. Integers only.
void tick(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    static const int secs = [] {
        const char *v = std::getenv("WOTM_STAGE_BREAK");
        const int s = v && *v ? std::atoi(v) : 0;
        if (s > 0)
            std::fprintf(stderr, "[stages] WOTM_STAGE_BREAK=%d: the map's first copy with a destructible breaks %d s into the fight\n", s, s);
        return s > 0 ? s : 0;
    }();
    if (!secs || g_breakDone || !g_copy.istate || g_pupMap.empty() || g_pupMap != g_active)
        return;
    const uint32_t f = rd32(rdram, wotm::sym::g_frame);
    if (!g_breakStart || f < g_breakStart || (g_breakAt && f < g_breakAt))   // first tick of the fight (or g_frame went back:
    {                                                                         // rtMain resets it after a pause): count from here
        g_breakStart = f ? f : 1;
        if (g_breakAt)
            g_breakAt = g_breakStart;
        return;
    }
    if (!g_breakAt)
    {
        if (f - g_breakStart < static_cast<uint32_t>(secs) * 60u)
            return;
        const uint32_t id = (rd32(rdram, g_copy.istate) >> 7) & 0x7FF;   // the head word's id2: the copy's interactive id
        const uint32_t d = id && id < rd32(rdram, kNumInteractives) ? rd32(rdram, kDbInteractive + 4 * id) : 0;
        const uint32_t kids = rd32(rdram, g_copy.destructible + 8) >> 16;   // DESTRUCTIBLE numKids (u16 +0xA)
        if (!d || !kids)
        {
            std::fprintf(stderr, "[stages] break: copy of 0x%x has no registered destructible (interactive %u, 0x%X, %u states), nothing broken\n",
                         g_copy.source - sm::kBase, id, d, kids);
            g_breakDone = true;
            return;
        }
        callGuest(rdram, ctx, runtime, kChangeState, d, kids - 1, 0, 0);
        std::fprintf(stderr, "[stages] break: copy of 0x%x (interactive %u) to state %u\n", g_copy.source - sm::kBase, id, kids - 1);
        g_breakAt = f;
        return;
    }
    if (f - g_breakAt < 120)
        return;
    std::fprintf(stderr, "[stages] break check: copy state %u, source state %u\n", rd32(rdram, g_copy.istate + 4),
                 g_copy.sourceIstate ? rd32(rdram, g_copy.sourceIstate + 4) : 0xFFFFFFFFu);
    g_breakDone = true;
}

// Shell::LoadUserintDB (shell.ngp: the menus load, Shell.cpp's main loop, before userintMain): no map until a stage is chosen again
void loadUserintDBTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::LoadUserintDB__5Shell)
        setActive("");
    g_origLoadUserintDB(rdram, ctx, runtime);
}

// Registers `hook` at the function's entry and at every resume point inside it (the function table has one slot per address a
// thread can resume at, all pointing at the same recompiled function, which picks up from ctx->pc). A hook on the entry alone
// misses the completion of a call that returned midway; with every slot wrapped, the resume comes through the hook too.
// Returns how many slots were wrapped (0: the function is missing).
int wrapAll(PS2Runtime &runtime, uint32_t entry, uint32_t size, PS2Runtime::RecompiledFunction hook, PS2Runtime::RecompiledFunction &orig)
{
    if (!runtime.hasFunction(entry) || !(orig = runtime.lookupFunction(entry)))
        return 0;
    int n = 0;
    for (uint32_t a = entry; a < entry + size; a += 4)
        if (runtime.hasFunction(a) && runtime.lookupFunction(a) == orig)
        {
            runtime.registerFunction(a, hook);
            ++n;
        }
    return n;
}

void install(PS2Runtime &runtime)
{
    // the Interactives guard stands on its own (a leaf, size 0x2C, no call: no resume point inside it); its log resets with
    // LoadLevelDB's hook below
    const bool guard = runtime.hasFunction(kAddInteractive) && (g_origAddInteractive = runtime.lookupFunction(kAddInteractive));
    if (guard)
        runtime.registerFunction(kAddInteractive, &addInteractiveTapped);
    else
        std::fprintf(stderr, "[stages] Interactives guard unavailable: addInteractive has no function in the runtime\n");
    // size from native/wotm_syms.h (createPickup__12LevelPickups... 0x4D0); it calls the pickup constructors and init
    const bool pickupGuard = wrapAll(runtime, kCreatePickup, 0x4D0, &createPickupTapped, g_origCreatePickup) > 0;
    if (!pickupGuard)
        std::fprintf(stderr, "[stages] pickup memory guard unavailable: createPickup has no function in the runtime\n");
    // sizes from native/wotm_syms.h (LoadLevelDB__5Shell 0x140, zipInflateAll__FPcPv 0x108)
    const int nLoad = wrapAll(runtime, kLoadLevelDB, 0x140, &loadLevelDBTapped, g_origLoadLevelDB);
    const int nZip = nLoad ? wrapAll(runtime, kZipInflateAll, 0x108, &zipTapped, g_origZip) : 0;
    if (nLoad && nZip && runtime.hasFunction(kDbInitDb) && (g_origDbInitDb = runtime.lookupFunction(kDbInitDb)) &&
        runtime.hasFunction(wotm::fn::LoadUserintDB__5Shell) && (g_origLoadUserintDB = runtime.lookupFunction(wotm::fn::LoadUserintDB__5Shell)))
    {
        runtime.registerFunction(kDbInitDb, &dbInitDbTapped);
        runtime.registerFunction(wotm::fn::LoadUserintDB__5Shell, &loadUserintDBTapped);
        // size from native/wotm_syms.h (exportPoints__11PowerUpTool 0x80)
        const int nPup = wrapAll(runtime, kExportPoints, 0x80, &exportPointsTapped, g_origExportPoints);
        std::fprintf(stderr, "[stages] map hooks: LoadLevelDB (%d entry and resume points), zipInflateAll (%d), dbInitDb, exportPoints (%d)%s, "
                     "Interactives guard %s, pickup memory guard %s\n",
                     nLoad, nZip, nPup, nPup ? "" : ": power-up places stay where the disc puts them", guard ? "on" : "unavailable",
                     pickupGuard ? "on" : "unavailable");
    }
    else
        std::fprintf(stderr, "[stages] maps unavailable: LoadLevelDB, zipInflateAll, dbInitDb or LoadUserintDB has no function in the runtime\n");
}
} // namespace wotm_stagemap
