// ABOUTME: Map creator, pure part (definitions of wotm_stagemap.hpp): stage.json, the hierarchy walk, pieces and the edits,
// ABOUTME: compiled on its own with strict float rules like wotm_detmath.cpp, since its results are written to guest RAM.
#include "wotm_stagemap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <set>
#include <utility>

#include "wotm_detmath.hpp"
#include "wotm_json.hpp"

namespace wotm::stagemap {

namespace {

std::string hex(uint32_t v)
{
    char b[16];
    std::snprintf(b, sizeof b, "0x%x", v);
    return b;
}

std::string num(double v)
{
    char b[48];
    std::snprintf(b, sizeof b, "%.1f", v);
    for (char *c = b; *c; ++c)
        if (*c == ',')
            *c = '.';
    return b;
}

bool validName(const std::string &s)
{
    if (s.empty() || int(s.size()) > kNameLen)
        return false;
    for (char c : s)
        if (!(c == ' ' || c == '-' || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return false;
    return true;
}

bool validBase(const std::string &s)
{
    if (s.empty() || s.size() > 15)
        return false;
    for (char c : s)
        if (!(c == '-' || c == '_' || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            return false;
    return true;
}

int hexDigit(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

// "0x" and 1 to 8 hex digits
bool parseNode(const std::string &s, uint32_t &out)
{
    if (s.size() < 3 || s.size() > 10 || s[0] != '0' || (s[1] != 'x' && s[1] != 'X'))
        return false;
    uint32_t v = 0;
    for (size_t i = 2; i < s.size(); ++i)
    {
        const int d = hexDigit(s[i]);
        if (d < 0)
            return false;
        v = v << 4 | uint32_t(d);
    }
    out = v;
    return true;
}

// The double whose shortest text reads back to f, sign of zero included: what write() puts in the file for a float
double shortest(float f)
{
    if (f == 0)
        return std::signbit(f) ? -0.0 : 0.0;
    char b[48];
    for (int prec = 1; prec <= 9; ++prec)
    {
        std::snprintf(b, sizeof b, "%.*g", prec, double(f));
        for (char *c = b; *c; ++c)
            if (*c == ',')
                *c = '.';
        json::Value v;
        if (json::parse(b, v) && float(v.asNum()) == f)
            return v.asNum();
    }
    return double(f);
}

json::Value numberOf(float f, bool integralAsInt)
{
    if (integralAsInt && f != 0 && std::floor(f) == f && std::fabs(f) < 1e9f)
        return json::Value::integer(int64_t(f));
    return json::Value::number(shortest(f));
}

bool finiteFloat(const json::Value *v, float &out)
{
    if (!v || !v->isNum())
        return false;
    out = float(v->asNum());
    return std::isfinite(out);
}

} // namespace

bool parse(const std::string &text, Map &out, std::string &why)
{
    json::Value v;
    std::string err;
    if (!json::parse(text, v, &err))
    {
        why = "not valid JSON: " + err;
        return false;
    }
    if (v.type != json::Value::Obj)
    {
        why = "not a JSON object";
        return false;
    }
    Map m;
    const json::Value *name = v.get("name"), *base = v.get("base"), *hash = v.get("baseHash"), *edits = v.get("edits");
    if (!name || name->type != json::Value::Str || !validName(name->s))
    {
        why = "\"name\" must be 1 to 17 of A-Z, 0-9, space and -";
        return false;
    }
    if (!base || base->type != json::Value::Str || !validBase(base->s))
    {
        why = "\"base\" must be a stage file name such as CENTRAL";
        return false;
    }
    if (!hash || hash->type != json::Value::Str || hash->s.size() != 16)
    {
        why = "\"baseHash\" must be 16 hex digits";
        return false;
    }
    for (char c : hash->s)
    {
        const int d = hexDigit(c);
        if (d < 0)
        {
            why = "\"baseHash\" must be 16 hex digits";
            return false;
        }
        m.baseHash = m.baseHash << 4 | uint64_t(d);
    }
    if (!edits || edits->type != json::Value::Arr || edits->arr.size() > 4096)
    {
        why = "\"edits\" must be a list (at most 4096)";
        return false;
    }
    m.name = name->s;
    m.base = base->s;
    for (size_t i = 0; i < edits->arr.size(); ++i)
    {
        const json::Value &e = edits->arr[i];
        const std::string at = "edit " + std::to_string(i + 1) + ": ";
        if (e.type != json::Value::Obj)
        {
            why = at + "not an object";
            return false;
        }
        for (const auto &kv : e.obj)
            if (kv.first != "node" && kv.first != "move" && kv.first != "turn" && kv.first != "remove")
            {
                why = at + "unknown key \"" + kv.first + "\"";
                return false;
            }
        Edit ed;
        const json::Value *node = e.get("node"), *move = e.get("move"), *turn = e.get("turn"), *remove = e.get("remove");
        if (!node || node->type != json::Value::Str || !parseNode(node->s, ed.node) || ed.node >= kMaxNode || ed.node % 4)
        {
            why = at + "\"node\" must be \"0x<offset in the stage image>\", a multiple of 4";
            return false;
        }
        if (remove)
        {
            if (remove->type != json::Value::Bool || !remove->b)
            {
                why = at + "\"remove\" can only be true";
                return false;
            }
            if (move || turn)
            {
                why = at + "\"remove\" goes alone (no move or turn)";
                return false;
            }
            ed.remove = true;
        }
        if (move)
        {
            if (move->type != json::Value::Arr || move->arr.size() != 3 || !finiteFloat(&move->arr[0], ed.pos[0]) ||
                !finiteFloat(&move->arr[1], ed.pos[1]) || !finiteFloat(&move->arr[2], ed.pos[2]))
            {
                why = at + "\"move\" must be [x, y, z] (numbers)";
                return false;
            }
            ed.move = true;
        }
        if (turn && !finiteFloat(turn, ed.turn))
        {
            why = at + "\"turn\" must be a number of degrees";
            return false;
        }
        if (!remove && !move && !turn)
        {
            why = at + "does nothing (no move, turn or remove)";
            return false;
        }
        m.edits.push_back(ed);
    }
    out = std::move(m);
    return true;
}

std::string write(const Map &m)
{
    json::Value v = json::Value::object();
    v.at("name") = json::Value::string(m.name);
    v.at("base") = json::Value::string(m.base);
    char h[24];
    std::snprintf(h, sizeof h, "%016llx", static_cast<unsigned long long>(m.baseHash));
    v.at("baseHash") = json::Value::string(h);
    json::Value &edits = v.at("edits");
    edits = json::Value::array();
    for (const Edit &e : m.edits)
    {
        json::Value o = json::Value::object();
        o.at("node") = json::Value::string(hex(e.node));
        if (e.remove)
            o.at("remove") = json::Value::boolean(true);
        else
        {
            if (e.move)
            {
                json::Value &p = o.at("move");
                p = json::Value::array();
                for (float f : e.pos)
                    p.arr.push_back(numberOf(f, false));
            }
            if (e.turn != 0)
                o.at("turn") = numberOf(e.turn, true);
        }
        edits.arr.push_back(std::move(o));
    }
    return json::dump(v);
}

std::vector<int> covered(const GridDims &g, const float lo[3], const float hi[3])
{
    auto span = [&](float a, float b, float origin, int k, int &c0, int &c1) {
        const double f0 = std::floor((double(a) - origin) / g.cell), f1 = std::floor((double(b) - origin) / g.cell);
        if (std::isnan(f0) || std::isnan(f1))
        {
            c0 = 1, c1 = 0;   // nothing
            return;
        }
        c0 = f0 < 0 ? 0 : f0 > k ? k : int(f0);
        c1 = f1 > k - 1 ? k - 1 : f1 < -1 ? -1 : int(f1);
    };
    int c0, c1, r0, r1;
    span(lo[0], hi[0], g.minX, g.nCols, c0, c1);
    span(lo[1], hi[1], g.minY, g.nRows, r0, r1);
    std::vector<int> out;
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c)
            out.push_back(r * g.nCols + c);
    return out;
}

std::vector<uint32_t> slots(const Mem &m, uint32_t n)
{
    std::vector<uint32_t> out;
    if (!m.ok(n, 4))
        return out;
    bool bad = false;
    auto arr = [&](uint32_t a, uint32_t k, uint32_t stride = 4, uint32_t off = 0) {
        k = std::min<uint32_t>(k, 4096);
        for (uint32_t i = 0; i < k && !bad; ++i)
        {
            const uint32_t s = a + stride * i + off;
            if (!m.ok(s, 4))
                bad = true;
            out.push_back(s);
        }
    };
    auto count32 = [&](uint32_t a) {
        if (!m.ok(a, 4))
            bad = true;
        return m.u32(a);
    };
    switch (m.u32(n) & 63)
    {
    case kOpGroup:          // numKids u16 +8, child[] +0x20
        if (!m.ok(n + 8, 2))
            return {};
        arr(n + 0x20, m.u16(n + 8));
        break;
    case kOpTranslate:      // numKids +8, child[] +0x1C
        arr(n + 0x1C, count32(n + 8));
        break;
    case kOpRotate:         // numKids +8, child[] +0x50
        arr(n + 0x50, count32(n + 8));
        break;
    case kOpScale:          // numKids +8, child[] +0x2C
        arr(n + 0x2C, count32(n + 8));
        break;
    case kOpSwitch:         // numKids u8 +0xB, child[] +0xC
        if (!m.ok(n + 0xB, 1))
            return {};
        arr(n + 0xC, m.u8(n + 0xB));
        break;
    case kOpLod:            // numLods +4, lods[] +0x20 (16 bytes each, child at +8)
    {
        const uint32_t k = count32(n + 4);
        if (k > 0 && k < 16)
            arr(n + 0x20, k, 16, 8);
        break;
    }
    case kOpControl:        // child1 +4, child2 +8
        arr(n + 4, 2);
        break;
    case kOpActionData:     // child +4
        arr(n + 4, 1);
        break;
    case kOpInteractive:    // child +0xC
        arr(n + 0xC, 1);
        break;
    case kOpDestructible:   // numKids u16 +0xA, states[] +0xC (12 bytes each, child at +0)
        if (!m.ok(n + 0xA, 2))
            return {};
        arr(n + 0xC, m.u16(n + 0xA), 12);
        break;
    case kOpCharInstance:   // count +0x28, child[] +0x2C
        arr(n + 0x2C, count32(n + 0x28));
        break;
    case kOpAnimXform:      // count +4, child[] +0xCC
        arr(n + 0xCC, count32(n + 4));
        break;
    case kOpSkelBone:       // count +4, child[] +0xC
        arr(n + 0xC, count32(n + 4));
        break;
    case kOpAnimScale:      // count +4, child[] +0x60
        arr(n + 0x60, count32(n + 4));
        break;
    default:
        break;
    }
    if (bad)
        return {};
    return out;
}

std::vector<uint32_t> children(const Mem &m, uint32_t n)
{
    std::vector<uint32_t> out;
    for (uint32_t s : slots(m, n))
    {
        const uint32_t k = m.u32(s);
        if (k && m.ok(k, 4))
            out.push_back(k);
    }
    return out;
}

std::vector<uint32_t> roots(const Mem &m)
{
    std::vector<uint32_t> out;
    if (!m.ok(kBase, 4))
        return out;
    const uint32_t n = std::min<uint32_t>(m.u32(kBase), 4096);
    for (uint32_t i = 0; i < n; ++i)
        if (m.ok(kBase + 4 + 4 * i, 4))
            out.push_back(m.u32(kBase + 4 + 4 * i));
    return out;
}

std::map<uint32_t, uint32_t> parents(const Mem &m)
{
    std::map<uint32_t, uint32_t> par;
    std::vector<std::pair<uint32_t, uint32_t>> stack;
    const std::vector<uint32_t> rs = roots(m);
    for (auto it = rs.rbegin(); it != rs.rend(); ++it)
        if (*it && m.ok(*it, 4))
            stack.push_back({*it, 0});
    while (!stack.empty())
    {
        const auto [n, p] = stack.back();
        stack.pop_back();
        if (par.count(n))
            continue;
        par[n] = p;
        const std::vector<uint32_t> ks = children(m, n);
        for (auto it = ks.rbegin(); it != ks.rend(); ++it)
            if (!par.count(*it))
                stack.push_back({*it, n});
    }
    return par;
}

std::vector<uint32_t> subtree(const Mem &m, uint32_t n)
{
    std::vector<uint32_t> out, stack{n};
    std::set<uint32_t> seen;
    while (!stack.empty())
    {
        const uint32_t k = stack.back();
        stack.pop_back();
        if (!seen.insert(k).second)
            continue;
        out.push_back(k);
        const std::vector<uint32_t> ks = children(m, k);
        stack.insert(stack.end(), ks.rbegin(), ks.rend());
    }
    return out;
}

bool isPiece(const Mem &m, const std::map<uint32_t, uint32_t> &par, uint32_t node)
{
    const auto it = par.find(node);
    if (it == par.end() || it->second == 0 || !m.ok(node, 4))
        return false;
    const uint32_t op = m.u32(node) & 63;
    if ((op != kOpTranslate && op != kOpRotate) || !m.ok(node, op == kOpRotate ? 0x50 : 0x1C))
        return false;
    uint32_t root = it->second;
    while (par.at(root) != 0 && !transforming(m.u32(root) & 63))
        root = par.at(root);
    if (transforming(m.u32(root) & 63))
        return false;
    bool geometry = false;
    uint32_t firstId = 0;
    for (uint32_t k : subtree(m, node))
    {
        const uint32_t w = m.u32(k);
        geometry = geometry || (w & 63) == kOpObject || (w & 63) == kOpPolyColl;
        if (!firstId)
            firstId = w >> 18;
    }
    if (!geometry)
        return false;
    return (m.u32(root) >> 18) == kWorldId || (firstId >= kPickupFirst && firstId <= kPickupLast);
}

namespace {

void hatsUnder(const Mem &m, uint32_t a, int depth, std::set<uint32_t> &seen, std::vector<uint32_t> &out)
{
    if (!a || depth > 20 || !m.ok(a, 8) || !seen.insert(a).second)
        return;
    const uint16_t type = m.u16(a);
    if (type == kActHatPickup)
    {
        if (m.ok(a, 0x18))
            out.push_back(a);
    }
    else if (type == kActGroup)
    {
        const uint32_t k = std::min<uint32_t>(m.u32(a + 8), 256);
        for (uint32_t i = 0; i < k && m.ok(a + 0xC + 4 * i, 4); ++i)
            hatsUnder(m, m.u32(a + 0xC + 4 * i), depth + 1, seen, out);
    }
    else if (type == kActTransform)
        hatsUnder(m, m.u32(a + 8), depth + 1, seen, out);
}

} // namespace

std::vector<uint32_t> hatActions(const Mem &m, uint32_t n)
{
    std::vector<uint32_t> out;
    if (!m.ok(n, 0xC) || (m.u32(n) & 63) != kOpDestructible)
        return out;
    std::set<uint32_t> seen;
    const uint32_t k = std::min<uint32_t>(m.u16(n + 0xA), 4096);
    for (uint32_t i = 0; i < k && m.ok(n + 0xC + 12 * i + 4, 4); ++i)
        hatsUnder(m, m.u32(n + 0xC + 12 * i + 4), 0, seen, out);
    return out;
}

// ---------------------------------------------------------------- edits

namespace {

// The collision grid as lists of (list node, path) per cell
struct Grid {
    uint32_t node = 0;
    GridDims d;
    std::vector<std::vector<std::pair<uint32_t, uint32_t>>> cells;
};

bool readGrid(const Mem &m, const std::map<uint32_t, uint32_t> &par, Grid &g, std::string &why)
{
    int found = 0;
    for (const auto &kv : par)
        if ((m.u32(kv.first) & 63) == kOpGrid)
            g.node = kv.first, ++found;
    if (found != 1)
    {
        why = std::to_string(found) + " collision grids (expected 1)";
        return false;
    }
    const uint32_t a = g.node;
    g.d = GridDims{m.s16(a + 4), m.s16(a + 6), m.f32(a + 8), m.f32(a + 0xC), m.f32(a + 0x10), m.f32(a + 0x14), m.f32(a + 0x18)};
    if (!m.ok(a, 0x20) || g.d.nCols <= 0 || g.d.nRows <= 0 || !(g.d.cell > 0) || !std::isfinite(g.d.cell) ||
        !m.ok(a + 0x20, 4 * uint32_t(g.d.nCols * g.d.nRows)))
    {
        why = "the collision grid is not readable";
        return false;
    }
    std::set<uint32_t> seen;
    g.cells.assign(size_t(g.d.nCols * g.d.nRows), {});
    for (size_t i = 0; i < g.cells.size(); ++i)
        for (uint32_t ln = m.u32(a + 0x20 + 4 * uint32_t(i)); ln; ln = m.u32(ln))
        {
            if (!m.ok(ln, 8) || !seen.insert(ln).second)
            {
                why = "collision grid cell " + std::to_string(i) + " has a broken list";
                return false;
            }
            g.cells[i].push_back({ln, m.u32(ln + 4)});
        }
    return true;
}

std::vector<uint32_t> pathNodes(const Mem &m, uint32_t path)
{
    std::vector<uint32_t> out;
    if (!m.ok(path, 0x24))
        return out;
    const uint32_t k = std::min<uint32_t>(m.u32(path + 0x1C), 256);
    for (uint32_t i = 0; i < k; ++i)
        if (m.ok(path + 0x24 + 4 * i, 4))
            out.push_back(m.u32(path + 0x24 + 4 * i));
    return out;
}

// node -> the collision paths listed in the grid whose node list holds it, in path address order
std::map<uint32_t, std::vector<uint32_t>> pathIndex(const Mem &m, const Grid &grid)
{
    std::set<uint32_t> listed;
    for (const auto &cell : grid.cells)
        for (const auto &ln : cell)
            listed.insert(ln.second);
    std::map<uint32_t, std::vector<uint32_t>> out;
    for (uint32_t p : listed)
    {
        std::vector<uint32_t> nodes = pathNodes(m, p);
        if (!m.ok(p, 0x24 + 4 * uint32_t(nodes.size())))
            continue;
        std::sort(nodes.begin(), nodes.end());
        nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
        for (uint32_t n : nodes)
            out[n].push_back(p);
    }
    return out;
}

// Row-vector affine transform (p' = p * R + t), like ngp.walk and the native renderer
struct Xf {
    double r[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    double t[3] = {0, 0, 0};
    void apply(const double p[3], double out[3]) const
    {
        for (int c = 0; c < 3; ++c)
            out[c] = p[0] * r[0][c] + p[1] * r[1][c] + p[2] * r[2][c] + t[c];
    }
    // `a` first, then this
    Xf after(const Xf &a) const
    {
        Xf o;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                o.r[i][j] = a.r[i][0] * r[0][j] + a.r[i][1] * r[1][j] + a.r[i][2] * r[2][j];
        apply(a.t, o.t);
        return o;
    }
    double scale() const
    {
        double s = 0;
        for (const auto &row : r)
            s = std::max(s, std::sqrt(row[0] * row[0] + row[1] * row[1] + row[2] * row[2]));
        return s;
    }
};

struct Sphere { double c[3]; double r; };

// World spheres of the first GROUP, OBJECT or LOD on each branch under n. TRANSLATE, ROTATE and SCALE are applied; ANIM_XFORM
// and ANIM_SCALE count as identity (their place comes from the animation at run time); a NULL ends the branch.
void spheres(const Mem &m, uint32_t n, const Xf &x, int depth, std::set<uint32_t> &seen, std::vector<Sphere> &out)
{
    if (depth > 64 || !m.ok(n, 4) || !seen.insert(n).second)
        return;
    const uint32_t op = m.u32(n) & 63;
    Xf here = x;
    if (op == kOpObject || op == kOpGroup || op == kOpLod)
    {
        if (!m.ok(n, 0x20))
            return;
        const double c[3] = {m.f32(n + 0x10), m.f32(n + 0x14), m.f32(n + 0x18)};
        Sphere s;
        x.apply(c, s.c);
        s.r = double(m.f32(n + 0x1C)) * x.scale();
        out.push_back(s);
        return;
    }
    if (op == kOpNull)
        return;
    if (op == kOpTranslate && m.ok(n, 0x1C))
    {
        Xf t;
        t.t[0] = m.f32(n + 0x10), t.t[1] = m.f32(n + 0x14), t.t[2] = m.f32(n + 0x18);
        here = x.after(t);
    }
    else if (op == kOpRotate && m.ok(n, 0x50))
    {
        Xf r;
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
                r.r[i][j] = m.f32(n + 0x10 + 16 * uint32_t(i) + 4 * uint32_t(j));
            r.t[i] = m.f32(n + 0x40 + 4 * uint32_t(i));
        }
        here = x.after(r);
    }
    else if (op == kOpScale && m.ok(n, 0x2C))
    {
        Xf s;   // p * scale, then + trans (ngp.walk)
        for (int i = 0; i < 3; ++i)
            s.r[i][i] = m.f32(n + 0x20 + 4 * uint32_t(i)), s.t[i] = m.f32(n + 0x10 + 4 * uint32_t(i));
        here = x.after(s);
    }
    for (uint32_t k : children(m, n))
        spheres(m, k, here, depth + 1, seen, out);
}

// What one edit writes, computed before anything is written
struct Plan {
    std::vector<std::pair<uint32_t, uint32_t>> writes;
    std::vector<std::string> notes;
    int relinked = 0, fromReserve = 0;
    void u32(uint32_t a, uint32_t v) { writes.push_back({a, v}); }
    void f32(uint32_t a, float f)
    {
        uint32_t v;
        std::memcpy(&v, &f, 4);
        writes.push_back({a, v});
    }
};

// Takes every list node of `paths` out of the grid (into `pool`, in cell order) and marks the cells it touched
void unlink(Grid &grid, const std::vector<uint32_t> &paths, std::deque<uint32_t> &pool, std::set<int> &dirty)
{
    for (size_t cell = 0; cell < grid.cells.size(); ++cell)
    {
        auto &lst = grid.cells[cell];
        for (auto it = lst.begin(); it != lst.end();)
            if (std::find(paths.begin(), paths.end(), it->second) != paths.end())
            {
                pool.push_back(it->first);
                it = lst.erase(it);
                dirty.insert(int(cell));
            }
            else
                ++it;
    }
}

// Rewrites the lists of the dirty cells (head, then next and path of each list node)
void writeCells(const Grid &grid, const std::set<int> &dirty, Plan &plan)
{
    for (int cell : dirty)
    {
        const auto &lst = grid.cells[size_t(cell)];
        plan.u32(grid.node + 0x20 + 4 * uint32_t(cell), lst.empty() ? 0 : lst[0].first);
        for (size_t j = 0; j < lst.size(); ++j)
        {
            plan.u32(lst[j].first, j + 1 < lst.size() ? lst[j + 1].first : 0);
            plan.u32(lst[j].first + 4, lst[j].second);
        }
    }
}

// cos and sin of a turn in degrees; right angles exact, the rest through wotm::detmath (same bits on every PC)
bool turnOf(float deg, float &c, float &s)
{
    double a = std::fmod(double(deg), 360.0);
    if (a < 0)
        a += 360.0;
    c = 1, s = 0;
    if (a == 0)
        return false;
    if (a == 90)
        c = 0, s = 1;
    else if (a == 180)
        c = -1, s = 0;
    else if (a == 270)
        c = 0, s = -1;
    else
    {
        const float rad = float(a) * (3.14159265358979f / 180.f);
        c = wotm::detmath::cosf(rad);
        s = wotm::detmath::sinf(rad);
    }
    return true;
}

using Box = std::pair<std::array<float, 3>, std::array<float, 3>>;

// Plans a move and/or turn of piece `node`; returns the reason for refusing it, or "" with `plan`, `grid`, `pool` and `res`
// updated (the caller commits them only on success).
std::string planMove(const Mem &m, const std::map<uint32_t, uint32_t> &par, const std::vector<uint32_t> &through,
                     const std::map<uint32_t, std::set<uint32_t>> &hatOwners, const Edit &e, uint32_t node, Grid &grid,
                     std::deque<uint32_t> &pool, Reserve &res, Plan &plan)
{
    const uint32_t op = m.u32(node) & 63, posAt = op == kOpRotate ? 0x40 : 0x10;
    const double old[3] = {m.f32(node + posAt), m.f32(node + posAt + 4), m.f32(node + posAt + 8)};
    float to[3] = {float(old[0]), float(old[1]), float(old[2])};
    if (e.move)
        to[0] = e.pos[0], to[1] = e.pos[1], to[2] = e.pos[2];
    float c = 1, s = 0;
    const bool turning = turnOf(e.turn, c, s);

    // new boxes: the old corners relative to the old place, turned, at the new place
    std::vector<Box> nb;
    auto boxes = [&]() {
        nb.clear();
        for (uint32_t p : through)
        {
            std::array<float, 3> lo, hi;
            if (!turning)
                for (int k = 0; k < 3; ++k)
                {
                    const double d = double(to[k]) - old[k];
                    lo[size_t(k)] = float(double(m.f32(p + 4 * uint32_t(k))) + d);
                    hi[size_t(k)] = float(double(m.f32(p + 0xC + 4 * uint32_t(k))) + d);
                }
            else
            {
                double mn[3] = {INFINITY, INFINITY, INFINITY}, mx[3] = {-INFINITY, -INFINITY, -INFINITY};
                for (int corner = 0; corner < 8; ++corner)
                {
                    double q[3];
                    for (int k = 0; k < 3; ++k)
                        q[k] = double(m.f32(p + (corner >> k & 1 ? 0xC : 0) + 4 * uint32_t(k))) - old[k];
                    const double w[3] = {q[0] * c - q[1] * s + to[0], q[0] * s + q[1] * c + to[1], q[2] + to[2]};
                    for (int k = 0; k < 3; ++k)
                        mn[k] = std::min(mn[k], w[k]), mx[k] = std::max(mx[k], w[k]);
                }
                for (int k = 0; k < 3; ++k)
                    lo[size_t(k)] = float(mn[k]), hi[size_t(k)] = float(mx[k]);
            }
            nb.push_back({lo, hi});
        }
    };
    boxes();

    // keep the piece's collision inside the grid's stored extent (spec 1f: the stored max, not minX + nCols * cell)
    if (!nb.empty())
    {
        double lo[2] = {INFINITY, INFINITY}, hi[2] = {-INFINITY, -INFINITY};
        for (const Box &b : nb)
            for (int k = 0; k < 2; ++k)
                lo[k] = std::min(lo[k], double(b.first[size_t(k)])), hi[k] = std::max(hi[k], double(b.second[size_t(k)]));
        const double gmin[2] = {grid.d.minX, grid.d.minY}, gmax[2] = {grid.d.maxX, grid.d.maxY};
        bool clamped = false;
        for (int k = 0; k < 2; ++k)
        {
            const double shift = lo[k] < gmin[k] ? gmin[k] - lo[k] : hi[k] > gmax[k] ? gmax[k] - hi[k] : 0;
            if (shift != 0)
                to[k] = float(double(to[k]) + shift), clamped = true;
        }
        if (clamped)
        {
            boxes();
            plan.notes.push_back("node " + hex(node - kBase) + ": kept inside the collision grid's edge, at " + num(to[0]) + " " +
                                 num(to[1]) + " " + num(to[2]));
        }
    }

    // the piece's own transform
    uint32_t replaceWith = 0;   // a new ROTATE for a turned TRANSLATE
    Xf world;
    world.t[0] = to[0], world.t[1] = to[1], world.t[2] = to[2];
    if (op == kOpRotate)
    {
        for (int i = 0; i < 3; ++i)
        {
            const uint32_t row = node + 0x10 + 16 * uint32_t(i);
            const double a = m.f32(row), b = m.f32(row + 4);
            const float n0 = turning ? float(a * c - b * s) : float(a), n1 = turning ? float(a * s + b * c) : float(b);
            if (turning)
                plan.f32(row, n0), plan.f32(row + 4, n1);
            world.r[i][0] = n0, world.r[i][1] = n1, world.r[i][2] = m.f32(row + 8);
        }
        for (int k = 0; k < 3; ++k)
            plan.f32(node + 0x40 + 4 * uint32_t(k), to[k]);
    }
    else if (!turning)
    {
        for (int k = 0; k < 3; ++k)
            plan.f32(node + 0x10 + 4 * uint32_t(k), to[k]);
    }
    else
    {
        if (m.u32(node + 0xC) != 0)
            return "its TRANSLATE carries action data, which a ROTATE has no room for";
        const uint32_t k = m.u32(node + 8);
        if (k > 4096 || !m.ok(node + 0x1C, 4 * k))
            return "its children are not readable";
        replaceWith = res.alloc(0x50 + 4 * k);
        if (!replaceWith || !m.ok(replaceWith, 0x50 + 4 * k))
            return "the reserve is full (no room for the ROTATE a turn needs)";
        const uint32_t r = replaceWith;
        plan.u32(r, (m.u32(node) & ~63u) | kOpRotate);   // ids and the other header bits kept (dbProcInteractive dispatches on the ids)
        plan.u32(r + 4, m.u32(node + 4));                 // ctx: the GS state the TRANSLATE handed to its children
        plan.u32(r + 8, k);
        plan.u32(r + 0xC, 0);
        const float mat[16] = {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, to[0], to[1], to[2], 1};
        for (uint32_t i = 0; i < 16; ++i)
            plan.f32(r + 0x10 + 4 * i, mat[i]);
        for (uint32_t i = 0; i < k; ++i)
            plan.u32(r + 0x50 + 4 * i, m.u32(node + 0x1C + 4 * i));
        world.r[0][0] = c, world.r[0][1] = s, world.r[1][0] = -s, world.r[1][1] = c;
        bool slotted = false;
        for (uint32_t sl : slots(m, par.at(node)))
            if (m.u32(sl) == node)
                plan.u32(sl, r), slotted = true;
        if (!slotted)
            return "its parent's child slot was not found";
    }

    // hat pickup offsets of the destructibles under the piece turn with it (doPickupHat adds them unrotated)
    if (turning)
    {
        const std::vector<uint32_t> sub = subtree(m, node);
        const std::set<uint32_t> inside(sub.begin(), sub.end());
        std::set<uint32_t> done;
        for (uint32_t d : sub)
            for (uint32_t h : hatActions(m, d))
            {
                if (!done.insert(h).second)
                    continue;
                const auto it = hatOwners.find(h);
                bool shared = false;
                if (it != hatOwners.end())
                    for (uint32_t o : it->second)
                        shared = shared || !inside.count(o);
                if (shared)
                {
                    plan.notes.push_back("node " + hex(node - kBase) + ": hat action " + hex(h - kBase) +
                                         " is also used by a destructible outside it, offset left as is");
                    continue;
                }
                const double x = m.f32(h + 0xC), y = m.f32(h + 0x10);
                plan.f32(h + 0xC, float(x * c - y * s));
                plan.f32(h + 0x10, float(x * s + y * c));
            }
    }

    // boxes and path entries; every path out of the grid first, so all its freed list nodes serve all its paths
    std::set<int> dirty;
    unlink(grid, through, pool, dirty);
    for (size_t i = 0; i < through.size(); ++i)
    {
        const uint32_t p = through[i];
        for (int k = 0; k < 3; ++k)
            plan.f32(p + 4 * uint32_t(k), nb[i].first[size_t(k)]), plan.f32(p + 0xC + 4 * uint32_t(k), nb[i].second[size_t(k)]);
        if (replaceWith)
        {
            const std::vector<uint32_t> nodes = pathNodes(m, p);
            for (uint32_t j = 0; j < nodes.size(); ++j)
                if (nodes[j] == node)
                    plan.u32(p + 0x24 + 4 * j, replaceWith);
        }
        for (int cell : covered(grid.d, nb[i].first.data(), nb[i].second.data()))
        {
            uint32_t ln = 0;
            if (!pool.empty())
                ln = pool.front(), pool.pop_front();
            else
            {
                ln = res.alloc(8);
                if (!ln || !m.ok(ln, 8))
                    return "the reserve is full (no room for its collision grid nodes)";
                ++plan.fromReserve;
            }
            grid.cells[size_t(cell)].push_back({ln, p});
            dirty.insert(cell);
            ++plan.relinked;
        }
    }
    writeCells(grid, dirty, plan);

    // culling spheres of the ancestor GROUPs grow to hold the piece (centers kept); LOD spheres drive LOD switching and CONTROL
    // nodes keep their see-through volumes where the disc puts them: noted only
    std::vector<Sphere> sph;
    std::set<uint32_t> seen;
    for (uint32_t k : children(m, node))
        spheres(m, k, world, 1, seen, sph);
    for (uint32_t a = par.at(node); a != 0; a = par.at(a))
    {
        const uint32_t aop = m.u32(a) & 63;
        if (aop == kOpControl)
        {
            plan.notes.push_back("node " + hex(node - kBase) + ": CONTROL " + hex(a - kBase) +
                                 " above it keeps its see-through volumes and sphere where the disc puts them");
            continue;
        }
        if ((aop != kOpGroup && aop != kOpLod) || !m.ok(a, 0x20) || sph.empty())
            continue;
        const double cx = m.f32(a + 0x10), cy = m.f32(a + 0x14), cz = m.f32(a + 0x18), rad = m.f32(a + 0x1C);
        double need = 0;
        for (const Sphere &sp : sph)
        {
            const double dx = sp.c[0] - cx, dy = sp.c[1] - cy, dz = sp.c[2] - cz;
            need = std::max(need, std::sqrt(dx * dx + dy * dy + dz * dz) + sp.r);
        }
        if (need <= rad)
            continue;
        if (aop == kOpLod)
        {
            plan.notes.push_back("node " + hex(node - kBase) + ": LOD " + hex(a - kBase) +
                                 " above it no longer covers it (sphere left as is: it drives LOD switching)");
            continue;
        }
        float r = float(need);
        if (double(r) < need)
            r = std::nextafter(r, INFINITY);
        plan.f32(a + 0x1C, r);
        plan.f32(a + 0xC, r * r);
    }
    return "";
}

} // namespace

Result apply(const Mem &m, uint32_t imageSize, const Map &map, Reserve &res)
{
    Result out;
    const std::map<uint32_t, uint32_t> par = parents(m);
    Grid grid;
    std::string noGrid;
    const bool haveGrid = readGrid(m, par, grid, noGrid);
    const std::map<uint32_t, std::vector<uint32_t>> through = haveGrid ? pathIndex(m, grid) : std::map<uint32_t, std::vector<uint32_t>>{};
    // every node that holds a pointer to each node in a child slot (a piece must hang from its parent only)
    std::map<uint32_t, std::set<uint32_t>> holders;
    for (const auto &kv : par)
        for (uint32_t sl : slots(m, kv.first))
            if (const uint32_t k = m.u32(sl))
                holders[k].insert(kv.first);
    auto pathsOf = [&](uint32_t node) {
        const auto it = through.find(node);
        return it == through.end() ? std::vector<uint32_t>{} : it->second;
    };

    // 1. checks, in file order; the first edit of a node stands
    std::vector<const Edit *> removes, moves;
    std::set<uint32_t> seen;
    for (const Edit &e : map.edits)
    {
        auto refuse = [&](const std::string &why) {
            ++out.refused;
            out.log.push_back("node " + hex(e.node) + ": " + why + ", edit refused");
        };
        if (!seen.insert(e.node).second)
        {
            refuse("edited twice (the first edit stands)");
            continue;
        }
        if (e.node >= imageSize || imageSize - e.node < 4)
        {
            refuse("outside the stage image");
            continue;
        }
        const uint32_t node = kBase + e.node;
        if (!isPiece(m, par, node))
        {
            refuse("not a piece");
            continue;
        }
        const auto h = holders.find(node);
        if (h != holders.end() && h->second.size() > 1)
        {
            for (uint32_t other : h->second)
                if (other != par.at(node))
                {
                    refuse("also hangs from node " + hex(other - kBase) + ", not only from its parent");
                    break;
                }
            continue;
        }
        (e.remove ? removes : moves).push_back(&e);
    }

    // 2. removes: NULL opcode (the game's own way to drop a node, dbs.cpp:167-168); their paths leave the grid (traversal stops at
    // the NULL anyway) and the list nodes go to the pool
    std::deque<uint32_t> pool;
    for (const Edit *e : removes)
    {
        const uint32_t node = kBase + e->node;
        Plan plan;
        plan.u32(node, (m.u32(node) & ~63u) | kOpNull);
        if (haveGrid)
        {
            std::set<int> dirty;
            unlink(grid, pathsOf(node), pool, dirty);
            writeCells(grid, dirty, plan);
        }
        for (const auto &w : plan.writes)
            m.w32(w.first, w.second);
        ++out.applied;
    }

    // 3. moves and turns; for the turns, which destructibles reach each hat pickup action. Taken from the hierarchy as the disc
    // has it (par): a hat that a destructible removed by this map also reaches still counts as shared and is left as is
    std::map<uint32_t, std::set<uint32_t>> hatOwners;
    bool anyTurn = false;
    for (const Edit *e : moves)
    {
        float c, s;
        anyTurn = anyTurn || turnOf(e->turn, c, s);
    }
    if (anyTurn)
        for (const auto &kv : par)
            for (uint32_t h : hatActions(m, kv.first))
                hatOwners[h].insert(kv.first);
    for (const Edit *e : moves)
    {
        float c, s;
        if (!e->move && !turnOf(e->turn, c, s))
        {
            ++out.applied;   // a whole number of turns: nothing to do
            continue;
        }
        if (!haveGrid)
        {
            ++out.refused;
            out.log.push_back("node " + hex(e->node) + ": " + noGrid + ", edit refused");
            continue;
        }
        const uint32_t node = kBase + e->node;
        Grid g2 = grid;
        std::deque<uint32_t> pool2 = pool;
        Reserve r2 = res;
        Plan plan;
        const std::string why = planMove(m, par, pathsOf(node), hatOwners, *e, node, g2, pool2, r2, plan);
        if (!why.empty())
        {
            ++out.refused;
            out.log.push_back("node " + hex(e->node) + ": " + why + ", edit refused");
            continue;
        }
        for (const auto &w : plan.writes)
            m.w32(w.first, w.second);
        grid = std::move(g2);
        pool = std::move(pool2);
        res = r2;
        ++out.applied;
        out.relinked += plan.relinked;
        out.fromReserve += plan.fromReserve;
        out.log.insert(out.log.end(), plan.notes.begin(), plan.notes.end());
    }
    return out;
}

} // namespace wotm::stagemap
