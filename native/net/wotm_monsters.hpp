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

    // A partida da WOTM_MATCH e do lobby da a cada monstro repetido o traje seguinte (o n-esimo igual leva o traje n): a lista cabe no
    // disco? Sem caber, *bad recebe o indice do primeiro que nao tem arquivo.
    inline bool fits(const int *monsters, int n, int *bad = nullptr)
    {
        for (int i = 0; i < n; ++i)
        {
            int costume = 0;
            for (int j = 0; j < i; ++j)
                costume += monsters[j] == monsters[i];
            if (costume >= costumes(monsters[i]))
            {
                if (bad)
                    *bad = i;
                return false;
            }
        }
        return true;
    }
}
