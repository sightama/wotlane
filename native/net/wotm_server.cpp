// ABOUTME: wotm_server: servidor dedicado do netcode do wotlane (lobbies criados pelos jogadores, com codigo e senha opcional, e partidas em lockstep), sem codigo do jogo.
// ABOUTME: Uso: wotm_server [--port 7777] [--lobbies 16] [--players 8] [--delay N]. Encaminhe TCP e UDP da porta no roteador.
#include "wotm_net_server.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
    std::atomic<bool> g_stop{false};
    void onSignal(int) { g_stop = true; }

    int usage()
    {
        std::printf("uso: wotm_server [--port 7777] [--lobbies 16] [--players 8] [--delay <quadros>]\n"
                    "  --port     porta TCP (lobby) e UDP (partida); encaminhe as duas no roteador para jogar pela internet\n"
                    "  --lobbies  quantos lobbies podem estar abertos ao mesmo tempo (padrao 16)\n"
                    "  --players  jogadores por lobby (2 a 8; padrao 8)\n"
                    "  --delay    atraso de entrada fixo em quadros (padrao: pelo ping de cada partida)\n");
        return 2;
    }
}

int main(int argc, char **argv)
{
    wotm_net::ServerConfig cfg;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const bool hasValue = i + 1 < argc;
        if (a == "--port" && hasValue)
            cfg.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        else if (a == "--lobbies" && hasValue)
            cfg.lobbies = std::atoi(argv[++i]);
        else if (a == "--players" && hasValue)
            cfg.maxPlayers = std::atoi(argv[++i]);
        else if (a == "--delay" && hasValue)
            cfg.forcedDelay = static_cast<uint32_t>(std::atoi(argv[++i]));
        else
            return usage();
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    wotm_net::Server server(cfg);
    std::string err;
    if (!server.start(err))
    {
        std::fprintf(stderr, "wotm_server: %s\n", err.c_str());
        return 1;
    }
    while (!g_stop)
        server.poll(5);
    std::printf("[servidor] encerrado\n");
    return 0;
}
