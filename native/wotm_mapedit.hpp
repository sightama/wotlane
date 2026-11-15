// ABOUTME: Map editor, pure part: the stage's pieces with their place and bounds, picking along the view ray, and an editing session
// ABOUTME: (move, slide, turn, remove, copy, undo, reset) that turns into a stage.json edit list for the loader (wotm_stagemap.hpp).
#pragma once
#include "wotm_stagemap.hpp"

#include <cmath>
#include <cstdint>
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

// Distance along the ray (eye + t * dir, dir of any length) to the sphere's near side; -1 when the ray misses it or it is behind
inline float hitDistance(const float eye[3], const float dir[3], const float c[3], float r)
{
    const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (r <= 0 || len <= 0)
        return -1;
    const float d[3] = {dir[0] / len, dir[1] / len, dir[2] / len};
    const float o[3] = {c[0] - eye[0], c[1] - eye[1], c[2] - eye[2]};
    const float along = o[0] * d[0] + o[1] * d[1] + o[2] * d[2];
    const float off2 = o[0] * o[0] + o[1] * o[1] + o[2] * o[2] - along * along;
    if (off2 > r * r)
        return -1;
    const float half = std::sqrt(r * r - off2), t = along - half >= 0 ? along - half : along + half;
    return t >= 0 ? t : -1;
}

// The piece whose sphere the ray reaches first; -1 for none
inline int pick(const std::vector<Piece> &ps, const float eye[3], const float dir[3])
{
    int best = -1;
    float bestT = 0;
    for (size_t i = 0; i < ps.size(); ++i)
    {
        const float t = hitDistance(eye, dir, ps[i].centre, ps[i].radius);
        if (t >= 0 && (best < 0 || t < bestT))
            best = int(i), bestT = t;
    }
    return best;
}

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
    // the item the ray reaches first, the copies included and the removed left out, as it stands now; selects it and returns it
    int pickItem(const float eye[3], const float dir[3])
    {
        int best = -1;
        float bestT = 0;
        for (size_t i = 0; i < items_.size(); ++i)
        {
            if (items_[i].removed)
                continue;
            float c[3];
            centreOf(items_[i], c);
            const float t = hitDistance(eye, dir, c, pieces_[size_t(items_[i].piece)].radius);
            if (t >= 0 && (best < 0 || t < bestT))
                best = int(i), bestT = t;
        }
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
    void save() { undo_.push_back({items_, sel_}); }
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

} // namespace wotm::mapedit
