// Monstro 3D direto do disco, sem o jogo: le o .NGP/.TEX/.RTX de MON/, avalia uma animacao (curvas), monta as matrizes das
// partes e a paleta de ossos como a travessia do jogo, e devolve os triangulos posados com textura. Porte de tools/animcurve.py e
// tools/pose.py (conferidos contra dumps de RAM de luta); formato e resultados em docs/passos/MODELOS_3D.md.
//
// Uso: Model m; m.load(discRoot, iso, "MONKEY0"); m.pose(90, quadro); m.verts() / m.textures(). O desenho com raylib fica em
// Renderer (abaixo), que usa um RenderTexture como o wotm_saveicon.hpp.
// Nao reproduz: molas do pelo (BoneSpring), olhar/cabeca procedural e camadas de mistura de animacao.
#pragma once
#include "wotm_native.hpp"   // Ram, M4, mul, xform, decodeObject
#include "wotm_disc.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace wotm::model {

constexpr uint32_t kBase = 0xA00000;   // o .NGP do monstro vem pre-ligado para ca (lido nessa base, dispensa o .PTR)
constexpr int kIdle = 0x5A;            // StateIdle::chooseBaseIdle
constexpr int kUvLayer = 0x91;         // camada da textura rolando (so canais de UV; Kineticlops), tocada por cima da principal

// ---------------------------------------------------------------- curvas (src/common/AnimCurve.cpp)

struct Curve {
    const Ram *m = nullptr;
    uint32_t a = 0, keys = 0;
    int ctype = 0, dtype = 0, pre = 0, post = 0, n = 0, ksz = 4, tsz = 4, ssz = 4;
    uint16_t idx = 0;
    float bt = 0, dt = 1, bv = 0, dv = 1;

    Curve(const Ram &mem, uint32_t addr) : m(&mem), a(addr) {
        const uint16_t h = mem.u16(a);
        ctype = h & 7; dtype = (h >> 3) & 7; pre = (h >> 6) & 7; post = (h >> 9) & 7;
        idx = mem.u16(a + 2);
        if (ctype == 3) return;
        n = mem.s16(a + 4);
        keys = a + (dtype == 0 ? 8 : 6);
        bt = mem.f32(a - 16); dt = mem.f32(a - 12); bv = mem.f32(a - 8); dv = mem.f32(a - 4);
        tsz = dtype == 0 ? 4 : (dtype == 1 ? 2 : 1);
        ssz = dtype == 0 ? 4 : 2;
        switch (ctype) {   // tamanho da chave com o alinhamento do C++ do retail
        case 2: ksz = tsz; break;
        case 5: ksz = 2 * tsz; break;
        case 4: ksz = dtype == 0 ? 12 : (dtype == 1 ? 6 : 4); break;
        case 1: ksz = dtype == 0 ? 16 : (dtype == 1 ? 8 : 6); break;
        default: ksz = 20; break;
        }
    }
    float raw(uint32_t o) const { return dtype == 0 ? m->f32(o) : dtype == 1 ? float(m->u16(o)) : float(m->u8(o)); }
    float rt(int i) const { return raw(keys + uint32_t(i * ksz)); }
    float rv(int i) const { return raw(keys + uint32_t(i * ksz) + (ctype == 2 ? 0u : uint32_t(tsz))); }
    float T(int i) const {
        if (ctype == 2) return float(i) * dt + bt;
        const float r = rt(i);
        return dtype == 0 ? r : r * dt + bt;
    }
    float V(int i) const { const float r = rv(i); return dtype == 0 ? r : r * dv + bv; }
    static float slope(int16_t s) {
        const int hi = s >> 14;
        if (hi == -2) return 16384.f / (-32768.f - float(s));
        if (hi == 1) return 16384.f / (32768.f - float(s));
        return float(s) * (1.f / 16384.f);
    }
    float S(int i, int k = 0) const {
        const uint32_t o = keys + uint32_t(i * ksz + 2 * tsz + k * ssz);
        return dtype == 0 ? m->f32(o) : slope(m->s16(o));
    }
    int find(float t) const {
        int lo = 0;
        if (ctype == 2) lo = int((t - bt) / dt);
        else {
            const float kt = dtype == 0 ? t : float(int((t - bt) / dt));
            for (int i = 0; i < n - 1; ++i) {
                if (rt(i) <= kt) lo = i;
                else break;
            }
        }
        return std::max(0, std::min(lo, n - 2));
    }
    float between(float t, int i) const {
        switch (ctype) {
        case 2: { const float v0 = V(i), v1 = V(i + 1); return v0 + (v1 - v0) * ((t - bt) / dt - float(i)); }
        case 5: return V(i);
        case 0: {
            const uint32_t o = keys + uint32_t(i * ksz);
            const float x = t - m->f32(o), c0 = m->f32(o + 8), c1 = m->f32(o + 12), c2 = m->f32(o + 16);
            return x * (x * (x * c0 + c1) + c2) + m->f32(o + 4);
        }
        case 1: {
            const float t0 = T(i), v0 = V(i), dvv = V(i + 1) - v0, m0 = S(i, 0), m1 = S(i, 1);
            const float s = (t - t0) / (T(i + 1) - t0);
            return s * (s * (s * (m0 + m1 - 2 * dvv) + (3 * dvv - (2 * m0 + m1))) + m0) + v0;
        }
        case 4: {
            const float t0 = T(i), v0 = V(i), dtt = T(i + 1) - t0, dvv = V(i + 1) - v0, m0 = S(i), m1 = S(i + 1);
            const float inv = 1.f / dtt, aa = m0 * dtt, bb = m1 * dtt, inv2 = inv * inv, x = t - t0;
            return x * (x * (x * ((aa + bb - 2 * dvv) * (inv2 * inv)) + (3 * dvv - (2 * aa + bb)) * inv2) + m0) + v0;
        }
        default: return 0.f;
        }
    }
    float eval(float t) const {
        if (ctype == 3) return m->f32(a + 4);
        if (n < 2) return n == 1 ? V(0) : 0.f;
        float off = 0.f;
        const int last = n - 1;
        const float first = T(0), end = T(last);
        if (t < first) {
            const float dist = first - t, rng = end - first;
            if (pre == 0) return V(0);
            if (pre == 1) return 0.f;
            if (rng <= 0.f) return V(0);
            if (pre == 2 || pre == 3) {
                const int k = int(dist / rng) + 1;
                t += float(k) * rng;
                if (pre == 3) off = (V(0) - V(last)) * float(k);
            } else if (pre == 4) {
                const int k = int(dist / rng);
                t = (k & 1) ? t + float(k + 1) * rng : dist - float(k) * rng;
            }
        } else if (end <= t) {
            const float rng = end - first, dist = t - end;
            if (post == 0) return V(last);
            if (post == 1) return 0.f;
            if (rng <= 0.f) return V(last);
            if (post == 2 || post == 3) {
                const int k = int(dist / rng) + 1;
                t -= float(k) * rng;
                if (post == 3) off = (V(last) - V(0)) * float(k);
            } else if (post == 4) {
                const int k = int(dist / rng);
                t = (k & 1) ? t - float(k + 1) * rng : (end - t) + float(k) * rng;
            }
        }
        return between(t, find(t)) + off;
    }
};

// ---------------------------------------------------------------- texturas (.TEX / .RTX)

struct TexImage {
    int w = 0, h = 0;
    std::vector<uint32_t> rgba;   // R | G << 8 | B << 16 | A << 24, alfa 0..255
};

struct TexPacket {
    uint32_t off = 0;
    uint16_t texId = 0, tbp = 0;
    int psm = 0, logW = 0, logH = 0;
    bool rtx = false;
    int w() const { return 1 << logW; }
    int h() const { return 1 << logH; }
    bool high() const { return psm == 0x1B || psm == 0x24 || psm == 0x2C; }
    bool isPalette() const { return rtx && psm == 0 && ((logW == 4 && logH == 4) || (logW == 3 && logH == 1)); }
    uint32_t data() const { return off + ((rtx && !high()) ? 0x10u : 0x80u); }   // texmLoadResTexture
};

inline std::vector<TexPacket> parseTex(const std::vector<uint8_t> &d, bool rtx) {
    std::vector<TexPacket> out;
    auto u32 = [&](size_t o) { uint32_t v = 0; if (o + 4 <= d.size()) std::memcpy(&v, &d[o], 4); return v; };
    auto u16 = [&](size_t o) { uint16_t v = 0; if (o + 2 <= d.size()) std::memcpy(&v, &d[o], 2); return v; };
    if (d.size() < 8 || std::memcmp(d.data(), "IE\x03\x04", 4) == 0) return out;   // zip IE: os de MON/ vem crus
    size_t off = size_t(u32(4)) * 16;
    while (off + 16 <= d.size() && out.size() < 4096) {
        const uint32_t size = u32(off), bits = u32(off + 12);
        TexPacket p;
        p.off = uint32_t(off); p.texId = u16(off + 8); p.tbp = u16(off + 10);
        p.psm = bits & 0x3F; p.logW = (bits >> 8) & 15; p.logH = (bits >> 12) & 15; p.rtx = rtx;
        const size_t end = size == 0 ? d.size() : off + size_t(size >> 2) * 16;
        if (end <= off || end > d.size()) break;
        out.push_back(p);
        if (size == 0) break;
        off = end;
    }
    return out;
}

// ---------------------------------------------------------------- modelo

struct PVtx {
    float x, y, z;       // mundo do monstro (Z para cima)
    float nx, ny, nz;    // normal (unitaria; 0 se o pacote nao tem)
    float u, v;
    uint8_t r, g, b, a;  // cor de vertice crua (0x80 = 1,0)
    int16_t tex;         // indice em textures(); -1 = sem textura
    // Estado do GS da secao/objeto (Scene::groupState): ALPHA_1 (byte baixo; vale so com PRIM.ABE), limiar do teste de alfa
    // (-1 = sem teste) e ZBUF_1.ZMSK
    uint8_t blend = 0x44, abe = 0, zmsk = 0;
    int16_t aref = -1;
};

// Estado do GS herdado na travessia (o ctx de GROUP/TRANSLATE/ROTATE/SCALE em +4 e de ANIM_XFORM/ANIM_SCALE em +8; os filhos
// herdam). Inicial = ctxDef do jogo (ALPHA_1 0x44, TEST_1 0x5040C: sem teste).
struct GsState {
    uint8_t blend = 0x44;
    int16_t aref = -1;
    bool zmsk = false;
    // Pacote A+D do ctx: tag em +0x10 (NLOOP), pares (dado, endereco) a partir de +0x20 (como Scene::groupState)
    void apply(const Ram &ram, uint32_t ctx) {
        ctx &= 0x0FFFFFFFu;
        if (!ctx || !ram.ok(ctx, 0x20) || ram.u32(ctx + 0x14) != 0x10000000 || ram.u32(ctx + 0x18) != 0xE) return;
        const uint32_t n = std::min<uint32_t>(ram.u32(ctx + 0x10) & 0x7FFF, 8);
        if (!ram.ok(ctx, 0x20 + 16 * n)) return;
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t d = ram.u32(ctx + 0x20 + 16 * i), reg = ram.u32(ctx + 0x28 + 16 * i) & 0xFF;
            if (reg == 0x42) blend = uint8_t(d & 0xFF);
            else if (reg == 0x47) {
                const uint32_t atst = (d >> 1) & 7, ref = (d >> 4) & 0xFF;
                aref = (!(d & 1) || atst == 1) ? int16_t(-1) : atst == 6 ? int16_t(ref) : atst == 5 ? int16_t(std::max(int(ref) - 1, 0)) : int16_t(8);
            } else if (reg == 0x4E) zmsk = (ram.u32(ctx + 0x24 + 16 * i) & 1) != 0;
        }
    }
};

class Model {
public:
    bool load(const std::filesystem::path &discRoot, const std::string &iso, const std::string &name) {
        std::vector<uint8_t> ngp, tex, rtx;
        if (!wotm::disc::readFile(discRoot, iso, "MON/" + name + ".NGP", ngp) || ngp.size() < 16) return false;
        wotm::disc::readFile(discRoot, iso, "MON/" + name + ".TEX", tex);
        wotm::disc::readFile(discRoot, iso, "MON/" + name + ".RTX", rtx);
        buf_.assign(kBase + ngp.size() + 16, 0);
        std::memcpy(buf_.data() + kBase, ngp.data(), ngp.size());
        ram_.p = buf_.data();
        ram_.size = buf_.size();
        texData_ = std::move(tex);
        rtxData_ = std::move(rtx);
        for (const TexPacket &p : parseTex(texData_, false)) byId_.emplace(p.texId, p);
        for (const TexPacket &p : parseTex(rtxData_, true)) {
            if (p.isPalette()) pals_.emplace(p.tbp, p);
            else byId_.emplace(p.texId, p);
        }
        if (ram_.u32(kBase) == 0) return false;
        root_ = ram_.u32(kBase + 4);
        cis_.clear();
        findCis(root_, 0);
        main_ = 0;
        uint16_t best = 0;
        for (uint32_t ci : cis_) {
            const uint16_t na = ram_.u16(ram_.u32(ci + 0x20));
            if (na > best) { best = na; main_ = ci; }
        }
        name_ = name;
        static uint32_t s_gen = 0;
        gen_ = ++s_gen;
        haveFrame_ = false;
        return main_ != 0;
    }

    // Numero unico desta carga (o Renderer refaz as texturas quando muda; o endereco do Model pode se repetir entre cargas)
    uint32_t gen() const { return gen_; }

    // Caixa fixa para enquadrar a animacao `id` inteira (amostra `samples` quadros; deixa a pose no quadro 0). Sem ela o Renderer
    // enquadra pela caixa do quadro corrente e a camera acompanha a animacao.
    void computeFraming(int id, int samples = 24) {
        const float len = animLength(id);
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (int s = 0; s < samples; ++s) {
            if (!pose(id, len * float(s) / float(samples))) { pose(-1, 0.f); s = samples; }
            float a[3], b[3];
            bounds(a, b);
            for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], a[k]); hi[k] = std::max(hi[k], b[k]); }
        }
        for (int k = 0; k < 3; ++k) { frameLo_[k] = lo[k]; frameHi_[k] = hi[k]; }
        haveFrame_ = lo[0] <= hi[0];
        pose(id, 0.f);
    }
    // Caixa de enquadramento: a fixa (computeFraming) ou a do quadro corrente
    void framing(float lo[3], float hi[3]) const {
        if (!haveFrame_) { bounds(lo, hi); return; }
        for (int k = 0; k < 3; ++k) { lo[k] = frameLo_[k]; hi[k] = frameHi_[k]; }
    }

    bool ok() const { return main_ != 0; }
    const std::string &name() const { return name_; }

    // Duracao da animacao `id` em quadros do jogo (60 por segundo); 0 se nao existe
    float animLength(int id) const {
        uint32_t a = anim(id);
        return a ? std::max(0.f, ram_.f32(a + 8) - ram_.f32(a + 4)) : 0.f;
    }

    // Monta a pose da animacao `id` no quadro `frame` (a partir do inicio, em laco) e refaz verts(). id < 0: pose de base.
    bool pose(int id, float frame) {
        ctxs_.clear();
        ctxs_.push_back(makeCtx(main_));
        if (id >= 0) {
            const uint32_t an = anim(id);
            if (!an) return false;
            const float st = ram_.f32(an + 4), et = ram_.f32(an + 8);
            float t = st + frame;
            if (et > st && t > et) t = st + std::fmod(t - st, et - st);
            Ctx &c = ctxs_[0];
            const uint16_t nch = ram_.u16(an + 0xC);
            for (uint32_t k = 0; k < nch; ++k) {
                const Curve cv(ram_, ram_.u32(an + 0x18 + 4 * k));
                if (cv.idx < c.val.size()) { c.val[cv.idx] = cv.eval(t); c.dirty[cv.idx] = 1; }
            }
            // Camada da textura rolando (Kineticlops: a 0x91 so tem as curvas dos canais de UV dos OBJECTs com uvScroll), tocada em
            // laco por cima da principal, no tempo dela; na luta (build/dumps_live/ram_energy_robot.bin) os canais 142/143 valem
            // 0,637/1,275, a proporcao 1:2 dessa camada (a 0 de 240 quadros, a outra com so as duas curvas, e 1:3)
            if (const uint32_t ul = anim(kUvLayer)) {
                const float us = ram_.f32(ul + 4), ue = ram_.f32(ul + 8);
                const float tu = ue > us ? us + std::fmod(frame, ue - us) : us;
                const uint16_t nu = ram_.u16(ul + 0xC);
                for (uint32_t k = 0; k < nu; ++k) {
                    const Curve cv(ram_, ram_.u32(ul + 0x18 + 4 * k));
                    if (cv.idx < c.val.size()) { c.val[cv.idx] = cv.eval(tu); c.dirty[cv.idx] = 1; }
                }
            }
        }
        items_.clear();
        gs_ = GsState{};
        visit(root_, ident(), -1, -1, -1, 0);
        build();
        return true;
    }

    const std::vector<PVtx> &verts() const { return verts_; }
    const std::vector<TexImage> &textures() const { return texs_; }
    // Caixa dos vertices posados
    void bounds(float lo[3], float hi[3]) const {
        for (int k = 0; k < 3; ++k) { lo[k] = 1e30f; hi[k] = -1e30f; }
        for (const PVtx &v : verts_) {
            const float p[3] = {v.x, v.y, v.z};
            for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], p[k]); hi[k] = std::max(hi[k], p[k]); }
        }
    }

private:
    struct Ctx {
        uint32_t ci = 0, animPkt = 0;
        std::vector<float> val;
        std::vector<uint8_t> dirty;
        std::unordered_map<uint32_t, M4> pal;   // endereco da matriz na paleta -> matriz
        std::vector<uint32_t> outAddr;          // animMatrixPtr[k]
        float v(int i, float fb) const { return i >= 0 && size_t(i) < val.size() ? val[size_t(i)] : fb; }
        bool isDirty(int i) const { return i >= 0 && size_t(i) < dirty.size() && dirty[size_t(i)]; }
    };
    struct Item { uint32_t node; M4 m; int ctx; GsState gs; float uv[2]; };
    // ctx do no: o estado do GS dos filhos; volta ao do pai na saida
    struct GsScope {
        Model &md;
        GsState prev;
        GsScope(Model &m, uint32_t ctx) : md(m), prev(m.gs_) { md.gs_.apply(md.ram_, ctx); }
        ~GsScope() { md.gs_ = prev; }
    };

    uint32_t anim(int id) const {
        const uint32_t ch = ram_.u32(main_ + 0x20);
        const uint16_t na = ram_.u16(ch);
        for (uint32_t k = 0; k < na; ++k) {
            const uint32_t a = ram_.u32(ch + 0xC + 4 * k);
            if (int(ram_.u32(a) >> 18) == id) return a;
        }
        return 0;
    }

    Ctx makeCtx(uint32_t ci) const {
        Ctx c;
        c.ci = ci;
        const uint32_t ch = ram_.u32(ci + 0x20);
        const uint16_t nch = ram_.u16(ch + 2);
        const uint32_t vp = ram_.u32(ci + 4);
        c.val.resize(nch);
        c.dirty.assign(nch, 0);
        for (uint32_t i = 0; i < nch; ++i) c.val[i] = ram_.f32(vp + 4 * i);
        const uint32_t amp = ram_.u32(ci + 0x14);
        for (uint32_t k = 0; k < 64 && ram_.ok(amp + 4 * k); ++k) {
            const uint32_t a = ram_.u32(amp + 4 * k) & 0x0FFFFFFFu;
            if (!ram_.ok(a, 64)) break;
            c.outAddr.push_back(a);
        }
        c.animPkt = ram_.u32(ci + 0x18);
        return c;
    }

    void findCis(uint32_t n, int depth) {
        if (depth > 80 || !ram_.ok(n)) return;
        const uint32_t op = ram_.u32(n) & 63;
        if (op == 25) cis_.push_back(n);
        uint32_t arr = 0, cnt = 0;
        switch (op) {
        case 1: arr = n + 0x20; cnt = ram_.u16(n + 8); break;
        case 2: { const uint32_t nl = std::min<uint32_t>(ram_.u32(n + 4), 16); for (uint32_t i = 0; i < nl; ++i) findCis(ram_.u32(n + 0x20 + 16 * i + 8), depth + 1); return; }
        case 3: arr = n + 0x1C; cnt = ram_.u32(n + 8); break;
        case 4: arr = n + 0x50; cnt = ram_.u32(n + 8); break;
        case 17: arr = n + 0xCC; cnt = ram_.u32(n + 4); break;
        case 25: arr = n + 0x2C; cnt = ram_.u32(n + 0x28); break;
        case 35: arr = n + 0xC; cnt = ram_.u32(n + 4); break;
        case 38: arr = n + 0x60; cnt = ram_.u32(n + 4); break;
        case 30: arr = n + 0x2C; cnt = ram_.u32(n + 8); break;
        default: return;
        }
        for (uint32_t k = 0; k < std::min<uint32_t>(cnt, 4096); ++k) findCis(ram_.u32(arr + 4 * k), depth + 1);
    }

    static M4 prh(float x, float y, float z) {   // mathfRotMatrixPRH
        const float sz = std::sin(z), cz = std::cos(z), sx = std::sin(x), cx = std::cos(x), sy = std::sin(y), cy = std::cos(y);
        M4 m = ident();
        m.m[0][0] = cy * cz; m.m[0][1] = -cy * sz; m.m[0][2] = sy;
        m.m[1][0] = sx * sy * cz + cx * sz; m.m[1][1] = -sx * sy * sz + cx * cz; m.m[1][2] = -sx * cy;
        m.m[2][0] = -cx * sy * cz + sx * sz; m.m[2][1] = cx * sy * sz + sx * cz; m.m[2][2] = cx * cy;
        return m;
    }

    // Pilha do esqueleto (hierSkelBoneNode / hierAnimTransNode...): o no de transformacao depois de um SKEL_BONE desempilha
    void skelStep(int ctx, const M4 &local, int &si, int &mi) {
        if (si < 0 || mi < -1 || mi + 1 >= kStack) return;
        smat_[mi + 1] = mul(local, mi >= 0 ? smat_[mi] : ident());
        const uint32_t bone = skel_[si];
        const int16_t o = ram_.s16(bone + 8);
        if (ctx >= 0 && o >= 0 && size_t(o) < ctxs_[size_t(ctx)].outAddr.size())
            ctxs_[size_t(ctx)].pal[ctxs_[size_t(ctx)].outAddr[size_t(o)]] = mul(readM4(ram_, bone + 0x10), smat_[mi + 1]);
        --si;
        ++mi;
    }

    void visit(uint32_t n, const M4 &m, int si, int mi, int ctx, int depth) {
        if (depth > 80 || !ram_.ok(n)) return;
        const uint32_t op = ram_.u32(n) & 63;
        auto kids = [&](uint32_t arr, uint32_t cnt, const M4 &mm, int s2, int m2, int c2) {
            for (uint32_t q = 0; q < std::min<uint32_t>(cnt, 4096); ++q) visit(ram_.u32(arr + 4 * q), mm, s2, m2, c2, depth + 1);
        };
        const GsScope gsc(*this, (op == 1 || op == 3 || op == 4 || op == 30) ? ram_.u32(n + 4) : (op == 17 || op == 38) ? ram_.u32(n + 8) : 0u);
        switch (op) {
        case 0: {
            // Deslocamento do UV (Scene::objectUvOffset): +0x20/+0x24; com uvScroll (bit 0x800 de +0xC) os canais +0x30/+0x32
            Item it{n, m, ctx, gs_, {ram_.f32(n + 0x20), ram_.f32(n + 0x24)}};
            if ((ram_.u16(n + 0xC) & 0x800) && ctx >= 0)
                for (int k = 0; k < 2; ++k) it.uv[k] = ctxs_[size_t(ctx)].v(ram_.s16(n + 0x30 + 2 * k), 0.f);
            items_.push_back(it);
            break;
        }
        case 1: if (ram_.u16(n + 0xA) & 1) kids(n + 0x20, ram_.u16(n + 8), m, si, mi, ctx); break;
        case 2: {   // LOD: o mais detalhado
            const uint32_t nl = ram_.u32(n + 4);
            if (nl > 0 && nl < 16) visit(ram_.u32(n + 0x20 + 16 * (nl - 1) + 8), m, si, mi, ctx, depth + 1);
            break;
        }
        case 3: {
            const M4 L = translateM(ram_.f32(n + 0x10), ram_.f32(n + 0x14), ram_.f32(n + 0x18));
            int s2 = si, m2 = mi;
            skelStep(ctx, L, s2, m2);
            kids(n + 0x1C, ram_.u32(n + 8), mul(L, m), s2, m2, ctx);
            break;
        }
        case 4: {
            const M4 L = readM4(ram_, n + 0x10);
            int s2 = si, m2 = mi;
            skelStep(ctx, L, s2, m2);
            kids(n + 0x50, ram_.u32(n + 8), mul(L, m), s2, m2, ctx);
            break;
        }
        case 30: {
            const M4 L = mul(scaleM(ram_.f32(n + 0x20), ram_.f32(n + 0x24), ram_.f32(n + 0x28)),
                             translateM(ram_.f32(n + 0x10), ram_.f32(n + 0x14), ram_.f32(n + 0x18)));
            kids(n + 0x2C, ram_.u32(n + 8), mul(L, m), si, mi, ctx);
            break;
        }
        case 6: {
            const uint8_t w = ram_.u8(n + 8), cnt = ram_.u8(n + 0xB);
            if (w < cnt) visit(ram_.u32(n + 0xC + 4u * w), m, si, mi, ctx, depth + 1);
            break;
        }
        case 8: case 23: visit(ram_.u32(n + 4), m, si, mi, ctx, depth + 1); break;
        case 35: {
            if (si + 1 >= kStack) break;
            const int s2 = si + 1;
            skel_[s2] = n;
            int m2 = mi;
            if (ram_.u16(n + 0xA) && mi + 1 < kStack) { m2 = mi + 1; smat_[m2] = ident(); }
            kids(n + 0xC, ram_.u32(n + 4), m, s2, m2, ctx);
            break;
        }
        case 25: {
            int c = -1;
            for (size_t i = 0; i < ctxs_.size(); ++i) if (ctxs_[i].ci == n) c = int(i);
            if (c < 0) { ctxs_.push_back(makeCtx(n)); c = int(ctxs_.size() - 1); }
            kids(n + 0x2C, ram_.u32(n + 0x28), m, si, mi, c);
            break;
        }
        case 17: {   // ANIM_XFORM
            float vis = ram_.f32(n + 0xC);
            const int vidx = ram_.s16(n + 0x10);
            if (ctx >= 0 && ctxs_[size_t(ctx)].isDirty(vidx)) vis = ctxs_[size_t(ctx)].val[size_t(vidx)];
            if (vis == 0.f) break;
            // hierAnimTransNode: a matriz so e remontada quando um canal do no (firstIdx em +0x1E, mascara dirty.animating em +0xC1)
            // foi escrito; senao vale a de atMat[matrixIdx] (+0xC4), que vem pronta no arquivo (as garras do Robo-47, so com canal de
            // visibilidade, tem ali uma matriz que a PRH do vetor do no nao da).
            bool rebuild = false;
            const Ctx *cp = ctx >= 0 ? &ctxs_[size_t(ctx)] : nullptr;
            const int first = ram_.s16(n + 0x1E);
            const uint8_t animMask = ram_.u8(n + 0xC1);
            if (cp && first >= 0)
                for (int b = 0; b < 8; ++b)
                    if (((animMask >> b) & 1) && cp->isDirty(first + b)) { rebuild = true; break; }
            M4 L;
            if (rebuild) {
                float r[3] = {ram_.f32(n + 0x20), ram_.f32(n + 0x24), ram_.f32(n + 0x28)};
                float tr[3] = {ram_.f32(n + 0x30), ram_.f32(n + 0x34), ram_.f32(n + 0x38)};
                for (int j = 0; j < 3; ++j) {
                    const int ir = ram_.s16(n + 0x12 + 2 * j), it = ram_.s16(n + 0x18 + 2 * j);
                    if (ir >= 0) r[j] = -cp->v(ir, -r[j]);
                    if (it >= 0) tr[j] = cp->v(it, tr[j]);
                }
                L = prh(r[0], r[1], r[2]);
                const uint16_t bits = ram_.u16(n + 0xC2);
                if (bits & 1) L = mul(readM4(ram_, n + 0x40), L);
                if (bits & 2) L = mul(L, readM4(ram_, n + 0x80));
                for (int j = 0; j < 3; ++j) { L.m[3][j] += tr[j]; L.m[j][3] = 0.f; }
                L.m[3][3] = 1.f;
            } else {
                const uint32_t at = cp ? ram_.u32(cp->ci + 0xC) : 0;
                L = ram_.ok(at + 64 * ram_.u32(n + 0xC4), 64) ? readM4(ram_, at + 64 * ram_.u32(n + 0xC4)) : ident();
            }
            int s2 = si, m2 = mi;
            skelStep(ctx, L, s2, m2);
            kids(n + 0xCC, ram_.u32(n + 4), mul(L, m), s2, m2, ctx);
            break;
        }
        case 38: {   // ANIM_SCALE: le val direto
            const Ctx *c = ctx >= 0 ? &ctxs_[size_t(ctx)] : nullptr;
            auto g = [&](uint32_t off, float fb) { const int i = ram_.s16(n + off); return (c && i >= 0) ? c->v(i, fb) : fb; };
            const int vidx = ram_.s16(n + 0x10);
            const float vis = (c && vidx >= 0) ? c->v(vidx, 1.f) : 1.f;
            if (vis == 0.f) break;
            const float sx = g(0x12, ram_.f32(n + 0x20)), sy = g(0x14, ram_.f32(n + 0x24)), sz = g(0x16, ram_.f32(n + 0x28));
            if (sx == 0.f && sy == 0.f && sz == 0.f) break;
            const float tx = g(0x18, ram_.f32(n + 0x30)), ty = g(0x1A, ram_.f32(n + 0x34)), tz = g(0x1C, ram_.f32(n + 0x38));
            const M4 pre = translateM(ram_.f32(n + 0x40), ram_.f32(n + 0x44), ram_.f32(n + 0x48));
            const M4 post = translateM(tx + ram_.f32(n + 0x50), ty + ram_.f32(n + 0x54), tz + ram_.f32(n + 0x58));
            kids(n + 0x60, ram_.u32(n + 4), mul(mul(mul(pre, scaleM(sx, sy, sz)), post), m), si, mi, ctx);
            break;
        }
        case 39: {
            const uint32_t c2 = ram_.u32(n + 0xC);
            if (ram_.ok(c2) && (ram_.u32(c2) & 63) == 31) {
                if (ram_.u16(c2 + 0xA)) visit(ram_.u32(c2 + 0xC), m, si, mi, ctx, depth + 1);
            } else visit(c2, m, si, mi, ctx, depth + 1);
            break;
        }
        case 31: if (ram_.u16(n + 0xA)) visit(ram_.u32(n + 0xC), m, si, mi, ctx, depth + 1); break;
        default: break;
        }
    }

    // Paleta do OBJECT skinned (Scene::palette): animPktPtr[packetPtrIdx] -> tag ref -> bloco de 32 matrizes
    bool palette(uint32_t node, int ctx, std::array<M4, 32> &pm) const {
        for (M4 &x : pm) x = ident();
        if (ctx < 0) return false;
        const Ctx &c = ctxs_[size_t(ctx)];
        if (!ram_.ok(c.animPkt)) return false;
        const int32_t idx = int32_t(ram_.u32(node + 0x68));
        if (idx < 0 || idx > 63) return false;
        const uint32_t chain = ram_.u32(c.animPkt + 4 * uint32_t(idx)) & 0x0FFFFFFFu;
        if (!ram_.ok(chain, 8) || ((ram_.u32(chain) >> 28) & 7) != 3) return false;
        const uint32_t block = ram_.u32(chain + 4) & 0x0FFFFFFFu;
        for (uint32_t s = 0; s < 32; ++s) {
            auto it = c.pal.find(block + 0x10 + 0x40 * s);
            if (it != c.pal.end()) pm[s] = it->second;
        }
        return true;
    }

    const MeshData &mesh(uint32_t node) {
        auto it = meshes_.find(node);
        if (it == meshes_.end()) it = meshes_.emplace(node, decodeObject(ram_, node)).first;
        return it->second;
    }

    static void rot3(const M4 &m, float x, float y, float z, float out[3]) {
        out[0] = x * m.m[0][0] + y * m.m[1][0] + z * m.m[2][0];
        out[1] = x * m.m[0][1] + y * m.m[1][1] + z * m.m[2][1];
        out[2] = x * m.m[0][2] + y * m.m[1][2] + z * m.m[2][2];
    }

    void build() {
        verts_.clear();
        std::array<M4, 32> pm;
        for (const Item &it : items_) {
            const MeshData &md = mesh(it.node);
            if (!md.valid || md.tris.empty()) continue;
            const bool skin = md.skinned && palette(it.node, it.ctx, pm);
            for (const Vtx &v : md.tris) {
                PVtx o{};
                float p[3], nrm[3];
                const float n0[3] = {v.nx / 127.f, v.ny / 127.f, v.nz / 127.f};
                if (skin) {
                    const float w1 = v.w, s = v.w * 2048.f, w2 = s - std::trunc(s), w3 = 1.f - w1 - w2;
                    const uint8_t codes[3] = {v.cx, v.cy, v.cz};
                    const float ws[3] = {w1, w2, w3};
                    float q[3] = {0, 0, 0};
                    for (int k = 0; k < 3; ++k) {
                        if (ws[k] == 0.f) continue;
                        float a[3];
                        xform(pm[(codes[k] & 0x7F) / 4], v.x, v.y, v.z, a);
                        for (int c = 0; c < 3; ++c) q[c] += a[c] * ws[k];
                    }
                    xform(it.m, q[0], q[1], q[2], p);
                    float nb[3];
                    rot3(pm[(v.cx & 0x7F) / 4], n0[0], n0[1], n0[2], nb);   // a normal so gira pelo osso x (Scene::vertexRgb)
                    rot3(it.m, nb[0], nb[1], nb[2], nrm);
                } else {
                    xform(it.m, v.x, v.y, v.z, p);
                    rot3(it.m, n0[0], n0[1], n0[2], nrm);
                }
                const float len = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
                if (len > 1e-6f) for (float &c : nrm) c /= len;
                o.x = p[0]; o.y = p[1]; o.z = p[2];
                o.nx = nrm[0]; o.ny = nrm[1]; o.nz = nrm[2];
                o.u = v.u + it.uv[0]; o.v = v.v + it.uv[1];
                o.blend = it.gs.blend; o.abe = md.abe ? 1 : 0; o.zmsk = it.gs.zmsk ? 1 : 0; o.aref = it.gs.aref;
                o.r = v.r; o.g = v.g; o.b = v.b; o.a = v.a;
                o.tex = v.slot == kSlotNoTex ? int16_t(-1) : texIndex(it.node, v.slot);
                verts_.push_back(o);
            }
        }
    }

    // Textura do slot `s` do OBJECT (texId em +0x50 + 2s, TEX0 em +0x70 + 0x30s): os numeros do .NGP do monstro sao os do proprio
    // .TEX/.RTX (texId do pacote, CBP = tbp da paleta no .RTX), sem deslocamento (conferido nos 10 monstros e nas fantasias).
    int16_t texIndex(uint32_t node, uint8_t s) {
        if (s >= 5 || s >= ram_.u16(node + 0x5A)) return -1;
        const uint16_t id = ram_.u16(node + 0x50 + 2u * s);
        const uint64_t t0 = uint64_t(ram_.u32(node + 0x70 + 0x30u * s)) | (uint64_t(ram_.u32(node + 0x74 + 0x30u * s)) << 32);
        const uint64_t key = (uint64_t(id) << 48) ^ t0;
        auto f = texKey_.find(key);
        if (f != texKey_.end()) return f->second;
        int16_t res = -1;
        TexImage img;
        if (decodeTex(id, t0, img)) { texs_.push_back(std::move(img)); res = int16_t(texs_.size() - 1); }
        texKey_.emplace(key, res);
        return res;
    }

    bool decodeTex(uint16_t id, uint64_t t0, TexImage &out) const {
        auto it = byId_.find(id);
        if (it == byId_.end()) return false;
        const TexPacket &p = it->second;
        const std::vector<uint8_t> &d = p.rtx ? rtxData_ : texData_;
        const int w = p.w(), h = p.h();
        const uint32_t cbp = uint32_t(t0 >> 37) & 0x3FFF, csa = uint32_t(t0 >> 56) & 31, tcc = uint32_t(t0 >> 34) & 1;
        auto fixA = [](uint32_t a) { return a >= 128 ? 255u : a * 2; };
        auto rd32 = [&](size_t o) { uint32_t v = 0; if (o + 4 <= d.size()) std::memcpy(&v, &d[o], 4); return v; };
        out.w = w; out.h = h;
        out.rgba.assign(size_t(w) * size_t(h), 0);
        const uint32_t base = p.data();
        const bool t8 = p.psm == 0x13 || p.psm == 0x1B, t4 = p.psm == 0x14 || p.psm == 0x24 || p.psm == 0x2C;
        if (t8 || t4) {
            auto pe = pals_.find(uint16_t(cbp));
            if (pe == pals_.end()) return false;
            const TexPacket &e = pe->second;
            const size_t np = size_t(e.w()) * size_t(e.h());
            std::vector<uint32_t> pal(np);
            for (size_t i = 0; i < np; ++i) {
                size_t j = i;
                if (np >= 256 && t8) j = (i & 0xE7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1);   // CSM1
                size_t o = e.data() + 4 * j;
                uint32_t c = 0;
                if (o + 4 <= rtxData_.size()) std::memcpy(&c, &rtxData_[o], 4);
                pal[i] = (c & 0xFFFFFFu) | (fixA(c >> 24) << 24);
            }
            for (size_t i = 0; i < out.rgba.size(); ++i) {
                uint32_t k;
                if (t8) k = base + i < d.size() ? d[base + i] : 0;
                else {
                    const uint8_t b = base + i / 2 < d.size() ? d[base + i / 2] : 0;
                    k = (i & 1) ? (b >> 4) : (b & 15);
                    const uint32_t ix = (csa * 16 + k) & 255;
                    if (np >= 256 && ix < np) k = ix;
                }
                out.rgba[i] = k < np ? pal[k] : 0;
            }
        } else if (p.psm == 0 || p.psm == 1) {
            for (size_t i = 0; i < out.rgba.size(); ++i) {
                const uint32_t c = rd32(base + 4 * i);
                out.rgba[i] = (c & 0xFFFFFFu) | ((p.psm == 1 ? 255u : fixA(c >> 24)) << 24);
            }
        } else if (p.psm == 2 || p.psm == 0xA) {
            for (size_t i = 0; i < out.rgba.size(); ++i) {
                uint16_t c = 0;
                if (base + 2 * i + 2 <= d.size()) std::memcpy(&c, &d[base + 2 * i], 2);
                const uint32_t r = (c & 31) * 255 / 31, g = ((c >> 5) & 31) * 255 / 31, b = ((c >> 10) & 31) * 255 / 31;
                out.rgba[i] = r | g << 8 | b << 16 | ((c & 0x8000) ? 0xFF000000u : 0u);
            }
        } else return false;
        if (!tcc) for (uint32_t &c : out.rgba) c |= 0xFF000000u;
        return true;
    }

    static constexpr int kStack = 64;
    std::vector<uint8_t> buf_, texData_, rtxData_;
    Ram ram_;
    std::string name_;
    uint32_t root_ = 0, main_ = 0;
    std::vector<uint32_t> cis_;
    std::vector<Ctx> ctxs_;
    std::vector<Item> items_;
    uint32_t skel_[kStack] = {};
    M4 smat_[kStack];
    std::unordered_map<uint32_t, MeshData> meshes_;
    std::map<uint16_t, TexPacket> byId_, pals_;
    std::unordered_map<uint64_t, int16_t> texKey_;
    std::vector<TexImage> texs_;
    std::vector<PVtx> verts_;
    GsState gs_;
    uint32_t gen_ = 0;
    bool haveFrame_ = false;
    float frameLo_[3] = {}, frameHi_[3] = {};
};

// ---------------------------------------------------------------- desenho (raylib, thread da janela)

// Desenha o modelo posado num RenderTexture. yaw em graus (0 = de frente: camera em +Y; o monstro olha para +Y), pitch para cima.
// Luz: uma direcional fixa + ambiente, por vertice, sobre a cor de vertice do pacote (o jogo usa as luzes da fase).
//
// O RenderTexture sai com cor PRE-MULTIPLICADA pelo alfa (cole com BLEND_ALPHA_PREMULTIPLY e tom (a, a, a, a)): assim as misturas
// do GS que somam ao fundo (aditiva) continuam certas fora do que foi desenhado por baixo. `background` (opcional) desenha o fundo
// em 2D dentro do RenderTexture antes do monstro (a foto da fase na lista de saves), para as misturas que dependem do destino
// (escurecer, subtrair, (Cd - Cs)·As) terem sobre o que agir. Mistura do GS (ALPHA_1 = A,B,C,D em pares de bits, Cv = (A-B)·C + D):
//   0x44 (Cs-Cd)As+Cd comum | 0x48 Cs·As+Cd aditiva | 0x42 Cd-Cs·As | 0x46 Cd·(1-As) | 0x81 (Cd-Cs)·As. Sem PRIM.ABE: opaco.
class Renderer {
public:
    ~Renderer() { release(); }
    void release() {
        for (Texture2D &t : gpu_) if (t.id) UnloadTexture(t);
        gpu_.clear();
        if (rt_.id) UnloadRenderTexture(rt_);
        rt_ = {};
        if (shader_.id) UnloadShader(shader_);
        shader_ = {};
        gen_ = 0;
    }

    const RenderTexture2D &target() const { return rt_; }

    template <class Bg>
    const RenderTexture2D &render(const Model &m, int w, int h, float yawDeg, float pitchDeg, float zoom, Bg &&background) {
        if (rt_.id == 0 || rt_.texture.width != w || rt_.texture.height != h) {
            if (rt_.id) UnloadRenderTexture(rt_);
            rt_ = LoadRenderTexture(w, h);
            SetTextureFilter(rt_.texture, TEXTURE_FILTER_BILINEAR);
        }
        upload(m);
        float lo[3], hi[3];
        m.framing(lo, hi);
        const float c[3] = {(lo[0] + hi[0]) * .5f, (lo[1] + hi[1]) * .5f, (lo[2] + hi[2]) * .5f};
        const float ext = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 1e-3f});
        const float fovy = 30.f;
        const float dist = ext * 0.62f / std::tan(fovy * 0.5f * 3.14159265f / 180.f) / std::max(zoom, 0.05f);
        const float yaw = yawDeg * 3.14159265f / 180.f, pit = pitchDeg * 3.14159265f / 180.f;
        Camera3D cam{};
        cam.target = {c[0], c[1], c[2]};
        cam.position = {c[0] + std::sin(yaw) * std::cos(pit) * dist, c[1] + std::cos(yaw) * std::cos(pit) * dist, c[2] + std::sin(pit) * dist};
        cam.up = {0.f, 0.f, 1.f};   // Z para cima no mundo do monstro
        cam.fovy = fovy;
        cam.projection = CAMERA_PERSPECTIVE;
        // luz de cima, da frente-esquerda da camera
        const float L0[3] = {std::sin(yaw - 0.7f) * 0.6f, std::cos(yaw - 0.7f) * 0.6f, 0.8f};
        const float ln = std::sqrt(L0[0] * L0[0] + L0[1] * L0[1] + L0[2] * L0[2]);
        const float L[3] = {L0[0] / ln, L0[1] / ln, L0[2] / ln};

        BeginTextureMode(rt_);
        rlEnableDepthMask();
        ClearBackground(BLANK);
        background();
        rlDrawRenderBatchActive();
        const double nearWas = rlGetCullDistanceNear(), farWas = rlGetCullDistanceFar();
        rlSetClipPlanes(double(dist) * 0.05, double(dist) * 4.0);
        BeginMode3D(cam);
        rlEnableDepthTest();
        rlDisableBackfaceCulling();
        BeginShaderMode(shader_);
        // passe 0: sem ABE (opaco, grava Z); passe 1: com ABE, na ordem do modelo (o Z segue o ZMSK)
        const auto &vs = m.verts();
        for (int pass = 0; pass < 2; ++pass) {
            size_t i = 0;
            while (i + 2 < vs.size()) {
                const PVtx &h0 = vs[i];
                if (int(h0.abe != 0) != pass) { i += 3; continue; }
                // corrida de triangulos com o mesmo estado
                size_t j = i;
                auto same = [&](const PVtx &q) { return int(q.abe != 0) == pass && q.tex == h0.tex && q.blend == h0.blend && q.aref == h0.aref && q.zmsk == h0.zmsk; };
                while (j + 2 < vs.size() && same(vs[j]) && j - i < 3 * 1500) j += 3;
                setState(pass ? h0.blend : 0x44, pass != 0, h0.aref, pass && h0.zmsk);
                rlBegin(RL_TRIANGLES);
                rlSetTexture(h0.tex >= 0 && size_t(h0.tex) < gpu_.size() ? gpu_[size_t(h0.tex)].id : rlGetTextureIdDefault());
                for (size_t k = i; k < j; ++k) {
                    const PVtx &v = vs[k];
                    const float d = std::max(0.f, v.nx * L[0] + v.ny * L[1] + v.nz * L[2]);
                    const float lit = (v.nx == 0.f && v.ny == 0.f && v.nz == 0.f) ? 1.f : 0.7f + 0.6f * d;
                    auto ch = [&](uint8_t x) { return uint8_t(std::min(255.f, float(x) * 2.f * lit)); };
                    rlColor4ub(ch(v.r), ch(v.g), ch(v.b), uint8_t(std::min(255, v.a * 2)));
                    rlTexCoord2f(v.u, v.v);
                    rlVertex3f(v.x, v.y, v.z);
                }
                rlEnd();
                rlDrawRenderBatchActive();
                i = j;
            }
        }
        rlSetTexture(0);
        EndShaderMode();
        EndBlendMode();
        rlEnableDepthMask();
        rlEnableBackfaceCulling();
        EndMode3D();
        EndTextureMode();
        rlSetClipPlanes(nearWas, farWas);
        return rt_;
    }
    const RenderTexture2D &render(const Model &m, int w, int h, float yawDeg, float pitchDeg, float zoom = 1.f) {
        return render(m, w, h, yawDeg, pitchDeg, zoom, [] {});
    }

private:
    // Estado de um lote: fatores do GL que reproduzem a mistura do GS sobre cor pre-multiplicada e a saida do shader
    // (modo 0: (C·a, a); 1: (C·a, 0) — soma ou subtrai sem mexer na cobertura; 2: (0, a) — so escurece o destino)
    void setState(uint8_t blend, bool abe, int16_t aref, bool zmsk) {
        int mode = 0;
        if (!abe) { mode = 3; rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD); }   // opaco: alfa 1
        else if (blend == 0x48) { mode = 1; rlSetBlendFactors(RL_ONE, RL_ONE, RL_FUNC_ADD); }
        else if (blend == 0x42) { mode = 1; rlSetBlendFactors(RL_ONE, RL_ONE, RL_FUNC_REVERSE_SUBTRACT); }
        else if (blend == 0x46) { mode = 2; rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD); }
        else if (blend == 0x81) { mode = 0; rlSetBlendFactorsSeparate(RL_ONE, RL_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_REVERSE_SUBTRACT, RL_FUNC_ADD); }
        else { mode = 0; rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD); }
        BeginBlendMode(blend == 0x81 && abe ? BLEND_CUSTOM_SEPARATE : BLEND_CUSTOM);
        if (zmsk) rlDisableDepthMask(); else rlEnableDepthMask();
        const float mf = float(mode), ar = float(aref);
        SetShaderValue(shader_, locMode_, &mf, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader_, locAref_, &ar, SHADER_UNIFORM_FLOAT);
    }

    void upload(const Model &m) {
        if (!shader_.id) {
            // As do GS = alfa da textura * alfa do vertice (0x80 = 1); o teste descarta As*128 <= aref (aref < 0: sem teste)
            static const char *fs =
                "#version 330\n"
                "in vec2 fragTexCoord; in vec4 fragColor; uniform sampler2D texture0; uniform vec4 colDiffuse;\n"
                "uniform float mode; uniform float aref; out vec4 finalColor;\n"
                "void main() {\n"
                "  vec4 c = texture(texture0, fragTexCoord) * fragColor * colDiffuse;\n"
                "  if (aref >= 0.0 && c.a * 128.0 <= aref) discard;\n"
                "  if (mode > 2.5) finalColor = vec4(c.rgb, 1.0);\n"
                "  else if (mode > 1.5) finalColor = vec4(0.0, 0.0, 0.0, c.a);\n"
                "  else if (mode > 0.5) finalColor = vec4(c.rgb * c.a, 0.0);\n"
                "  else finalColor = vec4(c.rgb * c.a, c.a);\n"
                "}\n";
            shader_ = LoadShaderFromMemory(nullptr, fs);
            locMode_ = GetShaderLocation(shader_, "mode");
            locAref_ = GetShaderLocation(shader_, "aref");
        }
        if (gen_ == m.gen() && gpu_.size() == m.textures().size()) return;
        for (Texture2D &t : gpu_) if (t.id) UnloadTexture(t);
        gpu_.clear();
        gen_ = m.gen();
        for (const TexImage &ti : m.textures()) {
            Image img{const_cast<uint32_t *>(ti.rgba.data()), ti.w, ti.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            Texture2D t = LoadTextureFromImage(img);
            SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
            gpu_.push_back(t);
        }
    }

    uint32_t gen_ = 0;
    std::vector<Texture2D> gpu_;
    RenderTexture2D rt_{};
    Shader shader_{};
    int locMode_ = -1, locAref_ = -1;
};

}  // namespace wotm::model
