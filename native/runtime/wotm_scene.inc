// wotm_scene.inc (incluido por ps2_runtime.cpp; raylib e ps2_runtime.h ja estao incluidos)
#include <atomic>
static std::atomic<bool> g_wotmShotKey{false};   // F8 na janela (wotm_foto.inc) -> um quadro da checagem VU1 x nativo agora (wotm_vu1check.inc)
#include "wotm_live.inc"
#include "wotm_foto.inc"   // F7 pausa, F6 avanca um quadro, F8 foto com nota (WOTM_FOTOS)
#include "wotm_syms.h"   // wotm::sym / wotm::fn: enderecos do ELF por nome (tools/native_syms.py --gen)

extern bool g_wotmVu1Tap;   // ps2_memory.cpp (checagem VU1 x nativo)
static void wotmDrawScene(const uint8_t *rdram)
{
    if (!rdram) return;
    wotm_live::initWindowAspect();   // janela na proporcao do widescreen (uma vez, antes da abertura)
    {
        // Sem VU1 ninguem conclui o pacote: o jogo espera `objsInPacket | objsInAlphaPacket` zerarem (rtWaitForVu1, e um laco
        // embutido em particleDraw). Cada apresentacao faz o papel da conclusao do VU1. No quadro amostrado pela checagem VU1 x
        // nativo nao: la o hierFlushObjQ dispara o DMA de verdade, e zerar aqui descartava a OT de alfa ainda nao despachada (os
        // objetos 0x8000/0x4000 saiam como "so o nativo desenha", pista 7 do VU1CHECK).
        static const bool s_noVu1 = std::getenv("PS2X_NO_VU1") != nullptr;
        if (s_noVu1 && !g_wotmVu1Tap)
        {
            const uint32_t zero = 0;
            std::memcpy(const_cast<uint8_t *>(rdram) + wotm::sym::objsInPacket, &zero, 4);
            std::memcpy(const_cast<uint8_t *>(rdram) + wotm::sym::objsInAlphaPacket, &zero, 4);
        }
    }
    wotmFotoKeys();
    const bool drew = wotmDrawNative(rdram); // camada nativa (native/wotm_native.hpp): fase lida da RAM, sem VU1/GS
    {
        // Sem cena nativa (carregamento, troca de tela) a janela ficava com o quadro do GS. Com PS2X_NO_GS o GS nao desenha nada: a
        // area exibida so tem os uploads de textura, e entre o pulo da abertura e o menu aparecia a VRAM crua (blocos de textura
        // embaralhados; tambem entre lutas). Aqui fica preto; WOTM_GSFRAME=1 volta a mostrar o quadro do GS (diagnostico).
        // Retangulo, nao ClearBackground: o quadro do GS ainda esta no lote do raylib, e o glClear imediato vinha antes dele.
        static const bool s_blackGap = std::getenv("PS2X_NO_GS") != nullptr && std::getenv("WOTM_GSFRAME") == nullptr;
        if (!drew && s_blackGap)
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), BLACK);
    }
    wotmFotoOverlay(drew, rdram);
}
#include "wotm_vu1check.inc"
