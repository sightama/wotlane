// Particulas e efeitos (poeira, fogo, faiscas, explosoes, rastros) lidos da RAM do EE e desenhados com raylib.
//
// No retail, `particlePrepDraw` (particleMove + o *PacketBuild de cada tipo) monta um pacote VIF por sistema e `particleDraw`
// (0x21B378; o laco que espera o VU1 fica em 0x21B8A0) manda esses pacotes para o microcodigo de particulas do VU1, que expande
// cada particula em sprite/linha/faixa. Aqui o corte e antes do VU1: le o estado de cada sistema (`_ParticleType`,
// include/hieri_types.h) e desenha direto. Detalhes e o que ainda e hipotese: docs/FORMATOS.md ("Particulas").
//
// Confirmado no codigo da CPU (asm de common/particle e common/partdraw):
//   - fx[100] em 0x5BA280 (0x2EE0 bytes cada); lista ativa: fxActiveTop (0x6F88EC) -> fxList[i].next (+8, 12 bytes por entrada), -1 = fim
//   - por particula: pos[i] (FVECTOR; w = tamanho, raio: particleFOVcalcMaxRadius usa maxSize + expansionRate*lifetime),
//     vel[i] (FVECTOR), age[i] (int16, em campos); numParticles em +0x2ED0
//   - so desenha com info.attribs & 0x400 (pacote montado), numParticles >= 1 (sprine: >= 2) e tipo != 11
//   - tipo = vis.attribs (+0x2E90): 1 linha, 2 sweepTrail, 3 sprite, 4 texSprite, 5 stripe, 6 strite, 8 sprine, 9 kite,
//     13 frag, 15 dustTrail, 16 wake (tabela de saltos de particlePrepDraw)
//   - cor (loadColors): tabela RGBA em 0x256C10 (0x80 = 1,0), vis.numColors cores a partir de vis.colorIndex; t = age * colorsPerFrame,
//     cor = lerp(cor[t >> 12], cor[(t >> 12) + 1], (t & 0xFFF) / 4096); 0 cores = 0x80808080, 1 cor = constante
//   - textura: TextureInfo[vis.texInfoIndex] (0x5B9E80: numTextures, texIndex, du, dv); texId[texIndex] (0x6DFAB0, u16) indexa o
//     texInfo global e TEX0[texIndex] (0x6DF6B0, u64) e o registrador TEX0 (psm/CBP). texSprite escolhe o quadro por particula:
//     attribs&1 aleatorio, attribs&2 uvI[i] >> 2, senao (age * mapsPerFrame) >> 12 (attribs&4: trava no ultimo, senao modulo)
//   - mistura: vis.alphaContext e o registrador ALPHA do GS (0x44 = (Cs-Cd)*As+Cd, 0x48 = Cs*As+Cd aditivo)
//   - ordem: do mais longe ao mais perto pela profundidade de emit.ParticleToEmitVec (+0x2CA0) + vis.priority; opacos (alpha 0) antes
// Lido no microcodigo de particulas do VU1 (pacote de carga em 0x253060, contexto padrao em 0x256B90; sprite chama 0x59, linha 0x0B,
// strite 0x151):
//   - sprite: o VU1 manda um SPRITE do GS (retangulo alinhado a tela) de meio-lado = pos.w / 2 * foco / profundidade, limitado a
//     foco / 2 (medido pela checagem VU1 x nativo em sprite e texSprite: a leitura antiga, pos.w inteiro, dava o dobro); ou seja,
//     pos.w e o lado em unidades de mundo e o meio-lado nunca passa de metade da profundidade. Os bits baixos de pos.w indexam os cantos de
//     UV (3 normal, 1 espelha U, 2 espelha V, 0 os dois); a cor sai da tabela pela idade, como no loadColors
//   - linha: de pos ate pos - vel (a cauda e projetada com a matriz do quadro anterior: borrao de movimento da camera)
//   - strite: ver Particles::striteTriangle (cauda com a matriz do quadro anterior, prevViewMat)
// Destroços (frag, 13) nao passam por aqui: Scene::collectParticles/drawSprites (atlas de celulas 8x8, interpolados; pos.w = lado e bits baixos = celula/espelhamento).
// Sprine: Particles::sprineQuads (sprites ao longo da curva do microprograma). Ainda aproximado: as faixas (stripe, sweepTrail) ligam
// particulas consecutivas; kite, wake e dustTrail viram sprites;
// a cauda das linhas usa a matriz atual.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "rlgl.h"

// GL 1.1 (opengl32), so para ler a profundidade no teste de oclusao do lens flare; o raylib nao expoe leitura de profundidade
#ifdef _WIN32
extern "C" __declspec(dllimport) void __stdcall glReadPixels(int x, int y, int w, int h, unsigned format, unsigned type, void *data);
#else
extern "C" void glReadPixels(int x, int y, int w, int h, unsigned format, unsigned type, void *data);
#endif

namespace wotm {

// Normal do vertice para o shader do Scene::draw: (fogBurnThru, 1, FGE + 2 * GS). O rlNormal3f normaliza; o shader le as razoes
// por y. FGE = neblina; GS = alfa cru do GS na textura e no vertice (As = At * Av / 16384). rlNormal3f(0, 0, 0) desliga os dois.
inline void shaderNormal(float burn, bool fge, bool gs) { rlNormal3f(burn, 1.f, (fge ? 1.f : 0.f) + (gs ? 2.f : 0.f)); }

// Textura branca com alfa 0x80 (1,0 do GS): o "sem textura" no modo GS (a padrao do raylib, alfa 255, valeria 2,0).
inline unsigned gsWhiteTexture() {
    static unsigned id = 0;
    if (!id) {
        uint32_t px = 0x80FFFFFFu;
        Image img{&px, 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        id = LoadTextureFromImage(img).id;
    }
    return id;
}

namespace fxaddr {
constexpr uint32_t fx = 0x005BA280;           // _ParticleType fx[100]
constexpr uint32_t stride = 0x2EE0;           // sizeof(_ParticleType)
constexpr uint32_t count = 100;
constexpr uint32_t fxList = 0x006DF200;       // 12 bytes por sistema: next em +8
constexpr uint32_t fxActiveTop = 0x006F88EC;  // primeiro ativo (-1 = nenhum)
constexpr uint32_t textureInfo = 0x005B9E80;  // TextureInfoType[128]: numTextures s16, texIndex u16, du u16, dv u16
constexpr uint32_t texId = 0x006DFAB0;        // u16[128]: indice no texInfo global
constexpr uint32_t tex0 = 0x006DF6B0;         // u64[128]: TEX0 de cada textura de particula
constexpr uint32_t colors = 0x00256C10;       // RGBA (0x80 = 1,0), indexado por vis.colorIndex
}

// Offsets dentro de _ParticleType (include/hieri_types.h)
namespace fxoff {
constexpr uint32_t uvI = 0x2B40;          // u8[256]
constexpr uint32_t emitVec = 0x2CA0;      // emit.ParticleToEmitVec
constexpr uint32_t fxId = 0x2DDC;         // emit.fxId (parece ser o indice do preset de particleInitFx; nao confirmado em dump)
constexpr uint32_t infoAttribs = 0x2E30;  // info.attribs (0x400 = pacote montado neste quadro)
constexpr uint32_t minSize = 0x2E38;      // info.minSize (pedestres: largura do carimbo)
constexpr uint32_t maxSize = 0x2E3C;      // info.maxSize (pedestres: altura do carimbo)
constexpr uint32_t lifetime = 0x2E4A;     // info.lifetime (s16, campos)
constexpr uint32_t mapsPerFrame = 0x2E74; // info.mapsPerFrame (4.12)
constexpr uint32_t colorsPerFrame = 0x2E78;
constexpr uint32_t type = 0x2E90;         // vis.attribs
constexpr uint32_t texMask = 0x2E94;      // vis.texMask (numero de quadros do texSprite)
constexpr uint32_t texInfoIndex = 0x2E98;
constexpr uint32_t numColors = 0x2E9C;
constexpr uint32_t colorIndex = 0x2EA0;
constexpr uint32_t priority = 0x2EA4;
constexpr uint32_t alphaContext = 0x2EA8;
constexpr uint32_t pos = 0x2EB0;          // FVECTOR*
constexpr uint32_t vel = 0x2EB4;          // FVECTOR*
constexpr uint32_t age = 0x2EB8;          // int16*
constexpr uint32_t numParticles = 0x2ED0;
constexpr uint32_t maxParticles = 0x2ED4;
}

enum FxType : uint32_t {
    FX_LINE = 1, FX_SWEEPTRAIL = 2, FX_SPRITE = 3, FX_TEXSPRITE = 4, FX_STRIPE = 5, FX_STRITE = 6, FX_ARC = 7, FX_SPRINE = 8, FX_KITE = 9,
    FX_FRAG = 13, FX_PEDS = 14, FX_DUSTTRAIL = 15, FX_WAKE = 16, FX_ARC2 = 17,
};

inline const char *fxTypeName(uint32_t t) {
    switch (t) {
    case FX_LINE: return "line";
    case FX_SWEEPTRAIL: return "sweepTrail";
    case FX_SPRITE: return "sprite";
    case FX_TEXSPRITE: return "texSprite";
    case FX_STRIPE: return "stripe";
    case FX_STRITE: return "strite";
    case FX_SPRINE: return "sprine";
    case FX_KITE: return "kite";
    case FX_FRAG: return "frag";
    case FX_DUSTTRAIL: return "dustTrail";
    case FX_WAKE: return "wake";
    case FX_ARC: return "arco";
    case FX_ARC2: return "arco17";
    case FX_PEDS: return "pedestre";
    default: return "?";
    }
}

// Um sistema de particulas pronto para desenhar
struct FxSystem {
    uint32_t base = 0;     // endereco do _ParticleType
    uint32_t index = 0;    // indice em fx[]
    uint32_t type = 0;
    int n = 0;             // numParticles
    float key = 0;         // ordem de desenho (maior = mais longe = antes)
};

// Eixos da camera para montar os quadrados: right/up no plano da tela, back = -frente, profundidade(p) = depth . (p, 1)
// ox/oy: coordenadas x e y de vista (ox . (p, 1)), na mesma base de right/up.
// Neblina (Particles::setFog): W(p) = fogW . (p, 1) (linha 3 do worldToScreenMat, com o olho), F = fogFA + fogFB / W; fogFB 0 = sem neblina.
struct FxView { float right[3], up[3], back[3], depth[4], ox[4], oy[4]; float fogW[4] = {0, 0, 0, 0}, fogFA = 0.f, fogFB = 0.f; };

class Particles {
public:
    // texture(texId, tex0) -> id de textura do raylib (0 = sem textura); fornecido pela Scene (paletas do .RTX)
    using TexFn = std::function<unsigned(uint32_t texId, uint64_t tex0)>;

    std::vector<FxSystem> systems;

    // Textura composta de poeira/fumaca: TEX0 com TBP 0x3F60 (VRAM 0xFD800, CT32 64x64) e sem pixels na RAM (texId 0xFFFF; o jogo a
    // compoe na VRAM, Scene::dustTexture). O TexFn a recebe com texId = kDustTexId. Quatro celulas de 32x32; o sprite (VisualInfo
    // 105) e o dustTrail (1, 2 e 118) sobem a tabela de UV das celulas e escolhem a entrada pelos bits baixos de pos.w.
    static constexpr uint32_t kDustTexId = 0xFFFF;
    static bool isDustTex(uint64_t tex0) { return (tex0 & 0x3FFF) == 0x3F60; }
    static uint64_t systemTex0(const Ram &ram, uint32_t b, uint32_t frame = 0) {
        const uint32_t k = (ram.u16(fxaddr::textureInfo + 8 * (ram.u32(b + fxoff::texInfoIndex) & 127) + 2) + frame) & 127;
        return uint64_t(ram.u32(fxaddr::tex0 + 8 * k)) | (uint64_t(ram.u32(fxaddr::tex0 + 8 * k + 4)) << 32);
    }

    // Teste de visao do particleDraw: viewInFOV(0, &FOVsphere (+0x2C80), &FOVextent (+0x2C90)), a caixa (centro, meia-extensao) contra
    // os 6 planos de worldCtx[0] (fovNormsTP em +0xF0, fovSigns em +0x170; src/common/view.cpp). Falso = o retail nao desenha o sistema.
    static bool inViewFov(const Ram &ram, uint32_t b) {
        constexpr uint32_t w = 0x00445CD0u;   // worldCtx[0]
        float c[3], e[3];
        for (int k = 0; k < 3; ++k) { c[k] = ram.f32(b + 0x2C80 + 4 * k); e[k] = ram.f32(b + 0x2C90 + 4 * k); }
        for (uint32_t i = 0; i < 2; ++i)
            for (uint32_t j = 0; j < 3; ++j) {
                const uint32_t n = w + 0xF0 + 64 * i + 16 * j, s = w + 0x170 + 64 * i + 16 * j;
                float d = 0.f;
                for (uint32_t k = 0; k < 3; ++k) d += ram.f32(n + 4 * k) * (c[k] + e[k] * ram.f32(s + 4 * k));
                if (d < ram.f32(w + 0xF0 + 64 * i + 48 + 4 * j)) return false;
            }
        return true;
    }

    // Focos da worldToScreenMat M (pixels do GS por unidade de vista a W = 1): parte das linhas 0 e 1 perpendicular a linha 3 (frente).
    // 384 e 180,48 na luta.
    static bool screenFoci(const float M[4][4], float &fx, float &fy) {
        float n3 = 0.f, d03 = 0.f, d13 = 0.f;
        for (int k = 0; k < 3; ++k) { n3 += M[3][k] * M[3][k]; d03 += M[0][k] * M[3][k]; d13 += M[1][k] * M[3][k]; }
        if (!(n3 > 1e-6f)) return false;
        fx = 0.f; fy = 0.f;
        for (int k = 0; k < 3; ++k) {
            const float a0 = M[0][k] - d03 / n3 * M[3][k], a1 = M[1][k] - d13 / n3 * M[3][k];
            fx += a0 * a0; fy += a1 * a1;
        }
        fx = std::sqrt(fx); fy = std::sqrt(fy);
        return fx > 1e-6f && fy > 1e-6f;
    }

    // Strite como o microprograma do VU1 (entrada 0xA88; conferido pela checagem, erro < 0,1 px). Em pixels do GS:
    //   H = M (pos) com a worldToScreenMat atual; T = M' (pos - vel) com a prevViewMat (fx + 0x2EC0: 4 qwords no pacote, colunas
    //   x/y/z/translacao em coordenadas de mundo): a cauda sai onde estava na tela do quadro anterior (borrao de movimento).
    //   A = 3T - 2H (ponta), B = 3H - 2T (base), divididos pelo proprio w. Direcao m = (B - A) normalizada no espaco dividido pelos focos
    //   (fx, fy da matriz: 384 e 180,48 na luta); ponta recua 0,5 pos.w, base avanca 0,25 pos.w e a meia-largura e 0,433 pos.w
    //   (0,866 / 2), em unidades de vista: pixel = foco * deslocamento * pos.w / w.
    // Os tres pontos voltam ao mundo pela matriz atual, cada um na sua profundidade. c[0] = c[3] = ponta, c[1]/c[2] = base.
    static bool striteTriangle(const Ram &ram, uint32_t b, const float p[4], const float v[3], float c[4][3]) {
        constexpr uint32_t kW2S = 0x006E1FD0u, kEo = 0x00445CD0u + 0x20;   // worldToScreenMat[0], worldCtx[0].eo (= -olho)
        float M[4][4], eye[3];
        for (int r = 0; r < 4; ++r)
            for (int k = 0; k < 4; ++k) M[r][k] = ram.f32(kW2S + 16 * r + 4 * k);
        for (int k = 0; k < 3; ++k) eye[k] = -ram.f32(kEo + 4 * k);
        const uint32_t pm = ram.u32(b + 0x2EC0);
        if (!ram.ok(pm, 64)) return false;
        float P[16];
        bool any = false;
        for (int k = 0; k < 16; ++k) { P[k] = ram.f32(pm + 4 * k); any = any || P[k] != 0.f; }
        if (!any) return false;
        float H[4], T[4];
        const float d[3] = {p[0] - eye[0], p[1] - eye[1], p[2] - eye[2]}, t[3] = {p[0] - v[0], p[1] - v[1], p[2] - v[2]};
        for (int r = 0; r < 4; ++r) {
            H[r] = M[r][0] * d[0] + M[r][1] * d[1] + M[r][2] * d[2] + M[r][3];
            T[r] = P[12 + r] + P[r] * t[0] + P[4 + r] * t[1] + P[8 + r] * t[2];
        }
        float A[4], B[4];
        for (int r = 0; r < 4; ++r) { A[r] = 3.f * T[r] - 2.f * H[r]; B[r] = 3.f * H[r] - 2.f * T[r]; }
        if (!(A[3] > 1e-3f) || !(B[3] > 1e-3f)) return false;
        float fx, fy;
        if (!screenFoci(M, fx, fy)) return false;
        const float ax = A[0] / A[3], ay = A[1] / A[3], bx = B[0] / B[3], by = B[1] / B[3];
        float mx = (bx - ax) / fx, my = (by - ay) / fy;
        const float ml = std::sqrt(mx * mx + my * my);
        if (!(ml > 1e-9f)) return false;
        mx /= ml; my /= ml;
        const float w = p[3], ka = 0.5f * w / A[3], kb = w / B[3];
        const float px[3] = {ax - ka * mx * fx, bx + kb * (0.25f * mx * fx - 0.4330127f * my * fx), bx + kb * (0.25f * mx * fx + 0.4330127f * my * fx)};
        const float py[3] = {ay - ka * my * fy, by + kb * (0.25f * my * fy + 0.4330127f * mx * fy), by + kb * (0.25f * my * fy - 0.4330127f * mx * fy)};
        const float pw[3] = {A[3], B[3], B[3]};
        // volta ao mundo: linhas 0, 1 e 3 de M aplicadas a (q - olho) dao (x w, y w, w)
        const float m00 = M[0][0], m01 = M[0][1], m02 = M[0][2], m10 = M[1][0], m11 = M[1][1], m12 = M[1][2], m30 = M[3][0], m31 = M[3][1], m32 = M[3][2];
        const float det = m00 * (m11 * m32 - m12 * m31) - m01 * (m10 * m32 - m12 * m30) + m02 * (m10 * m31 - m11 * m30);
        if (!(std::fabs(det) > 1e-12f)) return false;
        for (int k = 0; k < 3; ++k) {
            const float r0 = px[k] * pw[k] - M[0][3], r1 = py[k] * pw[k] - M[1][3], r3 = pw[k] - M[3][3];
            const float q0 = (r0 * (m11 * m32 - m12 * m31) - m01 * (r1 * m32 - m12 * r3) + m02 * (r1 * m31 - m11 * r3)) / det;
            const float q1 = (m00 * (r1 * m32 - m12 * r3) - r0 * (m10 * m32 - m12 * m30) + m02 * (m10 * r3 - r1 * m30)) / det;
            const float q2 = (m00 * (m11 * r3 - r1 * m31) - m01 * (m10 * r3 - r1 * m30) + r0 * (m10 * m31 - m11 * m30)) / det;
            const int dst = k == 0 ? 0 : k;
            c[dst][0] = eye[0] + q0; c[dst][1] = eye[1] + q1; c[dst][2] = eye[2] + q2;
        }
        for (int a = 0; a < 3; ++a) c[3][a] = c[0][a];
        return true;
    }

    // Reune os sistemas desenhaveis (mesmos testes de particleDraw). A ordem sai no draw (camera atual).
    void collect(const Ram &ram) {
        systems.clear();
        uint32_t i = ram.u32(fxaddr::fxActiveTop);
        for (uint32_t guard = 0; i < fxaddr::count && guard < fxaddr::count; ++guard) {
            const uint32_t b = fxaddr::fx + i * fxaddr::stride;
            const uint32_t next = ram.u32(fxaddr::fxList + 12 * i + 8);
            FxSystem s;
            s.base = b;
            s.index = i;
            s.type = ram.u32(b + fxoff::type);
            s.n = int32_t(ram.u32(b + fxoff::numParticles));
            const int maxN = int32_t(ram.u32(b + fxoff::maxParticles));
            const bool built = (ram.u32(b + fxoff::infoAttribs) & 0x400) != 0;
            const int minN = s.type == FX_SPRINE ? 2 : 1;
            if (built && s.type != 11 && s.type != FX_FRAG && s.n >= minN && s.n <= std::max(maxN, 1) && s.n <= 1024 && ram.ok(ram.u32(b + fxoff::pos), 16 * uint32_t(s.n)) &&
                inViewFov(ram, b))
                systems.push_back(s);
            i = next;
        }
        static const bool list = std::getenv("WOTM_FXLIST") != nullptr;
        if (list) dump(ram);
        collectFlares(ram);
    }

    // Lens flare (tipo 11): fora do particleDraw comum. Dois fx[] tipo 11 (lensFlarePacket[vista], 0x6F8908; particleInitAfter) cujo
    // pos[] (fx + 0x2EB0) e a lista HierLensFlareList[74] (48 bytes) de lensFlareList[vista] (0x6F8900), com lensFlareIndex[vista]
    // (0x6F88F8) entradas. A lista e alimentada pelo hierLightNode (luzes com hasLensFlare: pos no mundo, flags = as da luz, intensity,
    // cor, dotProd/beamWidth do feixe) e pelo particleAddLensFlareToList (sistemas com vis.lightColor & 0x1000000: flags 3, Zfudge,
    // intensity = lightIntensity^2, cor = lightColor / 255).
    //  - particleLensFlareTestZbuffer (no fim do particleDraw, depois do VU1 desenhar a cena): projeta cada entrada, descarta as fora da
    //    tela ou com W < 1 (compacta a lista), grava pos = (X, Y, Z do GS), distance = W (100 com lightAtInfinity, bit 7 das flags) e le
    //    do GS o Z 4x4 em (X - 2, Y - 2) (sceGsStoreImage PSMZ32).
    //  - particleLensFlareRender: amostra visivel = Z <= limiar. Luz no infinito: as 16, limiar fptoui(1,03125 Z) + 1; local: as 4
    //    centrais, limiar fptoui(a + b / (W - max(Zfudge; 1,5))) (a, b = viewGetZBuffParams). intensity *= visiveis / amostras; pos +=
    //    centroide das visiveis - (2 ou 1); com 0 < ID < 0x7FF (so luzes no infinito: particleLensFlareAddToDeBounceList), intensity =
    //    media com a do quadro anterior (D_0078FF00); abaixo de 1/256 zera o byte das flags. Depois o lensFlarePacketBuild grava q44 =
    //    (centro X, centro Y, 18, n) e o UNPACK de 3n qwords da lista em q45.
    // No runtime o GS emulado nao tem a cena (sem VU1); aqui o teste e refeito contra a profundidade do nativo (drawFlares), e o centroide
    // e a fracao do GS emulado sao desfeitos supondo visibilidade total la (ver o WOTM_FLARELOG para conferir).
    // O microprograma 0x2B20 (build/vu1/particle.txt), por entrada (q45 + 3i):
    //  - so com flags & 1; Q = 1/distance; brilho = I Q Q (o segundo MULq le o Q antigo: o DIV 1/beamWidth ainda nao terminou; com o Q
    //    novo as particulas, sem beamWidth, sairiam com lixo), tamanho = sqrt(I) Q. Com flags & 8 (direcional): os dois vezes
    //    f / beamWidth, f = max(dotProd, 0) + beamWidth - 1 (|dotProd| com flags & 0x10, bidirecional); some se brilho f <= 0.
    //  - 18 SPRITEs (tabelas no pacote, q8-q25: TEX0, t, meio-lado com o seletor de UV nos 2 bits baixos; q26-q43: R, G, B, A). Sprite k
    //    no ponto C + t_k (S - C) da linha do centro da tela C ate a fonte S; cantos +/- (h, h/2) px do GS. k = 0: h = meio-lado tamanho A,
    //    cor min(255, RGB rgb). k > 0: h = meio-lado, cor min(32, RGB rgb A brilho). Alfa 0x80, ALPHA 0x48 (aditivo), UV de (0,5; 0,5) no
    //    canto + ate (uvMax - 0,5) no canto - (uvMax = 32, 64 ou 128 texels pelo seletor). Depois de um sprite com as tres cores < 1, a
    //    entrada sai dos seguintes.
    struct FlareSprite { uint64_t tex0 = 0; uint32_t texId = 0; float t = 0.f, half = 0.f, uvMax = 0.f, rgba[4] = {0, 0, 0, 0}; };
    struct Flare { float x = 0, y = 0, z = 0, w = 0, zfudge = 0, dot = 0, bw = 0, rgb[3] = {0, 0, 0}, intensity = 0; uint32_t flags = 0, id = 0; };
    struct FlareFrame {
        float cx = 2048.f, cy = 2048.f, vw = 640.f, vh = 224.f, zScale = 0.f, zOff = 0.f;
        int nspr = 0;
        FlareSprite spr[18];
        std::vector<Flare> e;
    };
    FlareFrame flares;

    // A lista so esta pronta (projetada, com a fracao visivel) entre o fim do particleDraw e o hier do quadro seguinte. A apresentacao
    // roda em outra thread e quase sempre a pega ja refeita pelo hier (em coordenadas de mundo, sem os flares de particula, que o
    // particleAddLensFlareToList so acrescenta dentro do particleDraw). O runtime chama captureFlares na thread do jogo logo depois do
    // particleDraw; o collect usa essa copia. Sem captura (viewer sobre dump) le a RAM e so desenha se a lista estiver projetada.
    struct FlareSlot { std::mutex m; FlareFrame f; bool used = false; };
    static FlareSlot &flareSlot() { static FlareSlot s; return s; }
    static void captureFlares(const Ram &ram) {
        FlareFrame f;
        readFlares(ram, f);
        FlareSlot &s = flareSlot();
        std::lock_guard<std::mutex> g(s.m);
        s.f = std::move(f);
        s.used = true;
    }

    void collectFlares(const Ram &ram) {
        {
            FlareSlot &s = flareSlot();
            std::lock_guard<std::mutex> g(s.m);
            if (s.used) { flares = s.f; logFlares(); return; }
        }
        readFlares(ram, flares);
        logFlares();
    }
    void logFlares() const {
        static const bool log = std::getenv("WOTM_FLARELOG") != nullptr;
        if (log)
            for (const Flare &fl : flares.e)
                std::fprintf(stderr, "[flare] n %zu centro %.0f %.0f | %.1f %.1f Z %.0f W %.1f id %u flags %02x dot %.3f bw %.3f rgb %.3f %.3f %.3f I %.4g\n",
                             flares.e.size(), flares.cx, flares.cy, fl.x, fl.y, fl.z, fl.w, fl.id, fl.flags & 0xFF, fl.dot, fl.bw, fl.rgb[0], fl.rgb[1],
                             fl.rgb[2], fl.intensity);
    }

    static void readFlares(const Ram &ram, FlareFrame &f) {
        constexpr uint32_t kIndex = 0x006F88F8u, kPacket = 0x006F8908u, kViewDef = 0x006E1904u, kScreenMats = 0x006E1990u;
        f.e.clear();
        f.nspr = 0;
        const uint32_t p = ram.u32(kPacket);
        if (!ram.ok(p, fxaddr::stride)) return;
        const uint32_t pos = ram.u32(p + fxoff::pos), q44 = pos - 0x18, q8 = q44 - 36 * 16;
        const int n = int32_t(ram.u32(kIndex));
        if (n <= 0 || n > 74 || !ram.ok(q8, 37 * 16 + 48 * uint32_t(n)) || int32_t(ram.u32(q44 + 12)) != n) return;
        f.cx = ram.f32(q44);
        f.cy = ram.f32(q44 + 4);
        f.nspr = std::min<int>(18, std::max<int>(0, int32_t(ram.u32(q44 + 8))));
        const uint32_t def = ram.u32(kViewDef) & 0x0FFFFFFF;
        if (ram.ok(def, 0x18) && ram.u16(def + 4) && ram.u16(def + 6)) { f.vw = ram.u16(def + 4); f.vh = ram.u16(def + 6); }
        f.zScale = ram.f32(kScreenMats + 0x28);   // viewScreenMats[0][2][2] e [2][3]
        f.zOff = ram.f32(kScreenMats + 0x2C);
        const uint32_t gif = (p + 0xC + 15) & ~15u;   // lensFlarePacketInit: FLUSHA, UNPACK de 4 qwords em 0x110 (tag A+D, ALPHA, TEX0, tag SPRITE)
        for (int k = 0; k < f.nspr; ++k) {
            FlareSprite &s = f.spr[k];
            const uint32_t a = q8 + 16 * uint32_t(k), c = a + 18 * 16;
            s.tex0 = uint64_t(ram.u32(a)) | (uint64_t(ram.u32(a + 4)) << 32);
            s.t = ram.f32(a + 8);
            s.half = ram.f32(a + 12);
            s.uvMax = float(ram.u32(gif + 16 + 16 * (ram.u32(a + 12) & 3) + 12) & 0xFFFF) / 16.f;
            for (int j = 0; j < 4; ++j) s.rgba[j] = ram.f32(c + 4 * uint32_t(j));
            s.texId = ram.u16(p + 0x2C40 + 2 * uint32_t(k));
        }
        const float x0 = f.cx - 0.5f * f.vw, x1 = f.cx + 0.5f * f.vw, y0 = f.cy - 0.5f * f.vh, y1 = f.cy + 0.5f * f.vh;
        for (int i = 0; i < n; ++i) {
            const uint32_t e = pos + 48 * uint32_t(i);
            Flare fl;
            fl.x = ram.f32(e); fl.y = ram.f32(e + 4); fl.z = ram.f32(e + 8); fl.id = ram.u32(e + 0xC);
            fl.flags = ram.u32(e + 0x10); fl.zfudge = ram.f32(e + 0x10); fl.w = ram.f32(e + 0x14);
            fl.dot = ram.f32(e + 0x18); fl.bw = ram.f32(e + 0x1C);
            for (int j = 0; j < 3; ++j) fl.rgb[j] = ram.f32(e + 0x20 + 4 * uint32_t(j));
            fl.intensity = ram.f32(e + 0x2C);
            // fora da tela = lista ainda em coordenadas de mundo (o hier ja a refez e o teste de Z nao rodou): nada a desenhar
            if (!(fl.x >= x0 - 3.f && fl.x <= x1 + 3.f && fl.y >= y0 - 3.f && fl.y <= y1 + 3.f)) { f.e.clear(); break; }
            f.e.push_back(fl);
        }
    }

    // Desenha os flares do ultimo quadro do jogo em espaco de tela (dentro de BeginMode3D, depois das particulas; o pacote do lens flare
    // sai depois delas no particleDraw). Le a profundidade ja desenhada para o teste de oclusao (o particleLensFlareRender).
    void drawFlares(const TexFn &texture) {
        static const bool off = std::getenv("WOTM_NOFX") != nullptr || std::getenv("WOTM_NOFLARE") != nullptr;
        if (off || flares.e.empty() || flares.nspr == 0) return;
        const FlareFrame &f = flares;
        rlDrawRenderBatchActive();
        const int fbw = rlGetFramebufferWidth(), fbh = rlGetFramebufferHeight();
        if (fbw <= 0 || fbh <= 0) return;
        const float sx = float(fbw) / f.vw, sy = float(fbh) / f.vh;   // px do GS -> px do nativo
        const double zn = rlGetCullDistanceNear(), zf = rlGetCullDistanceFar();
        // Z do GS (24 bits) que o retail leria no pixel (gx, gy) da tela do GS: a + b / W da profundidade nativa; vazio (fundo) = 0
        auto gsZ = [&](int gx, int gy) -> uint32_t {
            const int px = std::clamp(int((float(gx) + 0.5f) * sx), 0, fbw - 1), py = std::clamp(int((float(gy) + 0.5f) * sy), 0, fbh - 1);
            float d = 1.f;
            glReadPixels(px, fbh - 1 - py, 1, 1, 0x1902 /* GL_DEPTH_COMPONENT */, 0x1406 /* GL_FLOAT */, &d);
            if (d >= 1.f) return 0u;
            const double ndc = 2.0 * d - 1.0, W = 2.0 * zn * zf / (zf + zn - ndc * (zf - zn));
            const double z = double(f.zScale) + double(f.zOff) / std::max(W, 1e-3);
            return uint32_t(std::clamp(z, 0.0, 16777215.0));
        };
        auto fptoui = [](float v) -> uint32_t { return v <= 0.f ? 0u : v >= 4294967040.f ? 0xFFFFFFFFu : uint32_t(v); };

        struct Vis { float x, y, bright, size; const Flare *fl; bool on; };
        std::vector<Vis> vis;
        static const bool log = std::getenv("WOTM_FLARELOG") != nullptr;
        for (const Flare &fl : f.e) {
            if (!(fl.flags & 0xFF)) continue;
            const bool inf = (fl.flags & 0x80) != 0;
            // pixel do canto do bloco 4x4: (int) X - 0x6C2 (o retail supoe centro 2048 e tela 640 x 224)
            const int bx = int(fptoui(fl.x)) - int(f.cx - 0.5f * f.vw) - 2, by = int(fptoui(fl.y)) - int(f.cy - 0.5f * f.vh) - 2;
            uint32_t thr;
            if (inf) thr = fptoui(fl.z * 1.03125f) + 1;
            else {
                const float fud = fl.zfudge > 1.5f ? fl.zfudge : 1.5f, d = fl.w - fud;
                thr = fptoui((f.zScale * d + f.zOff) / d);
            }
            const int lo = inf ? 0 : 1, hi = inf ? 4 : 3;
            int cnt = 0, sumC = 0, sumR = 0;
            for (int r = lo; r < hi; ++r)
                for (int c = lo; c < hi; ++c)
                    if (gsZ(bx + c, by + r) <= thr) { ++cnt; sumC += c; sumR += r; }
            float frac = float(cnt) / float((hi - lo) * (hi - lo));
            if (fl.id && fl.id < 0x7FF) {   // debounce por ID (D_0078FF00): media com a fracao do quadro anterior
                float &prev = flarePrev_[fl.id];
                const float raw = frac;
                frac = 0.5f * (prev + raw);
                prev = raw;
            }
            const float I = fl.intensity * frac;
            if (log)
                std::fprintf(stderr, "[flare] desenho %.1f %.1f visiveis %d limiar %u I %.4g\n", fl.x, fl.y, cnt, thr, I);
            if (cnt == 0 || I < 1.f / 256.f) continue;
            // o retail ja somou o centroide do GS emulado (visibilidade total: 1,5 - 2 ou 1,5 - 1); troca pelo daqui
            const float shift = 1.5f - (inf ? 2.f : 1.f);
            Vis v{fl.x - shift + (float(sumC) / float(cnt) - (inf ? 2.f : 1.f)), fl.y - shift + (float(sumR) / float(cnt) - (inf ? 2.f : 1.f)), 0.f, 0.f, &fl, true};
            // microprograma 0x2B20, laco por entrada
            if (!(fl.flags & 1) || !(fl.w != 0.f)) continue;
            const float q = 1.f / fl.w;
            v.bright = I * q * q;
            v.size = std::sqrt(std::max(I, 0.f)) * q;
            if (fl.flags & 8) {
                const float k = ((fl.flags & 0x10) ? std::fabs(fl.dot) : std::max(fl.dot, 0.f)) + fl.bw - 1.f;
                if (!(v.bright * k > 0.f) || fl.bw == 0.f) continue;
                v.bright = v.bright * k / fl.bw;
                v.size = v.size * k / fl.bw;
            }
            vis.push_back(v);
        }
        if (vis.empty()) return;

        rlMatrixMode(RL_PROJECTION);
        rlPushMatrix();
        rlLoadIdentity();
        rlOrtho(0.0, double(fbw), double(fbh), 0.0, -1.0, 1.0);
        rlMatrixMode(RL_MODELVIEW);
        rlPushMatrix();
        rlLoadIdentity();
        rlDisableDepthTest();
        rlDisableBackfaceCulling();
        setBlend(BLEND_ADDITIVE);   // ALPHA 0x48: Cs As + Cd
        shaderNormal(0.f, false, true);
        const float ox = f.cx - 0.5f * f.vw, oy = f.cy - 0.5f * f.vh;
        for (int k = 0; k < f.nspr; ++k) {
            const FlareSprite &s = f.spr[k];
            unsigned tex = s.texId ? texture(s.texId, s.tex0) : 0;
            if (!tex) tex = blobTexture();
            const float tw = float(1u << ((s.tex0 >> 26) & 15)), th = float(1u << ((s.tex0 >> 30) & 15));
            const float u0 = 0.5f / tw, v0 = 0.5f / th, u1 = (s.uvMax - 0.5f) / tw, v1 = (s.uvMax - 0.5f) / th;
            rlBegin(RL_QUADS);
            rlSetTexture(tex);
            for (Vis &v : vis) {
                if (!v.on) continue;
                const Flare &fl = *v.fl;
                const float px = f.cx + s.t * (v.x - f.cx), py = f.cy + s.t * (v.y - f.cy);
                float h, col[3];
                for (int j = 0; j < 3; ++j) col[j] = s.rgba[j] * fl.rgb[j];
                if (k == 0) h = s.half * v.size * s.rgba[3];
                else {
                    h = s.half;
                    for (float &c : col) c *= s.rgba[3] * v.bright;
                }
                if (col[0] < 1.f && col[1] < 1.f && col[2] < 1.f) v.on = false;   // as entradas seguintes nao desenham mais esta
                const float cap = k == 0 ? 255.f : 32.f;
                uint8_t rgb[3];
                for (int j = 0; j < 3; ++j) rgb[j] = uint8_t(std::clamp(col[j], 0.f, cap));
                // cantos com FTOI4 (1/16 px); o canto + leva o UV (0,5; 0,5) e o canto - o (uvMax - 0,5)
                auto gx = [](float a) { return std::trunc(a * 16.f) / 16.f; };
                const float xa = (gx(px + h) - ox) * sx, ya = (gx(py + 0.5f * h) - oy) * sy;
                const float xb = (gx(px - h) - ox) * sx, yb = (gx(py - 0.5f * h) - oy) * sy;
                rlColor4ub(rgb[0], rgb[1], rgb[2], 0x80);
                rlTexCoord2f(u0, v0); rlVertex2f(xa, ya);
                rlTexCoord2f(u0, v1); rlVertex2f(xa, yb);
                rlTexCoord2f(u1, v1); rlVertex2f(xb, yb);
                rlTexCoord2f(u1, v0); rlVertex2f(xb, ya);
            }
            rlEnd();
            rlDrawRenderBatchActive();
        }
        rlSetTexture(0);
        rlNormal3f(0.f, 0.f, 0.f);
        setBlend(BLEND_ALPHA);
        rlEnableBackfaceCulling();
        rlEnableDepthTest();
        rlMatrixMode(RL_PROJECTION);
        rlPopMatrix();
        rlMatrixMode(RL_MODELVIEW);
        rlPopMatrix();
        // diagnostico: WOTM_FLARESHOT=<prefixo> grava ate 8 quadros (>= 1 s entre eles) com um flare de meio-lado >= 8 px do GS
        static const char *shot = std::getenv("WOTM_FLARESHOT");
        static int shots = 0;
        static double lastShot = -10.0;
        float big = 0.f;
        for (const Vis &v : vis) big = std::max(big, f.spr[0].half * v.size * f.spr[0].rgba[3]);
        if (shot && shots < 8 && big >= 8.f && GetTime() - lastShot >= 1.0) {
            lastShot = GetTime();
            unsigned char *px = rlReadScreenPixels(fbw, fbh);
            for (int i = 0; i < fbw * fbh; ++i) px[4 * i + 3] = 255;   // o alfa do quadro e o das misturas, nao opacidade
            Image img{px, fbw, fbh, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            char name[512];
            std::snprintf(name, sizeof name, "%s_%d.png", shot, shots++);
            ExportImage(img, name);
            RL_FREE(px);
            std::fprintf(stderr, "[flare] captura %s (meio-lado %.1f px, %zu entradas)\n", name, big, vis.size());
        }
    }

    // Diagnostico (WOTM_FXLIST=1): um sistema por linha e a primeira particula
    void dump(const Ram &ram) const {
        std::fprintf(stderr, "[fx] %zu sistemas desenhaveis\n", systems.size());
        for (const FxSystem &s : systems) {
            const uint32_t b = s.base, p = ram.u32(b + fxoff::pos), a = ram.u32(b + fxoff::age);
            const uint32_t ti = ram.u32(b + fxoff::texInfoIndex);
            const uint32_t tinfo = fxaddr::textureInfo + 8 * (ti & 127);
            std::fprintf(stderr, "[fx] #%u preset %u %s n=%d alpha=%02x cores=%u@%u texInfo=%u (n=%d idx=%u) cpf=%u vida=%d | p0 %.1f %.1f %.1f tam %.2f idade %d\n",
                         s.index, ram.u32(b + fxoff::fxId), fxTypeName(s.type), s.n, ram.u32(b + fxoff::alphaContext),
                         ram.u32(b + fxoff::numColors), ram.u32(b + fxoff::colorIndex), ti, ram.s16(tinfo), ram.u16(tinfo + 2),
                         ram.u32(b + fxoff::colorsPerFrame), ram.s16(b + fxoff::lifetime),
                         ram.f32(p), ram.f32(p + 4), ram.f32(p + 8), ram.f32(p + 12), ram.s16(a));
        }
    }

    // Desenha (dentro de BeginMode3D, depois dos objetos opacos e translucidos). Testa profundidade, nao escreve.
    // eye: olho da camera (o W da neblina e medido a partir dele, como no VU1)
    void draw(const Ram &ram, const TexFn &texture, const float eye[3]) {
        static const bool off = std::getenv("WOTM_NOFX") != nullptr;
        if (off || systems.empty()) return;
        const Matrix mv = rlGetMatrixModelview();
        FxView view{{mv.m0, mv.m4, mv.m8}, {mv.m1, mv.m5, mv.m9}, {mv.m2, mv.m6, mv.m10}, {-mv.m2, -mv.m6, -mv.m10, -mv.m14},
                    {mv.m0, mv.m4, mv.m8, mv.m12}, {mv.m1, mv.m5, mv.m9, mv.m13}};
        float row3[4];
        for (int k = 0; k < 4; ++k) row3[k] = ram.f32(addr::worldToScreenMat + 48 + 4 * k);
        setFog(ram, view, row3, eye);
        // particleDraw: do mais longe ao mais perto pela profundidade de emit.ParticleToEmitVec + vis.priority; opacos (alpha 0) antes
        for (FxSystem &s : systems) {
            if (ram.u32(s.base + fxoff::alphaContext) == 0) { s.key = 50000.f + ram.f32(s.base + fxoff::priority); continue; }
            const float x = ram.f32(s.base + fxoff::emitVec), y = ram.f32(s.base + fxoff::emitVec + 4), z = ram.f32(s.base + fxoff::emitVec + 8);
            s.key = -(mv.m2 * x + mv.m6 * y + mv.m10 * z + mv.m14) + ram.f32(s.base + fxoff::priority);
        }
        std::stable_sort(systems.begin(), systems.end(), [](const FxSystem &a, const FxSystem &b) { return a.key > b.key; });
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        rlDisableBackfaceCulling();
        for (const FxSystem &s : systems) drawSystem(ram, s, texture, view);
        rlDrawRenderBatchActive();
        rlNormal3f(0.f, 0.f, 0.f);   // sem neblina para o que vem depois (overlay)
        rlSetBlendMode(BLEND_ALPHA);
        rlEnableDepthMask();
        rlEnableBackfaceCulling();
    }

    // Cor RGBA (0x80 = 1,0) da particula com idade `age`, como loadColors
    static uint32_t colorAt(const Ram &ram, uint32_t b, int age) {
        const uint32_t nc = ram.u32(b + fxoff::numColors);
        const uint32_t tab = fxaddr::colors + 4 * ram.u32(b + fxoff::colorIndex);
        if (nc == 0) return 0x80808080u;
        if (nc == 1) return ram.u32(tab);
        const uint32_t t = uint32_t(std::max(age, 0)) * ram.u32(b + fxoff::colorsPerFrame);
        uint32_t k = t >> 12, f = t & 0xFFF;
        if (k >= nc - 1) { k = nc - 2; f = 0x1000; }   // o retail nao trava (a particula morre antes); aqui so por seguranca
        const uint32_t c0 = ram.u32(tab + 4 * k), c1 = ram.u32(tab + 4 * (k + 1));
        uint32_t out = 0;
        for (int sh = 0; sh < 32; sh += 8) {
            const int a = int((c0 >> sh) & 255), z = int((c1 >> sh) & 255);
            out |= uint32_t((a + (((z - a) * int(f)) >> 12)) & 255) << sh;
        }
        return out;
    }


    static bool isStrip(uint32_t type) { return type == FX_STRIPE || type == FX_SWEEPTRAIL; }

    // Neblina das particulas: o contexto comum dos microprogramas (qwords 0-7, de 0x256B90) leva fogFA em q4.w e fogFB em q5.w, os
    // mesmos de vu1FogParams; o VU1 calcula F = min(fogFA + fogFB / W, 255) e descarta a particula com F < 1 (CLIP), com ou sem FGE.
    // Medido na METRO (fogFA -107,9, fogFB 326.600: W > ~3.000): os pedestres somem ali (VU1CHECK.md, "Explicadas").
    static void setFog(const Ram &ram, FxView &v, const float row3[4], const float eye[3]) {
        v.fogFA = ram.f32(addr::vu1FogParams);
        v.fogFB = ram.f32(addr::vu1FogParams + 12);
        v.fogW[3] = row3[3];
        for (int a = 0; a < 3; ++a) { v.fogW[a] = row3[a]; v.fogW[3] -= row3[a] * eye[a]; }
    }
    // Ponto (mundo) todo na neblina: F < 1
    static bool foggedAt(const FxView &v, const float p[3]) {
        if (v.fogFB == 0.f) return false;
        const float w = v.fogW[0] * p[0] + v.fogW[1] * p[1] + v.fogW[2] * p[2] + v.fogW[3];
        return w > 1e-3f && v.fogFA + v.fogFB / w < 1.f;
    }
    // Particula toda na neblina (no centro do quadrado)
    static bool fogged(const FxView &v, const float c[4][3]) {
        float p[3];
        for (int a = 0; a < 3; ++a) p[a] = 0.25f * (c[0][a] + c[1][a] + c[2][a] + c[3][a]);
        return foggedAt(v, p);
    }

    // Gerador do VU (RINIT/RNEXT, como o PCSX2): R em [1, 2) com 23 bits de LFSR
    static uint32_t vuRinit(uint32_t bits) { return 0x3F800000u | (bits & 0x7FFFFFu); }
    static float vuRnext(uint32_t &R) {
        const uint32_t x = (R >> 4) & 1, y = (R >> 22) & 1;
        R = (((R << 1) ^ x ^ y) & 0x7FFFFFu) | 0x3F800000u;
        float f;
        std::memcpy(&f, &R, 4);
        return f;
    }

    // Arco eletrico (tipos 7 e 17; electricArcPacketInit/Build/Update, microprograma 0x19F8, lido em build/vu1/particle.txt).
    // Pacote: pos[] (fx +0x2EB0) em cadeias (cabeca: w baixo = 1 + elos; elo: w baixo = 5 << (nivel - 1), ou 0 = reto; cabeca 0
    // encerra); antes dele o cabecalho (pos - 0x28: minSize, semente, segInitU, 138 + CLUToffset, expansionRate, jag, segDeltaU,
    // numColors - 1) e 16 cores RGBA em float 0..255 (pos - 0x13C), que o electricArcPacketUpdate reescreve.
    //  - RINIT(semente) uma vez por pacote. Elo de nivel L de a a b: 2^L sub-trechos; amp = jag * |b - a|, metade a cada nivel; em cada
    //    vao, meio = 0,5a + r * amp - 1,5 * amp + 0,5b, r = (rx, ry, rz) numa janela deslizante de RNEXT (tres na entrada do elo; depois
    //    de cada meio, rx = ry, ry = rz, rz = RNEXT).
    //  - Fita (sub-rotina 0x1F70) nos S + 1 pontos, em coordenadas de tela isotropicas com y para baixo (o VU1 corrige o y pela razao
    //    dos focos): fantasmas 2P0 - P1 e 2PS - PS-1 (homogeneos), t_in e t_out unitarios, n = (t.y, -t.x); v = n_in + n_out +
    //    (t_out - t_in) se t_in x t_out < 0, senao + (t_in - t_out) (sem renormalizar). Vertices p +/- s_k / 2 * v no plano de vista,
    //    s_k = minSize + exp * k / S; A (+) com V = 1, B (-) com V = 0; U_k = segInitU + segDeltaU * (k - S / 2) (perspectiva, STQ);
    //    cor = lerp da tabela em clut + k * (nc - 1) / S, + 0,5, truncada (a mesma nos dois vertices).
    //  - O VU1 marca ADC com W < 1, F < 1 ou fora da banda de recorte; aqui some o quadrado com algum W < 1 (o F, no forEachQuad).
    // g(k, cantos[4][3] (A_k, B_k, B_k+1, A_k+1), uv[4][2], cor[2] (RGBA 0x80 = 1,0, em k e k + 1)).
    template <class G> static void arcQuads(const Ram &ram, const FxSystem &s, const FxView &view, G &&g) {
        const uint32_t pos = ram.u32(s.base + fxoff::pos);
        if (s.n < 3 || !ram.ok(pos - 0x13C, 0x13C + 16 * uint32_t(s.n))) return;
        const float minSize = ram.f32(pos - 0x28), segInitU = ram.f32(pos - 0x20), clut = ram.f32(pos - 0x1C) - 138.f;
        const float expRate = ram.f32(pos - 0x18), jag = ram.f32(pos - 0x14), segDeltaU = ram.f32(pos - 0x10), ncm1 = ram.f32(pos - 0xC);
        uint32_t R = vuRinit(ram.u32(pos - 0x24));
        struct V3 { float x, y, z; };
        auto at = [&](int i) { return V3{ram.f32(pos + 16 * uint32_t(i)), ram.f32(pos + 16 * uint32_t(i) + 4), ram.f32(pos + 16 * uint32_t(i) + 8)}; };
        auto w16 = [&](int i) { return int(ram.u16(pos + 16 * uint32_t(i) + 12)); };
        auto color = [&](float cpos) {   // cor RGBA (0x80 = 1,0) na posicao cpos da tabela (lerp, + 0,5, truncada)
            const int k0 = std::clamp(int(cpos), 0, 15), k1 = std::min(k0 + 1, 15);
            const float fr = cpos - float(int(cpos));
            uint32_t out = 0;
            for (int c = 0; c < 4; ++c) {
                const float a = ram.f32(pos - 0x13C + 16 * uint32_t(k0) + 4 * uint32_t(c)), b = ram.f32(pos - 0x13C + 16 * uint32_t(k1) + 4 * uint32_t(c));
                const float v = a + 0.5f - a * fr + b * fr;
                out |= uint32_t(std::clamp(int(v), 0, 255)) << (8 * c);
            }
            return out;
        };
        // tela isotropica (y para baixo) e W do VU1 (linha 3 do worldToScreenMat; sem camera do jogo, a profundidade)
        auto proj = [&](const V3 &p, float &x, float &y) {
            const float z = view.depth[0] * p.x + view.depth[1] * p.y + view.depth[2] * p.z + view.depth[3];
            const float iz = 1.f / (std::fabs(z) > 1e-6f ? z : 1e-6f);
            x = (view.ox[0] * p.x + view.ox[1] * p.y + view.ox[2] * p.z + view.ox[3]) * iz;
            y = -(view.oy[0] * p.x + view.oy[1] * p.y + view.oy[2] * p.z + view.oy[3]) * iz;
        };
        auto wOf = [&](const V3 &p) {
            if (view.fogW[0] == 0.f && view.fogW[1] == 0.f && view.fogW[2] == 0.f)
                return view.depth[0] * p.x + view.depth[1] * p.y + view.depth[2] * p.z + view.depth[3];
            return view.fogW[0] * p.x + view.fogW[1] * p.y + view.fogW[2] * p.z + view.fogW[3];
        };
        std::vector<V3> P;
        int i = 0;
        while (i < s.n) {
            const int links = w16(i) - 1;
            if (links < 0 || i + links >= s.n) break;   // cabeca 0 (fim) ou cadeia truncada
            P.clear();
            P.push_back(at(i));
            for (int l = 1; l <= links; ++l) {
                const V3 b = at(i + l);
                const int wl = w16(i + l);
                if (wl == 0) { P.push_back(b); continue; }
                int h = wl / 5;   // meia-distancia (em pontos) do primeiro nivel: 2^(L - 1)
                if (h < 1 || h > 64) { P.push_back(b); continue; }
                const size_t base = P.size() - 1, end = base + size_t(2 * h);
                P.resize(end + 1);
                P[end] = b;
                const V3 a = P[base];
                const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
                float amp = jag * std::sqrt(dx * dx + dy * dy + dz * dz);
                float rx = vuRnext(R), ry = vuRnext(R), rz = vuRnext(R);
                for (; h >= 1; h /= 2, amp *= 0.5f)
                    for (size_t j = base; j < end; j += size_t(2 * h)) {
                        const V3 &pa = P[j], &pb = P[j + size_t(2 * h)];
                        V3 &m = P[j + size_t(h)];
                        m.x = 0.5f * pa.x + rx * amp - 1.5f * amp + 0.5f * pb.x;
                        m.y = 0.5f * pa.y + ry * amp - 1.5f * amp + 0.5f * pb.y;
                        m.z = 0.5f * pa.z + rz * amp - 1.5f * amp + 0.5f * pb.z;
                        rx = ry; ry = rz; rz = vuRnext(R);
                    }
            }
            i += links + 1;
            const int S = int(P.size()) - 1;
            if (S < 1) continue;
            // pontos na tela, com os fantasmas nas pontas
            const size_t np = P.size();
            std::vector<float> sx(np + 2), sy(np + 2);
            const V3 g0{2.f * P[0].x - P[1].x, 2.f * P[0].y - P[1].y, 2.f * P[0].z - P[1].z};
            const V3 gn{2.f * P[np - 1].x - P[np - 2].x, 2.f * P[np - 1].y - P[np - 2].y, 2.f * P[np - 1].z - P[np - 2].z};
            proj(g0, sx[0], sy[0]);
            for (size_t k = 0; k < np; ++k) proj(P[k], sx[k + 1], sy[k + 1]);
            proj(gn, sx[np + 1], sy[np + 1]);
            std::vector<float> ox(np), oy(np);   // v de cada ponto (tela isotropica, y para baixo)
            std::vector<char> ok(np);
            for (size_t k = 0; k < np; ++k) {
                float tix = sx[k + 1] - sx[k], tiy = sy[k + 1] - sy[k], tox = sx[k + 2] - sx[k + 1], toy = sy[k + 2] - sy[k + 1];
                const float li = std::sqrt(tix * tix + tiy * tiy), lo = std::sqrt(tox * tox + toy * toy);
                ok[k] = li > 1e-12f && lo > 1e-12f && wOf(P[k]) >= 1.f;
                if (!ok[k]) continue;
                tix /= li; tiy /= li; tox /= lo; toy /= lo;
                const float cross = tix * toy - tox * tiy;
                const float sgn = cross < 0.f ? 1.f : -1.f;
                ox[k] = tiy + toy + sgn * (tox - tix);
                oy[k] = -tix - tox + sgn * (toy - tiy);
            }
            float cs[4][3], uv[4][2];
            const float u0 = segInitU - segDeltaU * 0.5f * float(S);
            auto corner = [&](size_t k, float side, float out[3]) {
                // o VU1 desloca v * s * (metade do foco, q5.xy) / W pixels: no plano de vista, s / 2 (medido: razao 0,5 contra s)
                const float sz = 0.5f * (minSize + expRate * float(k) / float(S));
                const float wx = side * sz * ox[k], wy = side * sz * oy[k];   // no plano de vista: y da tela para baixo = -up
                const float p[3] = {P[k].x, P[k].y, P[k].z};
                for (int a = 0; a < 3; ++a) out[a] = p[a] + view.right[a] * wx - view.up[a] * wy;
            };
            for (size_t k = 0; k + 1 < np; ++k) {
                if (!ok[k] || !ok[k + 1]) continue;
                corner(k, 1.f, cs[0]); corner(k, -1.f, cs[1]); corner(k + 1, -1.f, cs[2]); corner(k + 1, 1.f, cs[3]);
                const float ua = u0 + segDeltaU * float(k), ub = u0 + segDeltaU * float(k + 1);
                uv[0][0] = ua; uv[0][1] = 1.f; uv[1][0] = ua; uv[1][1] = 0.f;
                uv[2][0] = ub; uv[2][1] = 0.f; uv[3][0] = ub; uv[3][1] = 1.f;
                const uint32_t col[2] = {color(clut + float(k) * ncm1 / float(S)), color(clut + float(k + 1) * ncm1 / float(S))};
                g(int(k), cs, uv, col);
            }
        }
    }

    // Sprine (tipo 8; sprinePacketInit/Build, microprograma 0x2770 em build/vu1/particle.txt). O pacote fica antes de pos[] (qword 8):
    // cores RGBA (V4-8, q264-295), UV (V4-16 em 1/16 de texel, q296-299: u0, v0, u1, v1), e q300-q306 num UNPACK de 7 qwords que termina
    // em pos - 4: q300.x = preAgeFactor (pa); q301 = (h (nc - 1) / vida, h exp, -, f6); q302 = (minSize, N, (N + 1) >> 1, sprites por
    // lote; os tres inteiros); q303 e q304 = pesos das diferencas avancadas (x h^2); q305 = M2 (linha 2 da ParticleToEmitMat);
    // q306 = ((n - 1) N, idade[n - 1] N, n - 1; inteiro, float, inteiro). h = 1/N.
    //  - Um trecho por par A = pos[i], B = pos[i - 1] (i de n - 1 a 1), com Pm = pos[i + 1] e Pn = pos[i - 2]; fantasmas: no 1o trecho
    //    Pm = B - 2 M2 ((B - A) . M2) (reflexo de B no plano de A normal a M2), no ultimo Pn = 2 pos[0] - pos[1].
    //  - D1 = q303 . (Pm, A, B, Pn), D2 = q304 . (...), D2' = q304 invertido (w, z, y, x) . (...). p = A; a cada passo: emite em p,
    //    p += D1, D1 += D2; se depois disso faltam (N + 1) >> 1 passos (e N > 2), D1 += c (D2' - D2), D2 = D2' (c = 0,5 com N par,
    //    0,125 impar). N passos por trecho; pos[0] nao e emitido.
    //  - Passo global s (aN = q306.y + s): r = valor corrente do gerador; j = 1 + pa (2 - r); lado = (minSize + q301.y aN) (1 + (r - 1) f6);
    //    cor = tabela em min(31, q301.x aN j) (lerp, + 0,5, truncada; no 1o passo de um trecho que nao comeca lote, j entra duas vezes).
    //    Depois RNEXT, e o UV do passo e q296 + (bits de r & 3). RINIT(pos[n - 1].w) e um RNEXT no comeco; RINIT(pos[i].w) em cada trecho
    //    (so reinicia o gerador; o r corrente segue).
    //  - SPRITE alinhado a tela: centro p +/- (192, 90,24) lado / W px, ou seja meio-lado lado / 2 no plano de vista. Some com W < 1 ou
    //    F < 1 (o F no forEachQuad).
    // g(passo, cantos[4][3] ((-,-), (+,-), (+,+), (-,+) na tela), uv[4][2], cor RGBA (0x80 = 1,0)).
    template <class G> static void sprineQuads(const Ram &ram, const FxSystem &s, const FxView &view, G &&g) {
        const uint32_t pos = ram.u32(s.base + fxoff::pos);
        const uint32_t q300 = pos - 0x74;
        if (s.n < 2 || !ram.ok(q300 - 0x24 - 0x80 - 4, 0x200) || !ram.ok(pos, 16 * uint32_t(s.n))) return;
        if ((ram.u32(q300 - 4) & 0x7F0003FFu) != (0x6C000000u | 300u) || (ram.u32(pos - 4) & 0x3FF) != 8) return;   // UNPACK de q300 e de pos
        const uint32_t uvAt = q300 - 4 - 0x20, colAt = uvAt - 4 - 0x80;   // dados dos UNPACK de q296 (4 x V4-16) e q264 (32 x V4-8)
        if ((ram.u32(uvAt - 4) & 0x3FF) != 296 || (ram.u32(colAt - 4) & 0x3FF) != 264) return;
        auto q = [&](int k, int c) { return ram.f32(q300 + 16 * uint32_t(k - 300) + 4 * uint32_t(c)); };
        auto qi = [&](int k, int c) { return int(ram.u32(q300 + 16 * uint32_t(k - 300) + 4 * uint32_t(c)) & 0xFFFF); };
        const float pa = q(300, 0), cPos = q(301, 0), cExp = q(301, 1), f6 = q(301, 3), minSize = q(302, 0);
        const int N = qi(302, 1), half = qi(302, 2), batch = qi(302, 3), total = qi(306, 0);
        if (N < 2 || N > 64 || batch < 1 || total != (s.n - 1) * N) return;
        float W3[4], W4[4];
        for (int c = 0; c < 4; ++c) { W3[c] = q(303, c); W4[c] = q(304, c); }
        const float M2[3] = {q(305, 0), q(305, 1), q(305, 2)};
        const float cc = (N & 1) ? 0.125f : 0.5f;
        const uint64_t t0 = systemTex0(ram, s.base);
        const float tw = 16.f * float(1u << ((t0 >> 26) & 15)), th = 16.f * float(1u << ((t0 >> 30) & 15));
        struct V3 { float v[3]; };
        auto P = [&](int i) { return V3{{ram.f32(pos + 16 * uint32_t(i)), ram.f32(pos + 16 * uint32_t(i) + 4), ram.f32(pos + 16 * uint32_t(i) + 8)}}; };
        auto wbits = [&](int i) { return ram.u32(pos + 16 * uint32_t(i) + 12); };
        auto color = [&](float cpos) {
            const int k0 = std::clamp(int(cpos), 0, 31), k1 = std::min(k0 + 1, 31);
            const float fr = cpos - float(k0);
            uint32_t out = 0;
            for (int c = 0; c < 4; ++c) {
                const float a = ram.u8(colAt + 4 * uint32_t(k0) + uint32_t(c)), b = ram.u8(colAt + 4 * uint32_t(k1) + uint32_t(c));
                out |= uint32_t(std::clamp(int(a + 0.5f - a * fr + b * fr), 0, 255)) << (8 * c);
            }
            return out;
        };
        auto wOf = [&](const float p[3]) {
            if (view.fogW[0] == 0.f && view.fogW[1] == 0.f && view.fogW[2] == 0.f)
                return view.depth[0] * p[0] + view.depth[1] * p[1] + view.depth[2] * p[2] + view.depth[3];
            return view.fogW[0] * p[0] + view.fogW[1] * p[1] + view.fogW[2] * p[2] + view.fogW[3];
        };
        uint32_t R = vuRinit(wbits(s.n - 1));
        float r = vuRnext(R);
        float aN = q(306, 1);
        int step = 0;
        V3 Pm, A = P(s.n - 1), B = P(s.n - 2), Pn;
        {
            float d = 0.f;
            for (int a = 0; a < 3; ++a) d += (B.v[a] - A.v[a]) * M2[a];
            for (int a = 0; a < 3; ++a) Pm.v[a] = B.v[a] - M2[a] * (2.f * d);
        }
        for (int i = s.n - 1; i >= 1; --i) {
            if (i >= 2) Pn = P(i - 2);
            else for (int a = 0; a < 3; ++a) Pn.v[a] = 2.f * B.v[a] - A.v[a];
            float D1[3], D2[3], D2b[3], p[3];
            for (int a = 0; a < 3; ++a) {
                D1[a] = Pm.v[a] * W3[0] + A.v[a] * W3[1] + B.v[a] * W3[2] + Pn.v[a] * W3[3];
                D2[a] = Pm.v[a] * W4[0] + A.v[a] * W4[1] + B.v[a] * W4[2] + Pn.v[a] * W4[3];
                D2b[a] = Pm.v[a] * W4[3] + A.v[a] * W4[2] + B.v[a] * W4[1] + Pn.v[a] * W4[0];
                p[a] = A.v[a];
            }
            R = vuRinit(wbits(i));
            const bool twiceJ = step % batch != 0;   // 1o passo de trecho que nao comeca lote
            for (int k = 0; k < N; ++k, ++step) {
                const float j = 1.f + pa * (2.f - r);
                const float side = (minSize + cExp * aN) * (1.f + (r - 1.f) * f6);
                const float cpos = std::min(31.f, cPos * aN * j * (k == 0 && twiceJ ? j : 1.f));
                r = vuRnext(R);
                const uint32_t uvq = uvAt + 8 * (R & 3);
                const float u0 = ram.u16(uvq) / tw, v0 = ram.u16(uvq + 2) / th, u1 = ram.u16(uvq + 4) / tw, v1 = ram.u16(uvq + 6) / th;
                if (wOf(p) >= 1.f) {
                    const float h = 0.5f * side;
                    float cs[4][3];
                    for (int a = 0; a < 3; ++a) {
                        const float rx = view.right[a] * h, uy = view.up[a] * h;   // y da tela para baixo = -up
                        cs[0][a] = p[a] - rx + uy; cs[1][a] = p[a] + rx + uy;
                        cs[2][a] = p[a] + rx - uy; cs[3][a] = p[a] - rx - uy;
                    }
                    const float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
                    g(step, cs, uv, color(cpos));
                }
                aN += 1.f;
                for (int a = 0; a < 3; ++a) { p[a] += D1[a]; D1[a] += D2[a]; }
                if (N > 2 && N - 1 - k == half)
                    for (int a = 0; a < 3; ++a) { D1[a] = D1[a] - cc * D2[a] + cc * D2b[a]; D2[a] = D2b[a]; }
            }
            Pm = A; A = B; B = Pn;
        }
    }

    // Geometria do sistema em coordenadas de mundo, um quadrado por vez: f(i, cantos[4][3], uv[4][2]), i = particula (nas faixas,
    // a ponta de i a i + 1; cantos 0 e 1 em i, 2 e 3 em i + 1). Linhas (FX_LINE) nao passam por aqui. Serve ao draw e a checagem VU1.
    template <class F> static void forEachQuad(const Ram &ram, const FxSystem &s, const FxView &view, F &&emit) {
        auto f = [&](int i, const float c[4][3], const float uv[4][2]) { if (!fogged(view, c)) emit(i, c, uv); };
        const uint32_t b = s.base;
        const uint32_t pos = ram.u32(b + fxoff::pos), vel = ram.u32(b + fxoff::vel);
        const bool haveVel = ram.ok(vel, 16 * uint32_t(s.n));
        const float *back = view.back;
        auto P = [&](int i, float out[4]) { for (int c = 0; c < 4; ++c) out[c] = ram.f32(pos + 16 * uint32_t(i) + 4 * uint32_t(c)); };
        auto V = [&](int i, float out[3]) { for (int c = 0; c < 3; ++c) out[c] = haveVel ? ram.f32(vel + 16 * uint32_t(i) + 4 * uint32_t(c)) : 0.f; };
        float c[4][3], uv[4][2];
        if (s.type == FX_LINE) return;
        if (s.type == FX_ARC || s.type == FX_ARC2) {
            arcQuads(ram, s, view, [&](int k, const float cc[4][3], const float uu[4][2], const uint32_t *) { f(k, cc, uu); });
            return;
        }
        if (s.type == FX_SPRINE) {
            sprineQuads(ram, s, view, [&](int k, const float cc[4][3], const float uu[4][2], uint32_t) { f(k, cc, uu); });
            return;
        }
        if (isStrip(s.type)) {
            // Faixa ligando particulas consecutivas, de meia-largura |pos.w|. Como no VU1 (medido pela checagem em stripe: a aresta de
            // cada ponta sai perpendicular a faixa na tela), a ponta i abre perpendicular, na tela, a direcao entre as vizinhas
            // (i - 1 e i + 1), com a meia-largura no plano de vista (projeta em |pos.w| * foco / profundidade).
            // stripe: o stripePacketBuild manda ao VU1 a idade da particula mais nova (n - 1), e a largura sai descontada do quanto ela
            // ja cresceu: com o emissor parado a faixa inteira afina. Medido pela checagem (preset 106): meia-largura =
            // |pos.w| - |pos.w[n - 1]| + minSize (com o emissor ativo, a mais nova tem idade ~0 e da |pos.w|).
            // Polilinha das particulas (stripe, sweepTrail), cada ponto com a meia-largura e a particula (para a cor no draw).
            struct Pt { float p[3], hw, u; int seg; };
            std::vector<Pt> pts;
            pts.reserve(size_t(s.n));
            {
                float wHead = 0.f;
                if (s.type == FX_STRIPE) { float ph[4]; P(s.n - 1, ph); wHead = std::fabs(ph[3]) - ram.f32(b + fxoff::minSize); }
                for (int i = 0; i < s.n; ++i) {
                    float q[4];
                    P(i, q);
                    if (!std::isfinite(q[0])) continue;
                    pts.push_back({{q[0], q[1], q[2]}, std::max(std::fabs(q[3]) - wHead, 0.f), float(i) / float(std::max(s.n - 1, 1)), i});
                }
            }
            auto scr = [&](const float *q, float &x, float &y) {
                const float z = view.depth[0] * q[0] + view.depth[1] * q[1] + view.depth[2] * q[2] + view.depth[3];
                const float iz = 1.f / std::max(z, 1e-3f);
                x = (view.ox[0] * q[0] + view.ox[1] * q[1] + view.ox[2] * q[2] + view.ox[3]) * iz;
                y = (view.oy[0] * q[0] + view.oy[1] * q[1] + view.oy[2] * q[2] + view.oy[3]) * iz;
            };
            auto off = [&](size_t i, float o[3]) {
                float xa, ya, xb, yb;
                scr(pts[i > 0 ? i - 1 : 0].p, xa, ya); scr(pts[std::min(i + 1, pts.size() - 1)].p, xb, yb);
                float nx = -(yb - ya), ny = xb - xa;
                const float l = std::sqrt(nx * nx + ny * ny);
                if (!(l > 1e-9f)) return false;
                nx *= pts[i].hw / l; ny *= pts[i].hw / l;
                for (int a = 0; a < 3; ++a) o[a] = view.right[a] * nx + view.up[a] * ny;
                return true;
            };
            for (size_t i = 0; i + 1 < pts.size(); ++i) {
                const Pt &a0 = pts[i], &a1 = pts[i + 1];
                if (a1.seg != a0.seg + 1) continue;   // particula invalida no meio: sem ponte
                float o0[3], o1[3];
                if (!off(i, o0) || !off(i + 1, o1)) continue;
                for (int a = 0; a < 3; ++a) {
                    c[0][a] = a0.p[a] - o0[a]; c[1][a] = a0.p[a] + o0[a];
                    c[2][a] = a1.p[a] + o1[a]; c[3][a] = a1.p[a] - o1[a];
                }
                uv[0][0] = a0.u; uv[0][1] = 0.f; uv[1][0] = a0.u; uv[1][1] = 1.f;
                uv[2][0] = a1.u; uv[2][1] = 1.f; uv[3][0] = a1.u; uv[3][1] = 0.f;
                f(a0.seg, c, uv);
            }
            return;
        }
        if (s.type == FX_PEDS) {
            // Pedestres (pedsCreate/stampPacketInit, PedGroup): carimbo em pe no Z do mundo, base em pos, largura info.minSize na
            // horizontal da camera e altura info.maxSize (medido pela checagem VU1 x nativo: a inclinacao na tela segue a perspectiva
            // de um eixo vertical e o centro fica meia altura acima de pos). pos.w nao e tamanho: e float(variante de cor) (scatter)
            // com a meia-palavra baixa trocada pelo quadro da animacao (update). A folha tem os quadros em U (texMask + 1) e as
            // variantes em V (vis.numColors), du/dv do TextureInfo em 1/16 de texel.
            const float W = ram.f32(b + fxoff::minSize), H = ram.f32(b + fxoff::maxSize);
            float rh[3] = {view.right[0], view.right[1], 0.f};
            const float rl = std::sqrt(rh[0] * rh[0] + rh[1] * rh[1]);
            if (rl < 1e-6f) return;
            rh[0] /= rl; rh[1] /= rl;
            const uint32_t tinfo = fxaddr::textureInfo + 8 * (ram.u32(b + fxoff::texInfoIndex) & 127);
            const uint32_t k = ram.u16(tinfo + 2) & 127;
            const uint64_t t0 = uint64_t(ram.u32(fxaddr::tex0 + 8 * k)) | (uint64_t(ram.u32(fxaddr::tex0 + 8 * k + 4)) << 32);
            const float texW = float(1u << ((t0 >> 26) & 15)), texH = float(1u << ((t0 >> 30) & 15));
            const uint32_t frames = ram.u32(b + fxoff::texMask) + 1, rows = std::max<uint32_t>(ram.u32(b + fxoff::numColors), 1);
            const float du = float(ram.u16(tinfo + 4) >> 4) / float(frames) / texW, dv = float(ram.u16(tinfo + 6) >> 4) / float(rows) / texH;
            for (int i = 0; i < s.n; ++i) {
                float p[4];
                P(i, p);
                uint32_t wbits;
                std::memcpy(&wbits, &p[3], 4);
                if (int32_t(wbits) < 0 || !std::isfinite(p[0])) continue;   // pedsTurnOff: w = -1
                const uint32_t vbits = wbits & 0xFFFF0000u;
                float variant;
                std::memcpy(&variant, &vbits, 4);
                const float u0 = float((wbits & 0xFFFF) % frames) * du, v0 = float(uint32_t(std::max(variant, 0.f)) % rows) * dv;
                for (int a = 0; a < 3; ++a) {
                    c[0][a] = p[a] - rh[a] * W * 0.5f; c[1][a] = p[a] + rh[a] * W * 0.5f;
                    c[2][a] = c[1][a]; c[3][a] = c[0][a];
                }
                c[2][2] += H; c[3][2] += H;
                uv[0][0] = u0; uv[0][1] = v0 + dv; uv[1][0] = u0 + du; uv[1][1] = v0 + dv;
                uv[2][0] = u0 + du; uv[2][1] = v0; uv[3][0] = u0; uv[3][1] = v0;
                f(i, c, uv);
            }
            return;
        }
        // Sprites (sprite, texSprite, dustTrail, frag, kite, wake, strite): um quad por particula
        const bool dust = isDustTex(systemTex0(ram, b));
        for (int i = 0; i < s.n; ++i) {
            float p[4];
            P(i, p);
            if (!std::isfinite(p[0]) || !std::isfinite(p[3])) continue;
            // microcodigo de sprites: meio-lado na tela = pos.w / 2 * foco / profundidade, limitado a foco / 2 (pos.w e o lado em mundo;
            // medido em sprite e texSprite). Os outros tipos que caem aqui continuam com pos.w inteiro (nao conferidos).
            const float depth = view.depth[0] * p[0] + view.depth[1] * p[1] + view.depth[2] * p[2] + view.depth[3];
            float half = (s.type == FX_SPRITE || s.type == FX_TEXSPRITE ? 0.5f : 1.f) * std::fabs(p[3]), cap = 0.5f;
            if (s.type == FX_DUSTTRAIL) {
                // dustTrail (particleEmit, is15): pos.w e a idade normalizada (0..1, vel.w = 1/vida) e o byte 3 da palavra de idade e o
                // sorteio de tamanho (de min*255/max a 255). Medido pela checagem VU1 x nativo em 5 presets: meio-lado =
                // S/2 + exp*vida*w/2, limitado a 0,375 * profundidade; o termo da idade e exato, S ~ 1,12*max*a3/255 - 0,3*min e
                // aproximado (erro ate ~4%; a formula do microcodigo para o byte nao foi achada, deve envolver info +0x2D88).
                const uint32_t ages = ram.u32(b + fxoff::age);
                const float a3 = float(ram.u8(ages + 4 * uint32_t(i) + 3));
                const float mn = ram.f32(b + fxoff::minSize), mx = ram.f32(b + fxoff::maxSize);
                const float S = 1.12f * mx * a3 / 255.f - 0.3f * mn;
                half = std::max(0.5f * S + 0.5f * ram.f32(b + 0x2E44) * float(ram.s16(b + fxoff::lifetime)) * p[3], 0.f);
                cap = 0.375f;
            }
            const float r = depth > 0.f ? std::min(half, cap * depth) : half;
            float ax[3] = {view.right[0] * r, view.right[1] * r, view.right[2] * r}, ay[3] = {view.up[0] * r, view.up[1] * r, view.up[2] * r};
            float cpos[3] = {p[0], p[1], p[2]};
            if (s.type == FX_STRITE) {
                // microcodigo do strite: cabeca pos e cauda pos - vel projetadas, e a faixa esticada 2x para cada lado
                // (de pos + 2*vel a pos - 3*vel); a largura sai de pos.w. Aqui no plano da tela, centrada em pos - vel/2.
                float v[3];
                V(i, v);
                const float vd = v[0] * back[0] + v[1] * back[1] + v[2] * back[2];
                float vs[3] = {v[0] - back[0] * vd, v[1] - back[1] * vd, v[2] - back[2] * vd};
                const float vl = std::sqrt(vs[0] * vs[0] + vs[1] * vs[1] + vs[2] * vs[2]);
                if (vl > 1e-4f) {
                    for (int a = 0; a < 3; ++a) vs[a] /= vl;
                    const float side[3] = {vs[1] * back[2] - vs[2] * back[1], vs[2] * back[0] - vs[0] * back[2], vs[0] * back[1] - vs[1] * back[0]};
                    const float len = 2.5f * vl + r * 0.5f;
                    for (int a = 0; a < 3; ++a) { ax[a] = vs[a] * len; ay[a] = side[a] * r * 0.5f; cpos[a] = p[a] - 0.5f * v[a]; }
                }
            }
            // conjunto de UV: os bits baixos de pos.w indexam os cantos que o spritePacketInit sobe (VU 0x168): 3 normal, 1 espelha U,
            // 2 espelha V, 0 os dois. Com info.attribs & 0x2000 sao 16 conjuntos (giros tambem); aqui so os 4 espelhamentos.
            uint32_t wbits;
            std::memcpy(&wbits, &p[3], 4);
            const bool fu = (wbits & 2) == 0, fv = (wbits & 1) == 0;
            float u0 = fu ? 1.f : 0.f, u1 = fu ? 0.f : 1.f, v0 = fv ? 1.f : 0.f, v1 = fv ? 0.f : 1.f;
            if (dust) {   // celula wbits & 3 da textura composta: (9, 9, 503, 503) + 512 * (k & 1, k >> 1), em 1/16 de texel de 64
                const float cu = float(wbits & 1) * 0.5f, cv = float((wbits >> 1) & 1) * 0.5f;
                u0 = cu + 9.f / 1024.f; u1 = cu + 503.f / 1024.f; v0 = cv + 9.f / 1024.f; v1 = cv + 503.f / 1024.f;
            }
            const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
            for (int k = 0; k < 4; ++k)
                for (int a = 0; a < 3; ++a) c[k][a] = cpos[a] + ax[a] * sx[k] + ay[a] * sy[k];
            // O VU1 manda strite e wake como UM triangulo por particula (checagem VU1 x nativo); aqui o quad degenera (canto 3 = canto 0).
            if (s.type == FX_STRITE) {
                float v[3];
                V(i, v);
                if (!striteTriangle(ram, b, p, v, c))   // sem a camera do jogo: ponta no meio dos cantos 0 e 3 (aproximacao antiga)
                    for (int a = 0; a < 3; ++a) c[0][a] = c[3][a] = cpos[a] - ax[a];
            } else if (s.type == FX_WAKE) {
                // triangulo equilatero no plano XY do mundo, centrado em pos, um vertice em +X; raio pelo pos.w.
                // Medido so no preset 146 (minSize = maxSize = 90), w de 0 a 0,94: R = 45 + 120 w + 7,5 s (2 - s), s = min(w / 0,235, 1)
                // (erro < 0,4 unidade; o termo que satura e ajuste, nao formula do microprograma).
                const float sw = std::min(std::fabs(p[3]) / 0.235f, 1.f);
                const float R = 0.5f * ram.f32(b + fxoff::minSize) + 120.1f * p[3] + 7.5f * sw * (2.f - sw);
                const float dx[3] = {1.f, -0.5f, -0.5f}, dy[3] = {0.f, 0.8660254f, -0.8660254f};
                for (int k = 0; k < 3; ++k) { c[k][0] = p[0] + R * dx[k]; c[k][1] = p[1] + R * dy[k]; c[k][2] = p[2]; }
                for (int a = 0; a < 3; ++a) c[3][a] = c[0][a];
            }
            uv[0][0] = u0; uv[0][1] = v1; uv[1][0] = u1; uv[1][1] = v1;
            uv[2][0] = u1; uv[2][1] = v0; uv[3][0] = u0; uv[3][1] = v0;
            f(i, c, uv);
        }
    }

private:
    unsigned blob_ = 0;   // textura de reserva (disco suave) quando a do sistema nao decodifica
    std::unordered_map<uint32_t, float> flarePrev_;   // debounce do lens flare: fracao visivel do quadro anterior por ID

    unsigned blobTexture() {
        if (blob_) return blob_;
        const int n = 64;
        std::vector<uint32_t> px(size_t(n * n));
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                const float dx = (x + 0.5f) / n * 2.f - 1.f, dy = (y + 0.5f) / n * 2.f - 1.f;
                const float r = std::sqrt(dx * dx + dy * dy);
                const float a = std::max(0.f, 1.f - r);
                px[size_t(y * n + x)] = 0x00FFFFFFu | (uint32_t(a * a * 128.f) << 24);   // alfa do GS: 0x80 = 1,0
            }
        Image img{px.data(), n, n, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        const Texture2D t = LoadTextureFromImage(img);
        if (t.id) SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        blob_ = t.id;
        return blob_;
    }

    // Registrador ALPHA do GS -> modo de mistura do raylib. Campos de 2 bits: A, B, C (0 Cs/As, 1 Cd/Ad, 2 0/FIX), D.
    static constexpr int kBlendSubtract = -2;   // Cd - Cs*As: o BLEND_SUBTRACT_COLORS do raylib faz Cs - Cd
    static int blendFor(uint32_t alpha) {
        const uint32_t A = alpha & 3, B = (alpha >> 2) & 3, C = (alpha >> 4) & 3, D = (alpha >> 6) & 3;
        if (A == 0 && B == 2 && C == 0 && D == 1) return BLEND_ADDITIVE;        // Cs*As + Cd
        if (A == 2 && B == 0 && C == 0 && D == 1) return kBlendSubtract;        // Cd - Cs*As
        return BLEND_ALPHA;                                                     // (Cs-Cd)*As + Cd e o resto
    }
    static void setBlend(int mode) {
        if (mode == kBlendSubtract) { rlSetBlendFactors(RL_SRC_ALPHA, RL_ONE, RL_FUNC_REVERSE_SUBTRACT); rlSetBlendMode(RL_BLEND_CUSTOM); }
        else rlSetBlendMode(mode);
    }

    // RGBAQ do GS (0x80 = 1,0) cru: o modo GS do shader (shaderNormal) faz texel * Cv / 128 e At * Av / 16384
    static void vcolor(uint32_t c) { rlColor4ub(uint8_t(c & 255), uint8_t((c >> 8) & 255), uint8_t((c >> 16) & 255), uint8_t(c >> 24)); }

    // Quadro de textura do texSprite (texSpritePacketBuild)
    static uint32_t texFrame(const Ram &ram, uint32_t b, int i, int age) {
        const uint32_t frames = ram.u32(b + fxoff::texMask), attr = ram.u32(b + fxoff::infoAttribs);
        if (frames == 0) return 0;
        if (attr & 1) return uint32_t(i * 2654435761u >> 16) % frames;   // retail: rand() a cada quadro; aqui fixo por particula
        if (attr & 2) return (ram.u8(b + fxoff::uvI + uint32_t(i)) >> 2) % frames;
        const uint32_t f = (uint32_t(std::max(age, 0)) * ram.u32(b + fxoff::mapsPerFrame)) >> 12;
        if (attr & 4) return std::min(f, frames - 1);
        return f % frames;
    }

    void drawSystem(const Ram &ram, const FxSystem &s, const TexFn &texture, const FxView &view) {
        const uint32_t b = s.base;
        const uint32_t pos = ram.u32(b + fxoff::pos), vel = ram.u32(b + fxoff::vel), age = ram.u32(b + fxoff::age);
        const uint32_t ti = ram.u32(b + fxoff::texInfoIndex) & 127;
        const uint32_t tinfo = fxaddr::textureInfo + 8 * ti;
        const int16_t numTex = ram.s16(tinfo);
        const uint32_t texIndex = ram.u16(tinfo + 2);
        const bool haveVel = ram.ok(vel, 16 * uint32_t(s.n));
        const bool haveAge = ram.ok(age, 2 * uint32_t(s.n));

        auto texFor = [&](uint32_t frame) -> unsigned {
            const uint32_t k = (texIndex + frame) & 127;
            const uint32_t id = ram.u16(fxaddr::texId + 2 * k);
            const uint64_t t0 = uint64_t(ram.u32(fxaddr::tex0 + 8 * k)) | (uint64_t(ram.u32(fxaddr::tex0 + 8 * k + 4)) << 32);
            unsigned gl = (numTex > 0 && id) ? texture(id, t0) : isDustTex(t0) ? texture(kDustTexId, t0) : 0;
            return gl ? gl : blobTexture();
        };

        rlDrawRenderBatchActive();
        setBlend(blendFor(ram.u32(b + fxoff::alphaContext)));
        // neblina no shader do Scene::draw: o XGKICK das particulas leva PRIM.FGE em todos os tipos medidos pela checagem VU1 (sprite,
        // texSprite, stripe, strite, arco, sprine, frag, pedestre, dustTrail); alfa do GS (texturas com alfa cru); o draw zera no fim
        shaderNormal(0.f, true, true);
        auto A = [&](int i) { return haveAge ? int(ram.s16(age + 2 * uint32_t(i))) : 0; };

        if (s.type == FX_LINE) {
            rlBegin(RL_LINES);
            rlSetTexture(gsWhiteTexture());   // sem textura no modo GS: alfa 0x80
            for (int i = 0; i < s.n; ++i) {
                float p[3], v[3];
                for (int c = 0; c < 3; ++c) {
                    p[c] = ram.f32(pos + 16 * uint32_t(i) + 4 * uint32_t(c));
                    v[c] = haveVel ? ram.f32(vel + 16 * uint32_t(i) + 4 * uint32_t(c)) : 0.f;
                }
                if (!std::isfinite(p[0])) continue;
                const uint32_t c = colorAt(ram, b, A(i));
                vcolor(c);
                rlVertex3f(p[0], p[1], p[2]);
                vcolor(c & 0x00FFFFFFu);   // microcodigo de linhas: a cauda e pos - vel (projetada com a matriz do quadro anterior)
                rlVertex3f(p[0] - v[0], p[1] - v[1], p[2] - v[2]);
            }
            rlEnd();
            return;
        }

        if (s.type == FX_ARC || s.type == FX_ARC2) {   // fita com a cor de cada ponto (arcQuads)
            rlBegin(RL_QUADS);   // rlBegin antes de rlSetTexture (ver a armadilha em wotm_native.hpp)
            rlSetTexture(texFor(0));
            arcQuads(ram, s, view, [&](int, const float c[4][3], const float uv[4][2], const uint32_t col[2]) {
                if (fogged(view, c)) return;
                for (int k = 0; k < 4; ++k) {
                    vcolor(k < 2 ? col[0] : col[1]);
                    rlTexCoord2f(uv[k][0], uv[k][1]);
                    rlVertex3f(c[k][0], c[k][1], c[k][2]);
                }
            });
            rlEnd();
            rlSetTexture(0);
            return;
        }

        if (s.type == FX_SPRINE) {   // sprites da curva com a cor de cada passo (sprineQuads)
            rlBegin(RL_QUADS);
            rlSetTexture(texFor(0));
            sprineQuads(ram, s, view, [&](int, const float c[4][3], const float uv[4][2], uint32_t col) {
                if (fogged(view, c)) return;
                for (int k = 0; k < 4; ++k) {
                    vcolor(col);
                    rlTexCoord2f(uv[k][0], uv[k][1]);
                    rlVertex3f(c[k][0], c[k][1], c[k][2]);
                }
            });
            rlEnd();
            rlSetTexture(0);
            return;
        }

        // faixas: uma textura e a cor de cada ponta; sprites: textura (quadro do texSprite) e cor por particula
        const bool strip = isStrip(s.type), perFrame = s.type == FX_TEXSPRITE;
        const unsigned stripTex = strip ? texFor(0) : 0;
        unsigned cur = ~0u;
        bool open = false;
        forEachQuad(ram, s, view, [&](int i, const float c[4][3], const float uv[4][2]) {
            const int ag = A(i);
            const unsigned tex = strip ? stripTex : texFor(perFrame ? texFrame(ram, b, i, ag) : 0);
            if (tex != cur || !open) {
                if (open) { rlEnd(); rlDrawRenderBatchActive(); }
                rlBegin(RL_QUADS);   // rlBegin antes de rlSetTexture (ver a armadilha em wotm_native.hpp)
                rlSetTexture(tex);
                cur = tex;
                open = true;
            }
            // pedestres: vis.colorIndex e a propria cor RGBA (stampPacketInit a sobe byte a byte); numColors conta as variantes da folha
            const uint32_t c0 = s.type == FX_PEDS ? ram.u32(b + fxoff::colorIndex) : colorAt(ram, b, ag), c1 = strip ? colorAt(ram, b, A(i + 1)) : c0;
            for (int k = 0; k < 4; ++k) {
                vcolor(k < 2 ? c0 : c1);
                rlTexCoord2f(uv[k][0], uv[k][1]);
                rlVertex3f(c[k][0], c[k][1], c[k][2]);
            }
        });
        if (open) rlEnd();
        rlSetTexture(0);
    }
};

}  // namespace wotm
