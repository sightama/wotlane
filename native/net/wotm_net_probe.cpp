// ABOUTME: wotm_net_probe: checks a wotm_server end to end from this PC: two clients form a lobby over TCP, start a match and play a
// ABOUTME: short lockstep exchange over UDP. Use after deploying or when friends can't connect. Exit 0 when everything worked.
#include "wotm_net_client.hpp"
#include "wotm_net_proto.hpp"
#include "wotm_net_socket.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

using namespace wotm_net;
namespace p = wotm_net::proto;

namespace
{
    int usage()
    {
        std::printf("usage: wotm_net_probe <host>[:7777] [--frames 300]\n"
                    "  Two clients on this PC create a lobby on the server, start a match and exchange <frames> frames of input\n"
                    "  (300 = 5 s at 60 fps). Prints each step; exit 0 when the lobby (TCP) and the match (UDP) both work.\n");
        return 2;
    }

    std::string readUntil(LineSocket &s, const std::string &prefix, int ms = 5000)
    {
        std::string line;
        while (s.readLine(line, ms))
            if (line.rfind(prefix, 0) == 0 || line.rfind("ERROR", 0) == 0)
                return line;
        return {};
    }

    int fail(const char *step, const std::string &detail)
    {
        std::printf("FAIL  %s: %s\n", step, detail.empty() ? "no answer" : detail.c_str());
        return 1;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2 || argv[1][0] == '-')
        return usage();
    std::string host = argv[1];
    uint16_t port = p::kDefaultPort;
    if (const size_t colon = host.rfind(':'); colon != std::string::npos)
        port = static_cast<uint16_t>(std::atoi(host.c_str() + colon + 1)), host.resize(colon);
    uint32_t frames = 300;
    for (int i = 2; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--frames")
            frames = static_cast<uint32_t>(std::atoi(argv[++i]));

    // lobby (TCP): A creates, B joins by code, A starts
    LineSocket a, b;
    if (!a.connect(host, port, 5000) || !b.connect(host, port, 5000))
        return fail("lobby (TCP)", "can't connect to " + host + ":" + std::to_string(port));
    const std::string build = "probe";
    a.sendLine("HELLO " + std::to_string(p::kProtocol) + " " + build);
    b.sendLine("HELLO " + std::to_string(p::kProtocol) + " " + build);
    if (std::string l = readUntil(a, "WELCOME"); l.rfind("WELCOME", 0) != 0 || readUntil(b, "WELCOME").rfind("WELCOME", 0) != 0)
        return fail("lobby (TCP)", l);
    a.sendLine("CREATE Probe ProbeA 3");
    const std::string created = readUntil(a, "CREATED");
    if (created.rfind("CREATED ", 0) != 0)
        return fail("create lobby", created);
    const std::string code = created.substr(8);
    b.sendLine("JOIN " + code + " ProbeB 1");
    if (std::string l = readUntil(b, "JOINED"); l.rfind("JOINED", 0) != 0)
        return fail("join lobby", l);
    std::printf("ok    lobby %s on %s:%u (TCP)\n", code.c_str(), host.c_str(), port);
    a.sendLine("START");
    std::map<std::string, std::string> ma = p::keyValues(p::words(readUntil(a, "MATCH"))),
                                       mb = p::keyValues(p::words(readUntil(b, "MATCH")));
    if (!ma.count("token") || !mb.count("token"))
        return fail("start match", "no MATCH line");

    // match (UDP): both clients connect with their tokens and exchange input frame by frame
    MatchClient ca, cb;
    std::string err;
    ClientConfig cfg{host, port, ma["token"], build, "probe", "ProbeA"};
    if (!ca.start(cfg, err))
        return fail("match (UDP)", err);
    cfg.token = mb["token"], cfg.name = "ProbeB";
    if (!cb.start(cfg, err))
        return fail("match (UDP)", err);
    if (!ca.waitStarted(10000) || !cb.waitStarted(10000))
        return fail("match (UDP)", "the match didn't start: " + ca.message() + " / " + cb.message() + " (is UDP " + std::to_string(port) + " open?)");
    const auto t0 = std::chrono::steady_clock::now();
    const uint32_t d = ca.delay();
    for (uint32_t f = 0; f < frames + d; ++f)
    {
        PadState s;
        s.buttons = static_cast<uint16_t>(0xFFFFu & ~f);
        if (f < frames)
            ca.pushLocal(f + d, s), cb.pushLocal(f + d, s);
        if (!ca.waitFrame(f, 5000) || !cb.waitFrame(f, 5000))
            return fail("match (UDP)", "frame " + std::to_string(f) + " never arrived");
        if (ca.input(0, f) != cb.input(0, f) || ca.input(1, f) != cb.input(1, f))
            return fail("match (UDP)", "the two clients disagree on frame " + std::to_string(f));
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("ok    match: %u frames in %.1f s over UDP, input delay %u, round trip %u ms\n", frames + d, secs, d, ca.rttMs());
    ca.stop();
    cb.stop();
    std::printf("PASS\n");
    return 0;
}
