// CPUs de 4 a 7 no free for all de 1 jogador (tela 12; NATIVO.md, "Ate 7 CPUs pelo menu"). O gancho da tela 12 (wotm_overrides.cpp)
// guarda o numero escolhido; o menu segue com 3 CPUs (telas 15, 19 e 8 do retail), e o changeScreenHook do wotm_padsel.inc grava o
// numero de verdade e sorteia as CPUs extras quando a fase e confirmada.
#pragma once
#include <atomic>

namespace wotm_menucpus
{
constexpr int kMax = 7;           // 1 jogador + 7 IAs: o teto da WOTM_MATCH (8 monstros)
inline std::atomic<int> g_want{0};   // 4..7: CPUs pedidas na tela 12; 0: o que o retail guarda em m_numAIs vale
}
