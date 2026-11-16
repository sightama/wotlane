// ABOUTME: Map editor, pure part: the stage's pieces with their place and bounds, picking along the view ray, and an editing session
// ABOUTME: (move, slide, turn, remove, copy, undo, reset) that turns into a stage.json edit list for the loader (wotm_stagemap.hpp).
#pragma once
#include "wotm_stagemap.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Spec: docs/superpowers/specs/2026-11-09-map-creator-design.md, "Editor". The editor never writes the edits itself: a session is
// an edit list, saved as stage.json and applied by the loader at the next load, like a map made by tools/stage_map.py. Nothing here
// is game state (the editor runs offline), so plain float math is fine.
namespace wotm::mapedit {

namespace sm = wotm::stagemap;

// A piece as the disc has it: its node (guest address), its place (a TRANSLATE's translation, a ROTATE's translation row) and one
// sphere around it (sm::pieceBounds; radius 0 when it has none, so it can't be picked)
struct Piece { uint32_t node = 0; float pos[3] = {0, 0, 0}; float centre[3] = {0, 0, 0}; float radius = 0; };

inline std::vector<Piece> pieces(const sm::Mem &m)
{
    const std::map<uint32_t, uint32_t> par = sm::parents(m);
    std::vector<Piece> out;
    for (const auto &kv : par)
    {
        const uint32_t n = kv.first;
        if (!sm::isPiece(m, par, n))
            continue;
        Piece p;
        p.node = n;
        const uint32_t at = (m.u32(n) & 63) == sm::kOpRotate ? n + 0x40 : n + 0x10;
        for (int k = 0; k < 3; ++k)
            p.pos[k] = m.f32(at + 4 * uint32_t(k));
        if (!sm::pieceBounds(m, n, p.centre, p.radius))
            p.radius = 0;
        out.push_back(p);
    }
    return out;
}

// Distance along the ray (eye + t * dir, dir of any length) to the sphere's near side; -1 when the ray misses it, it is behind, or
// the eye is inside it (a sphere around the eye, such as the ground's, is never what the crosshair is on)
inline float hitDistance(const float eye[3], const float dir[3], const float c[3], float r)
{
    const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (r <= 0 || len <= 0)
        return -1;
    const float d[3] = {dir[0] / len, dir[1] / len, dir[2] / len};
    const float o[3] = {c[0] - eye[0], c[1] - eye[1], c[2] - eye[2]};
    const float dist2 = o[0] * o[0] + o[1] * o[1] + o[2] * o[2];
    const float along = o[0] * d[0] + o[1] * d[1] + o[2] * d[2];
    if (dist2 < r * r || along < 0 || dist2 - along * along > r * r)
        return -1;
    return along - std::sqrt(r * r - (dist2 - along * along));
}

// One sphere a ray may pick (centre, radius)
struct Sphere { float c[3]; float r; };

// The sphere the ray reaches first, or, when that one wholly contains other spheres the ray also reaches, the smallest of those
// (a piece standing inside a bigger piece's sphere is the one aimed at); -1 for none
inline int nearestSphere(const std::vector<Sphere> &ss, const float eye[3], const float dir[3])
{
    std::vector<float> t(ss.size());
    int best = -1;
    for (size_t i = 0; i < ss.size(); ++i)
    {
        t[i] = hitDistance(eye, dir, ss[i].c, ss[i].r);
        if (t[i] >= 0 && (best < 0 || t[i] < t[size_t(best)]))
            best = int(i);
    }
    for (bool again = best >= 0; again;)
    {
        again = false;
        const Sphere &o = ss[size_t(best)];
        for (size_t i = 0; i < ss.size(); ++i)
        {
            const float dx = ss[i].c[0] - o.c[0], dy = ss[i].c[1] - o.c[1], dz = ss[i].c[2] - o.c[2];
            if (t[i] >= 0 && ss[i].r < o.r && std::sqrt(dx * dx + dy * dy + dz * dz) + ss[i].r <= o.r)
                best = int(i), again = true;
            if (again)
                break;
        }
    }
    return best;
}

// The piece whose sphere the ray reaches first (see nearestSphere); -1 for none
inline int pick(const std::vector<Piece> &ps, const float eye[3], const float dir[3])
{
    std::vector<Sphere> ss;
    for (const Piece &p : ps)
        ss.push_back({{p.centre[0], p.centre[1], p.centre[2]}, p.radius});
    return nearestSphere(ss, eye, dir);
}

// How many steps a session can undo (the oldest are dropped; each step holds a copy of every item)
constexpr int kUndoSteps = 100;

// An item as it stands now (Session::view): the piece's node (a copy: its source's), its place and turn, its sphere carried along
struct ItemView { uint32_t node = 0; bool copy = false, removed = false; float pos[3] = {0, 0, 0}; float turn = 0; float centre[3] = {0, 0, 0};
                  float radius = 0; };

// One editing session on one stage. Items are the stage's pieces (in pieces() order) followed by the copies made; the selected
// item is what move, slide, turn, remove, copy and reset act on. Every change can be undone, one step at a time.
class Session {
public:
    Session(std::string base, uint64_t baseHash, std::vector<Piece> ps) : base_(std::move(base)), hash_(baseHash), pieces_(std::move(ps))
    {
        for (size_t i = 0; i < pieces_.size(); ++i)
        {
            Item it;
            it.piece = int(i);
            for (int k = 0; k < 3; ++k)
                it.pos[k] = pieces_[i].pos[k];
            items_.push_back(it);
        }
    }

    const std::vector<Piece> &stagePieces() const { return pieces_; }
    int selected() const { return sel_; }
    // selects stock piece i (-1: nothing)
    void selectPiece(int i) { sel_ = i >= 0 && i < int(pieces_.size()) ? i : -1; }
    // selects the stock piece at `node` (its offset in the image, as stage.json writes it); false when no piece is there
    bool selectNode(uint32_t node)
    {
        for (size_t i = 0; i < pieces_.size(); ++i)
            if (pieces_[i].node - sm::kBase == node)
                return selectPiece(int(i)), true;
        sel_ = -1;
        return false;
    }
    // the item the ray reaches first, the copies included and the removed left out, as it stands now; selects it and returns it
    int pickItem(const float eye[3], const float dir[3])
    {
        std::vector<Sphere> ss(items_.size());
        for (size_t i = 0; i < items_.size(); ++i)
        {
            Sphere &sp = ss[i];
            centreOf(items_[i], sp.c);
            sp.r = items_[i].removed ? 0 : pieces_[size_t(items_[i].piece)].radius;   // a removed item has no sphere to pick
        }
        const int best = nearestSphere(ss, eye, dir);
        sel_ = best;
        return best;
    }
    // the selected item's place now; false with nothing selected (or a removed item)
    bool position(float out[3]) const
    {
        if (!live())
            return false;
        for (int k = 0; k < 3; ++k)
            out[k] = items_[size_t(sel_)].pos[k];
        return true;
    }
    // the selected item's turn now (degrees added to its disc rotation)
    float turnOf() const { return live() ? items_[size_t(sel_)].turn : 0.f; }
    // the stock piece the selected item is (or is a copy of); -1 with nothing selected
    int pieceOf() const { return live() ? items_[size_t(sel_)].piece : -1; }
    bool selectedIsCopy() const { return live() && items_[size_t(sel_)].copy; }

    void move(float dx, float dy, float dz)
    {
        if (!live())
            return;
        save();
        Item &it = items_[size_t(sel_)];
        it.pos[0] += dx, it.pos[1] += dy, it.pos[2] += dz;
    }
    // moves on the ground relative to the camera: `forward` along where it looks (yaw: degrees from +X towards +Y), `right` to
    // its right
    void slide(float forward, float right, float yawDeg)
    {
        const float a = yawDeg * 3.14159265358979f / 180.f, c = std::cos(a), s = std::sin(a);
        move(forward * c + right * s, forward * s - right * c, 0);
    }
    void turn(float deg)
    {
        if (!live())
            return;
        save();
        items_[size_t(sel_)].turn += deg;
    }
    void remove()
    {
        if (!live())
            return;
        save();
        items_[size_t(sel_)].removed = true;
        sel_ = -1;
    }
    // a copy of the selected item, offset on the ground, with its turn; the copy becomes the selection
    void copy(float dx, float dy)
    {
        if (!live())
            return;
        save();
        Item c = items_[size_t(sel_)];
        c.copy = true;
        c.pos[0] += dx, c.pos[1] += dy;
        items_.push_back(c);
        sel_ = int(items_.size()) - 1;
    }
    // the selected stock piece back to its disc place and rotation (a copy has no disc place: nothing)
    void reset()
    {
        if (!live() || items_[size_t(sel_)].copy)
            return;
        save();
        Item &it = items_[size_t(sel_)];
        for (int k = 0; k < 3; ++k)
            it.pos[k] = pieces_[size_t(it.piece)].pos[k];
        it.turn = 0;
    }
    std::vector<ItemView> view() const
    {
        std::vector<ItemView> out;
        for (const Item &it : items_)
        {
            ItemView v;
            v.node = pieces_[size_t(it.piece)].node;
            v.copy = it.copy, v.removed = it.removed, v.turn = it.turn, v.radius = pieces_[size_t(it.piece)].radius;
            for (int k = 0; k < 3; ++k)
                v.pos[k] = it.pos[k];
            centreOf(it, v.centre);
            out.push_back(v);
        }
        return out;
    }
    bool undo()
    {
        if (undo_.empty())
            return false;
        items_ = std::move(undo_.back().first);
        sel_ = undo_.back().second;
        undo_.pop_back();
        return true;
    }

    // The session as a map: a piece edit for each stock piece removed, moved or turned (in pieces() order), then a copy for each
    // copy kept (in the order made)
    sm::Map toMap(const std::string &id, const std::string &name) const
    {
        sm::Map m;
        m.id = id, m.name = name, m.base = base_, m.baseHash = hash_;
        for (const Item &it : items_)
        {
            const Piece &p = pieces_[size_t(it.piece)];
            if (it.copy)
            {
                if (it.removed)
                    continue;
                sm::Copy c;
                c.node = p.node - sm::kBase;
                for (int k = 0; k < 3; ++k)
                    c.at[k] = it.pos[k];
                c.turn = it.turn;
                m.copies.push_back(c);
                continue;
            }
            const bool moved = it.pos[0] != p.pos[0] || it.pos[1] != p.pos[1] || it.pos[2] != p.pos[2];
            if (!it.removed && !moved && it.turn == 0)
                continue;
            sm::Edit e;
            e.node = p.node - sm::kBase;
            e.remove = it.removed;
            if (!it.removed)
            {
                e.move = moved;
                for (int k = 0; k < 3; ++k)
                    e.pos[k] = it.pos[k];
                e.turn = it.turn;
            }
            m.edits.push_back(e);
        }
        return m;
    }

private:
    struct Item { int piece = 0; bool copy = false, removed = false; float pos[3] = {0, 0, 0}; float turn = 0; };

    bool live() const { return sel_ >= 0 && sel_ < int(items_.size()) && !items_[size_t(sel_)].removed; }
    void save()
    {
        undo_.push_back({items_, sel_});
        if (int(undo_.size()) > kUndoSteps)
            undo_.erase(undo_.begin());
    }
    // the item's sphere centre: the disc one, carried by the item's move (turns are about the piece's place: the centre turns too)
    void centreOf(const Item &it, float out[3]) const
    {
        const Piece &p = pieces_[size_t(it.piece)];
        const float a = it.turn * 3.14159265358979f / 180.f, c = std::cos(a), s = std::sin(a);
        const float ox = p.centre[0] - p.pos[0], oy = p.centre[1] - p.pos[1];
        out[0] = it.pos[0] + ox * c - oy * s;
        out[1] = it.pos[1] + ox * s + oy * c;
        out[2] = it.pos[2] + p.centre[2] - p.pos[2];
    }

    std::string base_;
    uint64_t hash_;
    std::vector<Piece> pieces_;
    std::vector<Item> items_;
    std::vector<std::pair<std::vector<Item>, int>> undo_;
    int sel_ = -1;
};

// One editor command: what the keys and the mouse send, and what WOTM_MAPEDIT_TEST spells out
struct Command {
    enum Kind { Select, Pick, Move, Slide, Turn, Copy, Remove, Undo, Reset, Save } kind = Undo;
    uint32_t node = 0;   // Select: the piece's offset in the image
    float a[6] = {};     // Pick: eye, direction; Move: dx, dy, dz; Slide: forward, right, camera yaw; Turn: degrees; Copy: dx, dy
};

// Runs a command on the session; true for Save (the caller writes the file and says where)
inline bool run(Session &s, const Command &c)
{
    switch (c.kind)
    {
    case Command::Select: s.selectNode(c.node); break;
    case Command::Pick: s.pickItem(c.a, c.a + 3); break;
    case Command::Move: s.move(c.a[0], c.a[1], c.a[2]); break;
    case Command::Slide: s.slide(c.a[0], c.a[1], c.a[2]); break;
    case Command::Turn: s.turn(c.a[0]); break;
    case Command::Copy: s.copy(c.a[0], c.a[1]); break;
    case Command::Remove: s.remove(); break;
    case Command::Undo: s.undo(); break;
    case Command::Reset: s.reset(); break;
    case Command::Save: return true;
    }
    return false;
}

// "select 0x<node>; move dx dy dz; turn deg; copy dx dy; remove; undo; reset; save" (';' between commands, spaces between words).
// Numbers stay inside the world (+-1000000, as stage.json's): false with `why` on the first bad command
inline bool parseScript(const std::string &text, std::vector<Command> &out, std::string &why)
{
    out.clear();
    std::stringstream all(text);
    std::string part;
    while (std::getline(all, part, ';'))
    {
        std::stringstream words(part);
        std::string verb;
        if (!(words >> verb))
            continue;
        Command c;
        int need = 0;
        if (verb == "select") c.kind = Command::Select;
        else if (verb == "move") c.kind = Command::Move, need = 3;
        else if (verb == "turn") c.kind = Command::Turn, need = 1;
        else if (verb == "copy") c.kind = Command::Copy, need = 2;
        else if (verb == "remove") c.kind = Command::Remove;
        else if (verb == "undo") c.kind = Command::Undo;
        else if (verb == "reset") c.kind = Command::Reset;
        else if (verb == "save") c.kind = Command::Save;
        else
            return why = "unknown command \"" + verb + "\"", false;
        if (c.kind == Command::Select)
        {
            std::string node;
            char *end = nullptr;
            if (!(words >> node) || node.rfind("0x", 0) != 0 || (c.node = uint32_t(std::strtoul(node.c_str() + 2, &end, 16)), *end))
                return why = "select needs a node: select 0x<offset>", false;
        }
        for (int i = 0; i < need; ++i)
        {
            std::string w;
            char *end = nullptr;
            if (!(words >> w) || (c.a[i] = std::strtof(w.c_str(), &end), *end) || !std::isfinite(c.a[i]) || std::fabs(c.a[i]) > 1e6f)
                return why = verb + " needs " + std::to_string(need) + " numbers (from -1000000 to 1000000)", false;
        }
        out.push_back(c);
    }
    return true;
}

} // namespace wotm::mapedit
