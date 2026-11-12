// ABOUTME: Lobby client for wotm_server: the line exchanges of create, join, set, start and the wait for the match (wotm_lobby_client.hpp).
// ABOUTME: Tested against the real server in wotm_net_test.cpp.
#include "wotm_lobby_client.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace wotm_net
{
    namespace
    {
        constexpr int kReplyMs = 10000;   // an answer from the server itself

        std::vector<std::string> split(const std::string &s, char sep)
        {
            std::vector<std::string> out;
            size_t i = 0;
            while (i <= s.size())
            {
                const size_t j = std::min(s.find(sep, i), s.size());
                out.push_back(s.substr(i, j - i));
                i = j + 1;
            }
            return out;
        }
    }

    bool LobbyClient::expect(const std::string &prefix, std::string &line, std::string &err, int ms)
    {
        while (true)
        {
            if (!s_.readLine(line, ms))
            {
                err = s_.open() ? "no answer from the server" : "the server closed the connection";
                return false;
            }
            if (line.rfind("ERROR", 0) == 0)
            {
                err = line.size() > 6 ? line.substr(6) : line;
                return false;
            }
            if (line.rfind(prefix, 0) == 0)
                return true;
        }
    }

    bool LobbyClient::connect(const std::string &host, uint16_t port, const std::string &build, std::string &err, int protocol)
    {
        if (!s_.connect(host, port, kReplyMs))
        {
            err = "can't reach the server at " + host + ":" + std::to_string(port);
            return false;
        }
        s_.sendLine("HELLO " + std::to_string(protocol) + " " + build);
        std::string line;
        return expect("WELCOME", line, err, kReplyMs);
    }

    std::string LobbyClient::create(const std::string &lobby, const std::string &name, int monster, const std::string &password, std::string &err)
    {
        s_.sendLine("CREATE " + lobby + " " + name + " " + std::to_string(monster) + (password.empty() ? "" : " " + password));
        std::string line;
        if (!expect("CREATED ", line, err, kReplyMs))
            return {};
        code_ = line.substr(8);
        if (!expect("JOINED ", line, err, kReplyMs) || !expect("SETTINGS", line, err, kReplyMs))
            return {};
        return code_;
    }

    bool LobbyClient::join(const std::string &code, const std::string &name, int monster, const std::string &password, std::string &err)
    {
        s_.sendLine("JOIN " + code + " " + name + " " + std::to_string(monster) + (password.empty() ? "" : " " + password));
        std::string line;
        if (!expect("JOINED ", line, err, kReplyMs))
            return false;
        code_ = line.substr(7);
        return expect("SETTINGS", line, err, kReplyMs);
    }

    bool LobbyClient::set(int stage, int wins, const std::string &mods, std::string &err)
    {
        s_.sendLine("SET stage=" + std::to_string(stage) + " wins=" + std::to_string(wins) + " mods=" + mods);
        std::string line;
        return expect("SETTINGS", line, err, kReplyMs);
    }

    bool LobbyClient::waitMembers(int count, std::string &err, int ms)
    {
        std::string line;
        while (expect("MEMBERS", line, err, ms))
        {
            const std::vector<std::string> w = proto::words(line);
            if (w.size() >= 4 && w[3] != "-" && static_cast<int>(split(w[3], ',').size()) >= count)
                return true;
        }
        return false;
    }

    bool LobbyClient::start(std::string &err)
    {
        if (!s_.sendLine("START"))
        {
            err = "the server closed the connection";
            return false;
        }
        return true;
    }

    bool LobbyClient::waitMatch(std::map<std::string, std::string> &match, std::string &err, int ms)
    {
        std::string line;
        if (!expect("MATCH", line, err, ms))
            return false;
        match = proto::keyValues(proto::words(line));
        return true;
    }

    std::vector<std::pair<std::string, std::string>> matchEnv(const std::map<std::string, std::string> &m, const std::string &host)
    {
        auto get = [&](const char *k) { const auto it = m.find(k); return it == m.end() ? std::string{} : it->second; };
        const int you = std::atoi(get("you").c_str());
        const std::vector<std::string> names = split(get("names"), ',');
        const std::string wins = get("wins");
        return {
            {"WOTM_NET", host + ":" + get("port")},
            {"WOTM_NETTOKEN", get("token")},
            {"WOTM_NETNAME", you >= 0 && you < static_cast<int>(names.size()) ? names[static_cast<size_t>(you)] : std::string{}},
            {"WOTM_MATCH", get("monsters") + "," + get("stage")},
            {"WOTM_PLAYERS", get("players")},
            {"WOTM_FIXEDSTEP", get("step")},
            {"WOTM_SEED", get("seed")},
            {"WOTM_KILLTARGET", wins.empty() ? "0" : wins},
            {"WOTM_MODIFIERS", get("mods")},
            {"WOTM_LOCALPLAYER", std::to_string(you + 1)},
        };
    }
}
