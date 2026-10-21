// ABOUTME: Estado da partida em rede compartilhado entre a thread do jogo (wotm_sync.inc escreve) e a da janela (wotm_live.inc mostra a
// ABOUTME: mensagem; com rede, as teclas de debug que mexem no jogo ficam desligadas: F1 trapacas, F5 camera livre, F6/F7/F8 pausa e foto).
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace wotm_netstatus
{
    inline std::atomic<bool> g_active{false};    // partida em rede (WOTM_NET)
    inline std::atomic<bool> g_waiting{false};   // a thread do jogo esta parada esperando a rede (o vigia nao conta como travamento)
    inline std::atomic<uint32_t> g_pingMs{0};     // ida e volta ao servidor
    inline std::atomic<uint32_t> g_delay{0};      // atraso de entrada da partida, em quadros
    inline std::mutex g_mx;
    inline std::string g_message;                // linha no centro da tela; vazia, nada

    inline void setMessage(const std::string &m)
    {
        std::lock_guard<std::mutex> lk(g_mx);
        g_message = m;
    }
    inline std::string message()
    {
        std::lock_guard<std::mutex> lk(g_mx);
        return g_message;
    }
}
