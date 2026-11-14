// ABOUTME: How a match client reaches the server: ENet over UDP (native builds) or WebSocket (the browser version, which can't open UDP
// ABOUTME: sockets; also native, for tests). MatchClient sends and receives the same encoded messages through either one.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wotm_net
{
    struct TransportEvent
    {
        enum class Kind
        {
            Connected,
            Message,
            Closed
        };
        Kind kind = Kind::Closed;
        std::vector<uint8_t> data;   // Message: one encoded protocol message
    };

    class Transport
    {
    public:
        virtual ~Transport() = default;
        // starts connecting (it completes in poll, with Connected or Closed)
        virtual bool open(const std::string &host, uint16_t port, std::string &err) = 0;
        virtual void send(uint8_t channel, const std::vector<uint8_t> &data, bool reliable) = 0;
        // the next event, waiting up to `waitMs` for one; false if none arrived
        virtual bool poll(TransportEvent &ev, int waitMs) = 0;
        virtual uint32_t rttMs() const = 0;
        // leaves politely (the server sees the player go at once) and frees everything
        virtual void close() = 0;
    };

    // ws://... or wss://... = WebSocket (in the browser, the page's WebSocket); anything else = ENet to host:port
    bool isWebSocketAddress(const std::string &host);
    std::unique_ptr<Transport> makeTransport(const std::string &host);
}
