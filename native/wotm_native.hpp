// Camada nativa do WotM: le a cena direto da RAM do EE e desenha com raylib (sem VU1/GS).
//
// Usada de dois jeitos: pelo runtime recompilado (wotm_scene.inc, a cada quadro, sobre a RDRAM viva)
// e pelo visualizador standalone (native/viewer.cpp, sobre um dump `ram_<quadro>.bin`).
// Formatos: docs/FORMATOS.md. Este arquivo nao contem dados do jogo.
//
// Convencoes do jogo (PS2): vetores-linha (p' = p * M), mundo com Z para cima, float32 little-endian.
#pragma once
#include "wotm_syms.h"   // wotm::sym / wotm::fn: enderecos do ELF por nome (tools/native_syms.py --gen)

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
constexpr uint32_t world = wotm::sym::world;            // _worldctx *world (ep@0, skyCs@4, eo@0x20, weMat@0x30)
constexpr uint32_t worldCtx = wotm::sym::worldCtx;         // _worldctx worldCtx[5] (0x1F0 cada)
constexpr uint32_t g_doFOVGroups = wotm::sym::g_doFOVGroups; // int: o hierTraverseAsm corta GROUP por esfera (Scene::outOfView)
constexpr uint32_t viewInfo = wotm::sym::viewInfo;         // 5 x 0x14: [0] = _cs* da camera, [1] = viewport*
constexpr uint32_t worldToScreenMat = wotm::sym::worldToScreenMat; // 5 x 0x40
constexpr uint32_t paraL = wotm::sym::paraL;            // float[4][4]: linhas x, y, z; colunas = luzes paralelas 0-2 e vetor-meio do especular, x32
constexpr uint32_t vu1FogParams = 0x0025B280;     // float[4] (D_0025B280, qword 21 do VU1): fogFA, fogMaxVal, 1, fogFB (fogUpdate)
constexpr uint32_t fogColorCtx = wotm::sym::fogColorCtx;      // u64[6]: pacote do FOGCOL que o fogUpdate manda ao GS; [4] = R | G << 8 | B << 16
constexpr uint32_t vu1LightColor = 0x0025B238;    // float[4][4] (D_0025B238): cor/128 das luzes 0-2 (linhas); a linha 3 e o ambiente/especular
constexpr uint32_t csActiveList = wotm::sym::x_6CsPool_m_activeList;     // CsPool::m_activeList (CsNode: cs@0, next@4)
constexpr uint32_t csHPActiveList = wotm::sym::x_6CsPool_m_HPActiveList;   // CsPool::m_HPActiveList
constexpr uint32_t tempVramTexAddr = wotm::sym::tempVramTexAddr;  // fim dos recursos (.RTX) na VRAM, em palavras; >> 6 = bloco base das paletas
constexpr uint32_t fileStatus = wotm::sym::fileStatus;      // FileStatus: resAddr@0x78, resLoaded@0xB0, names[14][9]@0xB1, maxTexAddr[14]@0x14C, maxResAddr[14]@0x168
constexpr uint32_t texInfo = wotm::sym::texInfo;         // texInfo[1000] x 16: [0] = pacote de upload da textura (RAM), [4] = residencia/VRAM
constexpr uint32_t shellFadePacket = 0x00731540; // D_00731540: pacote do Shell::FadeScreen (DMA, A+D ALPHA_1, SPRITE); o laco reescreve o alfa em +0x4C a cada campo
constexpr uint32_t fontPacket = 0x00735900;      // pacote GIF do texto do quadro (fontBuildPrim/fontDmaFontData): cabecalho A+D, depois corridas de sprites
constexpr uint32_t psBlkLst = wotm::sym::psBlkLst;        // _pshadowblocker[10] x 0x30: cs@0x10, cs2@0x18, &origem@0x1C, &raio@0x20, fator@0x24, emUso u16@0x2E
constexpr uint32_t psLitDirWs = wotm::sym::psLitDirWs;      // direcao da luz das sombras (da cena para a luz), mundo
constexpr uint32_t psShadowIntens = wotm::sym::psShadowIntens;  // float, 80 = 0,625 (escala do GS)
constexpr uint32_t psShadowOn = 0x006F8D7C;      // psSetShadowOnOff
}

// Pacote de upload de uma textura (`texInfo[id]`): o `texmSetupTexture` monta em +0x20 o `sceGsLoadImage` do SDK
// (`sceGsSetDefLoadImage`): BITBLTBUF em +0x30 (DBW nos bits 48..53, DPSM 56..61), TRXREG em +0x50 (RRW 0..11, RRH 32..43); pixels
// lineares a partir de +0x80. (Ate 2026-10-15 o stub do runtime gravava ali uma struct propria, com largura em +0x24, altura em +0x26 e
// psm em +0x2B; dumps de RAM daquela epoca nao servem mais.)
struct UploadHdr {
    uint32_t w, h, psm, bw;
    static UploadHdr read(const Ram &ram, uint32_t ptr) {
        return {ram.u16(ptr + 0x50) & 0xFFFu, ram.u16(ptr + 0x54) & 0xFFFu, ram.u8(ptr + 0x37) & 0x3Fu, ram.u8(ptr + 0x36) & 0x3Fu};
    }
};

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
    uint8_t cull = 0;                 // secao com culling (bit 15 do seletor): o VU1 descarta a face de costas (Scene::backFacing)
    int8_t nx = 0, ny = 0, nz = 0;    // normal (V3-8 @0x175, mesmo indice da posicao; modulo ~127): luz por vertice (Scene::vertexRgb)
    uint8_t fge = 0;                  // PRIM.FGE da secao: o GS aplica a neblina (Scene::draw, shader)
};

// OBJECT sprite (objType 2, flags +0xC bits 5-6): secao com tag GIF SPRITE em H e 6 qwords por sprite, sem indices: ST0 @H+1,
// RGBA @H+2 (V4-8, passo 6), centro local + lado em w @H+3 (V4-32, passo 6), ST1 @H+4 (V2-16, passo 3 com o ST0). O microprograma
// (0x2E58, `pktAddVu1ObjAsm` pela tabela +0x158) projeta o centro e emite os cantos centro -/+ (512, 256) * lado / W pixels do GS,
// com ST0/ST1 (/4096, + uvOff), cor do pacote vezes q5 (limitada a 255, sem luz) e o F da neblina no centro. Halo do power-up.
struct ObjSprite {
    float x, y, z, size;          // centro no espaco do objeto; lado (o w do V4-32)
    float u0, v0, u1, v1;         // ST dos cantos (-, -) e (+, +) da tela do GS (4096 = 1,0)
    uint8_t r, g, b, a;           // RGBA cru do GS (0x80 = 1,0)
    uint8_t slot = 0, fge = 0;    // como Vtx::slot / Vtx::fge
};

struct MeshData {
    std::vector<Vtx> tris;   // lista de triangulos (3 vertices por triangulo)
    std::vector<ObjSprite> sprites;   // secoes SPRITE (ObjSprite): voltadas para a camera, montadas no desenho
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
    uint8_t curFge = 0;   // PRIM.FGE em vigor (neblina; mesma regra do TME)
    uint8_t curSprite = 0;   // PRIM do tipo SPRITE em vigor (ObjSprite; mesma regra do TME)
    uint32_t posTot = 0, posUsed = 0;   // posicoes carregadas / usadas por algum indice (diagnostico)
    uint32_t dropAdc = 0, dropDegen = 0;   // triangulos descartados por ADC / degenerados (diagnostico)
    uint32_t secTot = 0, secBad = 0, secBadEx = 0;   // secoes de tira / com nloop != indices (diagnostico)
    uint32_t oobIdx = 0, totIdx = 0;   // indices de vertice fora de `pos` (viram (0,0,0)) / total: diagnostico de decodificacao
};

// Pula os qwords zerados entre sub-pacotes.
inline uint32_t skipZeroQwords(const Ram &ram, uint32_t pp) {
    uint32_t skip = 0;
    while (skip < 0x400 && ram.ok(pp + skip, 16) && ram.u32(pp + skip) == 0 && ram.u32(pp + skip + 4) == 0) skip += 16;
    return pp + skip;
}

// Assinatura barata do pacote em `pp` (ja sem os qwords zerados): tag, primeira e ultima palavra (sem a ultima nos skinned, cujo
// fim so se acha decodificando). Scene::mesh revalida o cache com ela; uma formula diferente da do decodePacket fazia a malha ser
// decodificada de novo a cada chamada, e o ponteiro devolvido antes ficava solto.
inline uint32_t packetSig(const Ram &ram, uint32_t pp) {
    const uint32_t tag = ram.u32(pp), nwords = ((tag & 0xFFFF) + 1) * 4 - 2;
    return tag ^ ram.u32(pp + 8) ^ (ram.u32(pp + 12) * 31u) ^ ((tag & 0x10000) ? 0u : ram.u32(pp + 4 * (nwords + 1)) * 17u);
}

// Decodifica um sub-pacote em `md`; devolve o endereco do proximo (cadeia skinned) ou 0.
inline uint32_t decodePacket(const Ram &ram, uint32_t pp, MeshData &md, bool first) {
    pp = skipZeroQwords(ram, pp);
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
        md.sig = packetSig(ram, pp);
        md.valid = true;
    }
    uint32_t endPp = 0;

    struct P3 { float x, y, z, w; uint8_t cx, cy, cz; };
    std::vector<P3> pos;
    std::vector<std::array<int8_t, 3>> nrm;   // normais (V3-8 @0x175), pelo indice da posicao
    std::vector<char> usedPos;   // diagnostico: posicao referenciada por algum indice
    uint32_t hdrAddr = 0, nloop = 0, hdrSel = 0;
    bool haveHdr = false;
    std::vector<uint8_t> idx;
    std::vector<std::array<uint8_t, 4>> rgba;
    std::vector<std::array<int16_t, 2>> uv;
    std::vector<uint32_t> adc;
    std::vector<std::array<float, 4>> spos;   // centros + lado das secoes SPRITE
    uint32_t mask = 0;

    auto flush = [&]() {
        if (haveHdr && md.curSprite) {
            const uint32_t sel = hdrSel & 0x7FFF;
            if (sel >= 1 && sel <= 13) md.curSlot = uint8_t((sel - 1) / 3);
            const uint8_t slot = md.curTme ? md.curSlot : kSlotNoTex;
            const size_t n = std::min<size_t>({nloop, spos.size(), rgba.size(), uv.size() / 2});
            for (size_t k = 0; k < n; ++k) {
                ObjSprite s;
                s.x = spos[k][0]; s.y = spos[k][1]; s.z = spos[k][2]; s.size = spos[k][3];
                s.u0 = uv[2 * k][0] / 4096.f; s.v0 = uv[2 * k][1] / 4096.f; s.u1 = uv[2 * k + 1][0] / 4096.f; s.v1 = uv[2 * k + 1][1] / 4096.f;
                s.r = rgba[k][0]; s.g = rgba[k][1]; s.b = rgba[k][2]; s.a = rgba[k][3];
                s.slot = slot; s.fge = md.curFge;
                md.sprites.push_back(s);
                md.slotMask |= 1u << slot;
            }
        } else if (haveHdr && !idx.empty()) {
            const uint32_t n = std::min<uint32_t>(nloop, uint32_t(idx.size()));
            ++md.secTot;
            if (nloop != idx.size() || rgba.size() < n || uv.size() < n) { ++md.secBad; if (md.secBadEx < 4) { ++md.secBadEx; std::fprintf(stderr, "   secao: nloop=%u idx=%zu rgba=%zu uv=%zu" "%c", nloop, idx.size(), rgba.size(), uv.size(), 10); } }
            std::vector<bool> restart(n + 1, false);
            for (uint32_t k : adc) if (k < n) restart[k] = true;
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
                if (j < nrm.size()) { v.nx = nrm[j][0]; v.ny = nrm[j][1]; v.nz = nrm[j][2]; }
                const uint32_t sel = hdrSel & 0x7FFF;   // bit 15: culling da secao (os predios usam 0x8001, 0x8004...)
                if (sel >= 1 && sel <= 13) md.curSlot = uint8_t((sel - 1) / 3);   // 0 = mantem a textura anterior
                v.slot = md.curTme ? md.curSlot : kSlotNoTex;
                v.fge = md.curFge;
                v.cull = uint8_t((hdrSel >> 15) & 1);
                v.sub = md.curSub;
                md.slotMask |= 1u << v.slot;
                if (k < rgba.size()) { v.r = rgba[k][0]; v.g = rgba[k][1]; v.b = rgba[k][2]; v.a = rgba[k][3]; }
                else { v.r = v.g = v.b = v.a = 128; }
                if (k < uv.size()) { v.u = uv[k][0] / 4096.f; v.v = uv[k][1] / 4096.f; }
                strip.push_back(v);
            }
            emit();
        }
        haveHdr = false; idx.clear(); rgba.clear(); uv.clear(); adc.clear(); spos.clear();
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
            } else if (vn == 2 && vl == 2 && addr == 0x175) {                 // V3-8 normais (chegam junto das posicoes)
                nrm.resize(n);
                for (uint32_t k = 0; k < n; ++k) nrm[k] = {int8_t(ram.u8(a + 3 * k)), int8_t(ram.u8(a + 3 * k + 1)), int8_t(ram.u8(a + 3 * k + 2))};
            } else if (haveHdr && md.curSprite && vn == 3 && vl == 0 && addr == hdrAddr + 3) {   // V4-32 centros + lado (SPRITE)
                spos.resize(n);
                for (uint32_t k = 0; k < n; ++k) spos[k] = {ram.f32(a + 16 * k), ram.f32(a + 16 * k + 4), ram.f32(a + 16 * k + 8), ram.f32(a + 16 * k + 12)};
            } else if (haveHdr && md.curSprite && vn == 1 && vl == 1 && addr == hdrAddr + 1) {   // V2-16 ST0/ST1 (SPRITE)
                uv.resize(n);
                for (uint32_t k = 0; k < n; ++k) uv[k] = {ram.s16(a + 4 * k), ram.s16(a + 4 * k + 2)};
            } else if (vn == 3 && vl == 0 && n == 1) {                        // V4-32: tag GIF
                hdrAddr = addr; nloop = ram.u32(a) & 0x7FFF; haveHdr = true;
                hdrSel = ram.u32(a + 12);   // palavra alta de REGS: o jogo a usa como seletor de textura (1 + 3*slot)
                if ((ram.u32(a + 4) >> 14) & 1) md.curTme = uint8_t((ram.u32(a + 4) >> 19) & 1);   // PRE: PRIM nos bits 47-57 (TME = bit 4)
                if ((ram.u32(a + 4) >> 14) & 1) md.curFge = uint8_t((ram.u32(a + 4) >> 20) & 1);   // FGE = bit 5 do PRIM
                if ((ram.u32(a + 4) >> 14) & 1) md.curSprite = uint8_t(((ram.u32(a + 4) >> 15) & 7) == 6);   // tipo 6 = SPRITE
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

// Teste de alfa padrao (TEST_1 0x8D: alfa > 8): o que o shader do Scene::draw descarta quando o GROUP nao traz outro.
// kGsAlphaNoTest: TEST_1 com ATE = 0 (sem teste: o GS desenha todo pixel, inclusive As = 0; os brilhos aditivos 0x5040C dependem disso).
inline constexpr int16_t kGsAlphaRef = 8, kGsAlphaNoTest = -1;

struct DrawItem {
    M4 m;
    uint32_t node;   // HierObject
    uint32_t animPkt = 0;   // CHAR_INSTANCE::animPktPtr (paleta de matrizes dos objetos skinned)
    int lodIdx = 0;         // indice do LOD escolhido (diagnostico; a cadeia da paleta e animPktPtr[packetPtrIdx], Scene::palette)
    std::shared_ptr<std::vector<M4>> pal;   // copia das 31 matrizes da paleta (preenchida por takeSnapshot; vazia = ler da RAM)
    uint8_t blend = kGsAlphaNormal;   // byte baixo do ALPHA_1 do no mais proximo com ctx (GROUP, TRANSLATE...; Scene::CtxScope, gsBlend)
    float fade = 1.f;       // cross-fade de LOD (hierLod): multiplica o alfa; < 1 vai para o passe translucido
    float uvOff[2] = {0, 0};   // deslocamento do UV (qword 4 do cabecalho do VU1, somado ao UV normalizado): Scene::objectUvOffset
    float color[4] = {1, 1, 1, 1};   // multiplicador da cor do vertice (qword 5, vf31; sem o fade, que fica em `fade`): Scene::objectColor
    float spec = 0.f;  // especular do VU1 (qword 20) que vale para este objeto: Scene::walk (herdado do ultimo com specLightMe)
    uint8_t env = 0;   // mapa de ambiente (Scene::objectEnv): 0 = UV do pacote, 1 = mapa comum, 2 = video/quadro anterior, 3 = CLOAK
    float envK = 0.f;  // peso da normal no UV do mapa de ambiente (envMapWeight ou o do CLOAK)
    uint16_t hudId = 0;   // so em Scene::hud: id do elemento de HUD (do epNode do _cs)
    int16_t aref = kGsAlphaRef;   // teste de alfa do TEST_1 do GROUP que contem o objeto: descarta As * 128 <= aref (Scene::groupState; kGsAlphaNoTest = sem teste)
    bool zmsk = false;   // ZBUF_1.ZMSK do GROUP que contem o objeto: nao grava profundidade (so vale no passe translucido; Scene::groupState)
    bool lodX = false;   // dentro de um cross-fade de LOD (os dois LODs, inclusive o de fade 1): com o bit 0x4000 vai para a OT de alfa
};

struct Camera {
    float pos[3] = {0, 0, 0};
    float yaw = 0, pitch = 0;   // radianos; yaw 0 olha para +X, Z para cima
};

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
    float uv[4] = {0, 0, 1, 1};   // u0, v0, u1, v1 normalizados ((u0, v0) no canto de cima a esquerda da tela)
    uint32_t gsCol = 0;     // destroço: RGBA do GS (0x80 = 1,0) do RGBAQ do pacote; 0 = usa rgba
    int16_t aref = 8;       // destroço: limiar do teste de alfa (Scene::setAlphaRef); kGsAlphaRef = padrao
    bool opaque = false;    // destroço sem ABE com alphaContext 0: grava profundidade (ZBUF do setOpeningContext)
    uint32_t key = 0;       // fx * 4096 + indice: casa a mesma particula entre dois quadros
};

// Glifo do texto do jogo (fontSpritePrint*): um sprite do GS. UV em texels do atlas, XY em pixels da tela do jogo (640 x 224, campo).
struct TextQuad {
    float u0, v0, u1, v1;
    float x0, y0, x1, y1;
    uint8_t r, g, b, a;   // RGBAQ do GS: 0x80 = 1,0 (a cor modula a textura)
    uint64_t tex0;        // TEX0 em vigor (atlas T8H 512x512 do .RTX da fase + paleta CBP)
};

// Retangulo de tela sem textura (fade): SPRITE do GS com ABE e ALPHA_1 0x44 ((Cs - Cd)·As + Cd, As = a/0x80) sobre a vista, montado
// como pacote GIF por rtFadeScreen, TheGame::updateFadeOut/updateFadeOutAndIn, uiFadeScreen e Shell::FadeScreen. XY na tela do jogo
// (640 x 224, campo), como o texto.
struct ScreenRect {
    float x0, y0, x1, y1;
    uint8_t r, g, b, a;   // RGBAQ do GS: a = 0x80 cobre tudo
    // Saiu ao GS antes do texto do quadro (fila do gsPkt despachada antes do fontDmaFontData, como no ShellRtLoop::endField dos
    // dialogos de fim de fase): o texto vai por cima. No rtMain a fila sai depois do texto e o fade cobre tudo (false).
    bool underText = false;
};

// Le de um pacote (tag DMA em p, tags GIF PACKED a partir de p + 16) o primeiro SPRITE sem textura com mistura (REGS RGBAQ, XYZ2, XYZ2),
// exigindo que o A+D do mesmo pacote tenha posto ALPHA_1 = 0x44. Outro formato: false.
inline bool readScreenRect(const Ram &ram, uint32_t p, ScreenRect &out) {
    auto u64 = [&](uint32_t a) { return uint64_t(ram.u32(a)) | (uint64_t(ram.u32(a + 4)) << 32); };
    uint64_t alpha = ~0ull;
    uint32_t q = p + 16;
    for (int t = 0; t < 4 && ram.ok(q, 16); ++t) {
        const uint64_t tag = u64(q), regs = u64(q + 8);
        const uint32_t nloop = uint32_t(tag & 0x7FFF), flg = uint32_t(tag >> 58) & 3, nreg = (uint32_t(tag >> 60) & 15) ? (uint32_t(tag >> 60) & 15) : 16;
        const uint32_t prim = uint32_t(tag >> 47) & 0x7FF;
        q += 16;
        if (flg != 0) return false;
        bool ad = true;
        for (uint32_t i = 0; i < nreg; ++i) ad = ad && ((regs >> (4 * i)) & 0xF) == 0xE;
        if (ad) {
            for (uint32_t i = 0; i < nloop * nreg && ram.ok(q, 16); ++i, q += 16)
                if ((u64(q + 8) & 0xFF) == 0x42) alpha = u64(q);   // ALPHA_1
            continue;
        }
        // PRE, SPRITE (6), ABE (0x40), sem TME (0x10)
        if (!((tag >> 46) & 1) || (prim & 7) != 6 || !(prim & 0x40) || (prim & 0x10) || nreg != 3 || (regs & 0xFFF) != 0x551 || nloop < 1 ||
            (alpha & 0xFF) != 0x44 || !ram.ok(q, 48))
            return false;
        constexpr float kOx = 2048.f - 320.f, kOy = 2048.f - 112.f;   // viewport cheio: 640 x 224 (campo), centro 2048 (como o texto)
        out.r = uint8_t(std::min(255u, ram.u32(q))); out.g = uint8_t(std::min(255u, ram.u32(q + 4)));
        out.b = uint8_t(std::min(255u, ram.u32(q + 8))); out.a = uint8_t(std::min(255u, ram.u32(q + 12)));
        out.x0 = float(ram.u32(q + 16) & 0xFFFF) / 16.f - kOx; out.y0 = float(ram.u32(q + 20) & 0xFFFF) / 16.f - kOy;
        out.x1 = float(ram.u32(q + 32) & 0xFFFF) / 16.f - kOx; out.y1 = float(ram.u32(q + 36) & 0xFFFF) / 16.f - kOy;
        return true;
    }
    return false;
}

// Quadrados de tela do ScreenPolys (HUD fora do hier: o icone de vida do Hud::drawHealthIcon). Cada fonte de s_sourceObjects (0x20
// bytes: pkt, end, numPolys, headerQw, curQw, obj, rgba, fx) tem um pacote GIF em s_polyBuffer: o polyInitPacket poe o A+D com o TEX0
// do objeto em pkt + 0x30 (ALPHA_1 0x44, TEST_1 0x8D: alfa > 8) e o polyBuildPrim acrescenta, a partir de pkt + 16·headerQw, 5 qwords
// por quadrado: RGBAQ, UV, XYZ2, UV, XYZ2 (SPRITE com textura, UV em texels 12.4). O dmaData fecha as tags, manda e zera numPolys:
// le-se com numPolys > 0, isto e, na entrada do dmaData (hook) ou antes dele (a coleta do replay, na entrada do hier, ja os tem: o
// Hud::addHealthIndicators roda antes). XY como o texto: tela do jogo 640 x 224, centro do GS em 2048.
inline void readScreenPolys(const Ram &ram, std::vector<TextQuad> &out) {
    auto u64 = [&](uint32_t a) { return uint64_t(ram.u32(a)) | (uint64_t(ram.u32(a + 4)) << 32); };
    const int n = int(ram.u32(sym::s_numSourceObjects));
    for (int s = 0; s < n && s < 8; ++s) {   // s_sourceObjects: 0x100 bytes = 8 fontes
        const uint32_t so = sym::s_sourceObjects + 0x20 * uint32_t(s);
        const uint32_t pkt = ram.u32(so), polys = ram.u32(so + 8), hdr = ram.u32(so + 0xC);
        if (!polys || polys > 256 || !ram.ok(pkt + 16 * (hdr + 5 * polys)) || (u64(pkt + 0x38) & 0xFF) != 0x06) continue;
        const uint64_t tex0 = u64(pkt + 0x30);
        for (uint32_t i = 0; i < polys; ++i) {
            const uint32_t q = pkt + 16 * (hdr + 5 * i);
            constexpr float kOx = 2048.f - 320.f, kOy = 2048.f - 112.f;
            TextQuad t{};
            t.r = uint8_t(std::min(255u, ram.u32(q))); t.g = uint8_t(std::min(255u, ram.u32(q + 4)));
            t.b = uint8_t(std::min(255u, ram.u32(q + 8))); t.a = uint8_t(std::min(255u, ram.u32(q + 12)));
            t.u0 = float(ram.u32(q + 16) & 0x3FFF) / 16.f; t.v0 = float(ram.u32(q + 20) & 0x3FFF) / 16.f;
            t.x0 = float(ram.u32(q + 32) & 0xFFFF) / 16.f - kOx; t.y0 = float(ram.u32(q + 36) & 0xFFFF) / 16.f - kOy;
            t.u1 = float(ram.u32(q + 48) & 0x3FFF) / 16.f; t.v1 = float(ram.u32(q + 52) & 0x3FFF) / 16.f;
            t.x1 = float(ram.u32(q + 64) & 0xFFFF) / 16.f - kOx; t.y1 = float(ram.u32(q + 68) & 0xFFFF) / 16.f - kOy;
            t.tex0 = tex0;
            out.push_back(t);
        }
    }
}

// Bloqueador de sombra ativo (Scene::collectShadows / drawShadows): entrada de psBlkLst e os objetos que projetam a silhueta.
struct ShadowBlocker {
    int index = 0;               // entrada de psBlkLst
    float origin[3] = {0, 0, 0}; // cs->trans
    float extent = 0;            // meio lado do quadro de luz (raio do no * 1,1)
    std::vector<DrawItem> casters;
    std::vector<uint32_t> casterNodes;
};
struct ParaLights { float dir[4][3] = {}; float col[3][3] = {}; };   // luzes paralelas (Scene::readParaLights)
struct Fog { float fa = 0.f, maxVal = 255.f, fb = 0.f; float col[3] = {0, 0, 0}; };   // neblina (Scene::readFog, Scene::fogF)

// Tudo o que o desenho le do quadro do jogo, copiado da RAM de uma vez (itens com a paleta copiada, particulas, sombras, luzes,
// neblina). Na luta e no menu e tirado na thread do EE na entrada de timerWaitForMinUpdateRate, com o quadro pronto
// (wotmCaptureFrame); os desenhos ate o proximo quadro do jogo repetem este, sem ler a RAM que o jogo esta reescrevendo.
struct Snapshot {
    std::vector<Sprite> sprites;
    std::vector<DrawItem> items, sky, overlay, hud, hp, hpOverlay;
    std::vector<FxSystem> fx;
    Particles::FlareFrame flares;          // lens flare do quadro (Particles::collectFlares); sem ele o lado que desenha nao tem a lista
    std::vector<ShadowBlocker> shadows;
    std::vector<ScreenRect> screenRects;   // fades do quadro (wotmScreenRect, nos hooks dos emissores; nao vem do collect)
    std::vector<TextQuad> screenPolys;     // HUD do ScreenPolys (readScreenPolys: no takeSnapshot e no hook do dmaData)
    ParaLights lights;
    Fog fog;
    GameCamera cam;
    uint32_t g = 0;     // contador de quadros do jogo
    bool valid = false;
};

class Scene {
public:
    // Largura do quadro nativo / largura 4:3 da mesma altura (widescreen Hor+; 1 = tela original). Preenche: wotm_live.inc
    float wide = 1.f;
    // Corte por esfera contra a vista do jogo, como o hierTraverseAsm (Scene::outOfView); o viewer com camera livre o desliga.
    bool fovCull = true;
    std::vector<DrawItem> items;
    std::vector<DrawItem> sky;   // ceu / nuvens (desenhados antes, centrados na camera)
    std::vector<DrawItem> overlay;   // icones de botao (McButtons): por cima de tudo, sem teste de profundidade
    std::vector<DrawItem> hud;       // elementos de HUD (ids 0x1C20..0x2133): por cima da cena, sem profundidade, depois dos icones (DrawItem::hudId = id do objeto)
    // Lista de alta prioridade (CsPool::m_HPActiveList; hier(v, 1) -> hierTraceHPCsList), fora o HUD: moldura dos dialogos (shell->m_cs610,
    // pauseLevelOverlay e cia.) e icones de botao deles. O ShellRtLoop::endField a desenha depois da fila do gsPkt (fade) e antes do
    // texto; no rtMain e no displayDialog vem antes dos dois. drawHp, chamado pelo lado que apresenta depois dos fades sob o texto.
    std::vector<DrawItem> hp, hpOverlay;   // hpOverlay: os icones de botao dela (fadeByNode)
    std::vector<Sprite> sprites;   // particulas (destroços, poeira) lidas de fx[]; desenhadas como sprites voltados para a camera
    std::vector<TextQuad> text;    // glifos do quadro (updateText), desenhados por drawQuads depois da cena 3D e do screenPolys
    std::vector<ScreenRect> screenRects;   // fades (Snapshot::screenRects), desenhados por drawScreenRects por ultimo
    std::vector<TextQuad> screenPolys;     // HUD do ScreenPolys (Snapshot::screenPolys), desenhado por drawScreenPolys antes do texto
    uint32_t visited = 0;
    uint32_t byOp[64] = {};

    gs::Vram vram;   // so para depuracao: a VRAM do dump guarda paletas velhas (restos do menu)

    // Paletas da fase, lidas do `.RTX` do disco: entradas de `(palavra0 >> 2) * 16` bytes com cabecalho de 16 bytes (descritor GS
    // em +8: DBP relativo no meia-palavra +0xA, TW/TH nos bits 40..47) e o corpo em ordem logica de paleta (entrada i = palavra i):
    // 16x16 CT32 = 256 cores, 8x2 CT32 = 16 cores. O TEX0.CBP do objeto e `base + DBP`, com base = tempVramTexAddr >> 6.
    // A chave e o CBP ABSOLUTO (base do arquivo + DBP). Base do nivel = tempVramTexAddr >> 6; a do monstro i = fileStatus.maxTexAddr[i-1]
    // (texmResInit empilha os arquivos .RTX na ordem em que o Shell os carregou: nivel, jogadores, IAs).
    std::unordered_map<uint32_t, std::vector<uint32_t>> rtxPal;
    // Imagens residentes do .RTX (PSMT8H/4HL/4HH: telas, fontes, logos do menu, retratos do HUD): chave (psm << 16) | DBP. Os pixels
    // da RAM em `texInfo[id]` nao servem (o ponteiro aponta para outra coisa); o TBP0 do TEX0 e o DBP da entrada coincidem. Nos
    // arquivos de monstro o DBP e relativo: o texmResRelocate soma o fim da area residente do arquivo anterior (`resBase`).
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
            const bool ok = loadRtx((root + sep + "MON/" + names[size_t(i)] + ".RTX").c_str(), ram.u16(fs + 0x14C + 2 * (i - 1)), true,
                                    ram.u16(fs + 0x168 + 2 * (i - 1)));   // maxTexAddr e maxResAddr do arquivo anterior
            if (log) *log += " " + names[size_t(i)] + (ok ? "" : "(?)");
        }
        invalidate();
        resSig_ = sig;
        return lvl;
    }

    // base: soma no DBP das paletas (base do arquivo); resBase: soma no DBP das imagens residentes (0 na fase e no menu; num monstro,
    // fileStatus.maxResAddr do arquivo anterior, como no texmResRelocate).
    bool loadRtx(const char *path, uint32_t base, bool append = false, uint32_t resBase = 0) {
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
        // A entrada com tamanho 0 e a ultima e tambem vale (vai ate o fim do arquivo): nos monstros e o retrato do HUD (T8H 128x64).
        bool last = false;
        while (!last && off + 16 <= data.size()) {
            uint32_t w[4];
            std::memcpy(w, data.data() + off, 16);
            last = w[0] == 0;
            const size_t sz = last ? data.size() - off : size_t(w[0] >> 2) * 16;
            if (sz < 16 || off + sz > data.size()) break;
            const uint64_t desc = uint64_t(w[2]) | (uint64_t(w[3]) << 32);
            const uint32_t psm = uint32_t(desc >> 32) & 0x3F, tw = uint32_t(desc >> 40) & 15, th = uint32_t(desc >> 44) & 15;
            if (psm == 0 && ((tw == 4 && th == 4) || (tw == 3 && th == 1))) {
                std::vector<uint32_t> p((sz - 16) / 4);
                std::memcpy(p.data(), data.data() + off + 16, p.size() * 4);
                rtxPal[base + ((w[2] >> 16) & 0xFFFF)] = std::move(p);
                ++added;
            } else if ((psm == gs::T8H || psm == gs::T4HL || psm == gs::T4HH) && sz > 0x80) {
                rtxImg[(psm << 16) | ((resBase + ((w[2] >> 16) & 0xFFFF)) & 0xFFFF)].assign(data.begin() + long(off) + 0x80, data.begin() + long(off + sz));
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
        for (DustSrc &s : dustSrc_) s.ok = false;
        dustHash_ = 0;
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

    // Chave de textura de um objeto: id no `texInfo` + paleta (CBP/CSA/CPSM do TEX0) + TCC = 0 (bit 30; decodeTex). 0 = sem textura.
    // texId 0 com TEX0 = o buffer em exibicao, isto e, o quadro anterior (rtMain -> texmPrepTexInfoZero; texmGetTextureAddrs monta
    // PSMCT24 640 de largura, janela 1024x256): chave kScreenKey, textura de screenTexture.
    static constexpr uint64_t kScreenKey = 1ull << 63;
    static uint64_t texKey(const Ram &ram, uint32_t node, uint32_t slot = 0) {
        const uint64_t t0 = objectTex0(ram, node, slot);
        const uint32_t tid = objectTexId(ram, node, slot);
        if (!t0) return 0;
        if (!tid) return kScreenKey;
        const gs::Tex0 t = gs::Tex0::decode(t0);
        const uint64_t noTcc = (t0 >> 34) & 1 ? 0 : 1ull << 30;
        return (uint64_t(tid) << 40) | noTcc | (uint64_t(t.cbp) << 12) | (uint64_t(t.csa) << 4) | t.cpsm | (1ull << 62);
    }
    // No CLOAK (DrawItem::env 3) o pktBldEnvMapDma troca o TEX0 de cada slot com textura pelo do texId 0 (texmGetTextureAddrs).
    static uint64_t texKey(const Ram &ram, const DrawItem &it, uint32_t slot) {
        const uint64_t k = texKey(ram, it.node, slot);
        return k && it.env == 3 ? kScreenKey : k;
    }

    // Preenche a paleta de 256 entradas. O GS le CLUTs de 256 cores em CSM1 com os bits 3 e 4 do indice trocados (entradas 8-15 e
    // 16-23 de cada grupo de 32); o .RTX guarda o upload cru, entao a leitura tem que aplicar a troca (sem ela: granulado em degrades).
    // rawAlpha: alfa cru do GS (0x80 = 1,0), para o que o Scene::draw mistura como o GS; senao, 0..255 com 0x80 saturado (fixAlpha).
    static void fillPal(uint32_t pal[256], const std::vector<uint32_t> &src, bool is8bit, bool rawAlpha = false) {
        for (size_t i = 0; i < 256 && i < src.size(); ++i) {
            const size_t k = (is8bit && src.size() >= 256) ? ((i & 0xE7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1)) : i;
            pal[i] = rawAlpha ? src[k] : gs::fixAlpha(src[k]);
        }
    }

    // Textura do objeto: os pixels vem do pacote de upload apontado por `texInfo[texId]` (pixels lineares a partir de +0x80,
    // cabecalho em UploadHdr); a paleta vem do .RTX (carregado junto com a fase).
    // TEX0.TCC = 0 (so RGB): o GS usa o alfa do vertice e ignora o da textura; aqui o texel sai com alfa 1,0 (a tela do drive-in do
    // menu, tex 76, com vertices de alfa 0 e 0x80 por cima do logo).
    bool decodeTex(const Ram &ram, uint32_t node, std::vector<uint32_t> &px, uint32_t &w, uint32_t &h, uint32_t slot = 0, bool rawAlpha = false) const {
        const uint64_t t0v = objectTex0(ram, node, slot);
        if (!t0v) return false;
        if (!decodeTexRgba(ram, node, t0v, px, w, h, slot, rawAlpha)) return false;
        if (!((t0v >> 34) & 1)) {
            const uint32_t one = rawAlpha ? 0x80000000u : 0xFF000000u;
            for (uint32_t &c : px) c = (c & 0x00FFFFFFu) | one;
        }
        return true;
    }
    bool decodeTexRgba(const Ram &ram, uint32_t node, uint64_t t0v, std::vector<uint32_t> &px, uint32_t &w, uint32_t &h, uint32_t slot, bool rawAlpha) const {
        const gs::Tex0 t0 = gs::Tex0::decode(t0v);
        const uint32_t ptr = ram.u32(addr::texInfo + 16 * objectTexId(ram, node, slot)) & 0x0FFFFFFF;
        if (t0.psm == gs::T8H || t0.psm == gs::T4HL || t0.psm == gs::T4HH) {
            uint32_t pal[256] = {};
            if (decodeResident(t0, px, w, h, rawAlpha, pal)) return true;
            // Paleta sem a imagem no .RTX: a RAM em `ptr` (nao confiavel).
            if (rtxPal.find(t0.cbp) == rtxPal.end() || !ram.ok(ptr + 0x80)) return false;
            return gs::decodeUpload(ram.p + ptr, ram.size - ptr, w, h, t0.psm == gs::T8H ? gs::T8 : gs::T4, t0.csa, pal, px);
        }
        return decodeUploadTex(ram, ptr, t0, px, w, h, false, rawAlpha);
    }

    // Texturas residentes em PSMT8H/4HL/4HH (telas e logos do menu, fontes, retratos, icones do HUD): `texInfo[id]` aponta para outra
    // coisa; os pixels vem do .RTX do disco, entrada com DBP == TEX0.TBP0 e mesmo psm (4HL e 4HH repartem o DBP: sao os dois nibbles).
    // 8 bits, ou 4 bits empacotados (nibble baixo primeiro); tamanho = TW/TH do TEX0. `pal` sai preenchida se a paleta existe.
    bool decodeResident(const gs::Tex0 &t0, std::vector<uint32_t> &px, uint32_t &w, uint32_t &h, bool rawAlpha, uint32_t pal[256]) const {
        if (t0.psm != gs::T8H && t0.psm != gs::T4HL && t0.psm != gs::T4HH) return false;
        const auto it = rtxPal.find(t0.cbp);
        if (it == rtxPal.end()) return false;
        fillPal(pal, it->second, t0.psm == gs::T8H, rawAlpha);
        w = 1u << t0.tw; h = 1u << t0.th;
        const auto im = rtxImg.find((t0.psm << 16) | t0.tbp0);
        return im != rtxImg.end() &&
               gs::decodeUpload(im->second.data(), im->second.size(), w, h, t0.psm == gs::T8H ? gs::T8 : gs::T4, t0.csa, pal, px);
    }

    // Pixels do pacote de upload em `ptr` (= texInfo[texId]; cabecalho em UploadHdr, pixels lineares a partir de +0x80) com a paleta do .RTX pelo CBP do TEX0. Serve aos objetos (decodeTex) e as particulas (particleTexture).
    bool decodeUploadTex(const Ram &ram, uint32_t ptr, const gs::Tex0 &t0, std::vector<uint32_t> &px, uint32_t &w, uint32_t &h,
                         bool grayFallback = false, bool rawAlpha = false) const {
        if (!ram.ok(ptr + 0x80)) return false;
        uint32_t pal[256] = {};
        const UploadHdr hd = UploadHdr::read(ram, ptr);
        const uint32_t psm = hd.psm;
        if (psm == gs::T8 || psm == gs::T4) {
            // A paleta vem do .RTX carregado (loadRtx); sem ela a textura cai para a cor de vertice.
            const auto it = rtxPal.find(t0.cbp);
            if (it != rtxPal.end()) fillPal(pal, it->second, psm == gs::T8, rawAlpha);
            else if (grayFallback) {
                // Particulas: muitas texturas sao mascaras de luminancia/alfa; sem a paleta, rampa cinza com alfa = indice (1,0 no fim)
                const uint32_t n = psm == gs::T8 ? 256 : 16;
                for (uint32_t i = 0; i < n; ++i) {
                    const uint32_t v = i * 255 / (n - 1), a = rawAlpha ? i * 128 / (n - 1) : v;
                    pal[i] = v | (v << 8) | (v << 16) | (a << 24);
                }
            } else return false;
        }
        w = hd.w; h = hd.h;
        static const bool t4swz = std::getenv("WOTM_T4SWZ") != nullptr;   // diagnostico: pixels T4 do pacote como memoria do GS (nibbles embaralhados)
        if (t4swz && psm == gs::T4 && w && h && w <= 512 && h <= 512) {
            px.assign(size_t(w) * h, 0);
            const uint32_t bw = std::max<uint32_t>(1, hd.bw);
            for (uint32_t y = 0; y < h; ++y)
                for (uint32_t x = 0; x < w; ++x) {
                    const uint32_t n = gs::nibT4(0, bw, x, y);
                    const uint8_t b = ram.u8(ptr + 0x80 + n / 2);
                    px[size_t(y) * w + x] = pal[(t0.csa * 16 + ((n & 1) ? (b >> 4) : (b & 15))) & 255];
                }
            return true;
        }
        return gs::decodeUpload(ram.p + ptr + 0x80, ram.size - ptr - 0x80, w, h, psm, t0.csa, pal, px, rawAlpha);
    }

    // Textura de particula: texId (indice do texInfo global) + TEX0 da tabela de particulas (psm/CBP). 0 = nao decodificou.
    // Particles::kDustTexId = a textura composta de poeira/fumaca (dustTexture).
    // WOTM_FXNOGRAY=1: sem a rampa cinza quando a paleta nao esta nos .RTX carregados (diagnostico).
    unsigned particleTexture(const Ram &ram, uint32_t texId, uint64_t tex0) {
        if (texId == Particles::kDustTexId) return dustTexture(ram);
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
        if (decodeUploadTex(ram, ptr, t0, px, w, h, gray, true) && w && h && px.size() >= size_t(w) * h) {
            Image img{px.data(), int(w), int(h), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            t = LoadTextureFromImage(img);
            if (t.id) { SetTextureFilter(t, TEXTURE_FILTER_BILINEAR); SetTextureWrap(t, TEXTURE_WRAP_CLAMP); }
        }
        tex_[key] = t;
        return t.id;
    }

    // Textura composta de poeira/fumaca (VRAM 0xFD800, TEX0 com TBP 0x3F60, CT32 64x64, quatro celulas de 32x32), que o jogo refaz
    // na VRAM a cada particleDraw com algum sistema (texDrawDustPacketBuild, pacote em 0x78F6E0). Aqui a mesma conta na CPU, refeita
    // quando os parametros mudam (a fase anda 1/256 por quadro; depende da camera). docs/passos/VU1CHECK.md, "Corrigido".
    //  - Passe 1 (microprograma 0x2E98): um gerador h = frac(c + h * b) sorteia 70 normais x 4 celulas; cada uma vira um SPRITE de
    //    meio-lado 5 + 4t texels (t = frac(fase + h.w)) no centro da celula + A * n * t, com A das colunas do worldToScreenMat (q0-q3)
    //    e o foco (q5); z = min(72 - 1000 t (frente . n), 128), a = min(t, 1 - t) z, emitido so com a >= 1 (hemisferio voltado para a
    //    camera), ate 199 sprites. Desenha a fonte 1 (TextureInfo[32]) com RGBA (1, 106, 1, a) aditivo so RGB: a densidade fica no G.
    //  - Metade do RGB e A = G & 0x7F (copyGreenChannelToAlpha, o truque do CT16).
    //  - Passe 2 (MSCNT, 0x31C0): fundo e sprites com cor = ambiente + 0,5 + soma das luzes da fase (dustLighting) pela normal, e
    //    A = a * 1,20755, com a fonte 2 (TextureInfo[33]) em (Cs - Cd) * As + Cd so RGB.
    // Aritmetica em float comum (o VU trunca); a distribuicao e a mesma do dustsim.py da investigacao. Na METRO: cor ~(57, 57, 40),
    // alfa ate 127 (fumaca escura).
    unsigned dustTexture(const Ram &ram) {
        constexpr uint32_t kPkt = 0x0078F6E0u, kParams = 0x006F8DA4u, kNormals = 0x006F8D9Cu, kColors = 0x006F8DA0u, kCtx = 0x00256B90u;
        const uint32_t pa = ram.u32(kParams), na = ram.u32(kNormals), ca = ram.u32(kColors);
        if (!ram.ok(pa - 16, 48) || !ram.ok(na, 48) || !ram.ok(ca, 80) || !ram.ok(kPkt + 0xB4, 128)) return dustTex_.id;
        float P[8], L[3][4], C[5][3], ctx[8][4];
        for (int i = 0; i < 8; ++i) P[i] = ram.f32(pa + 4 * uint32_t(i));
        for (int r = 0; r < 3; ++r) for (int i = 0; i < 4; ++i) L[r][i] = ram.f32(na + 16 * uint32_t(r) + 4 * uint32_t(i));
        for (int k = 0; k < 5; ++k) for (int i = 0; i < 3; ++i) C[k][i] = ram.f32(ca + 16 * uint32_t(k) + 4 * uint32_t(i));
        for (int q = 0; q < 8; ++q) for (int i = 0; i < 4; ++i) ctx[q][i] = ram.f32(kCtx + 16 * uint32_t(q) + 4 * uint32_t(i));
        const uint32_t nloop = ram.u32(pa - 16), seed = ram.u32(pa - 4);
        uint64_t hash = 1469598103934665603ull;
        auto mix = [&](const void *d, size_t n) { for (size_t i = 0; i < n; ++i) hash = (hash ^ static_cast<const uint8_t *>(d)[i]) * 1099511628211ull; };
        mix(P, sizeof P); mix(L, sizeof L); mix(C, sizeof C); mix(ctx, sizeof ctx); mix(&nloop, 4); mix(&seed, 4);
        if (dustTex_.id && hash == dustHash_) return dustTex_.id;

        auto source = [&](uint32_t ti, DustSrc &s) {   // decodificada uma vez por (texId, TEX0)
            const uint32_t k = ram.u16(fxaddr::textureInfo + 8 * ti + 2) & 127, id = ram.u16(fxaddr::texId + 2 * k);
            const uint64_t t0 = uint64_t(ram.u32(fxaddr::tex0 + 8 * k)) | (uint64_t(ram.u32(fxaddr::tex0 + 8 * k + 4)) << 32);
            if (!id || id >= 1000) return false;
            if (s.ok && s.id == id && s.tex0 == t0) return true;
            const uint32_t ptr = ram.u32(addr::texInfo + 16 * id) & 0x0FFFFFFF;
            s.id = id; s.tex0 = t0;
            s.ok = decodeUploadTex(ram, ptr, gs::Tex0::decode(t0), s.px, s.w, s.h, false, true) && s.w && s.h && s.px.size() >= size_t(s.w) * s.h;
            return s.ok;
        };
        DustSrc &src1 = dustSrc_[0], &src2 = dustSrc_[1];
        if (!source(32, src1) || !source(33, src2)) return dustTex_.id;
        const double startTime = GetTime();   // diagnostico (WOTM_DUSTLOG): custo da conta, sem a decodificacao das fontes

        // ---- passe 1: lista de sprites
        auto fbits = [](uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; };
        auto fbitsOf = [](float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; };
        uint32_t rnd = 0x3F800000u | (seed & 0x7FFFFFu);   // RINIT / RNEXT do VU
        auto rnext = [&]() { const uint32_t x = rnd & 0x7FFFFFu; rnd = ((((x << 1) | (((x >> 4) ^ (x >> 22)) & 1u)) & 0x7FFFFFu) | 0x3F800000u); return fbits(rnd); };
        float h[4];
        for (float &v : h) v = rnext();
        const float hc[4] = {fbits(0x3f3659ad), fbits(0x3f1ec458), fbits(0x3eb64807), fbits(0x3f16d60a)};
        const float hb[4] = {fbits(0x3f7e2a20), fbits(0x3f7cd6ea), fbits(0x3f66b74f), fbits(0x3f7e6ea7)};
        const float sx = P[2] / ctx[5][0], sy = P[2] / ctx[5][1];
        float col[3][3];
        for (int i = 0; i < 3; ++i) {
            const float *c = ctx[i];
            col[i][0] = (c[0] + ctx[6][0] * c[3]) * sx; col[i][1] = (c[1] + ctx[6][1] * c[3]) * sy; col[i][2] = P[6] * c[3];
        }
        const float fwd[3] = {ctx[0][3], ctx[1][3], ctx[2][3]};
        static const float kCenter[4][2] = {{2032, 2032}, {2064, 2032}, {2032, 2064}, {2064, 2064}};   // celulas na tela do GS (2016 = texel 0)
        int16_t uvtab[16][4];   // UNPACK V4-16 do pacote: 4 sub-imagens x 4 espelhamentos da fonte, em 1/16 de texel
        for (uint32_t k = 0; k < 16; ++k)
            for (uint32_t c = 0; c < 4; ++c) uvtab[k][c] = ram.s16(kPkt + 0xB4 + 8 * k + 2 * c);
        struct DSpr { int x0, y0, x1, y1, a1; const int16_t *uv; float n[3]; int rgb2[3], a2; };
        std::vector<DSpr> spr;
        float r21[3] = {h[0] - 1.5f, h[1] - 1.5f, h[2] - 1.5f};
        int left = int(nloop), vi5 = 29;
        bool full = false;
        while (left > 0 && !full) {
            float t = P[3] + h[3];
            t -= t >= 0.f ? std::floor(t) : std::trunc(t);
            for (int i = 0; i < 4; ++i) { h[i] = hc[i] + h[i] * hb[i]; h[i] -= std::trunc(h[i]); }
            const float r2 = r21[0] * r21[0] + r21[1] * r21[1] + r21[2] * r21[2];
            const float half = (1.f + P[0]) + t * P[1];
            std::rotate(h, h + 1, h + 4);   // MR32
            const float rs = 1.f / std::sqrt(r2);
            float n[3] = {r21[0] * rs, r21[1] * rs, r21[2] * rs};
            for (int i = 0; i < 3; ++i) r21[i] = h[i] - 0.5f;
            for (int k = 0; k < 4; ++k) {
                const uint32_t idx = fbitsOf(h[0]) & 15;
                std::rotate(h, h + 1, h + 4);
                --left;
                if (vi5 >= 0x3FC) { full = true; break; }
                const float d[3] = {n[0] * t, n[1] * t, n[2] * t};
                const float cx = col[0][0] * d[0] + col[1][0] * d[1] + col[2][0] * d[2] + kCenter[k][0];
                const float cy = col[0][1] * d[0] + col[1][1] * d[1] + col[2][1] * d[2] + kCenter[k][1];
                const float cz = std::min(col[0][2] * d[0] + col[1][2] * d[1] + col[2][2] * d[2] + P[7], 128.f);
                const float a = std::min(1.f - t, t) * cz;
                if (a >= 1.f) {
                    DSpr s{int((cx - half) * 16.f) - 2016 * 16, int((cy - half) * 16.f) - 2016 * 16, int((cx + half) * 16.f) - 2016 * 16,
                           int((cy + half) * 16.f) - 2016 * 16, int(a), uvtab[idx], {n[0], n[1], n[2]}, {0, 0, 0}, 0};
                    spr.push_back(s);
                    vi5 += 5;
                }
                const float n0 = n[0];
                n[0] = -n[1]; n[1] = n[2]; n[2] = n0;   // a normal gira de celula para celula: (x, y, z) -> (-y, z, x)
            }
        }

        // ---- passe 2: luz
        auto light = [&](const float nv[3], int out[3]) {
            float dots[4];
            for (int j = 0; j < 4; ++j) dots[j] = std::max(0.f, L[0][j] * nv[0] + L[1][j] * nv[1] + L[2][j] * nv[2]);
            for (int i = 0; i < 3; ++i) {
                float v = C[4][i] + 0.5f;
                for (int j = 0; j < 4; ++j) v += C[j][i] * dots[j];
                out[i] = std::min(255, std::max(0, int(v)));
            }
        };
        int bg[3];
        light(fwd, bg);
        for (DSpr &s : spr) { light(s.n, s.rgb2); s.a2 = int(float(s.a1) * P[5]); }

        // ---- rasterizacao como o GS (bilinear com CLAMP na fonte; regra do canto superior esquerdo)
        auto sample = [](const DustSrc &src, float u, float v, int out[4]) {
            u -= 0.5f; v -= 0.5f;
            const int i0 = int(std::floor(u)), j0 = int(std::floor(v));
            const float fu = u - float(i0), fv = v - float(j0);
            float acc[4] = {0, 0, 0, 0};
            for (int dj = 0; dj < 2; ++dj)
                for (int di = 0; di < 2; ++di) {
                    const float wgt = (di ? fu : 1.f - fu) * (dj ? fv : 1.f - fv);
                    const int x = std::min(std::max(i0 + di, 0), int(src.w) - 1), y = std::min(std::max(j0 + dj, 0), int(src.h) - 1);
                    const uint32_t p = src.px[size_t(y) * src.w + size_t(x)];
                    for (int c = 0; c < 4; ++c) acc[c] += float((p >> (8 * c)) & 255) * wgt;
                }
            for (int c = 0; c < 4; ++c) out[c] = int(acc[c]);
        };
        std::vector<std::array<int, 4>> fb(64 * 64, std::array<int, 4>{0, 0, 0, 0});
        auto draw = [&](const DSpr &s, const DustSrc &src, const int rgb[3], int a, bool additive) {
            const float u0 = s.uv[0] / 16.f, v0 = s.uv[1] / 16.f, u1 = s.uv[2] / 16.f, v1 = s.uv[3] / 16.f;
            if (s.x1 <= s.x0 || s.y1 <= s.y0) return;
            for (int py = std::max(0, (s.y0 + 15) >> 4); py < std::min(64, (s.y1 + 15) >> 4); ++py)
                for (int px = std::max(0, (s.x0 + 15) >> 4); px < std::min(64, (s.x1 + 15) >> 4); ++px) {
                    int t[4];
                    sample(src, u0 + (u1 - u0) * float(px * 16 - s.x0) / float(s.x1 - s.x0), v0 + (v1 - v0) * float(py * 16 - s.y0) / float(s.y1 - s.y0), t);
                    const int as = (a * t[3]) >> 7;
                    auto &d = fb[size_t(py) * 64 + size_t(px)];
                    for (int c = 0; c < 3; ++c) {
                        const int cs = (rgb[c] * t[c]) >> 7;
                        d[c] = additive ? std::min(255, ((cs * as) >> 7) + d[c]) : std::max(0, std::min(255, (((cs - d[c]) * as) >> 7) + d[c]));
                    }
                }
        };
        const int rgb1[3] = {1, int(P[4]), 1};
        for (const DSpr &s : spr) draw(s, src1, rgb1, s.a1, true);
        for (auto &p : fb) {
            for (int c = 0; c < 3; ++c) p[c] >>= 1;          // sprite (0, 0, 0, 64) em (Cs - Cd) * As + Cd: ((-Cd * 64) >> 7) + Cd
            p[3] = p[1] & 0x7F;                            // copyGreenChannelToAlpha (o bit 15 do CT16 sai 0)
            for (int c = 0; c < 3; ++c) p[c] = bg[c];      // passe 2: fundo
        }
        for (const DSpr &s : spr) draw(s, src2, s.rgb2, s.a2, false);

        std::vector<uint32_t> px(64 * 64);   // alfa cru do GS (0x80 = 1,0), como as outras texturas de particula
        for (size_t i = 0; i < px.size(); ++i)
            px[i] = uint32_t(fb[i][0]) | (uint32_t(fb[i][1]) << 8) | (uint32_t(fb[i][2]) << 16) | (uint32_t(fb[i][3]) << 24);
        if (!dustTex_.id) {
            Image img{px.data(), 64, 64, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            dustTex_ = LoadTextureFromImage(img);
            if (dustTex_.id) { SetTextureFilter(dustTex_, TEXTURE_FILTER_BILINEAR); SetTextureWrap(dustTex_, TEXTURE_WRAP_CLAMP); }
        } else {
            UpdateTexture(dustTex_, px.data());
        }
        dustHash_ = hash;
        {   // diagnostico: WOTM_DUSTLOG=1 imprime o resumo a cada regeneracao
            static const bool lg = std::getenv("WOTM_DUSTLOG") != nullptr;
            if (lg) {
                int amax = 0; double asum = 0, cw[3] = {0, 0, 0};
                for (const auto &p : fb) { amax = std::max(amax, p[3]); asum += p[3]; for (int c = 0; c < 3; ++c) cw[c] += double(p[c]) * p[3]; }
                std::fprintf(stderr, "[dust] fase %.4f sprites %zu fundo %d %d %d alfa max %d media %.1f cor ponderada %.0f %.0f %.0f (%.2f ms)\n", P[3], spr.size(), bg[0], bg[1], bg[2],
                             amax, asum / 4096.0, asum ? cw[0] / asum : 0.0, asum ? cw[1] / asum : 0.0, asum ? cw[2] / asum : 0.0, (GetTime() - startTime) * 1000.0);
            }
        }
        return dustTex_.id;
    }
    struct DustSrc { std::vector<uint32_t> px; uint32_t w = 0, h = 0, id = 0; uint64_t tex0 = 0; bool ok = false; };
    DustSrc dustSrc_[2];   // fontes 1 e 2 (TextureInfo[32] e [33]) decodificadas
    Texture2D dustTex_{};
    uint64_t dustHash_ = 0;

    Particles fx;   // sistemas de particulas do quadro (collect junto com a cena, draw depois dos translucidos)

    // ------------------------------------------------------------ sombras projetadas (common/ps)
    // No retail, cada bloqueador ativo (ate 10 em psBlkLst: monstros e afins, registrados por psBlockerAdd) e desenhado de uma camera
    // posta na direcao da luz (vista 4, a `radius * fator` da origem) para uma textura pequena, filtrada e reprojetada sobre os objetos
    // proximos (psBldBlkRObjs). Aqui: a silhueta e rasterizada na CPU num quadro ortografico no espaco da luz (sem trocar de
    // framebuffer dentro do BeginMode3D, o que quebraria o render texture do runtime) e os triangulos dos objetos proximos sao
    // redesenhados com essa textura projetada. docs/passos/NATIVO.md ("Sombras").
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
        static const float kShadowPull = std::getenv("WOTM_SHADOWPULL") ? float(std::atof(std::getenv("WOTM_SHADOWPULL"))) : 0.002f;   // diagnostico
        const float kShadowPullMax = kShadowPull * 1000.f;
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
                    // so recebe sombra o OBJECT sem 0x200 nem 0x60 nas flags: o hierTraverseAsm so chama o psObjectCull (teste do
                    // receptor) nesses. No luta_hud recebem so os pisos (flags 0x90); o vidro, as arvores e os brilhos nao
                    if (ram.u16(it.node + 0xC) & 0x260) continue;
                    // esfera do objeto (+0x10 centro, +0x1C raio) contra o cilindro de sombra
                    float c[3];
                    xform(it.m, ram.f32(it.node + 0x10), ram.f32(it.node + 0x14), ram.f32(it.node + 0x18), c);
                    const float rad = std::fabs(ram.f32(it.node + 0x1C));
                    float u, v, h;
                    lightUV(c, u, v, h);
                    const float rr = rad / E;
                    if (!(std::isfinite(u) && std::isfinite(rr)) || u * u + v * v > (1.5f + rr) * (1.5f + rr) || h - rr > 0.5f) continue;
                    forEachTri(ram, it, [&](const float p[3][3], const Vtx *, const uint8_t (*)[3], const float (*)[3]) {
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
                                // 0,2% da distancia (ate 2); WOTM_SHADOWPULL troca (no luta_hud 0 a 0,002 da o mesmo)
                                if (dl > 1e-3f) { const float s = std::min(kShadowPullMax, dl * kShadowPull) / dl; q[0] += dx * s; q[1] += dy * s; q[2] += dz * s; }
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

    // Face de costas vista de `eye`: o VU1 (microprograma dos objetos) poe ADC nos triangulos de area positiva na tela do GS
    // (y para baixo), so nas secoes com o bit 15 do seletor (Vtx::cull). Em mundo: normal (b - a) x (c - a) para longe do olho.
    static bool backFacing(const float p[3][3], const float eye[3]) {
        const float u[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]};
        const float v[3] = {p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
        const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        return n[0] * (p[0][0] - eye[0]) + n[1] * (p[0][1] - eye[1]) + n[2] * (p[0][2] - eye[2]) > 0.f;
    }

    // Triangulos de um item em coordenadas de mundo (com skinning), todos os slots: f(p, v, rgb) com os tres vertices decodificados (UV,
    // cor, slot de textura e culling da secao), a cor de cada um nas unidades do GS (vertexRgb: com a luz do VU1 nos iluminados) e a
    // normal no mundo do mapa de ambiente (envNormal; zero sem DrawItem::env)
    template <class F> void forEachTri(const Ram &ram, const DrawItem &it, F &&f) {
        const MeshData *md = mesh(ram, it.node);
        if (!md || !md->valid) return;
        const uint32_t pal = md->skinned ? palette(ram, it) : 0;
        const ObjLight ol = objectLight(ram, it);
        const auto &t = md->tris;
        std::vector<M4> pm;   // paleta (osso x da normal nos iluminados e no mapa de ambiente)
        if (pal && (ol.lit || it.env))
            for (uint32_t s = 0; s < 32; ++s) pm.push_back(readM4(ram, pal + 0x10 + 0x40 * s));
        float p[3][3], nw[3][3] = {};
        uint8_t rgb[3][3];
        for (size_t k = 0; k + 2 < t.size(); k += 3) {
            for (int c = 0; c < 3; ++c) {
                const Vtx &v = t[k + c];
                const M4 *bone = pm.empty() ? nullptr : &pm[(v.cx & 0x7F) / 4];
                if (pal) skinVertex(ram, pal, it.m, v, p[c]);
                else xform(it.m, v.x, v.y, v.z, p[c]);
                vertexRgb(ol, it.color, bone, it.m, v, rgb[c]);
                if (it.env) envNormal(v, bone, it.m, nw[c]);
            }
            f(p, &t[k], rgb, nw);
        }
    }

    // Silhueta do bloqueador no quadro de luz: rasterizada na CPU (kShadowRes^2), borrada (o retail filtra com sprites) e enviada
    // como textura preta com alfa = cobertura. Uma textura por posicao em `shadows`, reaproveitada entre quadros.
    unsigned shadowTexture(const Ram &ram, size_t slot, const ShadowBlocker &b, const float R[3], const float U[3]) {
        constexpr int N = kShadowRes;
        std::vector<float> cov(size_t(N * N), 0.f);
        const float E = b.extent;
        for (const DrawItem &it : b.casters) {
            forEachTri(ram, it, [&](const float p[3][3], const Vtx *, const uint8_t (*)[3], const float (*)[3]) {
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
        const UploadHdr hd = UploadHdr::read(ram, ptr);
        const uint32_t psm = hd.psm;
        if (psm == gs::T8 || psm == gs::T4) {
            const auto it = rtxPal.find(t0.cbp);
            if (it == rtxPal.end()) return false;
            fillPal(pal, it->second, psm == gs::T8, true);
        }
        w = hd.w; h = hd.h;
        return w && h && gs::decodeUpload(ram.p + ptr + 0x80, ram.size - ptr - 0x80, w, h, psm, t0.csa, pal, px, true);
    }

    // diagnostico: WOTM_TEXFROM=<de>:<para> desenha os vertices do slot <de> com a textura do slot <para>
    static uint32_t texRemap(uint32_t slot) {
        static int from = -1, to = -1; static bool init = false;
        if (!init) { init = true; if (const char *e = std::getenv("WOTM_TEXFROM")) std::sscanf(e, "%d:%d", &from, &to); }
        return (from >= 0 && int(slot) == from) ? uint32_t(to) : slot;
    }

    unsigned (*movieTexture)() = nullptr;   // textura do filme (texId 999); 0 = sem quadro. Quem toca o filme preenche (wotm_live.inc)
    // quadro anterior (texId 0), render texture de cabeca para baixo com a tela de 640x224 do GS inteira; 0 = nao ha. Preenche: wotm_live.inc
    unsigned (*screenTexture)() = nullptr;
    unsigned screenId_ = 0;   // ultimo id devolvido para kScreenKey (o desenho converte o UV dele)

    unsigned textureId(const Ram &ram, const DrawItem &it, uint32_t slot) {
        if (texKey(ram, it, slot) == kScreenKey) return screenId_ = screenTexture ? screenTexture() : 0;
        return textureId(ram, it.node, slot);
    }
    unsigned textureId(const Ram &ram, uint32_t node, uint32_t slot = 0) {
        // filme do menu: moviesSetScreenObject poe texId 999 no objeto (wotm_fmv.hpp); a textura vem do tocador, quadro a quadro
        if (movieTexture && objectTexId(ram, node, slot) == 999)
            if (const unsigned id = movieTexture()) return id;
        const uint64_t key = texKey(ram, node, slot);
        if (!key) return 0;
        if (key == kScreenKey) return screenId_ = screenTexture ? screenTexture() : 0;
        auto it = tex_.find(key);
        if (it != tex_.end()) return it->second.id;
        Texture2D t{};
        std::vector<uint32_t> px;
        uint32_t w = 0, h = 0;
        if (decodeTex(ram, node, px, w, h, slot, true)) {   // alfa cru do GS: o shader do draw mistura As = At * Av / 16384
            if (std::getenv("WOTM_ALPHASTAT")) {
                size_t a0 = 0, am = 0;
                for (uint32_t c : px) { const uint32_t a = c >> 24; if (a == 0) ++a0; else if (a < 255) ++am; }
                if (a0 || am) std::fprintf(stderr, "alpha tex=%u %ux%u transparente=%.0f%% parcial=%.0f%%\n", ram.u16(node + 0x50), w, h, 100.0 * a0 / px.size(), 100.0 * am / px.size());
            }
            {
                // Texels com alfa 0 guardam uma cor qualquer (escura); o filtro bilinear a mistura nas bordas e faz uma franja.
                // Espalha a cor dos vizinhos visiveis para dentro dos transparentes (2 passos de 3x3).
                size_t partial = 0, clear = 0;
                // alfa cru: semitransparente = abaixo de ~1,0 com o vertice em 0x80 (0x7C); 0x80..0xFF e opaco
                for (uint32_t c : px) { const uint32_t a = c >> 24; if (a == 0) ++clear; else if (a < 0x7C) ++partial; }
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
        lights_ = readParaLights(ram);
        fog_ = readFog(ram);
        items.clear();
        sky.clear();
        overlay.clear();
        hud.clear();
        hp.clear();
        hpOverlay.clear();
        sprites.clear();
        atMat_ = 0;
        animVal_ = 0;
        cs_ = 0;
        blend_ = kGsAlphaNormal;
        aref_ = kGsAlphaRef;
        zmsk_ = false;
        lodX_ = false;
        spec_ = 0.f;
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
        hpList_ = true;
        csList(ram, addr::csHPActiveList);
        hpList_ = false;
        fx.wide = wide;
        fx.collect(ram);          // particulas com cor/textura/mistura do retail (wotm_fx.hpp); destroços (13) ficam com collectParticles
        collectShadows(ram);
        collectParticles(ram);
        static const bool ds = std::getenv("WOTM_DESTRSTAT") != nullptr;
        if (ds) { std::fprintf(stderr, "[destr] estados:"); for (int i = 0; i < 8; ++i) std::fprintf(stderr, " %u", destrHist_[i]); std::fprintf(stderr, "\n"); }
    }

    // Copia o estado atual da cena (depois de collect) para um instantaneo: itens, ceu, camera e as paletas de matrizes dos skinned
    // (a RAM e reescrita a cada quadro do jogo; o instantaneo precisa guardar os valores).
    Snapshot takeSnapshot(const Ram &ram, uint32_t g) {
        Snapshot sn;
        sn.items = items;
        sn.sky = sky;
        sn.overlay = overlay;
        sn.hud = hud;
        sn.hp = hp;
        sn.hpOverlay = hpOverlay;
        sn.sprites = sprites;
        sn.fx = fx.systems;
        sn.flares = fx.flares;
        sn.shadows = shadows;
        sn.lights = lights_;
        sn.fog = fog_;
        sn.cam = readGameCamera(ram, 0);
        readScreenPolys(ram, sn.screenPolys);
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

    // Desenha o instantaneo do ultimo quadro do jogo como esta (sem interpolar: a logica roda a 60/s) e devolve a sua camera.
    GameCamera show(const Snapshot &s) {
        items = s.items;
        sky = s.sky;
        overlay = s.overlay;
        hud = s.hud;
        hp = s.hp;
        hpOverlay = s.hpOverlay;
        sprites = s.sprites;
        fx.systems = s.fx;
        fx.flares = s.flares;
        shadows = s.shadows;
        screenRects = s.screenRects;
        screenPolys = s.screenPolys;
        lights_ = s.lights;
        fog_ = s.fog;
        cam_ = s.cam;
        eyeOk_ = s.cam.ok;
        for (int i = 0; i < 3; ++i) eye_[i] = s.cam.pos[i];
        return s.cam;
    }

    // ---- Particulas (fx[], 0x5BA280: 98 x 0x2EE0 bytes; o VU1 monta os quadrados, aqui viram sprites voltados para a camera) ----
    // Por efeito: vis.attribs & 0xFF = tipo (3 sprite, 4 texsprite, 13 destroço, 14 pedestre, 15 poeira...), numParticles +0x2ED0, pos[] (+0x2EB0,
    // 16 bytes: xyz mundo e w = rotacao nos destroços), age[] (+0x2EB8, int16), info.lifetime (+0x2E4A), tamanho min/max (+0x2E38/+0x2E3C),
    // texLoad[0] (+0x2C40) = indice em texInfo e TEX0ptr (+0x2EBC) = TEX0 (psm, CBP da paleta). So destroços (13), com o atlas FragTex em
    // celulas de 8x8; destroço sem atlas vira circulo suave. Os outros tipos (inclusive a poeira composta, dustTexture) ficam com `fx`.
    void collectParticles(const Ram &ram) {
        static const bool off = std::getenv("WOTM_NOFX") != nullptr;   // diagnostico: sem particulas
        if (off) return;
        constexpr uint32_t kFx = wotm::sym::fx, kStride = 12000u;
        FxView fog{};   // o VU1 descarta a particula toda na neblina (F < 1; Particles::setFog)
        float row3[4];
        for (int k = 0; k < 4; ++k) row3[k] = ram.f32(addr::worldToScreenMat + 48 + 4 * k);
        Particles::setFog(ram, fog, row3, eye_);
        for (uint32_t i = 1; i < fxaddr::count && sprites.size() < 6000; ++i) {   // fx[100]: o VU1 desenha destroços em fx[98] e fx[99]
            const uint32_t b = kFx + i * kStride;
            const uint32_t np = ram.u32(b + 0x2ED0);
            const uint32_t type = ram.u32(b + 0x2E90) & 0xFF;
            if (!np || np > 1024 || type != 13 || !Particles::inViewFov(ram, b, wide)) continue;   // so destroços; os outros tipos ficam com fx (wotm_fx.hpp: cor, textura e mistura do retail)
            const uint32_t pos = ram.u32(b + 0x2EB0), age = ram.u32(b + 0x2EB8);
            if (!ram.ok(pos, 16 * np) || !ram.ok(age, 2 * np)) continue;
            const int life = int(int16_t(ram.u16(b + 0x2E30 + 0x1A)));
            const float minS = ram.f32(b + 0x2E30 + 8), maxS = ram.f32(b + 0x2E30 + 0xC);
            const uint32_t texLoad = ram.u16(b + 0x2C40);
            const uint32_t t0p = ram.u32(b + 0x2EBC);
            uint64_t tex0 = 0;
            if (ram.ok(t0p, 8)) tex0 = uint64_t(ram.u32(t0p)) | (uint64_t(ram.u32(t0p + 4)) << 32);
            // fragPacketInit: TextureInfo[+0x2E98] da o atlas (largura e altura em 1/16 de texel; celulas de 8x8 = 0x80), uma
            // cor RGBAQ por sistema e o contexto (alphaContext 0: TEST_1 0x5040B, alfa >= 0x40, e ZBUF gravando)
            const uint32_t ti = wotm::sym::TextureInfo + 8 * (ram.u32(b + 0x2E98) & 127);
            const uint32_t tw16 = ram.u16(ti + 4), th16 = ram.u16(ti + 6);
            const uint32_t cols = tw16 >> 7, rows = th16 >> 7;
            const bool atlas = type == 13 && texLoad != 0xFFFF && tex0 && cols && rows;
            uint32_t gsCol = 0x80808080u;
            if (ram.u32(b + 0x2E30) & 0x200) {   // cor em floats (0..255; alfa 0..1)
                const uint32_t cp = ram.u32(b + 0x2E58);
                if (ram.ok(cp, 16)) {
                    auto ub = [](float v) { return uint32_t(std::clamp(v, 0.f, 255.f)); };
                    gsCol = ub(ram.f32(cp)) | ub(ram.f32(cp + 4)) << 8 | ub(ram.f32(cp + 8)) << 16 | ub(ram.f32(cp + 12) * 128.f) << 24;
                }
            } else {
                gsCol = (ram.u32(fxaddr::colors + 4 * ram.u32(b + 0x2EA0)) & 0x00FFFFFFu) | 0x80000000u;
            }
            const bool opaque = ram.u32(b + 0x2EA8) == 0;
            const float counter = float(int32_t(ram.u32(b + 0x2DC0)));   // fragPacketBuild: +4 por quadro, vai ao q330.y
            for (uint32_t k = 0; k < np; ++k) {
                Sprite s;
                for (int c = 0; c < 3; ++c) s.pos[c] = ram.f32(pos + 16 * k + 4 * c);
                if (!std::isfinite(s.pos[0]) || !std::isfinite(s.pos[1]) || !std::isfinite(s.pos[2])) continue;
                if (Particles::foggedAt(fog, s.pos)) continue;
                const int a = int(int16_t(ram.u16(age + 2 * k)));
                if (a < 0 || (life > 0 && a >= life)) continue;
                const float t = life > 0 ? float(a) / float(life) : 0.f;
                s.key = i * 4096u + k;
                if (atlas) {   // destroço: SPRITE do GS alinhado a tela (fragPacketInit/fragPacketBuild, microprograma 0x1180)
                    // pos.w (particleEmit, is13): float em [-2, 2) que o particleMove faz variar com vel.w, com os bits baixos sorteados.
                    //  - lado = max(minSize, |w| maxSize / 2) (q330 = (minSize, contador, 128, maxSize / 2)), sem giro;
                    //  - entrada de UV = bits(w) & (linhas * 4 - 1) (w da tag em q328): 4 por linha do atlas (normal, espelha V,
                    //    espelha U, os dois), u0 = 0x08, u1 = 0x88 em 1/16 de texel, v = 0x80 * linha + idem;
                    //  - coluna: FTOI4(contador * w) & ((colunas - 1) * 0x80) somado aos dois U (q329.w): o destroço passa pelas
                    //    celulas da linha conforme o contador anda (4 por quadro);
                    //  - PRIM 0x136 (SPRITE, TME, FGE, FST, sem ABE): opaco, com a cor do sistema; some quando o particleMove o tira.
                    const float w = ram.f32(pos + 16 * k + 12);
                    const uint32_t e = ram.u32(pos + 16 * k + 12) & (rows * 4 - 1);
                    const int32_t colOff = int32_t(counter * w * 16.f) & int32_t((cols - 1) * 0x80);
                    const uint32_t vRow = 0x80 * (e >> 2);
                    const uint32_t lo = 0x08, hi = 0x88;
                    const bool fu = (e & 3) == 2 || (e & 3) == 3, fv = (e & 3) == 1 || (e & 3) == 3;
                    const float uA = float((fu ? hi : lo) + colOff), uB = float((fu ? lo : hi) + colOff);
                    const float vA = float((fv ? hi : lo) + vRow), vB = float((fv ? lo : hi) + vRow);
                    s.uv[0] = uA / float(tw16); s.uv[1] = vA / float(th16); s.uv[2] = uB / float(tw16); s.uv[3] = vB / float(th16);
                    s.texId = texLoad; s.tex0 = tex0;
                    s.size = std::max(minS, std::fabs(w) * maxS * 0.5f);
                    s.rot = 0.f;
                    s.gsCol = gsCol;
                    s.opaque = opaque;
                    s.aref = opaque ? int16_t(0x40 - 1) : kGsAlphaRef;   // TEST_1 0x5040B: GEQUAL 0x40
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

    // Camera para os ObjSprite: R/U = eixos direita/cima da camera do raylib (a mesma do desenho), M = worldToScreenMat, focos fx/fy.
    struct SpriteView {
        bool ok = false;
        float R[3], U[3], eye[3], M[4][4], fx = 0.f, fy = 0.f;
        static SpriteView make(const Ram &ram, const float eye[3]) {
            SpriteView v;
            const Matrix mv = rlGetMatrixModelview();
            const float R[3] = {mv.m0, mv.m4, mv.m8}, U[3] = {mv.m1, mv.m5, mv.m9};
            for (int a = 0; a < 3; ++a) { v.R[a] = R[a]; v.U[a] = U[a]; v.eye[a] = eye[a]; }
            for (int r = 0; r < 4; ++r)
                for (int k = 0; k < 4; ++k) v.M[r][k] = ram.f32(addr::worldToScreenMat + 16 * r + 4 * k);
            v.ok = Particles::screenFoci(v.M, v.fx, v.fy);
            return v;
        }
    };

    // Cantos do ObjSprite no mundo como o microprograma 0x2E58: centro c = m * (x, y, z), W = linha 3 de M * (c - olho), cantos
    // c -/+ (512, 256) * lado / W pixels do GS. No plano de vista do centro (mesmo W), o deslocamento e 512 * lado / fx ao longo de R e
    // 256 * lado / fy ao longo de -U (o y do GS desce). Ordem: (-, -), (+, -), (+, +), (-, +) do GS. Falso com W < 1 (o CLIP descarta).
    static bool objSpriteCorners(const SpriteView &v, const M4 &m, const ObjSprite &s, float c[4][3]) {
        if (!v.ok) return false;
        float p[3];
        xform(m, s.x, s.y, s.z, p);
        float w = v.M[3][3];
        for (int a = 0; a < 3; ++a) w += v.M[3][a] * (p[a] - v.eye[a]);
        if (!(w >= 1.f)) return false;
        const float hx = 512.f * s.size / v.fx, hy = 256.f * s.size / v.fy;
        const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
        for (int k = 0; k < 4; ++k)
            for (int a = 0; a < 3; ++a) c[k][a] = p[a] + v.R[a] * hx * sx[k] - v.U[a] * hy * sy[k];
        return true;
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
        shaderNormal(0.f, true, true);   // neblina (FGE) e alfa do GS no shader do draw, como o XGKICK dos destroços
        unsigned cur = ~0u;
        bool open = false, curOpaque = false;
        int16_t curRef = kGsAlphaRef;
        gsBlend(kGsAlphaNormal);
        for (const auto &o : order) {
            const Sprite &s = sprites[o.second];
            int tw = 0, th = 0;
            const unsigned id = fxTexture(ram, s.texId, s.tex0, tw, th);
            if (!id) continue;
            if (id != cur || !open || s.opaque != curOpaque || s.aref != curRef) {
                if (open) { rlEnd(); rlDrawRenderBatchActive(); }
                if (s.opaque != curOpaque) {   // destroço opaco: sem ABE (PRIM 0x136) e gravando profundidade
                    gsBlend(s.opaque ? kGsNoAbe : kGsAlphaNormal);
                    if (s.opaque) rlEnableDepthMask(); else rlDisableDepthMask();
                    curOpaque = s.opaque;
                }
                if (s.aref != curRef) { setAlphaRef(s.aref); curRef = s.aref; }
                rlBegin(RL_TRIANGLES);
                rlSetTexture(id);
                cur = id; open = true;
            }
            const float u0 = s.uv[0], v0 = s.uv[1], u1 = s.uv[2], v1 = s.uv[3];
            // cor e alfa no modo GS do shader: 0x80 = 1,0 (a textura traz o alfa cru)
            auto ch = [](float v) { return uint8_t(128.f * std::max(0.f, std::min(1.f, v))); };
            unsigned char cr = ch(s.rgba[0]), cg = ch(s.rgba[1]), cb = ch(s.rgba[2]), ca = ch(s.rgba[3]);
            if (s.gsCol) { cr = uint8_t(s.gsCol); cg = uint8_t(s.gsCol >> 8); cb = uint8_t(s.gsCol >> 16); ca = uint8_t(s.gsCol >> 24); }
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
        if (curRef != kGsAlphaRef) setAlphaRef(kGsAlphaRef);
        gsBlend(kGsAlphaNormal);
        rlNormal3f(0.f, 0.f, 0.f);
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
            const UploadHdr hd = UploadHdr::read(ram, ptr);
            const uint32_t w = hd.w, h = hd.h;
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
                         (unsigned long long)t0v, t0.cbp, t0.csa, ram.u32(addr::tempVramTexAddr) >> 6, UploadHdr::read(ram, ptr).psm, ptr,
                         int(rtxPal.count(t0.cbp)));
        }
    }

    const MeshData *mesh(const Ram &ram, uint32_t node) {
        const uint32_t pp = ram.u32(node + 4) & 0x0FFFFFFF;
        auto it = cache_.find(pp);
        if (it != cache_.end()) {
            // revalida a assinatura (a RAM do nivel pode ter sido reaproveitada)
            if (it->second->sig == packetSig(ram, skipZeroQwords(ram, pp))) return it->second.get();
        }
        auto md = std::make_shared<MeshData>(decodeObject(ram, node));
        cache_[pp] = md;
        return md.get();
    }

    // Limiar do teste de alfa no shader do draw (DrawItem::aref, unidades do alfa do GS: 0x80 = 1,0). Descarrega o lote antes: o
    // uniform vale para o que for desenhado depois.
    void setAlphaRef(int16_t r) {
        if (alphaRefLoc_ < 0) return;
        rlDrawRenderBatchActive();
        const float f = float(r);
        SetShaderValue(alphaShader_, alphaRefLoc_, &f, SHADER_UNIFORM_FLOAT);
    }

    // Desenha com o raylib (deve estar dentro de BeginMode3D).
    // Liga o shader do GS (neblina, alfa) para uma passada de objetos; devolve se ligou (quem chama faz EndShaderMode e zera
    // g_premulShader no fim).
    bool beginShader(const Ram &ram, bool wire) {
        // Teste de alfa: pixels transparentes (sombras, helices, grades) nao podem escrever profundidade nem cor.
        // A normal do vertice leva (fogBurnThru, 1, FGE + 2 * GS) (shaderNormal; o rlNormal3f normaliza, entao o shader le as razoes
        // por y); quem desenha a preenche e a zera no fim (normal 0 = sem neblina, alfa comum).
        //  - FGE: neblina (ver Fog).
        //  - GS: textura e vertice trazem cor e alfa crus do GS (0x80 = 1,0) e o shader modula como o GS (TFX MODULATE):
        //    cor = texel * Cv / 128 (Cv > 0x80 clareia) e As = min(At * Av / 128, 255) / 128 (ate 1,99: a funcao de textura satura
        //    em 255). Na mistura comum o As fica limitado a 1;
        //    sem o modo GS, o produto comum (sombras, depuracao).
        //  - PM: mistura aditiva/subtrativa (gsBlend 1 e 2); a saida e a cor vezes o As inteiro, e a mistura usa fator 1.
        //  - modo (+8, +16): os dois passes de GsBlendFactors::doubled (alfa As / 2; saida branca).
        if (!alphaShaderTried_) {
            alphaShaderTried_ = true;
            static const char *vs =
                "#version 330\n"
                "in vec3 vertexPosition; in vec2 vertexTexCoord; in vec3 vertexNormal; in vec4 vertexColor;\n"
                "uniform mat4 mvp; uniform vec3 fogEye; uniform vec4 fogRow3; uniform vec3 fogParams;\n"
                "out vec2 fragTexCoord; out vec4 fragColor; out float fragFog; out float fragGsA; out float fragPm; out float fragMode;\n"
                "void main() {\n"
                "  fragTexCoord = vertexTexCoord; fragColor = vertexColor;\n"
                // normal = (burn, 1, FGE + 2 * GS + 4 * PM) normalizada pelo rlNormal3f: as razoes por y sobrevivem; y = 0 = sem nada
                "  float burn = 0.0, fl = 0.0;\n"
                "  if (vertexNormal.y > 1e-6) { burn = vertexNormal.x / vertexNormal.y; fl = floor(vertexNormal.z / vertexNormal.y + 0.5); }\n"
                "  fragGsA = mod(floor(fl / 2.0), 2.0) > 0.5 ? 1.0 : 0.0;\n"
                "  fragPm = mod(floor(fl / 4.0), 2.0) > 0.5 ? 1.0 : 0.0;\n"
                "  fragMode = floor(fl / 8.0);\n"
                "  float w = dot(fogRow3.xyz, vertexPosition - fogEye) + fogRow3.w;\n"
                "  float f = 255.0;\n"
                "  if (mod(fl, 2.0) > 0.5 && w > 0.001) f = clamp(burn + fogParams.x + fogParams.z / w, 0.0, fogParams.y);\n"
                "  fragFog = f / 255.0;\n"
                "  gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
                "}\n";
            static const char *fs =
                "#version 330\n"
                "in vec2 fragTexCoord; in vec4 fragColor; in float fragFog; in float fragGsA; in float fragPm; in float fragMode;\n"
                "uniform sampler2D texture0; uniform vec4 colDiffuse; uniform vec3 fogColor; uniform float fogSplit; uniform float alphaRef; out vec4 finalColor;\n"
                "void main() { vec4 t = texture(texture0, fragTexCoord) * fragColor * colDiffuse;\n"
                // GS: cor = texel * cor do vertice / 128 (o vertice vem cru, 0x80 = 1,0; acima disso clareia), limitada a 255;
                // As = At * Av / 128, saturado em 255 pela funcao de textura (como a cor), / 128: ate 255/128
                "  float as = t.a;\n"
                "  if (fragGsA > 0.5) { t.rgb = min(t.rgb * 1.9922, vec3(1.0)); as = min(t.a * 3.9690, 1.9922); t.a = min(as, 1.0); }\n"
                // teste de alfa do GS (TEST_1 do GROUP, DrawItem::aref; padrao kGsAlphaRef = alfa > 8)
                "  if (as * 128.0 <= alphaRef) discard;\n"
                "  if (fragMode > 1.5) { finalColor = vec4(1.0); return; }\n"   // 2o passe do doubled (gsBlendDouble)

                "  if (fogSplit < -1.5) { finalColor = vec4(vec3(1.0 - fragFog), 1.0); return; }\n"   // WOTM_FOGSPLIT=2: mascara da neblina
                "  float f = gl_FragCoord.x < fogSplit ? 1.0 : fragFog;\n"
                "  vec3 c = mix(fogColor, t.rgb, f);\n"
                // aditivo/subtrativo: a cor ja vezes o As inteiro (a mistura usa fator 1; gsBlend)
                // doubled (modo 1): alfa As / 2, sem o limite de 1 (o 2o passe dobra o destino)
                "  finalColor = fragPm > 0.5 ? vec4(c * as, 1.0) : vec4(c, fragMode > 0.5 ? as * 0.5 : t.a); }\n";
            alphaShader_ = LoadShaderFromMemory(vs, fs);
            fogLoc_[0] = GetShaderLocation(alphaShader_, "fogEye");
            fogLoc_[1] = GetShaderLocation(alphaShader_, "fogRow3");
            fogLoc_[2] = GetShaderLocation(alphaShader_, "fogParams");
            fogLoc_[3] = GetShaderLocation(alphaShader_, "fogColor");
            fogLoc_[4] = GetShaderLocation(alphaShader_, "fogSplit");
            alphaRefLoc_ = GetShaderLocation(alphaShader_, "alphaRef");
            setAlphaRef(kGsAlphaRef);
        }
        const bool useShader = !wire && alphaShader_.id > 0;
        if (useShader) {
            float row3[4];
            for (int k = 0; k < 4; ++k) row3[k] = ram.f32(addr::worldToScreenMat + 48 + 4 * k);
            const float params[3] = {fog_.fa, fog_.maxVal, fog_.fb};
            if (fogLoc_[0] >= 0) SetShaderValue(alphaShader_, fogLoc_[0], eye_, SHADER_UNIFORM_VEC3);
            if (fogLoc_[1] >= 0) SetShaderValue(alphaShader_, fogLoc_[1], row3, SHADER_UNIFORM_VEC4);
            if (fogLoc_[2] >= 0) SetShaderValue(alphaShader_, fogLoc_[2], params, SHADER_UNIFORM_VEC3);
            if (fogLoc_[3] >= 0) SetShaderValue(alphaShader_, fogLoc_[3], fog_.col, SHADER_UNIFORM_VEC3);
            // diagnostico: WOTM_FOGSPLIT=1 tira a neblina da metade esquerda da tela (comparacao no mesmo quadro); =2 desenha a mascara
            // da neblina (branco = so FOGCOL) e imprime os parametros a cada ~2 s
            static const int split = std::getenv("WOTM_FOGSPLIT") ? std::atoi(std::getenv("WOTM_FOGSPLIT")) : 0;
            // =3: neblina so nos quadros pares (duas capturas seguidas = com e sem, quase a mesma cena)
            static uint32_t drawCount = 0;
            const bool oddFrame = (drawCount++ & 1) != 0;
            const float splitX = split == 1 ? 0.5f * float(rlGetFramebufferWidth()) : split == 2 ? -2.f
                               : (split == 3 && oddFrame) ? 1e9f : -1.f;
            static int fogLog = 0;
            if (split && fogLog++ % 120 == 0)
                std::fprintf(stderr, "[fog] FA %.3f FB %.1f max %.1f cor %.0f %.0f %.0f row3 %.3f %.3f %.3f %.3f\n", fog_.fa, fog_.fb, fog_.maxVal,
                             fog_.col[0] * 255.f, fog_.col[1] * 255.f, fog_.col[2] * 255.f, row3[0], row3[1], row3[2], row3[3]);
            if (fogLoc_[4] >= 0) SetShaderValue(alphaShader_, fogLoc_[4], &splitX, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(alphaShader_);
            g_premulShader = true;
        }
        return useShader;
    }

    void draw(const Ram &ram, bool wire = false) {
        const bool useShader = beginShader(ram, wire);
        rlDisableDepthTest();   // o ceu e desenhado primeiro, centrado no olho, sem profundidade
        drawList(ram, sky, wire);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
        static const bool onePass = std::getenv("WOTM_ONEPASS") != nullptr;   // diagnostico: tudo num passe so (sem separar translucidos)
        // Como o hier: fora da OT de alfa, opacos e translucidos juntos na ordem da travessia (gravando Z conforme o ZMSK do GROUP); a
        // OT de alfa (passe 1) depois. Com dois passes (opacos primeiro), a previa da fase (tex 102, As ~ 1) saia antes do logo do
        // telao (tex 140/141, sem gravar Z) e o logo ficava por cima; no GS a previa vem depois e o cobre.
        drawList(ram, items, wire, (wire || onePass) ? -1 : 2);
        if (!wire && !onePass) {
            rlDrawRenderBatchActive();
            rlDisableDepthMask();   // translucidos: o drawList grava profundidade item a item pelo ZBUF_1.ZMSK (DrawItem::zmsk)
            drawList(ram, items, wire, 1);
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
        }
        // sombras projetadas depois da OT de alfa: no XGKICK os receptores redesenhados com a textura da sombra vem depois dela
        if (!wire) drawShadows(ram);
        // particulas por ultimo (testam profundidade, nao escrevem): destroços (drawSprites), depois os demais tipos na ordem de particleDraw
        if (!wire) drawSprites(ram);
        if (!wire) fx.draw(ram, [&](uint32_t id, uint64_t t0) { return particleTexture(ram, id, t0); }, eye_);
        if (!wire) fx.drawFlares([&](uint32_t id, uint64_t t0) { return particleTexture(ram, id, t0); });   // lens flare: tela, depois das particulas
        rlDrawRenderBatchActive();
        rlDisableDepthTest();   // icones de botao e HUD por cima de tudo (z perto de 0: o teste de profundidade os esconde sob o chao)
        drawList(ram, overlay, wire, -1, true);
        drawList(ram, hud, wire, -1);   // HUD por ultimo, na ordem da travessia
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
        if (useShader) EndShaderMode();
        g_premulShader = false;
    }

    // Lista de alta prioridade (`hp`), com a camera 3D do quadro ligada: na frente da camera, por cima do que ja esta na tela, sem
    // profundidade (como os icones de botao).
    void drawHp(const Ram &ram, bool wire = false) {
        if (hp.empty() && hpOverlay.empty()) return;
        const bool useShader = beginShader(ram, wire);
        rlDisableDepthTest();
        drawList(ram, hp, wire, -1);
        drawList(ram, hpOverlay, wire, -1, true);
        rlDrawRenderBatchActive();
        rlEnableDepthTest();
        if (useShader) EndShaderMode();
        g_premulShader = false;
    }

    // Translucido: textura com alfa parcial ou vertices de alfa baixo. Esses vao no segundo passe (tras para frente, sem escrever profundidade).
    bool translucent(const Ram &ram, const DrawItem &it) {
        const MeshData *md = mesh(ram, it.node);
        if (it.fade < 1.f) return true;   // cross-fade de LOD
        if (md && md->lowAlpha) return true;
        if (md && !md->sprites.empty()) return true;   // ObjSprite: brilho (alphaSort no halo do power-up)
        if (md && md->abe && it.blend != kGsAlphaNormal) return true;   // mistura aditiva/escurecedora precisa do passe translucido
        for (uint32_t sl = 0; sl < 5; ++sl) {
            if (md && md->slotMask && !(md->slotMask & (1u << sl))) continue;
            const uint64_t k = texKey(ram, it, sl);
            if (!k) continue;
            textureId(ram, it, sl);
            const auto f = partial_.find(k);
            if (f != partial_.end() && f->second) return true;
        }
        return false;
    }

    // Faixa da OT de alfa (pktLinkAlphaObjAsm): z = vf30.x do programa 0x0 do VU0, a distancia do centro da esfera do no (+0x10) ao
    // plano 0 da vista (o de perto, fovNorms; em mundo, o mesmo de Particles::inViewVolume), no espaco do no (dividida pela escala).
    // z <= 8 -> faixa 1; senao int(min(z, 1016) / 8) + 1, ate 128. pktInitReverseAlphaOT encadeia da faixa 128 para a 0.
    // OBJECT com o bit 0x8000 das flags (+0xC): o hierTraverseAsm o liga na OT de alfa (pktLinkAlphaObjAsm), que sai depois de
    // todas as cadeias normais do hier; os demais saem na ordem da travessia.
    // Bit 0x4000: so dentro de um cross-fade de LOD (o hierTraverseAsm testa flags & 0xC000; sem o 0x8000, exige o peso do LOD
    // herdado > 0). No luta_hud as colunas da tex 234 saem no meio da OT (programa 1001 de 936-1214).
    static bool inAlphaOT(const Ram &ram, const DrawItem &it) {
        const uint16_t f = ram.u16(it.node + 0xC);
        return (f & 0x8000) || ((f & 0x4000) && it.lodX);
    }

    static int alphaBin(const Ram &ram, const DrawItem &it) {
        constexpr uint32_t w = wotm::sym::worldCtx;   // worldCtx[0]: plano 0 em +0xF0, limiar em +0x120
        const M4 &m = it.m;
        float c[3], s2 = 0.f, z = -ram.f32(w + 0xF0 + 48);
        xform(m, ram.f32(it.node + 0x10), ram.f32(it.node + 0x14), ram.f32(it.node + 0x18), c);
        for (int k = 0; k < 3; ++k) z += ram.f32(w + 0xF0 + 4 * k) * c[k];
        for (int i = 0; i < 3; ++i) s2 = std::max(s2, m.m[i][0] * m.m[i][0] + m.m[i][1] * m.m[i][1] + m.m[i][2] * m.m[i][2]);
        if (s2 > 1e-12f) z /= std::sqrt(s2);
        if (!(z > 8.f)) return 1;
        return int(std::min(z, 1016.f) * 0.125f) + 1;
    }

    // pass: -1 tudo; 0 so opacos/recortados; 1 so a OT de alfa (inAlphaOT; alphaBin, empate LIFO); 2 tudo fora da OT, na ordem da
    // travessia (lotes so entre itens seguidos com a mesma textura e o mesmo estado)
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
        constexpr uint32_t kSlotDouble = 0x80000000u;   // Ent::slot: 2o passe de GsBlendFactors::doubled (destino * 2)
        std::vector<Ent> order;   // (chave de textura, indice do item, slot de textura)
        order.reserve(list.size());
        const bool far2near = pass == 1 && !wire;   // OT de alfa: faixas e LIFO
        const bool trav = pass == 2 && !wire;       // fora da OT: ordem da travessia, como as cadeias normais do hier
        for (uint32_t i = 0; i < list.size(); ++i) {
            if (pass >= 0 && !wire) {
                const bool sel = pass == 0 ? !translucent(ram, list[i]) : pass == 1 ? inAlphaOT(ram, list[i]) : !inAlphaOT(ram, list[i]);
                if (!sel) continue;
            }
            {   // diagnostico: WOTM_ONLYTEX=<id[,id..]> desenha so esses texId (bisseccao de defeitos)
                static const char *only = std::getenv("WOTM_ONLYTEX");
                if (only) {   // com '-' na frente: desenha tudo menos esses
                    const bool skip = *only == '-';
                    const std::string s = std::string(",") + (skip ? only + 1 : only) + ",";
                    if ((s.find("," + std::to_string(ram.u16(list[i].node + 0x50)) + ",") == std::string::npos) != skip) continue;
                }
            }
            {   // diagnostico: WOTM_ONLYNODE=<hex[,hex..]> desenha so esses nos (isola um personagem do resto da cena)
                static const char *onlyN = std::getenv("WOTM_ONLYNODE");
                if (onlyN) {
                    char b[16]; std::snprintf(b, sizeof b, ",%x,", list[i].node);
                    if ((std::string(",") + onlyN + ",").find(b) == std::string::npos) continue;
                }
                static const char *skipN = std::getenv("WOTM_SKIPNODE");   // =<hex[,hex..]>: desenha tudo menos esses nos
                if (skipN) {
                    char b[16]; std::snprintf(b, sizeof b, ",%x,", list[i].node);
                    if ((std::string(",") + skipN + ",").find(b) != std::string::npos) continue;
                }
            }
            const MeshData *mdi = mesh(ram, list[i].node);
            {   // diagnostico: WOTM_LISTITEMS=1 lista os itens de cada passe (no, texId, zBufferFudge, translucido, posicao, triangulos)
                static const bool li = std::getenv("WOTM_LISTITEMS") != nullptr;
                if (li && !wire)
                    std::fprintf(stderr, "[item] pass %d no %x tex %u fz %.4f tris %zu pos %.1f %.1f %.1f blend %02x abe %d zmsk %d fade %.2f\n", pass, list[i].node,
                                 unsigned(ram.u16(list[i].node + 0x50)), ram.f32(list[i].node + 0x28), mdi ? mdi->tris.size() / 3 : size_t(0), list[i].m.m[3][0],
                                 list[i].m.m[3][1], list[i].m.m[3][2], list[i].blend, mdi ? int(mdi->abe) : -1, int(list[i].zmsk), list[i].fade);
            }
            const uint32_t smask = (mdi && mdi->slotMask) ? mdi->slotMask : 1u;
            for (uint32_t sl = 0; sl < 32; ++sl) {   // slots 0-4 e kSlotNoTex
            if (!(smask & (1u << sl))) continue;
            { static const int onlySl = std::getenv("WOTM_ONLYSLOT") ? std::atoi(std::getenv("WOTM_ONLYSLOT")) : -1; if (onlySl >= 0 && int(sl) != onlySl) continue; }   // diagnostico: so este slot de textura
            uint64_t key = wire ? 0 : texKey(ram, list[i], texRemap(sl));
            if (far2near) key = uint64_t(2 + 128 - alphaBin(ram, list[i]));   // faixa mais longe primeiro (nunca 0 nem 1)
            order.push_back({key, i, sl});
            if (!wire && mdi && mdi->abe && gsBlendFactors(list[i].blend).doubled) order.push_back({key, i, sl | kSlotDouble});   // 2o passe, logo depois
            }
        }
        // Passe translucido na ordem do retail: os objetos fora da OT de alfa saem nas cadeias normais, na ordem da travessia; depois
        // a OT (inAlphaOT), das faixas mais longe para as mais perto e, dentro da faixa, o ultimo da travessia primeiro
        // (pktLinkAlphaObjAsm insere no comeco da lista). Os slots de um item e o 2o passe ficam juntos.
        if (!trav)   // na travessia a ordem ja e a da lista (itens crescentes, slots do item juntos)
            std::stable_sort(order.begin(), order.end(), [far2near](const Ent &a, const Ent &b) {
                if (a.key != b.key) return a.key < b.key;
                return far2near && a.item > b.item;   // OT: o ultimo da travessia primeiro
            });
        {   // diagnostico: WOTM_ALPHAORDER=1 lista o passe translucido na ordem em que sai (no, texId, faixa da OT de alfa, ALPHA_1)
            static const bool ao = std::getenv("WOTM_ALPHAORDER") != nullptr;
            if (ao && far2near)
                for (const Ent &e : order)
                    std::fprintf(stderr, "[alpha] no %x tex %u faixa %d item %u slot %x blend %02x\n", list[e.item].node, unsigned(ram.u16(list[e.item].node + 0x50)),
                                 130 - int(e.key), e.item, e.slot, list[e.item].blend);
        }

        SpriteView sprView;   // eixos e focos para os ObjSprite (montado no primeiro item que tiver)
        EnvView envV;         // camera do UV do mapa de ambiente (idem, no primeiro item com DrawItem::env)
        const unsigned white = gsWhiteTexture();   // secoes sem textura (PRIM.TME 0): alfa 0x80 = 1,0 do GS
        uint64_t cur = ~0ull;
        bool curClamp = false;     // lote aberto com DrawItem::env 1 (textura em CLAMP)
        unsigned clampedId = 0;    // textura posta em CLAMP no lote aberto; volta a REPEAT depois do flush
        bool open = false;
        uint8_t curBlend = kGsAlphaNormal;   // DrawItem::blend ativo no GL (o lote anterior ja foi descarregado quando isto muda)
        bool curPremul = false;              // gsBlendPremul(curBlend): cor pre-multiplicada no shader (shaderNormal com pm)
        constexpr uint8_t kBlendDouble = 0xFF;   // curBlend no 2o passe do doubled (A = 3 nao existe: nunca e um ALPHA)
        bool curDbl = false;   // lote aberto e o 2o passe do doubled
        int curMode = 0;       // modo do shaderNormal no lote aberto (1 e 2: passes do doubled)
        uint8_t curItBlend = 0;
        int16_t curItRef = 0;   // estado do item do lote aberto (passe da travessia: lote so com o mesmo estado)
        bool curItZ = false, curItAbe = false;
        auto setBlend = [&](uint8_t b) {
            if (b == curBlend) return;
            curBlend = b;
            curPremul = gsBlendPremul(b);
            gsBlend(b);
        };
        int16_t curRef = kGsAlphaRef;   // DrawItem::aref no uniform do shader (idem)
        auto setRef = [&](int16_t r) {
            if (r == curRef) return;
            curRef = r;
            setAlphaRef(r);
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
            if (!md || !md->valid || (md->tris.empty() && md->sprites.empty())) continue;
            const bool dbl = (o.slot & kSlotDouble) != 0;
            const uint32_t slot = o.slot & ~kSlotDouble;
            const bool multi = md->slotMask & (md->slotMask - 1);   // mais de um slot usado
            const bool envClamp = it.env == 1 && !wire;   // mapa de ambiente comum: CLAMP_1 = 5 (as texturas de objeto repetem)
            // na travessia o lote continua so com a mesma textura e o mesmo estado do GS (mistura, teste de alfa, ZMSK)
            const bool travState = trav && open && (it.blend != curItBlend || it.aref != curItRef || it.zmsk != curItZ || md->abe != curItAbe);
            if (o.key != cur || !open || far2near || travState || envClamp != curClamp || dbl != curDbl) {
                curItBlend = it.blend; curItRef = it.aref; curItZ = it.zmsk; curItAbe = md->abe;
                if (open) { rlEnd(); rlDrawRenderBatchActive(); }   // um lote por textura (evita o alinhamento automatico do rlgl)
                if (clampedId) { rlTextureParameters(clampedId, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT); rlTextureParameters(clampedId, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_REPEAT); clampedId = 0; }
                if (!wire && dbl) { gsBlendDouble(); curBlend = kBlendDouble; curPremul = false; }
                // sem ABE o GS grava a cor da fonte sem misturar (o alfa, com o fade do LOD, so vale para o teste): o predio 133 do
                // luta_hud em cross-fade saia meio transparente e deixava ver o predio de tras
                else if (!wire) {
                    static const bool idcOpaque = std::getenv("WOTM_IDCOLOR") && std::atoi(std::getenv("WOTM_IDCOLOR")) == 2;
                    setBlend(md->abe && !idcOpaque ? it.blend : kGsNoAbe);
                }
                curDbl = dbl;
                curMode = dbl ? 2 : (!wire && md->abe && gsBlendFactors(it.blend).doubled) ? 1 : 0;
                if (!wire) setRef(it.aref);
                // passe translucido: grava profundidade como o GS, a menos que o GROUP ligue o ZMSK (o chamador volta a mascara ao fim)
                if (far2near || trav) { if (it.zmsk) rlDisableDepthMask(); else rlEnableDepthMask(); }
                cur = o.key;
                curClamp = envClamp;
                const unsigned id = wire ? 0 : textureId(ram, it, texRemap(slot));
                if (envClamp && id && id != screenId_) { rlTextureParameters(id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP); rlTextureParameters(id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP); clampedId = id; }
                rlBegin(wire ? RL_LINES : RL_TRIANGLES);   // rlBegin com outro modo reseta a textura do lote: setar depois
                static const bool idc = std::getenv("WOTM_IDCOLOR") != nullptr;   // diagnostico: cor = texId do objeto (leia o pixel para achar o id)
                rlSetTexture((id && !idc) ? id : white);
                open = true;
            }
            static const bool idcMode = std::getenv("WOTM_IDCOLOR") != nullptr;   // uma vez: getenv por vertice e lento no Windows
            static const bool idcNode = idcMode && std::atoi(std::getenv("WOTM_IDCOLOR")) == 2;   // =2: no >> 4 em RGB, sem mistura nem neblina
            const unsigned tid = (wire || !cur) ? 0 : textureId(ram, it, texRemap(slot));
            // alfa como o GS (textura com alfa cru, ver textureId), menos no quadro anterior (render texture) e no filme
            const bool gsA = !(tid && (tid == screenId_ || (movieTexture && objectTexId(ram, it.node, texRemap(slot)) == 999)));
            const auto &t = md->tris;
            {   // diagnostico: WOTM_TEXINFO=<id> imprime (ate WOTM_TEXINFO_N vezes, padrao 6) os objetos com esse texId: mistura, cor, fudge, textura e vertices
                static const int ti = std::getenv("WOTM_TEXINFO") ? std::atoi(std::getenv("WOTM_TEXINFO")) : -1;
                static int printed = 0;
                static const int tiMax = std::getenv("WOTM_TEXINFO_N") ? std::atoi(std::getenv("WOTM_TEXINFO_N")) : 6;
                if (printed < tiMax && ((ti >= 0 && int(objectTexId(ram, it.node, slot)) == ti) || (ti == -2 && !md->sprites.empty()) || (ti == -3 && it.blend != kGsAlphaNormal && !t.empty()) || (ti == -4 && it.aref == kGsAlphaNoTest))) {   // -2: todos os OBJECT sprite; -3: malhas fora da mistura comum; -4: sem teste de alfa
                    ++printed;
                    std::fprintf(stderr, "[texinfo] tex %u node %x blend %d abe %d color %.3f %.3f %.3f %.3f fade %.3f gsA %d key %llx tris %zu sprites %zu tex0 %llx aref %d\n", objectTexId(ram, it.node, slot), it.node, it.blend,
                                 int(md->abe), it.color[0], it.color[1], it.color[2], it.color[3], it.fade, int(gsA), (unsigned long long)o.key, t.size(),
                                 md->sprites.size(), (unsigned long long)objectTex0(ram, it.node, slot), int(it.aref));
                    std::fprintf(stderr, "[texinfo]   fudge %.4f burn %.3f flags %04x pos %.1f %.1f %.1f\n", ram.f32(it.node + 0x28), ram.f32(it.node + 0x2C),
                                 ram.u16(it.node + 0xC), it.m.m[3][0], it.m.m[3][1], it.m.m[3][2]);
                    std::vector<uint32_t> tp;
                    uint32_t tw = 0, th = 0;
                    if (decodeTex(ram, it.node, tp, tw, th, slot, true)) {
                        double s[4] = {0, 0, 0, 0};
                        uint32_t amin = 255, amax = 0;
                        for (uint32_t c : tp) { for (int k = 0; k < 4; ++k) s[k] += (c >> (8 * k)) & 255; amin = std::min(amin, c >> 24); amax = std::max(amax, c >> 24); }
                        std::fprintf(stderr, "[texinfo]   textura %ux%u media %.0f %.0f %.0f %.0f alfa %u..%u texKey %llx\n", tw, th, s[0] / tp.size(), s[1] / tp.size(),
                                     s[2] / tp.size(), s[3] / tp.size(), amin, amax, (unsigned long long)texKey(ram, it.node, slot));
                        if (const char *dir = std::getenv("WOTM_TEXINFO_PNG")) {   // grava <dir>/texinfo_<texKey>.png (alfa cru do GS, 0x80 = 1,0)
                            char name[1024];
                            std::snprintf(name, sizeof name, "%s/texinfo_%llx.png", dir, (unsigned long long)texKey(ram, it.node, slot));
                            Image im{tp.data(), int(tw), int(th), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                            ExportImage(im, name);
                        }
                        for (uint32_t y = 0; y < th; y += th / 8) {
                            std::fprintf(stderr, "[texinfo]   ");
                            for (uint32_t x = 0; x < tw; x += tw / 8) std::fprintf(stderr, "%08x ", tp[y * tw + x]);
                            std::fprintf(stderr, "\n");
                        }
                    }
                    for (size_t q = 0; q < t.size() && q < 6; ++q)
                        std::fprintf(stderr, "[texinfo]   v %.1f %.1f %.1f uv %.3f %.3f rgba %u %u %u %u fge %d\n", t[q].x, t[q].y, t[q].z, t[q].u, t[q].v,
                                     t[q].r, t[q].g, t[q].b, t[q].a, int(t[q].fge));
                    for (size_t q = 0; q < md->sprites.size() && q < 6; ++q) {   // OBJECT sprite (objType 2)
                        const ObjSprite &s = md->sprites[q];
                        std::fprintf(stderr, "[texinfo]   sprite uv %.3f %.3f - %.3f %.3f rgba %u %u %u %u fge %d\n", s.u0, s.v0, s.u1, s.v1,
                                     unsigned(s.r), unsigned(s.g), unsigned(s.b), unsigned(s.a), int(s.fge));
                    }
                }
            }
            static const bool forceOpaqueEnv = std::getenv("WOTM_OPAQUE") != nullptr;   // diagnostico: alfa de vertice ignorado em skinned
            const bool forceOpaque = forceOpaqueEnv && md->skinned;
            const float fade = (fadeByNode ? std::min(1.f, std::max(0.f, ram.f32(it.node + 0x4C))) : 1.f) * it.fade * std::max(0.f, it.color[3]);
            const ObjLight ol = objectLight(ram, it);
            const float fogBurn = ram.f32(it.node + 0x2C);   // fogBurnThru (somado ao F da neblina)
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
            const float fzScale = 1.f / fz;   // zBufferFudge > 1 = mais perto (no raio do olho)
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
                if (t[k].cull && !wire && backFacing(p, eye_)) continue;
                float eu[3], ev[3];   // UV do mapa de ambiente por vertice
                bool envOk[3] = {false, false, false};
                if (it.env && tid) {
                    if (!envV.ok && eyeOk_) {
                        float W[4][4];
                        for (int r = 0; r < 4; ++r)
                            for (int c = 0; c < 4; ++c) W[r][c] = ram.f32(addr::worldToScreenMat + 16 * r + 4 * c);
                        envV = envView(ram, W, eye_);
                    }
                    for (int c = 0; c < 3; ++c) {
                        float nw[3];
                        envNormal(t[k + c], pm ? &pm[(t[k + c].cx & 0x7F) / 4] : nullptr, pm ? skinM : it.m, nw);
                        envOk[c] = envUv(it.env, it.envK, envV, p[c], nw, eu[c], ev[c]);
                    }
                }
                for (int c = 0; c < 3; ++c) {
                    const int cs[2] = {c, (c + 1) % 3};
                    for (int e = 0; e < (wire ? 2 : 1); ++e) {
                        const Vtx &v = t[k + cs[e]];
                        bool gsV = gsA;   // modo GS do shader neste vertice (as cores de depuracao vao sem ele)
                        if (idcNode) { gsV = false; const uint32_t n = it.node >> 4; rlColor4ub(uint8_t(n), uint8_t(n >> 8), uint8_t(n >> 16), 255); }
                        else if (idcMode) { gsV = false; const uint32_t tx = objectTexId(ram, it.node, slot); rlColor4ub(uint8_t(tx & 255), uint8_t(tx >> 8), 128, 255); }
                        else if (v.isAdc) { gsV = false; rlColor4ub(255, 0, 0, 255); }   // WOTM_SHOWADC
                        else if (noVcol && md->skinned) { gsV = false; rlColor4ub(255, 255, 255, 255); }   // diagnostico
                        else {
                            uint8_t rgb[3];
                            vertexRgb(ol, it.color, pm ? &pm[(v.cx & 0x7F) / 4] : nullptr, pm ? skinM : it.m, v, rgb);
                            // cor e alfa crus no modo GS (o shader faz texel * Cv / 128 e At * Av / 16384); no quadro anterior e no filme,
                            // o antigo (cor dobrada e limitada a 1,0, alfa 4a/255)
                            const float va = v.a * fade * (gsA ? 1.f : 4.f);
                            const int k = gsA ? 1 : 2;
                            rlColor4ub(uint8_t(std::min(255, rgb[0] * k)), uint8_t(std::min(255, rgb[1] * k)), uint8_t(std::min(255, rgb[2] * k)), forceOpaque ? uint8_t(255) : uint8_t(std::min(255.f, va)));
                        }
                        const float tu = envOk[cs[e]] ? eu[cs[e]] : v.u + it.uvOff[0], tv = envOk[cs[e]] ? ev[cs[e]] : v.v + it.uvOff[1];
                        // texels do GS na tela 640x224, que em widescreen e a faixa 4:3 central do quadro anterior (gsScreenRect)
                        if (tid && tid == screenId_) rlTexCoord2f(0.5f - 0.5f / wide + tu * (1024.f / 640.f) / wide, 1.f - tv * (256.f / 224.f));
                        else if (tid && envOk[cs[e]]) rlTexCoord2f(tu, tv);
                        else if (tid) { static const char *un = std::getenv("WOTM_UVNEG"); const float su = (un && std::strchr(un, 'u')) ? -1.f : 1.f, sv = (un && std::strchr(un, 'v')) ? -1.f : 1.f; rlTexCoord2f(su * (v.u + it.uvOff[0]), sv * (v.v + it.uvOff[1])); }   // diagnostico: WOTM_UVNEG=u|v|uv inverte o sinal do UV
                        shaderNormal(fogBurn, v.fge && !idcNode, gsV, curPremul, curMode);   // neblina e modulacao do GS no shader do draw
                        rlVertex3f(p[cs[e]][0], p[cs[e]][1], p[cs[e]][2]);
                    }
                }
            }
            if (!md->sprites.empty() && !sprView.ok) sprView = SpriteView::make(ram, eye_);
            for (const ObjSprite &s : md->sprites) {
                if (multi && s.slot != slot) continue;
                float c[4][3];
                if (!objSpriteCorners(sprView, it.m, s, c)) continue;
                if (fudge) for (auto &q : c) for (int a = 0; a < 3; ++a) q[a] = eye_[a] + (q[a] - eye_[a]) * fzScale;
                const uint8_t cr = uint8_t(std::min(255.f, s.r * it.color[0])), cg = uint8_t(std::min(255.f, s.g * it.color[1])),
                              cb = uint8_t(std::min(255.f, s.b * it.color[2])), ca = uint8_t(std::min(255.f, s.a * fade));
                // ST como o GS (repete): desloca o par para >= 0, para nao depender do modo de repeticao da textura do raylib
                const float u0 = s.u0 + it.uvOff[0], u1 = s.u1 + it.uvOff[0], v0 = s.v0 + it.uvOff[1], v1 = s.v1 + it.uvOff[1];
                const float du = std::floor(std::min(u0, u1)), dv = std::floor(std::min(v0, v1));
                const float cu[4] = {u0 - du, u1 - du, u1 - du, u0 - du}, cv[4] = {v0 - dv, v0 - dv, v1 - dv, v1 - dv};
                static const int triIdx[6] = {0, 1, 2, 0, 2, 3}, edgeIdx[8] = {0, 1, 1, 2, 2, 3, 3, 0};
                const int *ix = wire ? edgeIdx : triIdx;
                for (int q = 0; q < (wire ? 8 : 6); ++q) {
                    const int k = ix[q];
                    rlColor4ub(cr, cg, cb, ca);
                    if (tid) rlTexCoord2f(cu[k], cv[k]);
                    shaderNormal(fogBurn, s.fge, gsA, curPremul, curMode);
                    rlVertex3f(c[k][0], c[k][1], c[k][2]);
                }
            }
        }
        if (open) { rlEnd(); rlDrawRenderBatchActive(); }   // descarrega o ultimo lote ANTES de religar o culling (o estado GL vale no flush)
        if (clampedId) { rlTextureParameters(clampedId, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT); rlTextureParameters(clampedId, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_REPEAT); }
        rlNormal3f(0.f, 0.f, 0.f);   // o que vier depois (sombras, particulas) sai sem neblina
        setBlend(kGsAlphaNormal);
        setRef(kGsAlphaRef);
        rlSetTexture(0);
        if (trav) rlEnableDepthMask();   // a travessia muda a mascara item a item; quem vem depois espera gravar
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

    // Desenha quadrados de tela (texto, HUD do ScreenPolys) em coordenadas da tela do jogo (640 x 224) escaladas para a faixa 4:3
    // central de `rw` x `rh` (fora do modo 3D, por cima da cena). Ordem do rtMain: screenPolys (ScreenPolys::dmaData) e depois text
    // (fontDmaFontData).
    void drawQuads(const std::vector<TextQuad> &quads, float rw, float rh) {
        if (quads.empty()) return;
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
        const GsScreenRect gr = gsScreenRect(rw, rh);
        const float sx = gr.w / 640.f, sy = gr.h / 224.f;
        if (textShader_.id > 0) BeginShaderMode(textShader_);
        for (const TextQuad &t : quads) {
            const Texture2D tex = residentTexture(t.tex0);
            if (!tex.id) continue;
            const bool gain = textShader_.id > 0;   // sem o shader a cor fica em <= 1,0 (dobra aqui)
            const int k = gain ? 1 : 2;
            const Color tint{uint8_t(std::min(255, t.r * k)), uint8_t(std::min(255, t.g * k)), uint8_t(std::min(255, t.b * k)), uint8_t(std::min(255, t.a * 2))};
            // UV invertido (u1 < u0, v1 < v0: o ScreenPolys espelha e o polyBuildPrim grava V = 0x80 - v): o GS anda de u0 para u1. O
            // DrawTexturePro com largura negativa espelha a faixa [x, x + |w|], entao a origem e o menor dos dois.
            DrawTexturePro(tex, Rectangle{std::min(t.u0, t.u1), std::min(t.v0, t.v1), t.u1 - t.u0, t.v1 - t.v0},
                           Rectangle{gr.x0 + t.x0 * sx, t.y0 * sy, (t.x1 - t.x0) * sx, (t.y1 - t.y0) * sy}, Vector2{0.f, 0.f}, 0.f, tint);
        }
        if (textShader_.id > 0) EndShaderMode();
    }

    // Fades na ordem do GS: underText = true antes do texto (ShellRtLoop::endField despacha a fila do gsPkt antes do
    // fontDmaFontData: o dialogo de tokens sai sobre a tela escura), false por cima de tudo (rtMain manda a fila depois do texto;
    // uiFadeScreen sai depois do fontDmaFontData; Shell::FadeScreen). Retangulo da largura da tela do jogo cobre o quadro inteiro (em
    // widescreen o fade escurece a janela toda); um menor fica na faixa 4:3 central, como o texto.
    void drawScreenRects(float rw, float rh, bool underText = false) const {
        const GsScreenRect gr = gsScreenRect(rw, rh);
        const float sy = rh / 224.f;
        for (const ScreenRect &r : screenRects) {
            if (r.underText != underText) continue;
            const uint8_t a = uint8_t(std::min(255, int(r.a) * 255 / 0x80));
            if (!a) continue;
            const bool full = r.x0 <= 0.f && r.x1 >= 640.f;
            const float x0 = full ? 0.f : gr.x0 + r.x0 * gr.w / 640.f, x1 = full ? rw : gr.x0 + r.x1 * gr.w / 640.f;
            DrawRectangleRec(Rectangle{x0, r.y0 * sy, x1 - x0, (r.y1 - r.y0) * sy}, Color{r.r, r.g, r.b, a});
        }
    }

private:
    std::unordered_map<uint64_t, Texture2D> fontTex_;   // texturas residentes de drawQuads por TEX0 (id 0 = nao decodificado)
    Shader textShader_{};
    bool textShaderTried_ = false;

    // Textura residente de um quadrado de tela (decodeResident): atlas da fonte (T8H 512x512 do .RTX da fase), icones do HUD (T4HL).
    Texture2D residentTexture(uint64_t tex0) {
        const auto it = fontTex_.find(tex0);
        if (it != fontTex_.end()) return it->second;
        Texture2D t{};
        std::vector<uint32_t> px;
        uint32_t pal[256] = {}, w = 0, h = 0;
        if (decodeResident(gs::Tex0::decode(tex0), px, w, h, false, pal)) {
            Image img{px.data(), int(w), int(h), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            t = LoadTextureFromImage(img);
            if (t.id) { GenTextureMipmaps(&t); SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR); SetTextureWrap(t, TEXTURE_WRAP_CLAMP); }
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
    int alphaRefLoc_ = -1;   // uniform do teste de alfa (DrawItem::aref) no alphaShader_
    int fogLoc_[5] = {-1, -1, -1, -1, -1};   // uniforms da neblina no alphaShader_: fogEye, fogRow3, fogParams, fogColor, fogSplit
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
    uint32_t cs_ = 0;      // _cs em desenho (drawCs; 0 no mundo e no ceu): colorQuad em objectColor
    int lodIdx_ = 0;       // LOD escolhido mais recentemente na descida
    int16_t aref_ = kGsAlphaRef;       // DrawItem::aref herdado do no mais proximo com ctx de TEST_1 (CtxScope)
    bool zmsk_ = false;                // DrawItem::zmsk herdado do no mais proximo com ctx de ZBUF_1 (CtxScope)
    uint8_t blend_ = kGsAlphaNormal;   // DrawItem::blend herdado do no mais proximo com ctx de ALPHA_1 (CtxScope)
    float fade_ = 1.f;     // DrawItem::fade (cross-fade de LOD do hierLod) na descida
    bool lodX_ = false;    // DrawItem::lodX na descida
    float spec_ = 0.f;     // DrawItem::spec: o qword 20 que esta na memoria do VU1 neste ponto da ordem do hier

    // O ctx de um GROUP (+4) e um pacote de estado do GS: tag A+D (NLOOP em +0x10, um registrador A+D) e os pares (dado, endereco) a
    // partir de +0x20; medidos: ALPHA_1 (0x42), TEST_1 (0x47) e ZBUF_1. Traduz para o estado dos filhos (-1 = ctx sem o registrador,
    // herda o do pai):
    //  - alpha: byte baixo do ALPHA_1 (A,B,C,D = bits 0-1, 2-3, 4-5, 6-7; ver gsBlend) para DrawItem::blend. Medidos: 0x44 comum, 0x48
    //    aditivo (Kineticlops), 0x42 escurece e muda o tom, 0x46 e 0x81 (Kineticlops em outra cor);
    //  - aref: limiar do teste de alfa do TEST_1 para DrawItem::aref. Sem ATE (bit 0): sem teste (kGsAlphaNoTest; o halo dos
    //    power-ups e o brilho dos postes, 0x5040C com alfa de vertice 6 a 38, perdiam o degrade no limiar 8 e viravam disco de borda
    //    dura) e ALWAYS (ATST 1) tambem. GREATER (6) passa com A > AREF (bits 4-11), GEQUAL (5) com A >= AREF; os outros ATST nao
    //    aparecem e ficam no padrao do shader (kGsAlphaRef).
    //    O 0x81 do Kineticlops vem com 0x5040D (A > 0x40): (Cd - Cs) As nao soma o fundo, e sem o teste o corpo inteiro sairia preto.
    //  - zmsk: ZBUF_1.ZMSK (bit 32) para DrawItem::zmsk. O logo do telao do drive-in (tex 140/141, GROUP com ZTST GEQUAL e ZMSK) fica
    //    atras do telao (e8da50, sem ctx: grava Z) e o GS o esconde; sem o Z do telao, o nativo o mostrava na selecao de fase.
    static constexpr int kRefAbsent = -1000;   // groupState: ctx sem TEST_1 (herda o aref do pai)
    static void groupState(const Ram &ram, uint32_t ctx, int &alpha, int &aref, int &zmsk) {
        alpha = zmsk = -1;
        aref = kRefAbsent;
        if (!ctx || !ram.ok(ctx, 0x20) || ram.u32(ctx + 0x14) != 0x10000000 || ram.u32(ctx + 0x18) != 0xE) return;
        const uint32_t n = std::min<uint32_t>(ram.u32(ctx + 0x10) & 0x7FFF, 8);
        if (!ram.ok(ctx, 0x20 + 16 * n)) return;
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t d = ram.u32(ctx + 0x20 + 16 * i), reg = ram.u32(ctx + 0x28 + 16 * i) & 0xFF;
            if (reg == 0x42) alpha = int(d & 0xFF);
            else if (reg == 0x47) {
                const uint32_t atst = (d >> 1) & 7, ref = (d >> 4) & 0xFF;
                static const bool oldAte = std::getenv("WOTM_ATEOLD") != nullptr;   // diagnostico: ATE 0 com o limiar padrao (antes de 2026-10-17)
                aref = (!(d & 1) || atst == 1) ? (oldAte ? kGsAlphaRef : kGsAlphaNoTest) : atst == 6 ? int(ref) : atst == 5 ? std::max(int(ref) - 1, 0) : kGsAlphaRef;
            } else if (reg == 0x4E) zmsk = int(ram.u32(ctx + 0x24 + 16 * i) & 1);
        }
    }

    // Estado do GS herdado na descida. O hierTraverseAsm troca o ctx atual ($s4, que comeca no ctxGetDefaultGSCtx) em todo no que
    // carrega um ctx nao nulo, nao so no GROUP: TRANSLATE, ROTATE e SCALE em +4; ANIM_XFORM e ANIM_SCALE em +8 (contagem em +4). Os
    // filhos herdam e o pai volta ao sair (o $s4 vai na pilha da travessia). A esfera da explosao (tex 146) esta abaixo de um desses
    // com ALPHA_1 0x48 (aditivo), sem GROUP com ctx; o nativo a desenhava com a mistura comum. O mesmo com o halo dos power-ups (tex 70):
    // saia como um disco escuro em vez de clarear.
    struct CtxScope {
        Scene &s;
        uint8_t blend;
        int16_t aref;
        bool zmsk;
        CtxScope(Scene &sc, const Ram &ram, uint32_t ctx) : s(sc), blend(sc.blend_), aref(sc.aref_), zmsk(sc.zmsk_) {
            int b, r, z;
            groupState(ram, ctx & 0x0FFFFFFF, b, r, z);
            if (b >= 0) s.blend_ = uint8_t(b);
            if (r != kRefAbsent) s.aref_ = int16_t(r);
            if (z >= 0) s.zmsk_ = z != 0;
        }
        ~CtxScope() { s.blend_ = blend; s.aref_ = aref; s.zmsk_ = zmsk; }
    };

    // Deslocamento do UV do OBJECT, como o hierTraverseAsm monta o qword 4 do cabecalho do VU1 (vf19.xy, que os programas de desenho somam
    // ao UV normalizado): uOffset/vOffset (+0x20/+0x24) e, com uvScroll (bit 0x800 das flags) dentro de um CHAR_INSTANCE, os canais de
    // animacao uOffIdx/vOffIdx (+0x30/+0x32; indice negativo = 0).
    void objectUvOffset(const Ram &ram, uint32_t node, float out[2]) const {
        out[0] = ram.f32(node + 0x20);
        out[1] = ram.f32(node + 0x24);
        if (!(ram.u16(node + 0xC) & 0x800) || !animVal_) return;
        for (int k = 0; k < 2; ++k) {
            const int16_t idx = ram.s16(node + 0x30 + 2 * k);
            out[k] = idx >= 0 ? ram.f32(animVal_ + 4 * uint32_t(idx)) : 0.f;
        }
    }

    // Multiplicador da cor do vertice do OBJECT, como o hierTraverseAsm monta o qword 5 do cabecalho do VU1 (vf31, que todos os programas
    // de desenho aplicam a cor do vertice): redAnim..alphaAnim (+0x40) ou, dentro de um CHAR_INSTANCE, os canais rAnimIdx..aAnimIdx
    // (+0x34; indice negativo = o do no), vezes o colorQuad (+0x70) do CS em desenho. O retail tambem multiplica o alfa pelo fade do
    // no (aqui em DrawItem::fade). Detalhes do asm (0x2085C8-0x2086D0):
    //  - o canal azul nunca vem da animacao: no lugar do lwc1 do valor animado ha um swc1 do blueAnim (fica o +0x48);
    //  - o colorQuad so entra com CsColor::isEnabled() (D_007B3040, copiado no scratchpad +0x8C pelo pktCacheForAsm);
    //  - o RGB vezes a cor ambiente animada (viewAnimAmbientRed/Green/Blue, 0x6F8AD0, = ambiente / 32 do viewSetAmbient, que o
    //    resetcom le do arquivo da fase) quando viewAmbientChanged (0x6F8ADC; scratchpad +0x160 e +0x84).
    void objectColor(const Ram &ram, uint32_t node, float out[4]) const {
        const bool quad = cs_ && ram.u32(0x007B3040) != 0;
        const bool amb = ram.u32(wotm::sym::viewAmbientChanged) != 0;
        for (int k = 0; k < 4; ++k) {
            out[k] = ram.f32(node + 0x40 + 4 * k);
            const int16_t idx = ram.s16(node + 0x34 + 2 * k);
            if (animVal_ && idx >= 0 && k != 2) out[k] = ram.f32(animVal_ + 4 * uint32_t(idx));
            if (quad) out[k] *= ram.f32(cs_ + 0x70 + 4 * k);
            if (amb && k < 3) out[k] *= ram.f32(wotm::sym::viewAnimAmbientRed + 4 * uint32_t(k));
            if (!std::isfinite(out[k])) out[k] = 1.f;
        }
    }

    // Mapa de ambiente: o pktAddVu1ObjAsm chama o pktBldEnvMapDma quando as flags (+0xC) tem envMapped (0x2), envMapIsVideo (0x4) ou
    // envMapCloakFx (0x8, que ele liga quando o _cs em desenho tem cloakMe, +0xAA). Ramo 2 (video, quadro anterior ou CLOAK) com
    // envMapIsVideo, texId[0] (+0x50) = 0 ou CLOAK; senao ramo 1. k = envMapWeight (+0x5C; 0 = 16); no CLOAK, cloakWeight (+0xAC) + j/64,
    // j = 3 - (count & 7) com count & 8, senao (count & 7) - 3 (count = contador de quadros 0x6F88E0). O CLOAK (env 3) tambem poe
    // ALPHA_1 0x44 (kGsAlphaNormal), troca o multiplicador de cor q5 por (2,59375; 2,59375; 2,59375; 1) e a textura de cada slot pela do
    // quadro anterior (texKey com DrawItem).
    void objectEnv(const Ram &ram, uint32_t node, DrawItem &it) const {
        const uint16_t f = ram.u16(node + 0xC);
        const bool cloak = cs_ && ram.s16(cs_ + 0xAA) != 0;
        if (!(f & 0x6) && !cloak) return;
        it.env = cloak ? 3 : ((f & 0x4) || ram.u16(node + 0x50) == 0) ? 2 : 1;
        it.envK = ram.f32(node + 0x5C);
        if (it.envK == 0.f) it.envK = 16.f;
        if (cloak) {
            const uint32_t n = ram.u32(wotm::sym::count_006F88E0);
            const int j = (n & 8) ? 3 - int(n & 7) : int(n & 7) - 3;
            it.envK = ram.f32(cs_ + 0xAC) + float(j) / 64.f;
            it.blend = kGsAlphaNormal;
            for (int c = 0; c < 3; ++c) it.color[c] = 2.59375f;
            it.color[3] = 1.f;
        }
    }

    // Paleta de matrizes de um objeto skinned: animPktPtr[packetPtrIdx] e uma cadeia DMA que alterna a paleta (tag ref 0x3000007d, bloco
    // de 125 qwords: 31 matrizes de 4 qwords a partir de +0x10) e os sub-pacotes da malha. O indice vem do OBJECT (+0x68, packetPtrIdx;
    // negativo = sem cadeia), como no pktAddVu1ObjAsm, e nao da posicao do filho no LOD: os dois coincidem no Congar e no Robo-47, mas
    // nos objetos do Raptros vale 5 e 6 com o LOD 2, e a paleta de outra parte do corpo quebrava a malha. Devolve 0 se nao achar.
    static uint32_t palette(const Ram &ram, const DrawItem &it) {
        const int32_t idx = int32_t(ram.u32(it.node + 0x68));
        if (!it.animPkt || idx < 0 || idx > 63) return 0;
        const uint32_t chain = ram.u32(it.animPkt + 4 * uint32_t(idx)) & 0x0FFFFFFF;
        if (!ram.ok(chain, 8) || ((ram.u32(chain) >> 28) & 7) != 3) return 0;
        const uint32_t block = ram.u32(chain + 4) & 0x0FFFFFFF;
        return ram.ok(block, 0x10 + 0x40 * 32) ? block : 0;
    }

    // Luz por vertice, como o programa do VU1 em 0x1010 (OBJECT com dirLight, bit 0x100 das flags em +0xC): com a normal crua n (modulo
    // ~127) girada pelo osso x (skinned) e pelo objeto, d_i = max(0, n.L_i / 4096) para as tres luzes paralelas e o vetor-meio do
    // especular (colunas de paraL, x32; o hier as leva ao espaco de cada objeto em lightDir) e
    //   cor = min(255, cor do vertice * (soma de cor_i * d_i) + especular * d_3^16)
    // com cor_i em vu1LightColor (D_0025B238, cor/128; qwords 17-19 do cabecalho) e especular = specIntensity (+0x6C) com specLightMe
    // (bit 0x2000), senao 0 (qword 20). O qword 5 do cabecalho, que tambem multiplica a cor do vertice, vale 1.
    static ParaLights readParaLights(const Ram &ram) {
        ParaLights l;
        for (int i = 0; i < 4; ++i)
            for (int a = 0; a < 3; ++a) l.dir[i][a] = ram.f32(addr::paraL + 16 * a + 4 * i);
        for (int i = 0; i < 3; ++i)
            for (int c = 0; c < 3; ++c) l.col[i][c] = ram.f32(addr::vu1LightColor + 16 * i + 4 * c);
        return l;
    }
    ParaLights lights_;   // luzes paralelas do ultimo collect (vertexRgb)

public:
    // Neblina, como os programas de desenho do VU1 e o GS: por vertice das secoes com PRIM.FGE,
    //   F = clamp(fogBurnThru do OBJECT (+0x2C) + fogFA + fogFB / W, 0, fogMaxVal)   (W = linha 3 do worldToScreenMat . (p - olho))
    // e o GS mistura (F * cor + (255 - F) * FOGCOL) / 255 (F = 255: sem neblina). Parametros de vu1FogParams; FOGCOL de fogColorCtx[4].
    static Fog readFog(const Ram &ram) {
        Fog f;
        f.fa = ram.f32(addr::vu1FogParams);
        f.maxVal = ram.f32(addr::vu1FogParams + 4);
        f.fb = ram.f32(addr::vu1FogParams + 12);
        const uint32_t c = ram.u32(addr::fogColorCtx + 0x20);
        for (int k = 0; k < 3; ++k) f.col[k] = float((c >> (8 * k)) & 0xFF) / 255.f;
        return f;
    }
    Fog fog_;   // do ultimo collect
    // F de um ponto (mundo) de uma secao com FGE; mesma conta do shader do draw. row3/eye: linha 3 do worldToScreenMat e olho.
    float fogF(const float p[3], float burn, const float row3[4], const float eye[3]) const {
        const float w = row3[0] * (p[0] - eye[0]) + row3[1] * (p[1] - eye[1]) + row3[2] * (p[2] - eye[2]) + row3[3];
        if (!(w > 1e-3f)) return fog_.maxVal;
        return std::min(fog_.maxVal, std::max(0.f, burn + fog_.fa + fog_.fb / w));
    }

    // UV do mapa de ambiente (DrawItem::env), como o pktBldEnvMapDma monta q997-q999 e o programa do VU1 (0x0A78; 0x0C08 com recorte)
    // calcula ST = R . n + (S + desloc) * escala, Q = 1. n = normal crua / 4096 (ITOF12; nos skinned, girada pelo osso x e truncada
    // como em vertexRgb) levada ao mundo pelas linhas da matriz do objeto (com a escala do _cs); e = (direita . n, baixo . n) com os
    // eixos do olho (weMat de worldCtx, iguais aos tirados da worldToScreenMat); S = (X, Y) do vertice na tela do GS. Com o ViewDef da
    // vista 0 (viewInfo[0].def: w, h em +4/+6 e offX/offY em +0x10/+0x14):
    //   ramo 1: s = 0,5 + k e.x + f (X - offX) / w,  t = 0,5 - k e.y - f (Y - offY) / h,  f = 1 - 0,05 |k|
    //   ramo 2: s = k e.x + (X - offX + w/2) / 1024,  t = k e.y + (Y - offY + h/2) / 256   (texels da tela 1024 x 256; sem o
    //           meio texel do campo impar, viewIsOddScan, nem os +320 das vistas > 0)
    struct EnvView { float W[4][4]; float eye[3]; float right[3], down[3]; float offX = 2048.f, offY = 2048.f, w = 640.f, h = 224.f; bool ok = false; };
    static EnvView envView(const Ram &ram, const float W[4][4], const float eye[3]) {
        EnvView ev;
        std::memcpy(ev.W, W, sizeof ev.W);
        std::memcpy(ev.eye, eye, sizeof ev.eye);
        float rl = 0.f, dl = 0.f;
        for (int a = 0; a < 3; ++a) {
            ev.right[a] = W[0][a] - 2048.f * W[3][a];
            ev.down[a] = W[1][a] - 2048.f * W[3][a];
            rl += ev.right[a] * ev.right[a];
            dl += ev.down[a] * ev.down[a];
        }
        if (!(rl > 0.f && dl > 0.f)) return ev;
        rl = std::sqrt(rl); dl = std::sqrt(dl);
        for (int a = 0; a < 3; ++a) { ev.right[a] /= rl; ev.down[a] /= dl; }
        const uint32_t def = ram.u32(0x006E1904) & 0x0FFFFFFF;   // viewInfo[0].def
        if (ram.ok(def, 0x18) && ram.u16(def + 4) && ram.u16(def + 6)) {
            ev.w = ram.u16(def + 4); ev.h = ram.u16(def + 6);
            ev.offX = ram.f32(def + 0x10); ev.offY = ram.f32(def + 0x14);
        }
        ev.ok = true;
        return ev;
    }
    // Normal do vertice no mundo para o envUv. bone: osso x (skinned) ou nulo; m: matriz do objeto.
    static void envNormal(const Vtx &v, const M4 *bone, const M4 &m, float out[3]) {
        float n[3] = {float(v.nx), float(v.ny), float(v.nz)};
        if (bone) {
            const float b[3] = {n[0], n[1], n[2]};
            for (int c = 0; c < 3; ++c) n[c] = std::trunc(b[0] * bone->m[0][c] + b[1] * bone->m[1][c] + b[2] * bone->m[2][c]);
        }
        for (int c = 0; c < 3; ++c) out[c] = (n[0] * m.m[0][c] + n[1] * m.m[1][c] + n[2] * m.m[2][c]) / 4096.f;
    }
    // Falso atras da camera (fica o UV do pacote).
    static bool envUv(uint8_t env, float k, const EnvView &ev, const float p[3], const float nw[3], float &u, float &v) {
        if (!env || !ev.ok) return false;
        const float d[3] = {p[0] - ev.eye[0], p[1] - ev.eye[1], p[2] - ev.eye[2]};
        const float q = ev.W[3][0] * d[0] + ev.W[3][1] * d[1] + ev.W[3][2] * d[2] + ev.W[3][3];
        if (!(q > 1e-3f)) return false;
        const float X = (ev.W[0][0] * d[0] + ev.W[0][1] * d[1] + ev.W[0][2] * d[2] + ev.W[0][3]) / q;
        const float Y = (ev.W[1][0] * d[0] + ev.W[1][1] * d[1] + ev.W[1][2] * d[2] + ev.W[1][3]) / q;
        const float ex = k * (ev.right[0] * nw[0] + ev.right[1] * nw[1] + ev.right[2] * nw[2]);
        const float ey = k * (ev.down[0] * nw[0] + ev.down[1] * nw[1] + ev.down[2] * nw[2]);
        if (env == 1) {
            const float f = 1.f - 0.05f * std::fabs(k);
            u = 0.5f + ex + f * (X - ev.offX) / ev.w;
            v = 0.5f - ey - f * (Y - ev.offY) / ev.h;
        } else {
            u = ex + (X - ev.offX + ev.w * 0.5f) / 1024.f;
            v = ey + (Y - ev.offY + ev.h * 0.5f) / 256.f;
        }
        return true;
    }
private:

    struct ObjLight { bool lit = false; float spec = 0.f; };
    // O programa do mapa de ambiente (DrawItem::env) nao aplica a luz paralela, mesmo com dirLight.
    static ObjLight objectLight(const Ram &ram, const DrawItem &it) {
        const uint16_t f = ram.u16(it.node + 0xC);
        if (it.env) return {};
        return {(f & 0x100) != 0, it.spec};
    }

    // Cor do vertice nas unidades do GS (0x80 = 1,0): a do pacote vezes o multiplicador do objeto (DrawItem::color, vf31) ou, nos
    // iluminados, a luz do VU1. bone: osso x do vertice (skinned) ou nulo.
    void vertexRgb(const ObjLight &ol, const float mul[4], const M4 *bone, const M4 &m, const Vtx &v, uint8_t out[3]) const {
        const float vc[3] = {v.r * mul[0], v.g * mul[1], v.b * mul[2]};
        if (!ol.lit) {
            for (int c = 0; c < 3; ++c) out[c] = uint8_t(std::min(255.f, std::max(0.f, vc[c])));
            return;
        }
        float n[3] = {float(v.nx), float(v.ny), float(v.nz)};
        if (bone) {   // o laco de skinning do VU1 (0x3D0) grava a normal girada com FTOI12 (trunca a 1/4096) e a luz le com ITOF12
            const float b[3] = {n[0], n[1], n[2]};
            for (int c = 0; c < 3; ++c) n[c] = std::trunc(b[0] * bone->m[0][c] + b[1] * bone->m[1][c] + b[2] * bone->m[2][c]);
        }
        float w[3] = {0, 0, 0};   // no mundo, com as linhas do objeto normalizadas (o hier normaliza as colunas de lightDir)
        for (int r = 0; r < 3; ++r) {
            const float len = std::sqrt(m.m[r][0] * m.m[r][0] + m.m[r][1] * m.m[r][1] + m.m[r][2] * m.m[r][2]);
            if (len > 1e-6f)
                for (int c = 0; c < 3; ++c) w[c] += n[r] * m.m[r][c] / len;
        }
        float d[4];
        for (int i = 0; i < 4; ++i) d[i] = std::max(0.f, (w[0] * lights_.dir[i][0] + w[1] * lights_.dir[i][1] + w[2] * lights_.dir[i][2]) / 4096.f);
        float s = d[3];
        for (int k = 0; k < 4; ++k) s *= s;   // ^16
        for (int c = 0; c < 3; ++c) {
            const float x = vc[c] * (lights_.col[0][c] * d[0] + lights_.col[1][c] * d[1] + lights_.col[2][c] * d[2]) + ol.spec * s;
            out[c] = uint8_t(std::min(255.f, std::max(0.f, x)));
        }
    }

    // Skinning, como o microcodigo do VU1 (desmontado com tools/vu1dis.py; laco em 0x3C8): tres ossos por vertice.
    // Os codigos de osso (8 bits baixos de x, y, z; & 0x7F = endereco de qword da matriz na paleta) escolhem 3 matrizes e os pesos
    // saem do float w: w1 = w, w2 = frac(w * 2048) (mantissa baixa), w3 = 1 - w1 - w2. pos = w1*Mx*p + w2*My*p + w3*Mz*p.
    // Slot da paleta = (codigo & 0x7F) / 4. A normal so gira pelo osso x (vertexRgb).
    static void skinVertexPM(const M4 *pm, const M4 &m, const Vtx &v, float out[3]) {
        float w1 = v.w, w2 = 0.f, w3 = 0.f;
        { const float s = v.w * 2048.f; w2 = s - std::trunc(s); w3 = 1.f - w1 - w2; }
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
        float w1 = v.w, w2 = 0.f, w3 = 0.f;
        {   // 3 ossos: w1 = w, w2 = frac(w * 2048), w3 = o resto (microcodigo do VU1)
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
    static constexpr uint32_t kLosPoints = wotm::sym::g_cameraLosPoints, kLosPerView = wotm::sym::g_cameraLosPointsPerView;

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
        const uint32_t c1 = ram.u32(node + 4), c2 = ram.u32(node + 8);
        if (!eyeOk_) return c1;
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

    // Corte por esfera do hierTraverseAsm: o programa 0x0 do VU0 testa a esfera do no (+0x10 centro, +0x1C raio) contra os planos
    // da vista (Particles::inViewVolume); fora de algum plano, o OBJECT nao vai ao VU1 (a nao ser com nofov, bit 0 de +0xC) e o GROUP
    // pula os filhos (com g_doFOVGroups e sem noFOV). O jogo testa no espaco do no com os planos de colunas normalizadas; em mundo o
    // raio vai pela maior escala da matriz.
    bool outOfView(const Ram &ram, uint32_t node, const M4 &m) const {
        if (!fovCull) return false;
        float c[3], s2 = 0.f;
        xform(m, ram.f32(node + 0x10), ram.f32(node + 0x14), ram.f32(node + 0x18), c);
        for (int i = 0; i < 3; ++i) s2 = std::max(s2, m.m[i][0] * m.m[i][0] + m.m[i][1] * m.m[i][1] + m.m[i][2] * m.m[i][2]);
        static constexpr float kNoBox[3] = {0.f, 0.f, 0.f};
        return !Particles::inViewVolume(ram, c, kNoBox, ram.f32(node + 0x1C) * std::sqrt(s2), wide);
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
        case 0:                                                                                                 // OBJECT
            if (!(ram.u16(node + 0xC) & 1) && outOfView(ram, node, m)) break;
            items.push_back({m, node, animPkt_, lodIdx_, nullptr, blend_, fade_});
            // O pktAddVu1ObjAsm so grava o qword 20 (especular, specIntensity +0x6C) com specLightMe (0x2000); sem ele o programa de
            // luz le o que o ultimo objeto deixou na memoria do VU1. As partes do Togera sem o bit (corpo skinned, flags 0x310) herdam
            // o especular da parte anterior do proprio monstro (checagem: ~10% dos vertices dele saiam mais escuros, so no cinza).
            if (ram.u16(node + 0xC) & 0x2000) spec_ = ram.f32(node + 0x6C);
            items.back().spec = spec_;
            items.back().aref = aref_;
            items.back().zmsk = zmsk_;
            items.back().lodX = lodX_;
            objectUvOffset(ram, node, items.back().uvOff);
            objectColor(ram, node, items.back().color);
            objectEnv(ram, node, items.back());
            break;
        case 1: {                                                                           // GROUP (ctx em +4 = estado do GS: ALPHA_1 e TEST_1 dos filhos)
            const uint16_t gflags = ram.u16(node + 0xA);                                    // visual (bit 0), noFOV (bit 2)
            if (!(gflags & 1)) break;
            if (!(gflags & 4) && ram.u32(addr::g_doFOVGroups) && outOfView(ram, node, m)) break;
            const CtxScope cs(*this, ram, ram.u32(node + 4));
            kids(ram, node + 0x20, ram.u16(node + 8), m, depth);
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
                const float savedFade = fade_;
                // Cross-fade (hierLod): passada a distancia `fade` (+0x20 + 16i, ao quadrado como switchOutDis), o jogo desenha o LOD i e
                // o i - 1 juntos, com t = 1 - (d - fade) / (switchOutDis - fade): t > 0,5 -> i inteiro e i - 1 com alfa (1 - t) * 2;
                // senao i - 1 inteiro e i com alfa 2t; sem i - 1, i com alfa t. Sem isso o nativo trocava de LOD de uma vez (checagem
                // VU1, pista 7: o VU1 desenhava uma instancia "a mais", o LOD vizinho no mesmo lugar).
                float fi = 1.f, fprev = 0.f;
                const float fd = lod_ < 0 && eyeOk_ ? ram.f32(node + 0x20 + 16 * i) : 0.f;
                const bool savedX = lodX_;
                if (fd > 0.f) {
                    float d2 = 0.f, cc[3];
                    xform(m, ram.f32(node + 0x10), ram.f32(node + 0x14), ram.f32(node + 0x18), cc);
                    for (int a = 0; a < 3; ++a) d2 += (cc[a] - eye_[a]) * (cc[a] - eye_[a]);
                    if (fd < d2) {
                        const float t = 1.f - (d2 - fd) / (ram.f32(node + 0x20 + 16 * i + 4) - fd);
                        if (i >= 1) { if (t > 0.5f) fprev = (1.f - t) * 2.f; else { fprev = 1.f; fi = t * 2.f; } }
                        else fi = t;
                        lodX_ = true;   // o hierTraverseAsm guarda o peso (!= 0) e manda os OBJECTs 0x4000 para a OT de alfa
                    }
                }
                // ordem do hierTraverseAsm: no cross-fade ele segue direto o filho do LOD i - 1 e empilha o do LOD i (com o peso),
                // que sai depois. Com os translucidos gravando Z na travessia, o LOD i quase transparente na frente fazia o i - 1
                // solido falhar no teste (luta_hud: predio d51400 sob o d51c40 de fade 0,21, o horizonte aparecia)
                if (fprev > 0.f) {
                    const uint32_t c2 = ram.u32(node + 0x20 + 16 * (i - 1) + 8);
                    lodIdx_ = int(i - 1);
                    fade_ = savedFade * fprev;
                    if (ram.ok(c2)) walk(ram, c2, m, depth + 1);
                }
                lodIdx_ = int(i);
                fade_ = savedFade * fi;
                if (ram.ok(c) && fade_ > 0.f) walk(ram, c, m, depth + 1);
                fade_ = savedFade;
                lodX_ = savedX;
                lodIdx_ = savedLod;
            }
            break;
        }
        case 3: {                                                                           // TRANSLATE
            const CtxScope cs(*this, ram, ram.u32(node + 4));
            kids(ram, node + 0x1C, ram.u32(node + 8),
                 mul(translateM(ram.f32(node + 0x10), ram.f32(node + 0x14), ram.f32(node + 0x18)), m), depth);
            break;
        }
        case 4: {                                                                           // ROTATE
            const CtxScope cs(*this, ram, ram.u32(node + 4));
            kids(ram, node + 0x50, ram.u32(node + 8), mul(readM4(ram, node + 0x10), m), depth);
            break;
        }
        case 30: {                                                                          // SCALE
            const CtxScope cs(*this, ram, ram.u32(node + 4));
            kids(ram, node + 0x2C, ram.u32(node + 8),
                 mul(mul(scaleM(ram.f32(node + 0x20), ram.f32(node + 0x24), ram.f32(node + 0x28)),
                         translateM(ram.f32(node + 0x10), ram.f32(node + 0x14), ram.f32(node + 0x18))), m), depth);
            break;
        }
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
            const CtxScope cs(*this, ram, ram.u32(node + 8));
            kids(ram, node + 0xCC, ram.u32(node + 4), mul(local, m), depth);
            break;
        }
        case 38: {                                                                          // ANIM_SCALE: escala e depois translacao
            {   // hierAnimScaleNode nao grava +0xC: a visibilidade e o canal val[vidx] da saida de animacao do CHAR_INSTANCE (-1 = sempre visivel)
                const int vidx = int16_t(ram.u16(node + 0x10));
                const float vis = (vidx >= 0 && animVal_ && vidx < 4096) ? ram.f32(animVal_ + 4 * uint32_t(vidx)) : (vidx >= 0 ? ram.f32(node + 0xC) : 1.f);
                if (vis == 0.f) break;
            }
            // hierAnimScaleNode: o olho vai ao espaco do filho por (eo + translate + postScale) / scale + preScale, ou seja, o ponto do
            // filho p vai ao pai em scale * (p + preScale) + translate + postScale (preScale +0x40 e postScale +0x50; ignora-los
            // deslocava itens presos aos monstros, checagem VU1 pista 7).
            float sx = ram.f32(node + 0x20), sy = ram.f32(node + 0x24), sz = ram.f32(node + 0x28);
            if (sx == 0.f && sy == 0.f && sz == 0.f) sx = sy = sz = 1.f;
            const M4 pre = translateM(ram.f32(node + 0x40), ram.f32(node + 0x44), ram.f32(node + 0x48));
            const M4 post = translateM(ram.f32(node + 0x30) + ram.f32(node + 0x50), ram.f32(node + 0x34) + ram.f32(node + 0x54),
                                       ram.f32(node + 0x38) + ram.f32(node + 0x58));
            const CtxScope cs(*this, ram, ram.u32(node + 8));
            kids(ram, node + 0x60, ram.u32(node + 4), mul(mul(mul(pre, scaleM(sx, sy, sz)), post), m), depth);
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
                ++destrHist_[std::min<uint32_t>(w, 7)];
                if (w >= n) w = 0;
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

    bool hpList_ = false;   // collect percorrendo CsPool::m_HPActiveList (drawCs manda para `hp`)
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
        // id do objeto (14 bits altos); 0x1C20..0x2133 = elementos de HUD. O Hud::update poe o _cs do HUD a cada quadro na frente da camera
        // (trans = olho + R * gHudPos, mat = rotacao da vista), entao ele projeta como o resto; vai para `hud`, desenhado por cima de tudo.
        const uint32_t id = ram.u32(ep) >> 18;
        const bool isHud = needDrawMe && id >= 0x1C20 && id <= 0x2133 && !isButtonId(id);
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
        const uint32_t savedCs = cs_;
        cs_ = cs;
        walk(ram, ep, m, 0);
        cs_ = savedCs;
        if (hpList_ && needDrawMe && !isHud) {   // lista de alta prioridade: passada propria (ver `hp`)
            std::vector<DrawItem> &dst = isButtonId(id) ? hpOverlay : hp;
            dst.insert(dst.end(), std::make_move_iterator(items.begin() + before), std::make_move_iterator(items.end()));
            items.resize(before);
        } else if (needDrawMe && isButtonId(id)) {   // icones de botao: por cima da cena, sem profundidade (ver `overlay`)
            overlay.insert(overlay.end(), std::make_move_iterator(items.begin() + before), std::make_move_iterator(items.end()));
            items.resize(before);
        } else if (isHud) {   // HUD: por cima da cena (ver `hud`), com o id para a checagem
            for (size_t i = before; i < items.size(); ++i) items[i].hudId = uint16_t(id);
            hud.insert(hud.end(), std::make_move_iterator(items.begin() + before), std::make_move_iterator(items.end()));
            items.resize(before);
        }
    }
};

}  // namespace wotm
