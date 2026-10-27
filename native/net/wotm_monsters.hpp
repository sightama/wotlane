// ABOUTME: Monstros que existem no disco: quantos trajes (arquivos MON\<NOME><traje>.NGP/.TEX/.RTX/.PTR) cada indice do jogo tem.
// ABOUTME: Usado pela WOTM_MATCH (wotm_overrides.cpp) e pelo JOIN do wotm_server: monstro sem arquivo travava a carga da fase.
#pragma once

namespace wotm_monsters
{
    // Indice do MonsterLongNames do jogo (0x6EF860, char[16][8]): null, monkey, robot, lizard, shogun, mantis, spider, energy, lava,
    // dragon, rock, alien, ogre, assboss, jelly, final. spider e ogre foram cortados (nenhum arquivo em MON); assboss tem 2 trajes,
    // jelly e final 1. Um arquivo que falta e lido com 0 bytes, o slot dele no fileStatus fica com o endereco do seguinte, o mesmo .RTX
    // entra duas vezes e o texmResRelocate liga a lista de pacotes dele nela mesma (laco sem fim na carga).
    constexpr int kCount = 16;
    constexpr int kCostumes[kCount] = {0, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 0, 2, 1, 1};

    inline int costumes(int monster) { return monster >= 0 && monster < kCount ? kCostumes[monster] : 0; }

    // Primeiro traje automatico: o 0 do assboss (Goliath) e um prototipo (wotm_custom.inc)
    inline int firstCostume(int monster) { return monster == 13 ? 1 : 0; }

    constexpr int kMaxList = 16;

    // A lista cabe no disco com os trajes que a partida da WOTM_MATCH e do lobby da (userintMainDirect, wotm_overrides.cpp)?
    // `asked[i]` e o traje pedido (-1 ou asked nulo: automatico); os pedidos entram primeiro (o primeiro a pedir fica com ele), depois
    // cada automatico leva o menor livre do mesmo monstro a partir do firstCostume, e com mais de 4 iguais repete (c & 3). Trajes de mod
    // (4 ou mais) vem de mods/ e nao sao conferidos aqui. Sem caber, *bad recebe o indice do primeiro que nao tem arquivo.
    inline bool fits(const int *monsters, const int *asked, int n, int *bad = nullptr)
    {
        if (n > kMaxList)
            n = kMaxList;
        int given[kMaxList];
        auto taken = [&](int i, int c) {
            for (int j = 0; j < n; ++j)
                if (j != i && given[j] == c && monsters[j] == monsters[i])
                    return true;
            return false;
        };
        for (int i = 0; i < n; ++i)
            given[i] = -1;
        for (int i = 0; i < n; ++i)
            if (asked && asked[i] >= 0 && !taken(i, asked[i]))
                given[i] = asked[i];
        for (int i = 0; i < n; ++i)
            for (int c = firstCostume(monsters[i]); given[i] < 0; ++c)
                if (c > 3 || !taken(i, c))
                    given[i] = c & 3;
        for (int i = 0; i < n; ++i)
            if (given[i] < 4 && given[i] >= costumes(monsters[i]))
            {
                if (bad)
                    *bad = i;
                return false;
            }
        return true;
    }
}
