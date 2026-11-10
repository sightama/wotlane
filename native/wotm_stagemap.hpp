// ABOUTME: Map creator, pure part: stage.json, the image hash, the stage hierarchy walk, pieces and the edits (move, turn,
// ABOUTME: remove) with collision-grid relink and sphere widening, on a Mem view of guest memory (tested without the game).
#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// The rules mirror tools/stage_map.py (its docstring lists them; keep both in step): the children table, the walk (first
// parent), the piece rule, the grid coverage rule and the FNV-1a 64 hash. Spec: docs/superpowers/specs/2026-11-09-map-creator-design.md.
//
// The functions are defined in wotm_stagemap.cpp, compiled on its own with strict float rules like wotm_detmath.cpp: what they
// compute is written to guest RAM, so every build and every machine must get the same bits (a caller built with /fp:fast must
// not get its own copy). Link wotm_stagemap.cpp, wotm_json.cpp and wotm_detmath.cpp.
namespace wotm::stagemap {

constexpr uint32_t kBase = 0xA00000;                    // the stage image (file zero), as linked
constexpr uint32_t kReserve = 0x4000000 - 0x401000;     // PS2_RAM_SIZE - 0x401000: 4 MB for list nodes, ROTATEs and copies
constexpr uint32_t kReserveSize = 0x400000;
constexpr uint32_t kMaxNode = 0x2000000 - kBase;        // a node offset stays inside the 32 MB the game sees
constexpr int kNameLen = 17;                            // GameLevelLongNames[][18]

constexpr uint32_t kOpObject = 0, kOpGroup = 1, kOpLod = 2, kOpTranslate = 3, kOpRotate = 4, kOpSwitch = 6, kOpControl = 8,
                   kOpNull = 9, kOpGrid = 15, kOpAnimXform = 17, kOpActionData = 23, kOpCharInstance = 25, kOpScale = 30,
                   kOpDestructible = 31, kOpSkelBone = 35, kOpAnimScale = 38, kOpInteractive = 39, kOpPolyColl = 42;
constexpr uint32_t kWorldId = 1;                        // id1 of the world root, the one the game draws
constexpr uint32_t kPickupFirst = 0x12C0, kPickupLast = 0x13EB;   // level pickup ids (LevelPickups::createPickup)

// ActHead::type (src/game/ActionDispatch.cpp): ACT_GROUP {count +8, acts[] +0xC}, ACT_TRANSFORM {act +8}, ACT_HATPICKUP {radius
// +8, offset +0xC}. A destructible's state i has its action tree at +0xC + 12 * i + 4 (_hierdestrstate::actions).
constexpr uint16_t kActGroup = 0, kActTransform = 1, kActHatPickup = 10;

inline bool transforming(uint32_t op)
{
    return op == kOpTranslate || op == kOpRotate || op == kOpScale || op == kOpAnimXform || op == kOpAnimScale;
}

// One edit of stage.json. `node` is the piece's offset from kBase, as in the file. `move` is the new world position (a piece has
// no transform above it, so its own translation is its world position); `turn` is degrees about Z added to the piece's rotation,
// about its own position.
struct Edit { uint32_t node = 0; bool remove = false; bool move = false; float pos[3] = {0, 0, 0}; float turn = 0; };
// A power-up place moved: `index` into the stage's PTS/<STAGE>.PUP points, `from` the point's place on the disc (a guard: the edit
// only applies to a point there), `at` its new place (absolute). Power-ups are not pieces: PowerUpTool::exportPoints creates them from
// the points after the stage loads, and the runtime rewrites the points on its entry (wotm_stagemap.inc, applyPowerUps()).
struct PowerUpEdit { uint32_t index = 0; float from[3] = {0, 0, 0}; float at[3] = {0, 0, 0}; };
constexpr uint32_t kMaxPoints = 256;     // PointToolKit::points[256]
constexpr uint32_t kPointSize = 0x40;    // a point: position (_fvector) at +0, power-up type +0x2C, arg +0x2D
// A copy of a piece: `node` the source's offset (as in the file), `at` the copy's world position (absolute), `turn` degrees about Z
// added to the source's rotation, about `at`. Built from the source's disc state (apply()).
struct Copy { uint32_t node = 0; float at[3] = {0, 0, 0}; float turn = 0; };
// id and dir come from the folder (mods/<mod>/stages/<id>/), the rest from stage.json. stage.json has one "edits" list (at most
// 4096 entries of all kinds); the copies ({"copy": "0x..", ...}), the power-up moves ({"powerup": i, ...}) and the power-up places
// added ({"addpowerup": i, ...}) are kept apart from the piece edits, each in file order, and written after them in that order.
struct Map { std::string id, name, base, dir; uint64_t baseHash = 0; std::vector<Edit> edits; std::vector<PowerUpEdit> powerups;
             std::vector<Copy> copies; std::vector<PowerUpEdit> addPowerUps; };

// 64-bit FNV-1a, the same as stage_map.py's baseHash
inline uint64_t fnv1a(const uint8_t *p, size_t n)
{
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; ++i)
        h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

// Reads stage.json; false with `why` (plain English) when a field is missing or wrong. Unknown top-level keys are ignored,
// unknown keys in an edit are refused, and so is a second power-up move of the same index (additions may repeat an index).
bool parse(const std::string &json, Map &out, std::string &why);
// stage.json text for a map; parse() reads it back to the same values (floats bit for bit, -0 included)
std::string write(const Map &m);

struct Region { uint32_t base; uint8_t *p; uint32_t size; };

// Guest view: the image plus the reserve (runtime: slices of rdram). The first region must be the image, exactly the image's
// size. Reads of unmapped addresses give 0; check ok() first.
struct Mem {
    std::vector<Region> r;

    uint8_t *at(uint32_t a, uint32_t n) const
    {
        for (const Region &g : r)
            if (a >= g.base && uint64_t(a - g.base) + n <= g.size)
                return g.p + (a - g.base);
        return nullptr;
    }
    bool ok(uint32_t a, uint32_t n = 4) const { return at(a, n) != nullptr; }
    uint32_t u32(uint32_t a) const
    {
        uint32_t v = 0;
        if (const uint8_t *p = at(a, 4))
            std::memcpy(&v, p, 4);
        return v;
    }
    uint16_t u16(uint32_t a) const
    {
        uint16_t v = 0;
        if (const uint8_t *p = at(a, 2))
            std::memcpy(&v, p, 2);
        return v;
    }
    int16_t s16(uint32_t a) const { return int16_t(u16(a)); }
    uint8_t u8(uint32_t a) const
    {
        const uint8_t *p = at(a, 1);
        return p ? *p : 0;
    }
    float f32(uint32_t a) const
    {
        float v = 0;
        if (const uint8_t *p = at(a, 4))
            std::memcpy(&v, p, 4);
        return v;
    }
    void w32(uint32_t a, uint32_t v) const
    {
        if (uint8_t *p = at(a, 4))
            std::memcpy(p, &v, 4);
    }
    void wf32(uint32_t a, float v) const
    {
        if (uint8_t *p = at(a, 4))
            std::memcpy(p, &v, 4);
    }
};

// 16-aligned bump allocator over the reserve (start and size rounded up to 16); 0 when full
struct Reserve {
    uint32_t next, end;
    uint32_t alloc(uint32_t n)
    {
        const uint64_t a = (uint64_t(next) + 15) & ~uint64_t(15), size = (uint64_t(n) + 15) & ~uint64_t(15);
        if (a + size > end)
            return 0;
        next = uint32_t(a + size);
        return uint32_t(a);
    }
};

struct GridDims { int nCols = 0, nRows = 0; float cell = 0, minX = 0, minY = 0, maxX = 0, maxY = 0; };

// Cells a world box covers: cols floor((lo.x - minX) / cell) .. floor((hi.x - minX) / cell) inclusive, rows the same with Y,
// clamped to the grid; index row * nCols + col, ascending (spec 1f, stage_map.py covered(); computed in double like Python)
std::vector<int> covered(const GridDims &g, const float lo[3], const float hi[3]);

// Addresses of node n's child slots (layouts in include/hieri_types.h); empty when the node or any slot is not mapped
std::vector<uint32_t> slots(const Mem &m, uint32_t n);
// Child pointers of node n; null and unmapped pointers dropped (stage_map.py children())
std::vector<uint32_t> children(const Mem &m, uint32_t n);
// The header's root pointers (count at kBase, pointers after it)
std::vector<uint32_t> roots(const Mem &m);
// node -> first parent (0 for roots) for every node reachable from the roots: depth-first preorder, roots in order, children
// in slot order; a node reached twice keeps its first parent (stage_map.py walk_all())
std::map<uint32_t, uint32_t> parents(const Mem &m);
// Nodes under n (itself first), preorder, slot order, each once (stage_map.py subtree())
std::vector<uint32_t> subtree(const Mem &m, uint32_t n);
// A TRANSLATE or ROTATE with a parent, whose subtree holds an OBJECT or POLY_COLL, with no transforming node on its first-parent
// chain, and either under the world root (id1 1) or whose first nonzero id1 in preorder of its subtree is a level pickup
// (spec 1i, stage_map.py pieces())
bool isPiece(const Mem &m, const std::map<uint32_t, uint32_t> &par, uint32_t node);
// The hat pickup actions of DESTRUCTIBLE n's states (through ACT_GROUP and ACT_TRANSFORM, 20 levels at most), each once, in
// state and slot order. Destructible::DispatchAction drops the level pickups within the action's radius of the destructible's
// place + offset (doPickupHat, LevelPickups::hatCheck); the offset is added unrotated, so a turn turns it (apply()).
std::vector<uint32_t> hatActions(const Mem &m, uint32_t n);

// ---------------------------------------------------------------- copies (spec 2026-11-09-map-copy-design.md)

constexpr uint32_t kOpEmitter = 22, kOpScriptAction = 41;
constexpr uint16_t kActAnim = 2, kActDomino = 8, kActCinema = 9, kActPlayerPos = 11, kActRunScript = 12, kActAiNavigation = 13,
                   kActGenericEvent = 14;   // ACT_DOMINO {state +8, target +0xC}
constexpr int kInteractivesCap = 1000, kDestructiblesCap = 1400, kMonsterSlots = 8;
constexpr uint32_t kPickupBytes = 0x500;

// What copying piece `node` takes: `why` is "" when it can be copied, else the reason (logged as is). `nodes` is the copy set in
// preorder (the root first): the root, the per-instance nodes of its subtree (INTERACTIVE_STATE, DESTRUCTIBLE, SWITCH,
// PARTICLE_EMITTER, SCRIPT_ACTION, a level pickup node) and their ancestors inside the piece; the rest is shared. `bytes` their
// 16-aligned sizes (an unturned copy, without actions, paths and grid nodes); the counts are what each copy adds to the
// Interactives and Destructibles tables and to the level pickups, a node counted once per path from the root to it (the game
// registers it on each visit). `optionGroup`: a GROUP above the piece with collision-option
// flags (flags & 0xFF00: a query with that option stops at the original, not at its copies), 0 when none; logged by apply().
//
// Refused: not a piece; part of a destructible's state (a DESTRUCTIBLE or INTERACTIVE_STATE above it: a copy would outlive its
// building); under a mode-masked GROUP (flags & 0xB0: a copy would show in modes that blank the original); animated (ANIM_XFORM,
// CHAR_INSTANCE, SKEL_BONE or ANIM_SCALE in the subtree, an ANIM_XFORM, SKEL_BONE or ANIM_SCALE above, a CHAR_INSTANCE above
// when an OBJECT of the piece reads an animation channel, an ACT_ANIM in its destructibles' actions); a script action
// (ACT_CINEMA, ACT_PLAYERPOS, ACT_RUNSCRIPT, ACT_AINAVIGATION, ACT_GENERICEVENT); an action tree past 20 levels or 256 actions
// in a group (not read in part); a special id on a node dbsTraverse hands to dbProcInteractive (world, sky, font, monsters,
// pedestrians, weapons, effects, power-ups, particle textures, HUD, scripts, end points); an id1 of a fixed table on an
// INTERACTIVE_STATE or DESTRUCTIBLE (SanFran saves, CANYON2 and CAPITOL pillars, the 3-MILE vent and stacks); an emitter with a
// locator id (0x1D4C..0x1D6B, one fx per id); a shared node inside the piece holding a copied one (the copy set follows first
// parents: the copy would reach the original's state through it); a node in the copy set that nodeSize() gives 0 for. A level
// pickup id1 on any dispatched kind makes the node per-instance (a pickup id on an OBJECT therefore refuses the piece).
struct CopySet { std::string why; std::vector<uint32_t> nodes; uint32_t bytes = 0;
                 int interactives = 0, destructibles = 0, pickups = 0;
                 uint32_t optionGroup = 0; };
CopySet copySet(const Mem &m, const std::map<uint32_t, uint32_t> &par, uint32_t node);
// Size in bytes of a node of the copy set (include/hieri_types.h), 0 for a kind a copy cannot hold, for a node not wholly mapped
// and for a child count past what slots() reads (4096; a LOD's 16)
uint32_t nodeSize(const Mem &m, uint32_t n);
// The domino actions of DESTRUCTIBLE n's states (as hatActions())
std::vector<uint32_t> dominoActions(const Mem &m, uint32_t n);
// The stage's own entries, from the image as it is in `m` (after step 1 edits; a NULL ends a branch): Interactives =
// INTERACTIVE_STATE + level pickup + HomingBug nodes, Destructibles = DESTRUCTIBLE + dispatched nodes with a destructible id1,
// level pickups. Each node counts once per path from the header roots to it (dbsTraverse has no visited set), so a node held
// twice counts twice. An upper bound of what the load registers (masked GROUPs and empty nodes counted too). Call it before
// copies are linked in: it would count them as well.
struct StageCounts { int interactives = 0, destructibles = 0, pickups = 0; };
StageCounts stageCounts(const Mem &m, const std::map<uint32_t, uint32_t> &par);

// relinked: grid list nodes linked for moved paths; fromReserve: how many of them came from the reserve
struct Result { int applied = 0, refused = 0, relinked = 0, fromReserve = 0; std::vector<std::string> log; };

// Applies a map's edits to the stage image in `m` (imageSize bytes at kBase), taking grid nodes and ROTATEs from `res`. A turn
// also turns the offsets of the hat pickup actions of the destructibles under the piece (hatActions(); one that a destructible
// outside the piece also reaches is left as is and logged), so the pickups on the roof still drop where the roof is. Every
// edit is checked first, in file order (the first edit of a node stands); then the removes run, in file order, and their
// paths' list nodes go to a pool; then the moves and turns, in file order, take list nodes from the pool before the reserve.
// A move or turn is planned whole before anything is written: one that can't be done writes nothing and is logged.
Result apply(const Mem &m, uint32_t imageSize, const Map &map, Reserve &res);

// moved: power-up edits written; refused: edits left out (logged)
struct PowerUpResult { int moved = 0, refused = 0; std::vector<std::string> log; };

// Moves power-up places: `points` is the address of PowerUpTool's points (numPoints of kPointSize bytes, position at +0). In file
// order, each edit writes its `at` over the point's position when the point is at `from` (or already at `at`: a second call on
// the same points), within 0.01 on each axis; an index past numPoints, a point elsewhere or a second edit of an index is refused
// and logged, and the rest apply. Writes are absolute, so calling it again on the same points changes nothing.
PowerUpResult applyPowerUps(const Mem &m, uint32_t points, uint32_t numPoints, const std::vector<PowerUpEdit> &edits);

} // namespace wotm::stagemap
