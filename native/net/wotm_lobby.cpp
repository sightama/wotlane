// ABOUTME: wotm_lobby: command-line lobby client with no Python (the Mac friends package uses it): creates a lobby or joins one by code
// ABOUTME: on the shared server (or --server), and prints the game's environment as NAME=value lines, like tools/net_lobby.py.
#include "wotm_lobby_client.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>

using namespace wotm_net;

namespace
{
    int usage()
    {
        std::fprintf(stderr,
                     "usage: wotm_lobby --create <lobby> --name <name> --monster <id> [--stage S] [--wins W] [--mods jump,fast,turbo]\n"
                     "                  [--start-with <players>] [--server host:port] [--build <id>]\n"
                     "       wotm_lobby --code <CODE> --name <name> --monster <id> [--server host:port] [--build <id>]\n"
                     "  Without --server: the shared game server (%s). --build: default the wotm.buildid next to this program.\n"
                     "  --create prints \"lobby created, code XXXX\" on stderr, waits for --start-with players (default 2) and starts.\n"
                     "  Both print the game's environment (WOTM_NET=..., one per line) on stdout once the match starts.\n",
                     proto::kDefaultServer);
        return 2;
    }

    std::string buildNextTo(const char *argv0)
    {
        std::string dir = argv0;
        const size_t slash = dir.find_last_of("/\\");
        dir = slash == std::string::npos ? "." : dir.substr(0, slash);
        std::ifstream f(dir + "/wotm.buildid");
        std::string id;
        std::getline(f, id);
        while (!id.empty() && (id.back() == '\r' || id.back() == ' '))
            id.pop_back();
        return id.empty() ? "no-buildid" : id;
    }

    int fail(const std::string &err)
    {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
}

int main(int argc, char **argv)
{
    std::map<std::string, std::string> a;
    for (int i = 1; i < argc; ++i)
    {
        const std::string k = argv[i];
        if (k.rfind("--", 0) != 0 || i + 1 >= argc)
            return usage();
        a[k.substr(2)] = argv[++i];
    }
    if (!a.count("name") || !a.count("monster") || a.count("create") == a.count("code"))
        return usage();
    std::string server = a.count("server") ? a["server"] : proto::kDefaultServer;
    std::string host = server;
    uint16_t port = proto::kDefaultPort;
    if (const size_t colon = server.rfind(':'); colon != std::string::npos)
        host = server.substr(0, colon), port = static_cast<uint16_t>(std::atoi(server.c_str() + colon + 1));
    const std::string build = a.count("build") ? a["build"] : buildNextTo(argv[0]);
    const int monster = std::atoi(a["monster"].c_str());

    LobbyClient c;
    std::string err;
    if (!c.connect(host, port, build, err))
        return fail(err);
    if (a.count("create"))
    {
        const std::string code = c.create(a["create"], a["name"], monster, a.count("password") ? a["password"] : "", err);
        if (code.empty())
            return fail(err);
        std::fprintf(stderr, "lobby created, code %s\n", code.c_str());
        if (!c.set(a.count("stage") ? std::atoi(a["stage"].c_str()) : 1, a.count("wins") ? std::atoi(a["wins"].c_str()) : 0,
                   a.count("mods") ? a["mods"] : "", err) ||
            !c.waitMembers(a.count("start-with") ? std::atoi(a["start-with"].c_str()) : 2, err) || !c.start(err))
            return fail(err);
    }
    else if (!c.join(a["code"], a["name"], monster, a.count("password") ? a["password"] : "", err))
        return fail(err);
    std::map<std::string, std::string> match;
    if (!c.waitMatch(match, err))
        return fail(err);
    for (const auto &kv : matchEnv(match, host))
        std::printf("%s=%s\n", kv.first.c_str(), kv.second.c_str());
    return 0;
}
