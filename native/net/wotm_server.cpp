// ABOUTME: wotm_server: dedicated server for wotlane netcode (player-created lobbies with a code and optional password, lockstep matches), no game code.
// ABOUTME: Usage: wotm_server [--port 7777] [--lobbies 16] [--players 8] [--delay N]. Forward both TCP and UDP on that port in the router.
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
        std::printf("usage: wotm_server [--port 7777] [--lobbies 16] [--players 8] [--delay <frames>]\n"
                    "  --port     TCP (lobby) and UDP (match) port; forward both in the router to play over the internet\n"
                    "  --lobbies  how many lobbies can be open at the same time (default 16)\n"
                    "  --players  players per lobby (2 to 8; default 8)\n"
                    "  --delay    fixed input delay in frames (default: from each match's ping)\n");
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
    std::printf("[server] shut down\n");
    return 0;
}
