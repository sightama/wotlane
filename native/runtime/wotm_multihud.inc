// wotm_multihud.inc: placar nativo da luta de 5 a 8 jogadores (camera compartilhada, wotm_split4.inc). Incluido pelo wotm_live.inc
// depois do wotm_padsel.inc (usa a fonte do jogo dele).
//
// Com 5 a 8 jogadores a tela e uma vista so e o Hud 0 do jogo mostra so o jogador 1: aqui, em cima a direita da area 4:3, um
// quadro por jogador (cor do jogador, "P<n>", nome do monstro, barra de vida e vitorias), lida direto da RAM do jogo na janela
// (m_monsters[i]: m_health +0x44C, m_maxHealth +0x448, m_wins +0x3C, tipo +0x14 >> 5, m_dead +0xE8). Fora da luta de 5 a 8
// (wotmSharedHudPlayers() 0) e durante os cinemas nao desenha nada.

int wotmSharedHudPlayers();   // wotm_split4.inc

namespace wotm_multihud
{
constexpr uint32_t kMonsters = wotm::sym::shellGame + 0x120380;

// Dentro do BeginTextureMode do quadro (rw x rh), depois do texto do jogo. `tex0` = atlas da fonte do quadro (0: sem texto).
static void draw(wotm::Scene &scene, const wotm::Ram &ram, int rw, int rh, uint64_t tex0)
{
    const int n = wotmSharedHudPlayers();
    if (n < 5 || ram.u32(wotm::sym::moviePlaying))
        return;
    // em cima, a direita do retrato e das barras do Hud 0 (jogador 1, ate ~45% da largura 4:3), em duas linhas de 4; embaixo fica o
    // "WINS" do Hud 0
    const wotm::GsScreenRect gr = wotm::gsScreenRect(float(rw), float(rh));
    const float H = float(rh), pad = gr.w * 0.006f, x0 = gr.x0 + gr.w * 0.46f, areaW = gr.x0 + gr.w - pad - x0;
    const int cols = (n + 1) / 2;
    const float boxW = (areaW - pad * float(cols - 1)) / float(cols), boxH = H * 0.085f;
    std::vector<wotm::TextQuad> quads;
    for (int i = 0; i < n; ++i)
    {
        const float top = H * 0.02f + float(i / cols) * (boxH + pad);
        const uint32_t m = ram.u32(kMonsters + 4u * uint32_t(i)) & 0x1FFFFFFFu;
        if (!ram.ok(m, 0x450))
            continue;
        const float hp = ram.f32(m + 0x44C), maxHp = ram.f32(m + 0x448);
        const float frac = maxHp > 0.f && std::isfinite(hp) ? std::clamp(hp / maxHp, 0.f, 1.f) : 0.f;
        const bool dead = ram.u8(m + 0xE8) != 0 || hp <= 0.f;
        const uint32_t type = ram.u32(m + 0x14) >> 5;
        const int wins = static_cast<int32_t>(ram.u32(m + 0x3C));
        const Color c = wotm_padsel::kPlayerColors[i];
        const float x = x0 + float(i % cols) * (boxW + pad);
        const Rectangle box{x, top, boxW, boxH};
        DrawRectangleRounded(box, 0.25f, 6, Color{0, 0, 0, 150});
        DrawRectangleRoundedLinesEx(box, 0.25f, 6, std::max(1.f, H * 0.004f), Color{c.r, c.g, c.b, static_cast<unsigned char>(dead ? 110 : 230)});
        // barra de vida embaixo do quadro: verde > amarelo > vermelho
        const Rectangle bar{x + boxW * 0.06f, top + boxH * 0.66f, boxW * 0.88f, boxH * 0.18f};
        DrawRectangleRec(bar, Color{40, 40, 40, 220});
        const Color hc = frac > 0.5f ? Color{80, 200, 70, 255} : frac > 0.25f ? Color{230, 200, 50, 255} : Color{220, 60, 40, 255};
        DrawRectangleRec(Rectangle{bar.x, bar.y, bar.width * frac, bar.height}, hc);
        if (!tex0)
            continue;
        char label[32];
        std::snprintf(label, sizeof label, "P%d %s", i + 1, wotm_padsel::monsterName(type << 5));
        // cada letra tem 1,6 x a altura de largura (wotm_padsel::text): a altura cabe na largura do quadro
        const float th = std::min(boxH * 0.22f, boxW * 0.9f / (1.6f * float(std::max<size_t>(1, std::strlen(label)))));
        const Color tc = dead ? Color{0x50, 0x50, 0x50, 0x80} : Color{static_cast<unsigned char>(c.r / 2), static_cast<unsigned char>(c.g / 2),
                                                                   static_cast<unsigned char>(c.b / 2), 0x80};
        wotm_padsel::text(quads, ram, tex0, rw, rh, label, x + boxW * 0.5f, top + boxH * 0.24f, th, tc, 1.f);
        char w[16];
        std::snprintf(w, sizeof w, "WINS %d", wins);
        wotm_padsel::text(quads, ram, tex0, rw, rh, w, x + boxW * 0.5f, top + boxH * 0.47f, th * 0.85f, Color{0x70, 0x70, 0x70, 0x80}, 1.f);
    }
    if (!quads.empty())
        scene.drawQuads(quads, float(rw), float(rh));
}
} // namespace wotm_multihud
