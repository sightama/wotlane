// ABOUTME: Line-based text TCP socket (the netcode lobby), the LineLink interface it shares with WebSocket, and a non-blocking
// ABOUTME: listener, for Windows (Winsock) and POSIX. Used by the server and by the tests and tools that talk to the lobby.
#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace wotm_net
{
#ifdef _WIN32
    using SocketHandle = SOCKET;
    constexpr SocketHandle kNoSocket = INVALID_SOCKET;
    inline void closeSocket(SocketHandle s) { closesocket(s); }
    inline bool wouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
    inline void setNonBlocking(SocketHandle s)
    {
        u_long on = 1;
        ioctlsocket(s, FIONBIO, &on);
    }
    // Winsock reference-counts WSAStartup: each user calls it once
    inline bool socketsInit()
    {
        static const bool ok = [] {
            WSADATA d;
            return WSAStartup(MAKEWORD(2, 2), &d) == 0;
        }();
        return ok;
    }
#else
    using SocketHandle = int;
    constexpr SocketHandle kNoSocket = -1;
    inline void closeSocket(SocketHandle s) { ::close(s); }
    inline bool wouldBlock() { return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS; }
    inline void setNonBlocking(SocketHandle s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK); }
    inline bool socketsInit() { return true; }
#endif

    // waits up to `ms` for the socket to become readable (or writable)
    inline bool waitSocket(SocketHandle s, int ms, bool write)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(s, &set);
        timeval tv{ms / 1000, (ms % 1000) * 1000};
        const int n = select(static_cast<int>(s) + 1, write ? nullptr : &set, write ? &set : nullptr, nullptr, &tv);
        return n > 0;
    }

    // Sending to a peer that already closed raises SIGPIPE on POSIX, which kills the process (wotm_server when a lobby member drops);
    // with the signal suppressed, send fails and sendLine closes the socket like any other error. Windows has no SIGPIPE.
#if defined(MSG_NOSIGNAL)
    constexpr int kSendFlags = MSG_NOSIGNAL;
#else
    constexpr int kSendFlags = 0;
#endif

    // A connection that carries lobby lines: a TCP LineSocket or a WebSocket (WsLink, the browser version); the server and the tests
    // use either through this
    class LineLink
    {
    public:
        virtual ~LineLink() = default;
        virtual bool open() const = 0;
        // reads whatever has arrived without blocking; false if the connection dropped
        virtual bool pump() = 0;
        // next complete line already received (without the \n and \r)
        virtual bool nextLine(std::string &line) = 0;
        virtual bool sendLine(const std::string &line) = 0;
        // waits up to `ms` for something to arrive
        virtual void waitReadable(int ms) = 0;

        void waitReadable(int ms) override
        {
            if (open())
                waitSocket(s_, ms, false);
        }

        // everything received and not yet taken as lines (a WebSocket reads the raw bytes through this)
        std::string takeReceived()
        {
            std::string out;
            out.swap(in_);
            return out;
        }

        void close()
        {
            if (s_ != kNoSocket)
                closeSocket(s_);
            s_ = kNoSocket;
        }

        SocketHandle handle() const { return s_; }

    private:
        void setup()
        {
            setNonBlocking(s_);
            int one = 1;
            setsockopt(s_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&one), sizeof one);
#ifdef SO_NOSIGPIPE   // macOS: no MSG_NOSIGNAL, the socket option does the same
            setsockopt(s_, SOL_SOCKET, SO_NOSIGPIPE, reinterpret_cast<const char *>(&one), sizeof one);
#endif
        }
        bool connectedOk()
        {
            int err = 0;
            socklen_t len = sizeof err;
            return getsockopt(s_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &len) == 0 && err == 0;
        }

        SocketHandle s_ = kNoSocket;
        std::string in_;
    };

    class Listener
    {
    public:
        ~Listener() { close(); }
        // `bindAddr`: an IPv4 address to listen on only (e.g. 127.0.0.1, behind a proxy); null = every interface
        bool listen(uint16_t port, const char *bindAddr = nullptr)
        {
            if (!socketsInit())
                return false;
            s_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (s_ == kNoSocket)
                return false;
            int one = 1;
            setsockopt(s_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof one);
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bindAddr && inet_pton(AF_INET, bindAddr, &a.sin_addr) != 1)
            {
                close();
                return false;
            }
            a.sin_port = htons(port);
            if (bind(s_, reinterpret_cast<sockaddr *>(&a), sizeof a) != 0 || ::listen(s_, 16) != 0)
            {
                close();
                return false;
            }
            setNonBlocking(s_);
            return true;
        }
        // a new connection, if there is one; `peerIp` (optional) gets the address it came from
        bool accept(LineSocket &out, std::string *peerIp = nullptr)
        {
            if (s_ == kNoSocket)
                return false;
            sockaddr_in from{};
            socklen_t len = sizeof from;
            const SocketHandle c = ::accept(s_, reinterpret_cast<sockaddr *>(&from), &len);
            if (c == kNoSocket)
                return false;
            if (peerIp)
            {
                char buf[INET_ADDRSTRLEN] = {};
                *peerIp = inet_ntop(AF_INET, &from.sin_addr, buf, sizeof buf) ? buf : "";
            }
            out = LineSocket(c);
            return true;
        }
        void close()
        {
            if (s_ != kNoSocket)
                closeSocket(s_);
            s_ = kNoSocket;
        }

    private:
        SocketHandle s_ = kNoSocket;
    };
}
