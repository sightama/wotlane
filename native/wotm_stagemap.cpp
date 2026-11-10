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

bool vec3(const json::Value *v, float out[3])
{
    return v && v->type == json::Value::Arr && v->arr.size() == 3 && finiteFloat(&v->arr[0], out[0]) &&
           finiteFloat(&v->arr[1], out[1]) && finiteFloat(&v->arr[2], out[2]);
}

json::Value vec3Value(const float v[3])
{
    json::Value p = json::Value::array();
    for (int k = 0; k < 3; ++k)
        p.arr.push_back(numberOf(v[k], false));
    return p;
}

// {"<key>": <index>, "from": [x, y, z], "at": [x, y, z]}: a power-up moved (key "powerup") or a place added ("addpowerup")
bool parsePowerUp(const json::Value &e, const std::string &at, const std::string &key, PowerUpEdit &out, std::string &why)
{
    for (const auto &kv : e.obj)
        if (kv.first != key && kv.first != "from" && kv.first != "at")
        {
            why = at + "unknown key \"" + kv.first + "\" in a power-up " + (key == "powerup" ? "edit" : "addition");
            return false;
        }
    const json::Value *idx = e.get(key);
    if (!idx || idx->type != json::Value::Int || idx->frac || idx->i < 0 || idx->i >= int64_t(kMaxPoints))
    {
        why = at + "\"" + key + "\" must be the power-up's index in the stage's .PUP (0 to " + std::to_string(kMaxPoints - 1) + ")";
        return false;
    }
    out.index = uint32_t(idx->i);
    if (!vec3(e.get("from"), out.from))
    {
        why = at + "\"from\" must be [x, y, z] (numbers): the power-up's place on the disc";
        return false;
    }
    if (!vec3(e.get("at"), out.at))
    {
        why = at + "\"at\" must be [x, y, z] (numbers)";
        return false;
    }
    return true;
}

// {"copy": "0x<offset>", "at": [x, y, z], "turn": <degrees>} (turn optional)
bool parseCopy(const json::Value &e, const std::string &at, Copy &out, std::string &why)
{
    for (const auto &kv : e.obj)
        if (kv.first != "copy" && kv.first != "at" && kv.first != "turn")
        {
            why = at + "unknown key \"" + kv.first + "\" in a copy edit";
            return false;
        }
    const json::Value *node = e.get("copy"), *turn = e.get("turn");
    if (!node || node->type != json::Value::Str || !parseNode(node->s, out.node) || out.node >= kMaxNode || out.node % 4)
    {
        why = at + "\"copy\" must be \"0x<offset in the stage image>\" of the piece to copy, a multiple of 4";
        return false;
    }
    if (!vec3(e.get("at"), out.at))
    {
        why = at + "\"at\" must be [x, y, z] (numbers): where the copy goes";
        return false;
    }
    if (turn && !finiteFloat(turn, out.turn))
    {
        why = at + "\"turn\" must be a number of degrees";
        return false;
    }
    return true;
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
        if (e.get("copy"))
        {
            Copy c;
            if (!parseCopy(e, at, c, why))
                return false;
            m.copies.push_back(c);
            continue;
        }
        if (e.get("addpowerup"))
        {
            PowerUpEdit pe;   // the same index can be added any number of times, and moved too
            if (!parsePowerUp(e, at, "addpowerup", pe, why))
                return false;
            m.addPowerUps.push_back(pe);
            continue;
        }
        if (e.get("powerup"))
        {
            PowerUpEdit pe;
            if (!parsePowerUp(e, at, "powerup", pe, why))
                return false;
            for (const PowerUpEdit &other : m.powerups)
                if (other.index == pe.index)
                {
                    why = at + "power-up " + std::to_string(pe.index) + " is edited twice";
                    return false;
                }
            m.powerups.push_back(pe);
            continue;
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
                o.at("move") = vec3Value(e.pos);
            if (e.turn != 0)
                o.at("turn") = numberOf(e.turn, true);
        }
        edits.arr.push_back(std::move(o));
    }
    for (const Copy &c : m.copies)
    {
        json::Value o = json::Value::object();
        o.at("copy") = json::Value::string(hex(c.node));
        o.at("at") = vec3Value(c.at);
        if (c.turn != 0)
            o.at("turn") = numberOf(c.turn, true);
        edits.arr.push_back(std::move(o));
    }
    for (int pass = 0; pass < 2; ++pass)   // power-up moves, then power-up places added
        for (const PowerUpEdit &e : pass ? m.addPowerUps : m.powerups)
        {
            json::Value o = json::Value::object();
            o.at(pass ? "addpowerup" : "powerup") = json::Value::integer(int64_t(e.index));
            o.at("from") = vec3Value(e.from);
            o.at("at") = vec3Value(e.at);
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

// ---------------------------------------------------------------- copy set

uint32_t debrisPickupBytes(uint32_t type)
{
    uint32_t size = 0x170;   // Pickup
    switch (type + 0x1388)
    {
    case 0x1388: case 0x138D: return 0;   // no type; 0x138D creates nothing
    case 0x1391: size = 0x2A0; break;     // missile truck (game mode 1)
    case 0x1392: size = 0x2B0; break;     // tank (mode 1)
    case 0x1393: case 0x1394: case 0x139F: size = 0x300; break;   // helicopters
    case 0x13A0: size = 0x1D0; break;     // turret (mode 1)
    case 0x13A1: size = 0x230; break;     // UFO
    case 0x1395: case 0x1399: case 0x139A: case 0x139B: case 0x139C: case 0x139D: case 0x139E: size = 0x4C0; break;   // rigid
    case 0x138A: case 0x138B: case 0x1397: case 0x13A2: size = 0x270; break;   // car (head bit 7), else a Pickup
    case 0x13A3: size = 0x250; break;     // subway
    default: break;
    }
    return size + 0x10;   // + the list node (alloc16(0xC))
}

namespace {

// Every action under `a` (through ACT_GROUP and ACT_TRANSFORM, 20 levels at most), each once, depth first in slot order.
// `cut` is set when a limit (20 levels, 256 actions in a group) leaves part of the tree unread.
void actionsUnder(const Mem &m, uint32_t a, int depth, std::set<uint32_t> &seen, std::vector<uint32_t> &out, bool &cut)
{
    if (!a || seen.count(a))
        return;
    if (depth > 20)
    {
        cut = true;
        return;
    }
    if (!m.ok(a, 8))
        return;
    seen.insert(a);
    out.push_back(a);
    const uint16_t type = m.u16(a);
    if (type == kActGroup)
    {
        cut = cut || m.u32(a + 8) > 256;
        const uint32_t k = std::min<uint32_t>(m.u32(a + 8), 256);
        for (uint32_t i = 0; i < k && m.ok(a + 0xC + 4 * i, 4); ++i)
            actionsUnder(m, m.u32(a + 0xC + 4 * i), depth + 1, seen, out, cut);
    }
    else if (type == kActTransform)
        actionsUnder(m, m.u32(a + 8), depth + 1, seen, out, cut);
}

// The actions of DESTRUCTIBLE n's states, each once, in state and slot order; `cut` as actionsUnder()
std::vector<uint32_t> stateActions(const Mem &m, uint32_t n, bool &cut)
{
    std::vector<uint32_t> out;
    if (!m.ok(n, 0xC) || (m.u32(n) & 63) != kOpDestructible)
        return out;
    std::set<uint32_t> seen;
    const uint32_t k = m.u16(n + 0xA);
    for (uint32_t i = 0; i < k && m.ok(n + 0xC + 12 * i + 4, 4); ++i)
        actionsUnder(m, m.u32(n + 0xC + 12 * i + 4), 0, seen, out, cut);
    return out;
}

// The debris pickups of DESTRUCTIBLE n (amendment A4): per state, the chunks with a pickup type in the ACT_DEBRIS actions that
// state reaches (each once per state; a state's actions fire when the destructible enters it), and their pickup memory.
// Counting each ACT_DEBRIS once per state is exact: no stock tree reaches one twice in a state, and maps copy action trees but
// never change their shape; ACT_DEBRIS leaves stay shared.
void debrisOf(const Mem &m, uint32_t n, uint64_t &count, uint64_t &bytes)
{
    if (!m.ok(n, 0xC) || (m.u32(n) & 63) != kOpDestructible)
        return;
    const uint32_t k = m.u16(n + 0xA);
    for (uint32_t i = 0; i < k && m.ok(n + 0xC + 12 * i + 4, 4); ++i)
    {
        std::set<uint32_t> seen;
        std::vector<uint32_t> acts;
        bool cut = false;
        actionsUnder(m, m.u32(n + 0xC + 12 * i + 4), 0, seen, acts, cut);
        for (uint32_t a : acts)
        {
            if (m.u16(a) != kActDebris || !m.ok(a, 0x10))
                continue;
            const uint32_t chunks = std::min(m.u32(a + 8), kDebrisChunks);
            for (uint32_t c = 0; c < chunks; ++c)
            {
                const uint32_t ch = a + 0x10 + 0x30 * c;
                if (!m.ok(ch, 0x30))
                    break;
                if (const uint32_t type = uint32_t(m.u16(ch + 0xE) >> 5) & 31)
                    ++count, bytes += debrisPickupBytes(type);
            }
        }
    }
}

// Kinds dbsTraverse hands to dbProcInteractive when their id1 is nonzero (src/common/dbs.cpp:141-420); 31 and 39 always go there
bool dispatched(uint32_t op)
{
    switch (op)
    {
    case 0: case 1: case 2: case 3: case 4: case 6: case 11: case 17: case 30: case 40: case 41:
        return true;
    default:
        return false;
    }
}

bool pickupId(uint32_t id) { return id >= kPickupFirst && id <= kPickupLast; }

// How many times a traversal from `starts` (each counted as often as listed) reaches each node: the number of paths through
// child slots (a node held by two slots is reached twice, as dbsTraverse has no visited set), saturating at 2^40. Nodes on a
// cycle keep only the paths from outside it (a cycle would hang the game; 2b found none).
std::map<uint32_t, uint64_t> pathCounts(const Mem &m, const std::vector<uint32_t> &starts)
{
    constexpr uint64_t kMax = uint64_t(1) << 40;
    std::map<uint32_t, uint64_t> count;
    std::map<uint32_t, uint32_t> holders;   // reachable node -> child slots of reachable nodes that hold it
    std::map<uint32_t, std::vector<uint32_t>> kids;
    std::vector<uint32_t> stack;
    for (uint32_t s : starts)
        if (s && m.ok(s, 4))
            ++count[s], stack.push_back(s);
    while (!stack.empty())
    {
        const uint32_t n = stack.back();
        stack.pop_back();
        if (kids.count(n))
            continue;
        std::vector<uint32_t> &ks = kids[n] = children(m, n);
        for (uint32_t k : ks)
        {
            ++holders[k];
            if (!kids.count(k))
                stack.push_back(k);
        }
    }
    std::vector<uint32_t> ready;
    for (const auto &kv : kids)
        if (!holders.count(kv.first))
            ready.push_back(kv.first);
    while (!ready.empty())
    {
        const uint32_t n = ready.back();
        ready.pop_back();
        const uint64_t c = count[n];
        for (uint32_t k : kids.at(n))
        {
            count[k] = std::min(count[k] + c, kMax);
            if (--holders.at(k) == 0)
                ready.push_back(k);
        }
    }
    return count;
}

// x += v, saturating at INT_MAX (an estimate past any cap)
void addCount(int &x, uint64_t v)
{
    x = int(std::min<uint64_t>(uint64_t(x) + v, 0x7FFFFFFF));
}

// DESTRUCTIBLE n's debris pickups (debrisOf()), `times` over: added to `count` and `bytes`, saturating (2^40 bytes is past any
// room)
void addDebris(const Mem &m, uint32_t n, uint64_t times, int &count, uint64_t &bytes)
{
    uint64_t c = 0, b = 0;
    debrisOf(m, n, c, b);
    addCount(count, std::min<uint64_t>(c, 1u << 20) * times);
    bytes = std::min<uint64_t>(bytes + std::min<uint64_t>(b, 1u << 30) * std::min<uint64_t>(times, 1u << 20), uint64_t(1) << 40);
}

// dbProcInteractive's ranges a copy must not register twice (src/game/db.cpp:187-255), nullptr for a free id
const char *specialRange(uint32_t id)
{
    if (id == 0)
        return nullptr;
    if (id < 0x20)
        return "world, sky or font";
    if (id <= 0x400)
        return "monsters";
    if (id == 0xBB8)
        return "pedestrians";
    if (id >= 0xFA0 && id < 0x11F8)
        return "weapons";
    if (id >= 0x11F8 && id < 0x12C0)
        return "effects";
    if (id >= 0x13EC && id < 0x1450)
        return "power-ups";
    if (id >= 0x1B58 && id < 0x1C20)
        return "particle textures";
    if (id >= 0x1C20 && id < 0x2134)
        return "HUD";
    if (id >= 0x2134 && id < 0x2328)
        return "scripts";
    if (id >= 0x2328 && id < 0x3E80)
        return "end points";
    return nullptr;
}

// id1 tables that the game fills from INTERACTIVE_STATE and DESTRUCTIBLE ids (Destructible.cpp:161-169, :287-315)
const char *fixedTable(uint32_t id)
{
    if (id >= 0x275D && id <= 0x2762)
        return "SanFran saves";
    if (id >= 0x2775 && id <= 0x2777)
        return "CANYON2 pillars";
    if (id >= 0x27AE && id <= 0x27B3)
        return "CAPITOL pillars";
    if (id == 0x2712 || (id >= 0x2715 && id <= 0x2718))
        return "the 3-MILE vent and stacks";
    return nullptr;
}

const char *opName(uint32_t op)
{
    switch (op)
    {
    case kOpObject: return "OBJECT";
    case kOpGroup: return "GROUP";
    case kOpLod: return "LOD";
    case kOpTranslate: return "TRANSLATE";
    case kOpRotate: return "ROTATE";
    case kOpSwitch: return "SWITCH";
    case kOpControl: return "CONTROL";
    case kOpNull: return "NULL";
    case kOpGrid: return "GRID";
    case kOpAnimXform: return "ANIM_XFORM";
    case kOpEmitter: return "PARTICLE_EMITTER";
    case kOpActionData: return "ACTION_DATA";
    case kOpCharInstance: return "CHAR_INSTANCE";
    case kOpScale: return "SCALE";
    case kOpDestructible: return "DESTRUCTIBLE";
    case kOpSkelBone: return "SKEL_BONE";
    case kOpAnimScale: return "ANIM_SCALE";
    case kOpInteractive: return "INTERACTIVE_STATE";
    case kOpScriptAction: return "SCRIPT_ACTION";
    case kOpPolyColl: return "POLY_COLL";
    default: return "unknown";
    }
}

// "a SKEL_BONE", "an ANIM_XFORM": the kind's name with its article (stage_map.py with_article())
std::string withArticle(uint32_t op)
{
    const std::string name = opName(op);
    return (std::string("AEIOU").find(name[0]) != std::string::npos ? "an " : "a ") + name;
}

const char *scriptAction(uint16_t type)
{
    switch (type)
    {
    case kActCinema: return "ACT_CINEMA";
    case kActPlayerPos: return "ACT_PLAYERPOS";
    case kActRunScript: return "ACT_RUNSCRIPT";
    case kActAiNavigation: return "ACT_AINAVIGATION";
    case kActGenericEvent: return "ACT_GENERICEVENT";
    default: return nullptr;
    }
}

// An OBJECT that reads an animation channel (an index >= 0 at +0x30..+0x3C, src/common/dbs.cpp:154-163): only a CHAR_INSTANCE
// above it gives it one. An OBJECT too short to tell counts as reading one.
bool readsChannel(const Mem &m, uint32_t n)
{
    if (!m.ok(n, 0x3E))
        return true;
    for (uint32_t k = 0; k < 7; ++k)
        if (m.s16(n + 0x30 + 2 * k) >= 0)
            return true;
    return false;
}

} // namespace

uint32_t nodeSize(const Mem &m, uint32_t n)
{
    if (!m.ok(n, 4))
        return 0;
    // a child count past what slots() reads (4096) gives 0: the piece is refused rather than copied in part
    auto kids = [&](uint32_t base, uint32_t count, uint32_t each) { return count <= 4096 ? base + each * count : 0u; };
    uint32_t size = 0;
    switch (m.u32(n) & 63)
    {
    case kOpGroup: size = m.ok(n + 8, 2) ? kids(0x20, m.u16(n + 8), 4) : 0; break;
    case kOpTranslate: size = m.ok(n + 8) ? kids(0x1C, m.u32(n + 8), 4) : 0; break;
    case kOpRotate: size = m.ok(n + 8) ? kids(0x50, m.u32(n + 8), 4) : 0; break;
    case kOpScale: size = m.ok(n + 8) ? kids(0x2C, m.u32(n + 8), 4) : 0; break;
    case kOpSwitch: size = m.ok(n + 0xB, 1) ? 0xC + 4 * uint32_t(m.u8(n + 0xB)) : 0; break;
    case kOpLod: size = m.ok(n + 4) && m.u32(n + 4) < 16 ? 0x20 + 16 * m.u32(n + 4) : 0; break;
    case kOpControl: size = m.ok(n + 0xC, 1) ? 0x20 + 4 * uint32_t(m.u8(n + 0xC)) : 0; break;
    case kOpInteractive: size = 0x10; break;
    case kOpDestructible: size = m.ok(n + 0xA, 2) ? kids(0xC, m.u16(n + 0xA), 12) : 0; break;
    case kOpEmitter: size = 0xB0; break;
    case kOpScriptAction: size = 0x60; break;
    default: return 0;
    }
    return size && m.ok(n, size) ? size : 0;
}

std::vector<uint32_t> dominoActions(const Mem &m, uint32_t n)
{
    std::vector<uint32_t> out;
    bool cut = false;
    for (uint32_t a : stateActions(m, n, cut))
        if (m.u16(a) == kActDomino && m.ok(a, 0x10))
            out.push_back(a);
    return out;
}

CopySet copySet(const Mem &m, const std::map<uint32_t, uint32_t> &par, uint32_t node)
{
    CopySet c;
    auto refused = [](std::string why) {
        CopySet r;
        r.why = std::move(why);
        return r;
    };
    if (!isPiece(m, par, node))
        return refused("not a piece");
    // what it hangs under (dropped by the copy, which hangs under the wrapper)
    uint32_t charAbove = 0;
    for (uint32_t a = par.at(node); a; a = par.at(a))
    {
        const uint32_t op = m.u32(a) & 63;
        if (op == kOpDestructible || op == kOpInteractive)
            return refused("in a destructible's state (" + std::string(opName(op)) + " " + hex(a - kBase) + " above it)");
        if (op == kOpGroup)
        {
            const uint16_t flags = m.u16(a + 0xA);
            if (flags & 0xB0)
                return refused("mode-masked (GROUP " + hex(a - kBase) + " above it, flags " + hex(flags) + ")");
            if ((flags & 0xFF00) && !c.optionGroup)
                c.optionGroup = a;
        }
        if (op == kOpCharInstance && !charAbove)
            charAbove = a;
        if (op == kOpAnimXform || op == kOpAnimScale || op == kOpSkelBone)
            return refused("animated (" + withArticle(op) + " above it)");
    }
    // the subtree in preorder with each node's first parent inside it (subtree()'s order)
    std::vector<uint32_t> order, stack{node};
    std::map<uint32_t, uint32_t> up;
    {
        std::vector<uint32_t> from{0};
        while (!stack.empty())
        {
            const uint32_t k = stack.back(), p = from.back();
            stack.pop_back(), from.pop_back();
            if (up.count(k))
                continue;
            up[k] = p;
            order.push_back(k);
            const std::vector<uint32_t> ks = children(m, k);
            for (auto it = ks.rbegin(); it != ks.rend(); ++it)
                stack.push_back(*it), from.push_back(k);
        }
    }
    bool channel = false;
    for (uint32_t n : order)
    {
        const uint32_t op = m.u32(n) & 63;
        if (op == kOpAnimXform || op == kOpCharInstance || op == kOpSkelBone || op == kOpAnimScale)
            return refused("animated (" + withArticle(op) + " in it)");
        channel = channel || (op == kOpObject && readsChannel(m, n));
    }
    if (charAbove && channel)
        return refused("animated (the CHAR_INSTANCE " + hex(charAbove - kBase) + " above it drives its OBJECTs' channels)");
    for (uint32_t n : order)
        if ((m.u32(n) & 63) == kOpDestructible)
        {
            bool cut = false;
            for (uint32_t a : stateActions(m, n, cut))
            {
                const uint16_t type = m.u16(a);
                if (type == kActAnim)
                    return refused("animated (an ACT_ANIM action)");
                if (const char *s = scriptAction(type))
                    return refused(std::string("a script action (") + s + ")");
            }
            if (cut)
                return refused("an action tree past 20 levels or 256 actions in a group (" + hex(n - kBase) + ")");
        }
    // ids, per-instance nodes and what each copy registers
    std::set<uint32_t> keep{node};
    const std::map<uint32_t, uint64_t> paths = pathCounts(m, {node});   // each copy registers a node once per path to it
    for (uint32_t n : order)
    {
        const uint32_t w = m.u32(n), op = w & 63, id = w >> 18;
        const uint64_t times = paths.count(n) ? std::max<uint64_t>(paths.at(n), 1) : 1;
        bool own = op == kOpSwitch || op == kOpEmitter || op == kOpScriptAction;
        if (op == kOpInteractive || op == kOpDestructible)
        {
            if (const char *t = fixedTable(id))
                return refused("holds id1 " + hex(id) + " (a fixed id1 table: " + t + ")");
            own = true;
            addCount(op == kOpInteractive ? c.interactives : c.destructibles, times);
            if (op == kOpDestructible)
                addDebris(m, n, times, c.debrisPickups, c.debrisBytes);
        }
        else if (dispatched(op) && id)
        {
            if (const char *r = specialRange(id))
                return refused("holds special id " + hex(id) + " (" + r + ")");
            if (pickupId(id))
                own = true, addCount(c.interactives, times), addCount(c.pickups, times);
            else if (id >= 0x1450 && id < 0x1B58)
                addCount(c.destructibles, times);
        }
        if (op == kOpEmitter && id >= 0x1D4C && id <= 0x1D6B)
            return refused("an emitter with locator id " + hex(id));
        for (uint32_t k = n; own && k && keep.insert(k).second;)
            k = up.at(k);
    }
    // the copy set follows first parents; a node of the piece that is shared (not copied) but holds a copied node would let the
    // copy reach the original's per-instance nodes through it
    for (uint32_t n : order)
        if (!keep.count(n))
            for (uint32_t k : children(m, n))
                if (keep.count(k))
                    return refused("a shared node inside it (" + hex(n - kBase) + ") holds a copied node (" + hex(k - kBase) + ")");
    for (uint32_t n : order)
        if (keep.count(n))
        {
            const uint32_t size = nodeSize(m, n);
            if (!size)
            {
                const uint32_t op = m.u32(n) & 63;
                return refused("its copy would hold a node of kind " + std::to_string(op) + " (" + opName(op) + ")");
            }
            c.nodes.push_back(n);
            c.bytes += (size + 15) & ~15u;
        }
    return c;
}

StageCounts stageCounts(const Mem &m, const std::map<uint32_t, uint32_t> &par)
{
    StageCounts s;
    const std::map<uint32_t, uint64_t> paths = pathCounts(m, roots(m));
    for (const auto &kv : par)
    {
        const uint32_t w = m.u32(kv.first), op = w & 63, id = w >> 18;
        const uint64_t times = paths.count(kv.first) ? std::max<uint64_t>(paths.at(kv.first), 1) : 1;
        if (op == kOpInteractive)
            addCount(s.interactives, times);
        else if (op == kOpDestructible)
            addCount(s.destructibles, times), addDebris(m, kv.first, times, s.debrisPickups, s.debrisBytes);
        else if (dispatched(op) && id)
        {
            if (pickupId(id))
                addCount(s.interactives, times), addCount(s.pickups, times);
            else if (id == 0x11F8 || id == 0x1203)
                addCount(s.interactives, times);
            else if (id >= 0x1450 && id < 0x1B58)
                addCount(s.destructibles, times);
        }
    }
    return s;
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
    std::vector<std::string> sourceNotes;   // a copy's notes about its source: logged for its first applied copy only
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

struct Placement { float to[3] = {0, 0, 0}; float c = 1, s = 0; bool turning = false, clamped = false; std::vector<Box> boxes; };

// Where a piece whose disc place is `old` lands at `want` turned by `turn`: the boxes of `paths` (read from `m`), turned about
// `old` and moved to the place, and the place shifted back inside the grid's stored extent when they leave it (spec 1f)
Placement place(const Mem &m, const std::vector<uint32_t> &paths, const double old[3], const float want[3], float turn,
                const GridDims &g)
{
    Placement pl;
    float *to = pl.to;
    to[0] = want[0], to[1] = want[1], to[2] = want[2];
    float &c = pl.c, &s = pl.s;
    pl.turning = turnOf(turn, c, s);
    const bool turning = pl.turning;

    // new boxes: the old corners relative to the old place, turned, at the new place
    std::vector<Box> &nb = pl.boxes;
    auto boxes = [&]() {
        nb.clear();
        for (uint32_t p : paths)
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
        const double gmin[2] = {g.minX, g.minY}, gmax[2] = {g.maxX, g.maxY};
        for (int k = 0; k < 2; ++k)
        {
            const double shift = lo[k] < gmin[k] ? gmin[k] - lo[k] : hi[k] > gmax[k] ? gmax[k] - hi[k] : 0;
            if (shift != 0)
                to[k] = float(double(to[k]) + shift), pl.clamped = true;
        }
        if (pl.clamped)
            boxes();
    }
    return pl;
}

// The 3x3 rows of ROTATE `node` (read from `m`) turned by (c, s) about Z: each row's x and y turned, z kept
void rootMatrix(const Mem &m, uint32_t node, bool turning, float c, float s, float rows[3][3])
{
    for (int i = 0; i < 3; ++i)
    {
        const uint32_t row = node + 0x10 + 16 * uint32_t(i);
        const double a = m.f32(row), b = m.f32(row + 4);
        rows[i][0] = turning ? float(a * c - b * s) : float(a);
        rows[i][1] = turning ? float(a * s + b * c) : float(b);
        rows[i][2] = m.f32(row + 8);
    }
}

// Piece `node`'s place in `m`: a ROTATE's fourth row (+0x40), a TRANSLATE's trans (+0x10)
void discPlace(const Mem &m, uint32_t node, double old[3])
{
    const uint32_t posAt = (m.u32(node) & 63) == kOpRotate ? 0x40 : 0x10;
    for (uint32_t k = 0; k < 3; ++k)
        old[k] = m.f32(node + posAt + 4 * k);
}

// The first 0x50 bytes of the ROTATE that stands, at `r`, for TRANSLATE `node` (read from `m`) turned by (c, s) at `to`: the
// TRANSLATE's header word with the ROTATE opcode (ids and the other header bits kept: dbProcInteractive dispatches on the ids),
// its ctx (the GS state the TRANSLATE handed to its children), `k` children, no action data, the turned matrix
void planTurnedTranslate(const Mem &m, uint32_t node, uint32_t r, uint32_t k, float c, float s, const float to[3], Plan &plan)
{
    plan.u32(r, (m.u32(node) & ~63u) | kOpRotate);
    plan.u32(r + 4, m.u32(node + 4));
    plan.u32(r + 8, k);
    plan.u32(r + 0xC, 0);
    const float mat[16] = {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, to[0], to[1], to[2], 1};
    for (uint32_t i = 0; i < 16; ++i)
        plan.f32(r + 0x10 + 4 * i, mat[i]);
}

// The world transform of piece `node` (read from `m`) placed at `to` turned by (c, s): a ROTATE's rows turned (rootMatrix()),
// a turned TRANSLATE's (c, s) rows, else none
Xf placedXf(const Mem &m, uint32_t node, bool turning, float c, float s, const float to[3])
{
    Xf world;
    world.t[0] = to[0], world.t[1] = to[1], world.t[2] = to[2];
    if ((m.u32(node) & 63) == kOpRotate)
    {
        float rows[3][3];
        rootMatrix(m, node, turning, c, s, rows);
        for (int i = 0; i < 3; ++i)
            world.r[i][0] = rows[i][0], world.r[i][1] = rows[i][1], world.r[i][2] = rows[i][2];
    }
    else if (turning)
        world.r[0][0] = c, world.r[0][1] = s, world.r[1][0] = -s, world.r[1][1] = c;
    return world;
}

// The radius a sphere centred where node `a`'s is (read from `m`) needs to hold `sph`
double reach(const Mem &m, uint32_t a, const std::vector<Sphere> &sph)
{
    const double cx = m.f32(a + 0x10), cy = m.f32(a + 0x14), cz = m.f32(a + 0x18);
    double need = 0;
    for (const Sphere &sp : sph)
    {
        const double dx = sp.c[0] - cx, dy = sp.c[1] - cy, dz = sp.c[2] - cz;
        need = std::max(need, std::sqrt(dx * dx + dy * dy + dz * dz) + sp.r);
    }
    return need;
}

// Writes radius `need` (rounded up to a float) and its square into the sphere of GROUP `a`
void planRadius(uint32_t a, double need, Plan &plan)
{
    float r = float(need);
    if (double(r) < need)
        r = std::nextafter(r, INFINITY);
    plan.f32(a + 0x1C, r);
    plan.f32(a + 0xC, r * r);
}

// Writes into hat action `to` the offset (+0xC, +0x10) of hat action `h` (read from `m`) turned by (c, s): doPickupHat adds the
// offset to the destructible's place unrotated. Moves and copies share it, so a turned copy's hat has a turned piece's bits.
void planHatTurn(const Mem &m, uint32_t h, uint32_t to, float c, float s, Plan &plan)
{
    const double x = m.f32(h + 0xC), y = m.f32(h + 0x10);
    plan.f32(to + 0xC, float(x * c - y * s));
    plan.f32(to + 0x10, float(x * s + y * c));
}

// Plans a move and/or turn of piece `node`; returns the reason for refusing it, or "" with `plan`, `grid`, `pool` and `res`
// updated (the caller commits them only on success).
std::string planMove(const Mem &m, const std::map<uint32_t, uint32_t> &par, const std::vector<uint32_t> &through,
                     const std::map<uint32_t, std::set<uint32_t>> &hatOwners, const Edit &e, uint32_t node, Grid &grid,
                     std::deque<uint32_t> &pool, Reserve &res, Plan &plan)
{
    const uint32_t op = m.u32(node) & 63;
    double old[3];
    discPlace(m, node, old);
    float want[3] = {float(old[0]), float(old[1]), float(old[2])};
    if (e.move)
        want[0] = e.pos[0], want[1] = e.pos[1], want[2] = e.pos[2];
    const Placement pl = place(m, through, old, want, e.turn, grid.d);
    const float *to = pl.to;
    const float c = pl.c, s = pl.s;
    const bool turning = pl.turning;
    const std::vector<Box> &nb = pl.boxes;
    if (pl.clamped)
        plan.notes.push_back("node " + hex(node - kBase) + ": kept inside the collision grid's edge, at " + num(to[0]) + " " +
                             num(to[1]) + " " + num(to[2]));

    // the piece's own transform
    uint32_t replaceWith = 0;   // a new ROTATE for a turned TRANSLATE
    const Xf world = placedXf(m, node, turning, c, s, to);
    if (op == kOpRotate)
    {
        float rows[3][3];
        rootMatrix(m, node, turning, c, s, rows);
        for (int i = 0; i < 3; ++i)
        {
            const uint32_t row = node + 0x10 + 16 * uint32_t(i);
            if (turning)
                plan.f32(row, rows[i][0]), plan.f32(row + 4, rows[i][1]);
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
        planTurnedTranslate(m, node, r, k, c, s, to, plan);
        for (uint32_t i = 0; i < k; ++i)
            plan.u32(r + 0x50 + 4 * i, m.u32(node + 0x1C + 4 * i));
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
                planHatTurn(m, h, h, c, s, plan);
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
        const double rad = m.f32(a + 0x1C), need = reach(m, a, sph);
        if (need <= rad)
            continue;
        if (aop == kOpLod)
        {
            plan.notes.push_back("node " + hex(node - kBase) + ": LOD " + hex(a - kBase) +
                                 " above it no longer covers it (sphere left as is: it drives LOD switching)");
            continue;
        }
        planRadius(a, need, plan);
    }
    return "";
}

// A copy's private action trees (decision 8). The leaves a copy owns: an ACT_HATPICKUP when the copy is turned (its offset turned
// as a turned piece's) or when step 1 changed its offset in the live image (the copy keeps the disc's), an ACT_DOMINO whose target is a copied node (retargeted to the copy). Every ACT_GROUP and ACT_TRANSFORM
// on the way from a state's actions to an owned leaf is copied too; everything else is shared. One per copy: `memo` maps an
// action to what the copy stores in its place, so an action reached twice (two states, two groups, a cycle) is copied once.
// It reads whole trees with no depth limit of its own: it relies on copySet() refusing a source whose action trees go past 20
// levels or 256 actions in a group (so nothing it reaches is cut, and its recursion stays shallow).
struct ActionCopier
{
    const Mem &m, &snap;
    const std::map<uint32_t, uint32_t> &copyOf;
    bool turning;
    float c, s;
    Reserve &res;
    Plan &plan;
    std::set<uint32_t> needs;   // actions with an owned leaf under them (themselves included)
    std::map<uint32_t, uint32_t> memo;
    std::string why;

    // The kinds a copy may hold, and their sizes (an ACT_GROUP's count is at most 256: copySet() refuses more)
    static uint32_t sizeOf(const Mem &mm, uint32_t a)
    {
        switch (mm.u16(a))
        {
        case kActGroup: return 0xC + 4 * std::min<uint32_t>(mm.u32(a + 8), 256);
        case kActTransform: return 0x50;
        case kActHatPickup: return 0x18;
        case kActDomino: return 0x10;
        default: return 0;
        }
    }
    static std::vector<uint32_t> next(const Mem &mm, uint32_t a)
    {
        std::vector<uint32_t> out;
        if (mm.u16(a) == kActGroup)
        {
            const uint32_t k = std::min<uint32_t>(mm.u32(a + 8), 256);
            for (uint32_t i = 0; i < k && mm.ok(a + 0xC + 4 * i, 4); ++i)
                out.push_back(mm.u32(a + 0xC + 4 * i));
        }
        else if (mm.u16(a) == kActTransform)
            out.push_back(mm.u32(a + 8));
        return out;
    }
    bool owned(uint32_t a) const
    {
        const uint16_t type = snap.u16(a);
        // a hat is also owned when the live image no longer holds its disc offset (step 1 turned the source): a shared hat would
        // drop the copy's pickups at that turn
        if (type == kActHatPickup)
            return snap.ok(a, 0x18) && (turning || !m.ok(a, 0x18) || m.u32(a + 0xC) != snap.u32(a + 0xC) ||
                                        m.u32(a + 0x10) != snap.u32(a + 0x10));
        return type == kActDomino && snap.ok(a, 0x10) && copyOf.count(snap.u32(a + 0xC));
    }

    // Marks the actions reachable from `starts` that lead to an owned leaf; returns every reachable action, in visit order
    std::vector<uint32_t> mark(const std::vector<uint32_t> &starts)
    {
        std::vector<uint32_t> order, stack(starts.rbegin(), starts.rend());
        std::set<uint32_t> seen;
        std::map<uint32_t, std::vector<uint32_t>> from;   // action -> the actions that lead to it
        while (!stack.empty())
        {
            const uint32_t a = stack.back();
            stack.pop_back();
            if (!a || !snap.ok(a, 8) || !seen.insert(a).second)
                continue;
            order.push_back(a);
            const std::vector<uint32_t> ks = next(snap, a);
            for (auto it = ks.rbegin(); it != ks.rend(); ++it)
                from[*it].push_back(a), stack.push_back(*it);
        }
        std::vector<uint32_t> up;
        for (uint32_t a : order)
            if (owned(a))
                up.push_back(a);
        while (!up.empty())
        {
            const uint32_t a = up.back();
            up.pop_back();
            if (!needs.insert(a).second)
                continue;
            const auto it = from.find(a);
            if (it != from.end())
                up.insert(up.end(), it->second.begin(), it->second.end());
        }
        return order;
    }

    // What the copy stores in place of action `a`: `a` itself when nothing under it is owned, else its private copy (0 and `why`
    // set when the reserve has no room)
    uint32_t copy(uint32_t a)
    {
        if (!why.empty() || !needs.count(a))
            return a;
        const auto it = memo.find(a);
        if (it != memo.end())
            return it->second;
        const uint32_t size = sizeOf(snap, a);
        const uint32_t na = size && snap.ok(a, size) ? res.alloc(size) : 0;
        if (!na || !m.ok(na, size))
        {
            why = "the reserve is full (no room for its actions)";
            return 0;
        }
        memo[a] = na;
        for (uint32_t i = 0; i < size; i += 4)
            plan.u32(na + i, snap.u32(a + i));
        switch (snap.u16(a))
        {
        case kActGroup:
        {
            const std::vector<uint32_t> ks = next(snap, a);
            for (uint32_t i = 0; i < ks.size(); ++i)
                plan.u32(na + 0xC + 4 * i, copy(ks[i]));
            break;
        }
        case kActTransform: plan.u32(na + 8, copy(snap.u32(a + 8))); break;
        case kActHatPickup:   // unturned, the disc bytes copied above stand as they are
            if (turning)
                planHatTurn(snap, a, na, c, s, plan);
            break;
        case kActDomino: plan.u32(na + 0xC, copyOf.at(snap.u32(a + 0xC))); break;
        default: break;
        }
        return na;
    }
};

// The wrapper GROUP of the copies under header root `root`: reserved at `at` before the first copy (0: no room), with room for
// `cap` children; `kids` is 0 until the first copy that applies links it into the root's last slot. `need`: the radius its
// children need so far.
struct Wrapper { uint32_t root = 0, at = 0, cap = 0, kids = 0; double need = 0; };

// What a copy's planning changed in the grid's lists and the pool, to put back when the copy is refused: each touched cell's
// list as it was before its first change, and the pool's list nodes taken, in order
struct GridUndo {
    std::map<int, std::vector<std::pair<uint32_t, uint32_t>>> cells;
    std::vector<uint32_t> taken;
    void restore(Grid &grid, std::deque<uint32_t> &pool)
    {
        for (auto &kv : cells)
            grid.cells[size_t(kv.first)] = std::move(kv.second);
        for (auto it = taken.rbegin(); it != taken.rend(); ++it)
            pool.push_front(*it);
        cells.clear(), taken.clear();
    }
};

// Plans copy `e` of piece `node` (edit number `editNo` in the file write() writes), whose copy set `cs` and collision `paths`
// are read from `snap` (the image on entry), under wrapper `w`; returns the reason for refusing it, or "" with `plan`, `w`,
// `grid`, `pool`, `res` and `info` updated (the caller commits `w`, `res` and the writes only on success, and puts `grid` and
// `pool` back with `undo` on a refusal). Live state (the root's last slot and sphere, the grid's lists) is read from `m` and
// `grid`.
std::string planCopy(const Mem &m, const Mem &snap, const CopySet &cs, const std::vector<uint32_t> &paths, const Copy &e,
                     uint32_t node, size_t editNo, Grid &grid, std::deque<uint32_t> &pool, GridUndo &undo, Wrapper &w, Reserve &res,
                     Plan &plan, CopyInfo &info)
{
    double old[3];
    discPlace(snap, node, old);
    const Placement pl = place(snap, paths, old, e.at, e.turn, grid.d);
    const float *to = pl.to;
    if (pl.clamped)
        plan.notes.push_back("copy " + hex(node - kBase) + " (edit " + std::to_string(editNo) +
                             "): kept inside the collision grid's edge, at " + num(to[0]) + " " + num(to[1]) + " " + num(to[2]));
    const uint32_t op = snap.u32(node) & 63;
    const bool toRotate = op == kOpTranslate && pl.turning;   // a turned TRANSLATE's copy is a ROTATE (as a turn)
    if (toRotate && snap.u32(node + 0xC) != 0)
        return "its TRANSLATE carries action data, which a ROTATE has no room for";
    if (!w.at)
        return "the reserve is full (no room for the wrapper)";
    if (w.kids > w.cap)   // the old child + cap copies: never with apply()'s count
        return "its wrapper is full";

    // every node of the copy set gets its copy, mapped by the original's address: two copied holders of one node both get its
    // one copy
    std::map<uint32_t, uint32_t> copyOf;
    const uint32_t rootKids = snap.u32(node + 8);
    for (uint32_t n : cs.nodes)
    {
        const uint32_t size = n == node && toRotate ? 0x50 + 4 * rootKids : nodeSize(snap, n);
        const uint32_t a = size ? res.alloc(size) : 0;
        if (!a || !m.ok(a, size))
            return "the reserve is full (no room for its nodes)";
        copyOf[n] = a;
    }
    auto mapped = [&](uint32_t v) {
        const auto it = copyOf.find(v);
        return it == copyOf.end() ? v : it->second;
    };
    for (uint32_t n : cs.nodes)
    {
        const uint32_t a = copyOf.at(n);
        if (n == node && toRotate)
        {
            planTurnedTranslate(snap, node, a, rootKids, pl.c, pl.s, to, plan);
            for (uint32_t i = 0; i < rootKids; ++i)
                plan.u32(a + 0x50 + 4 * i, mapped(snap.u32(node + 0x1C + 4 * i)));
            continue;
        }
        const uint32_t size = nodeSize(snap, n);
        for (uint32_t i = 0; i < size; i += 4)
            plan.u32(a + i, snap.u32(n + i));
        for (uint32_t sl : slots(snap, n))
            plan.u32(a + (sl - n), mapped(snap.u32(sl)));
    }

    // the copied DESTRUCTIBLEs' action trees (decision 8), after the nodes; dominoes it keeps are noted
    {
        ActionCopier ac{m, snap, copyOf, pl.turning, pl.c, pl.s, res, plan, {}, {}, {}};
        std::vector<uint32_t> dests, starts;
        for (uint32_t n : cs.nodes)
            if ((snap.u32(n) & 63) == kOpDestructible)
            {
                dests.push_back(n);
                const uint32_t k = snap.u16(n + 0xA);
                for (uint32_t i = 0; i < k && snap.ok(n + 0xC + 12 * i + 4, 4); ++i)
                    starts.push_back(snap.u32(n + 0xC + 12 * i + 4));
            }
        const std::vector<uint32_t> reached = ac.mark(starts);
        for (uint32_t d : dests)
        {
            const uint32_t k = snap.u16(d + 0xA);
            for (uint32_t i = 0; i < k && snap.ok(d + 0xC + 12 * i + 4, 4); ++i)
            {
                const uint32_t at = d + 0xC + 12 * i + 4, was = snap.u32(at), now = ac.copy(was);
                if (!ac.why.empty())
                    return ac.why;
                if (now != was)
                    plan.u32(copyOf.at(d) + (at - d), now);
            }
        }
        const std::vector<uint32_t> sub = subtree(snap, node);
        const std::set<uint32_t> inside(sub.begin(), sub.end());
        int outside = 0;
        for (uint32_t a : reached)
        {
            if (snap.u16(a) != kActDomino || !snap.ok(a, 0x10))
                continue;
            const uint32_t t = snap.u32(a + 0xC);
            if (!t || copyOf.count(t))
                continue;
            if (inside.count(t))
                plan.sourceNotes.push_back("copy " + hex(node - kBase) + ": domino action " + hex(a - kBase) + " targets " +
                                           hex(t - kBase) + " inside it, not copied (kept: its copies act on the original's)");
            else
                ++outside;
        }
        if (outside)
            plan.sourceNotes.push_back("copy " + hex(node - kBase) + ": " + std::to_string(outside) +
                                       " domino actions point outside it (kept: its copies knock down the original's neighbours)");
    }

    // the root at the copy's place
    const uint32_t root = copyOf.at(node);
    if (op == kOpRotate)
    {
        float rows[3][3];
        rootMatrix(snap, node, pl.turning, pl.c, pl.s, rows);
        if (pl.turning)
            for (uint32_t i = 0; i < 3; ++i)
                plan.f32(root + 0x10 + 16 * i, rows[i][0]), plan.f32(root + 0x14 + 16 * i, rows[i][1]);
        for (uint32_t k = 0; k < 3; ++k)
            plan.f32(root + 0x40 + 4 * k, to[k]);
    }
    else if (!toRotate)
        for (uint32_t k = 0; k < 3; ++k)
            plan.f32(root + 0x10 + 4 * k, to[k]);

    // collision paths (decision 2): [header root, wrapper, the source's path from the piece root down with copied nodes
    // replaced]. The ancestors between the root and the piece (SWITCH, LOD, CONTROL, flagged GROUP) do not hang over the copy,
    // so its path does not go through them either. Header words (traceId +0x18, flags +0x20) copied, the box from place().
    // Linked with the step 1 rule: list nodes from the pool first (step 1's removes and moves left them), then the reserve.
    std::set<int> dirty;
    for (size_t i = 0; i < paths.size(); ++i)
    {
        const uint32_t p = paths[i];
        const std::vector<uint32_t> nodes = pathNodes(snap, p);
        if (nodes.size() != snap.u32(p + 0x1C))
            return "a collision path through it is not readable";
        const auto from = std::find(nodes.begin(), nodes.end(), node);
        if (from == nodes.end())
            return "a collision path through it does not reach it";
        std::vector<uint32_t> np{w.root, w.at};
        for (auto it = from; it != nodes.end(); ++it)
            np.push_back(mapped(*it));
        const uint32_t len = uint32_t(np.size()), size = 0x24 + 4 * len;
        const uint32_t q = res.alloc(size);
        if (!q || !m.ok(q, size))
            return "the reserve is full (no room for its collision paths)";
        const Box &b = pl.boxes[i];
        for (uint32_t k = 0; k < 3; ++k)
            plan.f32(q + 4 * k, b.first[k]), plan.f32(q + 0xC + 4 * k, b.second[k]);
        plan.u32(q + 0x18, snap.u32(p + 0x18));
        plan.u32(q + 0x1C, len);
        plan.u32(q + 0x20, snap.u32(p + 0x20));
        for (uint32_t j = 0; j < len; ++j)
            plan.u32(q + 0x24 + 4 * j, np[j]);
        for (int cell : covered(grid.d, b.first.data(), b.second.data()))
        {
            uint32_t ln = 0;
            if (!pool.empty())
                ln = pool.front(), pool.pop_front(), undo.taken.push_back(ln);
            else
            {
                ln = res.alloc(8);
                if (!ln || !m.ok(ln, 8))
                    return "the reserve is full (no room for its collision grid nodes)";
                ++plan.fromReserve;
            }
            undo.cells.emplace(cell, grid.cells[size_t(cell)]);   // its list before this copy's first change
            grid.cells[size_t(cell)].push_back({ln, q});
            dirty.insert(cell);
            ++plan.relinked;
        }
    }
    writeCells(grid, dirty, plan);

    // the wrapper: on the first copy its header (the root's opcode and isRelocated bits, ids 0: an id1 of 1 would make it a
    // second world root; the root's ctx, flags and sphere centre) and the root's last slot, its old child kept first
    if (!w.kids)
    {
        const std::vector<uint32_t> sl = slots(m, w.root);
        if (sl.empty())
            return "the root above it has no child slot";
        const uint32_t last = sl.back(), oldChild = m.u32(last);
        plan.u32(w.at, m.u32(w.root) & 0x7F);
        plan.u32(w.at + 4, m.u32(w.root + 4));
        for (uint32_t k = 0x10; k < 0x1C; k += 4)
            plan.u32(w.at + k, m.u32(w.root + k));
        plan.u32(w.at + 0x20, oldChild);
        plan.u32(last, w.at);
        w.kids = 1;
        std::vector<Sphere> sph;
        std::set<uint32_t> seen;
        spheres(m, oldChild, Xf{}, 1, seen, sph);
        w.need = reach(m, w.root, sph);
    }
    plan.u32(w.at + 0x20 + 4 * w.kids, root);
    ++w.kids;
    plan.u32(w.at + 8, w.kids | uint32_t(m.u16(w.root + 0xA)) << 16);   // numKids u16 +8, flags u16 +0xA

    // spheres: the wrapper's radius holds its children, the root's grows to hold the copy (centres kept)
    std::vector<Sphere> sph;
    std::set<uint32_t> seen;
    const Xf world = placedXf(snap, node, pl.turning, pl.c, pl.s, to);
    for (uint32_t k : children(snap, node))
        spheres(snap, k, world, 1, seen, sph);
    w.need = std::max(w.need, reach(m, w.root, sph));
    planRadius(w.at, w.need, plan);
    const double rootNeed = reach(m, w.root, sph);
    if (!sph.empty() && rootNeed > double(m.f32(w.root + 0x1C)))
        planRadius(w.root, rootNeed, plan);

    // the first DESTRUCTIBLE (preorder) and the INTERACTIVE_STATE that holds it (a pair the game drives together); with no
    // DESTRUCTIBLE, the first INTERACTIVE_STATE
    info.source = node, info.root = root;
    uint32_t dest = 0;
    for (uint32_t n : cs.nodes)
        if ((snap.u32(n) & 63) == kOpDestructible)
        {
            dest = n;
            break;
        }
    for (uint32_t n : cs.nodes)
        if ((snap.u32(n) & 63) == kOpInteractive && (!dest || snap.u32(n + 0xC) == dest))
        {
            info.istate = copyOf.at(n), info.sourceIstate = n;
            break;
        }
    info.destructible = dest ? copyOf.at(dest) : 0;
    return "";
}

// What a copy of piece `node` loses by hanging under the wrapper (decision 12, amendment A3): each CONTROL, LOD and SWITCH between
// the header root and the piece (read from `snap`, as `par`), and the GROUP with collision-option flags copySet() found (a
// query with that option stops at the original, not at its copies). Logged once per source.
std::vector<std::string> ancestorNotes(const Mem &snap, const std::map<uint32_t, uint32_t> &par, uint32_t node, const CopySet &cs)
{
    std::vector<std::string> out;
    const std::string head = "copy " + hex(node - kBase) + ": ";
    for (uint32_t a = par.at(node); a && par.at(a); a = par.at(a))
    {
        const uint32_t op = snap.u32(a) & 63;
        if (op == kOpControl || op == kOpLod || op == kOpSwitch)
            out.push_back(head + (op == kOpControl ? "CONTROL " : op == kOpLod ? "LOD " : "SWITCH ") + hex(a - kBase) +
                          " above it does not hang over its copies");
    }
    if (cs.optionGroup && par.count(cs.optionGroup) && par.at(cs.optionGroup))   // the header root's flags: the wrapper has them
        out.push_back(head + "GROUP " + hex(cs.optionGroup - kBase) + " above it does not hang over its copies (its collision-option " +
                      "flags " + hex(snap.u16(cs.optionGroup + 0xA)) + ": a query with that option still hits them)");
    return out;
}

// Text of a byte count in KB, rounded to the nearest
std::string kbOf(uint64_t bytes) { return std::to_string((bytes + 512) / 1024); }

} // namespace

Result apply(const Mem &m, uint32_t imageSize, const Map &map, Reserve &res, const Limits &lim)
{
    Result out;
    const Reserve entry = res;   // the summary counts the reserve from here (step 1's ROTATEs and list nodes too)
    // copies read their sources from the image as it is on entry (the disc state), before any edit writes
    std::vector<uint8_t> disc;
    if (!map.copies.empty())
        if (const uint8_t *p = m.at(kBase, imageSize))
            disc.assign(p, p + imageSize);
    const Mem snap{{{kBase, disc.data(), uint32_t(disc.size())}}};
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

    // 4. copies, in file order, from the snapshot (par, through and holders describe it: they were taken before any edit).
    // One wrapper per header root above a copy's source, reserved first with room for every copy under it.
    if (map.copies.empty())
        return out;
    // caps (decision 7, amendment A4), counted on the image after the step 1 edits and before any copy is linked in: slot 0,
    // the monster slots and the stage's own entries (its debris pickups included), then each applied copy's. Pickup memory:
    // kPickupBytes per level pickup and debrisPickupBytes() per debris pickup.
    const StageCounts stock = stageCounts(m, parents(m));
    int64_t interactives = 1 + kMonsterSlots + int64_t(stock.interactives) + stock.debrisPickups,
            destructibles = stock.destructibles;
    uint64_t pickupBytes = uint64_t(std::max(stock.pickups, 0)) * kPickupBytes + stock.debrisBytes;
    std::vector<uint32_t> tops(map.copies.size(), 0), wrapOrder;
    std::map<uint32_t, Wrapper> wraps;
    for (size_t i = 0; i < map.copies.size(); ++i)
    {
        const uint32_t off = map.copies[i].node, node = kBase + off;
        if (off >= imageSize || imageSize - off < 4 || !par.count(node) || !par.at(node))
            continue;
        uint32_t top = node;
        while (par.at(top))
            top = par.at(top);
        tops[i] = top;
        if (!wraps.count(top))
            wrapOrder.push_back(top), wraps[top].root = top;
        ++wraps[top].cap;
    }
    for (uint32_t top : wrapOrder)
    {
        Wrapper &w = wraps[top];
        if ((m.u32(top) & 63) != kOpGroup)
            continue;
        const uint32_t size = 0x20 + 4 * (1 + w.cap);
        Reserve r2 = res;
        const uint32_t a = r2.alloc(size);
        if (a && m.ok(a, size))
            w.at = a, res = r2;
    }
    std::map<uint32_t, CopySet> sets;   // per source
    std::set<uint32_t> noted;           // sources whose notes are logged
    // the logs of decision 12, per source (and reason) in order of first appearance
    struct Tally { uint32_t source; int copies; uint64_t bytes; int gridNodes; };
    std::vector<Tally> applied;
    std::vector<std::pair<std::pair<uint32_t, std::string>, int>> refusals;
    for (size_t i = 0; i < map.copies.size(); ++i)
    {
        const Copy &e = map.copies[i];
        auto refuse = [&](const std::string &why) {
            ++out.copyRefused;
            const std::pair<uint32_t, std::string> key{e.node, why};
            const auto it = std::find_if(refusals.begin(), refusals.end(), [&](const auto &x) { return x.first == key; });
            if (it == refusals.end())
                refusals.push_back({key, 1});
            else
                ++it->second;
        };
        if (disc.empty())
        {
            refuse("the stage image is not readable");
            continue;
        }
        if (e.node >= imageSize || imageSize - e.node < 4)
        {
            refuse("outside the stage image");
            continue;
        }
        const uint32_t node = kBase + e.node;
        if (!isPiece(snap, par, node))
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
        if (!haveGrid)
        {
            refuse(noGrid);
            continue;
        }
        if (!sets.count(node))
            sets[node] = copySet(snap, par, node);
        const CopySet &cs = sets.at(node);
        if (!cs.why.empty())
        {
            refuse(cs.why);
            continue;
        }
        if ((m.u32(tops[i]) & 63) != kOpGroup)
        {
            refuse("the root above it is not a GROUP");
            continue;
        }
        // caps, before anything is allocated; a copy that adds nothing to a table passes whatever its count
        const int64_t addInteractives = int64_t(cs.interactives) + cs.debrisPickups;
        const uint64_t addPickupBytes = uint64_t(cs.pickups) * kPickupBytes + cs.debrisBytes;
        if (addInteractives && interactives + addInteractives > lim.interactives)
        {
            refuse("the Interactives table would pass its cap (" + std::to_string(lim.interactives) + "; each copy adds " +
                   std::to_string(addInteractives) +
                   (cs.debrisPickups ? ", " + std::to_string(cs.debrisPickups) + " of them debris pickups" : std::string()) + ")");
            continue;
        }
        if (cs.destructibles && destructibles + cs.destructibles > lim.destructibles)
        {
            refuse("the Destructibles table would pass its cap (" + std::to_string(lim.destructibles) + "; each copy adds " +
                   std::to_string(cs.destructibles) + ")");
            continue;
        }
        if (addPickupBytes && pickupBytes + addPickupBytes > lim.pickupRoom)
        {
            refuse("not enough pickup memory (" + kbOf(lim.pickupRoom) + " KB for pickups; each copy adds " + kbOf(addPickupBytes) +
                   " KB)");
            continue;
        }
        // planned whole on copies of the wrapper and the reserve, the grid's lists and the pool with an undo list: a refused copy
        // writes nothing and takes nothing (decision 11)
        Wrapper w2 = wraps.at(tops[i]);
        GridUndo undo;
        Reserve r2 = res;
        Plan plan;
        CopyInfo info;
        const std::string why =
            planCopy(m, snap, cs, pathsOf(node), e, node, map.edits.size() + i + 1, grid, pool, undo, w2, r2, plan, info);
        if (!why.empty())
        {
            undo.restore(grid, pool);
            refuse(why);
            continue;
        }
        for (const auto &w : plan.writes)
            m.w32(w.first, w.second);
        const uint64_t took = uint64_t(r2.next) - res.next;
        wraps[tops[i]] = w2;
        res = r2;
        interactives += addInteractives, destructibles += cs.destructibles, pickupBytes += addPickupBytes;
        ++out.copied;
        out.copies.push_back(info);
        const auto t = std::find_if(applied.begin(), applied.end(), [&](const Tally &x) { return x.source == e.node; });
        if (t == applied.end())
            applied.push_back({e.node, 1, took, plan.relinked});
        else
            ++t->copies, t->bytes += took, t->gridNodes += plan.relinked;
        out.log.insert(out.log.end(), plan.notes.begin(), plan.notes.end());
        if (noted.insert(node).second)
        {
            const std::vector<std::string> above = ancestorNotes(snap, par, node, cs);
            out.log.insert(out.log.end(), above.begin(), above.end());
            out.log.insert(out.log.end(), plan.sourceNotes.begin(), plan.sourceNotes.end());
        }
    }

    // decision 12: what each source gave, then the refusals per source and reason, then the summary (integers only)
    for (const Tally &t : applied)
        out.log.push_back("copied " + hex(t.source) + " x" + std::to_string(t.copies) + " (" + kbOf(t.bytes) + " KB, " +
                          std::to_string(t.gridNodes) + " grid nodes)");
    for (const auto &rf : refusals)
        out.log.push_back("copy " + hex(rf.first.first) + " x" + std::to_string(rf.second) + " refused: " + rf.first.second);
    out.interactives = int(std::min<int64_t>(interactives, INT32_MAX));
    out.destructibles = int(std::min<int64_t>(destructibles, INT32_MAX));
    out.pickupBytes = uint32_t(std::min<uint64_t>(pickupBytes, 0xFFFFFFFFu));
    out.copySummary = "copies: " + std::to_string(out.copied) + " applied, " + std::to_string(out.copyRefused) + " refused; reserve " +
                      kbOf(uint64_t(res.next) - entry.next) + " of " + kbOf(uint64_t(entry.end) - entry.next) + " KB; interactives " +
                      std::to_string(out.interactives) + " of " + std::to_string(lim.interactives) + ", destructibles " +
                      std::to_string(out.destructibles) + " of " + std::to_string(lim.destructibles) + ", pickup room " +
                      kbOf(out.pickupBytes) +
                      (lim.pickupRoom == 0xFFFFFFFFu ? std::string(" KB (no limit)") : " of " + kbOf(lim.pickupRoom) + " KB");
    return out;
}

PowerUpResult applyPowerUps(const Mem &m, uint32_t points, uint32_t numPoints, const std::vector<PowerUpEdit> &edits)
{
    PowerUpResult out;
    std::set<uint32_t> seen;
    auto near = [](float a, float b) { return std::fabs(double(a) - double(b)) <= 0.01; };
    for (const PowerUpEdit &e : edits)
    {
        const std::string at = "power-up " + std::to_string(e.index) + ": ";
        auto refuse = [&](const std::string &why) {
            ++out.refused;
            out.log.push_back(at + why + ", edit refused");
        };
        if (!seen.insert(e.index).second)
        {
            refuse("edited twice (the first edit stands)");
            continue;
        }
        if (numPoints > kMaxPoints)
        {
            refuse("the point count " + std::to_string(numPoints) + " is past the " + std::to_string(kMaxPoints) + " points PowerUpTool holds");
            continue;
        }
        if (e.index >= numPoints)
        {
            refuse("the stage has " + std::to_string(numPoints) + " power-up places");
            continue;
        }
        const uint32_t a = points + kPointSize * e.index;
        if (!m.ok(a, 12))
        {
            refuse("its point is not readable");
            continue;
        }
        const float p[3] = {m.f32(a), m.f32(a + 4), m.f32(a + 8)};
        const bool atFrom = near(p[0], e.from[0]) && near(p[1], e.from[1]) && near(p[2], e.from[2]);
        const bool atAt = near(p[0], e.at[0]) && near(p[1], e.at[1]) && near(p[2], e.at[2]);
        if (!atFrom && !atAt)
        {
            refuse("is at " + num(p[0]) + " " + num(p[1]) + " " + num(p[2]) + ", not at its \"from\" " + num(e.from[0]) + " " +
                   num(e.from[1]) + " " + num(e.from[2]));
            continue;
        }
        for (uint32_t k = 0; k < 3; ++k)
            m.wf32(a + 4 * k, e.at[k]);
        ++out.moved;
    }
    return out;
}

PowerUpResult appendPowerUps(const Mem &m, uint32_t points, uint32_t numPoints, const std::vector<PowerUpEdit> &adds, uint32_t &count)
{
    PowerUpResult out;
    count = numPoints;
    auto near = [](float a, float b) { return std::fabs(double(a) - double(b)) <= 0.01; };
    for (const PowerUpEdit &e : adds)
    {
        const std::string at = "added power-up " + std::to_string(e.index) + ": ";
        auto refuse = [&](const std::string &why) {
            ++out.refused;
            out.log.push_back(at + why + ", refused");
        };
        if (numPoints > kMaxPoints)
        {
            refuse("the point count " + std::to_string(numPoints) + " is past the " + std::to_string(kMaxPoints) + " points PowerUpTool holds");
            continue;
        }
        if (e.index >= numPoints)
        {
            refuse("the stage has " + std::to_string(numPoints) + " power-up places");
            continue;
        }
        if (count >= kMaxPoints)
        {
            refuse("PowerUpTool holds " + std::to_string(kMaxPoints) + " points");
            continue;
        }
        const uint32_t src = points + kPointSize * e.index, dst = points + kPointSize * count;
        const uint8_t *s = m.at(src, kPointSize);
        uint8_t *d = m.at(dst, kPointSize);
        if (!s)
        {
            refuse("its point is not readable");
            continue;
        }
        if (!d)
        {
            refuse("its new point (" + std::to_string(count) + ") is outside memory");
            continue;
        }
        const float p[3] = {m.f32(src), m.f32(src + 4), m.f32(src + 8)};
        if (!near(p[0], e.from[0]) || !near(p[1], e.from[1]) || !near(p[2], e.from[2]))
        {
            refuse("is at " + num(p[0]) + " " + num(p[1]) + " " + num(p[2]) + ", not at its \"from\" " + num(e.from[0]) + " " +
                   num(e.from[1]) + " " + num(e.from[2]));
            continue;
        }
        std::memmove(d, s, kPointSize);
        for (uint32_t k = 0; k < 3; ++k)
            m.wf32(dst + 4 * k, e.at[k]);
        ++count;
        ++out.moved;
    }
    if (out.moved > 0 && count > kMaxPowerUps)
        out.log.push_back("added " + std::to_string(out.moved) + " power-ups (" + std::to_string(count) + " places: the game creates " +
                          std::to_string(kMaxPowerUps) + " at most)");
    return out;
}

PointEditsResult applyPointEdits(const Mem &m, uint32_t points, uint32_t numPoints, uint32_t stock, const std::vector<PowerUpEdit> &adds,
                                 const std::vector<PowerUpEdit> &moves, uint32_t &count)
{
    PointEditsResult out;
    count = numPoints;
    if (numPoints == stock)
        out.added = appendPowerUps(m, points, stock, adds, count);
    out.moved = applyPowerUps(m, points, std::min(stock, numPoints), moves);
    return out;
}

} // namespace wotm::stagemap
