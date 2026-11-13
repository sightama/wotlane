// ABOUTME: Lobby client for wotm_server (TCP line protocol, wotm_net_proto.hpp): create or join a lobby by code, owner settings, start,
// ABOUTME: and the game environment for the MATCH line. Used by wotm_lobby (and the in-game lobby); same flow as tools/net_lobby.py.
#pragma once

#include "wotm_net_proto.hpp"
#include "wotm_net_socket.hpp"

#include <climits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace wotm_net
{
    class LobbyClient
    {
    public:
        static constexpr int kWaitForever = INT_MAX;   // waiting on other players: joining, starting
        // HELLO with the protocol (the current one unless a test asks for another) and this game's build id
        bool connect(const std::string &host, uint16_t port, const std::string &build, std::string &err, int protocol = proto::kProtocol);
        // CREATE: the creator owns the lobby; returns its 4-letter code ("" with err on failure)
        std::string create(const std::string &lobby, const std::string &name, int monster, const std::string &password, std::string &err);
        bool join(const std::string &code, const std::string &name, int monster, const std::string &password, std::string &err);
        // owner only: stage, wins and gameplay modifiers ("jump,fast,turbo", "" = none) of the next match
        bool set(int stage, int wins, const std::string &mods, std::string &err);
        // waits until the lobby has `count` members (they may still be on their way: no time limit unless `ms` says one)
        bool waitMembers(int count, std::string &err, int ms = kWaitForever);
        bool start(std::string &err);
        // waits for the owner to start (no time limit unless `ms` says one): the MATCH line's key=value pairs
        bool waitMatch(std::map<std::string, std::string> &match, std::string &err, int ms = kWaitForever);
        const std::string &code() const { return code_; }

    private:
        // reads up to a line starting with prefix; ERROR (or a closed connection, or the time limit) becomes false with err
        bool expect(const std::string &prefix, std::string &line, std::string &err, int ms);
        LineSocket s_;
        std::string code_;
    };

    // the game's environment for a MATCH line (WOTM_NET, WOTM_NETTOKEN, WOTM_MATCH...), in a fixed order; `host` is the server's address
    std::vector<std::pair<std::string, std::string>> matchEnv(const std::map<std::string, std::string> &match, const std::string &host);
}
