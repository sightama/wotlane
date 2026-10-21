// ABOUTME: Linha do tempo das entradas do lockstep: por jogador e por quadro, o estado do controle (6 bytes) que todos os PCs aplicam.
// ABOUTME: Sem dependencia do jogo nem da rede; usada pelo cliente (wotm.exe) e pelo servidor.
#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace wotm_net
{
    // Estado de um controle como o runtime o guarda (PadInputState do Pad.cpp): botoes ativos em 0, analogicos com centro em 0x80
    struct PadState
    {
        uint16_t buttons = 0xFFFFu;
        uint8_t rx = 0x80u, ry = 0x80u, lx = 0x80u, ly = 0x80u;
        bool operator==(const PadState &o) const { return buttons == o.buttons && rx == o.rx && ry == o.ry && lx == o.lx && ly == o.ly; }
        bool operator!=(const PadState &o) const { return !(*this == o); }
    };

    // Quadro F (contado desde a carga da fase) so roda quando todo jogador tem entrada para F. A entrada local amostrada no quadro F vale
    // no quadro F + atraso; os quadros 0 .. atraso - 1 ficam neutros para todos (ninguem tinha como manda-los a tempo). Um jogador que
    // caiu fica neutro do quadro da queda em diante. Cada (jogador, quadro) recebe um valor so: o primeiro que chega (os pacotes repetem
    // entradas ja enviadas).
    class InputTimeline
    {
    public:
        InputTimeline(int players, uint32_t delay) : delay_(delay), players_(static_cast<size_t>(std::max(players, 0)))
        {
            for (Player &p : players_)
                for (uint32_t f = 0; f < delay; ++f)
                    p.frames[f] = PadState{};
            for (size_t i = 0; i < players_.size(); ++i)
                advance(players_[i]);
        }

        uint32_t delay() const { return delay_; }
        int players() const { return static_cast<int>(players_.size()); }

        bool has(int player, uint32_t frame) const
        {
            const Player *p = find(player);
            return p && (frame >= p->dropFrame || p->frames.count(frame) != 0 || frame < p->base);
        }

        bool ready(uint32_t frame) const
        {
            for (int i = 0; i < players(); ++i)
                if (!has(i, frame))
                    return false;
            return true;
        }

        PadState get(int player, uint32_t frame) const
        {
            const Player *p = find(player);
            if (!p || frame >= p->dropFrame)
                return {};
            const auto it = p->frames.find(frame);
            return it != p->frames.end() ? it->second : PadState{};
        }

        // primeiro quadro sem entrada deste jogador (tudo antes dele ja chegou)
        uint32_t nextMissing(int player) const
        {
            const Player *p = find(player);
            return p ? p->contiguous : 0u;
        }

        void set(int player, uint32_t frame, const PadState &s)
        {
            Player *p = find(player);
            if (!p || frame < p->base || frame >= p->dropFrame)
                return;
            p->frames.emplace(frame, s);
            advance(*p);
        }

        // entrada local amostrada no quadro sampledFrame: devolve o quadro em que ela vale
        uint32_t scheduleLocal(int player, uint32_t sampledFrame, const PadState &s)
        {
            const uint32_t frame = sampledFrame + delay_;
            set(player, frame, s);
            return frame;
        }

        void drop(int player, uint32_t frame)
        {
            if (Player *p = find(player))
            {
                p->dropFrame = std::min(p->dropFrame, frame);
                p->frames.erase(p->frames.lower_bound(p->dropFrame), p->frames.end());
                advance(*p);
            }
        }

        bool dropped(int player, uint32_t frame) const
        {
            const Player *p = find(player);
            return p && frame >= p->dropFrame;
        }

        // quadros antes de `frame` nao serao mais lidos (ja rodaram em todo lugar que importa)
        void discardBefore(uint32_t frame)
        {
            for (Player &p : players_)
            {
                p.frames.erase(p.frames.begin(), p.frames.lower_bound(frame));
                p.base = std::max(p.base, std::min(frame, p.contiguous));
            }
        }

    private:
        struct Player
        {
            std::map<uint32_t, PadState> frames;
            uint32_t base = 0;                    // quadros antes deste ja foram descartados
            uint32_t contiguous = 0;              // primeiro quadro que falta
            uint32_t dropFrame = 0xFFFFFFFFu;     // neutro daqui em diante
        };

        static void advance(Player &p)
        {
            while (p.contiguous < p.dropFrame && p.frames.count(p.contiguous) != 0)
                ++p.contiguous;
            if (p.contiguous >= p.dropFrame)
                p.contiguous = p.dropFrame;
        }

        Player *find(int player) { return player >= 0 && static_cast<size_t>(player) < players_.size() ? &players_[player] : nullptr; }
        const Player *find(int player) const
        {
            return player >= 0 && static_cast<size_t>(player) < players_.size() ? &players_[player] : nullptr;
        }

        uint32_t delay_;
        std::vector<Player> players_;
    };
}
