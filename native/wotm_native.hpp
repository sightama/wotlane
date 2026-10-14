// Camada nativa do WotM: le a cena direto da RAM do EE e desenha com raylib (sem VU1/GS).
//
// Usada de dois jeitos: pelo runtime recompilado (wotm_scene.inc, a cada quadro, sobre a RDRAM viva)
// e pelo visualizador standalone (native/viewer.cpp, sobre um dump `ram_<quadro>.bin`).
// Formatos: docs/FORMATOS.md. Este arquivo nao contem dados do jogo.
//
// Convencoes do jogo (PS2): vetores-linha (p' = p * M), mundo com Z para cima, float32 little-endian.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <map>
#include <set>
#include <cstring>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "rlgl.h"
#include "wotm_gs.hpp"

namespace wotm {

// ---------------------------------------------------------------- RAM

struct Ram {
    const uint8_t *p = nullptr;
    size_t size = 0;

    bool ok(uint32_t a, uint32_t n = 4) const { return p && a >= 0x1000 && uint64_t(a) + n <= size; }
    uint32_t u32(uint32_t a) const { uint32_t v = 0; if (ok(a)) std::memcpy(&v, p + a, 4); return v; }
    uint16_t u16(uint32_t a) const { uint16_t v = 0; if (ok(a, 2)) std::memcpy(&v, p + a, 2); return v; }
    int16_t s16(uint32_t a) const { return int16_t(u16(a)); }
    uint8_t u8(uint32_t a) const { return ok(a, 1) ? p[a] : 0; }
    int8_t s8(uint32_t a) const { return int8_t(u8(a)); }
    float f32(uint32_t a) const { float v = 0; if (ok(a)) std::memcpy(&v, p + a, 4); return v; }
};

// Enderecos fixos do retail NTSC-U (SCUS_971.97): symbol_addrs.txt
namespace addr {
constexpr uint32_t world = 0x006F87C4;            // _worldctx *world (ep@0, skyCs@4, eo@0x20, weMat@0x30)
constexpr uint32_t worldCtx = 0x00445CD0;         // _worldctx worldCtx[5] (0x1F0 cada)
constexpr uint32_t viewInfo = 0x006E1900;         // 5 x 0x14: [0] = _cs* da camera, [1] = viewport*
constexpr uint32_t worldToScreenMat = 0x006E1FD0; // 5 x 0x40
constexpr uint32_t csActiveList = 0x0043A2C0;     // CsPool::m_activeList (CsNode: cs@0, next@4)
constexpr uint32_t csHPActiveList = 0x0043A2E0;   // CsPool::m_HPActiveList
constexpr uint32_t tempVramTexAddr = 0x006F8818;  // fim dos recursos (.RTX) na VRAM, em palavras; >> 6 = bloco base das paletas
constexpr uint32_t fileStatus = 0x00445390;      // FileStatus: resAddr@0x78, resLoaded@0xB0, names[14][9]@0xB1, maxTexAddr[14]@0x14C
constexpr uint32_t texInfo = 0x0050F100;         // texInfo[1000] x 16: [0] = pacote de upload da textura (RAM), [4] = residencia/VRAM
constexpr uint32_t fontPacket = 0x00735900;      // pacote GIF do texto do quadro (fontBuildPrim/fontDmaFontData): cabecalho A+D, depois corridas de sprites
constexpr uint32_t psBlkLst = 0x0051C600;        // _pshadowblocker[10] x 0x30: cs@0x10, cs2@0x18, &origem@0x1C, &raio@0x20, fator@0x24, emUso u16@0x2E
constexpr uint32_t psLitDirWs = 0x0054CC90;      // direcao da luz das sombras (da cena para a luz), mundo
constexpr uint32_t psShadowIntens = 0x006F8820;  // float, 80 = 0,625 (escala do GS)
constexpr uint32_t psShadowOn = 0x006F8D7C;      // psSetShadowOnOff
}

}  // namespace wotm

// Particulas e efeitos: precisa de Ram (acima); a Scene os desenha depois dos translucidos
#include "wotm_fx.hpp"

namespace wotm {

// ---------------------------------------------------------------- matrizes (vetores-linha)

struct M4 {
    float m[4][4];
};

inline M4 ident() {
    M4 r{};
    for (int i = 0; i < 4; ++i) r.m[i][i] = 1.f;
    return r;
}

// a primeiro, depois b
inline M4 mul(const M4 &a, const M4 &b) {
    M4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[i][k] * b.m[k][j];
            r.m[i][j] = s;
        }
    return r;
}

inline M4 translateM(float x, float y, float z) {
    M4 r = ident();
    r.m[3][0] = x; r.m[3][1] = y; r.m[3][2] = z;
    return r;
}

inline M4 scaleM(float x, float y, float z) {
    M4 r = ident();
    r.m[0][0] = x; r.m[1][1] = y; r.m[2][2] = z;
    return r;
}

inline M4 readM4(const Ram &ram, uint32_t a) {
    M4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) r.m[i][j] = ram.f32(a + 16 * i + 4 * j);
    return r;
}

inline void xform(const M4 &m, float x, float y, float z, float out[3]) {
    out[0] = x * m.m[0][0] + y * m.m[1][0] + z * m.m[2][0] + m.m[3][0];
    out[1] = x * m.m[0][1] + y * m.m[1][1] + z * m.m[2][1] + m.m[3][1];
    out[2] = x * m.m[0][2] + y * m.m[1][2] + z * m.m[2][2] + m.m[3][2];
}

// ---------------------------------------------------------------- malha (polyPkt)

// Um HierObject::polyPkt e uma sub-cadeia de DMA (tag QWC/ID=ret + 8 bytes) com um fluxo VIF para o microcodigo do VU1:
//   UNPACK V3-32 @0xB5   posicoes (ate 190)        UNPACK V3-8 @0x175   normais (signed /127)
//   por secao de strip (terminada por MSCNT): tag GIF em H (V4-32, NLOOP = n vertices), depois 3 qwords por vertice
//   (ST, RGBAQ, XYZF): S-8 @H+1 indice da posicao, V4-8 @H+2 RGBA (0x80 = 1.0), V2-16 @H+3 UV (4096 = 1.0).
//   STMASK 0xBFBFBFBF + S-8 @H+3+3k marca ADC (nao desenha) no vertice k: reinicia o strip.
constexpr uint8_t kSlotNoTex = 31;   // Vtx::slot de secao com PRIM.TME 0 na tag GIF: o GS desenha so a cor de vertice

struct Vtx {
    float x, y, z;
    float u, v;
    uint8_t r, g, b, a;
    float w = 1.f;                    // peso do osso de x (so objetos skinned)
    uint8_t cx = 0, cy = 0, cz = 0;   // codigos de osso (endereco de qword no VU1: slot = (codigo & 0x7F) / 4)
    uint8_t slot = 0;                 // indice do gsTexCtx/texId da secao (palavra 3 da tag GIF = 1 + 3*slot; 0 = slot 0); kSlotNoTex = sem textura
    uint8_t sub = 0;                  // indice do sub-pacote da cadeia (diagnostico)
    uint8_t isAdc = 0;                // triangulo normalmente descartado pelo ADC (so com WOTM_SHOWADC)
};

struct MeshData {
    std::vector<Vtx> tris;   // lista de triangulos (3 vertices por triangulo)
    uint32_t qwc = 0;
    uint32_t sig = 0;        // assinatura barata para detectar RAM reaproveitada por outro nivel
    uint16_t texId = 0;
    bool valid = false;
    bool lowAlpha = false;   // algum vertice com alfa baixo (< 0x3C; 0x40 = opaco): nevoa/sombras, desenhado no passe translucido
    bool skinned = false;    // polyPkt e uma cadeia de sub-pacotes com V4-32 (codigos de osso + peso)
    bool abe = false;        // PRIM.ABE da tag GIF: so entao o alfa de vertice vale (blend); senao opaco
    uint32_t slotMask = 0;   // slots de textura usados pelas secoes (bit i = gsTexCtx[i])
    uint8_t curSub = 0;   // sub-pacote em decodificacao (diagnostico)
    uint8_t curSlot = 0;  // textura corrente do GS: seletor 0 na tag = nao troca o contexto (herda o da secao anterior)
    uint8_t curTme = 1;   // PRIM.TME em vigor (tag GIF com PRE; sem PRE o GS mantem o PRIM anterior)
    uint32_t posTot = 0, posUsed = 0;   // posicoes carregadas / usadas por algum indice (diagnostico)
    uint32_t dropAdc = 0, dropDegen = 0;   // triangulos descartados por ADC / degenerados (diagnostico)
    uint32_t secTot = 0, secBad = 0, secBadEx = 0;   // secoes de tira / com nloop != indices (diagnostico)
    uint32_t oobIdx = 0, totIdx = 0;   // indices de vertice fora de `pos` (viram (0,0,0)) / total: diagnostico de decodificacao
};

// Decodifica um sub-pacote em `md`; devolve o endereco do proximo (cadeia skinned) ou 0.
inline uint32_t decodePacket(const Ram &ram, uint32_t pp, MeshData &md, bool first) {
    uint32_t skip = 0;
    while (skip < 0x400 && ram.ok(pp + skip, 16) && ram.u32(pp + skip) == 0 && ram.u32(pp + skip + 4) == 0) skip += 16;   // qwords zerados entre sub-pacotes
    pp += skip;
    if (!ram.ok(pp, 16)) return 0;
    const uint32_t tag = ram.u32(pp);
    if (((tag >> 28) & 7) != 6) return 0;
    const bool skinnedPkt = (tag & 0x10000) != 0;   // bit 16: vertices V4-32 com codigos de osso
    const uint32_t qwc = tag & 0xFFFF;
    uint32_t nwords = (qwc + 1) * 4 - 2;
    const uint32_t base = pp + 8;
    if (skinnedPkt) nwords = ram.size > base ? uint32_t(std::min<size_t>(60000, (ram.size - base) / 4)) : 0;   // fim = dois NOPs seguidos
    if (nwords == 0 || (!skinnedPkt && !ram.ok(base, nwords * 4))) return 0;
    if (skinnedPkt) md.skinned = true;
    if (first) {
        md.qwc = qwc;
        md.sig = tag ^ ram.u32(pp + 8) ^ (ram.u32(pp + 12) * 31u) ^ (skinnedPkt ? 0u : ram.u32(pp + 4 * (nwords + 1)) * 17u);
        md.valid = true;
    }
    uint32_t endPp = 0;

    struct P3 { float x, y, z, w; uint8_t cx, cy, cz; };
    std::vector<P3> pos;
    std::vector<char> usedPos;   // diagnostico: posicao referenciada por algum indice
    uint32_t hdrAddr = 0, nloop = 0, hdrSel = 0;
    bool haveHdr = false;
    std::vector<uint8_t> idx;
    std::vector<std::array<uint8_t, 4>> rgba;
    std::vector<std::array<int16_t, 2>> uv;
    std::vector<uint32_t> adc;
    uint32_t mask = 0;

    auto flush = [&]() {
        if (haveHdr && !idx.empty()) {
            const uint32_t n = std::min<uint32_t>(nloop, uint32_t(idx.size()));
            ++md.secTot;
            if (nloop != idx.size() || rgba.size() < n || uv.size() < n) { ++md.secBad; if (md.secBadEx < 4) { ++md.secBadEx; std::fprintf(stderr, "   secao: nloop=%u idx=%zu rgba=%zu uv=%zu" "%c", nloop, idx.size(), rgba.size(), uv.size(), 10); } }
            std::vector<bool> restart(n + 1, false);
            static const int adcMode = std::getenv("WOTM_ADCMODE") ? std::atoi(std::getenv("WOTM_ADCMODE")) : 0;   // diagnostico: 1 = ignora ADC
            static const int adcShift = std::getenv("WOTM_ADCSHIFT") ? std::atoi(std::getenv("WOTM_ADCSHIFT")) : 0;   // diagnostico
            if (adcMode != 1) for (uint32_t k : adc) { const int kk = int(k) + adcShift; if (kk >= 0 && uint32_t(kk) < n) restart[uint32_t(kk)] = true; }
            { static const bool hs = std::getenv("WOTM_HDRSEL") != nullptr; if (hs) std::fprintf(stderr, "[hdrsel] tex=%u sel=%u n=%u\n", md.texId, hdrSel, n); }   // diagnostico
            std::vector<Vtx> strip;
            // ADC (bit de GIF no vertice k): o GS NAO desenha o triangulo que termina em k, mas a tira continua (a convencao de
            // "novo strip" e ADC nos dois primeiros vertices). Tratar como reinicio perdia um triangulo por reinicio (furos).
            auto emit = [&]() {
                for (size_t k = 2; k < strip.size(); ++k) {
                    static const bool showAdc = std::getenv("WOTM_SHOWADC") != nullptr;
                    const bool adcTri = k < restart.size() && restart[k];
                    if (adcTri) { ++md.dropAdc; if (!showAdc) continue; }
                    const Vtx &a = strip[(k & 1) ? k - 1 : k - 2];
                    const Vtx &b = strip[(k & 1) ? k - 2 : k - 1];
                    const Vtx &c = strip[k];
                    // descarta triangulos degenerados (juncao de strips por vertice repetido)
                    auto same = [](const Vtx &p, const Vtx &q) { return p.x == q.x && p.y == q.y && p.z == q.z; };
                    if (same(a, b) || same(b, c) || same(a, c)) { ++md.dropDegen; continue; }
                    { Vtx ta = a, tb = b, tc = c; ta.isAdc = tb.isAdc = tc.isAdc = adcTri ? 1 : 0; md.tris.push_back(ta); md.tris.push_back(tb); md.tris.push_back(tc); }
                }
                strip.clear();
            };
            for (uint32_t k = 0; k < n; ++k) {
                const uint32_t j = idx[k];
                ++md.totIdx; if (j >= pos.size()) ++md.oobIdx;
                if (j < pos.size()) { if (usedPos.size() < pos.size()) usedPos.resize(pos.size(), 0); usedPos[j] = 1; }
                const P3 p = j < pos.size() ? pos[j] : P3{0, 0, 0, 1.f, 0, 0, 0};
                Vtx v{};
                v.x = p.x; v.y = p.y; v.z = p.z; v.w = p.w; v.cx = p.cx; v.cy = p.cy; v.cz = p.cz;
                const uint32_t sel = hdrSel & 0x7FFF;   // bit 15: flag (os predios usam 0x8001, 0x8004...)
                if (sel >= 1 && sel <= 13) md.curSlot = uint8_t((sel - 1) / 3);   // 0 = mantem a textura anterior
                v.slot = md.curTme ? md.curSlot : kSlotNoTex;
                v.sub = md.curSub;
                md.slotMask |= 1u << v.slot;
                if (k < rgba.size()) { v.r = rgba[k][0]; v.g = rgba[k][1]; v.b = rgba[k][2]; v.a = rgba[k][3]; }
                else { v.r = v.g = v.b = v.a = 128; }
                if (k < uv.size()) { v.u = uv[k][0] / 4096.f; v.v = uv[k][1] / 4096.f; }
                strip.push_back(v);
            }
            emit();
        }
        haveHdr = false; idx.clear(); rgba.clear(); uv.clear(); adc.clear();
    };

    uint32_t i = 0;
    while (i < nwords) {
        const uint32_t w = ram.u32(base + 4 * i);
        const uint32_t cmd = (w >> 24) & 0x7F, num = (w >> 16) & 0xFF, imm = w & 0xFFFF;
        const uint32_t a = base + 4 * (i + 1);
        if (cmd >= 0x60) {
            const uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, n = num ? num : 256, addr = imm & 0x3FF;
            static const uint32_t comps[4] = {1, 2, 3, 4}, bits[4] = {32, 16, 8, 5};
            const uint32_t nw = vl == 3 ? (n * 16 + 31) / 32 : (n * bits[vl] * comps[vn] + 31) / 32;
            if (vn == 2 && vl == 0 && addr == 0xB5) {                        // V3-32 posicoes
                if (haveHdr) flush();
                pos.resize(n);
                for (uint32_t k = 0; k < n; ++k) pos[k] = {ram.f32(a + 12 * k), ram.f32(a + 12 * k + 4), ram.f32(a + 12 * k + 8), 1.f, 0, 0, 0};
            } else if (vn == 3 && vl == 0 && addr == 0xB5 && n > 1) {         // V4-32 (skinned): codigos de osso nos 8 bits baixos de x/y/z, peso em w
                if (haveHdr) flush();
                pos.resize(n);
                for (uint32_t k = 0; k < n; ++k) {
                    const uint32_t o = a + 16 * k;
                    pos[k] = {ram.f32(o), ram.f32(o + 4), ram.f32(o + 8), ram.f32(o + 12),
                              uint8_t(ram.u32(o) & 0xFF), uint8_t(ram.u32(o + 4) & 0xFF), uint8_t(ram.u32(o + 8) & 0xFF)};
                }
            } else if (vn == 3 && vl == 0 && n == 1) {                        // V4-32: tag GIF
                hdrAddr = addr; nloop = ram.u32(a) & 0x7FFF; haveHdr = true;
                hdrSel = ram.u32(a + 12);   // palavra alta de REGS: o jogo a usa como seletor de textura (1 + 3*slot)
                if ((ram.u32(a + 4) >> 14) & 1) md.curTme = uint8_t((ram.u32(a + 4) >> 19) & 1);   // PRE: PRIM nos bits 47-57 (TME = bit 4)
                if (((ram.u32(a + 4) >> 15) >> 6) & 1) md.abe = true;
            } else if (haveHdr && vn == 0 && vl == 2 && (mask & 0xFF) == 0xBF) {   // S-8 sob mascara: ADC
                for (uint32_t k = 0; k < n; ++k) {
                    const int e = int(addr) - int(hdrAddr) - 3 + 3 * int(k);
                    if (e >= 0 && e % 3 == 0) adc.push_back(uint32_t(e / 3));
                }
            } else if (haveHdr && vn == 0 && vl == 2 && addr == hdrAddr + 1) {      // S-8 indices
                idx.resize(n);
                for (uint32_t k = 0; k < n; ++k) idx[k] = ram.u8(a + k);
            } else if (haveHdr && vn == 3 && vl == 2 && addr == hdrAddr + 2) {      // V4-8 RGBA
                rgba.resize(n);
                for (uint32_t k = 0; k < n; ++k) rgba[k] = {ram.u8(a + 4 * k), ram.u8(a + 4 * k + 1), ram.u8(a + 4 * k + 2), ram.u8(a + 4 * k + 3)};
            } else if (haveHdr && vn == 1 && vl == 1 && addr == hdrAddr + 3) {      // V2-16 UV
                uv.resize(n);
                for (uint32_t k = 0; k < n; ++k) uv[k] = {ram.s16(a + 4 * k), ram.s16(a + 4 * k + 2)};
            }
            i += 1 + nw;
            continue;
        }
        uint32_t extra = 0;
        if (cmd == 0x20) { mask = ram.u32(a); extra = 1; }
        else if (cmd == 0x30 || cmd == 0x31) extra = 4;
        else if (cmd == 0x4A) extra = (num ? num : 256) * 2;
        else if (cmd == 0x50 || cmd == 0x51) extra = (imm ? imm : 65536) * 4;
        else if (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) flush();   // MSCAL/MSCALF/MSCNT: fim da secao
        else if (cmd == 0 && skinnedPkt && i > 8 && ram.u32(a) == 0) {  // dois NOPs seguidos: fim do sub-pacote skinned
            endPp = (base + 4 * (i + 2) + 15) & ~15u;
            break;
        }
        i += 1 + extra;
    }
    flush();
    md.posTot += uint32_t(pos.size());
    for (char c : usedPos) md.posUsed += c ? 1u : 0u;
    return endPp;
}

inline MeshData decodeObject(const Ram &ram, uint32_t node) {
    MeshData md;
    uint32_t pp = ram.u32(node + 4) & 0x0FFFFFFF;
    if (!ram.ok(pp, 16)) return md;
    md.texId = ram.u16(node + 0x50);
    for (int guard = 0; pp && guard < 256; ++guard) { md.curSub = uint8_t(guard); pp = decodePacket(ram, pp, md, guard == 0); }
    for (const Vtx &v : md.tris) if (v.a < 0x3C) { md.lowAlpha = true; break; }
    return md;
}

// ---------------------------------------------------------------- cena

struct DrawItem {
    M4 m;
    uint32_t node;   // HierObject
    uint32_t animPkt = 0;   // CHAR_INSTANCE::animPktPtr (paleta de matrizes dos objetos skinned)
    int lodIdx = 0;         // indice do LOD escolhido (animPktPtr[lodIdx] = cadeia DMA da paleta)
    std::shared_ptr<std::vector<M4>> pal;   // copia das 31 matrizes da paleta (preenchida por takeSnapshot; vazia = ler da RAM)
    uint8_t blend = 0;      // mistura do ALPHA_1 do GROUP que contem o objeto (ctx em +4): 0 = (Cs-Cd)*As+Cd, 1 = Cs*As+Cd (aditivo), 2 = Cd*(1-As)
};

struct Camera {
    float pos[3] = {0, 0, 0};
    float yaw = 0, pitch = 0;   // radianos; yaw 0 olha para +X, Z para cima
};

// ---------------------------------------------------------------- interpolacao entre quadros do jogo
// O jogo atualiza a 30 quadros/s e a tela desenha a 60: sem interpolar, camera e animacoes ficam "picotadas". A camada ao vivo guarda dois
// instantaneos (A anterior, B atual) e desenha a mistura; a rotacao usa slerp (lerp elemento a elemento encolhe a malha em giros rapidos).
struct Quat { float w, x, y, z; };

// Rotacao (linhas = imagens dos eixos) + escala uniforme; falso se a matriz nao for rotacao * escala uniforme (ai o chamador faz lerp simples).
inline bool decomposeRot(const M4 &m, Quat &q, float &scale) {
    float rl[3];
    for (int i = 0; i < 3; ++i) rl[i] = std::sqrt(m.m[i][0] * m.m[i][0] + m.m[i][1] * m.m[i][1] + m.m[i][2] * m.m[i][2]);
    scale = rl[0];
    if (!(scale > 1e-4f) || std::fabs(rl[1] - scale) > 0.03f * scale || std::fabs(rl[2] - scale) > 0.03f * scale) return false;
    float A[3][3];   // matriz-coluna padrao A = (R/escala)^T
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) A[r][c] = m.m[c][r] / scale;
    for (int i = 0; i < 3; ++i) {   // ortogonalidade
        const int j = (i + 1) % 3;
        const float d = m.m[i][0] * m.m[j][0] + m.m[i][1] * m.m[j][1] + m.m[i][2] * m.m[j][2];
        if (std::fabs(d) > 0.03f * scale * scale) return false;
    }
    const float det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
    if (det < 0.9f) return false;   // reflexao: lerp simples
    const float tr = A[0][0] + A[1][1] + A[2][2];
    if (tr > 0.f) { const float t = std::sqrt(tr + 1.f) * 2.f; q = {0.25f * t, (A[2][1] - A[1][2]) / t, (A[0][2] - A[2][0]) / t, (A[1][0] - A[0][1]) / t}; }
    else if (A[0][0] > A[1][1] && A[0][0] > A[2][2]) { const float t = std::sqrt(1.f + A[0][0] - A[1][1] - A[2][2]) * 2.f; q = {(A[2][1] - A[1][2]) / t, 0.25f * t, (A[0][1] + A[1][0]) / t, (A[0][2] + A[2][0]) / t}; }
    else if (A[1][1] > A[2][2]) { const float t = std::sqrt(1.f + A[1][1] - A[0][0] - A[2][2]) * 2.f; q = {(A[0][2] - A[2][0]) / t, (A[0][1] + A[1][0]) / t, 0.25f * t, (A[1][2] + A[2][1]) / t}; }
    else { const float t = std::sqrt(1.f + A[2][2] - A[0][0] - A[1][1]) * 2.f; q = {(A[1][0] - A[0][1]) / t, (A[0][2] + A[2][0]) / t, (A[1][2] + A[2][1]) / t, 0.25f * t}; }
    return true;
}

inline Quat slerp(Quat a, Quat b, float t) {
    float d = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (d < 0.f) { b = {-b.w, -b.x, -b.y, -b.z}; d = -d; }
    float ka, kb;
    if (d > 0.9995f) { ka = 1.f - t; kb = t; }
    else { const float th = std::acos(d), sn = std::sin(th); ka = std::sin((1.f - t) * th) / sn; kb = std::sin(t * th) / sn; }
    Quat r{ka * a.w + kb * b.w, ka * a.x + kb * b.x, ka * a.y + kb * b.y, ka * a.z + kb * b.z};
    const float n = std::sqrt(r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z);
    if (n > 0.f) { r.w /= n; r.x /= n; r.y /= n; r.z /= n; }
    return r;
}

inline M4 lerpM4(const M4 &a, const M4 &b, float t) {
    M4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) r.m[i][j] = a.m[i][j] + (b.m[i][j] - a.m[i][j]) * t;   // translacao e coluna 4 (e fallback da rotacao)
    Quat qa, qb;
    float sa, sb;
    if (decomposeRot(a, qa, sa) && decomposeRot(b, qb, sb)) {
        const Quat q = slerp(qa, qb, t);
        const float s = sa + (sb - sa) * t;
        const float x = q.x, y = q.y, z = q.z, w = q.w;
        const float A[3][3] = {{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
                               {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
                               {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) r.m[i][j] = A[j][i] * s;   // volta para vetores-linha
    }
    return r;
}

// Camera do jogo para uma vista: o que o viewUpdate gravou (worldToScreenMat e worldCtx[view].eo). Sem matriz valida (menus), o
// `_cs` de viewInfo[view] (posicao em +0x10, matriz em +0x20: as linhas sao os eixos do olho em coordenadas de mundo = direita,
// FRENTE, cima). worldToScreenMat tem foco 384 sobre 640 px (HFOV ~79,6 graus) e, em 4:3, o VFOV e ~63,7 graus (o eixo de
// profundidade do olho e o Y).
struct GameCamera {
    bool ok = false;
    float pos[3] = {0, 0, 0};
    float fwd[3] = {0, 1, 0};
    float up[3] = {0, 0, 1};
    float fovy = 63.7f;   // graus
};

inline GameCamera readGameCamera(const Ram &ram, int view = 0) {
    GameCamera c;
    const uint32_t cs = ram.u32(addr::viewInfo + 0x14 * view);
    if (!ram.ok(cs, 0x60)) return c;
    for (int i = 0; i < 3; ++i) {
        c.pos[i] = ram.f32(cs + 0x10 + 4 * i);
        c.fwd[i] = ram.f32(cs + 0x20 + 16 * 1 + 4 * i);
        c.up[i] = ram.f32(cs + 0x20 + 16 * 2 + 4 * i);
    }
    // worldToScreenMat NAO guarda o foco em elementos fixos: a linha 0 e 384*direita + 2048*frente e a linha 1 e -180,48*cima + 2048*frente
    // (o jogo embute os eixos da camera). Ler [0][0]/[1][2] cru dava um FOV que mudava com a orientacao da camera (zoom in/out ao girar).
    const uint32_t wm = addr::worldToScreenMat + 0x40 * view;
    // So a propria matriz: linha 3 = frente (unitaria); o foco vertical e a parte da linha 1 perpendicular a ela. Misturar com o _cs lido
    // em outro instante dava picos de FOV de 1-2 graus em leituras rasgadas (o jogo reescreve a matriz e o _cs em momentos diferentes).
    float r1[3], r3[3], d13 = 0.f, n3 = 0.f;
    for (int i = 0; i < 3; ++i) { r1[i] = ram.f32(wm + 0x10 + 4 * i); r3[i] = ram.f32(wm + 0x30 + 4 * i); d13 += r1[i] * r3[i]; n3 += r3[i] * r3[i]; }
    float fv = 0.f;
    const float f0 = n3 > 0.5f ? 1.f : 0.f;
    if (n3 > 0.5f) {
        for (int i = 0; i < 3; ++i) { const float p = r1[i] - d13 / n3 * r3[i]; fv += p * p; }
        fv = std::sqrt(fv);
    }
    if (f0 > 0.5f && fv > 1.f) {
        c.fovy = 2.f * std::atan(224.f / (2.f * fv)) * 180.f / 3.14159265f;
        // Olho e eixos do que o viewUpdate gravou junto com a matriz (worldCtx[view].eo = -cs->trans, em +0x20), nao do _cs: na
        // introducao da luta o jogo poe outra camera nesse _cs por um instante no meio do quadro, e a leitura pegava esse instante
        // (inteiro ou pela metade: a tela cortava por um quadro para outro canto do mapa).
        const uint32_t eo = addr::worldCtx + 0x1F0 * view + 0x20;
        float un = 0.f;
        for (int i = 0; i < 3; ++i) {
            c.pos[i] = -ram.f32(eo + 4 * i);
            c.fwd[i] = r3[i] / std::sqrt(n3);
            c.up[i] = -(r1[i] - d13 / n3 * r3[i]);   // linha 1 = -180,48*cima + 2048*frente
            un += c.up[i] * c.up[i];
        }
        for (int i = 0; i < 3; ++i) c.up[i] /= std::sqrt(un);
    }
    c.ok = std::isfinite(c.pos[0]) && std::isfinite(c.fwd[0]) && (std::fabs(c.fwd[0]) + std::fabs(c.fwd[1]) + std::fabs(c.fwd[2])) > 0.1f;
    return c;
}

// Particula desenhada como sprite voltado para a camera (destroços, poeira...). Lida de fx[] (0x5BA280, 98 x 12000 bytes).
struct Sprite {
    float pos[3] = {0, 0, 0};
    float size = 1.f;       // lado do quadrado em unidades do mundo
    float rot = 0.f;        // radianos, em torno do eixo de visada
    float rgba[4] = {1, 1, 1, 1};
    uint32_t texId = 0;     // indice em texInfo (0 = circulo suave procedural)
    uint64_t tex0 = 0;      // TEX0 do efeito (psm, CBP da paleta)
    uint32_t cell = 0;      // celula do atlas (celulas de 8x8 texels)
    uint32_t key = 0;       // fx * 4096 + indice: casa a mesma particula entre dois quadros
};

// Glifo do texto do jogo (fontSpritePrint*): um sprite do GS. UV em texels do atlas, XY em pixels da tela do jogo (640 x 224, campo).
struct TextQuad {
    float u0, v0, u1, v1;
    float x0, y0, x1, y1;
    uint8_t r, g, b, a;   // RGBAQ do GS: 0x80 = 1,0 (a cor modula a textura)
    uint64_t tex0;        // TEX0 em vigor (atlas T8H 512x512 do .RTX da fase + paleta CBP)
};

// Instantaneo da cena em um quadro do jogo (itens ja com a paleta copiada) para interpolar entre dois quadros.
struct Snapshot {
    std::vector<Sprite> sprites;
    std::vector<DrawItem> items, sky, overlay;
    GameCamera cam;
    double t = 0.0;     // relogio da apresentacao no momento da captura (segundos)
    uint32_t g = 0;     // contador de quadros do jogo
    bool valid = false;
};

// Mistura de camera: posicao lerp, eixos nlerp, FOV lerp. Corte (teleporte ou giro brusco) = sem mistura.
inline GameCamera lerpCamera(const GameCamera &a, const GameCamera &b, float t) {
    GameCamera r = b;
    if (!a.ok || !b.ok) return r;
    const float dx = b.pos[0] - a.pos[0], dy = b.pos[1] - a.pos[1], dz = b.pos[2] - a.pos[2];
    const float dotf = a.fwd[0] * b.fwd[0] + a.fwd[1] * b.fwd[1] + a.fwd[2] * b.fwd[2];
    if (dx * dx + dy * dy + dz * dz > 400.f * 400.f || dotf < 0.7f) return r;   // corte de camera
    for (int i = 0; i < 3; ++i) {
        r.pos[i] = a.pos[i] + (b.pos[i] - a.pos[i]) * t;
        r.fwd[i] = a.fwd[i] + (b.fwd[i] - a.fwd[i]) * t;
        r.up[i] = a.up[i] + (b.up[i] - a.up[i]) * t;
    }
    for (float *v : {r.fwd, r.up}) {
        const float n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (n > 1e-6f) { v[0] /= n; v[1] /= n; v[2] /= n; }
    }
    r.fovy = a.fovy + (b.fovy - a.fovy) * t;
    return r;
}

class Scene {
public:
    std::vector<DrawItem> items;
    std::vector<DrawItem> sky;   // ceu / nuvens (desenhados antes, centrados na camera)
    std::vector<DrawItem> overlay;   // icones de botao (McButtons): por cima de tudo, sem teste de profundidade
    std::vector<Sprite> sprites;   // particulas (destroços, poeira) lidas de fx[]; desenhadas como sprites voltados para a camera
    std::vector<TextQuad> text;    // glifos do quadro (updateText), desenhados por drawText depois da cena 3D
    uint32_t visited = 0;
    uint32_t byOp[64] = {};

    gs::Vram vram;   // so para depuracao: a VRAM do dump guarda paletas velhas (restos do menu)

    // Paletas da fase, lidas do `.RTX` do disco: entradas de `(palavra0 >> 2) * 16` bytes com cabecalho de 16 bytes (descritor GS
    // em +8: DBP relativo no meia-palavra +0xA, TW/TH nos bits 40..47) e o corpo em ordem logica de paleta (entrada i = palavra i):
    // 16x16 CT32 = 256 cores, 8x2 CT32 = 16 cores. O TEX0.CBP do objeto e `base + DBP`, com base = tempVramTexAddr >> 6.
    // A chave e o CBP ABSOLUTO (base do arquivo + DBP). Base do nivel = tempVramTexAddr >> 6; a do monstro i = fileStatus.maxTexAddr[i-1]
    // (texmResInit empilha os arquivos .RTX na ordem em que o Shell os carregou: nivel, jogadores, IAs).
    std::unordered_map<uint32_t, std::vector<uint32_t>> rtxPal;
    // Imagens residentes do .RTX (PSMT8H/4HL/4HH: telas, fontes, logos do menu): chave (psm << 16) | DBP. Os pixels da RAM em
    // `texInfo[id]` nao servem (o ponteiro aponta para outra coisa); o TBP0 do TEX0 e o DBP da entrada coincidem.
    std::unordered_map<uint32_t, std::vector<uint8_t>> rtxImg;

    void clearRtx() { rtxPal.clear(); rtxImg.clear(); }

    // Carrega o nivel e os monstros do jogo atual a partir do disco extraido (`root` contem LVL/ e MON/). Idempotente: so refaz quando
    // os nomes/bases em fileStatus mudam. Devolve verdadeiro se achou ao menos o arquivo do nivel.
    bool loadResources(const Ram &ram, const std::string &rootArg, std::string *log = nullptr) {
        const uint32_t fs = addr::fileStatus;
        const int n = ram.u8(fs + 0xB0);
        if (n < 1 || n > 14) return false;
        uint32_t sig = ram.u32(addr::tempVramTexAddr) * 2654435761u;
        std::vector<std::string> names;
        names.resize(size_t(n));
        for (int i = 0; i < n; ++i) {
            for (int c = 0; c < 8; ++c) {
                const char ch = char(ram.u8(fs + 0xB1 + 9 * i + c));
                if (!ch) break;
                names[size_t(i)] += char(std::toupper(static_cast<unsigned char>(ch)));
            }
            sig = (sig ^ ram.u16(fs + 0x14C + 2 * i)) * 16777619u;
            for (char ch : names[size_t(i)]) sig = (sig ^ uint8_t(ch)) * 16777619u;
        }
        if (resSig_ == sig && !rtxPal.empty()) return true;
        if (names[0].empty()) return false;
        // Sem raiz explicita: WOTM_DISC (como em wotm_fmv.hpp), senao o diretorio atual.
        std::string root = rootArg;
        if (root.empty())
            if (const char *d = std::getenv("WOTM_DISC")) root = d;
        std::string sep = root.empty() || root.back() == '/' || root.back() == '\\' ? "" : "/";
        rtxPal.clear(); rtxImg.clear();
        const uint32_t base0 = ram.u32(addr::tempVramTexAddr) >> 6;
        bool lvl = loadRtx((root + sep + "LVL/" + names[0] + ".RTX").c_str(), base0);
        // O menu (SHELL) mora em SHELL/SHELL.RTX, nao em LVL/.
        if (!lvl) lvl = loadRtx((root + sep + names[0] + "/" + names[0] + ".RTX").c_str(), base0);
        if (!lvl) lvl = loadRtx((root + sep + "SHELL/" + names[0] + ".RTX").c_str(), base0);   // LOAD, UI, SHELL2, PRESHELL
        if (log) *log = names[0] + (lvl ? "" : " (nao achado)");
        for (int i = 1; i < n; ++i) {
            if (names[size_t(i)].empty()) continue;
            const bool ok = loadRtx((root + sep + "MON/" + names[size_t(i)] + ".RTX").c_str(), ram.u16(fs + 0x14C + 2 * (i - 1)), true);
            if (log) *log += " " + names[size_t(i)] + (ok ? "" : "(?)");
        }
        invalidate();
        resSig_ = sig;
        return lvl;
    }

    bool loadRtx(const char *path, uint32_t base, bool append = false) {
        if (!append) { rtxPal.clear(); rtxImg.clear(); }
        std::vector<uint8_t> raw;
        if (FILE *f = std::fopen(path, "rb")) {
            std::fseek(f, 0, SEEK_END);
            raw.resize(size_t(std::ftell(f)));
            std::fseek(f, 0, SEEK_SET);
            if (std::fread(raw.data(), 1, raw.size(), f) != raw.size()) raw.clear();
            std::fclose(f);
        }
        if (raw.size() < 34) return false;
        std::vector<uint8_t> data;
        static const uint8_t kMagic[4] = {'I', 'E', 3, 4};
        if (std::memcmp(raw.data(), kMagic, 4) == 0) {   // zip com o magic trocado, um arquivo em deflate cru
            uint32_t csize = 0;
            uint16_t nlen = 0, xlen = 0;
            std::memcpy(&csize, raw.data() + 18, 4);
            std::memcpy(&nlen, raw.data() + 26, 2);
            std::memcpy(&xlen, raw.data() + 28, 2);
            const size_t off = 30 + nlen + xlen;
            if (off + csize > raw.size()) return false;
            int outSize = 0;
            unsigned char *d = DecompressData(raw.data() + off, int(csize), &outSize);
            if (!d || outSize <= 0) return false;
            data.assign(d, d + outSize);
            MemFree(d);
        } else {
            data = raw;
        }
        size_t off = 0, added = 0;
        while (off + 16 <= data.size()) {
            uint32_t w[4];
            std::memcpy(w, data.data() + off, 16);
            if (w[0] == 0) break;
            const size_t sz = size_t(w[0] >> 2) * 16;
            if (sz < 16 || off + sz > data.size()) break;
            const uint64_t desc = uint64_t(w[2]) | (uint64_t(w[3]) << 32);
            const uint32_t psm = uint32_t(desc >> 32) & 0x3F, tw = uint32_t(desc >> 40) & 15, th = uint32_t(desc >> 44) & 15;
            if (psm == 0 && ((tw == 4 && th == 4) || (tw == 3 && th == 1))) {
                std::vector<uint32_t> p((sz - 16) / 4);
                std::memcpy(p.data(), data.data() + off + 16, p.size() * 4);
                rtxPal[base + ((w[2] >> 16) & 0xFFFF)] = std::move(p);
                ++added;
            } else if ((psm == gs::T8H || psm == gs::T4HL || psm == gs::T4HH) && sz > 0x80) {
                rtxImg[(psm << 16) | ((w[2] >> 16) & 0xFFFF)].assign(data.begin() + long(off) + 0x80, data.begin() + long(off + sz));
            }
            off += sz;
        }
        return added > 0;
    }

    void invalidate() {
        cache_.clear();
        for (auto &kv : tex_) if (kv.second.id) UnloadTexture(kv.second);
        tex_.clear();
        for (auto &kv : fontTex_) if (kv.second.id) UnloadTexture(kv.second);
        fontTex_.clear();
    }

    // TEX0 do objeto (gsTexCtx[slot]); 0 quando o objeto nao tem textura ou a secao e kSlotNoTex
    static uint64_t objectTex0(const Ram &ram, uint32_t node, uint32_t slot = 0) {
        const uint32_t n = slot == kSlotNoTex ? 0 : ram.u16(node + 0x5A);
        if (n == 0) return 0;
        if (slot >= n || slot >= 5) slot = 0;
        return uint64_t(ram.u32(node + 0x70 + 0x30 * slot)) | (uint64_t(ram.u32(node + 0x74 + 0x30 * slot)) << 32);
    }
    // texId do slot (texId[5] em +0x50); slot invalido cai no 0; kSlotNoTex = 0
    static uint32_t objectTexId(const Ram &ram, uint32_t node, uint32_t slot = 0) {
        if (slot == kSlotNoTex) return 0;
        if (slot >= ram.u16(node + 0x5A) || slot >= 5) slot = 0;
        return ram.u16(node + 0x50 + 2 * slot);
    }

    // Chave de textura de um objeto: id no `texInfo` + paleta (CBP/CSA/CPSM do TEX0). 0 = sem textura.
    static uint64_t texKey(const Ram &ram, uint32_t node, uint32_t slot = 0) {
        const uint64_t t0 = objectTex0(ram, node, slot);
        const uint32_t tid = objectTexId(ram, node, slot);
        if (!t0 || !tid) return 0;
        const gs::Tex0 t = gs::Tex0::decode(t0);
        return (uint64_t(tid) << 40) | (uint64_t(t.cbp) << 12) | (uint64_t(t.csa) << 4) | t.cpsm | (1ull << 62);
    }

    // Preenche a paleta de 256 entradas. O GS le CLUTs de 256 cores em CSM1 com os bits 3 e 4 do indice trocados (entradas 8-15 e
    // 16-23 de cada grupo de 32); o .RTX guarda o upload cru, entao a leitura tem que aplicar a troca (sem ela: granulado em degrades).
    // WOTM_PALNOSWAP=1 desliga (diagnostico).
    static void fillPal(uint32_t pal[256], const std::vector<uint32_t> &src, bool is8bit) {
        static const bool swap = std::getenv("WOTM_PALNOSWAP") == nullptr;
        for (size_t i = 0; i < 256 && i < src.size(); ++i) {
            const size_t k = (swap && is8bit && src.size() >= 256) ? ((i & 0xE7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1)) : i;
            pal[i] = gs::fixAlpha(src[k]);
        }
    }

    // Textura do objeto: os pixels vem do pacote de upload apontado por `texInfo[texId]` (pixels lineares a partir de +0x80,
    // descritor em +0x24 largura, +0x26 altura, +0x2B PSM); a paleta vem da VRAM (carregada junto com a fase).
    bool decodeTex(const Ram &ram, uint32_t node, std::vector<uint32_t> &px, uint32_t &w, uint32_t &h, uint32_t slot = 0) const {
        const uint64_t t0v = objectTex0(ram, node, slot);
        if (!t0v) return false;
        const gs::Tex0 t0 = gs::Tex0::decode(t0v);
        const uint32_t ptr = ram.u32(addr::texInfo + 16 * objectTexId(ram, node, slot)) & 0x0FFFFFFF;
        if (!ram.ok(ptr + 0x80)) return false;
        uint32_t pal[256] = {};
        if (t0.psm == gs::T8H || t0.psm == gs::T4HL || t0.psm == gs::T4HH) {
            // Texturas residentes do menu em PSMT8H/4HL/4HH (telas, fontes, logos, retratos): `texInfo[id]` aponta para outra coisa; os
            // pixels vem do .RTX do disco, entrada com DBP == TEX0.TBP0 e mesmo psm (4HL e 4HH repartem o DBP: sao os dois nibbles).
            // 8 bits, ou 4 bits empacotados (nibble baixo primeiro); tamanho = TW/TH do TEX0. Fallback: a RAM em `ptr` (nao confiavel).
            const auto it = rtxPal.find(t0.cbp);
            if (it == rtxPal.end()) return false;
            fillPal(pal, it->second, t0.psm == gs::T8H);
            w = 1u << t0.tw; h = 1u << t0.th;
            const auto im = rtxImg.find((t0.psm << 16) | t0.tbp0);
            if (im != rtxImg.end())
                return gs::decodeUpload(im->second.data(), im->second.size(), w, h, t0.psm == gs::T8H ? gs::T8 : gs::T4, t0.csa, pal, px);
            return gs::decodeUpload(ram.p + ptr, ram.size - ptr, w, h, t0.psm == gs::T8H ? gs::T8 : gs::T4, t0.csa, pal, px);
        }
        return decodeUploadTex(ram, ptr, t0, px, w, h);
    }

    // Pixels do pacote de upload em `ptr` (= texInfo[texId]; descritor em +0x24 largura, +0x26 altura, +0x2B PSM, pixels lineares
    // a partir de +0x80) com a paleta do .RTX pelo CBP do TEX0. Serve aos objetos (decodeTex) e as particulas (particleTexture).
    bool decodeUploadTex(const Ram &ram, uint32_t ptr, const gs::Tex0 &t0, std::vector<uint32_t> &px, uint32_t &w, uint32_t &h,
                         bool grayFallback = false) const {
        if (!ram.ok(ptr + 0x80)) return false;
        uint32_t pal[256] = {};
        const uint32_t psm = ram.u8(ptr + 0x2B);
        if (psm == gs::T8 || psm == gs::T4) {
            // A paleta vem do .RTX carregado (loadRtx); sem ela a textura cai para a cor de vertice.
            const auto it = rtxPal.find(t0.cbp);
            if (it != rtxPal.end()) fillPal(pal, it->second, psm == gs::T8);
            else if (grayFallback) {
                // Particulas: muitas texturas sao mascaras de luminancia/alfa; sem a paleta, rampa cinza com alfa = indice
                const uint32_t n = psm == gs::T8 ? 256 : 16;
                for (uint32_t i = 0; i < n; ++i) { const uint32_t v = i * 255 / (n - 1); pal[i] = v | (v << 8) | (v << 16) | (v << 24); }
            } else return false;
        }
        w = ram.u16(ptr + 0x24); h = ram.u16(ptr + 0x26);
        static const bool t4swz = std::getenv("WOTM_T4SWZ") != nullptr;   // diagnostico: pixels T4 do pacote como memoria do GS (nibbles embaralhados)
        if (t4swz && psm == gs::T4 && w && h && w <= 512 && h <= 512) {
            px.assign(size_t(w) * h, 0);
            const uint32_t bw = std::max<uint32_t>(1, ram.u8(ptr + 0x2A));
            for (uint32_t y = 0; y < h; ++y)
                for (uint32_t x = 0; x < w; ++x) {
                    const uint32_t n = gs::nibT4(0, bw, x, y);
                    const uint8_t b = ram.u8(ptr + 0x80 + n / 2);
                    px[size_t(y) * w + x] = pal[(t0.csa * 16 + ((n & 1) ? (b >> 4) : (b & 15))) & 255];
                }
            return true;
        }
        return gs::decodeUpload(ram.p + ptr + 0x80, ram.size - ptr - 0x80, w, h, psm, t0.csa, pal, px);
    }

    // Textura de particula: texId (indice do texInfo global) + TEX0 da tabela de particulas (psm/CBP). 0 = nao decodificou.
    // WOTM_FXNOGRAY=1: sem a rampa cinza quando a paleta nao esta nos .RTX carregados (diagnostico).
    unsigned particleTexture(const Ram &ram, uint32_t texId, uint64_t tex0) {
        if (!texId || texId >= 1000) return 0;
        const gs::Tex0 t0 = gs::Tex0::decode(tex0);
        const uint64_t key = (uint64_t(texId) << 40) | (uint64_t(t0.cbp) << 12) | (uint64_t(t0.csa) << 4) | t0.cpsm | (1ull << 61);
        const auto it = tex_.find(key);
        if (it != tex_.end()) return it->second.id;
        static const bool gray = std::getenv("WOTM_FXNOGRAY") == nullptr;
        const uint32_t ptr = ram.u32(addr::texInfo + 16 * texId) & 0x0FFFFFFF;
        std::vector<uint32_t> px;
        uint32_t w = 0, h = 0;
        Texture2D t{};
        if (decodeUploadTex(ram, ptr, t0, px, w, h, gray) && w && h && px.size() >= size_t(w) * h) {
            Image img{px.data(), int(w), int(h), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            t = LoadTextureFromImage(img);
            if (t.id) { SetTextureFilter(t, TEXTURE_FILTER_BILINEAR); SetTextureWrap(t, TEXTURE_WRAP_CLAMP); }
        }
        tex_[key] = t;
        return t.id;
    }

    Particles fx;   // sistemas de particulas do quadro (collect junto com a cena, draw depois dos translucidos)

    // ------------------------------------------------------------ sombras projetadas (common/ps)
    // No retail, cada bloqueador ativo (ate 10 em psBlkLst: monstros e afins, registrados por psBlockerAdd) e desenhado de uma camera
    // posta na direcao da luz (vista 4, a `radius * fator` da origem) para uma textura pequena, filtrada e reprojetada sobre os objetos
    // proximos (psBldBlkRObjs). Aqui: a silhueta e rasterizada na CPU num quadro ortografico no espaco da luz (sem trocar de
    // framebuffer dentro do BeginMode3D, o que quebraria o render texture do runtime) e os triangulos dos objetos proximos sao
    // redesenhados com essa textura projetada. docs/passos/NATIVO.md ("Sombras").
    struct ShadowBlocker {
        int index = 0;               // entrada de psBlkLst
        float origin[3] = {0, 0, 0}; // cs->trans
        float extent = 0;            // meio lado do quadro de luz (raio do no * 1,1)
        std::vector<DrawItem> casters;
        std::vector<uint32_t> casterNodes;
    };
    std::vector<ShadowBlocker> shadows;

    // Bloqueadores ativos, como psFindActiveBlockers: sombras ligadas (psSetShadowOnOff), entrada em uso, cs desenhavel e visivel na vista 0
    void collectShadows(const Ram &ram) {
        shadows.clear();
        static const bool off = std::getenv("WOTM_NOSHADOW") != nullptr;
        if (off || !ram.u32(addr::psShadowOn)) return;
        for (int i = 0; i < 10; ++i) {
            const uint32_t e = addr::psBlkLst + 0x30 * uint32_t(i);
            const uint32_t cs = ram.u32(e + 0x10);
            if (ram.u16(e + 0x2E) != 1 || !ram.ok(cs, 0xB0) || !ram.u8(cs + 0xC) || !(ram.u8(cs + 0xE) & 1)) continue;
            ShadowBlocker b;
            b.index = i;
            for (int c = 0; c < 3; ++c) b.origin[c] = ram.f32(cs + 0x10 + 4 * c);
            const uint32_t rp = ram.u32(e + 0x20);   // &raio da esfera do no raiz (GROUP) do cs; 0 se o no nao e GROUP
            const float r = rp ? ram.f32(rp) : 0.f;
            b.extent = (r > 1.f && r < 5000.f ? r : 80.f) * 1.1f;
            std::vector<DrawItem> keep;
            keep.swap(items);
            drawCs(ram, cs, false);
            const uint32_t cs2 = ram.u32(e + 0x18);   // segundo cs (psBlockerSet2ndCs: o que o monstro segura, por exemplo)
            if (ram.ok(cs2, 0xB0) && ram.u8(cs2 + 0xC)) drawCs(ram, cs2, false);
            b.casters.swap(items);
            items.swap(keep);
            for (const DrawItem &it : b.casters) b.casterNodes.push_back(it.node);
            std::sort(b.casterNodes.begin(), b.casterNodes.end());
            if (!b.casters.empty()) shadows.push_back(std::move(b));
        }
    }

    // Desenha as sombras sobre os objetos ja desenhados (depois dos opacos, antes dos translucidos).
    void drawShadows(const Ram &ram) {
        if (shadows.empty()) return;
        float L[3] = {ram.f32(addr::psLitDirWs), ram.f32(addr::psLitDirWs + 4), ram.f32(addr::psLitDirWs + 8)};   // da cena para a luz
        const float ll = std::sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
        if (!(ll > 1e-3f)) return;
        for (float &c : L) c /= ll;
        // base ortonormal do quadro de luz
        const float hint[3] = {std::fabs(L[2]) < 0.9f ? 0.f : 1.f, 0.f, std::fabs(L[2]) < 0.9f ? 1.f : 0.f};
        float R[3] = {hint[1] * L[2] - hint[2] * L[1], hint[2] * L[0] - hint[0] * L[2], hint[0] * L[1] - hint[1] * L[0]};
        const float rl = std::sqrt(R[0] * R[0] + R[1] * R[1] + R[2] * R[2]);
        for (float &c : R) c /= rl;
        const float U[3] = {L[1] * R[2] - L[2] * R[1], L[2] * R[0] - L[0] * R[2], L[0] * R[1] - L[1] * R[0]};
        float inten = ram.f32(addr::psShadowIntens);   // 80 no psVarInit (escala do GS: 0x80 = 1,0)
        inten = (inten > 0.f && inten < 1000.f) ? std::min(1.f, inten / 128.f) : 0.625f;

        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        rlDisableBackfaceCulling();
        rlSetBlendMode(BLEND_ALPHA);
        for (size_t k = 0; k < shadows.size(); ++k) {
            const ShadowBlocker &b = shadows[k];
            const unsigned tex = shadowTexture(ram, k, b, R, U);
            if (!tex) continue;
            const float E = b.extent;
            auto lightUV = [&](const float p[3], float &u, float &v, float &h) {
                const float d[3] = {p[0] - b.origin[0], p[1] - b.origin[1], p[2] - b.origin[2]};
                u = (d[0] * R[0] + d[1] * R[1] + d[2] * R[2]) / E;
                v = (d[0] * U[0] + d[1] * U[1] + d[2] * U[2]) / E;
                h = (d[0] * L[0] + d[1] * L[1] + d[2] * L[2]) / E;   // > 0: entre o bloqueador e a luz
            };
            rlBegin(RL_TRIANGLES);
            rlSetTexture(tex);
            auto receive = [&](const std::vector<DrawItem> &list) {
                for (const DrawItem &it : list) {
                    if (std::binary_search(b.casterNodes.begin(), b.casterNodes.end(), it.node)) continue;
                    // esfera do objeto (+0x10 centro, +0x1C raio) contra o cilindro de sombra
                    float c[3];
                    xform(it.m, ram.f32(it.node + 0x10), ram.f32(it.node + 0x14), ram.f32(it.node + 0x18), c);
                    const float rad = std::fabs(ram.f32(it.node + 0x1C));
                    float u, v, h;
                    lightUV(c, u, v, h);
                    const float rr = rad / E;
                    if (!(std::isfinite(u) && std::isfinite(rr)) || u * u + v * v > (1.5f + rr) * (1.5f + rr) || h - rr > 0.5f) continue;
                    forEachTri(ram, it, [&](const float p[3][3], uint8_t) {
                        float tu[3], tv[3], th[3];
                        for (int i = 0; i < 3; ++i) lightUV(p[i], tu[i], tv[i], th[i]);
                        if ((tu[0] < -1 && tu[1] < -1 && tu[2] < -1) || (tu[0] > 1 && tu[1] > 1 && tu[2] > 1) ||
                            (tv[0] < -1 && tv[1] < -1 && tv[2] < -1) || (tv[0] > 1 && tv[1] > 1 && tv[2] > 1) ||
                            (th[0] > 0.5f && th[1] > 0.5f && th[2] > 0.5f))
                            return;
                        for (int i = 0; i < 3; ++i) {
                            // some acima do bloqueador (lado da luz); puxa o vertice para o olho para nao brigar com a superficie
                            const float fade = th[i] < 0.15f ? 1.f : std::max(0.f, 1.f - (th[i] - 0.15f) / 0.35f);
                            float q[3] = {p[i][0], p[i][1], p[i][2]};
                            if (eyeOk_) {
                                const float dx = eye_[0] - q[0], dy = eye_[1] - q[1], dz = eye_[2] - q[2];
                                const float dl = std::sqrt(dx * dx + dy * dy + dz * dz);
                                if (dl > 1e-3f) { const float s = std::min(2.f, dl * 0.002f) / dl; q[0] += dx * s; q[1] += dy * s; q[2] += dz * s; }
                            }
                            rlColor4ub(255, 255, 255, uint8_t(255.f * inten * fade));
                            rlTexCoord2f(tu[i] * 0.5f + 0.5f, tv[i] * 0.5f + 0.5f);
                            rlVertex3f(q[0], q[1], q[2]);
                        }
                    });
                }
            };
            receive(items);
            rlEnd();
            rlDrawRenderBatchActive();
        }
        rlSetTexture(0);
        rlEnableDepthMask();
        rlEnableBackfaceCulling();
    }

    // Triangulos de um item em coordenadas de mundo (com skinning), todos os slots: f(p, slot de textura da secao)
    template <class F> void forEachTri(const Ram &ram, const DrawItem &it, F &&f) {
        const MeshData *md = mesh(ram, it.node);
        if (!md || !md->valid) return;
        const uint32_t pal = md->skinned ? palette(ram, it) : 0;
        const auto &t = md->tris;
        float p[3][3];
        for (size_t k = 0; k + 2 < t.size(); k += 3) {
            for (int c = 0; c < 3; ++c) {
                if (pal) skinVertex(ram, pal, it.m, t[k + c], p[c]);
                else xform(it.m, t[k + c].x, t[k + c].y, t[k + c].z, p[c]);
            }
            f(p, t[k].slot);
        }
    }

    // Silhueta do bloqueador no quadro de luz: rasterizada na CPU (kShadowRes^2), borrada (o retail filtra com sprites) e enviada
    // como textura preta com alfa = cobertura. Uma textura por posicao em `shadows`, reaproveitada entre quadros.
    unsigned shadowTexture(const Ram &ram, size_t slot, const ShadowBlocker &b, const float R[3], const float U[3]) {
        constexpr int N = kShadowRes;
        std::vector<float> cov(size_t(N * N), 0.f);
        const float E = b.extent;
        for (const DrawItem &it : b.casters) {
            forEachTri(ram, it, [&](const float p[3][3], uint8_t) {
                float x[3], y[3];
                for (int i = 0; i < 3; ++i) {
                    const float d[3] = {p[i][0] - b.origin[0], p[i][1] - b.origin[1], p[i][2] - b.origin[2]};
                    x[i] = ((d[0] * R[0] + d[1] * R[1] + d[2] * R[2]) / E * 0.5f + 0.5f) * N;
                    y[i] = ((d[0] * U[0] + d[1] * U[1] + d[2] * U[2]) / E * 0.5f + 0.5f) * N;
                }
                const float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
                if (!(std::fabs(area) > 1e-6f)) return;
                const int x0 = std::max(1, int(std::floor(std::min({x[0], x[1], x[2]})))), x1 = std::min(N - 2, int(std::ceil(std::max({x[0], x[1], x[2]}))));
                const int y0 = std::max(1, int(std::floor(std::min({y[0], y[1], y[2]})))), y1 = std::min(N - 2, int(std::ceil(std::max({y[0], y[1], y[2]}))));
                for (int py = y0; py <= y1; ++py)
                    for (int px = x0; px <= x1; ++px) {
                        const float cx = px + 0.5f, cy = py + 0.5f;
                        const float w0 = (x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy);
                        const float w1 = (x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy);
                        const float w2 = (x[0] - cx) * (y[1] - cy) - (x[1] - cx) * (y[0] - cy);
                        if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) cov[size_t(py * N + px)] = 1.f;
                    }
            });
        }
        for (int pass = 0; pass < 2; ++pass) {   // caixa 3x3 duas vezes; a borda fica em 0 (CLAMP nao estende sombra)
            std::vector<float> o(cov.size(), 0.f);
            for (int y = 1; y < N - 1; ++y)
                for (int x = 1; x < N - 1; ++x) {
                    float s = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx) s += cov[size_t((y + dy) * N + x + dx)];
                    o[size_t(y * N + x)] = s / 9.f;
                }
            cov.swap(o);
        }
        std::vector<uint32_t> px(cov.size());
        for (size_t i = 0; i < cov.size(); ++i) px[i] = uint32_t(std::min(1.f, cov[i]) * 255.f) << 24;
        if (shadowTex_.size() <= slot) shadowTex_.resize(slot + 1);
        Texture2D &t = shadowTex_[slot];
        if (!t.id) {
            Image img{px.data(), N, N, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            t = LoadTextureFromImage(img);
            if (t.id) { SetTextureFilter(t, TEXTURE_FILTER_BILINEAR); SetTextureWrap(t, TEXTURE_WRAP_CLAMP); }
        } else {
            UpdateTexture(t, px.data());
        }
        return t.id;
    }

    // Textura de efeito (particulas): `texInfo[id]` guarda o pacote (como nos objetos) e o TEX0 do efeito (fx[i].TEX0ptr) traz psm e CBP da paleta.
    bool decodeFxTex(const Ram &ram, uint32_t id, uint64_t tex0, std::vector<uint32_t> &px, uint32_t &w, uint32_t &h) const {
        const gs::Tex0 t0 = gs::Tex0::decode(tex0);
        const uint32_t ptr = ram.u32(addr::texInfo + 16 * id) & 0x0FFFFFFF;
        if (!ram.ok(ptr + 0x80)) return false;
        uint32_t pal[256] = {};
        const uint32_t psm = ram.u8(ptr + 0x2B);
        if (psm == gs::T8 || psm == gs::T4) {
            const auto it = rtxPal.find(t0.cbp);
            if (it == rtxPal.end()) return false;
            fillPal(pal, it->second, psm == gs::T8);
        }
        w = ram.u16(ptr + 0x24); h = ram.u16(ptr + 0x26);
        return w && h && gs::decodeUpload(ram.p + ptr + 0x80, ram.size - ptr - 0x80, w, h, psm, t0.csa, pal, px);
    }

    // diagnostico: WOTM_TEXFROM=<de>:<para> desenha os vertices do slot <de> com a textura do slot <para>
    static uint32_t texRemap(uint32_t slot) {
        static int from = -1, to = -1; static bool init = false;
        if (!init) { init = true; if (const char *e = std::getenv("WOTM_TEXFROM")) std::sscanf(e, "%d:%d", &from, &to); }
        return (from >= 0 && int(slot) == from) ? uint32_t(to) : slot;
    }

    unsigned (*movieTexture)() = nullptr;   // textura do filme (texId 999); 0 = sem quadro. Quem toca o filme preenche (wotm_live.inc)

    unsigned textureId(const Ram &ram, uint32_t node, uint32_t slot = 0) {
        // filme do menu: moviesSetScreenObject poe texId 999 no objeto (wotm_fmv.hpp); a textura vem do tocador, quadro a quadro
        if (movieTexture && objectTexId(ram, node, slot) == 999)
            if (const unsigned id = movieTexture()) return id;
        const uint64_t key = texKey(ram, node, slot);
        if (!key) return 0;
        auto it = tex_.find(key);
        if (it != tex_.end()) return it->second.id;
        Texture2D t{};
        std::vector<uint32_t> px;
        uint32_t w = 0, h = 0;
        if (decodeTex(ram, node, px, w, h, slot)) {
            if (std::getenv("WOTM_ALPHASTAT")) {
                size_t a0 = 0, am = 0;
                for (uint32_t c : px) { const uint32_t a = c >> 24; if (a == 0) ++a0; else if (a < 255) ++am; }
                if (a0 || am) std::fprintf(stderr, "alpha tex=%u %ux%u transparente=%.0f%% parcial=%.0f%%\n", ram.u16(node + 0x50), w, h, 100.0 * a0 / px.size(), 100.0 * am / px.size());
            }
            {
                // Texels com alfa 0 guardam uma cor qualquer (escura); o filtro bilinear a mistura nas bordas e faz uma franja.
                // Espalha a cor dos vizinhos visiveis para dentro dos transparentes (2 passos de 3x3).
                size_t partial = 0, clear = 0;
                for (uint32_t c : px) { const uint32_t a = c >> 24; if (a == 0) ++clear; else if (a < 250) ++partial; }
                partial_[key] = partial * 100 > px.size();   // mais de 1% dos texels semitransparentes
                if (clear && clear < px.size()) {
                    std::vector<uint8_t> solid(px.size());
                    for (size_t i = 0; i < px.size(); ++i) solid[i] = (px[i] >> 24) != 0;
                    for (int pass = 0; pass < 2; ++pass) {
                        std::vector<uint32_t> np = px;
                        std::vector<uint8_t> ns = solid;
                        for (uint32_t y = 0; y < h; ++y)
                            for (uint32_t x = 0; x < w; ++x) {
                                if (solid[y * w + x]) continue;
                                uint32_t r = 0, g = 0, b = 0, n = 0;
                                for (int dy = -1; dy <= 1; ++dy)
                                    for (int dx = -1; dx <= 1; ++dx) {
                                        const size_t q = size_t((y + h + uint32_t(dy)) % h) * w + (x + w + uint32_t(dx)) % w;
                                        if (!solid[q]) continue;
                                        r += px[q] & 255; g += (px[q] >> 8) & 255; b += (px[q] >> 16) & 255; ++n;
                                    }
                                if (n) { np[y * w + x] = (r / n) | ((g / n) << 8) | ((b / n) << 16); ns[y * w + x] = 1; }
                            }
                        px.swap(np);
                        solid.swap(ns);
                    }
                }
            }
            Image img{};
            img.data = px.data(); img.width = int(w); img.height = int(h); img.mipmaps = 1;
            img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            t = LoadTextureFromImage(img);
            if (t.id) { SetTextureFilter(t, TEXTURE_FILTER_BILINEAR); SetTextureWrap(t, TEXTURE_WRAP_REPEAT); }
        }
        tex_[key] = t;
        return t.id;
    }

    // Reune os objetos visiveis a partir de world->ep e das listas de CS.
    // maxLod < 0: LOD automatico por distancia ao olho da camera do jogo (como hierLod); >= 0 forca esse indice (0 = mais grosseiro).
    void collect(const Ram &ram, int maxLod = -1) {
        items.clear();
        sky.clear();
        overlay.clear();
        sprites.clear();
        atMat_ = 0;
        animVal_ = 0;
        blend_ = 0;
        visited = 0;
        std::memset(destrHist_, 0, sizeof destrHist_);
        std::memset(byOp, 0, sizeof byOp);
        seen_.clear();
        lod_ = maxLod;
        eyeOk_ = false;
        {
            const GameCamera gc = readGameCamera(ram, 0);
            cam_ = gc;
            eyeOk_ = gc.ok;
            for (int c = 0; c < 3; ++c) eye_[c] = gc.pos[c];
        }
        const uint32_t wc = ram.u32(addr::world);
        if (ram.ok(wc, 0x40)) {
            const uint32_t ep = ram.u32(wc);
            if (ram.ok(ep)) walk(ram, ep, ident(), 0);
        }
        // ceu da vista 0: skyCs/skyCs2/skyClouds de worldCtx[0] (+4/+8/+0xC); hierTraceSky so olha o epNode e desenha no olho
        {
            std::vector<DrawItem> world;
            world.swap(items);
            const GameCamera gc = readGameCamera(ram, 0);
            for (uint32_t off : {4u, 8u, 0xCu}) drawCs(ram, ram.u32(addr::worldCtx + off), false, gc.ok ? gc.pos : nullptr);
            sky.swap(items);
            items.swap(world);
        }
        csList(ram, addr::csActiveList);
        csList(ram, addr::csHPActiveList);
        fx.collect(ram);          // particulas com cor/textura/mistura do retail (wotm_fx.hpp); destroços (13) ficam com collectParticles
        collectShadows(ram);
        collectParticles(ram);
        static const bool ds = std::getenv("WOTM_DESTRSTAT") != nullptr;
        if (ds) { std::fprintf(stderr, "[destr] estados:"); for (int i = 0; i < 8; ++i) std::fprintf(stderr, " %u", destrHist_[i]); std::fprintf(stderr, "\n"); }
    }

    // Copia o estado atual da cena (depois de collect) para um instantaneo: itens, ceu, camera e as paletas de matrizes dos skinned
    // (a RAM e reescrita a cada quadro do jogo; o instantaneo precisa guardar os valores).
    Snapshot takeSnapshot(const Ram &ram, double now, uint32_t g) {
        Snapshot sn;
        sn.items = items;
        sn.sky = sky;
        sn.overlay = overlay;
        sn.sprites = sprites;
        sn.cam = readGameCamera(ram, 0);
        sn.t = now;
        sn.g = g;
        sn.valid = true;
        std::unordered_map<uint32_t, std::shared_ptr<std::vector<M4>>> byBlock;
        for (std::vector<DrawItem> *list : {&sn.items, &sn.sky})
            for (DrawItem &it : *list) {
                if (!it.animPkt) continue;
                const uint32_t blk = palette(ram, it);
                if (!blk) continue;
                auto &sp = byBlock[blk];
                if (!sp) {
                    sp = std::make_shared<std::vector<M4>>();
                    sp->reserve(32);
                    for (uint32_t k = 0; k < 32; ++k) sp->push_back(readM4(ram, blk + 0x10 + 0x40 * k));
                }
                it.pal = sp;
            }
        return sn;
    }

    // Preenche items/sky com a mistura de A (anterior) e B (atual) em alpha (0 = A, 1 = B) e devolve a camera misturada. Itens que
    // existem so em B entram sem mistura; saltos grandes (teleporte, troca de LOD) tambem. Atualiza o olho usado no desenho.
    GameCamera blend(const Snapshot &A, const Snapshot &B, float alpha) {
        alpha = std::min(1.f, std::max(0.f, alpha));
        auto mix = [&](const std::vector<DrawItem> &la, const std::vector<DrawItem> &lb, std::vector<DrawItem> &out) {
            out.clear();
            out.reserve(lb.size());
            std::unordered_map<uint64_t, const DrawItem *> amap;
            amap.reserve(la.size() * 2);
            {
                std::unordered_map<uint32_t, uint32_t> occ;
                for (const DrawItem &a : la) amap[(uint64_t(a.node) << 8) | std::min<uint32_t>(occ[a.node]++, 255)] = &a;
            }
            std::unordered_map<uint32_t, uint32_t> occ;
            std::map<std::pair<const void *, const void *>, std::shared_ptr<std::vector<M4>>> palCache;
            for (const DrawItem &b : lb) {
                DrawItem r = b;
                const auto f = amap.find((uint64_t(b.node) << 8) | std::min<uint32_t>(occ[b.node]++, 255));
                if (f != amap.end()) {
                    const DrawItem &a = *f->second;
                    const float dx = b.m.m[3][0] - a.m.m[3][0], dy = b.m.m[3][1] - a.m.m[3][1], dz = b.m.m[3][2] - a.m.m[3][2];
                    if (dx * dx + dy * dy + dz * dz < 150.f * 150.f && a.lodIdx == b.lodIdx) {
                        r.m = lerpM4(a.m, b.m, alpha);
                        if (a.pal && b.pal && a.pal->size() == b.pal->size() && a.pal != b.pal) {
                            auto &sp = palCache[{a.pal.get(), b.pal.get()}];
                            if (!sp) {
                                sp = std::make_shared<std::vector<M4>>(a.pal->size());
                                for (size_t k = 0; k < sp->size(); ++k) (*sp)[k] = lerpM4((*a.pal)[k], (*b.pal)[k], alpha);
                            }
                            r.pal = sp;
                        }
                    }
                }
                out.push_back(std::move(r));
            }
        };
        mix(A.items, B.items, items);
        mix(A.sky, B.sky, sky);
        mix(A.overlay, B.overlay, overlay);
        {   // particulas: mesma (fx, indice) nos dois quadros -> posicao, tamanho e rotacao misturados
            std::unordered_map<uint32_t, const Sprite *> am;
            am.reserve(A.sprites.size() * 2);
            for (const Sprite &a : A.sprites) am[a.key] = &a;
            sprites.clear();
            sprites.reserve(B.sprites.size());
            for (const Sprite &b : B.sprites) {
                Sprite r = b;
                const auto f = am.find(b.key);
                if (f != am.end()) {
                    const Sprite &a = *f->second;
                    const float dx = b.pos[0] - a.pos[0], dy = b.pos[1] - a.pos[1], dz = b.pos[2] - a.pos[2];
                    if (dx * dx + dy * dy + dz * dz < 200.f * 200.f) {
                        for (int c = 0; c < 3; ++c) r.pos[c] = a.pos[c] + (b.pos[c] - a.pos[c]) * alpha;
                        r.size = a.size + (b.size - a.size) * alpha;
                        r.rot = a.rot + (b.rot - a.rot) * alpha;
                        for (int c = 0; c < 4; ++c) r.rgba[c] = a.rgba[c] + (b.rgba[c] - a.rgba[c]) * alpha;
                    }
                }
                sprites.push_back(r);
            }
        }
        const GameCamera c = lerpCamera(A.cam, B.cam, alpha);
        cam_ = c;
        eyeOk_ = c.ok;
        for (int i = 0; i < 3; ++i) eye_[i] = c.pos[i];
        return c;
    }

    // ---- Particulas (fx[], 0x5BA280: 98 x 0x2EE0 bytes; o VU1 monta os quadrados, aqui viram sprites voltados para a camera) ----
    // Por efeito: vis.attribs & 0xFF = tipo (3 sprite, 4 texsprite, 13 destroço, 14 pedestre, 15 poeira...), numParticles +0x2ED0, pos[] (+0x2EB0,
    // 16 bytes: xyz mundo e w = rotacao nos destroços), age[] (+0x2EB8, int16), info.lifetime (+0x2E4A), tamanho min/max (+0x2E38/+0x2E3C),
    // texLoad[0] (+0x2C40) = indice em texInfo e TEX0ptr (+0x2EBC) = TEX0 (psm, CBP da paleta). Poeira/fumaça "compostas" (texInfo 0xFFFF) nao
    // tem pixels no jogo (geradas na VRAM): ficam como circulo suave. Destroços (13) usam o atlas FragTex em celulas de 8x8.
    void collectParticles(const Ram &ram) {
        static const bool off = std::getenv("WOTM_NOFX") != nullptr;   // diagnostico: sem particulas
        if (off) return;
        constexpr uint32_t kFx = 0x005BA280u, kStride = 12000u;
        for (uint32_t i = 1; i < 98 && sprites.size() < 6000; ++i) {
            const uint32_t b = kFx + i * kStride;
            const uint32_t np = ram.u32(b + 0x2ED0);
            const uint32_t type = ram.u32(b + 0x2E90) & 0xFF;
            if (!np || np > 1024 || type != 13) continue;   // so destroços; os outros tipos ficam com fx (wotm_fx.hpp: cor, textura e mistura do retail)
            const uint32_t pos = ram.u32(b + 0x2EB0), age = ram.u32(b + 0x2EB8);
            if (!ram.ok(pos, 16 * np) || !ram.ok(age, 2 * np)) continue;
            const int life = int(int16_t(ram.u16(b + 0x2E30 + 0x1A)));
            const float minS = ram.f32(b + 0x2E30 + 8), maxS = ram.f32(b + 0x2E30 + 0xC);
            const uint32_t texLoad = ram.u16(b + 0x2C40);
            const uint32_t t0p = ram.u32(b + 0x2EBC);
            uint64_t tex0 = 0;
            if (ram.ok(t0p, 8)) tex0 = uint64_t(ram.u32(t0p)) | (uint64_t(ram.u32(t0p + 4)) << 32);
            const bool atlas = type == 13 && texLoad != 0xFFFF && tex0;
            for (uint32_t k = 0; k < np; ++k) {
                Sprite s;
                for (int c = 0; c < 3; ++c) s.pos[c] = ram.f32(pos + 16 * k + 4 * c);
                if (!std::isfinite(s.pos[0]) || !std::isfinite(s.pos[1]) || !std::isfinite(s.pos[2])) continue;
                const int a = int(int16_t(ram.u16(age + 2 * k)));
                if (a < 0 || (life > 0 && a >= life)) continue;
                const float t = life > 0 ? float(a) / float(life) : 0.f;
                const uint32_t h = (k * 2654435761u) ^ (i * 40503u);
                s.key = i * 4096u + k;
                if (atlas) {   // destroço: celula e tamanho fixos por particula, gira com w, some no fim da vida
                    s.texId = texLoad; s.tex0 = tex0;
                    s.cell = (h >> 8) & 0xFFFF;
                    s.size = minS + (maxS - minS) * float((h >> 4) & 255) / 255.f;
                    s.rot = ram.f32(pos + 16 * k + 12);
                    s.rgba[3] = t > 0.85f ? (1.f - t) / 0.15f : 1.f;
                } else {       // poeira/fumaça: circulo suave que cresce e esvanece
                    s.size = minS + (maxS > minS ? (maxS - minS) * t : 0.f);
                    if (s.size < 1.f) s.size = 8.f;
                    s.rgba[0] = 0.78f; s.rgba[1] = 0.72f; s.rgba[2] = 0.65f;
                    s.rgba[3] = 0.55f * (1.f - t);
                }
                sprites.push_back(s);
            }
        }
    }

    unsigned fxTexture(const Ram &ram, uint32_t texId, uint64_t tex0, int &w, int &h) {
        static Texture2D soft{};
        if (!texId || !tex0) {
            if (!soft.id) {   // circulo suave (alfa radial)
                std::vector<uint32_t> px(64 * 64);
                for (int y = 0; y < 64; ++y)
                    for (int x = 0; x < 64; ++x) {
                        const float d = std::sqrt(float((x - 32) * (x - 32) + (y - 32) * (y - 32))) / 32.f;
                        const float al = d >= 1.f ? 0.f : (1.f - d) * (1.f - d);
                        px[size_t(y) * 64 + x] = 0x00FFFFFFu | (uint32_t(al * 255.f) << 24);
                    }
                Image img{px.data(), 64, 64, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                soft = LoadTextureFromImage(img);
                SetTextureFilter(soft, TEXTURE_FILTER_BILINEAR);
                SetTextureWrap(soft, TEXTURE_WRAP_CLAMP);
            }
            w = h = 64;
            return soft.id;
        }
        const uint64_t key = (0xF0ull << 56) | (uint64_t(texId) << 32) | uint32_t(gs::Tex0::decode(tex0).cbp);
        auto it = tex_.find(key);
        if (it != tex_.end()) { w = it->second.width; h = it->second.height; return it->second.id; }
        Texture2D t{};
        std::vector<uint32_t> px;
        uint32_t tw = 0, th = 0;
        if (decodeFxTex(ram, texId, tex0, px, tw, th)) {
            Image img{px.data(), int(tw), int(th), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            t = LoadTextureFromImage(img);
            if (t.id) { SetTextureFilter(t, TEXTURE_FILTER_POINT); SetTextureWrap(t, TEXTURE_WRAP_CLAMP); }
        }
        tex_[key] = t;
        w = t.width; h = t.height;
        return t.id;
    }

    // Sprites ordenados do mais longe ao mais perto, sem escrever profundidade; um lote por textura.
    // Cantos (esq-baixo, dir-baixo, dir-cima, esq-cima) do destroço virado para a camera (R/U: eixos da tela), girado por s.rot.
    // Serve ao drawSprites e a checagem VU1.
    static void spriteCorners(const Sprite &s, const float R[3], const float U[3], float c[4][3]) {
        const float cs = std::cos(s.rot), sn = std::sin(s.rot), hs = s.size * 0.5f;
        const float cx[4] = {-1, 1, 1, -1}, cy[4] = {-1, -1, 1, 1};
        for (int a = 0; a < 3; ++a) {
            const float ax = (R[a] * cs + U[a] * sn) * hs, ay = (-R[a] * sn + U[a] * cs) * hs;   // eixos "direita" e "cima" girados
            for (int k = 0; k < 4; ++k) c[k][a] = s.pos[a] + ax * cx[k] + ay * cy[k];
        }
    }

    void drawSprites(const Ram &ram) {
        if (sprites.empty() || !cam_.ok) return;
        float R[3] = {cam_.fwd[1] * cam_.up[2] - cam_.fwd[2] * cam_.up[1], cam_.fwd[2] * cam_.up[0] - cam_.fwd[0] * cam_.up[2], cam_.fwd[0] * cam_.up[1] - cam_.fwd[1] * cam_.up[0]};
        const float rn = std::sqrt(R[0] * R[0] + R[1] * R[1] + R[2] * R[2]);
        if (rn < 1e-6f) return;
        for (float &v : R) v /= rn;
        const float *U = cam_.up;
        std::vector<std::pair<float, uint32_t>> order;
        order.reserve(sprites.size());
        for (uint32_t i = 0; i < sprites.size(); ++i) {
            const Sprite &s = sprites[i];
            const float dx = s.pos[0] - cam_.pos[0], dy = s.pos[1] - cam_.pos[1], dz = s.pos[2] - cam_.pos[2];
            if (dx * cam_.fwd[0] + dy * cam_.fwd[1] + dz * cam_.fwd[2] < 1.f) continue;   // atras da camera
            order.push_back({-(dx * dx + dy * dy + dz * dz), i});   // mais longe primeiro
        }
        std::sort(order.begin(), order.end());
        rlDisableBackfaceCulling();
        rlDisableDepthMask();
        unsigned cur = ~0u;
        bool open = false;
        for (const auto &o : order) {
            const Sprite &s = sprites[o.second];
            int tw = 0, th = 0;
            const unsigned id = fxTexture(ram, s.texId, s.tex0, tw, th);
            if (!id) continue;
            if (id != cur || !open) {
                if (open) { rlEnd(); rlDrawRenderBatchActive(); }
                rlBegin(RL_TRIANGLES);
                rlSetTexture(id);
                cur = id; open = true;
            }
            float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
            if (s.tex0 && tw >= 8 && th >= 8) {   // atlas de celulas 8x8
                const uint32_t cols = uint32_t(tw) / 8, rows = uint32_t(th) / 8, c = s.cell % (cols * rows);
                u0 = float(c % cols) * 8.f / float(tw); u1 = u0 + 8.f / float(tw);
                v0 = float(c / cols) * 8.f / float(th); v1 = v0 + 8.f / float(th);
            }
            const unsigned char cr = uint8_t(255.f * std::min(1.f, s.rgba[0])), cg = uint8_t(255.f * std::min(1.f, s.rgba[1])), cb = uint8_t(255.f * std::min(1.f, s.rgba[2])), ca = uint8_t(255.f * std::max(0.f, std::min(1.f, s.rgba[3])));
            float c[4][3];
            spriteCorners(s, R, U, c);
            const float cu[4] = {u0, u1, u1, u0}, cv[4] = {v1, v1, v0, v0};
            const int tri[6] = {0, 1, 2, 0, 2, 3};
            for (int q = 0; q < 6; ++q) {
                const int k = tri[q];
                rlColor4ub(cr, cg, cb, ca);
                rlTexCoord2f(cu[k], cv[k]);
                rlVertex3f(c[k][0], c[k][1], c[k][2]);
            }
        }
        if (open) { rlEnd(); rlDrawRenderBatchActive(); }
        rlSetTexture(0);
        rlEnableDepthMask();
        rlEnableBackfaceCulling();
    }

    // Diagnostico: faixa de UV por item texturizado (agrupada por tamanho da faixa).
    void dumpUv(const Ram &ram) {
        int shown = 0;
        for (const DrawItem &it : items) {
            const MeshData *md = mesh(ram, it.node);
            if (!md || md->tris.empty() || !texKey(ram, it.node)) continue;
            float u0 = 1e9f, u1 = -1e9f, v0 = 1e9f, v1 = -1e9f;
            for (const Vtx &v : md->tris) { u0 = std::min(u0, v.u); u1 = std::max(u1, v.u); v0 = std::min(v0, v.v); v1 = std::max(v1, v.v); }
            if ((u1 - u0 > 1.01f || v1 - v0 > 1.01f || u0 < -0.01f || v0 < -0.01f) && shown++ < 12)
                std::fprintf(stderr, "uv node=%06x tex=%u u[%.2f %.2f] v[%.2f %.2f] tris=%zu\n", it.node, ram.u16(it.node + 0x50), u0, u1, v0, v1, md->tris.size() / 3);
        }
        std::fprintf(stderr, "uv fora de [0,1]: %d itens (mostrados ate 12)\n", shown);
    }

    // Diagnostico (WOTM_UVISO): razao |grad u| / |grad v| por tamanho de textura. Deu 1,0 ate em 128x64: so mostra que as texturas
    // 128x64 sao arte quadrada comprimida. NAO prova UV em texels: renderizar com texel=uv/16 gera ruido; o UV normalizado (4096 = 1,0) esta certo.
    void dumpUvIso(const Ram &ram) {
        std::map<std::pair<uint32_t, uint32_t>, std::vector<float>> byDim;
        for (const DrawItem &it : items) {
            const MeshData *md = mesh(ram, it.node);
            if (!md || md->skinned || md->tris.empty() || !texKey(ram, it.node)) continue;
            const uint32_t ptr = ram.u32(addr::texInfo + 16 * ram.u16(it.node + 0x50)) & 0x0FFFFFFF;
            const uint32_t w = ram.u16(ptr + 0x24), h = ram.u16(ptr + 0x26);
            if (!w || !h) continue;
            for (size_t i = 0; i + 2 < md->tris.size(); i += 3) {
                const Vtx &a = md->tris[i], &b = md->tris[i + 1], &c = md->tris[i + 2];
                const double e1[3] = {b.x - a.x, b.y - a.y, b.z - a.z}, e2[3] = {c.x - a.x, c.y - a.y, c.z - a.z};
                const double g11 = e1[0]*e1[0] + e1[1]*e1[1] + e1[2]*e1[2], g22 = e2[0]*e2[0] + e2[1]*e2[1] + e2[2]*e2[2];
                const double g12 = e1[0]*e2[0] + e1[1]*e2[1] + e1[2]*e2[2];
                const double det = g11 * g22 - g12 * g12;
                if (det < 1e-3 * g11 * g22 || g11 < 1e-4) continue;   // degenerado/fino demais
                // |grad f|^2 = [d1 d2] G^-1 [d1 d2]^T, com d = diferencas de f ao longo de e1/e2
                auto gmag = [&](double d1, double d2) { return std::sqrt((g22 * d1 * d1 - 2 * g12 * d1 * d2 + g11 * d2 * d2) / det); };
                const double gu = gmag(b.u - a.u, c.u - a.u), gv = gmag(b.v - a.v, c.v - a.v);
                if (gu < 1e-7 || gv < 1e-7) continue;
                byDim[{w, h}].push_back(float(gu / gv));
            }
            {   // TW/TH do TEX0 (2^n) vs descritor, uma vez por textura
                static std::set<uint32_t> seenTex;
                const uint32_t tid = ram.u16(it.node + 0x50);
                if (seenTex.insert(tid).second) {
                    const gs::Tex0 t0 = gs::Tex0::decode(objectTex0(ram, it.node));
                    float u0 = 1e9f, u1 = -1e9f, v0 = 1e9f, v1 = -1e9f;
                    for (const Vtx &v : md->tris) { u0 = std::min(u0, v.u); u1 = std::max(u1, v.u); v0 = std::min(v0, v.v); v1 = std::max(v1, v.v); }
                    std::fprintf(stderr, "uviso-tex id=%u desc=%ux%u tex0 TW=%u TH=%u (=%ux%u) tfx=%u uv[%.2f %.2f][%.2f %.2f]\n", tid, w, h, t0.tw, t0.th, 1u << t0.tw, 1u << t0.th, t0.tfx, u0, u1, v0, v1);
                }
            }
        }
        for (auto &kv : byDim) {
            auto &v = kv.second;
            if (v.size() < 20) continue;
            std::sort(v.begin(), v.end());
            std::fprintf(stderr, "uviso tex %ux%u: tris=%zu razao |gu|/|gv| p25=%.3f med=%.3f p75=%.3f (h/w=%.3f)\n", kv.first.first, kv.first.second, v.size(),
                         v[v.size() / 4], v[v.size() / 2], v[v.size() * 3 / 4], double(kv.first.second) / kv.first.first);
        }
    }

    // Diagnostico (WOTM_SKINFIT="noA,noB,pal" em hex): compara variantes de skinning do no B com o no A (outro LOD do mesmo monstro,
    // tomado como referencia) pela distancia de Chamfer media entre as nuvens de vertices skinned.
    void fitSkin(const Ram &ram, uint32_t nodeA, uint32_t nodeB, uint32_t pal) {
        const MeshData *ma = mesh(ram, nodeA), *mb = mesh(ram, nodeB);
        if (!ma || !mb || ma->tris.empty() || mb->tris.empty()) { std::fprintf(stderr, "skinfit: malha vazia\n"); return; }
        auto cloud = [&](const MeshData *md, int variant) {
            std::vector<std::array<float, 3>> out;
            for (const Vtx &v : md->tris) {
                const int mask = (variant == 1) ? 0xFF : 0x7F;
                const float w = std::min(1.f, std::max(0.f, v.w));
                auto slotOf = [&](uint8_t c) { return int((c & mask) / 4); };
                float a[3], b[3];
                const int sa = slotOf(v.cx), sb = slotOf(v.cy);
                if (sa > 30 || sb > 30) { out.push_back({1e6f, 1e6f, 1e6f}); continue; }
                xform(readM4(ram, pal + 0x10 + 0x40 * sa), v.x, v.y, v.z, a);
                xform(readM4(ram, pal + 0x10 + 0x40 * sb), v.x, v.y, v.z, b);
                float ww = w;
                if (variant == 2) ww = 1.f - w;          // pesos trocados
                if (variant == 3) ww = 1.f;              // so o osso de x
                if (variant == 4) { ww = 0.f; }          // so o osso de y
                out.push_back({a[0] * ww + b[0] * (1 - ww), a[1] * ww + b[1] * (1 - ww), a[2] * ww + b[2] * (1 - ww)});
            }
            return out;
        };
        auto chamfer = [](const std::vector<std::array<float, 3>> &x, const std::vector<std::array<float, 3>> &y) {
            std::vector<double> d;
            const size_t step = std::max<size_t>(1, x.size() / 1500);
            for (size_t i = 0; i < x.size(); i += step) {
                float best = 1e30f;
                for (const auto &q : y) {
                    const float dx = x[i][0] - q[0], dy = x[i][1] - q[1], dz = x[i][2] - q[2];
                    best = std::min(best, dx * dx + dy * dy + dz * dz);
                }
                d.push_back(std::sqrt(double(best)));
            }
            std::sort(d.begin(), d.end());
            auto pc = [&](double q) { return d.empty() ? 0.0 : d[std::min(d.size() - 1, size_t(q * double(d.size())))]; };
            struct R { double med, p90, p99, mx; };
            return R{pc(0.5), pc(0.9), pc(0.99), d.empty() ? 0.0 : d.back()};
        };
        const auto ref = cloud(ma, 0);
        const char *names[5] = {"atual (mask 0x7F, w no osso x)", "mask 0xFF", "pesos trocados", "so osso x", "so osso y"};
        for (int v = 0; v < 5; ++v) {
            const auto c = cloud(mb, v);
            { const auto r1 = chamfer(c, ref), r2 = chamfer(ref, c); std::fprintf(stderr, "skinfit v%d (%s): B->A med %.1f p90 %.1f p99 %.1f max %.1f | A->B med %.1f p90 %.1f p99 %.1f max %.1f\n", v, names[v], r1.med, r1.p90, r1.p99, r1.mx, r2.med, r2.p90, r2.p99, r2.mx); }
        }
    }

    // Diagnostico (WOTM_BONECODES="no"): histograma dos codigos de osso (bit 0x80 e slot) de um no skinned.
    void boneCodes(const Ram &ram, uint32_t node) {
        const MeshData *md = mesh(ram, node);
        if (!md) return;
        size_t hi[3] = {0, 0, 0}, tot = 0, wlt1 = 0;
        std::map<int, size_t> slots[3];
        for (const Vtx &v : md->tris) {
            const uint8_t c[3] = {v.cx, v.cy, v.cz};
            for (int i = 0; i < 3; ++i) { if (c[i] & 0x80) ++hi[i]; ++slots[i][(c[i] & 0x7F) / 4]; }
            ++tot;
            if (v.w < 0.999f) ++wlt1;
        }
        std::fprintf(stderr, "bonecodes no=%06x vertices=%zu | com bit 0x80: x=%zu y=%zu z=%zu | w<1: %zu | slots usados em x: %zu, y: %zu\n", node, tot, hi[0], hi[1], hi[2], wlt1, slots[0].size(), slots[1].size());
        std::string row = "   slots x: ";
        for (auto &kv : slots[0]) row += std::to_string(kv.first) + "(" + std::to_string(kv.second) + ") ";
        std::fprintf(stderr, "%s\n", row.c_str());
    }

    // Diagnostico (WOTM_COVER="ref1,ref2,..:teste1,teste2"): cobertura na pose de referencia (posicoes cruas, sem skinning).
    // Amostra pontos na superficie dos nos de referencia e mede a fracao a menos de `tol` de algum triangulo dos nos de teste.
    void coverage(const Ram &ram, const std::vector<uint32_t> &refs, const std::vector<uint32_t> &tests, float tol) {
        auto gather = [&](const std::vector<uint32_t> &ns) {
            std::vector<std::array<float, 9>> t;
            for (uint32_t n : ns) {
                const MeshData *md = mesh(ram, n);
                if (!md) continue;
                static const uint32_t coverPal = std::getenv("WOTM_COVERPAL") ? uint32_t(std::strtoul(std::getenv("WOTM_COVERPAL"), nullptr, 16)) : 0;   // com paleta: skinned
                for (size_t k = 0; k + 2 < md->tris.size(); k += 3) {
                    if (coverPal) {
                        float q[3][3];
                        const M4 id = ident();
                        for (int c = 0; c < 3; ++c) skinVertex(ram, coverPal, id, md->tris[k + c], q[c]);
                        t.push_back({q[0][0], q[0][1], q[0][2], q[1][0], q[1][1], q[1][2], q[2][0], q[2][1], q[2][2]});
                    } else {
                        const Vtx &a = md->tris[k], &b = md->tris[k + 1], &c = md->tris[k + 2];
                        t.push_back({a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z});
                    }
                }
            }
            return t;
        };
        const auto R = gather(refs), T = gather(tests);
        // distancia ponto-triangulo (Ericson)
        auto dist2 = [](const float *p, const std::array<float, 9> &tr) {
            auto sub = [](const float *a, const float *b, float *o) { for (int i = 0; i < 3; ++i) o[i] = a[i] - b[i]; };
            auto dot = [](const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
            const float *a = &tr[0], *b = &tr[3], *c = &tr[6];
            float ab[3], ac[3], ap[3]; sub(b, a, ab); sub(c, a, ac); sub(p, a, ap);
            const float d1 = dot(ab, ap), d2 = dot(ac, ap);
            auto pt = [&](float u, float v, float w) { float q[3]; for (int i = 0; i < 3; ++i) q[i] = u * a[i] + v * b[i] + w * c[i]; float d[3]; sub(p, q, d); return dot(d, d); };
            if (d1 <= 0 && d2 <= 0) return pt(1, 0, 0);
            float bp[3]; sub(p, b, bp); const float d3 = dot(ab, bp), d4 = dot(ac, bp);
            if (d3 >= 0 && d4 <= d3) return pt(0, 1, 0);
            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0 && d1 >= 0 && d3 <= 0) { const float v = d1 / (d1 - d3); return pt(1 - v, v, 0); }
            float cp[3]; sub(p, c, cp); const float d5 = dot(ab, cp), d6 = dot(ac, cp);
            if (d6 >= 0 && d5 <= d6) return pt(0, 0, 1);
            const float vb = d5 * d2 - d1 * d6;
            if (vb <= 0 && d2 >= 0 && d6 <= 0) { const float w = d2 / (d2 - d6); return pt(1 - w, 0, w); }
            const float va = d3 * d6 - d5 * d4;
            if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) { const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6)); return pt(0, 1 - w, w); }
            const float den = 1.f / (va + vb + vc), v = vb * den, w = vc * den;
            return pt(1 - v - w, v, w);
        };
        size_t n = 0, ok = 0;
        uint32_t rng = 12345;
        auto rnd = [&]() { rng = rng * 1664525u + 1013904223u; return float(rng >> 8) / 16777216.f; };
        for (size_t s = 0; s < 4000 && !R.empty(); ++s) {
            const auto &tr = R[size_t(rnd() * float(R.size())) % R.size()];
            float u = rnd(), v = rnd(); if (u + v > 1) { u = 1 - u; v = 1 - v; }
            float p[3]; for (int i = 0; i < 3; ++i) p[i] = tr[i] * (1 - u - v) + tr[3 + i] * u + tr[6 + i] * v;
            float best = 1e30f;
            for (const auto &tt : T) best = std::min(best, dist2(p, tt));
            ++n; if (best < tol * tol) ++ok;
        }
        std::fprintf(stderr, "cover: %zu triangulos de teste cobrem %.1f%% da superficie de referencia (tol %.1f; %zu amostras)%c", T.size(), n ? 100.0 * double(ok) / double(n) : 0.0, tol, n, 10);
    }

    // Diagnostico (WOTM_SKINSAN=1): sanidade do skinning. Por triangulo, razao entre o perimetro skinned e o da pose de referencia;
    // vertices em triangulos com razao absurda sao "explodidos": agrupa por slot de osso e peso para achar o padrao.
    void dumpSkinSanity(const Ram &ram) {
        for (const DrawItem &it : items) {
            const MeshData *md = mesh(ram, it.node);
            if (!md || !md->skinned || md->tris.size() < 60) continue;
            const uint32_t pal = palette(ram, it);
            if (!pal) { std::fprintf(stderr, "skinsan no=%06x sem paleta\n", it.node); continue; }
            const M4 id = ident();
            size_t bad = 0, total = 0;
            std::map<std::string, size_t> why;
            for (size_t k = 0; k + 2 < md->tris.size(); k += 3) {
                float p[3][3];
                double o = 0, sk = 0;
                for (int c = 0; c < 3; ++c) skinVertex(ram, pal, id, md->tris[k + c], p[c]);
                for (int c = 0; c < 3; ++c) {
                    const Vtx &a = md->tris[k + c], &b = md->tris[k + (c + 1) % 3];
                    o += std::sqrt(double((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)));
                    const float *pa = p[c], *pb = p[(c + 1) % 3];
                    sk += std::sqrt(double((pa[0] - pb[0]) * (pa[0] - pb[0]) + (pa[1] - pb[1]) * (pa[1] - pb[1]) + (pa[2] - pb[2]) * (pa[2] - pb[2])));
                }
                ++total;
                if (o < 1e-4) continue;
                const double r = sk / o;
                if (r > 2.5 || r < 0.4) {
                    ++bad;
                    for (int c = 0; c < 3; ++c) {
                        const Vtx &v = md->tris[k + c];
                        char key[96];
                        std::snprintf(key, sizeof key, "cx=%02x(slot %d) cy=%02x(slot %d) cz=%02x w=%.2f", v.cx, (v.cx & 0x7F) / 4, v.cy, (v.cy & 0x7F) / 4, v.cz, v.w);
                        ++why[key];
                    }
                }
            }
            {   // fracao de triangulos com aresta mais longa que 3x a mediana (pontes espurias entre tiras)
                std::vector<float> longest;
                for (size_t k = 0; k + 2 < md->tris.size(); k += 3) {
                    float q[3][3];
                    for (int c = 0; c < 3; ++c) skinVertex(ram, pal, id, md->tris[k + c], q[c]);
                    float mx = 0;
                    for (int c = 0; c < 3; ++c) {
                        const float *a2 = q[c], *b2 = q[(c + 1) % 3];
                        mx = std::max(mx, std::sqrt((a2[0] - b2[0]) * (a2[0] - b2[0]) + (a2[1] - b2[1]) * (a2[1] - b2[1]) + (a2[2] - b2[2]) * (a2[2] - b2[2])));
                    }
                    longest.push_back(mx);
                }
                std::vector<float> sorted = longest;
                std::sort(sorted.begin(), sorted.end());
                const float med = sorted.empty() ? 0.f : sorted[sorted.size() / 2];
                size_t nl = 0;
                for (float l : longest) if (l > 3.f * med) ++nl;
                std::fprintf(stderr, "skinsan-longedge no=%06x mediana %.2f, triangulos > 3x: %zu de %zu (%.1f%%)\n", it.node, med, nl, longest.size(), longest.empty() ? 0.0 : 100.0 * nl / longest.size());
            }
            std::fprintf(stderr, "skinsan no=%06x tris=%zu ruins=%zu (%.1f%%)\n", it.node, total, bad, total ? 100.0 * bad / total : 0.0);
            std::vector<std::pair<size_t, std::string>> v;
            for (auto &kv : why) v.push_back({kv.second, kv.first});
            std::sort(v.rbegin(), v.rend());
            for (size_t i = 0; i < v.size() && i < 8; ++i) std::fprintf(stderr, "   %5zu x %s\n", v[i].first, v[i].second.c_str());
        }
    }

    // Diagnostico: lista os itens skinned (no, triangulos, posicao no mundo, paleta achada?).
    void dumpSkinned(const Ram &ram) {
        if (std::getenv("WOTM_LISTRIGID")) {   // diagnostico: partes rigidas dos personagens (itens nao skinned com animPkt): no, textura, raio, posicao no mundo, blend
            for (const DrawItem &it : items) {
                const MeshData *md = mesh(ram, it.node);
                if (!it.animPkt || (md && md->skinned)) continue;
                std::fprintf(stderr, "[rigid] pkt %x node %x tex %u tris %zu raio %.1f pos %.1f %.1f %.1f blend %d f30 %08x %08x\n", it.animPkt, it.node, unsigned(ram.u16(it.node + 0x50)),
                             md ? md->tris.size() / 3 : size_t(0), ram.f32(it.node + 8), it.m.m[3][0], it.m.m[3][1], it.m.m[3][2], int(it.blend), ram.u32(it.node + 0x30), ram.u32(it.node + 0x34));
            }
        }
        for (const DrawItem &it : items) {
            const MeshData *md = mesh(ram, it.node);
            if (!md || !md->skinned) continue;
            std::fprintf(stderr, "   m: [%.3f %.3f %.3f %.3f] [%.3f %.3f %.3f %.3f] [%.3f %.3f %.3f %.3f] [%.3f %.3f %.3f %.3f]%c", it.m.m[0][0], it.m.m[0][1], it.m.m[0][2], it.m.m[0][3], it.m.m[1][0], it.m.m[1][1], it.m.m[1][2], it.m.m[1][3], it.m.m[2][0], it.m.m[2][1], it.m.m[2][2], it.m.m[2][3], it.m.m[3][0], it.m.m[3][1], it.m.m[3][2], it.m.m[3][3], 10);
            std::fprintf(stderr, "skinned no=%06x tris=%zu pos=(%.0f %.0f %.0f) animPkt=%06x lod=%d pal=%06x\n", it.node, md->tris.size() / 3,
                         it.m.m[3][0], it.m.m[3][1], it.m.m[3][2], it.animPkt, it.lodIdx, palette(ram, it));
            {
                float u0 = 1e9f, u1 = -1e9f, v0 = 1e9f, v1 = -1e9f;
                for (const Vtx &v : md->tris) { u0 = std::min(u0, v.u); u1 = std::max(u1, v.u); v0 = std::min(v0, v.v); v1 = std::max(v1, v.v); }
                std::fprintf(stderr, "   uv u[%.2f %.2f] v[%.2f %.2f] abe=%d\n", u0, u1, v0, v1, int(md->abe));
                std::fprintf(stderr, "   lowAlpha=%d abe=%d (lowAlpha manda o objeto para o passe translucido, sem profundidade)\n", int(md->lowAlpha), int(md->abe));
                {   // histograma do alfa de vertice (a*4 efetivo) e media da cor: distingue camada opaca de camada de brilho/aditiva
                    unsigned h[8] = {0}; double sr = 0, sg = 0, sb = 0;
                    for (const Vtx &v : md->tris) { ++h[std::min(7, v.a * 4 / 32)]; sr += v.r; sg += v.g; sb += v.b; }
                    const double n = md->tris.empty() ? 1.0 : double(md->tris.size());
                    std::fprintf(stderr, "   alfa vtx (a*4 em oitavos de 256): %u %u %u %u %u %u %u %u | cor media rgb %.0f %.0f %.0f\n", h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], sr / n, sg / n, sb / n);
                }
                std::fprintf(stderr, "   indices: %u total, %u fora de pos; descartados: %u por ADC, %u degenerados; emitidos %zu; posicoes %u, usadas %u\n", md->totIdx, md->oobIdx, md->dropAdc, md->dropDegen, md->tris.size() / 3, md->posTot, md->posUsed);
            }
            const uint64_t t0v = objectTex0(ram, it.node);
            const gs::Tex0 t0 = gs::Tex0::decode(t0v);
            const uint32_t ptr = ram.u32(addr::texInfo + 16 * ram.u16(it.node + 0x50)) & 0x0FFFFFFF;
            std::fprintf(stderr, "   texId=%u tex0=%016llx cbp=%u csa=%u base=%u psm=%u ptr=%06x hasPal=%d\n", ram.u16(it.node + 0x50),
                         (unsigned long long)t0v, t0.cbp, t0.csa, ram.u32(addr::tempVramTexAddr) >> 6, ram.u8(ptr + 0x2B), ptr,
                         int(rtxPal.count(t0.cbp)));
        }
    }

    const MeshData *mesh(const Ram &ram, uint32_t node) {
        const uint32_t pp = ram.u32(node + 4) & 0x0FFFFFFF;
        auto it = cache_.find(pp);
        if (it != cache_.end()) {
            // revalida a assinatura (a RAM do nivel pode ter sido reaproveitada)
            const uint32_t tag = ram.u32(pp);
            const uint32_t qwc = tag & 0xFFFF, nwords = (qwc + 1) * 4 - 2;
            const uint32_t sig = tag ^ ram.u32(pp + 8) ^ (ram.u32(pp + 12) * 31u) ^ (ram.u32(pp + 4 * (nwords + 1)) * 17u);
            if (it->second->sig == sig) return it->second.get();
        }
        auto md = std::make_shared<MeshData>(decodeObject(ram, node));
        cache_[pp] = md;
        return md.get();
    }

    // Desenha com o raylib (deve estar dentro de BeginMode3D).
    void draw(const Ram &ram, bool wire = false) {
        // Teste de alfa: pixels transparentes (sombras, helices, grades) nao podem escrever profundidade nem cor.
        if (!alphaShaderTried_) {
            alphaShaderTried_ = true;
            static const char *fs =
                "#version 330\n"
                "in vec2 fragTexCoord; in vec4 fragColor;\n"
                "uniform sampler2D texture0; uniform vec4 colDiffuse; out vec4 finalColor;\n"
                "void main() { vec4 t = texture(texture0, fragTexCoord) * fragColor * colDiffuse; if (t.a < 0.06) discard; finalColor = t; }\n";
            alphaShader_ = LoadShaderFromMemory(nullptr, fs);
        }
        const bool useShader = !wire && alphaShader_.id > 0;
        if (useShader) BeginShaderMode(alphaShader_);
        rlDisableDepthTest();   // o ceu e desenhado primeiro, centrado no olho, sem profundidade
        drawList(ram, sky, wire);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
        static const bool onePass = std::getenv("WOTM_ONEPASS") != nullptr;   // diagnostico: tudo num passe so (sem separar translucidos)
        drawList(ram, items, wire, (wire || onePass) ? -1 : 0);
        if (!wire) drawShadows(ram);   // sobre os opacos, antes dos translucidos
        if (!wire && !onePass) {
            rlDrawRenderBatchActive();
            rlDisableDepthMask();   // translucidos testam profundidade mas nao a escrevem
            drawList(ram, items, wire, 1);
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
        }
        // particulas por ultimo (testam profundidade, nao escrevem): destroços interpolados (drawSprites), depois os demais tipos na ordem de particleDraw
        if (!wire) drawSprites(ram);
        if (!wire) fx.draw(ram, [&](uint32_t id, uint64_t t0) { return particleTexture(ram, id, t0); });
        rlDrawRenderBatchActive();
        rlDisableDepthTest();   // icones de botao por cima de tudo (z perto de 0: o teste de profundidade os esconde sob o chao)
        drawList(ram, overlay, wire, -1, true);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
        if (useShader) EndShaderMode();
    }

    // Translucido: textura com alfa parcial ou vertices de alfa baixo. Esses vao no segundo passe (tras para frente, sem escrever profundidade).
    bool translucent(const Ram &ram, const DrawItem &it) {
        const MeshData *md = mesh(ram, it.node);
        if (md && md->lowAlpha) return true;
        if (md && md->abe && it.blend) return true;   // mistura aditiva/escurecedora precisa do passe translucido
        for (uint32_t sl = 0; sl < 5; ++sl) {
            if (md && md->slotMask && !(md->slotMask & (1u << sl))) continue;
            const uint64_t k = texKey(ram, it.node, sl);
            if (!k) continue;
            textureId(ram, it.node, sl);
            const auto f = partial_.find(k);
            if (f != partial_.end() && f->second) return true;
        }
        return false;
    }

    // pass: -1 tudo; 0 so opacos/recortados; 1 so translucidos (ordenados do mais longe ao mais perto)
    // fadeByNode: multiplica o alfa pelo float em +0x4C do objeto (fade dos botoes: screenSetButtonAlpha grava `targetAlpha` ali)
    void drawList(const Ram &ram, const std::vector<DrawItem> &list, bool wire, int pass = -1, bool fadeByNode = false) {
        rlDisableBackfaceCulling();
        {   // diagnostico: WOTM_CMP=1 compara, por triangulo, as arestas de dois itens skinned com a mesma malha (ex.: jogador x inimigo)
            static const bool cmp = std::getenv("WOTM_CMP") != nullptr;
            static bool done = false;
            if (cmp && !done) {
                const DrawItem *A = nullptr, *B = nullptr;
                for (const DrawItem &it : list) {
                    const MeshData *md = mesh(ram, it.node);
                    if (!md || !md->skinned || md->tris.size() != 4428 || !it.animPkt) continue;
                    if (!A) A = &it; else if (!B && it.animPkt != A->animPkt) B = &it;
                }
                if (A && B) {
                    done = true;
                    const MeshData *md = mesh(ram, A->node);
                    const uint32_t pa = palette(ram, *A), pb = palette(ram, *B);
                    int slotBad[32] = {0}, slotAll[32] = {0}, nbad = 0, ntri = 0;
                    for (size_t k = 0; k + 2 < md->tris.size(); k += 3) {
                        float qa[3][3], qb[3][3];
                        for (int c = 0; c < 3; ++c) { skinVertex(ram, pa, M4{}, md->tris[k + c], qa[c]); skinVertex(ram, pb, M4{}, md->tris[k + c], qb[c]); }
                        auto mx = [](float q[3][3]) { float m = 0; for (int c = 0; c < 3; ++c) { const float *a = q[c], *b = q[(c + 1) % 3]; m = std::max(m, std::sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]))); } return m; };
                        const float ea = mx(qa), eb = mx(qb);
                        ++ntri;
                        auto area = [](float q[3][3]) { const float u[3] = {q[1][0]-q[0][0], q[1][1]-q[0][1], q[1][2]-q[0][2]}, w[3] = {q[2][0]-q[0][0], q[2][1]-q[0][1], q[2][2]-q[0][2]};
                            const float cx = u[1]*w[2]-u[2]*w[1], cy = u[2]*w[0]-u[0]*w[2], cz = u[0]*w[1]-u[1]*w[0]; return 0.5f * std::sqrt(cx*cx + cy*cy + cz*cz); };
                        const float aa = area(qa), ab = area(qb);
                        const bool bad = ab < 0.15f * aa && aa > 4.f;   // triangulo colapsado no B (area < 15%% da do A)
                        if (bad) ++nbad;
                        for (int c = 0; c < 3; ++c) {
                            const Vtx &v = md->tris[k + c];
                            const int sl[3] = {(v.cx & 0x7F) / 4, (v.cy & 0x7F) / 4, (v.cz & 0x7F) / 4};
                            for (int z = 0; z < 3; ++z) if (sl[z] < 32) { ++slotAll[sl[z]]; if (bad) ++slotBad[sl[z]]; }
                        }
                    }
                    std::fprintf(stderr, "[cmp] tris %d, colapsados no B (area < 15%% da do A): %d\n", ntri, nbad);
                    for (int z = 0; z < 31; ++z) std::fprintf(stderr, "[cmp] slot %2d: usos %4d, em rasgados %4d (%.0f%%)\n", z, slotAll[z], slotBad[z], slotAll[z] ? 100.0 * slotBad[z] / slotAll[z] : 0.0);
                }
            }
        }
        struct Ent { uint64_t key; uint32_t item; uint32_t slot; };
        std::vector<Ent> order;   // (chave de textura, indice do item, slot de textura)
        order.reserve(list.size());
        const bool far2near = pass == 1 && !wire;
        for (uint32_t i = 0; i < list.size(); ++i) {
            if (pass >= 0 && !wire && translucent(ram, list[i]) != (pass == 1)) continue;
            {   // diagnostico: WOTM_ONLYTEX=<id[,id..]> desenha so esses texId (bisseccao de defeitos)
                static const char *only = std::getenv("WOTM_ONLYTEX");
                if (only) {
                    const std::string s = std::string(",") + only + ",";
                    if (s.find("," + std::to_string(ram.u16(list[i].node + 0x50)) + ",") == std::string::npos) continue;
                }
            }
            {   // diagnostico: WOTM_ONLYNODE=<hex[,hex..]> desenha so esses nos (isola um personagem do resto da cena)
                static const char *onlyN = std::getenv("WOTM_ONLYNODE");
                if (onlyN) {
                    char b[16]; std::snprintf(b, sizeof b, ",%x,", list[i].node);
                    if ((std::string(",") + onlyN + ",").find(b) == std::string::npos) continue;
                }
            }
            const MeshData *mdi = mesh(ram, list[i].node);
            {   // diagnostico: WOTM_LISTITEMS=1 lista os itens de cada passe (no, texId, zBufferFudge, translucido, posicao, triangulos)
                static const bool li = std::getenv("WOTM_LISTITEMS") != nullptr;
                if (li && !wire)
                    std::fprintf(stderr, "[item] pass %d no %x tex %u fz %.4f tris %zu pos %.1f %.1f %.1f\n", pass, list[i].node, unsigned(ram.u16(list[i].node + 0x50)),
                                 ram.f32(list[i].node + 0x28), mdi ? mdi->tris.size() / 3 : size_t(0), list[i].m.m[3][0], list[i].m.m[3][1], list[i].m.m[3][2]);
            }
            const uint32_t smask = (mdi && mdi->slotMask) ? mdi->slotMask : 1u;
            for (uint32_t sl = 0; sl < 32; ++sl) {   // slots 0-4 e kSlotNoTex
            if (!(smask & (1u << sl))) continue;
            { static const int onlySl = std::getenv("WOTM_ONLYSLOT") ? std::atoi(std::getenv("WOTM_ONLYSLOT")) : -1; if (onlySl >= 0 && int(sl) != onlySl) continue; }   // diagnostico: so este slot de textura
            uint64_t key = wire ? 0 : texKey(ram, list[i].node, texRemap(sl));
            if (far2near) {
                const M4 &m = list[i].m;
                const float dx = m.m[3][0] - eye_[0], dy = m.m[3][1] - eye_[1], dz = m.m[3][2] - eye_[2];
                const float d2 = dx * dx + dy * dy + dz * dz;
                uint32_t bits;
                std::memcpy(&bits, &d2, 4);
                key = uint64_t(~bits) + 1;   // maior distancia primeiro; nunca 0 (0 = "sem textura" abaixo)
            }
            order.push_back({key, i, sl});
            }
        }
        std::stable_sort(order.begin(), order.end(), [](const Ent &a, const Ent &b) { return a.key < b.key; });

        const unsigned white = rlGetTextureIdDefault();
        uint64_t cur = ~0ull;
        bool open = false;
        int curBlend = 0;   // DrawItem::blend ativo no GL (o lote anterior ja foi descarregado quando isto muda)
        auto setBlend = [&](int b) {
            if (b == curBlend) return;
            curBlend = b;
            if (b == 1) { rlSetBlendFactors(RL_SRC_ALPHA, RL_ONE, RL_FUNC_ADD); rlSetBlendMode(RL_BLEND_CUSTOM); }          // Cs*As + Cd
            else if (b == 2) { rlSetBlendFactors(RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD); rlSetBlendMode(RL_BLEND_CUSTOM); }   // Cd*(1-As)
            else rlSetBlendMode(RL_BLEND_ALPHA);
        };
        for (const auto &o : order) {
            const DrawItem &it = list[o.item];
            const MeshData *md = mesh(ram, it.node);
            {   // diagnostico: WOTM_LISTSKIN=1 lista os itens skinned desenhados (no, LOD, animPkt, posicao do objeto)
                static const bool ls = std::getenv("WOTM_LISTSKIN") != nullptr;
                if (ls && md && md->skinned && !wire)
                    std::fprintf(stderr, "[skin] node %x lod %d animPkt %x pos %.1f %.1f %.1f tris %zu slot %u\n", it.node, it.lodIdx, it.animPkt, it.m.m[3][0], it.m.m[3][1], it.m.m[3][2], md->tris.size() / 3, o.slot);
                if (ls && md && md->skinned && !wire) {
                    uint64_t h = 1469598103934665603ull;   // FNV-1a da geometria decodificada (sem cor): compara meshes de monstros diferentes
                    for (const Vtx &v : md->tris) {
                        const float f[6] = {v.x, v.y, v.z, v.u, v.v, v.w};
                        const uint8_t *q = reinterpret_cast<const uint8_t *>(f);
                        for (int b = 0; b < 24; ++b) h = (h ^ q[b]) * 1099511628211ull;
                        const uint8_t c[4] = {v.cx, v.cy, v.cz, v.slot};
                        for (int b = 0; b < 4; ++b) h = (h ^ c[b]) * 1099511628211ull;
                    }
                    int perSlot[8] = {0};
                    for (const Vtx &v : md->tris) ++perSlot[v.slot & 7];
                    std::fprintf(stderr, "   slotMask %x verts/slot %d %d %d %d %d texIds %u %u %u\n", md->slotMask, perSlot[0], perSlot[1], perSlot[2], perSlot[3], perSlot[4],
                                 unsigned(ram.u16(it.node + 0x50)), unsigned(ram.u16(it.node + 0x52)), unsigned(ram.u16(it.node + 0x54)));
                    std::fprintf(stderr, "   geomhash %016llx nverts %zu\n", (unsigned long long)h, md->tris.size());
                    // arestas apos o skinning (unidades do mundo): malha rasgada tem cauda longa; compara modelos de peso e paletas
                    const uint32_t palD = palette(ram, it);
                    std::vector<float> el;
                    for (size_t k = 0; palD && k + 2 < md->tris.size(); k += 3) {
                        float q[3][3];
                        for (int c = 0; c < 3; ++c) skinVertex(ram, palD, it.m, md->tris[k + c], q[c]);
                        float mx = 0;
                        for (int c = 0; c < 3; ++c) {
                            const float *a = q[c], *b = q[(c + 1) % 3];
                            mx = std::max(mx, std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2])));
                        }
                        el.push_back(mx);
                    }
                    if (!el.empty()) {
                        std::sort(el.begin(), el.end());
                        std::fprintf(stderr, "   maxedge med %.2f p90 %.2f p99 %.2f max %.2f\n", el[el.size() / 2], el[el.size() * 9 / 10], el[el.size() * 99 / 100], el.back());
                    }
                }
                if (ls && md && md->skinned && !wire) { for (int r = 0; r < 3; ++r) std::fprintf(stderr, "   m[%d] %.3f %.3f %.3f %.3f\n", r, it.m.m[r][0], it.m.m[r][1], it.m.m[r][2], it.m.m[r][3]); }
            }
            if (!md || !md->valid || md->tris.empty()) continue;
            const uint32_t slot = o.slot;
            const bool multi = md->slotMask & (md->slotMask - 1);   // mais de um slot usado
            if (o.key != cur || !open || far2near) {
                if (open) { rlEnd(); rlDrawRenderBatchActive(); }   // um lote por textura (evita o alinhamento automatico do rlgl)
                if (!wire && md->abe) setBlend(it.blend); else if (!wire) setBlend(0);
                cur = o.key;
                const unsigned id = wire ? 0 : textureId(ram, it.node, texRemap(slot));
                rlBegin(wire ? RL_LINES : RL_TRIANGLES);   // rlBegin com outro modo reseta a textura do lote: setar depois
                static const bool idc = std::getenv("WOTM_IDCOLOR") != nullptr;   // diagnostico: cor = texId do objeto (leia o pixel para achar o id)
                rlSetTexture((id && !idc) ? id : white);
                open = true;
            }
            static const bool idcMode = std::getenv("WOTM_IDCOLOR") != nullptr;   // uma vez: getenv por vertice e lento no Windows
            const unsigned tid = (wire || !cur) ? 0 : textureId(ram, it.node, texRemap(slot));
            const auto &t = md->tris;
            static const bool forceOpaqueEnv = std::getenv("WOTM_OPAQUE") != nullptr;   // diagnostico: alfa de vertice ignorado em skinned
            const bool forceOpaque = forceOpaqueEnv && md->skinned;
            const float fade = fadeByNode ? std::min(1.f, std::max(0.f, ram.f32(it.node + 0x4C))) : 1.f;
            static const bool noVcol = std::getenv("WOTM_NOVCOL") != nullptr;
            static const bool noSkin = std::getenv("WOTM_NOSKIN") != nullptr;   // diagnostico: pose de referencia, sem skinning
            static const uint32_t palOverride = std::getenv("WOTM_PALBLOCK") ? uint32_t(std::strtoul(std::getenv("WOTM_PALBLOCK"), nullptr, 16)) : 0;   // diagnostico
            const uint32_t pal = (md->skinned && !noSkin) ? (palOverride ? palOverride : palette(ram, it)) : 0;
            std::vector<M4> pmTmp;   // paleta lida da RAM (item sem copia: viewer, ou WOTM_PALBLOCK)
            const M4 *pm = nullptr;
            if (md->skinned && !noSkin) {
                if (!palOverride && it.pal && it.pal->size() >= 31) pm = it.pal->data();
                else if (pal) { for (uint32_t kk = 0; kk < 32; ++kk) pmTmp.push_back(readM4(ram, pal + 0x10 + 0x40 * kk)); pm = pmTmp.data(); }
            }
            M4 skinM = it.m;
            {   // diagnostico: WOTM_SKINM1=<animPkt hex> usa a matriz do objeto desse item para todos os skinned (isola matriz x paleta)
                static const uint32_t refPkt = std::getenv("WOTM_SKINM1") ? uint32_t(std::strtoul(std::getenv("WOTM_SKINM1"), nullptr, 16)) : 0;
                if (refPkt && md->skinned)
                    for (const DrawItem &r : list) if (r.animPkt == refPkt) { skinM = r.m; break; }
            }   // diagnostico: WOTM_SKINPOS=x,y,z poe todos os itens skinned nessa posicao (testa dependencia de posicao)
            { static float sp[3]; static const int have = std::getenv("WOTM_SKINPOS") ? std::sscanf(std::getenv("WOTM_SKINPOS"), "%f,%f,%f", sp, sp + 1, sp + 2) : 0; if (have == 3 && md->skinned) for (int c = 0; c < 3; ++c) skinM.m[3][c] = sp[c]; }
            // zBufferFudge (objeto +0x28): multiplicador da profundidade (1,0 = nada). Escalar o vertice ao longo do raio a partir do
            // olho muda so a profundidade, nao a posicao na tela: resolve o z-fighting de camadas coplanares.
            const float fz = ram.f32(it.node + 0x28);
            const bool fudge = !wire && eyeOk_ && fz > 0.5f && fz < 2.f && fz != 1.f;
            static const bool fzOld = std::getenv("WOTM_FUDGEOLD") != nullptr;   // diagnostico: direcao antiga (>1 = mais longe)
            const float fzScale = fzOld ? fz : 1.f / fz;
            {   // diagnostico: WOTM_SKINBOX=1 imprime a caixa (mundo) dos vertices skinned de cada no, por osso x
                static const bool sb = std::getenv("WOTM_SKINBOX") != nullptr;
                if (sb && pm && md->skinned && !wire) {
                    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f}, rlo[3] = {1e30f, 1e30f, 1e30f}, rhi[3] = {-1e30f, -1e30f, -1e30f};
                    for (const Vtx &v : t) {
                        float p[3];
                        skinVertexPM(pm, skinM, v, p);
                        const float r[3] = {v.x, v.y, v.z};
                        for (int c = 0; c < 3; ++c) { lo[c] = std::min(lo[c], p[c]); hi[c] = std::max(hi[c], p[c]); rlo[c] = std::min(rlo[c], r[c]); rhi[c] = std::max(rhi[c], r[c]); }
                    }
                    for (int s = 0; s < 31; ++s) {   // maior |elemento| de cada matriz da paleta (detecta lixo/NaN)
                        float mx = 0.f; bool bad = false;
                        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) { const float e = pm[s].m[r][c]; if (!(e == e) || std::fabs(e) > 1e6f) bad = true; mx = std::max(mx, std::fabs(e)); }
                        if (bad || mx > 500.f) std::fprintf(stderr, "[skinbox]   osso %d: max|e| %.3g%s\n", s, double(mx), bad ? " (LIXO)" : "");
                    }
                    std::fprintf(stderr, "[skinbox] node %x mundo x[%.1f %.1f] y[%.1f %.1f] z[%.1f %.1f] | cru x[%.1f %.1f] y[%.1f %.1f] z[%.1f %.1f]\n", it.node, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2], rlo[0], rhi[0], rlo[1], rhi[1], rlo[2], rhi[2]);
                }
            }
            for (size_t k = 0; k + 2 < t.size(); k += 3) {
                if (multi && t[k].slot != slot) continue;   // so as secoes deste slot
                { static const int onlySub = std::getenv("WOTM_SUBPKT") ? std::atoi(std::getenv("WOTM_SUBPKT")) : -1; if (onlySub >= 0 && md->skinned && t[k].sub != onlySub) continue; }
                float p[3][3];
                for (int c = 0; c < 3; ++c) {
                    if (pm) skinVertexPM(pm, skinM, t[k + c], p[c]);
                    else xform(it.m, t[k + c].x, t[k + c].y, t[k + c].z, p[c]);
                    if (fudge) for (int a = 0; a < 3; ++a) p[c][a] = eye_[a] + (p[c][a] - eye_[a]) * fzScale;
                }
                for (int c = 0; c < 3; ++c) {
                    const int cs[2] = {c, (c + 1) % 3};
                    for (int e = 0; e < (wire ? 2 : 1); ++e) {
                        const Vtx &v = t[k + cs[e]];
                        if (idcMode) { const uint32_t tx = objectTexId(ram, it.node, slot); rlColor4ub(uint8_t(tx & 255), uint8_t(tx >> 8), 128, 255); }
                        else if (v.isAdc) rlColor4ub(255, 0, 0, 255);   // WOTM_SHOWADC
                        else if (noVcol && md->skinned) rlColor4ub(255, 255, 255, 255);   // diagnostico
                        else rlColor4ub(uint8_t(std::min(255, v.r * 2)), uint8_t(std::min(255, v.g * 2)), uint8_t(std::min(255, v.b * 2)), forceOpaque ? uint8_t(255) : uint8_t(std::min(255.f, v.a * 4 * fade)));
                        if (tid) { static const char *un = std::getenv("WOTM_UVNEG"); const float su = (un && std::strchr(un, 'u')) ? -1.f : 1.f, sv = (un && std::strchr(un, 'v')) ? -1.f : 1.f; rlTexCoord2f(su * v.u, sv * v.v); }   // diagnostico: WOTM_UVNEG=u|v|uv inverte o sinal do UV
                        rlVertex3f(p[cs[e]][0], p[cs[e]][1], p[cs[e]][2]);
                    }
                }
            }
        }
        if (open) { rlEnd(); rlDrawRenderBatchActive(); }   // descarrega o ultimo lote ANTES de religar o culling (o estado GL vale no flush)
        setBlend(0);
        rlSetTexture(0);
        rlEnableBackfaceCulling();
    }

    // Texto do jogo (menu de pausa, HUD): o jogo monta o quadro em `fontPacket` (um pacote GIF para o GS: cabecalho A+D com o TEX0 do atlas,
    // depois corridas de sprites [UV, XYZ3, UV, XYZ2] x n separadas por [PRIM, RGBAQ] que trocam a cor) e o despacha em fontDmaFontData, que
    // zera o contador (D_006F8D30 volta a D_006F8D44 = tamanho do cabecalho) e deixa o pacote na RAM. Contador != cabecalho = o jogo esta
    // montando o quadro: mantem o texto anterior. Num quadro sem texto o terminador (EOP) cai logo depois do cabecalho e o texto some.
    void updateText(const Ram &ram) {
        if (ram.u32(0x006F8D30) != ram.u32(0x006F8D44)) return;
        std::vector<TextQuad> q;
        uint32_t p = addr::fontPacket;
        uint64_t tex0 = 0;
        uint8_t col[4] = {0x80, 0x80, 0x80, 0x80};
        auto u64 = [&](uint32_t a) { return uint64_t(ram.u32(a)) | (uint64_t(ram.u32(a + 4)) << 32); };
        for (int guard = 0; guard < 4096 && ram.ok(p, 16); ++guard) {
            const uint64_t tag = u64(p), regs = u64(p + 8);
            const uint32_t nloop = uint32_t(tag & 0x7FFF), flg = uint32_t(tag >> 58) & 3, nreg = (uint32_t(tag >> 60) & 15) ? (uint32_t(tag >> 60) & 15) : 16;
            p += 16;
            if (flg == 0 && regs == 0xE) {   // A+D: endereco do registrador em hi, dado em lo
                for (uint32_t i = 0; i < nloop && ram.ok(p, 16); ++i, p += 16)
                    if ((u64(p + 8) & 0xFF) == 0x06) tex0 = u64(p);   // TEX0_1
            } else if (flg == 1 && nreg == 2 && (regs & 0xFF) == 0x10) {   // [PRIM, RGBAQ]: cor da proxima corrida
                for (uint32_t i = 0; i < nloop && ram.ok(p, 16); ++i, p += 16) {
                    const uint32_t c = ram.u32(p + 8);
                    col[0] = uint8_t(c); col[1] = uint8_t(c >> 8); col[2] = uint8_t(c >> 16); col[3] = uint8_t(c >> 24);
                }
            } else if (flg == 1 && nreg == 4 && (regs & 0xFFFF) == 0x53D3) {   // sprites: UV, XYZ3, UV, XYZ2 (12.4, centro do GS em 2048)
                for (uint32_t i = 0; i < nloop && ram.ok(p, 32); ++i, p += 32) {
                    constexpr float kOx = 2048.f - 320.f, kOy = 2048.f - 112.f;   // viewport cheio: 640 x 224 (campo), centro 2048
                    TextQuad t{};
                    t.u0 = float(ram.u16(p) & 0x3FFF) / 16.f; t.v0 = float(ram.u16(p + 2) & 0x3FFF) / 16.f;
                    t.x0 = float(ram.u16(p + 8)) / 16.f - kOx; t.y0 = float(ram.u16(p + 10)) / 16.f - kOy;
                    t.u1 = float(ram.u16(p + 16) & 0x3FFF) / 16.f; t.v1 = float(ram.u16(p + 18) & 0x3FFF) / 16.f;
                    t.x1 = float(ram.u16(p + 24)) / 16.f - kOx; t.y1 = float(ram.u16(p + 26)) / 16.f - kOy;
                    t.r = col[0]; t.g = col[1]; t.b = col[2]; t.a = col[3];
                    t.tex0 = tex0;
                    q.push_back(t);
                }
            } else {
                break;   // terminador (EOP, A+D de FBA) ou lixo
            }
            if (tag & 0x8000) break;   // EOP
        }
        text.swap(q);
    }

    // Desenha o texto em coordenadas da tela do jogo (640 x 224) escaladas para `rw` x `rh` (fora do modo 3D, por cima da cena).
    void drawText(const Ram &ram, float rw, float rh) {
        if (text.empty()) return;
        if (!textShaderTried_) {
            // O GS faz textura * cor / 0x80 (0x80 = 1,0) e satura em 1,0: cores acima de 0x80 clareiam o glifo. O raylib so multiplica por
            // <= 1; o shader dobra o RGB (a cor do vertice entra como r/255, ou seja r/0x80 = 2 * r/255).
            textShaderTried_ = true;
            static const char *fs =
                "#version 330\n"
                "in vec2 fragTexCoord; in vec4 fragColor;\n"
                "uniform sampler2D texture0; uniform vec4 colDiffuse; out vec4 finalColor;\n"
                "void main() { vec4 t = texture(texture0, fragTexCoord) * fragColor * colDiffuse; finalColor = vec4(min(t.rgb * 2.0, 1.0), t.a); }\n";
            textShader_ = LoadShaderFromMemory(nullptr, fs);
        }
        const float sx = rw / 640.f, sy = rh / 224.f;
        if (textShader_.id > 0) BeginShaderMode(textShader_);
        for (const TextQuad &t : text) {
            const Texture2D tex = fontTexture(ram, t.tex0);
            if (!tex.id) continue;
            const bool gain = textShader_.id > 0;   // sem o shader a cor fica em <= 1,0 (dobra aqui)
            const int k = gain ? 1 : 2;
            const Color tint{uint8_t(std::min(255, t.r * k)), uint8_t(std::min(255, t.g * k)), uint8_t(std::min(255, t.b * k)), uint8_t(std::min(255, t.a * 2))};
            DrawTexturePro(tex, Rectangle{t.u0, t.v0, t.u1 - t.u0, t.v1 - t.v0},
                           Rectangle{t.x0 * sx, t.y0 * sy, (t.x1 - t.x0) * sx, (t.y1 - t.y0) * sy}, Vector2{0.f, 0.f}, 0.f, tint);
        }
        if (textShader_.id > 0) EndShaderMode();
    }

private:
    std::unordered_map<uint64_t, Texture2D> fontTex_;   // atlas de fonte por TEX0 (id 0 = nao decodificado)
    Shader textShader_{};
    bool textShaderTried_ = false;

    // Atlas da fonte: PSMT8H 512x512 do .RTX da fase (entrada com DBP == TBP0) + paleta CT32 em CBP, como as telas do menu em decodeTex.
    Texture2D fontTexture(const Ram &, uint64_t tex0) {
        const auto it = fontTex_.find(tex0);
        if (it != fontTex_.end()) return it->second;
        Texture2D t{};
        const gs::Tex0 t0 = gs::Tex0::decode(tex0);
        const auto pal0 = rtxPal.find(t0.cbp);
        const auto im = rtxImg.find((t0.psm << 16) | t0.tbp0);
        std::vector<uint32_t> px;
        uint32_t pal[256] = {};
        if (t0.psm == gs::T8H && pal0 != rtxPal.end() && im != rtxImg.end()) {
            fillPal(pal, pal0->second, true);
            const uint32_t w = 1u << t0.tw, h = 1u << t0.th;
            if (gs::decodeUpload(im->second.data(), im->second.size(), w, h, gs::T8, t0.csa, pal, px)) {
                Image img{};
                img.data = px.data(); img.width = int(w); img.height = int(h); img.mipmaps = 1;
                img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                t = LoadTextureFromImage(img);
                if (t.id) { GenTextureMipmaps(&t); SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR); SetTextureWrap(t, TEXTURE_WRAP_CLAMP); }
            }
        }
        fontTex_[tex0] = t;
        return t;
    }

    std::unordered_map<uint32_t, std::shared_ptr<MeshData>> cache_;
    std::unordered_map<uint64_t, Texture2D> tex_;
    std::vector<uint32_t> seen_;   // nos ja visitados neste quadro (ciclos / DAG)
    int lod_ = -1;
    uint32_t destrHist_[8] = {};   // diagnostico (WOTM_DESTRSTAT): destrutiveis por estado atual
    Shader alphaShader_{};
    bool alphaShaderTried_ = false;
    std::unordered_map<uint64_t, bool> partial_;   // textura (texKey) com alfa semitransparente
    std::vector<Texture2D> shadowTex_;   // silhuetas das sombras (uma por bloqueador desenhado)
    static constexpr int kShadowRes = 128;
    uint32_t resSig_ = 0;   // assinatura dos recursos em fileStatus ja carregados (loadResources)
    GameCamera cam_;   // camera do ultimo collect/blend (eixos para os sprites)
    bool eyeOk_ = false;
    float eye_[3] = {0, 0, 0};
    uint32_t atMat_ = 0;   // animOutput.atMat do CHAR_INSTANCE que contem o no atual
    uint32_t animPkt_ = 0; // animPktPtr do CHAR_INSTANCE que contem o no atual
    uint32_t animVal_ = 0; // animOutput.val (floats dos canais de animacao) do CHAR_INSTANCE que contem o no atual
    int lodIdx_ = 0;       // LOD escolhido mais recentemente na descida
    uint8_t blend_ = 0;    // DrawItem::blend herdado do GROUP mais proximo com ctx de ALPHA_1

    // O ctx de um GROUP (+4) e um pacote de estado do GS: tag A+D com ALPHA_1 (data em +0x20, endereco 0x42 em +0x28), TEST_1 e ZBUF_1.
    // Traduz o ALPHA_1 (A,B,C,D = bits 0-1, 2-3, 4-5, 6-7) para DrawItem::blend; -1 = ctx sem ALPHA_1 (herda o do pai).
    static int groupBlend(const Ram &ram, uint32_t ctx) {
        if (!ctx || !ram.ok(ctx, 0x30) || ram.u32(ctx + 0x14) != 0x10000000 || ram.u32(ctx + 0x18) != 0xE || ram.u32(ctx + 0x28) != 0x42) return -1;
        switch (ram.u32(ctx + 0x20) & 0xFF) {
        case 0x44: return 0;   // (Cs-Cd)*As+Cd
        case 0x48: return 1;   // (Cs-0)*As+Cd: brilho aditivo (Kineticlops)
        case 0x42: return 2;   // (0-Cd)*As+Cd: escurece o fundo
        default: return 0;
        }
    }

    // Paleta de matrizes de um objeto skinned: animPktPtr[lod] e uma cadeia DMA (tag ref 0x3000007d) cujo endereco aponta o
    // bloco de 125 qwords (31 matrizes de 4 qwords a partir de +0x10). Devolve 0 se nao achar.
    static uint32_t palette(const Ram &ram, const DrawItem &it) {
        if (!it.animPkt || it.lodIdx < 0 || it.lodIdx > 15) return 0;
        const uint32_t chain = ram.u32(it.animPkt + 4 * uint32_t(it.lodIdx)) & 0x0FFFFFFF;
        if (!ram.ok(chain, 8) || ((ram.u32(chain) >> 28) & 7) != 3) return 0;
        const uint32_t block = ram.u32(chain + 4) & 0x0FFFFFFF;
        return ram.ok(block, 0x10 + 0x40 * 32) ? block : 0;
    }

    // Skinning, como o microcodigo do VU1 (desmontado com tools/vu1dis.py; laco em 0x3C8): tres ossos por vertice.
    // Os codigos de osso (8 bits baixos de x, y, z; & 0x7F = endereco de qword da matriz na paleta) escolhem 3 matrizes e os pesos
    // saem do float w: w1 = w, w2 = frac(w * 2048) (mantissa baixa), w3 = 1 - w1 - w2. pos = w1*Mx*p + w2*My*p + w3*Mz*p.
    // Slot da paleta = (codigo & 0x7F) / 4.
    static void skinVertexPM(const M4 *pm, const M4 &m, const Vtx &v, float out[3]) {
        static const bool old2 = std::getenv("WOTM_SKIN2BONE") != nullptr;
        float w1 = v.w, w2 = 0.f, w3 = 0.f;
        if (old2) { w1 = std::min(1.f, std::max(0.f, v.w)); w2 = 1.f - w1; }
        else { const float s = v.w * 2048.f; w2 = s - std::trunc(s); w3 = 1.f - w1 - w2; }
        const uint8_t codes[3] = {v.cx, v.cy, v.cz};
        const float ws[3] = {w1, w2, w3};
        float q[3] = {0, 0, 0};
        for (int k = 0; k < 3; ++k) {
            if (ws[k] == 0.f) continue;
            float a[3];
            xform(pm[(codes[k] & 0x7F) / 4], v.x, v.y, v.z, a);
            for (int c = 0; c < 3; ++c) q[c] += a[c] * ws[k];
        }
        xform(m, q[0], q[1], q[2], out);
    }

    static void skinVertex(const Ram &ram, uint32_t pal, const M4 &m, const Vtx &v, float out[3]) {
        static const bool old2 = std::getenv("WOTM_SKIN2BONE") != nullptr;   // diagnostico: modelo antigo (2 ossos, w clampado)
        float w1 = v.w, w2 = 0.f, w3 = 0.f;
        if (old2) { w1 = std::min(1.f, std::max(0.f, v.w)); w2 = 1.f - w1; }
        else {
            const float s = v.w * 2048.f;
            w2 = s - std::trunc(s);
            w3 = 1.f - w1 - w2;
        }
        const uint8_t codes[3] = {v.cx, v.cy, v.cz};
        const float ws[3] = {w1, w2, w3};
        float q[3] = {0, 0, 0};
        for (int k = 0; k < 3; ++k) {
            if (ws[k] == 0.f) continue;
            float a[3];
            xform(readM4(ram, pal + 0x10 + 0x40 * ((codes[k] & 0x7F) / 4)), v.x, v.y, v.z, a);
            for (int c = 0; c < 3; ++c) q[c] += a[c] * ws[k];
        }
        xform(m, q[0], q[1], q[2], out);
    }

    // ---- CONTROL (hierCtrlNode, 0x2048F0): predios com versao "transparente" quando bloqueiam a visao do monstro ----
    // No: +4 child1, +8 child2, +0xC numVolumes, +0xE volType, +0x10 esfera (centro, raio em +0x1C), +0x20 volumes BSP (ponteiros).
    // volType 0: se o olho NAO esta no ar (folha != 0x200) em algum volume -> child2 (camera dentro do predio). volType != 0: para cada
    // ponto de visada (g_cameraLosPoints: direcao unitaria da camera ate o monstro + distancia em w) o segmento olho->ponto nao pode
    // atravessar solido (folha != 0x200) de nenhum volume; se atravessar -> child2 (que pode ser nulo = o predio some).
    static constexpr uint32_t kLosPoints = 0x0050F020, kLosPerView = 0x0050F0C0;

    static void toLocal(const M4 &m, const float w[3], float out[3]) {
        for (int c = 0; c < 3; ++c) {
            const float *r = m.m[c];
            const float n2 = r[0] * r[0] + r[1] * r[1] + r[2] * r[2];
            out[c] = n2 > 1e-9f ? ((w[0] - m.m[3][0]) * r[0] + (w[1] - m.m[3][1]) * r[1] + (w[2] - m.m[3][2]) * r[2]) / n2 : 0.f;
        }
    }

    // Folha onde o ponto cai (0x200 = ar), igual a bspPointTest.
    static int bspPoint(const Ram &ram, uint32_t b, const float p[3]) {
        int ret = 0x200;
        for (int guard = 0; b && ram.ok(b, 32) && guard < 64; ++guard) {
            const float d = ram.f32(b + 0x10) * p[0] + ram.f32(b + 0x14) * p[1] + ram.f32(b + 0x18) * p[2] - ram.f32(b + 0x1C);
            const bool neg = d < 0.f;
            ret = neg ? ram.u16(b + 0xE) : ram.u16(b + 0xC);
            b = neg ? ram.u32(b + 4) : ram.u32(b);
        }
        return ret;
    }

    // Verdadeiro se o segmento a->c atravessa alguma folha que nao seja ar.
    static bool bspSegHit(const Ram &ram, uint32_t b, const float a[3], const float c[3], int depth = 0) {
        if (depth > 64) return false;
        if (!b || !ram.ok(b, 32)) return false;
        const float n[3] = {ram.f32(b + 0x10), ram.f32(b + 0x14), ram.f32(b + 0x18)}, dd = ram.f32(b + 0x1C);
        const float d0 = n[0] * a[0] + n[1] * a[1] + n[2] * a[2] - dd, d1 = n[0] * c[0] + n[1] * c[1] + n[2] * c[2] - dd;
        const uint32_t front = ram.u32(b), back = ram.u32(b + 4);
        auto side = [&](bool neg, const float p[3], const float q[3]) -> bool {
            const uint32_t kid = neg ? back : front;
            if (kid) return bspSegHit(ram, kid, p, q, depth + 1);
            return (neg ? ram.u16(b + 0xE) : ram.u16(b + 0xC)) != 0x200;   // folha
        };
        if (d0 >= 0.f && d1 >= 0.f) return side(false, a, c);
        if (d0 < 0.f && d1 < 0.f) return side(true, a, c);
        const float t = d0 / (d0 - d1);
        const float mid[3] = {a[0] + (c[0] - a[0]) * t, a[1] + (c[1] - a[1]) * t, a[2] + (c[2] - a[2]) * t};
        return d0 >= 0.f ? (side(false, a, mid) || side(true, mid, c)) : (side(true, a, mid) || side(false, mid, c));
    }

    uint32_t ctrlChild(const Ram &ram, uint32_t node, const M4 &m) const {
        static const bool off = std::getenv("WOTM_NOCTRL") != nullptr;   // diagnostico: sempre child1 (comportamento antigo)
        const uint32_t c1 = ram.u32(node + 4), c2 = ram.u32(node + 8);
        if (off || !eyeOk_) return c1;
        const uint32_t nvol = std::min<uint32_t>(ram.u8(node + 0xC), 16), vtype = ram.u8(node + 0xE);
        if (!nvol || !ram.ok(node, 0x20 + 4 * nvol)) return c1;
        float eyeL[3];
        toLocal(m, eye_, eyeL);
        bool visible = true;
        if (vtype == 0) {
            for (uint32_t i = 0; i < nvol && visible; ++i)
                if (bspPoint(ram, ram.u32(node + 0x20 + 4 * i), eyeL) != 0x200) visible = false;
        } else {
            const int nlos = std::min<int>(int(ram.u32(kLosPerView)), 2);   // vista 0
            for (int k = 0; k < nlos && visible; ++k) {
                const uint32_t pa = kLosPoints + 0x20 * 0 + 16 * k;
                const float len = ram.f32(pa + 12);
                if (!(len > 0.f) || len > 1e6f) continue;
                const float pw[3] = {eye_[0] + ram.f32(pa) * len, eye_[1] + ram.f32(pa + 4) * len, eye_[2] + ram.f32(pa + 8) * len};
                float pl[3];
                toLocal(m, pw, pl);
                for (uint32_t i = 0; i < nvol && visible; ++i)
                    if (bspSegHit(ram, ram.u32(node + 0x20 + 4 * i), eyeL, pl)) visible = false;
            }
        }
        return visible ? c1 : c2;
    }

    bool already(uint32_t n) {
        // lista pequena e ordenada por insercao; o jogo tem ate alguns milhares de nos
        for (uint32_t s : seen_) if (s == n) return true;
        seen_.push_back(n);
        return false;
    }

    void kids(const Ram &ram, uint32_t arr, uint32_t n, const M4 &m, int depth) {
        if (n > 4096) n = 4096;
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t c = ram.u32(arr + 4 * i);
            if (ram.ok(c)) walk(ram, c, m, depth + 1);
        }
    }

    void walk(const Ram &ram, uint32_t node, const M4 &m, int depth) {
        if (depth > 64 || visited > 200000 || !ram.ok(node)) return;
        // O mesmo no pode aparecer com matrizes diferentes (instancias): so corta ciclos profundos
        if (depth > 24 && already(node)) return;
        ++visited;
        const uint32_t op = ram.u32(node) & 0x3F;
        ++byOp[op];
        switch (op) {
        case 0: items.push_back({m, node, animPkt_, lodIdx_, nullptr, blend_}); break;                          // OBJECT
        case 1: {                                                                           // GROUP (ctx em +4 = estado do GS: ALPHA_1 dos filhos)
            const uint8_t saved = blend_;
            const int b = groupBlend(ram, ram.u32(node + 4) & 0x0FFFFFFF);
            if (b >= 0) blend_ = uint8_t(b);
            kids(ram, node + 0x20, ram.u16(node + 8), m, depth);
            blend_ = saved;
            break;
        }
        case 2: {                                                                           // LOD
            const uint32_t n = ram.u32(node + 4);
            if (n > 0 && n < 16) {
                int pick = -1;
                if (lod_ >= 0) pick = int(std::min<uint32_t>(uint32_t(lod_), n - 1));
                else {
                    // hierLod: o MAIOR indice cujo switchOutDis (distancia ao quadrado) > distancia ao centro do LOD; nenhum = nao desenha
                    float d2 = 0.f;
                    if (eyeOk_) {
                        float c[3];
                        xform(m, ram.f32(node + 0x10), ram.f32(node + 0x14), ram.f32(node + 0x18), c);
                        d2 = (c[0] - eye_[0]) * (c[0] - eye_[0]) + (c[1] - eye_[1]) * (c[1] - eye_[1]) + (c[2] - eye_[2]) * (c[2] - eye_[2]);
                    }
                    for (int k = int(n) - 1; k >= 0; --k)
                        if (d2 < ram.f32(node + 0x20 + 16 * k + 4)) { pick = k; break; }
                }
                if (pick < 0) break;
                const uint32_t i = uint32_t(pick);
                const uint32_t c = ram.u32(node + 0x20 + 16 * i + 8);
                const int savedLod = lodIdx_;
                lodIdx_ = int(i);
                if (ram.ok(c)) walk(ram, c, m, depth + 1);
                lodIdx_ = savedLod;
            }
            break;
        }
        case 3:                                                                             // TRANSLATE
            kids(ram, node + 0x1C, ram.u32(node + 8),
                 mul(translateM(ram.f32(node + 0x10), ram.f32(node + 0x14), ram.f32(node + 0x18)), m), depth);
            break;
        case 4: kids(ram, node + 0x50, ram.u32(node + 8), mul(readM4(ram, node + 0x10), m), depth); break;   // ROTATE
        case 30:                                                                            // SCALE
            kids(ram, node + 0x2C, ram.u32(node + 8),
                 mul(mul(scaleM(ram.f32(node + 0x20), ram.f32(node + 0x24), ram.f32(node + 0x28)),
                         translateM(ram.f32(node + 0x10), ram.f32(node + 0x14), ram.f32(node + 0x18))), m), depth);
            break;
        case 6: {                                                                           // SWITCH
            const uint32_t which = ram.u8(node + 8), n = ram.u8(node + 0xB);
            if (which < n) { const uint32_t c = ram.u32(node + 0xC + 4 * which); if (ram.ok(c)) walk(ram, c, m, depth + 1); }
            break;
        }
        case 8: {   // CONTROL: child1 (predio solido) ou child2 (versao "transparente"/ausente) conforme o volume bloqueia a visao do monstro
            const uint32_t c = ctrlChild(ram, node, m);
            if (c && ram.ok(c)) walk(ram, c, m, depth + 1);
            break;
        }
        case 23: { const uint32_t c = ram.u32(node + 4); if (ram.ok(c)) walk(ram, c, m, depth + 1); break; } // ACTION_DATA
        case 35: kids(ram, node + 0xC, ram.u32(node + 4), m, depth); break;                 // contêiner de ANIM_XFORM (partes rigidas do monstro: maos, cabeca...): contagem em +4, filhos em +0xC (dbsWalk caso 35)
        case 25: {                                                                          // CHAR_INSTANCE
            // As partes do corpo sao nos ANIM_XFORM (hierarquia rigida, sem skinning). O proprio jogo calcula a matriz local de
            // cada no a cada quadro (hierAnimTransNode) em animOutput.atMat[matrixIdx]; basta le-la.
            const uint32_t saved = atMat_, savedPkt = animPkt_, savedVal = animVal_;
            atMat_ = ram.u32(node + 0xC);
            animPkt_ = ram.u32(node + 0x18);
            animVal_ = ram.u32(node + 4);
            kids(ram, node + 0x2C, ram.u32(node + 0x28), m, depth);
            atMat_ = saved;
            animPkt_ = savedPkt;
            animVal_ = savedVal;
            break;
        }
        case 17: {                                                                          // ANIM_XFORM
            if (ram.f32(node + 0xC) == 0.f) break;                                          // visible
            M4 local = ident();
            const uint32_t idx = ram.u32(node + 0xC4);
            if (atMat_ && idx < 4096 && ram.ok(atMat_ + idx * 64, 64)) local = readM4(ram, atMat_ + idx * 64);
            local.m[0][3] = local.m[1][3] = local.m[2][3] = 0.f; local.m[3][3] = 1.f;
            kids(ram, node + 0xCC, ram.u32(node + 4), mul(local, m), depth);
            break;
        }
        case 38: {                                                                          // ANIM_SCALE: escala e depois translacao
            {   // hierAnimScaleNode nao grava +0xC: a visibilidade e o canal val[vidx] da saida de animacao do CHAR_INSTANCE (-1 = sempre visivel)
                const int vidx = int16_t(ram.u16(node + 0x10));
                const float vis = (vidx >= 0 && animVal_ && vidx < 4096) ? ram.f32(animVal_ + 4 * uint32_t(vidx)) : (vidx >= 0 ? ram.f32(node + 0xC) : 1.f);
                if (vis == 0.f) break;
            }
            float sx = ram.f32(node + 0x20), sy = ram.f32(node + 0x24), sz = ram.f32(node + 0x28);
            if (sx == 0.f && sy == 0.f && sz == 0.f) sx = sy = sz = 1.f;
            kids(ram, node + 0x60, ram.u32(node + 4),
                 mul(mul(scaleM(sx, sy, sz), translateM(ram.f32(node + 0x30), ram.f32(node + 0x34), ram.f32(node + 0x38))), m), depth);
            break;
        }
        case 39: {                                                                          // INTERACTIVE_STATE (+4 whichChild, +0xC filho)
            // O filho de um destrutivel e um DESTRUCTIBLE: o jogo escolhe o estado (0 = intacto, depois danificado/ruina...) em
            // `whichChild` deste no pai (docs/FORMATOS.md, "Destrutiveis"); a travessia desce so para child[whichChild].child.
            const uint32_t c = ram.u32(node + 0xC);
            if (!ram.ok(c)) break;
            if ((ram.u32(c) & 63) == 31) {
                const uint32_t n = ram.u16(c + 0xA);
                uint32_t w = ram.u32(node + 4);
                static const bool noDestr = std::getenv("WOTM_NODESTR") != nullptr;   // diagnostico: sempre o estado 0 (intacto)
                ++destrHist_[std::min<uint32_t>(w, 7)];
                if (noDestr || w >= n) w = 0;
                if (!n) break;
                const uint32_t ch = ram.u32(c + 0xC + 12 * w);
                if (ram.ok(ch)) walk(ram, ch, m, depth + 1);
            } else walk(ram, c, m, depth + 1);
            break;
        }
        case 31: {                                                                          // DESTRUCTIBLE: estado 0 (intacto)
            if (ram.u16(node + 0xA)) { const uint32_t c = ram.u32(node + 0xC); if (ram.ok(c)) walk(ram, c, m, depth + 1); }
            break;
        }
        default: break;   // colisao, luz, particulas, animacao...: proximos marcos
        }
    }

    void csList(const Ram &ram, uint32_t head) {
        uint32_t n = ram.u32(head + 4);
        for (int guard = 0; ram.ok(n) && n != head && guard < 4096; ++guard, n = ram.u32(n + 4)) {
            drawCs(ram, ram.u32(n), true);
        }
    }

    // Icones de botao (X, O, quadrado, triangulo) de `McButtons`: ids que `McButtons::initButton` aceita (0x1F51..0x1F54 e 0x240E..0x2411).
    // `drawButtonsForString` liga o `drawMe` e posiciona cada um com `AlignCsWithView` (alguns passos a frente da camera, orientado como ela),
    // entao nao sao HUD de espaco de tela. Ficam em z perto de 0 (podem cair abaixo do chao do cenario): o jogo os desenha por cima.
    static bool isButtonId(uint32_t id) { return (id >= 0x1F51 && id <= 0x1F54) || (id >= 0x240E && id <= 0x2411); }

    // Um `_cs` (instancia com matriz e posicao proprias): epNode em +0, drawMe +0xC, trans +0x10, mat +0x20, scaleMe +0xF, scale +0x80
    void drawCs(const Ram &ram, uint32_t cs, bool needDrawMe, const float *at = nullptr) {
        if (!ram.ok(cs, 0xB0) || (needDrawMe && !ram.u8(cs + 0xC))) return;
        const uint32_t ep = ram.u32(cs);
        if (!ram.ok(ep)) return;
        const uint32_t id = ram.u32(ep) >> 18;   // id do objeto (14 bits altos); 0x1C20..0x2133 = elementos de HUD (espaco de tela)
        if (needDrawMe && id >= 0x1C20 && id <= 0x2133 && !isButtonId(id)) return;
        M4 m = readM4(ram, cs + 0x20);
        m.m[3][0] = ram.f32(cs + 0x10); m.m[3][1] = ram.f32(cs + 0x14); m.m[3][2] = ram.f32(cs + 0x18); m.m[3][3] = 1.f;
        m.m[0][3] = m.m[1][3] = m.m[2][3] = 0.f;
        if (at) { m.m[3][0] = at[0]; m.m[3][1] = at[1]; m.m[3][2] = at[2]; }
        else if (eyeOk_) {
            // O hier leva o olho ao espaco do objeto com a propria mat (hierCsUpdate: mat * (eo + trans)) e volta com ela, nao com a
            // inversa: o VU1 desenha em olho + (trans - olho) * R^T * R (R = 3x3 da mat). Com R ortonormal nao muda nada; nos destrocos
            // que encolhem (linhas com norma < 1) o objeto sai puxado para a camera (ate ~37 unidades; checagem VU1, pista 3).
            float a[3];
            for (int i = 0; i < 3; ++i)
                a[i] = (m.m[3][0] - eye_[0]) * m.m[i][0] + (m.m[3][1] - eye_[1]) * m.m[i][1] + (m.m[3][2] - eye_[2]) * m.m[i][2];
            for (int k = 0; k < 3; ++k) m.m[3][k] = eye_[k] + a[0] * m.m[0][k] + a[1] * m.m[1][k] + a[2] * m.m[2][k];
        }
        if (ram.u8(cs + 0xF)) {   // scaleMe
            const float sx = ram.f32(cs + 0x80), sy = ram.f32(cs + 0x84), sz = ram.f32(cs + 0x88);
            m = mul(scaleM(sx, sy, sz), m);
        }
        const size_t before = items.size();
        walk(ram, ep, m, 0);
        if (needDrawMe && isButtonId(id)) {   // icones de botao: por cima da cena, sem profundidade (ver `overlay`)
            overlay.insert(overlay.end(), std::make_move_iterator(items.begin() + before), std::make_move_iterator(items.end()));
            items.resize(before);
        }
    }
};

}  // namespace wotm
