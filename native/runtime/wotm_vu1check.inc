// wotm_vu1check.inc: compara, por objeto, o que o microcodigo do VU1 manda ao GS com o que a camada nativa (native/wotm_native.hpp)
// desenha, em numeros e sem imagem. Incluido por wotm_scene.inc (escopo global de ps2_runtime.cpp).
//
// WOTM_VU1CHECK=<segundos>, no modo nativo de sempre (PS2X_NO_VU1/PS2X_NO_VIF1). O VIF1 processa toda cadeia que chega (g_wotmVif1Keep:
// microcodigo e dados do VU1 em dia; sem isso so ~78% casava) sem executar o VU1 e sem alimentar o GS (quem desenha e a camada
// nativa). A cada <segundos>, num quadro da luta (g_frame >= 30), da entrada de hier(0, 0) a do hier(0, 0) seguinte:
//  - g_wotmVu1Tap ligado: o hierFlushObjQ dispara o DMA de verdade (main.cpp), o VU1 executa e cada XGKICK chega aqui, sem ir ao
//    GS, com o id do MSCAL que o produziu;
//    o id guarda as origens na RAM dos trechos da cadeia desde o MSCAL anterior (pacote do quadro, polyPkt, paleta...);
//  - no inicio de cada cadeia do VIF1 a camada nativa coleta a cena com a RAM daquele instante: a cadeia referencia matrizes e
//    paletas no lugar (REF), e so o 1o pacote sai dentro do hier; o ultimo (com os monstros) sai pela interrupcao do DMA
//    (hierDmaHandler -> hierFlushObjQ), que o runtime entrega no meio da logica do quadro. A 1a coleta pega a cena inteira; as
//    outras, so os objetos que a cadeia referencia (a descida na hierarquia e refeita, porque as matrizes mudam);
//  - na entrada do hier(0, 0) seguinte compara. Cada triangulo do VU1 vai para o objeto nativo cujo polyPkt contem alguma das
//    origens do seu MSCAL e casa com o triangulo nativo mais proximo desse objeto na coleta da mesma cadeia (vertices a ate 1 px).
//    A camera vai copiada no pacote e uma cadeia do passe 0 pode sair ja dentro do passe 1: o nativo e projetado com a camera da
//    entrada de cada passe (worldToScreenMat da vista 0 e a posicao do _cs) e vale a que casar mais.
// So entram triangulos inteiros dentro da tela; degenerados (emendas de tira) ficam de fora dos dois lados. O microcodigo descarta
// faces de costas; as do nativo que sobram nesses objetos saem como "costas", nao como diferenca.
// Textura: cada par casado compara o TEX0 em vigor no GS quando o VU1 desenhou (contexto do PRIM) com o do slot da secao que o nativo
// usa (texKey/objectTex0), nos campos que escolhem os texels.
// Saida: linhas [vu1check] no stderr (totais e os objetos com mais diferenca). WOTM_VU1CHECK_DUMP=<prefixo> grava os triangulos de
// cada checagem em <prefixo>_<g_frame>.csv.
// Inventario (linhas "[vu1check] inv", funcao inventory): o que cada lado desenha sem par no outro, por dono. Do lado da referencia
// entram tambem os primitivos do PATH2 (DIRECT do VIF1) e do PATH3 (canal GIF), que e por onde saem HUD, menus e fade; do lado
// nativo, os objetos (inclusive ceu e icones) cujo polyPkt nenhum triangulo do VU1 tocou. E a lista do que falta ou sobra no port.
// Funciona em qualquer tela (qualquer chamador de hier(0, 0)); gatilhos mem:<endereco>[=<valor>] e a tecla F8 pegam os menus.
// WOTM_VU1CHECK_SNAP=<prefixo> grava um instantaneo de cada quadro amostrado (<prefixo>_<g_frame>.vu1snap) que reproduz a checagem
// sem jogo: `ps2EntryRunner --vu1cmp <arquivo> [--dump <prefixo>]` (namespace snap e wotmVu1CmpMain, no fim).
#ifdef WOTM_HAVE_NATIVE
#include <array>
#include "wotm_syms.h"   // wotm::sym / wotm::fn: enderecos do ELF por nome (tools/native_syms.py --gen)
#include <map>
#include <set>
#include <tuple>

namespace wotm_vu1check
{
// Textura de um triangulo: os campos do TEX0 que escolhem os texels (TBP0, TBW, PSM, TW, TH, CBP, CPSM, CSM, CSA; fora TCC/TFX/CLD).
// 0 = sem textura (PRIM.TME 0 no VU1; objeto sem texKey no nativo); kTexUnknown = o VU1 desenhou sem TEX0 visto no quadro.
constexpr uint64_t kTexMask = 0x3FFFFFFFFull | (0x1FFFFFFull << 37);
constexpr uint64_t kTexUnknown = ~0ull;

// Atributos por vertice e do triangulo, nas unidades do GS (os do nativo convertidos para o que ele desenha):
//  - u, v: coordenada de textura normalizada (1,0 = largura 2^TW): no VU1, S/Q e T/Q (ou UV/16/2^TW com PRIM.FST); no nativo, Vtx::u/v;
//  - c: RGBA cru (0x80 = 1,0): no VU1, o RGBAQ do vertice; no nativo, o RGB do Scene::vertexRgb (com a luz do VU1 nos iluminados) e Vtx::a * DrawItem::fade (o desenho ainda converte:
//    2r e 4a*fade sobre 255; a escala do alfa depende do alfa da textura e fica fora daqui);
//  - abe: PRIM.ABE (VU1) / MeshData::abe (nativo); alpha: byte baixo do ALPHA_1/2 (A, B, C, D) do contexto do PRIM (kAlphaUnknown = nao
//    visto); no nativo, o ALPHA equivalente ao DrawItem::blend (com abe) ou ao RL_BLEND_ALPHA (0x44);
//  - fge: PRIM.FGE (neblina; o nativo nao desenha neblina).
constexpr uint16_t kAlphaUnknown = 0x100;
struct Tri {
    uint32_t prog; float x[3], y[3]; uint64_t tex = 0; uint8_t slot = 0; uint16_t fbp = 0xFFFF;   // slot: so no nativo; fbp: FRAME do VU1 (0xFFFF = nao visto)
    float u[3] = {}, v[3] = {};
    uint8_t c[3][4] = {};
    uint64_t tex0 = 0;   // TEX0 inteiro (so no VU1: TCC/TFX)
    uint16_t alpha = kAlphaUnknown;
    uint32_t test = 0xFFFFFFFFu;   // TEST do contexto (so no VU1; 0xFFFFFFFF = nao visto)
    bool abe = false, fge = false;
    uint8_t f[3] = {255, 255, 255};   // F da neblina por vertice (VU1: campo F do XYZF2; nativo: Scene::fogF com a camera da projecao)
};

// Triangulo nativo em coordenadas de mundo; env/envK/nw: mapa de ambiente (o UV sai no project, Scene::envUv com a camera da projecao)
struct WTri { float p[3][3]; uint64_t tex; uint8_t slot; bool cull; float u[3], v[3]; uint8_t c[3][4]; bool abe; uint16_t alpha; bool fge; float burn;
              uint8_t env = 0; float envK = 0.f; float nw[3][3] = {}; };

struct NatObj {
    uint32_t node = 0, span = 0;
    uint16_t tex = 0;
    bool skinned = false, skip = false;   // skip: ceu/icones (o nativo os desenha em outro lugar de proposito)
    uint16_t hudId = 0;                    // elemento de HUD (Scene::hud, desenhado por cima da cena): id do objeto; 0 = nao e HUD
    int items = 0;
    uint32_t badVerts = 0;                 // vertices nao finitos (invariante)
    std::vector<WTri> tris;
    std::vector<std::array<float, 4>> sprites;   // ObjSprite: centro no mundo e lado (compareObjSprites)
};
using NatMap = std::map<uint32_t, NatObj>;   // por polyPkt
struct Cam { float W[4][4]; float eye[3]; };

// Particulas: o pacote de cada sistema fica dentro do proprio fx[i] (pkt em +0), entao a origem do trecho da cadeia diz o sistema.
// O VU1 manda cada particula como dois triangulos (quadrado) ou como um SPRITE do GS (retangulo); o nativo monta os mesmos quadrados
// (Particles::forEachQuad, e Scene::spriteCorners nos destroços) e eles sao projetados na tela (collectFx).
constexpr uint32_t kFx0 = wotm::fxaddr::fx, kFxEnd = wotm::fxaddr::fx + wotm::fxaddr::count * wotm::fxaddr::stride;
struct Quad { float x[4], y[4]; uint8_t n; bool fge = false; int idx = -1; };   // n = 4 (quadrado) ou 3 (triangulo sem par, so no VU1); fge: PRIM.FGE (VU1); idx: particula (nativo, so para o dump)

// Primitivo da referencia para o inventario (linhas "[vu1check] inv"): o que a referencia desenha fora do caminho 1 (PATH2: DIRECT do
// VIF1; PATH3: canal GIF), e os SPRITE do VU1, com a origem na RAM quando se conhece (PATH2: trecho da cadeia DMA; PATH3: so quando o
// DMA leu a RAM no lugar). Inteiros ou nao na tela (o HUD fica nas bordas).
struct RefPrim {
    uint8_t path, kind;   // path 1/2/3; kind = PRIM.PRIM (3/4/5 triangulo, 6 sprite)
    uint32_t src;         // origem na RAM do pacote (0 = desconhecida)
    float x0, y0, x1, y1; // caixa na tela (coordenadas do GS)
    uint64_t tex;         // TEX0 do contexto mascarado (0 = sem textura, kTexUnknown = nao visto)
    uint16_t fbp, alpha;
    bool abe;
    uint8_t rgba[4];
};
constexpr int kFxCams = 3;   // camera do instante da cadeia, da entrada do passe 0 e do passe 1
struct FxNat {
    uint32_t type = 0, preset = 0;
    int n = 0;
    bool built = false;               // info.attribs & 0x400 (pacote montado neste quadro)
    std::vector<Quad> quads[kFxCams];   // por camera
    Cam cams[kFxCams] = {};
    std::vector<std::array<float, 10>> raw;   // so para o dump: pos (4), vel (3), idade, minSize, maxSize de cada particula
    std::vector<uint32_t> rawColor;           // so para o dump: a cor nativa (Particles::colorAt) de cada particula
    float prevMat[16] = {}, curMat[16] = {};   // so para o dump: prevViewMat (+0x2EC0, no pacote) e worldToScreenMat crus
};
using FxMap = std::map<uint32_t, FxNat>;   // por indice em fx[]

int g_period = 0;   // segundos entre checagens
std::chrono::steady_clock::time_point g_next;
bool g_armed = false;
const uint8_t *g_rdram = nullptr;
uint32_t g_frame = 0, g_pass = 0;
Cam g_cam[2];                                   // camera na entrada de hier(0, 0) e hier(0, 1)
std::vector<NatMap> g_nats;                     // coleta nativa no inicio de cada cadeia do VIF1
NatMap g_natPass1;                               // coleta nativa na entrada do hier(0, 1) (matrizes do fim da travessia do passe 0)
std::vector<FxMap> g_fxNats;                    // particulas nativas por cadeia (so a 1a e as que referenciam fx[])
std::vector<char> g_fxChain;                    // cadeia com particulas coletadas
std::vector<std::vector<uint32_t>> g_progs;     // id - 1 -> origens dos trechos da cadeia ate o MSCAL
std::vector<uint32_t> g_progChain;              // id - 1 -> cadeia (indice em g_nats)
std::vector<Tri> g_vu;                          // triangulos do VU1 (Tri::prog = id do programa)
struct PxTri { uint32_t prog; float x[3], y[3]; uint16_t fbp; };
std::vector<PxTri> g_vuAll;                     // todos os triangulos do XGKICK, inteiros ou nao na tela (atribuicao por pixel no replay)
uint32_t g_vuKicked = 0, g_vuOff = 0;             // triangulos do XGKICK no quadro / fora da tela (parcial ou inteiro)
uint32_t g_vuPrimVerts[8] = {};                 // vertices em primitivos que a checagem nao compara (ponto, linha, leque), por PRIM.PRIM
std::vector<std::pair<uint32_t, Quad>> g_vuRects;   // SPRITE do GS inteiros na tela (programa, retangulo)
std::vector<RefPrim> g_vuRectInfo;   // paralelo a g_vuRects: textura, FRAME, ABE e cor de cada SPRITE (inventario)
uint32_t g_vuRectsKicked = 0, g_vuRectsRtt = 0;   // SPRITE do XGKICK / desses, em textura (FRAME fora da tela)
// SPRITE do lens flare (programa com origem no fx[] de lensFlarePacket[0]), inteiros ou nao: cantos na ordem do XGKICK, UV (FST) e cor
struct FlareRect { float x[2], y[2], u[2], v[2]; uint8_t c[4]; uint64_t tex0; };
std::vector<FlareRect> g_vuFlare;
wotm::Scene *g_scene = nullptr;
FILE *g_dump = nullptr;

std::vector<RefPrim> g_ref23;   // primitivos do PATH2/PATH3 do quadro (inventario)
uint8_t g_curPath = 2;   // caminho do pacote em gifTap(0, ...): 2 (PATH2, origem em g_wotmPath2Src) ou 3 (PATH3, origem em g_curSrc)
uint32_t g_curSrc = 0;

constexpr float kX0 = 1728.f, kX1 = 2368.f, kY0 = 1936.f, kY1 = 2160.f;   // tela do GS (640 x 224, centro 2048)
inline bool inScreen(const Tri &t) {
    for (int c = 0; c < 3; ++c)
        if (!(t.x[c] >= kX0 && t.x[c] < kX1 && t.y[c] >= kY0 && t.y[c] < kY1)) return false;
    return true;
}
inline float area2(const Tri &t) { return (t.x[1] - t.x[0]) * (t.y[2] - t.y[0]) - (t.x[2] - t.x[0]) * (t.y[1] - t.y[0]); }
inline bool degenerate(const Tri &t) { return std::fabs(area2(t)) < 0.02f; }

Cam readCam(const uint8_t *rdram)
{
    const wotm::Ram ram{rdram, PS2_RAM_SIZE};
    Cam c{};
    for (int r = 0; r < 4; ++r)
        for (int k = 0; k < 4; ++k) c.W[r][k] = ram.f32(wotm::addr::worldToScreenMat + 16 * r + 4 * k);
    const wotm::GameCamera gc = wotm::readGameCamera(ram, 0);
    for (int i = 0; i < 3; ++i) c.eye[i] = gc.pos[i];
    return c;
}

// Projecao como o VU1: d = p - olho, X = l0.d, Y = l1.d, W = l3.d (worldToScreenMat: linhas 0 e 1 = foco * eixo + 2048 * frente,
// linha 3 = frente). Falso atras da camera.
inline bool projectPt(const Cam &cam, const float p[3], float &x, float &y)
{
    const float d[3] = {p[0] - cam.eye[0], p[1] - cam.eye[1], p[2] - cam.eye[2]};
    const float q = cam.W[3][0] * d[0] + cam.W[3][1] * d[1] + cam.W[3][2] * d[2] + cam.W[3][3];
    x = (cam.W[0][0] * d[0] + cam.W[0][1] * d[1] + cam.W[0][2] * d[2] + cam.W[0][3]) / q;
    y = (cam.W[1][0] * d[0] + cam.W[1][1] * d[1] + cam.W[1][2] * d[2] + cam.W[1][3]) / q;
    return q > 1e-3f;
}

// So os triangulos inteiros na tela, na frente da camera e nao degenerados.
std::vector<Tri> project(const std::vector<WTri> &in, const Cam &cam, uint32_t key, std::vector<Tri> *culledOut = nullptr)
{
    std::vector<Tri> out;
    wotm::Scene::EnvView ev;   // so com algum triangulo de mapa de ambiente
    for (const WTri &w : in) {
        const bool culled = w.cull && wotm::Scene::backFacing(w.p, cam.eye);   // o nativo nao desenha (secao com culling)
        if (culled && !culledOut) continue;
        Tri t{key, {}, {}, w.tex, w.slot};
        std::memcpy(t.u, w.u, sizeof t.u);
        std::memcpy(t.v, w.v, sizeof t.v);
        std::memcpy(t.c, w.c, sizeof t.c);
        t.abe = w.abe;
        t.alpha = w.alpha;
        t.fge = w.fge;
        if (w.env) {
            if (!ev.ok) ev = wotm::Scene::envView(wotm::Ram{g_rdram, PS2_RAM_SIZE}, cam.W, cam.eye);
            for (int c = 0; c < 3; ++c) wotm::Scene::envUv(w.env, w.envK, ev, w.p[c], w.nw[c], t.u[c], t.v[c]);
        }
        if (w.fge)
            for (int c = 0; c < 3; ++c) t.f[c] = uint8_t(g_scene->fogF(w.p[c], w.burn, cam.W[3], cam.eye));
        bool ok = true;
        for (int c = 0; c < 3 && ok; ++c) ok = projectPt(cam, w.p[c], t.x[c], t.y[c]);
        if (ok && inScreen(t) && !degenerate(t)) (culled ? *culledOut : out).push_back(t);
    }
    return out;
}

// Eixos da tela tirados da matriz: linha 3 = frente; linhas 0 e 1 menos 2048 * frente = foco * direita / baixo.
wotm::FxView fxView(const Cam &cam)
{
    wotm::FxView v{};
    float f[3], fl = 0.f, rl = 0.f, ul = 0.f;
    for (int a = 0; a < 3; ++a) {
        f[a] = cam.W[3][a];
        v.right[a] = cam.W[0][a] - 2048.f * f[a];
        v.up[a] = cam.W[1][a] - 2048.f * f[a];
        fl += f[a] * f[a]; rl += v.right[a] * v.right[a]; ul += v.up[a] * v.up[a];
    }
    fl = std::sqrt(fl); rl = std::sqrt(rl); ul = std::sqrt(ul);
    v.depth[3] = cam.W[3][3] / fl;
    for (int a = 0; a < 3; ++a) {
        v.right[a] /= rl; v.up[a] /= ul;
        v.back[a] = -f[a] / fl;
        v.depth[a] = f[a] / fl;
        v.depth[3] -= v.depth[a] * cam.eye[a];
    }
    v.ox[3] = v.oy[3] = 0.f;   // x e y de vista (faixas: perpendicular na tela)
    for (int a = 0; a < 3; ++a) {
        v.ox[a] = v.right[a]; v.oy[a] = v.up[a];
        v.ox[3] -= v.right[a] * cam.eye[a]; v.oy[3] -= v.up[a] * cam.eye[a];
    }
    return v;
}

// Particulas nativas da RAM de agora (depois de Scene::collect), quadrados inteiros na tela, por sistema. Como nos objetos, a camera
// que o pacote leva nao e conhecida: os quadrados saem com a camera de agora e com as das entradas dos passes 0 e 1 (se ja houve), e
// na comparacao vale a que casar mais.
void collectFx(const uint8_t *rdram, FxMap &out)
{
    const wotm::Ram ram{rdram, PS2_RAM_SIZE};
    const Cam cams[kFxCams] = {readCam(rdram), g_cam[0], g_cam[1]};
    const int nCams = g_pass == 1 ? 3 : 2;
    for (int ci = 0; ci < nCams; ++ci) {
        const Cam &cam = cams[ci];
        wotm::FxView view = fxView(cam);
        wotm::Particles::setFog(ram, view, cam.W[3], cam.eye);   // o nativo descarta as particulas com F < 1, como o VU1
        auto add = [&](FxNat &s, const float c[4][3], int idx = -1) {
            Quad q{{}, {}, 4, false, idx};
            for (int k = 0; k < 4; ++k)
                if (!projectPt(cam, c[k], q.x[k], q.y[k]) || !(q.x[k] >= kX0 && q.x[k] < kX1 && q.y[k] >= kY0 && q.y[k] < kY1)) return;
            s.quads[ci].push_back(q);
        };
        for (const wotm::FxSystem &sys : g_scene->fx.systems) {
            FxNat &s = out[sys.index];
            s.type = sys.type;
            s.n = sys.n;
            s.preset = ram.u32(sys.base + wotm::fxoff::fxId);
            s.built = true;   // Particles::collect so aceita sistema com pacote montado
            s.cams[ci] = cam;
            static const bool wantRaw = std::getenv("WOTM_VU1CHECK_DUMP") != nullptr;
            if (!ci && wantRaw) {
                const uint32_t pp = ram.u32(sys.base + wotm::fxoff::pos), vp = ram.u32(sys.base + wotm::fxoff::vel), ap = ram.u32(sys.base + wotm::fxoff::age);
                const float mn = ram.f32(sys.base + wotm::fxoff::minSize), mx = ram.f32(sys.base + wotm::fxoff::maxSize);
                if (ram.ok(pp, 16 * uint32_t(sys.n)) && ram.ok(vp, 16 * uint32_t(sys.n)) && ram.ok(ap, 2 * uint32_t(sys.n)))
                    for (int k = 0; k < sys.n; ++k) {
                        s.raw.push_back({ram.f32(pp + 16 * k), ram.f32(pp + 16 * k + 4), ram.f32(pp + 16 * k + 8), ram.f32(pp + 16 * k + 12),
                                         ram.f32(vp + 16 * k), ram.f32(vp + 16 * k + 4), ram.f32(vp + 16 * k + 8), float(int16_t(ram.u16(ap + 2 * k))), mn, mx});
                        uint32_t col = wotm::Particles::colorAt(ram, sys.base, int16_t(ram.u16(ap + 2 * k)));
                        if (sys.type == wotm::FX_STRITE) {   // alfa do strite como o desenho (Particles::drawSystem)
                            float p[4], v[3], tri[4][3], ak = 1.f;
                            for (int a = 0; a < 4; ++a) p[a] = ram.f32(pp + 16 * k + 4 * a);
                            for (int a = 0; a < 3; ++a) v[a] = ram.f32(vp + 16 * k + 4 * a);
                            if (wotm::Particles::striteTriangle(ram, sys.base, p, v, tri, &ak)) col = (col & 0x00FFFFFFu) | (uint32_t(float(col >> 24) * ak) << 24);
                        }
                        s.rawColor.push_back(col);
                    }
                const uint32_t pm = ram.u32(sys.base + 0x2EC0);
                for (int k = 0; k < 16; ++k) {
                    if (ram.ok(pm, 64)) s.prevMat[k] = ram.f32(pm + 4 * k);
                    s.curMat[k] = ram.f32(wotm::addr::worldToScreenMat + 4 * k);
                }
            }
            wotm::Particles::forEachQuad(ram, sys, view, [&](int i, const float c[4][3], const float[4][2]) { add(s, c, i); });
        }
        for (const wotm::Sprite &sp : g_scene->sprites) {   // destroços (frag): Scene::collectParticles, key = fx * 4096 + particula
            FxNat &s = out[sp.key >> 12];
            if (!ci) {
                const uint32_t b = kFx0 + (sp.key >> 12) * wotm::fxaddr::stride;
                s.type = wotm::FX_FRAG;
                s.preset = ram.u32(b + wotm::fxoff::fxId);
                s.built = (ram.u32(b + wotm::fxoff::infoAttribs) & 0x400) != 0;
                ++s.n;
            }
            float c[4][3];
            wotm::Scene::spriteCorners(sp, view.right, view.up, c);
            add(s, c);
        }
    }
}

// Sistema de particulas (indice em fx[]) do programa: a ultima origem dentro de fx[]; -1 = nenhum.
int fxOf(uint32_t prog)
{
    int sys = -1;
    for (uint32_t a : g_progs[prog - 1])
        if (a >= kFx0 && a < kFxEnd) sys = int((a - kFx0) / wotm::fxaddr::stride);
    return sys;
}

double g_walkMs = 0.0, g_projMs = 0.0;   // tempo das coletas nativas no quadro amostrado (descida na hierarquia / triangulos)

// Triangulos nativos (RAM de agora, coordenadas de mundo, com skinning), por polyPkt. Com `only` (origens ordenadas dos trechos de
// uma cadeia), so os objetos cujo polyPkt contem alguma delas.
void collectNative(const uint8_t *rdram, NatMap &nat, const std::vector<uint32_t> *only = nullptr)
{
    if (!g_scene) g_scene = new wotm::Scene();
    const wotm::Ram ram{rdram, PS2_RAM_SIZE};
    auto t0 = std::chrono::steady_clock::now();
    g_scene->collect(ram);
    auto t1 = std::chrono::steady_clock::now();
    g_walkMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    auto add = [&](const std::vector<wotm::DrawItem> &list, bool skip) {
        for (const wotm::DrawItem &it : list) {
            const uint32_t pp = ram.u32(it.node + 4) & 0x0FFFFFFF;
            const wotm::MeshData *md = g_scene->mesh(ram, it.node);
            if (!pp || !md || !md->valid) continue;
            const uint32_t span = md->skinned ? 0x20000u : (md->qwc + 1) * 16u;
            if (only) {
                const auto s = std::lower_bound(only->begin(), only->end(), pp);
                if (s == only->end() || *s - pp >= span) continue;
            }
            NatObj &o = nat[pp];
            if (!o.items) {
                o.node = it.node;
                o.tex = ram.u16(it.node + 0x50);
                o.skinned = md->skinned;
                o.span = span;
                o.hudId = it.hudId;
            }
            o.skip = o.skip || skip;
            ++o.items;
            // os objetos skip (ceu, icones) tambem ganham triangulos: a comparacao por objeto os ignora, o inventario os conta
            uint64_t tex[32] = {};   // por slot: a textura com que o nativo desenha (Scene::textureId -> texKey/objectTex0)
            for (uint32_t s = 0; s < 32; ++s)
                if (md->slotMask & (1u << s)) {
                    const uint64_t k = wotm::Scene::texKey(ram, it, s);
                    // texId 0 = buffer em exibicao: o TEX0 que o texmGetTextureAddrs monta com displayBuffer (0x6F87F8)
                    tex[s] = k == wotm::Scene::kScreenKey ? ((uint64_t(ram.u32(wotm::sym::displayBuffer) | 0x28128000u) | (1ull << 33)) & kTexMask)
                           : k ? wotm::Scene::objectTex0(ram, it.node, s) & kTexMask : 0;
                }
            const uint16_t alpha = md->abe ? it.blend : wotm::kGsAlphaNormal;
            g_scene->forEachTri(ram, it, [&](const float p[3][3], const wotm::Vtx *v, const uint8_t (*rgb)[3], const float (*nw)[3]) {
                WTri w;
                w.slot = v[0].slot;
                w.cull = v[0].cull != 0;
                w.tex = tex[w.slot & 31];
                w.abe = md->abe;
                w.alpha = alpha;
                w.fge = v[0].fge != 0;
                w.burn = ram.f32(it.node + 0x2C);   // fogBurnThru
                w.env = it.env;
                w.envK = it.envK;
                std::memcpy(w.nw, nw, sizeof w.nw);
                bool ok = true;
                for (int c = 0; c < 3; ++c) {
                    for (int a = 0; a < 3; ++a) {
                        w.p[c][a] = p[c][a];
                        ok = ok && std::isfinite(p[c][a]);
                    }
                    w.u[c] = v[c].u + it.uvOff[0];
                    w.v[c] = v[c].v + it.uvOff[1];
                    std::memcpy(w.c[c], rgb[c], 3);   // a cor com que o nativo desenha (Scene::vertexRgb)
                    w.c[c][3] = uint8_t(std::min(255.f, v[c].a * it.fade * std::max(0.f, it.color[3])));
                }
                if (ok) o.tris.push_back(w);
                else ++o.badVerts;
            });
            for (const wotm::ObjSprite &s : md->sprites) {
                float p[3];
                wotm::xform(it.m, s.x, s.y, s.z, p);
                o.sprites.push_back({p[0], p[1], p[2], s.size});
            }
        }
    };
    add(g_scene->items, false);
    add(g_scene->sky, true);
    add(g_scene->overlay, true);
    add(g_scene->hp, false);
    add(g_scene->hpOverlay, true);
    add(g_scene->hud, false);   // HUD: o nativo desenha na frente da camera como os demais objetos; o inventario ainda o lista por id
    g_projMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
}

uint32_t g_chains = 0;   // cadeias do VIF1 no quadro amostrado

namespace snap { void chain(const uint8_t *ram, const std::pair<uint32_t, uint32_t> *segs, size_t n, const uint8_t *data, uint32_t bytes); }

// Inicio de cada cadeia do VIF1: coleta com a RAM que ela vai ler (a 1a pega tudo; as outras, so os objetos que referenciam).
void chainTap(const std::pair<uint32_t, uint32_t> *segs, size_t n, const uint8_t *data, uint32_t bytes)
{
    if (!g_armed || !g_rdram) return;
    snap::chain(g_rdram, segs, n, data, bytes);   // instantaneo (WOTM_VU1CHECK_SNAP): a RAM deste instante e os bytes da cadeia
    ++g_chains;
    std::vector<uint32_t> only;
    for (size_t i = 0; i < n; ++i) only.push_back(segs[i].second);
    std::sort(only.begin(), only.end());
    g_nats.emplace_back();
    collectNative(g_rdram, g_nats.back(), g_nats.size() == 1 ? nullptr : &only);
    if (g_nats.size() > 1) {
        // A OT de alfa e montada na descida do hier(v,0) e sai numa cadeia do hier(v,1), depois do TheGame::Update, que pode ter
        // desligado o _cs nesse meio (retícula que some: drawMe 0). O objeto que a cadeia referencia e esta coleta nao achou vem
        // da 1a coleta, a do momento em que o pacote foi montado.
        NatMap &cur = g_nats.back();
        const NatMap &first = g_nats.front();
        for (uint32_t a : only) {
            auto it = first.upper_bound(a);
            if (it == first.begin()) continue;
            --it;
            if (a - it->first < it->second.span && !cur.count(it->first)) cur.emplace(it->first, it->second);
        }
    }
    const bool fx = g_nats.size() == 1 || std::any_of(only.begin(), only.end(), [](uint32_t a) { return a >= kFx0 && a < kFxEnd; });
    g_fxNats.emplace_back();
    g_fxChain.push_back(fx);
    if (fx) collectFx(g_rdram, g_fxNats.back());
}

// Cabecalho do VU1 no MSCAL (so com o dump): endereco do programa e os qwords 5-8 (vf31 e as direcoes das luzes) e 17-20 (cores das
// luzes) da memoria de dados, que o programa de luz (0x1010) le com LQ n(vi0).
struct ProgHdr { uint32_t addr; float q[8][4]; float pt[7][4]; };
std::vector<ProgHdr> g_progHdr;

// Entrada do microprograma do lens flare (0x2B20) lida da memoria de dados do VU1 no MSCAL, como ele a le: q44 = (centro, 18, n),
// tabelas em q8-q43, lista em q45 + 3i, UV maximo no w de q0x110 + seletor. Uma por quadro amostrado (a ultima).
std::vector<wotm::Particles::FlareFrame> g_flareVuIn;
void flareFromVu(const uint8_t *vu)
{
    auto f32 = [&](uint32_t q, int c) { float v; std::memcpy(&v, vu + 16 * q + 4 * c, 4); return v; };
    auto u32 = [&](uint32_t q, int c) { uint32_t v; std::memcpy(&v, vu + 16 * q + 4 * c, 4); return v; };
    wotm::Particles::FlareFrame f;
    f.cx = f32(44, 0);
    f.cy = f32(44, 1);
    f.nspr = std::min<int>(18, int(u32(44, 2) & 0xFFFF));
    const int n = std::min<int>(74, int(u32(44, 3) & 0xFFFF));
    for (int k = 0; k < f.nspr; ++k) {
        auto &s = f.spr[k];
        s.tex0 = uint64_t(u32(8 + k, 0)) | (uint64_t(u32(8 + k, 1)) << 32);
        s.t = f32(8 + k, 2);
        s.half = f32(8 + k, 3);
        s.uvMax = float(u32(0x110 + (u32(8 + k, 3) & 3), 3) & 0xFFFF) / 16.f;
        for (int j = 0; j < 4; ++j) s.rgba[j] = f32(26 + k, j);
    }
    for (int i = 0; i < n; ++i) {
        const uint32_t q = 45 + 3 * uint32_t(i);
        wotm::Particles::Flare fl;
        fl.x = f32(q, 0); fl.y = f32(q, 1); fl.z = f32(q, 2); fl.id = u32(q, 3);
        fl.flags = u32(q + 1, 0); fl.zfudge = f32(q + 1, 0); fl.w = f32(q + 1, 1); fl.dot = f32(q + 1, 2); fl.bw = f32(q + 1, 3);
        for (int j = 0; j < 3; ++j) fl.rgb[j] = f32(q + 2, j);
        fl.intensity = f32(q + 2, 3);
        f.e.push_back(fl);
    }
    g_flareVuIn.push_back(std::move(f));
}

uint32_t mscalTap(const uint32_t *srcs, uint32_t n, uint32_t prog, const uint8_t *vu1Data)
{
    if (prog == 0x2B20u && vu1Data) flareFromVu(vu1Data);
    g_progs.emplace_back(srcs, srcs + n);
    g_progChain.push_back(g_nats.empty() ? 0u : uint32_t(g_nats.size() - 1));
    static const bool dump = std::getenv("WOTM_VU1CHECK_DUMP") != nullptr;
    if (dump) {
        ProgHdr h{prog, {}};
        static const int qw[8] = {5, 6, 7, 8, 17, 18, 19, 20};
        if (vu1Data && prog == 0x15D8u) for (int k = 0; k < 7; ++k) std::memcpy(h.pt[k], vu1Data + 16 * (169 + k), 16);   // luzes pontuais (q169-q175)
        if (vu1Data)
            for (int k = 0; k < 8; ++k) std::memcpy(h.q[k], vu1Data + 16 * qw[k], 16);
        g_progHdr.push_back(h);
    }
    return uint32_t(g_progs.size());
}

// Pacote do XGKICK: tags GIF; PACKED com XYZ2/XYZF2 (desenham, salvo ADC) e XYZ3/XYZF3 (so empilham); PRIM no tag ou por A+D.
// Triangulo impar da tira com os dois primeiros trocados, como o nativo (orientacao igual nos dois lados).
// TEX0_1/TEX0_2 (PACKED 0x6/0x7 ou A+D) valem ate a proxima troca, entre pacotes: cada triangulo leva o do contexto do PRIM.
uint64_t g_tex0[2] = {kTexUnknown, kTexUnknown};
uint16_t g_fbp[2] = {0xFFFF, 0xFFFF};   // FRAME_1/FRAME_2.FBP em vigor (A+D 0x4C/0x4D)
uint16_t g_alpha[2] = {kAlphaUnknown, kAlphaUnknown};   // ALPHA_1/ALPHA_2 em vigor (A+D 0x42/0x43), byte baixo
uint32_t g_test[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};   // TEST_1/TEST_2 em vigor (A+D 0x47/0x48); 0xFFFFFFFF = nao visto
// RGBAQ, ST e UV em vigor no GS (valem entre pacotes, como o TEX0)
uint8_t g_rgba[4] = {0x80, 0x80, 0x80, 0x80};
float g_s = 0.f, g_t = 0.f, g_q = 1.f;
uint32_t g_uv = 0;   // U nos bits 0-13, V nos 16-29 (10.4)
uint8_t g_fogF = 255;   // F da neblina em vigor (XYZF2/XYZF3 ou FOG por A+D 0x0A)
uint32_t g_gifZ = 0;    // Z do ultimo XYZ2/XYZF2 (24 bits no XYZF2), para o WOTM_VU1CHECK_Z
void gifTap(uint32_t prog, const uint8_t *p, uint32_t bytes)
{
    uint32_t prim = 4, nv = 0;
    float vx[3] = {}, vy[3] = {};
    uint32_t vz[3] = {};
    struct VAttr { float s, t, q; uint32_t uv; uint8_t c[4]; uint8_t f; };
    VAttr va[3] = {};
    auto vertex = [&](float x, float y, bool kick, uint8_t f) {
        vx[0] = vx[1]; vy[0] = vy[1];
        vx[1] = vx[2]; vy[1] = vy[2];
        vx[2] = x; vy[2] = y;
        vz[0] = vz[1]; vz[1] = vz[2]; vz[2] = g_gifZ;
        va[0] = va[1];
        va[1] = va[2];
        va[2] = {g_s, g_t, g_q, g_uv, {g_rgba[0], g_rgba[1], g_rgba[2], g_rgba[3]}, f};
        ++nv;
        const uint32_t type = prim & 7;
        if (!prog) {   // PATH2/PATH3: os registradores ja ficaram em g_tex0/g_fbp/...; os primitivos vao para o inventario (g_ref23)
            if (!kick) return;
            const bool tri = nv >= 3 && (type == 4 || type == 5 || (type == 3 && nv % 3 == 0));   // leque: caixa dos 3 ultimos (aproximada)
            const bool spr = type == 6 && nv % 2 == 0;
            if (!tri && !spr) return;
            const uint32_t ctx = (prim >> 9) & 1;
            const uint64_t t0 = g_tex0[ctx];
            RefPrim r{g_curPath, uint8_t(type), g_curPath == 2 ? g_wotmPath2Src : g_curSrc, vx[2], vy[2], vx[2], vy[2],
                      !((prim >> 4) & 1) ? 0 : t0 == kTexUnknown ? kTexUnknown : t0 & kTexMask, g_fbp[ctx], g_alpha[ctx], ((prim >> 6) & 1) != 0, {}};
            for (int k = tri ? 0 : 1; k < 2; ++k) {
                r.x0 = std::min(r.x0, vx[k]); r.x1 = std::max(r.x1, vx[k]);
                r.y0 = std::min(r.y0, vy[k]); r.y1 = std::max(r.y1, vy[k]);
            }
            std::memcpy(r.rgba, va[2].c, 4);
            g_ref23.push_back(r);
            return;
        }
        if (kick && nv >= 3 && (type == 4 || (type == 3 && nv % 3 == 0))) {
            const int a = (type == 4 && ((nv - 1) & 1)) ? 1 : 0;
            const uint32_t ctx = (prim >> 9) & 1;
            const uint64_t t0 = g_tex0[ctx];
            const uint64_t tex = !((prim >> 4) & 1) ? 0 : t0 == kTexUnknown ? kTexUnknown : t0 & kTexMask;
            Tri t{prog, {vx[a], vx[1 - a], vx[2]}, {vy[a], vy[1 - a], vy[2]}, tex};
            t.fbp = g_fbp[ctx];
            t.tex0 = t0;
            t.alpha = g_alpha[ctx];
            t.test = g_test[ctx];
            t.abe = (prim >> 6) & 1;
            t.fge = (prim >> 5) & 1;
            const bool fst = (prim >> 8) & 1;
            const float tw = t0 == kTexUnknown ? 1.f : float(1u << ((t0 >> 26) & 0xF)), th = t0 == kTexUnknown ? 1.f : float(1u << ((t0 >> 30) & 0xF));
            const int order[3] = {a, 1 - a, 2};
            for (int k = 0; k < 3; ++k) {
                const VAttr &s = va[order[k]];
                if (fst) { t.u[k] = (s.uv & 0x3FFF) / 16.f / tw; t.v[k] = ((s.uv >> 16) & 0x3FFF) / 16.f / th; }
                else if (s.q != 0.f) { t.u[k] = s.s / s.q; t.v[k] = s.t / s.q; }
                std::memcpy(t.c[k], s.c, 4);
                t.f[k] = s.f;
            }
            {   // diagnostico: WOTM_VU1CHECK_Z=<prog[,prog..]> imprime o Z do GS dos triangulos desses programas (indice do MSCAL, coluna 4 do dump)
                static const char *zp = std::getenv("WOTM_VU1CHECK_Z");
                if (zp && (*zp == '*' || (std::string(",") + zp + ",").find("," + std::to_string(prog) + ",") != std::string::npos))
                    std::fprintf(stderr, "[z] prog %u (%.1f %.1f %u) (%.1f %.1f %u) (%.1f %.1f %u)\n", prog, vx[0], vy[0], vz[0], vx[1], vy[1], vz[1], vx[2], vy[2], vz[2]);
            }
            ++g_vuKicked;
            g_vuAll.push_back({prog, {t.x[0], t.x[1], t.x[2]}, {t.y[0], t.y[1], t.y[2]}, t.fbp});   // atribuicao por pixel: inteiros ou nao
            if (!inScreen(t)) ++g_vuOff;
            else if (!degenerate(t)) g_vu.push_back(t);
        } else if (kick && type == 6) {   // SPRITE: retangulo entre os dois ultimos vertices (particulas)
            if (nv % 2) return;
            const Quad q{{vx[1], vx[2], vx[2], vx[1]}, {vy[1], vy[1], vy[2], vy[2]}, 4, ((prim >> 5) & 1) != 0};
            uint32_t lf = 0;   // lensFlarePacket[0]
            std::memcpy(&lf, g_rdram + wotm::sym::lensFlarePacket, 4);
            if (lf >= kFx0 && lf < kFxEnd && fxOf(prog) == int((lf - kFx0) / wotm::fxaddr::stride)) {
                const uint64_t t0 = g_tex0[(prim >> 9) & 1];
                const float tw = t0 == kTexUnknown ? 1.f : float(1u << ((t0 >> 26) & 0xF)), th = t0 == kTexUnknown ? 1.f : float(1u << ((t0 >> 30) & 0xF));
                FlareRect r{{vx[1], vx[2]}, {vy[1], vy[2]}, {(va[1].uv & 0x3FFF) / 16.f / tw, (va[2].uv & 0x3FFF) / 16.f / tw},
                            {((va[1].uv >> 16) & 0x3FFF) / 16.f / th, ((va[2].uv >> 16) & 0x3FFF) / 16.f / th}, {0, 0, 0, 0}, t0};
                std::memcpy(r.c, va[2].c, 4);
                g_vuFlare.push_back(r);
            }
            bool in = true;
            for (int k = 0; k < 4; ++k) in = in && q.x[k] >= kX0 && q.x[k] < kX1 && q.y[k] >= kY0 && q.y[k] < kY1;
            ++g_vuRectsKicked;
            // render-to-texture (FRAME fora dos buffers de tela): a composicao da poeira/fumaca (FBP 0x1FB, Scene::dustTexture)
            const uint16_t fbp = g_fbp[(prim >> 9) & 1];
            if (fbp != 0xFFFF && fbp != 0x46 && fbp != 0x8C) { ++g_vuRectsRtt; return; }
            if (in) {
                g_vuRects.push_back({prog, q});
                const uint64_t t0 = g_tex0[(prim >> 9) & 1];
                RefPrim r{1, 6, 0, std::min(vx[1], vx[2]), std::min(vy[1], vy[2]), std::max(vx[1], vx[2]), std::max(vy[1], vy[2]),
                          !((prim >> 4) & 1) ? 0 : t0 == kTexUnknown ? kTexUnknown : t0 & kTexMask, fbp, g_alpha[(prim >> 9) & 1], ((prim >> 6) & 1) != 0, {}};
                std::memcpy(r.rgba, va[2].c, 4);
                g_vuRectInfo.push_back(r);
            }
        } else if (kick && type != 3 && type != 4) ++g_vuPrimVerts[type];
    };
    for (uint32_t pos = 0; pos + 16 <= bytes;) {
        uint64_t lo, hi;
        std::memcpy(&lo, p + pos, 8);
        std::memcpy(&hi, p + pos + 8, 8);
        pos += 16;
        const uint32_t nloop = uint32_t(lo & 0x7FFF), flg = uint32_t((lo >> 58) & 3);
        const uint32_t nreg = uint32_t((lo >> 60) & 0xF) ? uint32_t((lo >> 60) & 0xF) : 16u;
        if ((lo >> 46) & 1) { prim = uint32_t((lo >> 47) & 0x7FF); nv = 0; }
        if (flg == 0) {
            for (uint32_t l = 0; l < nloop; ++l)
                for (uint32_t r = 0; r < nreg && pos + 16 <= bytes; ++r, pos += 16) {
                    const uint32_t reg = uint32_t((hi >> (4 * r)) & 0xF);
                    uint32_t w[4];
                    std::memcpy(w, p + pos, 16);
                    if (reg == 0x4 || reg == 0x5 || reg == 0xC || reg == 0xD) {
                        const bool adc = (w[3] >> 15) & 1;
                        if (reg == 0x4 || reg == 0xC) g_fogF = uint8_t((w[3] >> 4) & 0xFF);   // XYZF2/XYZF3: F nos bits 100-107
                        g_gifZ = (reg == 0x4 || reg == 0xC) ? ((w[2] >> 4) & 0xFFFFFF) : w[2];   // PACKED XYZF2: Z nos bits 68-91
                        vertex((w[0] & 0xFFFF) / 16.f, (w[1] & 0xFFFF) / 16.f, (reg == 0x4 || reg == 0x5) && !adc, g_fogF);
                    } else if (reg == 0x1) { for (int k = 0; k < 4; ++k) g_rgba[k] = uint8_t(w[k]); }
                    else if (reg == 0x2) { std::memcpy(&g_s, &w[0], 4); std::memcpy(&g_t, &w[1], 4); std::memcpy(&g_q, &w[2], 4); }
                    else if (reg == 0x3) g_uv = (w[0] & 0x3FFF) | ((w[1] & 0x3FFF) << 16);
                    else if (reg == 0xE && (w[2] & 0xFF) == 0x01) { for (int k = 0; k < 4; ++k) g_rgba[k] = uint8_t(w[0] >> (8 * k)); std::memcpy(&g_q, &w[1], 4); }
                    else if (reg == 0xE && (w[2] & 0xFF) == 0x02) { std::memcpy(&g_s, &w[0], 4); std::memcpy(&g_t, &w[1], 4); }
                    else if (reg == 0xE && (w[2] & 0xFF) == 0x03) g_uv = w[0] & 0x3FFF3FFF;
                    else if (reg == 0xE && ((w[2] & 0xFF) == 0x42 || (w[2] & 0xFF) == 0x43)) g_alpha[(w[2] & 0xFF) - 0x42] = uint16_t(w[0] & 0xFF);
                    else if (reg == 0xE && ((w[2] & 0xFF) == 0x47 || (w[2] & 0xFF) == 0x48)) g_test[(w[2] & 0xFF) - 0x47] = w[0];
                    else if (reg == 0x0 || (reg == 0xE && (w[2] & 0xFF) == 0x00)) { prim = w[0] & 0x7FF; nv = 0; }
                    else if (reg == 0x6 || reg == 0x7) g_tex0[reg - 0x6] = uint64_t(w[0]) | (uint64_t(w[1]) << 32);
                    else if (reg == 0xE && ((w[2] & 0xFF) == 0x06 || (w[2] & 0xFF) == 0x07))
                        g_tex0[(w[2] & 0xFF) - 0x06] = uint64_t(w[0]) | (uint64_t(w[1]) << 32);
                    else if (reg == 0xE && ((w[2] & 0xFF) == 0x4C || (w[2] & 0xFF) == 0x4D)) g_fbp[(w[2] & 0xFF) - 0x4C] = uint16_t(w[0] & 0x1FF);
                }
        } else if (flg == 1) pos += ((nloop * nreg + 1) / 2) * 16;
        else pos += nloop * 16;
        if ((lo >> 15) & 1) break;   // EOP
    }
}

// Pacote do canal GIF (PATH3) para o inventario: o mesmo parser do PATH2 (registradores e primitivos). A origem na RAM so se conhece
// quando o DMA do canal GIF leu a RAM no lugar (o ponteiro aponta para dentro dela); no replay e nos pacotes copiados fica 0.
void path3Tap(const uint8_t *data, uint32_t bytes)
{
    if (!g_armed) return;
    g_curPath = 3;
    g_curSrc = (g_rdram && data >= g_rdram && data < g_rdram + PS2_RAM_SIZE) ? uint32_t(data - g_rdram) : 0u;
    gifTap(0, data, bytes);
    g_curPath = 2;
    g_curSrc = 0;
}

// Objeto nativo (polyPkt) que contem alguma origem do programa; 0 = nenhum.
uint32_t objectOf(const NatMap &nat, uint32_t prog)
{
    for (uint32_t a : g_progs[prog - 1]) {
        auto it = nat.upper_bound(a);
        if (it == nat.begin()) continue;
        --it;
        if (a - it->first < it->second.span) return it->first;
    }
    return 0;
}

// Para cada triangulo do VU1, o nativo ainda livre de menor soma de distancias entre vertices (em qualquer ordem), com cada vertice
// a ate 1 px (grade de 4 px pelo centroide). vuPair = indice do nativo casado (-1 = nenhum); natHit marca os nativos casados.
// Mesma textura? TBP 0 no nativo: objeto da OT de alfa; o texmGetTextureAddrs do fim do hier preenche o TBP so na copia do pacote de
// alfa, nao no polyPkt de onde sai o TEX0 de referencia (o nativo desenha pela textura do texInfo, sem esse TBP).
inline bool sameTex(uint64_t vuTex, uint64_t natTex)
{
    if (vuTex == kTexUnknown) return false;
    const bool noTbp = natTex && !(natTex & 0x3FFFu);
    return noTbp ? (vuTex & ~0x3FFFull) == natTex : vuTex == natTex;
}

// Tres rodadas: (2) so pares de mesma textura; (1) triangulos do VU1 com alguma textura que o objeto nativo usa; (0) o resto. Sem
// isso, geometria repetida no mesmo lugar (camadas do objeto em slots diferentes, a passada da sombra projetada, cuja textura nenhum
// objeto nativo tem) trocava os pares e acusava textura diferente.
uint32_t match(const std::vector<Tri> &vu, const std::vector<Tri> &nat, std::vector<int> &vuPair, std::vector<char> &natHit)
{
    constexpr float kTol = 1.f;
    auto cell = [](float x, float y) { return (uint64_t(uint32_t(int32_t(std::floor(x / 4.f)))) << 32) | uint32_t(int32_t(std::floor(y / 4.f))); };
    std::unordered_multimap<uint64_t, uint32_t> grid;
    for (uint32_t i = 0; i < nat.size(); ++i)
        grid.emplace(cell((nat[i].x[0] + nat[i].x[1] + nat[i].x[2]) / 3.f, (nat[i].y[0] + nat[i].y[1] + nat[i].y[2]) / 3.f), i);
    vuPair.assign(vu.size(), -1);
    natHit.assign(nat.size(), 0);
    std::vector<uint64_t> natTexs;
    for (const Tri &n : nat) natTexs.push_back(n.tex);
    std::sort(natTexs.begin(), natTexs.end());
    natTexs.erase(std::unique(natTexs.begin(), natTexs.end()), natTexs.end());
    std::vector<char> known(vu.size(), 0);
    for (size_t i = 0; i < vu.size(); ++i)
        for (uint64_t nt : natTexs)
            if (sameTex(vu[i].tex, nt)) { known[i] = 1; break; }
    uint32_t matched = 0;
    for (int texRound = 2; texRound >= 0; --texRound)
    for (size_t i = 0; i < vu.size(); ++i) {
        if (vuPair[i] >= 0 || (texRound == 1 && !known[i])) continue;
        const Tri &t = vu[i];
        const float cx = (t.x[0] + t.x[1] + t.x[2]) / 3.f, cy = (t.y[0] + t.y[1] + t.y[2]) / 3.f;
        int best = -1;
        float bestCost = 1e30f;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                auto range = grid.equal_range(cell(cx + 4.f * dx, cy + 4.f * dy));
                for (auto g = range.first; g != range.second; ++g) {
                    if (natHit[g->second]) continue;
                    const Tri &n = nat[g->second];
                    if (texRound == 2 && !sameTex(t.tex, n.tex)) continue;
                    float cost = 0.f;
                    bool all = true;
                    for (int a = 0; a < 3 && all; ++a) {
                        float d = 1e30f;
                        int bb = 0;
                        for (int b = 0; b < 3; ++b) {
                            const float e = std::max(std::fabs(t.x[a] - n.x[b]), std::fabs(t.y[a] - n.y[b]));
                            if (e < d) { d = e; bb = b; }
                        }
                        all = d <= kTol;
                        // desempate pela cor (faces sobrepostas com cores de vertice diferentes): no maximo ~0,02 px
                        float dc = 0.f;
                        for (int k = 0; k < 3; ++k) dc += std::fabs(float(t.c[a][k]) - float(n.c[bb][k]));
                        cost += d + 1e-5f * dc;
                    }
                    if (all && cost < bestCost) { bestCost = cost; best = int(g->second); }
                }
            }
        if (best >= 0) { natHit[size_t(best)] = 1; vuPair[i] = best; ++matched; }
    }
    return matched;
}

// Particulas do VU1 de um sistema (na ordem do XGKICK) -> quadrados: dois triangulos seguidos do mesmo programa com dois vertices em
// comum viram um quadrado; o que sobra fica como triangulo (n = 3; o par saiu da tela ou e degenerado).
std::vector<Quad> vuQuads(const std::vector<Tri> &tris)
{
    std::vector<Quad> out;
    auto same = [](float ax, float ay, float bx, float by) { return std::fabs(ax - bx) < 0.01f && std::fabs(ay - by) < 0.01f; };
    for (size_t i = 0; i < tris.size(); ++i) {
        const Tri &a = tris[i];
        Quad q{{a.x[0], a.x[1], a.x[2]}, {a.y[0], a.y[1], a.y[2]}, 3, a.fge};
        if (i + 1 < tris.size() && tris[i + 1].prog == a.prog) {
            const Tri &b = tris[i + 1];
            int shared = 0, extra = -1;
            for (int k = 0; k < 3; ++k) {
                bool in = false;
                for (int j = 0; j < 3; ++j) in = in || same(b.x[k], b.y[k], a.x[j], a.y[j]);
                if (in) ++shared;
                else extra = k;
            }
            if (shared == 2) { q.x[3] = b.x[extra]; q.y[3] = b.y[extra]; q.n = 4; ++i; }
        }
        out.push_back(q);
    }
    return out;
}

// Casa cada quadrado do VU1 com o nativo livre mais proximo: todo vertice de um a ate 1 px de algum do outro (nos dois sentidos para
// os quadrados; triangulos soltos so no sentido VU1 -> nativo). Devolve quantos casaram.
uint32_t matchQuads(const std::vector<Quad> &vu, const std::vector<Quad> &nat)
{
    constexpr float kTol = 1.f;
    auto cell = [](float x, float y) { return (uint64_t(uint32_t(int32_t(std::floor(x / 4.f)))) << 32) | uint32_t(int32_t(std::floor(y / 4.f))); };
    auto cover = [](const Quad &a, const Quad &b, float &cost) {   // todo vertice de a perto de algum de b
        for (int i = 0; i < a.n; ++i) {
            float d = 1e30f;
            for (int j = 0; j < b.n; ++j) d = std::min(d, std::max(std::fabs(a.x[i] - b.x[j]), std::fabs(a.y[i] - b.y[j])));
            if (d > kTol) return false;
            cost += d;
        }
        return true;
    };
    std::unordered_multimap<uint64_t, uint32_t> grid;
    for (uint32_t i = 0; i < nat.size(); ++i) grid.emplace(cell(nat[i].x[0], nat[i].y[0]), i);
    std::vector<char> hit(nat.size(), 0);
    uint32_t matched = 0;
    for (const Quad &q : vu) {
        int best = -1;
        float bestCost = 1e30f;
        std::vector<uint32_t> cand;
        for (int k = 0; k < q.n; ++k)   // o canto 0 do nativo esta perto de algum canto do VU1
            for (int dx = -1; dx <= 1; ++dx)
                for (int dy = -1; dy <= 1; ++dy) {
                    auto range = grid.equal_range(cell(q.x[k] + 4.f * dx, q.y[k] + 4.f * dy));
                    for (auto g = range.first; g != range.second; ++g) cand.push_back(g->second);
                }
        for (uint32_t c : cand) {
            if (hit[c]) continue;
            float cost = 0.f;
            if (!cover(q, nat[c], cost) || (q.n == 4 && !cover(nat[c], q, cost))) continue;
            if (cost < bestCost) { bestCost = cost; best = int(c); }
        }
        if (best >= 0) { hit[size_t(best)] = 1; ++matched; }
    }
    return matched;
}

// O que o VU1 desenhou de um sistema numa cadeia: triangulos (viram quadrados aos pares) e SPRITE do GS (ja retangulos).
struct FxVu { std::vector<Tri> tris; std::vector<Quad> rects; };
using FxVuMap = std::map<uint32_t, std::map<uint32_t, FxVu>>;   // sistema -> cadeia

// Linha [vu1check] lens flare: SPRITEs do microprograma 0x2B20 contra Particles::flareSprites sobre a lista que o VU1 recebeu (copia do
// fim do particleDraw amostrado; a intensity ja vem do teste de Z do GS emulado, entao aqui nao ha teste de Z do nativo). Casa por
// cantos (1/4 px, na ordem do XGKICK); nos casados conta cor (> 1), UV (> 0,01 texel) e TEX0 (TBP/TW/TH/PSM) diferentes.
void compareFlare()
{
    std::vector<wotm::Particles::FlareQuad> nat;
    std::vector<std::vector<wotm::Particles::FlareIn>> ins(g_flareVuIn.size());
    size_t entradas = 0;
    std::vector<int> natFrame;   // indice em g_flareVuIn de cada sprite nativo (o TEX0 da tabela)
    for (size_t fi = 0; fi < g_flareVuIn.size(); ++fi) {
        const auto &f = g_flareVuIn[fi];
        for (const auto &fl : f.e) ins[fi].push_back({fl.x, fl.y, fl.intensity, &fl});
        entradas += f.e.size();
        wotm::Particles::flareSprites(f, ins[fi], [&](const wotm::Particles::FlareQuad &q) { nat.push_back(q); natFrame.push_back(int(fi)); });
    }
    if (g_vuFlare.empty() && nat.empty()) return;
    std::vector<char> used(nat.size(), 0);
    uint32_t m = 0, cor = 0, uv = 0, tex = 0;
    for (const FlareRect &r : g_vuFlare) {
        int best = -1;
        for (size_t j = 0; j < nat.size(); ++j) {
            const auto &q = nat[j];
            if (used[j] || std::fabs(q.xa - r.x[0]) > 0.25f || std::fabs(q.ya - r.y[0]) > 0.25f || std::fabs(q.xb - r.x[1]) > 0.25f ||
                std::fabs(q.yb - r.y[1]) > 0.25f) continue;
            if (best < 0 || (q.rgb[0] == r.c[0] && q.rgb[1] == r.c[1] && q.rgb[2] == r.c[2])) best = int(j);
        }
        if (best < 0) {
            if (g_dump) std::fprintf(g_dump, "flare,vu,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%u\n", r.x[0], r.y[0], r.x[1], r.y[1], r.c[0], r.c[1], r.c[2], r.c[3]);
            continue;
        }
        used[size_t(best)] = 1;
        ++m;
        const auto &q = nat[size_t(best)];
        bool dc = false;
        for (int j = 0; j < 3; ++j) dc = dc || std::abs(int(q.rgb[j]) - int(r.c[j])) > 1;
        dc = dc || r.c[3] != 0x80;
        cor += dc;
        uv += std::fabs(q.u0 - r.u[0]) * 64.f > 0.01f || std::fabs(q.v0 - r.v[0]) * 64.f > 0.01f || std::fabs(q.u1 - r.u[1]) * 64.f > 0.01f ||
              std::fabs(q.v1 - r.v[1]) * 64.f > 0.01f;
        const uint64_t mask = 0x3FFFull | (0x3Full << 20) | (0xFFull << 26);   // TBP0, PSM, TW, TH
        tex += (g_flareVuIn[size_t(natFrame[size_t(best)])].spr[q.k].tex0 & mask) != (r.tex0 & mask);
        if (g_dump && dc)
            std::fprintf(g_dump, "flarecor,%d,%u,%u,%u,%u,%u,%u,%u\n", q.k, r.c[0], r.c[1], r.c[2], r.c[3], q.rgb[0], q.rgb[1], q.rgb[2]);
        if (g_dump)
            std::fprintf(g_dump, "flareuv,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%llx\n", q.k, r.u[0], r.v[0], r.u[1], r.v[1], q.u0, q.v0, q.u1, q.v1,
                         (unsigned long long)r.tex0);
    }
    if (g_dump)
        for (size_t j = 0; j < nat.size(); ++j)
            if (!used[j])
                std::fprintf(g_dump, "flare,nat,%d,%.3f,%.3f,%.3f,%.3f,%u,%u,%u\n", nat[j].k, nat[j].xa, nat[j].ya, nat[j].xb, nat[j].yb, nat[j].rgb[0],
                             nat[j].rgb[1], nat[j].rgb[2]);
    std::fprintf(stderr, "[vu1check] g=%u lens flare: programas %zu entradas %zu | vu %zu nativo %zu casados %u | cor difere %u uv difere %u tex0 difere %u\n",
                 g_frame, g_flareVuIn.size(), entradas, g_vuFlare.size(), nat.size(), m, cor, uv, tex);
}

// Linha [vu1check] particulas e os sistemas com mais diferenca. rectsOrphan: SPRITE do GS fora de fx[].
void compareFx(const FxVuMap &fxVu, uint32_t rectsOrphan)
{
    int lastFx = -1;   // cadeia de referencia de quem o VU1 nao desenhou: a ultima com particulas coletadas
    for (size_t c = 0; c < g_fxChain.size(); ++c)
        if (g_fxChain[c]) lastFx = int(c);
    if (lastFx < 0) return;
    std::vector<uint32_t> keys;
    for (const auto &kv : fxVu) keys.push_back(kv.first);
    for (const auto &kv : g_fxNats[size_t(lastFx)]) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    struct Row { uint32_t sys; const FxNat *s; uint32_t vu, nat, m, soltos, fge; };
    std::vector<Row> rows;
    uint32_t sysVu = 0, sysNat = 0, sysBoth = 0, totVu = 0, totNat = 0, totM = 0, totSoltos = 0;
    static const FxNat kNone;
    for (uint32_t sys : keys) {
        uint32_t chain = uint32_t(lastFx);
        std::vector<Quad> vq, whole;
        const auto vit = fxVu.find(sys);
        if (vit != fxVu.end()) {
            size_t most = 0;
            for (const auto &kv : vit->second) {
                const std::vector<Quad> q = vuQuads(kv.second.tris);
                vq.insert(vq.end(), q.begin(), q.end());
                vq.insert(vq.end(), kv.second.rects.begin(), kv.second.rects.end());
                if (q.size() + kv.second.rects.size() > most) { most = q.size() + kv.second.rects.size(); chain = kv.first; }
            }
        }
        const auto nit = g_fxNats[chain].find(sys);
        const FxNat &s = nit != g_fxNats[chain].end() ? nit->second : kNone;
        uint32_t soltos = 0;
        const bool triFx = s.type == wotm::FX_STRITE || s.type == wotm::FX_WAKE;   // um triangulo por particula (o nativo: quad degenerado)
        for (const Quad &q : vq) ((q.n == 4 || triFx) ? (whole.push_back(q), 0u) : ++soltos);
        int cam = 0;   // a camera que casar mais
        uint32_t m = matchQuads(whole, s.quads[0]);
        for (int ci = 1; ci < kFxCams && m < whole.size(); ++ci)
            if (!s.quads[ci].empty())
                if (const uint32_t mc = matchQuads(whole, s.quads[ci]); mc > m) { m = mc; cam = ci; }
        const std::vector<Quad> &nq = s.quads[cam];
        const uint32_t nVu = uint32_t(whole.size()), nNat = uint32_t(nq.size());
        if (!nVu && !nNat) continue;
        sysVu += nVu > 0; sysNat += nNat > 0; sysBoth += nVu && nNat;
        totVu += nVu; totNat += nNat; totM += m; totSoltos += soltos;
        uint32_t fge = 0;
        for (const Quad &q : whole) fge += q.fge;
        rows.push_back({sys, &s, nVu, nNat, m, soltos, fge});
        if (g_dump) {
            for (const Quad &q : vq)
                std::fprintf(g_dump, "fx%u,vuq,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n", sys, q.n, q.x[0], q.y[0], q.x[1], q.y[1], q.x[2], q.y[2],
                             q.n == 4 ? q.x[3] : 0.f, q.n == 4 ? q.y[3] : 0.f);
            if (vit != fxVu.end())   // atributos dos triangulos do VU1 (UV, cor, ALPHA, TEX0), na ordem do XGKICK
                for (const auto &kv : vit->second)
                    for (const Tri &t : kv.second.tris)
                        std::fprintf(g_dump, "fx%u,vut,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%08x,%08x,%08x,%x,%d,%llx,%x,%u\n", sys,
                                     t.x[0], t.y[0], t.x[1], t.y[1], t.x[2], t.y[2], t.u[0], t.v[0], t.u[1], t.v[1], t.u[2], t.v[2],
                                     *(const uint32_t *)t.c[0], *(const uint32_t *)t.c[1], *(const uint32_t *)t.c[2], t.alpha, int(t.abe),
                                     (unsigned long long)t.tex0, t.test, t.prog);
            for (const Quad &q : nq)
                std::fprintf(g_dump, "fx%u,natq,4,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n", sys, q.x[0], q.y[0], q.x[1], q.y[1], q.x[2], q.y[2], q.x[3], q.y[3],
                             q.idx);
            std::fprintf(g_dump, "fxsys,%u,%u,%u,%u,%d,%d\n", sys, chain, s.preset, s.type, s.n, cam);
            // camera da comparacao (W 4x4 por linhas, olho) e as particulas cruas: offline projeta qualquer ponto como o projectPt
            const Cam &c = s.cams[cam];
            std::fprintf(g_dump, "fxcam,%u", sys);
            for (int r = 0; r < 4; ++r)
                for (int k = 0; k < 4; ++k) std::fprintf(g_dump, ",%.6g", c.W[r][k]);
            std::fprintf(g_dump, ",%.6g,%.6g,%.6g\n", c.eye[0], c.eye[1], c.eye[2]);
            std::fprintf(g_dump, "fxmat,%u", sys);   // prevViewMat (16) e worldToScreenMat (16), crus
            for (float f : s.prevMat) std::fprintf(g_dump, ",%.7g", f);
            for (float f : s.curMat) std::fprintf(g_dump, ",%.7g", f);
            std::fprintf(g_dump, "\n");
            for (size_t k = 0; k < s.raw.size(); ++k) {
                std::fprintf(g_dump, "fxp,%u,%zu", sys, k);
                for (float f : s.raw[k]) std::fprintf(g_dump, ",%.6g", f);
                std::fprintf(g_dump, ",%08x\n", k < s.rawColor.size() ? s.rawColor[k] : 0u);
            }
        }
    }
    const double pct = totVu ? 100.0 * totM / totVu : 0.0;
    std::fprintf(stderr, "[vu1check] g=%u particulas: sistemas vu %u nativo %u ambos %u | quadrados vu %u nativo %u casados %.1f%% faltam %u sobram %u | "
                         "triangulos soltos no vu %u | SPRITE do GS %u (%u em textura, %u na tela, %u fora de fx[]) | vertices do VU1 nao comparados: ponto %u linha %u "
                         "tira de linhas %u leque %u\n",
                 g_frame, sysVu, sysNat, sysBoth, totVu, totNat, pct, totVu - totM, totNat - totM, totSoltos, g_vuRectsKicked, g_vuRectsRtt, uint32_t(g_vuRects.size()),
                 rectsOrphan, g_vuPrimVerts[0], g_vuPrimVerts[1], g_vuPrimVerts[2], g_vuPrimVerts[5]);
    // por tipo (vis.attribs; 0 = sistema que o nativo nao tem): casados / quadrados do VU1 / quadrados nativos, e quantos do VU1 com FGE
    std::map<uint32_t, std::array<uint32_t, 4>> byType;
    for (const Row &r : rows) {
        auto &t = byType[r.s->n ? r.s->type : 0u];
        t[0] += r.m; t[1] += r.vu; t[2] += r.nat; t[3] += r.fge;
    }
    if (!byType.empty()) {
        std::string line;
        char part[112];
        for (const auto &kv : byType) {
            std::snprintf(part, sizeof part, " %u %s %u/%u/%u fge %u", kv.first, kv.first ? wotm::fxTypeName(kv.first) : "-", kv.second[0], kv.second[1],
                          kv.second[2], kv.second[3]);
            line += part;
        }
        std::fprintf(stderr, "[vu1check]   particulas por tipo (casados/vu/nativo, fge = do VU1 com PRIM.FGE):%s\n", line.c_str());
    }
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.vu + a.nat - 2 * a.m > b.vu + b.nat - 2 * b.m; });
    for (size_t i = 0; i < rows.size() && i < 8; ++i) {
        const Row &r = rows[i];
        const uint32_t diff = r.vu + r.nat - 2 * r.m;
        if (diff < 2 || 10 * diff < std::max(r.vu, r.nat)) continue;
        std::fprintf(stderr, "[vu1check]   fx %u preset %u tipo %u %s n=%d%s: vu %u nativo %u casados %u (soltos %u)\n", r.sys, r.s->preset, r.s->type,
                     r.s->n ? wotm::fxTypeName(r.s->type) : "-", r.s->n, r.s->n && !r.s->built ? " sem pacote" : "", r.vu, r.nat, r.m, r.soltos);
        if ((!r.s->n || !r.vu) && g_rdram) {   // so de um lado: estado cru agora (tipo, numParticles, info.attribs, emit.info.attribs, ativo?)
            const wotm::Ram ram{g_rdram, PS2_RAM_SIZE};
            const uint32_t b = kFx0 + r.sys * wotm::fxaddr::stride;
            bool listed = false;
            for (int32_t i = int32_t(ram.u32(wotm::fxaddr::fxActiveTop)), guard = 0; i >= 0 && i < 100 && guard < 100; ++guard) {
                if (uint32_t(i) == r.sys) { listed = true; break; }
                i = int32_t(ram.u32(wotm::fxaddr::fxList + 12 * uint32_t(i) + 8));
            }
            std::fprintf(stderr, "[vu1check]     cru: tipo %u n %u attribs %x emit %x preset %u ativo %d\n", ram.u32(b + wotm::fxoff::type) & 0xFF,
                         ram.u32(b + wotm::fxoff::numParticles), ram.u32(b + wotm::fxoff::infoAttribs), ram.u32(b + 0x2D80), ram.u32(b + wotm::fxoff::fxId), int(listed));
        }
    }
}

// Atributos de um par casado (Tri: unidades do GS; ver o comentario do Tri). Conta triangulos:
//  - uv: algum vertice a mais de 1 texel (TW/TH do TEX0 do VU1); uvVolta: o mesmo deslocamento inteiro (repeticao) em todos;
//  - cor: algum canal RGB a mais de 4; corTeto: so porque o VU1 passa de 0x80;
//  - alfa: com ABE nos dois, alfa de vertice a mais de 4;
//  - abe: PRIM.ABE diferente do MeshData::abe; alpha: com ABE nos dois, ALPHA_1/2 diferente do equivalente nativo;
//  - tfx: textura com TFX != MODULATE ou TCC != RGBA (o nativo sempre modula com alfa da textura); fge: com neblina;
//  - fgeDifere: PRIM.FGE diferente do Vtx::fge; neblina: com FGE nos dois, F de algum vertice a mais de 4.
struct Attr {
    uint32_t n = 0, uv = 0, uvVolta = 0, cor = 0, corTeto = 0, alfa = 0, abe = 0, alpha = 0, tfx = 0, fge = 0, fgeDifere = 0, neblina = 0;
    uint8_t vuF = 0, natF = 0;                    // 1o F diferente
    float du = 0.f, dv = 0.f;                     // 1o uv diferente: deslocamento em texels (VU1 - nativo) no vertice pior
    uint8_t vuC[4] = {}, natC[4] = {};            // 1a cor (ou alfa) diferente
    uint16_t vuAlpha = 0, natAlpha = 0;           // 1o ALPHA diferente
    void add(const Attr &o) {
        n += o.n; uv += o.uv; uvVolta += o.uvVolta; cor += o.cor; corTeto += o.corTeto; alfa += o.alfa; abe += o.abe; alpha += o.alpha;
        tfx += o.tfx; fge += o.fge; fgeDifere += o.fgeDifere; neblina += o.neblina;
    }
    uint32_t diffs() const { return uv + cor + alfa + abe + alpha + fgeDifere + neblina; }
};

void compareAttr(const Tri &t, const Tri &n, Attr &at)
{
    ++at.n;
    int map[3];
    for (int a = 0; a < 3; ++a) {
        float best = 1e30f;
        for (int b = 0; b < 3; ++b) {
            const float d = std::max(std::fabs(t.x[a] - n.x[b]), std::fabs(t.y[a] - n.y[b]));
            if (d < best) { best = d; map[a] = b; }
        }
    }
    const bool textured = t.tex && t.tex != kTexUnknown;
    if (textured) {
        const float tw = float(1u << ((t.tex0 >> 26) & 0xF)), th = float(1u << ((t.tex0 >> 30) & 0xF));
        float worst = 0.f, du = 0.f, dv = 0.f;
        bool wrap = true;
        float wu = 0.f, wv = 0.f;
        for (int a = 0; a < 3; ++a) {
            const float eu = (t.u[a] - n.u[map[a]]) * tw, ev = (t.v[a] - n.v[map[a]]) * th;
            if (std::max(std::fabs(eu), std::fabs(ev)) > worst) { worst = std::max(std::fabs(eu), std::fabs(ev)); du = eu; dv = ev; }
            const float iu = std::round(t.u[a] - n.u[map[a]]), iv = std::round(t.v[a] - n.v[map[a]]);
            if (!a) { wu = iu; wv = iv; }
            wrap = wrap && iu == wu && iv == wv && std::fabs(eu - iu * tw) <= 1.f && std::fabs(ev - iv * th) <= 1.f;
        }
        if (worst > 1.f) {
            if (wrap) ++at.uvVolta;
            else if (!at.uv++) { at.du = du; at.dv = dv; }
        }
        const uint32_t tcc = uint32_t((t.tex0 >> 34) & 1), tfx = uint32_t((t.tex0 >> 35) & 3);
        if (tfx != 0 || tcc != 1) ++at.tfx;
    }
    bool cor = false, teto = false, alfa = false;
    for (int a = 0; a < 3; ++a) {
        const uint8_t *vc = t.c[a], *nc = n.c[map[a]];
        for (int k = 0; k < 3; ++k)
            if (std::abs(int(vc[k]) - int(nc[k])) > 4) (vc[k] > 0x80 && nc[k] <= 0x80 ? teto : cor) = true;
        if (t.abe && n.abe && std::abs(int(vc[3]) - int(nc[3])) > 4) alfa = true;
        if ((cor || alfa) && !at.cor && !at.alfa) { std::memcpy(at.vuC, vc, 4); std::memcpy(at.natC, nc, 4); }
    }
    at.cor += cor;
    at.corTeto += teto && !cor;
    at.alfa += alfa;
    at.abe += t.abe != n.abe;
    if (t.abe && n.abe && t.alpha != kAlphaUnknown && t.alpha != n.alpha && !at.alpha++) { at.vuAlpha = t.alpha; at.natAlpha = n.alpha; }
    at.fge += t.fge;
    at.fgeDifere += t.fge != n.fge;
    if (t.fge && n.fge)
        for (int a = 0; a < 3; ++a)
            if (std::abs(int(t.f[a]) - int(n.f[map[a]])) > 4) {
                if (!at.neblina) { at.vuF = t.f[a]; at.natF = n.f[map[a]]; }
                ++at.neblina;
                break;
            }
}

// SPRITE do GS de OBJECT (objType 2, wotm::ObjSprite) contra o nativo: centro projetado -/+ (512, 256) * lado / W, como o
// microprograma 0x2E58, com a camera de cada passe (vale a que casar mais). Linha [vu1check] sprites de objeto.
void compareObjSprites(const std::vector<Quad> &vu)
{
    std::vector<Quad> nat[2];
    for (int ci = 0; ci < 2; ++ci) {
        const Cam &cam = g_cam[ci];
        for (const auto &kv : g_nats[0])
            for (const auto &s : kv.second.sprites) {
                const float d[3] = {s[0] - cam.eye[0], s[1] - cam.eye[1], s[2] - cam.eye[2]};
                const float w = cam.W[3][0] * d[0] + cam.W[3][1] * d[1] + cam.W[3][2] * d[2] + cam.W[3][3];
                float x, y;
                if (!(w >= 1.f) || !projectPt(cam, s.data(), x, y)) continue;
                const float hx = 512.f * s[3] / w, hy = 256.f * s[3] / w;
                const Quad q{{x - hx, x + hx, x + hx, x - hx}, {y - hy, y - hy, y + hy, y + hy}, 4};
                bool in = true;
                for (int k = 0; k < 4; ++k) in = in && q.x[k] >= kX0 && q.x[k] < kX1 && q.y[k] >= kY0 && q.y[k] < kY1;
                if (in) nat[ci].push_back(q);
            }
    }
    if (vu.empty() && nat[0].empty() && nat[1].empty()) return;
    const uint32_t m0 = matchQuads(vu, nat[0]), m1 = matchQuads(vu, nat[1]);
    const int ci = m1 > m0 ? 1 : 0;
    std::fprintf(stderr, "[vu1check] g=%u sprites de objeto: vu %zu nativo %zu casados %u (camera do passe %d)\n", g_frame, vu.size(), nat[ci].size(),
                 std::max(m0, m1), ci);
    if (g_dump)
        for (int s = 0; s < 2; ++s)
            for (const Quad &q : s ? nat[ci] : vu)
                std::fprintf(g_dump, "objspr,%s,%.3f,%.3f,%.3f,%.3f\n", s ? "nat" : "vu", q.x[0], q.y[0], q.x[2], q.y[2]);
}

// Textura em texto: TBP, PSM, tamanho, CLUT (ou "sem textura" / "tex ?").
void texStr(uint64_t v, char *s, size_t n)
{
    if (!v) { std::snprintf(s, n, "sem textura"); return; }
    if (v == kTexUnknown) { std::snprintf(s, n, "tex ?"); return; }
    const wotm::gs::Tex0 t = wotm::gs::Tex0::decode(v);
    std::snprintf(s, n, "tbp %x psm %u %ux%u cbp %x csa %u", t.tbp0, t.psm, 1u << t.tw, 1u << t.th, t.cbp, t.csa);
}

// ---- Inventario (linhas "[vu1check] inv"): o que cada lado desenha sem par no outro, por dono ----
// Referencia: triangulos e SPRITE do VU1 sem objeto nativo nem fx[] (agrupados pela origem do MSCAL) e todo primitivo do PATH2/PATH3,
// agrupados por (caminho, tipo, textura, FRAME, ABE). Nativo: objetos da 1a coleta com triangulos cujo polyPkt o VU1 nao tocou, inclusive
// ceu e icones (skip). "Nos dois lados" por objeto ja sai na comparacao (faltam/sobram). Tela em pixels (0..640 x 0..224, canto da tela
// do GS); "TELA INTEIRA" = caixa que cobre a tela (fade, limpeza); "em textura" = FRAME fora dos buffers de tela.
struct InvGroup { uint32_t n = 0; float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f; uint32_t src0 = ~0u, src1 = 0; uint8_t rgba[4] = {}; };
using InvKey = std::tuple<uint8_t, uint8_t, uint64_t, uint16_t, bool, uint32_t>;   // caminho, tipo (3 tri / 6 sprite), textura, FBP, ABE, origem (VU1: MSCAL & ~0xFF)

// Nome do dado do ELF que contem o endereco (wotm::symtab, gerado por tools/native_syms.py): "nome+0x10"; "" se nenhum (heap).
const char *symOf(uint32_t a, char *buf, size_t n)
{
    const wotm::symtab::Entry *e = wotm::symtab::find(a);
    if (!e) { buf[0] = 0; return buf; }
    if (a == e->addr) std::snprintf(buf, n, "%s", e->name);
    else std::snprintf(buf, n, "%s+0x%x", e->name, a - e->addr);
    return buf;
}

// Id de textura (indice em texInfo, o numero que o nativo usa) pelo TBP0 do TEX0: o pacote de upload de texInfo[id] leva o DBP no
// BITBLTBUF (+0x30, bits 32..45) e o PSM (bits 56..61). Texturas residentes do menu (texInfo aponta para outra coisa) ficam de fora.
std::map<uint32_t, uint32_t> texIdsByTbp(const uint8_t *rdram)
{
    std::map<uint32_t, uint32_t> out;
    if (!rdram) return out;
    const wotm::Ram ram{rdram, PS2_RAM_SIZE};
    for (uint32_t id = 1; id < 1000; ++id) {
        const uint32_t ptr = ram.u32(wotm::addr::texInfo + 16 * id) & 0x0FFFFFFF;
        if (!ptr || !ram.ok(ptr, 0x80)) continue;
        const uint32_t hi = ram.u32(ptr + 0x34);   // palavra alta do BITBLTBUF: DBP 0..13, DBW 16..21, DPSM 24..29
        const uint32_t dbp = hi & 0x3FFF, dpsm = (hi >> 24) & 0x3F;
        if (dpsm > 0x34 || ram.u16(ptr + 0x50) == 0) continue;   // nao parece um pacote de upload
        out.emplace(dbp, id);
    }
    return out;
}

void inventory(const std::map<uint32_t, std::map<uint32_t, std::vector<Tri>>> &vuBy, const std::map<uint32_t, uint32_t> &skipVu,
               const std::vector<const Tri *> &orphanTris, const std::vector<size_t> &orphanRects)
{
    const std::map<uint32_t, uint32_t> texIds = texIdsByTbp(g_rdram);
    auto texName = [&](uint64_t tex, char *s, size_t n) {
        texStr(tex, s, n);
        if (!tex || tex == kTexUnknown) return;
        const auto it = texIds.find(uint32_t(tex & 0x3FFF));
        if (it != texIds.end()) {
            char t[96];
            std::snprintf(t, sizeof t, "tex %u (%s)", it->second, s);
            std::snprintf(s, n, "%s", t);
        }
    };
    // HUD (Scene::hud): por id, com os triangulos do VU1 que cairam nos seus pacotes (a comparacao por objeto os inclui).
    {
        std::map<uint16_t, std::pair<uint32_t, uint32_t>> hud;   // id -> (objetos, tris do VU1)
        if (!g_nats.empty())
            for (const auto &kv : g_nats[0])
                if (kv.second.hudId) {
                    auto &h = hud[kv.second.hudId];
                    ++h.first;
                    const auto vb = vuBy.find(kv.first);
                    if (vb != vuBy.end()) for (const auto &c : vb->second) h.second += uint32_t(c.second.size());
                }
        uint32_t tris = 0;
        for (const auto &kv : hud) tris += kv.second.second;
        if (!hud.empty()) {
            std::string s;
            char b[48];
            size_t shown = 0;
            for (const auto &kv : hud) {
                if (!kv.second.second) continue;   // o VU1 nao desenhou este id neste quadro (fora da tela ou oculto)
                if (shown++ >= 40) { s += " ..."; break; }
                std::snprintf(b, sizeof b, " %x:%u", unsigned(kv.first), kv.second.second);
                s += b;
            }
            std::fprintf(stderr, "[vu1check] inv   HUD: %zu ids, %u tris do VU1; por id (id:tris):%s\n", hud.size(), tris, s.c_str());
        }
    }
    std::map<InvKey, InvGroup> groups;
    auto add = [&](const InvKey &k, float x0, float y0, float x1, float y1, uint32_t src, const uint8_t *rgba) {
        InvGroup &g = groups[k];
        if (!g.n && rgba) std::memcpy(g.rgba, rgba, 4);
        ++g.n;
        g.x0 = std::min(g.x0, x0); g.y0 = std::min(g.y0, y0); g.x1 = std::max(g.x1, x1); g.y1 = std::max(g.y1, y1);
        if (src) { g.src0 = std::min(g.src0, src); g.src1 = std::max(g.src1, src); }
    };
    auto origin = [&](uint32_t prog) { return g_progs[prog - 1].empty() ? 0u : g_progs[prog - 1].back() & ~0xFFu; };
    uint32_t nVuTris = 0, nVuRects = 0, nP2 = 0, nP3 = 0, nRtt = 0;
    for (const Tri *t : orphanTris) {
        add({1, 3, t->tex, t->fbp, t->abe, origin(t->prog)}, std::min({t->x[0], t->x[1], t->x[2]}), std::min({t->y[0], t->y[1], t->y[2]}),
            std::max({t->x[0], t->x[1], t->x[2]}), std::max({t->y[0], t->y[1], t->y[2]}), 0, t->c[0]);
        ++nVuTris;
    }
    for (size_t i : orphanRects) {
        if (i >= g_vuRectInfo.size()) continue;
        const RefPrim &r = g_vuRectInfo[i];
        add({1, 6, r.tex, r.fbp, r.abe, origin(g_vuRects[i].first)}, r.x0, r.y0, r.x1, r.y1, 0, r.rgba);
        ++nVuRects;
    }
    for (const RefPrim &r : g_ref23) {
        add({r.path, uint8_t(r.kind == 6 ? 6 : 3), r.tex, r.fbp, r.abe, 0}, r.x0, r.y0, r.x1, r.y1, r.src, r.rgba);
        ++(r.path == 2 ? nP2 : nP3);
        if (r.fbp != 0xFFFF && r.fbp != 0x46 && r.fbp != 0x8C) ++nRtt;
    }
    // Nativo sem referencia: objetos da 1a coleta cujo polyPkt nenhum triangulo do VU1 tocou
    struct NatOnly { uint32_t pp; const NatObj *o; uint32_t tris, onScreen; float x0, y0, x1, y1; uint8_t aMax = 0; };   // aMax: maior alfa de vertice (0 = invisivel: o nativo "desenha" mas nao aparece)
    std::vector<NatOnly> natOnly;
    uint32_t bothObjs = 0, natOnlyTris = 0, natOnlySkip = 0;
    if (!g_nats.empty())
        for (const auto &kv : g_nats[0]) {
            const NatObj &o = kv.second;
            if (vuBy.count(kv.first) || skipVu.count(kv.first)) { ++bothObjs; continue; }
            if (o.tris.empty()) continue;
            const std::vector<Tri> pr = project(o.tris, g_cam[0], kv.first);
            NatOnly r{kv.first, &o, uint32_t(o.tris.size()), uint32_t(pr.size()), 1e9f, 1e9f, -1e9f, -1e9f};
            for (const Tri &t : pr)
                for (int c = 0; c < 3; ++c) {
                    r.x0 = std::min(r.x0, t.x[c]); r.y0 = std::min(r.y0, t.y[c]); r.x1 = std::max(r.x1, t.x[c]); r.y1 = std::max(r.y1, t.y[c]);
                    r.aMax = std::max(r.aMax, t.c[c][3]);
                }
            natOnlyTris += r.onScreen;
            natOnlySkip += o.skip;
            natOnly.push_back(r);
        }
    uint32_t refPrims = 0;
    for (const auto &kv : groups) refPrims += kv.second.n;
    std::fprintf(stderr, "[vu1check] inv g=%u so a referencia: %zu grupos, %u primitivos (vu1 sem dono %u tris + %u sprites; path2 %u; path3 %u; em textura %u) | "
                         "so o nativo: %zu objetos, %u tris na tela (ceu/icones %u) | objetos nos dois lados %u\n",
                 g_frame, groups.size(), refPrims, nVuTris, nVuRects, nP2, nP3, nRtt, natOnly.size(), natOnlyTris, natOnlySkip, bothObjs);
    std::vector<std::pair<InvKey, InvGroup>> gs(groups.begin(), groups.end());
    std::sort(gs.begin(), gs.end(), [](const auto &a, const auto &b) { return a.second.n > b.second.n; });
    constexpr size_t kMaxLines = 24;
    for (size_t i = 0; i < gs.size() && i < kMaxLines; ++i) {
        const InvKey &k = gs[i].first;
        const InvGroup &g = gs[i].second;
        char tex[128], where[160] = "", src[192] = "", sym[96];
        texName(std::get<2>(k), tex, sizeof tex);
        if (std::get<0>(k) == 1) {
            symOf(std::get<5>(k), sym, sizeof sym);
            std::snprintf(where, sizeof where, "vu1 mscal %x%s%s%s", std::get<5>(k), sym[0] ? " (" : "", sym, sym[0] ? ")" : "");
        } else std::snprintf(where, sizeof where, "path%u", unsigned(std::get<0>(k)));
        if (g.src0 != ~0u) {
            symOf(g.src0, sym, sizeof sym);
            std::snprintf(src, sizeof src, " src %x-%x%s%s%s", g.src0, g.src1, sym[0] ? " (" : "", sym, sym[0] ? ")" : "");
        }
        const uint16_t fbp = std::get<3>(k);
        const bool full = g.x0 <= kX0 + 2.f && g.y0 <= kY0 + 2.f && g.x1 >= kX1 - 2.f && g.y1 >= kY1 - 2.f;
        std::fprintf(stderr, "[vu1check] inv   ref %s %s x%u: %s fbp %x abe %d cor %02x%02x%02x%02x tela %.0f,%.0f-%.0f,%.0f%s%s%s\n", where,
                     std::get<1>(k) == 6 ? "sprites" : "tris", g.n, tex, unsigned(fbp), int(std::get<4>(k)), g.rgba[0], g.rgba[1], g.rgba[2], g.rgba[3],
                     g.x0 - kX0, g.y0 - kY0, g.x1 - kX0, g.y1 - kY0, full ? " TELA INTEIRA" : "",
                     fbp != 0xFFFF && fbp != 0x46 && fbp != 0x8C ? " em textura" : "", src);
    }
    if (gs.size() > kMaxLines) std::fprintf(stderr, "[vu1check] inv   ref ... e mais %zu grupos\n", gs.size() - kMaxLines);
    std::sort(natOnly.begin(), natOnly.end(), [](const NatOnly &a, const NatOnly &b) { return a.onScreen > b.onScreen; });
    size_t shown = 0;
    for (const NatOnly &r : natOnly) {
        if (!r.onScreen) continue;   // objeto nativo inteiro fora da tela: sem interesse
        if (shown++ >= kMaxLines) break;
        std::fprintf(stderr, "[vu1check] inv   nat pkt %x no %x tex %u itens %d%s%s: tris %u (na tela %u) alfa max %02x tela %.0f,%.0f-%.0f,%.0f flags %x\n", r.pp, r.o->node,
                     unsigned(r.o->tex), r.o->items, r.o->skinned ? " skinned" : "", r.o->skip ? " ceu/icone" : "", r.tris, r.onScreen, r.aMax,
                     r.x0 - kX0, r.y0 - kY0, r.x1 - kX0, r.y1 - kY0, g_rdram ? unsigned(g_rdram[r.o->node + 0xC] | (g_rdram[r.o->node + 0xD] << 8)) : 0u);
    }
    if (shown > kMaxLines) std::fprintf(stderr, "[vu1check] inv   nat ... e mais %zu objetos\n", shown - kMaxLines);
}

void compare()
{
    if (g_progs.empty() || g_nats.empty()) {
        std::fprintf(stderr, "[vu1check] g=%u: nenhum MSCAL no quadro (o hierFlushObjQ nao disparou o DMA?)\n", g_frame);
        inventory({}, {}, {}, {});
        return;
    }
    // triangulos do VU1 por (objeto, cadeia); os sem objeto nativo ficam por origem do MSCAL
    std::map<uint32_t, std::map<uint32_t, std::vector<Tri>>> vuBy;
    std::map<uint32_t, uint32_t> orphan;
    FxVuMap fxVu;   // particulas: por sistema (fx[]) e cadeia
    uint32_t fxTris = 0, rectsOrphan = 0;
    std::vector<Quad> objRects;   // SPRITE do GS de OBJECT (ObjSprite)
    std::vector<const Tri *> orphanTriRefs;   // inventario: triangulos e SPRITE do VU1 sem objeto nativo nem fx[]
    std::vector<size_t> orphanRects;
    std::map<uint32_t, uint32_t> skipVu;   // inventario: triangulos do VU1 por objeto skip (ceu, icones): so contados
    for (size_t ri = 0; ri < g_vuRects.size(); ++ri) {
        const auto &r = g_vuRects[ri];
        if (const int sys = fxOf(r.first); sys >= 0) fxVu[uint32_t(sys)][g_progChain[r.first - 1]].rects.push_back(r.second);
        else if (objectOf(g_nats[g_progChain[r.first - 1]], r.first)) objRects.push_back(r.second);
        else {
            ++rectsOrphan;
            orphanRects.push_back(ri);
            if (g_dump) {   // origens do programa (trechos da cadeia DMA) do SPRITE fora de fx[]: mapear com o .map
                std::fprintf(g_dump, "rectorf,%u", r.first);
                for (uint32_t a : g_progs[r.first - 1]) std::fprintf(g_dump, ",%x", a);
                std::fprintf(g_dump, "\n");
            }
        }
    }
    // Render-to-texture: triangulos desenhados num FRAME fora dos dois buffers de tela (FBP 0x46 e 0x8C, 640x224) nao vao para a
    // tela; ficam fora. E o quadro da sombra projetada (FBP 0x1F3 = tbp 0x3E60), que recebe do psBlkDma as silhuetas sem textura
    // dos monstros; nem sempre algum receptor usa essa textura no mesmo quadro (comeco da luta).
    uint32_t rtt = 0;
    for (const Tri &t : g_vu) {
        if (t.fbp != 0xFFFF && t.fbp != 0x46 && t.fbp != 0x8C) { ++rtt; continue; }
        const uint32_t chain = g_progChain[t.prog - 1];
        const uint32_t key = objectOf(g_nats[chain], t.prog);
        if (key) {
            if (!g_nats[chain].at(key).skip) vuBy[key][chain].push_back(t);
            else ++skipVu[key];
            continue;
        }
        if (const int sys = fxOf(t.prog); sys >= 0) {
            fxVu[uint32_t(sys)][chain].tris.push_back(t);
            ++fxTris;
            continue;
        }
        ++orphan[g_progs[t.prog - 1].empty() ? 0u : g_progs[t.prog - 1].back() & ~0xFFu];
        orphanTriRefs.push_back(&t);
        if (g_dump) std::fprintf(g_dump, "0,orf,0,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%llx,%x\n", t.prog, t.x[0], t.y[0], t.x[1], t.y[1], t.x[2], t.y[2],
                                 (unsigned long long)t.tex, unsigned(t.fbp));
    }
    if (g_dump) {   // luzes da CPU: paraL (0x6DFC00, direcoes x32 nas colunas) e vu1LightColor (D_0025B238, cores/128 e ambiente)
        std::fprintf(g_dump, "luzes");
        for (uint32_t a : {wotm::sym::paraL, 0x25B238u})
            for (int k = 0; k < 16; ++k) { float f; std::memcpy(&f, g_rdram + a + 4 * k, 4); std::fprintf(g_dump, ",%g", f); }
        float spec;
        std::memcpy(&spec, g_rdram + wotm::sym::specularColor, 4);   // specularColor
        std::fprintf(g_dump, ",%g", spec);
        std::fprintf(g_dump, "\n");
    }
    if (g_dump)
        for (size_t i = 0; i < g_progs.size(); ++i) {
            std::fprintf(g_dump, "prog,%zu,%u", i + 1, g_progChain[i]);
            for (uint32_t a : g_progs[i]) std::fprintf(g_dump, ",%x", a);
            std::fprintf(g_dump, "\n");
            if (i < g_progHdr.size()) {
                std::fprintf(g_dump, "hdr,%zu,%x", i + 1, g_progHdr[i].addr);
                for (int k = 0; k < 8; ++k)
                    for (int c = 0; c < 4; ++c) std::fprintf(g_dump, ",%g", g_progHdr[i].q[k][c]);
                std::fprintf(g_dump, "\n");
                if (g_progHdr[i].addr == 0x15D8u) {   // luzes pontuais: q169-q171 (-luz no objeto, transpostas), q172-q174 (cores), q175 (falloffs)
                    std::fprintf(g_dump, "ptl,%zu", i + 1);
                    for (int k = 0; k < 7; ++k)
                        for (int c = 0; c < 4; ++c) std::fprintf(g_dump, ",%g", g_progHdr[i].pt[k][c]);
                    std::fprintf(g_dump, "\n");
                }
            }
        }
    // texDifere: casados com textura diferente; vuTex/natTex/natSlot: o par diferente mais comum do objeto
    struct Row { uint32_t pp; const NatObj *o; uint32_t vu, nat, falta, sobra, costas, texDifere; uint64_t vuTex, natTex; uint8_t natSlot; Attr at; };
    std::vector<Row> rows;
    uint32_t totVu = 0, totNat = 0, totFalta = 0, totSobra = 0, totCostas = 0, totRep = 0, objVu = 0, objNat = 0, objBoth = 0, bad = 0;
    uint32_t totTexDifere = 0, totTexUnknown = 0, totShadowOnly = 0, totRecorte = 0;
    Attr attrTot;
    std::vector<uint32_t> keys;   // objetos da 1a coleta e os que o VU1 desenhou
    for (const auto &kv : g_nats[0]) keys.push_back(kv.first);
    for (const auto &kv : vuBy) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    for (uint32_t key : keys) {
        // coleta de referencia do objeto: a da cadeia que mais trouxe triangulos dele; sem nenhum, a 1a
        uint32_t chain = 0;
        std::vector<Tri> vu;
        const auto vit = vuBy.find(key);
        if (vit != vuBy.end()) {
            size_t most = 0;
            for (const auto &kv : vit->second) {
                vu.insert(vu.end(), kv.second.begin(), kv.second.end());
                if (kv.second.size() > most) { most = kv.second.size(); chain = kv.first; }
            }
        }
        const auto oit = g_nats[chain].find(key);
        if (oit == g_nats[chain].end() || oit->second.skip) continue;
        const NatObj *op = &oit->second;
        // camera: a do hier que montou o pacote (copiada nele); uma cadeia do passe 0 pode sair pela interrupcao ja dentro do passe 1,
        // entao vale a que casar mais. Coleta: a da cadeia e, se nao casar tudo, a da 1a cadeia (a OT de alfa so sai no fim do quadro,
        // quando a animacao de pecas presas aos monstros ja pode ter andado; o pacote leva as matrizes do hier).
        std::vector<Tri> nat, natCulled;
        std::vector<int> vuPair;
        std::vector<char> natHit;
        uint32_t m = 0;
        bool first = true;
        const NatObj *cands[3] = {op, chain && g_nats[0].count(key) ? &g_nats[0].at(key) : nullptr,
                                  g_natPass1.count(key) ? &g_natPass1.at(key) : nullptr};
        for (const NatObj *cand : cands) {
            if (!cand || (!first && m >= vu.size())) continue;
            for (int ci = 0; ci < 2; ++ci) {
                if (!first && m >= vu.size()) break;
                std::vector<Tri> c1;
                std::vector<Tri> n1 = project(cand->tris, g_cam[ci], key, &c1);
                std::vector<int> p1;
                std::vector<char> h1;
                const uint32_t m1 = match(vu, n1, p1, h1);
                if (first || m1 > m) { m = m1; nat.swap(n1); natCulled.swap(c1); vuPair.swap(p1); natHit.swap(h1); op = cand; }
                first = false;
            }
        }
        const NatObj &o = *op;
        bad += o.badVerts;
        // Passadas repetidas: o VU1 redesenha o mesmo triangulo (a sombra projetada repassa cada objeto que a recebe, uma vez por
        // monstro, na OT de alfa: psAttachDmaToAlpha). O que sobra e casa com um nativo ja casado conta como repetido, nao como falta.
        uint32_t rep = 0;
        for (int round = 0; round < 4 && m + rep < vu.size(); ++round) {
            std::vector<size_t> idx;
            std::vector<Tri> rest;
            for (size_t i = 0; i < vu.size(); ++i)
                if (vuPair[i] == -1) { idx.push_back(i); rest.push_back(vu[i]); }
            std::vector<int> pair2;
            std::vector<char> hit2;
            if (!match(rest, nat, pair2, hit2)) break;
            for (size_t j = 0; j < rest.size(); ++j)
                if (pair2[j] >= 0) { vuPair[idx[j]] = -2; ++rep; }   // -2 = repetido (fora da comparacao de textura)
        }
        // Costas pelo recorte: o triangulo que sai da faixa de recorte (CLIP/FCAND no microprograma) vai para o caminho de clipping,
        // que nao descarta faces de costas; o nativo as descarta (Scene::backFacing) e elas ficam escondidas atras das de frente.
        uint32_t rec = 0;
        if (m + rep < vu.size() && !natCulled.empty()) {
            std::vector<size_t> idx;
            std::vector<Tri> rest;
            for (size_t i = 0; i < vu.size(); ++i)
                if (vuPair[i] == -1) { idx.push_back(i); rest.push_back(vu[i]); }
            std::vector<int> pair2;
            std::vector<char> hit2;
            match(rest, natCulled, pair2, hit2);
            for (size_t j = 0; j < rest.size(); ++j)
                if (pair2[j] >= 0) { vuPair[idx[j]] = -3; ++rec; }   // -3 = costas pelo recorte
        }
        totRecorte += rec;
        // textura dos casados: a do GS no XGKICK contra a que o nativo escolheu para o slot da secao
        // Casado so pela sombra: textura PSMCT32 que o objeto nativo nao usa (o quadro da sombra projetada, tbp 3e60) num triangulo que
        // nenhuma passada base casou; fica fora do "textura difere".
        uint32_t texDifere = 0;
        std::map<std::tuple<uint64_t, uint64_t, uint8_t>, uint32_t> texPairs;
        std::vector<uint64_t> natTexs;
        for (const Tri &n : nat) natTexs.push_back(n.tex);
        std::sort(natTexs.begin(), natTexs.end());
        natTexs.erase(std::unique(natTexs.begin(), natTexs.end()), natTexs.end());
        for (size_t i = 0; i < vu.size(); ++i) {
            if (vuPair[i] < 0) continue;
            const Tri &n = nat[size_t(vuPair[i])];
            if (vu[i].tex == kTexUnknown) ++totTexUnknown;
            else if (!sameTex(vu[i].tex, n.tex)) {
                const bool shadow = vu[i].tex && wotm::gs::Tex0::decode(vu[i].tex).psm == 0 &&
                                    std::none_of(natTexs.begin(), natTexs.end(), [&](uint64_t nt) { return sameTex(vu[i].tex, nt); });
                if (shadow) { ++totShadowOnly; continue; }
                ++texDifere; ++texPairs[{vu[i].tex, n.tex, n.slot}];
            }
        }
        std::tuple<uint64_t, uint64_t, uint8_t> texWorst{};
        uint32_t texWorstN = 0;
        for (const auto &kv : texPairs)
            if (kv.second > texWorstN) { texWorstN = kv.second; texWorst = kv.first; }
        totTexDifere += texDifere;
        // Atributos dos casados (fora os repetidos e os so pela sombra): cada vertice do VU1 com o vertice nativo mais proximo na tela.
        Attr at{};
        for (size_t i = 0; i < vu.size(); ++i) {
            if (vuPair[i] < 0) continue;
            const Tri &t = vu[i], &n = nat[size_t(vuPair[i])];
            if (t.tex && t.tex != kTexUnknown && !sameTex(t.tex, n.tex)) continue;   // textura diferente ja contada (ou sombra)
            compareAttr(t, n, at);
        }
        attrTot.add(at);
        if (vu.empty() && nat.empty()) continue;
        uint32_t vuBack = 0, costas = 0, sobra = 0;
        for (const Tri &t : vu) vuBack += area2(t) > 0.f;
        const bool culled = vuBack * 50 <= vu.size();
        for (size_t i = 0; i < nat.size(); ++i)
            if (!natHit[i]) ++((culled && area2(nat[i]) > 0.f) ? costas : sobra);
        const uint32_t nVu = uint32_t(vu.size()), nNat = uint32_t(nat.size()) - costas, falta = nVu - m - rep - rec;
        objVu += nVu > 0; objNat += nNat > 0; objBoth += nVu && nNat;
        totVu += nVu; totNat += nNat; totFalta += falta; totSobra += sobra; totCostas += costas; totRep += rep;
        rows.push_back({key, &o, nVu - rep, nNat, falta, sobra, costas, texDifere, std::get<0>(texWorst), std::get<1>(texWorst), std::get<2>(texWorst), at});
        if (g_dump) {
            // depois do FBP: u, v e RGBA (hex) de cada vertice, ABE, ALPHA, neblina e o TEX0 inteiro (so VU1)
            auto row = [&](const char *side, int hit, const Tri &t) {
                std::fprintf(g_dump, "%x,%s,%d,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%llx,%x", key, side, hit, t.prog, t.x[0], t.y[0], t.x[1], t.y[1], t.x[2],
                             t.y[2], (unsigned long long)t.tex, unsigned(t.fbp));
                for (int k = 0; k < 3; ++k)
                    std::fprintf(g_dump, ",%.5f,%.5f,%02x%02x%02x%02x", t.u[k], t.v[k], t.c[k][0], t.c[k][1], t.c[k][2], t.c[k][3]);
                std::fprintf(g_dump, ",%d,%x,%d,%llx\n", int(t.abe), unsigned(t.alpha), int(t.fge), (unsigned long long)t.tex0);
            };
            for (size_t i = 0; i < vu.size(); ++i) row("vu", vuPair[i] >= 0, vu[i]);
            for (size_t i = 0; i < nat.size(); ++i) row("nat", natHit[i], nat[i]);
            std::fprintf(g_dump, "obj,%u,%x,%x,%u,%d,%d,%x,%x\n", chain, key, o.node, unsigned(o.tex), o.skinned, o.items, o.span,
                         unsigned(g_rdram[o.node + 0xC] | (g_rdram[o.node + 0xD] << 8)));   // flags do OBJECT
        }
    }
    uint32_t orphanTris = 0;
    for (const auto &kv : orphan) orphanTris += kv.second;
    const double pct = totVu ? 100.0 * (totVu - totRep - totFalta) / (totVu - totRep) : 0.0;
    std::fprintf(stderr, "[vu1check] g=%u objetos vu %u nativo %u ambos %u | tris vu %u (repetidos %u) nativo %u casados %.1f%% faltam %u sobram %u "
                         "(costas %u) | costas pelo recorte %u | textura difere %u, sem TEX0 %u, so pela sombra %u | XGKICK %u tris, %u fora da tela, %u em textura (sombra) | particulas %u tris, sem objeto %u tris, cadeias %u (coleta nativa %.0f + %.0f ms), programas %zu, "
                         "vertices nao finitos %u, VIF1 fora de passo %u\n",
                 g_frame, objVu, objNat, objBoth, totVu, totRep, totNat, pct, totFalta, totSobra, totCostas, totRecorte, totTexDifere, totTexUnknown, totShadowOnly, g_vuKicked, g_vuOff, rtt, fxTris, orphanTris, g_chains,
                 g_walkMs, g_projMs, g_progs.size(), bad, g_wotmVif1Bad);
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.falta + a.sobra > b.falta + b.sobra; });
    int shown = 0;
    for (const Row &r : rows) {
        if (shown >= 8 || r.falta + r.sobra < 4 || 10 * (r.falta + r.sobra) < std::max(r.vu, r.nat)) continue;
        ++shown;
        std::fprintf(stderr, "[vu1check]   pkt %x no %x tex %u%s itens %d: vu %u nativo %u faltam %u sobram %u (costas %u) flags %x\n", r.pp,
                     r.o->node, unsigned(r.o->tex), r.o->skinned ? " skinned" : "", r.o->items, r.vu, r.nat, r.falta, r.sobra, r.costas,
                     unsigned(g_rdram[r.o->node + 0xC] | (g_rdram[r.o->node + 0xD] << 8)));   // flags do OBJECT (0x8000 = OT de alfa)
    }
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.texDifere > b.texDifere; });
    for (size_t i = 0; i < rows.size() && i < 8 && rows[i].texDifere; ++i) {
        const Row &r = rows[i];
        char a[96], b[96];
        texStr(r.vuTex, a, sizeof a);
        texStr(r.natTex, b, sizeof b);
        std::fprintf(stderr, "[vu1check]   textura pkt %x no %x tex %u%s: %u de %u casados; mais comum: vu (%s) nativo slot %u (%s)\n", r.pp,
                     r.o->node, unsigned(r.o->tex), r.o->skinned ? " skinned" : "", r.texDifere, r.vu - r.falta, a, unsigned(r.natSlot), b);
    }
    std::fprintf(stderr, "[vu1check] g=%u atributos: %u casados | uv difere %u (volta inteira %u) | cor difere %u (so acima de 0x80 %u) | alfa difere %u | "
                         "ABE difere %u | ALPHA difere %u | TFX/TCC fora do padrao %u | neblina %u (FGE difere %u, F difere %u)\n",
                 g_frame, attrTot.n, attrTot.uv, attrTot.uvVolta, attrTot.cor, attrTot.corTeto, attrTot.alfa, attrTot.abe, attrTot.alpha, attrTot.tfx, attrTot.fge,
                 attrTot.fgeDifere, attrTot.neblina);
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.at.diffs() > b.at.diffs(); });
    for (size_t i = 0; i < rows.size() && i < 8 && rows[i].at.diffs(); ++i) {
        const Row &r = rows[i];
        const Attr &a = r.at;
        std::fprintf(stderr, "[vu1check]   atributos pkt %x no %x tex %u%s flags %x: %u casados; uv %u (%+.1f %+.1f texels) cor %u alfa %u (vu %02x%02x%02x%02x nativo %02x%02x%02x%02x) "
                             "ABE %u ALPHA %u (vu %02x nativo %02x) tfx %u neblina %u (FGE difere %u, F difere %u: vu %u nativo %u)\n",
                     r.pp, r.o->node, unsigned(r.o->tex), r.o->skinned ? " skinned" : "", unsigned(g_rdram[r.o->node + 0xC] | (g_rdram[r.o->node + 0xD] << 8)), a.n,
                     a.uv, a.du, a.dv, a.cor, a.alfa, a.vuC[0], a.vuC[1], a.vuC[2], a.vuC[3], a.natC[0], a.natC[1], a.natC[2], a.natC[3], a.abe, a.alpha,
                     unsigned(a.vuAlpha), unsigned(a.natAlpha), a.tfx, a.fge, a.fgeDifere, a.neblina, unsigned(a.vuF), unsigned(a.natF));
    }
    std::vector<std::pair<uint32_t, uint32_t>> orph(orphan.begin(), orphan.end());
    std::sort(orph.begin(), orph.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    for (size_t i = 0; i < orph.size() && i < 3; ++i)
        if (orph[i].second >= 8) std::fprintf(stderr, "[vu1check]   so no VU1: MSCAL em %x, %u tris\n", orph[i].first, orph[i].second);
    compareFx(fxVu, rectsOrphan);
    compareObjSprites(objRects);
    compareFlare();
    inventory(vuBy, skipVu, orphanTriRefs, orphanRects);
}

// Entrada de hier (0x2029B0): a0 = vista, a1 = passe, ra = de onde veio (rtMain: 0x188750 passe 0, 0x18891C passe 1).
// Comeca um quadro amostrado na entrada do hier(0, 0): zera o estado da checagem e liga o tap do DMA.
void arm(const uint8_t *rdram, uint32_t frame)
{
    g_rdram = rdram;
    g_frame = frame;
    g_pass = 0;
    g_cam[0] = readCam(rdram);
    g_nats.clear();
    g_natPass1.clear();
    g_fxNats.clear();
    g_fxChain.clear();
    g_progs.clear();
    g_progChain.clear();
    g_progHdr.clear();
    g_vu.clear();
    g_vuAll.clear();
    g_vuKicked = g_vuOff = 0;
    std::memset(g_vuPrimVerts, 0, sizeof g_vuPrimVerts);
    g_vuRects.clear();
    g_vuFlare.clear();
    g_flareVuIn.clear();
    g_ref23.clear();
    g_vuRectInfo.clear();
    g_vuRectsKicked = g_vuRectsRtt = 0;
    g_tex0[0] = g_tex0[1] = kTexUnknown;
    g_fbp[0] = g_fbp[1] = 0xFFFF;
    g_alpha[0] = g_alpha[1] = kAlphaUnknown;
    g_test[0] = g_test[1] = 0xFFFFFFFFu;
    g_fogF = 255;
    g_chains = 0;
    g_walkMs = g_projMs = 0.0;
    g_armed = true;
    g_wotmVu1Tap = true;
}

// Fim do quadro amostrado: compara (com o CSV do WOTM_VU1CHECK_DUMP, se pedido) e desliga o tap.
void finish()
{
    g_wotmVu1Tap = false;
    g_armed = false;
    if (const char *path = std::getenv("WOTM_VU1CHECK_DUMP")) {
        char name[1024];
        std::snprintf(name, sizeof name, "%s_%u.csv", path, g_frame);
        g_dump = std::fopen(name, "w");
    }
    if (const char *pre = std::getenv("WOTM_VU1CMP_RAM"); pre && g_rdram) {   // RAM da entrada do hier(0,0) do quadro (crua, como os ram_*.bin)
        char name[1024];
        std::snprintf(name, sizeof name, "%s_%u.bin", pre, g_frame);
        if (FILE *f = std::fopen(name, "wb")) { std::fwrite(g_rdram, 1, PS2_RAM_SIZE, f); std::fclose(f); }
    }
    compare();
    if (g_dump) { std::fclose(g_dump); g_dump = nullptr; }
}

// ---- Instantaneo do quadro amostrado (WOTM_VU1CHECK_SNAP=<prefixo>) e replay sem jogo (ps2EntryRunner --vu1cmp <arquivo>) ----
// O que o VU1 manda ao GS e funcao pura de um estado: a RAM no instante de cada cadeia (a camada nativa a le), memoria e registradores
// do VU1/VIF1 na entrada do quadro, e os bytes de cada cadeia DMA (o VIF1 so le a copia, nunca a RAM). O arquivo guarda isso em
// registros na ordem em que aconteceram, varios quadros no mesmo arquivo (<prefixo>_<g_frame do primeiro>.vu1snap): 'A' (1o quadro:
// estado do VU1/VIF1, RAM e VRAM inteiras), 'a' (quadros seguintes: estado do VU1/VIF1 e so as paginas de 4 KB da RAM e da VRAM que
// mudaram desde o registro anterior), 'C' (cadeia: paginas que mudaram, origens dos trechos e os bytes), 'P' (entrada do hier(0,1):
// paginas que mudaram), 'E' (fim do quadro). Entre quadros da mesma fase quase tudo e igual, entao os seguintes custam poucos MB.
// WOTM_VU1CHECK_SNAP_MAX=<MB> (padrao 2048) para de gravar ao passar do teto. O replay refaz a sequencia dentro do runtime, sem jogo,
// e imprime a mesma saida [vu1check] da checagem ao vivo.
namespace snap
{
constexpr char kMagic[8] = {'W', 'V', 'U', '1', 'S', 'N', 'P', '4'};
constexpr size_t kGsRegsBytes = 15 * sizeof(uint64_t);   // GSRegisters de PMODE a BGCOLOR (os campos simples; CSR/vsyncTick sao atomicos e ficam de fora)
struct Hdr { char magic[8]; uint32_t frame, ramSize, vramSize, vu1State, vifRegs; };
constexpr uint32_t kPage = 4096;
FILE *g_out = nullptr;
bool g_inFrame = false;
std::vector<uint8_t> g_prev, g_prevVram;   // RAM e VRAM como ficaram no ultimo registro (para as paginas que mudaram)
PS2Runtime *g_rt = nullptr;
uint64_t g_bytes = 0, g_max = 2048ull << 20;
uint32_t g_frames = 0;
std::string g_prefix;   // WOTM_VU1CHECK_SNAP; vazio = sem instantaneo
bool g_perShot = false; // fotos (WOTM_FOTOS): um arquivo por quadro, <foto>.vu1snap, fechado no fim do quadro e sem teto
std::string g_file;     // arquivo em gravacao

void put(const void *p, size_t n)
{
    if (!g_out) return;
    if (std::fwrite(p, 1, n, g_out) != n) { std::fprintf(stderr, "[vu1check] erro ao gravar o instantaneo\n"); std::fclose(g_out); g_out = nullptr; }
    g_bytes += n;
}
template <class T> void putv(const T &v) { put(&v, sizeof v); }

void putDelta(const uint8_t *cur, std::vector<uint8_t> &prev)
{
    std::vector<uint32_t> pages;
    const uint32_t n = uint32_t(prev.size() / kPage);
    for (uint32_t pg = 0; pg < n; ++pg)
        if (std::memcmp(cur + size_t(pg) * kPage, prev.data() + size_t(pg) * kPage, kPage) != 0) pages.push_back(pg);
    putv(uint32_t(pages.size()));
    for (uint32_t pg : pages) {
        putv(pg);
        put(cur + size_t(pg) * kPage, kPage);
        std::memcpy(prev.data() + size_t(pg) * kPage, cur + size_t(pg) * kPage, kPage);
    }
}

void putState()
{
    putv(g_rt->vu1().state());
    putv(g_rt->cpu().vu0_fbrst);
    putv(g_rt->memory().vif1_regs);
    put(g_rt->memory().getVU1Code(), PS2_VU1_CODE_SIZE);
    put(g_rt->memory().getVU1Data(), PS2_VU1_DATA_SIZE);
    put(&g_rt->memory().gs_regs, kGsRegsBytes);   // PMODE/DISPFB/DISPLAY: qual buffer o GS apresenta (imagem de referencia do replay)
}

void begin(const uint8_t *ram, uint32_t frame)   // entrada do hier(0, 0) do quadro amostrado
{
    if ((g_prefix.empty() && !g_perShot) || !g_rt) return;
    if (g_out && g_bytes >= g_max && !g_perShot) {
        std::fprintf(stderr, "[vu1check] instantaneo parou no teto (WOTM_VU1CHECK_SNAP_MAX, %llu MB) com %u quadros\n", (unsigned long long)(g_max >> 20), g_frames);
        std::fclose(g_out);
        g_out = nullptr;
        return;
    }
    if (!g_out) {
        if (g_bytes >= g_max && !g_perShot) return;   // ja parou no teto
        char name[1024];
        if (g_perShot) std::snprintf(name, sizeof name, "%s", wotmFotoSnapPath().c_str());
        else std::snprintf(name, sizeof name, "%s_%u.vu1snap", g_prefix.c_str(), frame);
        g_out = std::fopen(name, "wb");
        if (!g_out) { std::fprintf(stderr, "[vu1check] nao consegui gravar %s\n", name); return; }
        g_file = name;
        Hdr h{};
        std::memcpy(h.magic, kMagic, 8);
        h.frame = frame;
        h.ramSize = PS2_RAM_SIZE;
        h.vramSize = uint32_t(PS2_GS_VRAM_SIZE);
        h.vu1State = sizeof(VU1State);
        h.vifRegs = sizeof(VIFRegisters);
        putv(h);
        putv(uint8_t('A'));
        putv(frame);
        putState();
        put(ram, PS2_RAM_SIZE);
        put(g_rt->memory().getGSVRAM(), PS2_GS_VRAM_SIZE);
        g_prev.assign(ram, ram + PS2_RAM_SIZE);
        g_prevVram.assign(g_rt->memory().getGSVRAM(), g_rt->memory().getGSVRAM() + PS2_GS_VRAM_SIZE);
        std::fprintf(stderr, "[vu1check] instantaneo: %s\n", name);
    } else {
        putv(uint8_t('a'));
        putv(frame);
        putState();
        putDelta(ram, g_prev);
        putDelta(g_rt->memory().getGSVRAM(), g_prevVram);
    }
    g_inFrame = true;
    ++g_frames;
}

void chain(const uint8_t *ram, const std::pair<uint32_t, uint32_t> *segs, size_t n, const uint8_t *data, uint32_t bytes)
{
    if (!g_out || !g_inFrame) return;
    putv(uint8_t('C'));
    putDelta(ram, g_prev);
    putv(uint32_t(n));
    put(segs, n * sizeof *segs);
    putv(bytes);
    put(data, bytes);
}

void pass1(const uint8_t *ram)
{
    if (!g_out || !g_inFrame) return;
    putv(uint8_t('P'));
    putDelta(ram, g_prev);
}

// Pacote do canal GIF (PATH3): o ambiente de desenho (FRAME/ZBUF/XYOFFSET/SCISSOR, sceGsPutDrawEnv) e uploads que o jogo manda fora da
// cadeia do VIF1. So a imagem de referencia do replay precisa deles; a comparacao numerica nao.
void gif(const uint8_t *data, uint32_t bytes)
{
    if (!g_out || !g_inFrame) return;
    putv(uint8_t('G'));
    putv(bytes);
    put(data, bytes);
}

void end()   // fim do quadro amostrado; o arquivo fica aberto para os proximos quadros da sessao
{
    if (!g_out || !g_inFrame) return;
    putv(uint8_t('E'));
    g_inFrame = false;
    if (g_out && g_perShot) { std::fclose(g_out); g_out = nullptr; }   // foto: arquivo proprio, o proximo comeca com 'A'
    else if (g_out) std::fflush(g_out);   // o processo e morto no fim do lote: o que ja esta no arquivo vale
}
} // namespace snap

// Cada pacote do canal GIF (PATH3) com o tap ligado: vai para o instantaneo e para o inventario.
void gifDmaTap(const uint8_t *data, uint32_t bytes)
{
    snap::gif(data, bytes);
    path3Tap(data, bytes);
}

// Gatilho (WOTM_VU1CHECK_WHEN=<cond>[,<cond>], qualquer uma basta) e rajada (WOTM_VU1CHECK_BURST=<k> quadros seguidos por disparo):
//  - fx:<tipo visual>: algum sistema de particulas vivo (numParticles > 0) com vis.attribs & 0xFF == tipo (3 sprite, 7 arco, 13 destroco...),
//    so a partir do g_frame 30. No quadro em que o sistema nasce o pacote ainda pode nao estar montado: use rajada;
//  - preset:<fxId>: idem, pelo preset do sistema (fxoff::fxId; nomes em tools/fx.py): separa a fumaca do vulcao da de um predio;
//  - frame:<a>[-<b>]: g_frame dentro da faixa.
// O disparo e por borda: a condicao passou de falsa a verdadeira (o evento nasceu). Um emissor permanente (fumaca do vulcao) dispara
// uma vez; uma explosao, a cada explosao nova. O periodo (WOTM_VU1CHECK) e o descanso minimo entre disparos e WOTM_VU1CHECK_SHOTS=<n>
// limita os disparos da sessao (0 = sem limite).
//  - mem:<endereco>[=<valor>]: a palavra (u32) da RAM nesse endereco vale <valor> (dispara ao entrar nela); sem <valor>, dispara a cada
//    mudanca da palavra. Com currScreen (0x6F8464): mem:0x6F8464 grava um quadro de cada tela do menu; mem:0x6F8464=1 so a de jogo.
//  - A tecla F8 na janela do runtime grava um quadro na hora, sem condicao nem descanso.
struct Cond { enum Kind { Fx, Preset, Frame, Mem } kind; uint32_t a, b; bool anyChange = false; uint32_t prev = 0; bool seen = false; };
std::vector<Cond> g_when;
uint32_t g_burst = 1, g_burstLeft = 0, g_shots = 0, g_shotsDone = 0;
bool g_condPrev = false;
bool g_keyOnly = false;   // fotos (WOTM_FOTOS): so o F8 dispara, sem periodo nem condicoes
bool g_keyShot = false;   // o quadro amostrado em andamento foi pedido pelo F8

// Condicoes de nivel (fx, preset, frame, mem:=valor): verdadeiro enquanto valem; o disparo e na borda de subida (fireNow). As mem: sem
// valor sao de mudanca: `changed` sai verdadeiro na avaliacao em que a palavra mudou, e isso ja e a borda (duas mudancas em avaliacoes
// seguidas disparam duas vezes).
bool condTrue(const uint8_t *rdram, uint32_t frame, bool &changed)
{
    const wotm::Ram ram{rdram, PS2_RAM_SIZE};
    bool any = false;   // todas sao avaliadas (as mem: sem valor guardam o ultimo visto)
    changed = false;
    for (Cond &c : g_when) {
        if (c.kind == Cond::Frame) { any = any || (frame >= c.a && frame <= c.b); continue; }
        if (c.kind == Cond::Mem) {
            if (!ram.ok(c.a, 4)) continue;
            const uint32_t v = ram.u32(c.a);
            if (c.anyChange) { changed = changed || (c.seen && v != c.prev); c.prev = v; c.seen = true; }
            else any = any || v == c.b;
            continue;
        }
        if (frame < 30) continue;   // como a amostragem periodica: no comeco da fase o fx[] tem sistema vivo sem nada desenhado (arco em g_frame 8)
        for (uint32_t i = 1; i < wotm::fxaddr::count && !any; ++i) {
            const uint32_t b = wotm::fxaddr::fx + i * wotm::fxaddr::stride;
            if (int32_t(ram.u32(b + wotm::fxoff::numParticles)) <= 0) continue;
            if (!(ram.u32(b + wotm::fxoff::infoAttribs) & 0x400)) continue;   // so com o pacote montado (vivo mas fora da vista nao desenha)
            if (c.kind == Cond::Fx ? (ram.u32(b + wotm::fxoff::type) & 0xFF) == c.a : ram.u32(b + wotm::fxoff::fxId) == c.a) any = true;
        }
    }
    return any;
}

// Verdadeiro so no quadro em que alguma condicao passa a valer (borda de subida). Sem condicoes: sempre.
bool fireNow(const uint8_t *rdram, uint32_t frame)
{
    if (g_when.empty()) return true;
    bool changed = false;
    const bool now = condTrue(rdram, frame, changed);
    const bool fire = changed || (now && !g_condPrev);
    g_condPrev = now;
    return fire;
}

void parseWhen(const char *s)
{
    for (const char *p = s; p && *p;) {
        const char *e = std::strchr(p, ',');
        const std::string tok(p, e ? size_t(e - p) : std::strlen(p));
        p = e ? e + 1 : nullptr;
        if (tok.rfind("fx:", 0) == 0) g_when.push_back({Cond::Fx, uint32_t(std::strtoul(tok.c_str() + 3, nullptr, 0)), 0});
        else if (tok.rfind("preset:", 0) == 0) g_when.push_back({Cond::Preset, uint32_t(std::strtoul(tok.c_str() + 7, nullptr, 0)), 0});
        else if (tok.rfind("frame:", 0) == 0) {
            char *dash = nullptr;
            const uint32_t a = uint32_t(std::strtoul(tok.c_str() + 6, &dash, 0));
            g_when.push_back({Cond::Frame, a, dash && *dash == '-' ? uint32_t(std::strtoul(dash + 1, nullptr, 0)) : a});
        } else if (tok.rfind("mem:", 0) == 0) {
            char *eq = nullptr;
            const uint32_t a = uint32_t(std::strtoul(tok.c_str() + 4, &eq, 0));
            Cond c{Cond::Mem, a, 0};
            if (eq && *eq == '=') c.b = uint32_t(std::strtoul(eq + 1, nullptr, 0));
            else c.anyChange = true;
            g_when.push_back(c);
        } else std::fprintf(stderr, "[vu1check] condicao desconhecida em WOTM_VU1CHECK_WHEN: %s\n", tok.c_str());
    }
}

// Qualquer chamador de hier(0, 0) marca um quadro (rtMain na luta; userintMain, ShellRtLoop e FadeScreen nos menus e no carregamento):
// o quadro amostrado vai da entrada de um hier(0, 0) a do seguinte, com ou sem passe 1 no meio (os menus podem nao ter).
uint32_t g_hierCalls = 0;   // chamadas de hier(0, 0) desde o ultimo quadro amostrado (diagnostico do gatilho)
void onHier(const uint8_t *rdram, uint32_t view, uint32_t pass, uint32_t ra)
{
    if (view != 0) return;
    uint32_t frame;
    std::memcpy(&frame, rdram + wotm::sym::g_frame, 4);   // g_frame (so anda na luta; nos menus fica parado)
    if (pass == 1) {
        if (g_armed) { snap::pass1(rdram); g_pass = 1; g_cam[1] = readCam(rdram); collectNative(rdram, g_natPass1); }
        return;
    }
    ++g_hierCalls;
    if (g_armed) {   // quadro amostrado completo (passes e cadeias da interrupcao)
        uint32_t inPkt = 0, inAlpha = 0;   // pacotes ainda nao despachados ao VU1 (a OT de alfa sai pela interrupcao do DMA; main.cpp a forca na luta)
        std::memcpy(&inPkt, rdram + wotm::sym::objsInPacket, 4);
        std::memcpy(&inAlpha, rdram + wotm::sym::objsInAlphaPacket, 4);
        std::fprintf(stderr, "[vu1check] fim do quadro amostrado: hier(0,0) de %x, passe 1 %s, %u cadeias, objsInPacket %u objsInAlphaPacket %u\n", ra,
                     g_pass == 1 ? "sim" : "nao", g_chains, inPkt, inAlpha);
        snap::end();
        finish();
        if (g_keyShot && wotmFotosOn()) {   // foto: para aqui, com o quadro fotografado na tela, para a nota
            g_keyShot = false;
            wotmFotoDone(rdram, g_frame, snap::g_file);
            wotmPauseBlock(snap::g_rt);
        }
        g_keyShot = false;
    }
    const auto now = std::chrono::steady_clock::now();
    const bool key = g_wotmShotKey.exchange(false);   // F8: na hora, sem condicao, descanso nem limite de disparos
    // avaliado todo quadro, para a borda nao se perder no descanso; sem condicoes (periodo), so na luta (g_frame >= 30)
    const bool fire = key || (!g_keyOnly && (g_when.empty() ? frame >= 30 : fireNow(rdram, frame)));
    if (!g_burstLeft && fire && (key || (now >= g_next && (!g_shots || g_shotsDone < g_shots)))) { g_burstLeft = g_burst; ++g_shotsDone; }
    if (g_burstLeft) {
        --g_burstLeft;
        g_next = now + std::chrono::seconds(g_period);
        uint32_t scr = 0;
        std::memcpy(&scr, rdram + wotm::sym::currScreen, 4);
        std::fprintf(stderr, "[vu1check] quadro amostrado: g_frame %u, currScreen %u, hier(0,0) de %x (%u chamadas desde o anterior)%s\n", frame, scr, ra,
                     g_hierCalls, key ? ", pela tecla F8" : "");
        g_hierCalls = 0;
        g_keyShot = key;
        arm(rdram, frame);
        snap::begin(rdram, frame);
    }
}

PS2Runtime::RecompiledFunction g_origHier = nullptr;
void hierChecked(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
{
    if (ctx->pc == wotm::fn::hier__Fii)
        onHier(rdram, getRegU32(ctx, 4), getRegU32(ctx, 5), getRegU32(ctx, 31));
    g_origHier(rdram, ctx, runtime);
}
} // namespace wotm_vu1check

void wotmVu1CheckInstall(PS2Runtime &runtime)
{
    // WOTM_FOTOS=<pasta> (ou 1): so o F8 dispara, e cada disparo e uma foto (wotm_foto.inc) com instantaneo proprio. Ganha do
    // WOTM_VU1CHECK (periodo, condicoes e WOTM_VU1CHECK_SNAP ficam de fora).
    const char *e = std::getenv("WOTM_VU1CHECK"), *fotos = std::getenv("WOTM_FOTOS");
    if (fotos && !*fotos) fotos = nullptr;
    if (!fotos && (!e || std::atoi(e) <= 0)) return;
    if (!std::getenv("PS2X_NO_VU1")) {   // o hierFlushObjQ do main.cpp (so com PS2X_NO_VU1) e quem libera o DMA no quadro amostrado
        std::fprintf(stderr, "[vu1check] desligado: so no modo nativo (PS2X_NO_VU1)\n");
        return;
    }
    if (!(wotm_vu1check::g_origHier = runtime.lookupFunction(wotm::fn::hier__Fii))) return;
    if (fotos && !wotmFotoInit(fotos)) fotos = nullptr;
    if (fotos) {
        wotm_vu1check::g_keyOnly = true;
        wotm_vu1check::snap::g_perShot = true;
    } else if (const char *sp = std::getenv("WOTM_VU1CHECK_SNAP")) wotm_vu1check::snap::g_prefix = sp;
    if (!fotos && (!e || std::atoi(e) <= 0)) return;
    wotm_vu1check::snap::g_rt = &runtime;
    g_wotmGifDmaTap = &wotm_vu1check::gifDmaTap;
    if ((fotos || std::getenv("WOTM_VU1CHECK_SNAP")) && !std::getenv("PS2X_GS_UPLOADS")) {
        // A VRAM do instantaneo precisa das texturas e CLUTs: com PS2X_NO_GS o GS so processa os uploads (sem rasterizar) quando
        // PS2X_GS_UPLOADS esta ligado, e o VIF1 deixa passar os pacotes do PATH2 para isso. Lido pelo GS/VIF1 na primeira vez que rodam,
        // ainda depois daqui.
#ifdef _WIN32
        _putenv_s("PS2X_GS_UPLOADS", "1");
#else
        setenv("PS2X_GS_UPLOADS", "1", 1);
#endif
        std::fprintf(stderr, "[vu1check] instantaneo ligado: uploads de textura vao ao GS (PS2X_GS_UPLOADS=1) para a VRAM do arquivo\n");
    }
    wotm_vu1check::g_period = e ? std::max(1, std::atoi(e)) : 1;
    if (const char *w = std::getenv("WOTM_VU1CHECK_WHEN"); w && !fotos) wotm_vu1check::parseWhen(w);
    if (const char *b = std::getenv("WOTM_VU1CHECK_BURST")) wotm_vu1check::g_burst = std::max(1u, uint32_t(std::strtoul(b, nullptr, 10)));
    if (const char *n = std::getenv("WOTM_VU1CHECK_SHOTS")) wotm_vu1check::g_shots = uint32_t(std::strtoul(n, nullptr, 10));
    if (const char *m = std::getenv("WOTM_VU1CHECK_SNAP_MAX")) wotm_vu1check::snap::g_max = uint64_t(std::strtoull(m, nullptr, 10)) << 20;
    if (!wotm_vu1check::g_when.empty() || wotm_vu1check::g_burst > 1 || wotm_vu1check::g_shots)
        std::fprintf(stderr, "[vu1check] gatilho: %s (por borda), rajada de %u quadro(s), ate %u disparo(s)\n",
                     std::getenv("WOTM_VU1CHECK_WHEN") ? std::getenv("WOTM_VU1CHECK_WHEN") : "periodo", wotm_vu1check::g_burst, wotm_vu1check::g_shots);
    wotm_vu1check::g_next = std::chrono::steady_clock::now() + std::chrono::seconds(wotm_vu1check::g_period);
    g_wotmVif1Keep = true;
    g_wotmGifTap = &wotm_vu1check::gifTap;
    g_wotmMscalTap = &wotm_vu1check::mscalTap;
    g_wotmChainTap = &wotm_vu1check::chainTap;
    runtime.registerFunction(wotm::fn::hier__Fii, &wotm_vu1check::hierChecked);
    if (fotos) std::fprintf(stderr, "[vu1check] so pelo F8 (fotos)\n");
    else std::fprintf(stderr, "[vu1check] comparando VU1 x nativo a cada %d s\n", wotm_vu1check::g_period);
}

// `ps2EntryRunner --vu1cmp <arquivo.vu1snap> [--dump <prefixo>]`: refaz a checagem de um instantaneo (WOTM_VU1CHECK_SNAP) sem jogo.
// Abre o runtime so pela memoria, VIF1 e VU1 (a janela do raylib vem junto no initialize), restaura o estado do registro 'A' e
// reaplica cada cadeia na ordem gravada, com a mesma coleta nativa e a mesma comparacao da checagem ao vivo.
// Imagens do quadro, ao lado do instantaneo: <base>_<g_frame>_ref.png (o GS em software do runtime, alimentado pelos mesmos pacotes do
// VU1 e do PATH2: e uma aproximacao do GS do PS2, nao o GS), _nat.png (a camada nativa, pela mesma funcao do jogo ao vivo, sem o
// contador de FPS), _diff.png (pixels que diferem em mais de 24 em algum canal, em vermelho sobre a referencia escurecida) e _trio.png
// (os tres lado a lado, 640x448 cada). Guia para o olho; o criterio continua sendo a tabela numerica.
// ---- Atribuicao por pixel (replay com imagens): quem desenhou cada pixel que difere, de cada lado (linhas "[vu1check] inv px") ----
// Um mapa de dono por pixel da tela do GS (640x224) por lado, preenchido na ordem de desenho (o ultimo ganha; sem Z, aproximacao):
// referencia = todos os triangulos e SPRITE do XGKICK no buffer de tela (inteiros ou nao) e os primitivos do PATH2/3; nativo = os
// triangulos projetados dos objetos da 1a coleta (camera do passe 0; ceu/icones marcados), os quadrados das particulas, o HUD por cima e o texto. Cada pixel que
// difere vira um par (dono na referencia, dono no nativo), contado; "nenhum" = fundo, limpeza ou framebuffer de antes.
namespace wotm_vu1check
{
struct PxOwner { uint8_t kind; uint32_t a, b; };   // 0 nenhum; 1 objeto (pkt, tex); 2 HUD (id); 3 fx (sistema, tipo); 4 MSCAL sem dono (origem); 5 path (caminho, tipo)
struct PxMap {
    static constexpr int W = 640, H = 224;
    std::vector<uint16_t> id = std::vector<uint16_t>(size_t(W) * H, 0);
    std::vector<PxOwner> owners{{0, 0, 0}};
    std::map<std::tuple<uint8_t, uint32_t, uint32_t>, uint16_t> index;
    uint16_t owner(uint8_t kind, uint32_t a, uint32_t b) {
        auto it = index.find({kind, a, b});
        if (it != index.end()) return it->second;
        if (owners.size() >= 65535) return 0;
        owners.push_back({kind, a, b});
        return index[{kind, a, b}] = uint16_t(owners.size() - 1);
    }
    void tri(uint16_t o, const float *x, const float *y) {   // coordenadas do GS (2048 no centro)
        float px[3], py[3];
        for (int k = 0; k < 3; ++k) { px[k] = x[k] - kX0; py[k] = y[k] - kY0; }
        const int x0 = std::max(0, int(std::floor(std::min({px[0], px[1], px[2]})))), x1 = std::min(W - 1, int(std::ceil(std::max({px[0], px[1], px[2]}))));
        const int y0 = std::max(0, int(std::floor(std::min({py[0], py[1], py[2]})))), y1 = std::min(H - 1, int(std::ceil(std::max({py[0], py[1], py[2]}))));
        if (x0 > x1 || y0 > y1) return;
        const float area = (px[1] - px[0]) * (py[2] - py[0]) - (px[2] - px[0]) * (py[1] - py[0]);
        if (std::fabs(area) < 1e-6f) return;
        for (int yy = y0; yy <= y1; ++yy)
            for (int xx = x0; xx <= x1; ++xx) {
                const float cx = xx + 0.5f, cy = yy + 0.5f;
                const float w0 = ((px[1] - cx) * (py[2] - cy) - (px[2] - cx) * (py[1] - cy)) / area;
                const float w1 = ((px[2] - cx) * (py[0] - cy) - (px[0] - cx) * (py[2] - cy)) / area;
                const float w2 = 1.f - w0 - w1;
                if (w0 >= 0.f && w1 >= 0.f && w2 >= 0.f) id[size_t(yy) * W + xx] = o;
            }
    }
    void rect(uint16_t o, float ax, float ay, float bx, float by) {
        const int x0 = std::max(0, int(std::floor(std::min(ax, bx) - kX0))), x1 = std::min(W - 1, int(std::ceil(std::max(ax, bx) - kX0)) - 1);
        const int y0 = std::max(0, int(std::floor(std::min(ay, by) - kY0))), y1 = std::min(H - 1, int(std::ceil(std::max(ay, by) - kY0)) - 1);
        for (int yy = y0; yy <= y1; ++yy)
            for (int xx = x0; xx <= x1; ++xx) id[size_t(yy) * W + xx] = o;
    }
};

inline bool screenFbp(uint16_t fbp) { return fbp == 0xFFFF || fbp == 0x46 || fbp == 0x8C; }

// Dono de um programa do VU1 (como a comparacao: objeto nativo pelo polyPkt, senao fx[], senao a origem do MSCAL)
PxOwner ownerOfProg(uint32_t prog)
{
    const uint32_t chain = g_progChain[prog - 1];
    if (chain < g_nats.size())
        if (const uint32_t key = objectOf(g_nats[chain], prog)) {
            const NatObj &o = g_nats[chain].at(key);
            if (o.hudId) return {2, o.hudId, 0};
            return {1, key, o.tex};
        }
    if (const int sys = fxOf(prog); sys >= 0) {
        uint32_t type = 0;
        if (!g_fxNats.empty()) { const auto it = g_fxNats[0].find(uint32_t(sys)); if (it != g_fxNats[0].end()) type = it->second.type; }
        return {3, uint32_t(sys), type};
    }
    return {4, g_progs[prog - 1].empty() ? 0u : g_progs[prog - 1].back() & ~0xFFu, 0};
}

void ownerName(const PxOwner &o, char *s, size_t n)
{
    char sym[96];
    switch (o.kind) {
    case 1: std::snprintf(s, n, "pkt %x tex %u", o.a, o.b); break;
    case 2: std::snprintf(s, n, "HUD %x", o.a); break;
    case 3: std::snprintf(s, n, "fx %u tipo %u", o.a, o.b); break;
    case 4: symOf(o.a, sym, sizeof sym); std::snprintf(s, n, "vu1 mscal %x%s%s%s", o.a, sym[0] ? " (" : "", sym, sym[0] ? ")" : ""); break;
    case 5: std::snprintf(s, n, "path%u %s", o.a, o.b == 6 ? "sprites" : "tris"); break;
    case 6: std::snprintf(s, n, "ceu/icone pkt %x tex %u", o.a, o.b); break;
    case 7: std::snprintf(s, n, "texto (fontPacket)"); break;
    case 8: std::snprintf(s, n, "HUD ScreenPolys"); break;
    default: std::snprintf(s, n, "nenhum"); break;
    }
}

// diff: imagem W x H (640 x 448) com d[0] == 255 nos pixels que diferem (vu1cmpImages). Imprime os pares de donos mais frequentes.
void attributePixels(const Image &diff, int W, int H)
{
    if (g_nats.empty()) return;
    PxMap ref, nat;
    // PATH3 primeiro: a limpeza do canal GIF vem antes das cadeias no PS2 (sem VU1 fica gravada depois; ver o replay dos 'G')
    for (const RefPrim &r : g_ref23)
        if (r.path == 3 && screenFbp(r.fbp)) ref.rect(ref.owner(5, r.path, r.kind == 6 ? 6 : 3), r.x0, r.y0, r.x1, r.y1);   // caixa (os triangulos do PATH2/3 sao raros)
    for (const PxTri &t : g_vuAll)
        if (screenFbp(t.fbp)) { const PxOwner o = ownerOfProg(t.prog); ref.tri(ref.owner(o.kind, o.a, o.b), t.x, t.y); }
    for (size_t i = 0; i < g_vuRects.size(); ++i) {
        const PxOwner o = ownerOfProg(g_vuRects[i].first);
        const Quad &q = g_vuRects[i].second;
        ref.rect(ref.owner(o.kind, o.a, o.b), q.x[0], q.y[0], q.x[2], q.y[2]);
    }
    for (const RefPrim &r : g_ref23)
        if (r.path == 2 && screenFbp(r.fbp)) ref.rect(ref.owner(5, r.path, r.kind == 6 ? 6 : 3), r.x0, r.y0, r.x1, r.y1);
    for (const auto &kv : g_nats[0]) {
        const NatObj &o = kv.second;
        if (o.hudId || o.tris.empty()) continue;   // HUD: depois das particulas (abaixo); ceu/icones entram (kind 6; o ceu nativo e centrado na camera, posicao aproximada)
        const uint16_t id = nat.owner(o.skip ? 6 : 1, kv.first, o.tex);
        for (const Tri &t : project(o.tris, g_cam[0], kv.first)) nat.tri(id, t.x, t.y);
    }
    if (!g_fxNats.empty())
        for (const auto &kv : g_fxNats[0]) {
            const uint16_t id = nat.owner(3, kv.first, kv.second.type);
            for (const Quad &q : kv.second.quads[1]) { nat.tri(id, q.x, q.y); const float x2[3] = {q.x[0], q.x[2], q.x[3]}, y2[3] = {q.y[0], q.y[2], q.y[3]}; nat.tri(id, x2, y2); }
        }
    for (const auto &kv : g_nats[0])   // HUD: o nativo o desenha por ultimo na cena 3D, sem profundidade
        if (kv.second.hudId) {
            const uint16_t id = nat.owner(2, kv.second.hudId, 0);
            for (const Tri &t : project(kv.second.tris, g_cam[0], kv.first)) nat.tri(id, t.x, t.y);
        }
    if (g_rdram) {   // HUD do ScreenPolys do nativo (readScreenPolys, como o takeSnapshot do replay), antes do texto como no rtMain
        std::vector<wotm::TextQuad> polys;
        wotm::readScreenPolys(wotm::Ram{g_rdram, PS2_RAM_SIZE}, polys);
        const uint16_t id = nat.owner(8, 0, 0);
        for (const wotm::TextQuad &t : polys) nat.rect(id, t.x0 + kX0, t.y0 + kY0, t.x1 + kX0, t.y1 + kY0);
    }
    if (g_scene && g_rdram) {   // texto do nativo (fontPacket, Scene::updateText): glifos em pixels da tela 640x224
        g_scene->updateText(wotm::Ram{g_rdram, PS2_RAM_SIZE});
        const uint16_t id = nat.owner(7, 0, 0);
        for (const wotm::TextQuad &t : g_scene->text) nat.rect(id, t.x0 + kX0, t.y0 + kY0, t.x1 + kX0, t.y1 + kY0);
    }
    std::map<std::pair<uint16_t, uint16_t>, uint32_t> pairs;
    uint32_t total = 0, refOnly = 0, natOnly = 0, none = 0, both = 0;
    const uint8_t *d = static_cast<const uint8_t *>(diff.data);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x, d += 4) {
            if (d[0] != 255) continue;
            const int gx = x * PxMap::W / W, gy = y * PxMap::H / H;
            const uint16_t r = ref.id[size_t(gy) * PxMap::W + gx], n = nat.id[size_t(gy) * PxMap::W + gx];
            ++total;
            ++pairs[{r, n}];
            if (r && !n) ++refOnly; else if (!r && n) ++natOnly; else if (!r && !n) ++none; else ++both;
        }
    if (!total) return;
    std::fprintf(stderr, "[vu1check] inv px g=%u: %u pixels diferentes: so a referencia pinta %u, so o nativo pinta %u, os dois %u, nenhum %u (fundo/limpeza/framebuffer de antes)\n",
                 g_frame, total, refOnly, natOnly, both, none);
    std::vector<std::pair<std::pair<uint16_t, uint16_t>, uint32_t>> v(pairs.begin(), pairs.end());
    std::sort(v.begin(), v.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    char a[160], b[160];
    for (size_t i = 0; i < v.size() && i < 14; ++i) {
        ownerName(ref.owners[v[i].first.first], a, sizeof a);
        ownerName(nat.owners[v[i].first.second], b, sizeof b);
        std::fprintf(stderr, "[vu1check] inv px   ref %s | nat %s: %u px (%.1f%%)\n", a, b, v[i].second, 100.0 * v[i].second / total);
    }
}
} // namespace wotm_vu1check

static uint16_t g_frameFbp = 0xFFFF;   // buffer de tela em que o quadro desenha (deduzido no aquecimento; 0xFFFF = usa o FRAME dos triangulos)
static void vu1cmpImages(const char *snapPath, uint32_t frame, PS2Runtime &runtime, const uint8_t *ram)
{
    constexpr int W = 640, H = 448;
    std::string base(snapPath);
    if (base.size() > 8 && base.compare(base.size() - 8, 8, ".vu1snap") == 0) base.resize(base.size() - 8);
    base += "_" + std::to_string(frame);

    Image ref{};
    {
        std::vector<uint8_t> px;
        uint32_t w = 0, h = 0, dfbp = 0, sfbp = 0;
        bool pref = false;
        // O DISPFB do instantaneo mostra o buffer do quadro anterior (duplo buffer; o jogo so o troca no vsync, depois do quadro). O que
        // interessa e o buffer em que este quadro desenhou: o FRAME mais usado pelos triangulos do VU1 na tela.
        {
            std::map<uint16_t, uint32_t> byFbp;
            for (const wotm_vu1check::Tri &t : wotm_vu1check::g_vu) if (t.fbp != 0xFFFF) ++byFbp[t.fbp];
            uint16_t best = 0xFFFF;
            uint32_t n = 0;
            for (const auto &kv : byFbp) if (kv.second > n) { n = kv.second; best = kv.first; }
            if (g_frameFbp != 0xFFFF) best = g_frameFbp;
            if (best != 0xFFFF) {
                GSRegisters &gr = runtime.memory().gs_regs;
                gr.dispfb1 = (gr.dispfb1 & ~0x1FFull) | best;
                gr.dispfb2 = (gr.dispfb2 & ~0x1FFull) | best;
            }
        }
        runtime.gs().latchHostPresentationFrame();
        const bool got = runtime.gs().copyLatchedHostPresentationFrame(px, w, h, &dfbp, &sfbp, &pref);
        {
            extern std::atomic<unsigned long long> g_wotmGifPackets, g_wotmGifBytes;
            const GSRegisters &gr = runtime.memory().gs_regs;
            std::fprintf(stderr, "[vu1cmp] gs: pacotes %llu (%llu bytes), quadro %ux%u dispfb %x fonte %x preferida %d, pmode %llx dispfb1 %llx dispfb2 %llx\n",
                         g_wotmGifPackets.load(), g_wotmGifBytes.load(), w, h, dfbp, sfbp, int(pref), (unsigned long long)gr.pmode, (unsigned long long)gr.dispfb1, (unsigned long long)gr.dispfb2);
        }
        if (got && w && h && px.size() >= size_t(w) * h * 4) {
            ref = GenImageColor(int(w), int(h), BLACK);
            std::memcpy(ref.data, px.data(), size_t(w) * h * 4);
            ImageResizeNN(&ref, W, H);
        } else {
            ref = GenImageColor(W, H, BLACK);
            std::fprintf(stderr, "[vu1cmp] sem quadro do GS para a imagem de referencia (o DISPFB do instantaneo nao aponta um buffer desenhado?)\n");
        }
    }

    wotm_live::g_hideFps = true;
    wotm_live::g_localCollect = true;   // sem jogo rodando: a cena sai da RAM do instantaneo, nao da captura da thread do EE
    // Lens flare: na entrada do quadro a lista ja esta em coordenadas de mundo (o readFlares a descarta); usa a que o VU1 recebeu
    // (g_flareVuIn) como a captura do jogo ao vivo, com o texId de cada sprite e a profundidade do GS lidos da RAM do quadro
    if (!wotm_vu1check::g_flareVuIn.empty()) {
        wotm::Particles::FlareFrame f = wotm_vu1check::g_flareVuIn.back();
        const wotm::Ram r{const_cast<uint8_t *>(ram), PS2_RAM_SIZE};
        const uint32_t p = r.u32(wotm::sym::lensFlarePacket);
        if (r.ok(p, 0x2EE0))
            for (int k = 0; k < f.nspr; ++k) f.spr[k].texId = r.u16(p + 0x2C40 + 2 * uint32_t(k));
        f.zScale = r.f32(wotm::sym::viewScreenMats + 0x28);
        f.zOff = r.f32(wotm::sym::viewScreenMats + 0x2C);
        auto &slot = wotm::Particles::flareSlot();
        std::lock_guard<std::mutex> g(slot.m);
        slot.f = std::move(f);
        slot.used = true;
    }
    {   // filme do telao do menu (texId 999): sem tocador no replay, o quadro que o tocador deixou na VRAM (CT32 em 0x3BA0, largura
        // 320, 256x128; wotm_fmv.hpp), como o GS da referencia o amostra
        static Texture2D movie{};
        std::vector<uint32_t> px;
        uint32_t mw = 0, mh = 0;
        const uint64_t tex0 = 0x3BA0ull | (5ull << 14) | (8ull << 26) | (7ull << 30);
        if (wotm::gs::decodeTexture({runtime.memory().getGSVRAM(), PS2_GS_VRAM_SIZE}, tex0, px, mw, mh)) {
            if (!movie.id) {
                Image im{px.data(), int(mw), int(mh), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                movie = LoadTextureFromImage(im);
            } else UpdateTexture(movie, px.data());
            wotm_live::g_replayMovieTex = []() { return movie.id; };
        }
    }
    if (const char *tl = std::getenv("WOTM_VU1CMP_VRAMTEX")) {   // diagnostico: <tex0 hex>[,..] grava <instantaneo>_vram_<tex0>.png da VRAM
        for (const char *s = tl; *s;) {
            char *end = nullptr;
            const uint64_t t0 = std::strtoull(s, &end, 16);
            if (end == s) break;
            std::vector<uint32_t> px;
            uint32_t mw = 0, mh = 0;
            if (wotm::gs::decodeTexture({runtime.memory().getGSVRAM(), PS2_GS_VRAM_SIZE}, t0, px, mw, mh)) {
                Image im{px.data(), int(mw), int(mh), 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                char name[1024];
                std::snprintf(name, sizeof name, "%s_vram_%llx.png", snapPath, (unsigned long long)t0);
                ExportImage(im, name);
            } else std::fprintf(stderr, "[vu1cmp] VRAMTEX %llx: decodeTexture falhou\n", (unsigned long long)t0);
            s = *end == ',' ? end + 1 : end;
        }
    }
    wotm_live::g_force43 = true;   // a referencia do GS e 4:3 (e o quadro fica em 960x720, renderHeight); o recorte abaixo supoe isso
    bool natOk = false;
    for (int pass = 0; pass < 2; ++pass) {   // o 1o desenho de uma fase so carrega (recursos, instantaneo A/B); o 2o desenha
        BeginDrawing();
        ClearBackground(BLACK);
        natOk = wotmDrawNative(ram);
        EndDrawing();
    }
    Image nat = LoadImageFromScreen();
    ImageFormat(&nat, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    {   // recorta o letterbox 4:3 do wotmDrawNative (render texture de 960x720 ajustada na janela)
        const float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
        const float k = std::min(sw / 960.f, sh / 720.f);
        ImageCrop(&nat, Rectangle{(sw - 960.f * k) * 0.5f, (sh - 720.f * k) * 0.5f, 960.f * k, 720.f * k});
        ImageResize(&nat, W, H);
    }
    if (!natOk) std::fprintf(stderr, "[vu1cmp] a camada nativa nao desenhou este quadro (cena sem fase?)\n");

    Image diff = GenImageColor(W, H, BLACK);
    uint32_t differ = 0;
    {
        const uint8_t *r = static_cast<const uint8_t *>(ref.data), *n = static_cast<const uint8_t *>(nat.data);
        uint8_t *d = static_cast<uint8_t *>(diff.data);
        for (int i = 0; i < W * H; ++i, r += 4, n += 4, d += 4) {
            int m = 0;
            for (int c = 0; c < 3; ++c) m = std::max(m, std::abs(int(r[c]) - int(n[c])));
            if (m > 24) { ++differ; d[0] = 255; d[1] = uint8_t(255 - m); d[2] = 0; }
            else { const uint8_t g = uint8_t((r[0] + r[1] + r[2]) / 12); d[0] = d[1] = d[2] = g; }
            d[3] = 255;
        }
    }
    wotm_vu1check::attributePixels(diff, W, H);   // linhas "inv px": dono de cada pixel que difere, nos dois lados
    Image trio = GenImageColor(3 * W + 8, H, DARKGRAY);
    const Rectangle src{0.f, 0.f, float(W), float(H)};
    ImageDraw(&trio, ref, src, Rectangle{0.f, 0.f, float(W), float(H)}, WHITE);
    ImageDraw(&trio, nat, src, Rectangle{float(W + 4), 0.f, float(W), float(H)}, WHITE);
    ImageDraw(&trio, diff, src, Rectangle{float(2 * W + 8), 0.f, float(W), float(H)}, WHITE);
    ExportImage(ref, (base + "_ref.png").c_str());
    ExportImage(nat, (base + "_nat.png").c_str());
    ExportImage(diff, (base + "_diff.png").c_str());
    ExportImage(trio, (base + "_trio.png").c_str());
    UnloadImage(ref);
    UnloadImage(nat);
    UnloadImage(diff);
    UnloadImage(trio);
    std::fprintf(stderr, "[vu1cmp] imagens: %s_{ref,nat,diff,trio}.png; pixels diferentes %u (%.1f%%)\n", base.c_str(), differ, 100.0 * differ / (W * H));
}

static int vu1cmpBody(const char *path, const char *dump, uint32_t onlyFrame, bool png)
{
    using namespace wotm_vu1check;
    g_wotmGsFeed = png;   // com imagens, o GS em software do runtime recebe os pacotes (PATH2 e XGKICK) do quadro
    if (dump) {
#ifdef _WIN32
        _putenv_s("WOTM_VU1CHECK_DUMP", dump);
#else
        setenv("WOTM_VU1CHECK_DUMP", dump, 1);
#endif
    }
    FILE *f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "[vu1cmp] nao abri %s\n", path); return 1; }
    auto get = [&](void *p, size_t n) { return std::fread(p, 1, n, f) == n; };
    snap::Hdr h{};
    if (!get(&h, sizeof h) || std::memcmp(h.magic, snap::kMagic, 8) != 0) { std::fprintf(stderr, "[vu1cmp] %s nao e um instantaneo\n", path); return 1; }
    if (h.ramSize != PS2_RAM_SIZE || h.vramSize != PS2_GS_VRAM_SIZE || h.vu1State != sizeof(VU1State) || h.vifRegs != sizeof(VIFRegisters)) {
        std::fprintf(stderr, "[vu1cmp] instantaneo de outro build do runtime (tamanhos do estado diferem)\n");
        return 1;
    }
    std::unique_ptr<PS2Runtime> rtp(new PS2Runtime());
    PS2Runtime &runtime = *rtp;
    if (!runtime.initialize("WotM vu1cmp")) return 1;
    PS2Memory &mem = runtime.memory();
    uint8_t *ram = mem.getRDRAM();
    g_wotmVif1Keep = true;
    g_wotmGifTap = &gifTap;
    g_wotmMscalTap = &mscalTap;
    g_wotmChainTap = &chainTap;
    auto applyDelta = [&](uint8_t *base, size_t size) {   // paginas que mudaram (RAM ou VRAM)
        uint32_t n = 0;
        if (!get(&n, 4)) return false;
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t pg = 0;
            if (!get(&pg, 4) || size_t(pg) * snap::kPage >= size || !get(base + size_t(pg) * snap::kPage, snap::kPage)) return false;
        }
        return true;
    };
    auto getState = [&]() {
        runtime.vu1().reset();
        const bool ok = get(&runtime.vu1().state(), sizeof(VU1State)) && get(&runtime.cpu().vu0_fbrst, 4) && get(&mem.vif1_regs, sizeof(VIFRegisters)) &&
                        get(mem.getVU1Code(), PS2_VU1_CODE_SIZE) && get(mem.getVU1Data(), PS2_VU1_DATA_SIZE) && get(&mem.gs_regs, snap::kGsRegsBytes);
        mem.markVU1CodeModified();
        return ok;
    };
    uint32_t chains = 0, frame = 0, framesDone = 0, framesSeen = 0;
    bool ok = true, inFrame = false, skip = false;   // skip: quadro que nao e o pedido (--frame): so aplica os deltas
    // Pacotes do canal GIF ('G') do quadro: o ambiente de desenho deste quadro foi mandado antes da entrada do hier(0,0), ou seja,
    // na janela do quadro anterior; o que a janela deste traz e o do seguinte, com os mesmos XYOFFSET/SCISSOR/ZBUF. Para a imagem de
    // referencia, os 'G' do quadro sao aplicados ao GS antes das cadeias (lidos adiantado do arquivo) e de novo na ordem gravada.
    std::vector<std::vector<uint8_t>> gifs;
    std::vector<uint8_t> vramRec(PS2_GS_VRAM_SIZE);   // VRAM como gravada no arquivo (os deltas do 'a' valem sobre ela, nao sobre a do replay)
    std::vector<uint8_t> frameRam;   // RAM na entrada do quadro (imagem nativa; a RAM de agora e a do fim do quadro)
#ifdef _WIN32
    auto tell = [&]() { return _ftelli64(f); };
    auto seek = [&](int64_t off, int whence) { return _fseeki64(f, off, whence) == 0; };
#else
    auto tell = [&]() { return int64_t(ftello(f)); };
    auto seek = [&](int64_t off, int whence) { return fseeko(f, off, whence) == 0; };
#endif
    // Aquecimento do GS para a imagem de referencia: o GS em software comeca zerado, e parte do seu estado (registradores que o jogo
    // so manda no fim do quadro, para o quadro seguinte) vem do quadro anterior; sem isso o primeiro quadro do arquivo sai sem o mundo.
    // Roda as cadeias e os pacotes do canal GIF do quadro uma vez so pelo GS (sem os taps da checagem), devolve a VRAM e o VU1/VIF1
    // ao estado de entrada e deixa o resto (registradores do GS) como ficou.
    std::vector<uint8_t> vramIn(PS2_GS_VRAM_SIZE), vu1CodeIn(PS2_VU1_CODE_SIZE), vu1DataIn(PS2_VU1_DATA_SIZE);
    auto warmUp = [&]() {
        gifs.clear();
        const int64_t pos = tell();
        std::memcpy(vramIn.data(), mem.getGSVRAM(), PS2_GS_VRAM_SIZE);
        std::memcpy(vu1CodeIn.data(), mem.getVU1Code(), PS2_VU1_CODE_SIZE);
        std::memcpy(vu1DataIn.data(), mem.getVU1Data(), PS2_VU1_DATA_SIZE);
        const VU1State vuIn = runtime.vu1().state();
        const VIFRegisters vifIn = mem.vif1_regs;
        g_wotmVu1Tap = false;   // sem taps: o XGKICK e o PATH2 vao direto ao GS (g_wotmGsFeed)
        std::vector<uint8_t> data;
        for (;;) {
            uint8_t type = 0;
            if (!get(&type, 1) || type == 'E' || type == 'A' || type == 'a') break;
            uint32_t n = 0, bytes = 0;
            bool more = true;
            auto skipDelta = [&]() { if (!get(&n, 4)) return false; return seek(int64_t(n) * (4 + int64_t(snap::kPage)), SEEK_CUR); };
            if (type == 'C') {
                more = skipDelta() && get(&n, 4) && seek(int64_t(n) * 8, SEEK_CUR) && get(&bytes, 4);
                if (more) { data.resize(bytes); more = get(data.data(), bytes); }
                if (more) mem.processVIF1Data(data.data(), bytes);
            }
            else if (type == 'P') more = skipDelta();
            else if (type == 'G') {
                more = get(&bytes, 4);
                if (more) { gifs.emplace_back(bytes); more = get(gifs.back().data(), bytes); }
                if (more) mem.submitGifPacket(GifPathId::Path3, gifs.back().data(), bytes, true);
            }
            else more = false;
            if (!more) break;
        }
        seek(pos, SEEK_SET);
        std::memcpy(mem.getGSVRAM(), vramIn.data(), PS2_GS_VRAM_SIZE);
        std::memcpy(mem.getVU1Code(), vu1CodeIn.data(), PS2_VU1_CODE_SIZE);
        std::memcpy(mem.getVU1Data(), vu1DataIn.data(), PS2_VU1_DATA_SIZE);
        mem.markVU1CodeModified();
        runtime.vu1().reset();
        runtime.vu1().state() = vuIn;
        mem.vif1_regs = vifIn;
        // A VRAM voltou ao estado de entrada, com o Z do quadro anterior ainda la (a limpeza para este quadro saiu na janela do
        // anterior, que o arquivo nao tem no 1o quadro). Os pacotes do canal GIF deste quadro limpam o Z (compartilhado) para o
        // seguinte: reaplicados agora, deixam o Z limpo para o passe real; o FRAME que eles poem e trocado pela propria cadeia.
        for (const auto &g : gifs) mem.submitGifPacket(GifPathId::Path3, g.data(), uint32_t(g.size()), true);
        // O FRAME em vigor agora e o do ultimo ambiente da janela, o do quadro seguinte. O deste quadro e o primeiro FRAME que os
        // pacotes do canal GIF da janela escrevem (o jogo o reenvia depois do render-to-texture); se so ha o do seguinte, e o outro
        // buffer de tela. Forca FRAME_1/FRAME_2 para ele e e esse buffer que a imagem de referencia mostra.
        uint64_t first = 0, last = 0;
        for (const auto &g : gifs)
            for (uint32_t pos = 0; pos + 16 <= g.size();) {
                uint64_t lo, hi;
                std::memcpy(&lo, g.data() + pos, 8);
                std::memcpy(&hi, g.data() + pos + 8, 8);
                pos += 16;
                const uint32_t nloop = uint32_t(lo & 0x7FFF), flg = uint32_t((lo >> 58) & 3), nreg = uint32_t((lo >> 60) & 0xF) ? uint32_t((lo >> 60) & 0xF) : 16u;
                if (flg == 0) {
                    for (uint32_t l = 0; l < nloop; ++l)
                        for (uint32_t r = 0; r < nreg && pos + 16 <= g.size(); ++r, pos += 16)
                            if (uint32_t((hi >> (4 * r)) & 0xF) == 0xE) {
                                uint64_t v0, v1;
                                std::memcpy(&v0, g.data() + pos, 8);
                                std::memcpy(&v1, g.data() + pos + 8, 8);
                                if ((v1 & 0xFF) == 0x4C) { if (!first) first = v0; last = v0; }
                            }
                } else if (flg == 1) pos += ((nloop * nreg + 1) / 2) * 16;
                else pos += nloop * 16;
            }
        if (last) {
            uint64_t fr = first != last ? first : ((last & ~0x1FFull) | ((last & 0x1FF) == 0x46 ? 0x8Cu : 0x46u));
            runtime.gs().writeRegister(0x4C, fr);
            runtime.gs().writeRegister(0x4D, fr);
            g_frameFbp = uint16_t(fr & 0x1FF);
        } else g_frameFbp = 0xFFFF;
        g_wotmVu1Tap = true;
    };
    for (;;) {
        uint8_t type = 0;
        if (!get(&type, 1)) { ok = !inFrame; break; }   // fim do arquivo entre quadros = normal; no meio de um = truncado
        if (type == 'A' || type == 'a') {
            if (inFrame) { ok = false; break; }
            ok = get(&frame, 4) && getState();
            // A VRAM gravada fica a parte (vramRec): o delta do 'a' so traz as paginas que o jogo mudou, e na captura (sem GS) o
            // framebuffer e o Z quase nao mudam; aplicado direto na VRAM do GS, deixava nelas o que o replay desenhou no quadro anterior,
            // e o Z velho escondia o mundo (imagem de referencia preta a partir do 2o quadro do arquivo; com --frame saia certa).
            if (ok) ok = type == 'A' ? (get(ram, PS2_RAM_SIZE) && get(vramRec.data(), PS2_GS_VRAM_SIZE))
                                     : (applyDelta(ram, PS2_RAM_SIZE) && applyDelta(vramRec.data(), PS2_GS_VRAM_SIZE));
            if (ok) std::memcpy(mem.getGSVRAM(), vramRec.data(), PS2_GS_VRAM_SIZE);
            if (!ok) break;
            inFrame = true;
            ++framesSeen;
            skip = onlyFrame && frame != onlyFrame;
            chains = 0;
            if (!skip) {
                if (png) { warmUp(); frameRam.assign(ram, ram + PS2_RAM_SIZE); }   // a imagem nativa usa a RAM da entrada do quadro
                arm(ram, frame);
            }
            continue;
        }
        if (!inFrame) { ok = false; break; }
        if (type == 'C') {
            uint32_t n = 0, bytes = 0;
            std::vector<std::pair<uint32_t, uint32_t>> segs;
            std::vector<uint8_t> data;
            ok = applyDelta(ram, PS2_RAM_SIZE) && get(&n, 4);
            if (ok) { segs.resize(n); ok = get(segs.data(), n * sizeof segs[0]) && get(&bytes, 4); }
            if (ok) { data.resize(bytes); ok = get(data.data(), bytes); }
            if (!ok) break;
            if (skip) continue;
            ++chains;
            mem.m_vif1ChainSrc = segs.empty() ? nullptr : &segs;
            if (mem.m_vif1ChainSrc) chainTap(segs.data(), segs.size(), data.data(), bytes);
            mem.processVIF1Data(data.data(), bytes);
            mem.m_vif1ChainSrc = nullptr;
        } else if (type == 'P') {
            ok = applyDelta(ram, PS2_RAM_SIZE);
            if (ok && !skip) { g_pass = 1; g_cam[1] = readCam(ram); collectNative(ram, g_natPass1); }
            if (!ok) break;
        } else if (type == 'G') {
            uint32_t bytes = 0;
            std::vector<uint8_t> data;
            ok = get(&bytes, 4);
            if (ok) { data.resize(bytes); ok = get(data.data(), bytes); }
            if (!ok) break;
            static const bool s_listG = std::getenv("WOTM_VU1CMP_LISTG") != nullptr;   // diagnostico: lista os registradores de cada pacote do canal GIF
            if (s_listG && !skip) {   // tags GIF do pacote: NLOOP, formato e os registradores A+D (com o valor)
                std::string s;
                char b[96];
                for (uint32_t pos = 0; pos + 16 <= bytes;) {
                    uint64_t lo, hi;
                    std::memcpy(&lo, data.data() + pos, 8);
                    std::memcpy(&hi, data.data() + pos + 8, 8);
                    pos += 16;
                    const uint32_t nloop = uint32_t(lo & 0x7FFF), flg = uint32_t((lo >> 58) & 3), nreg = uint32_t((lo >> 60) & 0xF) ? uint32_t((lo >> 60) & 0xF) : 16u;
                    std::snprintf(b, sizeof b, " [nloop %u flg %u nreg %u%s]", nloop, flg, nreg, (lo >> 15) & 1 ? " eop" : "");
                    s += b;
                    if (flg == 0) {
                        for (uint32_t l = 0; l < nloop; ++l)
                            for (uint32_t r = 0; r < nreg && pos + 16 <= bytes; ++r, pos += 16) {
                                const uint32_t reg = uint32_t((hi >> (4 * r)) & 0xF);
                                uint64_t v0, v1;
                                std::memcpy(&v0, data.data() + pos, 8);
                                std::memcpy(&v1, data.data() + pos + 8, 8);
                                if (reg == 0xE) std::snprintf(b, sizeof b, " %02llx=%llx", (unsigned long long)(v1 & 0xFF), (unsigned long long)v0);
                                else std::snprintf(b, sizeof b, " r%x", reg);
                                s += b;
                            }
                    } else if (flg == 1) pos += ((nloop * nreg + 1) / 2) * 16;
                    else pos += nloop * 16;
                }
                std::fprintf(stderr, "[vu1cmp] G %u bytes (depois de %u cadeias):%s\n", bytes, chains, s.c_str());
            }
            if (!skip) path3Tap(data.data(), bytes);   // inventario (sem origem na RAM: o arquivo guarda so os bytes)
            // Reaplicados na ordem gravada (alem do aquecimento) so os pacotes sem vertices (ambiente de desenho: XYOFFSET, SCISSOR,
            // FRAME...), de que a luta precisa. Os que desenham (a limpeza do quadro seguinte, dois SPRITE de tela inteira) ficam de fora:
            // sem VU1 a cadeia do VIF1 so e despachada tarde, entao eles ficam gravados depois das cadeias e, no GS, apagavam o buffer
            // que elas acabaram de desenhar (nos menus a imagem saia preta; no PS2 a limpeza vem antes). WOTM_VU1CMP_GORDER=1 reaplica
            // todos (diagnostico).
            static const bool s_gOrder = std::getenv("WOTM_VU1CMP_GORDER") && std::atoi(std::getenv("WOTM_VU1CMP_GORDER")) > 0;
            auto hasKick = [&]() {   // algum XYZ2/XYZF2 (PACKED ou REGLIST) no pacote
                for (uint32_t pos = 0; pos + 16 <= bytes;) {
                    uint64_t lo, hi;
                    std::memcpy(&lo, data.data() + pos, 8);
                    std::memcpy(&hi, data.data() + pos + 8, 8);
                    pos += 16;
                    const uint32_t nloop = uint32_t(lo & 0x7FFF), flg = uint32_t((lo >> 58) & 3), nreg = uint32_t((lo >> 60) & 0xF) ? uint32_t((lo >> 60) & 0xF) : 16u;
                    if (flg == 0 || flg == 1) {
                        for (uint32_t r = 0; r < nreg; ++r) {
                            const uint32_t reg = uint32_t((hi >> (4 * r)) & 0xF);
                            if (nloop && (reg == 0x4 || reg == 0x5)) return true;
                        }
                        pos += flg == 0 ? nloop * nreg * 16 : ((nloop * nreg + 1) / 2) * 16;
                    } else pos += nloop * 16;
                    if ((lo >> 15) & 1) break;
                }
                return false;
            };
            if (!skip && png && (s_gOrder || !hasKick())) mem.submitGifPacket(GifPathId::Path3, data.data(), bytes, true);
        } else if (type == 'E') {
            inFrame = false;
            if (skip) continue;
            std::fprintf(stderr, "[vu1cmp] quadro g_frame %u (%u cadeias, %zu pacotes do canal GIF)\n", frame, chains, gifs.size());
            finish();
            ++framesDone;
            if (png) vu1cmpImages(path, frame, runtime, frameRam.data());
        } else { ok = false; break; }
    }
    std::fclose(f);
    if (!ok) std::fprintf(stderr, "[vu1cmp] instantaneo truncado ou corrompido no quadro %u (%u cadeias lidas); %u quadro(s) inteiros antes\n", frame, chains, framesDone);
    if (onlyFrame && !framesDone) std::fprintf(stderr, "[vu1cmp] o quadro %u nao esta no arquivo (%u quadros)\n", onlyFrame, framesSeen);
    std::fprintf(stderr, "[vu1cmp] %s: %u quadro(s) comparados\n", path, framesDone);
    return framesDone ? 0 : 1;
}

// A checagem ao vivo roda na thread do EE; a thread principal tem 1 MB de pilha e estoura (coleta, VU1, comparacao). O corpo roda
// numa thread com pilha grande. Sem windows.h (briga com os nomes do raylib): so os dois simbolos do kernel32 que faltam.
#ifdef _WIN32
#include <process.h>
extern "C" __declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void *handle, unsigned long ms);
extern "C" __declspec(dllimport) int __stdcall CloseHandle(void *handle);
#else
#define __stdcall
#endif
struct Vu1CmpJob { const char *path, *dump; uint32_t frame; bool png; int rc; };
static unsigned __stdcall vu1cmpThread(void *p)
{
    Vu1CmpJob *j = static_cast<Vu1CmpJob *>(p);
    j->rc = vu1cmpBody(j->path, j->dump, j->frame, j->png);
    return 0;
}

int wotmVu1CmpMain(int argc, char **argv)
{
    Vu1CmpJob job{nullptr, nullptr, 0, true, 1};
    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) job.dump = argv[++i];
        else if (!std::strcmp(argv[i], "--frame") && i + 1 < argc) job.frame = uint32_t(std::strtoul(argv[++i], nullptr, 10));
        else if (!std::strcmp(argv[i], "--nopng")) job.png = false;
        else job.path = argv[i];
    }
    if (!job.path) { std::fprintf(stderr, "uso: ps2EntryRunner --vu1cmp <arquivo.vu1snap> [--frame <g_frame>] [--dump <prefixo>] [--nopng]\n"); return 2; }
#ifdef _WIN32
    const uintptr_t h = _beginthreadex(nullptr, 512u << 20, &vu1cmpThread, &job, 0, nullptr);
    if (!h) return vu1cmpBody(job.path, job.dump, job.frame, job.png);
    WaitForSingleObject(reinterpret_cast<void *>(h), 0xFFFFFFFFul);
    CloseHandle(reinterpret_cast<void *>(h));
#else
    vu1cmpThread(&job);
#endif
    return job.rc;
}
#else
void wotmVu1CheckInstall(PS2Runtime &) {}
int wotmVu1CmpMain(int, char **) { std::fprintf(stderr, "runtime sem a camada nativa (WOTM_HAVE_NATIVE): sem --vu1cmp\n"); return 1; }
#endif
