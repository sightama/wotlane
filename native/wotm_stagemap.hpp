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
constexpr uint32_t kReserve = 0x4000000 - 0x101000;     // PS2_RAM_SIZE - 0x101000: 1 MB for list nodes and ROTATEs (spec 1d)
constexpr uint32_t kReserveSize = 0x100000;
constexpr uint32_t kMaxNode = 0x2000000 - kBase;        // a node offset stays inside the 32 MB the game sees
constexpr int kNameLen = 17;                            // GameLevelLongNames[][18]

constexpr uint32_t kOpObject = 0, kOpGroup = 1, kOpLod = 2, kOpTranslate = 3, kOpRotate = 4, kOpSwitch = 6, kOpControl = 8,
                   kOpNull = 9, kOpGrid = 15, kOpAnimXform = 17, kOpActionData = 23, kOpCharInstance = 25, kOpScale = 30,
                   kOpDestructible = 31, kOpSkelBone = 35, kOpAnimScale = 38, kOpInteractive = 39, kOpPolyColl = 42;
constexpr uint32_t kWorldId = 1;                        // id1 of the world root, the one the game draws
constexpr uint32_t kPickupFirst = 0x12C0, kPickupLast = 0x13EB;   // level pickup ids (LevelPickups::createPickup)

inline bool transforming(uint32_t op)
{
    return op == kOpTranslate || op == kOpRotate || op == kOpScale || op == kOpAnimXform || op == kOpAnimScale;
}

// One edit of stage.json. `node` is the piece's offset from kBase, as in the file. `move` is the new world position (a piece has
// no transform above it, so its own translation is its world position); `turn` is degrees about Z added to the piece's rotation,
// about its own position.
struct Edit { uint32_t node = 0; bool remove = false; bool move = false; float pos[3] = {0, 0, 0}; float turn = 0; };
// id and dir come from the folder (mods/<mod>/stages/<id>/), the rest from stage.json
struct Map { std::string id, name, base, dir; uint64_t baseHash = 0; std::vector<Edit> edits; };

// 64-bit FNV-1a, the same as stage_map.py's baseHash
inline uint64_t fnv1a(const uint8_t *p, size_t n)
{
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; ++i)
        h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

// Reads stage.json; false with `why` (plain English) when a field is missing or wrong. Unknown top-level keys are ignored,
// unknown keys in an edit are refused.
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

// relinked: grid list nodes linked for moved paths; fromReserve: how many of them came from the reserve
struct Result { int applied = 0, refused = 0, relinked = 0, fromReserve = 0; std::vector<std::string> log; };

// Applies a map's edits to the stage image in `m` (imageSize bytes at kBase), taking grid nodes and ROTATEs from `res`. Every
// edit is checked first, in file order (the first edit of a node stands); then the removes run, in file order, and their
// paths' list nodes go to a pool; then the moves and turns, in file order, take list nodes from the pool before the reserve.
// A move or turn is planned whole before anything is written: one that can't be done writes nothing and is logged.
Result apply(const Mem &m, uint32_t imageSize, const Map &map, Reserve &res);

} // namespace wotm::stagemap
