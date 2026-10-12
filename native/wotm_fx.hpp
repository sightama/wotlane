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
//   - sprite: meio-lado na tela = pos.w * foco / profundidade (foco = o mesmo da matriz de projecao), limitado a foco / 2; ou seja,
//     pos.w e o meio-lado em unidades de mundo e nunca passa de metade da profundidade. Os bits baixos de pos.w indexam os cantos de
//     UV (3 normal, 1 espelha U, 2 espelha V, 0 os dois); a cor sai da tabela pela idade, como no loadColors
//   - linha: de pos ate pos - vel (a cauda e projetada com a matriz do quadro anterior: borrao de movimento da camera)
//   - strite: cabeca pos e cauda pos - vel, esticada 2x para cada lado (de pos + 2*vel a pos - 3*vel), largura por pos.w
// Destroços (frag, 13) nao passam por aqui: Scene::collectParticles/drawSprites (atlas de celulas 8x8, pos.w = giro, interpolados).
// Ainda aproximado: as faixas (sprine, stripe, sweepTrail) ligam particulas consecutivas; kite, wake e dustTrail viram sprites;
// a cauda das linhas e do strite usa a matriz atual.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>

#include "raylib.h"
#include "rlgl.h"

namespace wotm {

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
    FX_LINE = 1, FX_SWEEPTRAIL = 2, FX_SPRITE = 3, FX_TEXSPRITE = 4, FX_STRIPE = 5, FX_STRITE = 6, FX_SPRINE = 8, FX_KITE = 9,
    FX_FRAG = 13, FX_DUSTTRAIL = 15, FX_WAKE = 16,
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

class Particles {
public:
    // texture(texId, tex0) -> id de textura do raylib (0 = sem textura); fornecido pela Scene (paletas do .RTX)
    using TexFn = std::function<unsigned(uint32_t texId, uint64_t tex0)>;

    std::vector<FxSystem> systems;

    // Reune os sistemas desenhaveis (mesmos testes de particleDraw, menos o de campo de visao). A ordem sai no draw (camera atual).
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
            if (built && s.type != 11 && s.type != FX_FRAG && s.n >= minN && s.n <= std::max(maxN, 1) && s.n <= 1024 && ram.ok(ram.u32(b + fxoff::pos), 16 * uint32_t(s.n)))
                systems.push_back(s);
            i = next;
        }
        static const bool list = std::getenv("WOTM_FXLIST") != nullptr;
        if (list) dump(ram);
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
    void draw(const Ram &ram, const TexFn &texture) {
        static const bool off = std::getenv("WOTM_NOFX") != nullptr;
        if (off || systems.empty()) return;
        const Matrix mv = rlGetMatrixModelview();
        const float right[3] = {mv.m0, mv.m4, mv.m8}, up[3] = {mv.m1, mv.m5, mv.m9}, back[3] = {mv.m2, mv.m6, mv.m10};
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
        for (const FxSystem &s : systems) drawSystem(ram, s, texture, mv, right, up, back);
        rlDrawRenderBatchActive();
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

private:
    unsigned blob_ = 0;   // textura de reserva (disco suave) quando a do sistema nao decodifica

    unsigned blobTexture() {
        if (blob_) return blob_;
        const int n = 64;
        std::vector<uint32_t> px(size_t(n * n));
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                const float dx = (x + 0.5f) / n * 2.f - 1.f, dy = (y + 0.5f) / n * 2.f - 1.f;
                const float r = std::sqrt(dx * dx + dy * dy);
                const float a = std::max(0.f, 1.f - r);
                px[size_t(y * n + x)] = 0x00FFFFFFu | (uint32_t(a * a * 255.f) << 24);
            }
        Image img{px.data(), n, n, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        const Texture2D t = LoadTextureFromImage(img);
        if (t.id) SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
        blob_ = t.id;
        return blob_;
    }

    // Registrador ALPHA do GS -> modo de mistura do raylib. Campos de 2 bits: A, B, C (0 Cs/As, 1 Cd/Ad, 2 0/FIX), D.
    static int blendFor(uint32_t alpha) {
        const uint32_t A = alpha & 3, B = (alpha >> 2) & 3, C = (alpha >> 4) & 3, D = (alpha >> 6) & 3;
        if (A == 0 && B == 2 && C == 0 && D == 1) return BLEND_ADDITIVE;        // Cs*As + Cd
        if (A == 2 && B == 0 && C == 0 && D == 1) return BLEND_SUBTRACT_COLORS; // Cd - Cs*As
        return BLEND_ALPHA;                                                     // (Cs-Cd)*As + Cd e o resto
    }

    static void vcolor(uint32_t c) {
        const int r = int(c & 255), g = int((c >> 8) & 255), b = int((c >> 16) & 255), a = int(c >> 24);
        rlColor4ub(uint8_t(std::min(255, r * 2)), uint8_t(std::min(255, g * 2)), uint8_t(std::min(255, b * 2)), uint8_t(std::min(255, a * 2)));
    }

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

    void drawSystem(const Ram &ram, const FxSystem &s, const TexFn &texture, const Matrix &mv, const float right[3], const float up[3], const float back[3]) {
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
            unsigned gl = (numTex > 0 && id) ? texture(id, t0) : 0;
            return gl ? gl : blobTexture();
        };

        rlDrawRenderBatchActive();
        rlSetBlendMode(blendFor(ram.u32(b + fxoff::alphaContext)));

        auto P = [&](int i, float out[4]) { for (int c = 0; c < 4; ++c) out[c] = ram.f32(pos + 16 * uint32_t(i) + 4 * uint32_t(c)); };
        auto V = [&](int i, float out[3]) { for (int c = 0; c < 3; ++c) out[c] = haveVel ? ram.f32(vel + 16 * uint32_t(i) + 4 * uint32_t(c)) : 0.f; };
        auto A = [&](int i) { return haveAge ? int(ram.s16(age + 2 * uint32_t(i))) : 0; };

        if (s.type == FX_LINE) {
            rlSetTexture(0);
            rlBegin(RL_LINES);
            for (int i = 0; i < s.n; ++i) {
                float p[4], v[3];
                P(i, p); V(i, v);
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

        if (s.type == FX_SPRINE || s.type == FX_STRIPE || s.type == FX_SWEEPTRAIL) {
            // Faixa ligando particulas consecutivas, de largura 2*pos.w, virada para a camera
            const unsigned tex = texFor(0);
            rlSetTexture(tex);
            rlBegin(RL_QUADS);
            for (int i = 0; i + 1 < s.n; ++i) {
                float p0[4], p1[4];
                P(i, p0); P(i + 1, p1);
                if (!std::isfinite(p0[0]) || !std::isfinite(p1[0])) continue;
                float d[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
                float w[3] = {d[1] * back[2] - d[2] * back[1], d[2] * back[0] - d[0] * back[2], d[0] * back[1] - d[1] * back[0]};
                const float l = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
                if (l < 1e-6f) continue;
                const float r0 = std::fabs(p0[3]) / l, r1 = std::fabs(p1[3]) / l;
                const float u0 = float(i) / float(s.n - 1), u1 = float(i + 1) / float(s.n - 1);
                const uint32_t c0 = colorAt(ram, b, A(i)), c1 = colorAt(ram, b, A(i + 1));
                vcolor(c0); rlTexCoord2f(u0, 0.f); rlVertex3f(p0[0] - w[0] * r0, p0[1] - w[1] * r0, p0[2] - w[2] * r0);
                vcolor(c0); rlTexCoord2f(u0, 1.f); rlVertex3f(p0[0] + w[0] * r0, p0[1] + w[1] * r0, p0[2] + w[2] * r0);
                vcolor(c1); rlTexCoord2f(u1, 1.f); rlVertex3f(p1[0] + w[0] * r1, p1[1] + w[1] * r1, p1[2] + w[2] * r1);
                vcolor(c1); rlTexCoord2f(u1, 0.f); rlVertex3f(p1[0] - w[0] * r1, p1[1] - w[1] * r1, p1[2] - w[2] * r1);
            }
            rlEnd();
            rlSetTexture(0);
            return;
        }

        // Sprites (sprite, texSprite, dustTrail, frag, kite, wake, strite): um quad por particula
        const bool perFrame = s.type == FX_TEXSPRITE;
        unsigned cur = ~0u;
        bool open = false;
        for (int i = 0; i < s.n; ++i) {
            float p[4];
            P(i, p);
            if (!std::isfinite(p[0]) || !std::isfinite(p[3])) continue;
            const int ag = A(i);
            const unsigned tex = texFor(perFrame ? texFrame(ram, b, i, ag) : 0);
            if (tex != cur || !open) {
                if (open) { rlEnd(); rlDrawRenderBatchActive(); }
                rlBegin(RL_QUADS);   // rlBegin antes de rlSetTexture (ver a armadilha em wotm_native.hpp)
                rlSetTexture(tex);
                cur = tex;
                open = true;
            }
            // microcodigo de sprites: meio-lado na tela = pos.w * foco / profundidade, limitado a foco / 2 (pos.w e o raio em mundo)
            const float depth = -(mv.m2 * p[0] + mv.m6 * p[1] + mv.m10 * p[2] + mv.m14);
            const float r = depth > 0.f ? std::min(std::fabs(p[3]), 0.5f * depth) : std::fabs(p[3]);
            float ax[3] = {right[0] * r, right[1] * r, right[2] * r}, ay[3] = {up[0] * r, up[1] * r, up[2] * r};
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
                    for (int c = 0; c < 3; ++c) vs[c] /= vl;
                    const float side[3] = {vs[1] * back[2] - vs[2] * back[1], vs[2] * back[0] - vs[0] * back[2], vs[0] * back[1] - vs[1] * back[0]};
                    const float len = 2.5f * vl + r * 0.5f;
                    for (int c = 0; c < 3; ++c) { ax[c] = vs[c] * len; ay[c] = side[c] * r * 0.5f; cpos[c] = p[c] - 0.5f * v[c]; }
                }
            }
            // conjunto de UV: os bits baixos de pos.w indexam os cantos que o spritePacketInit sobe (VU 0x168): 3 normal, 1 espelha U,
            // 2 espelha V, 0 os dois. Com info.attribs & 0x2000 sao 16 conjuntos (giros tambem); aqui so os 4 espelhamentos.
            uint32_t wbits;
            std::memcpy(&wbits, &p[3], 4);
            const bool fu = (wbits & 2) == 0, fv = (wbits & 1) == 0;
            const float u0 = fu ? 1.f : 0.f, u1 = fu ? 0.f : 1.f, v0 = fv ? 1.f : 0.f, v1 = fv ? 0.f : 1.f;
            vcolor(colorAt(ram, b, ag));
            rlTexCoord2f(u0, v1); rlVertex3f(cpos[0] - ax[0] - ay[0], cpos[1] - ax[1] - ay[1], cpos[2] - ax[2] - ay[2]);
            rlTexCoord2f(u1, v1); rlVertex3f(cpos[0] + ax[0] - ay[0], cpos[1] + ax[1] - ay[1], cpos[2] + ax[2] - ay[2]);
            rlTexCoord2f(u1, v0); rlVertex3f(cpos[0] + ax[0] + ay[0], cpos[1] + ax[1] + ay[1], cpos[2] + ax[2] + ay[2]);
            rlTexCoord2f(u0, v0); rlVertex3f(cpos[0] - ax[0] + ay[0], cpos[1] - ax[1] + ay[1], cpos[2] - ax[2] + ay[2]);
        }
        if (open) rlEnd();
        rlSetTexture(0);
    }
};

}  // namespace wotm
