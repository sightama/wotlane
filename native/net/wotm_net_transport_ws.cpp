// ABOUTME: The match transport over WebSocket for native builds (WsLink on a TCP socket): what the tests use to play the browser's part
// ABOUTME: against wotm_server. One binary message per protocol message, with the channel byte in front.
#ifndef __EMSCRIPTEN__
#include "wotm_net_transport.hpp"
#include "wotm_net_wslink.hpp"

#include <chrono>

namespace wotm_net
{
    namespace
    {
        class WsTransport : public Transport
        {
        public:
            ~WsTransport() override { close(); }

            // the address is the whole URL (ws://host:port/match); connecting happens on the client's thread, in the first poll
            bool open(const std::string &host, uint16_t, std::string &) override
            {
                url_ = host;
                return true;
            }

            void send(uint8_t channel, const std::vector<uint8_t> &data, bool) override
            {
                if (!link_.ready())
                    return;
                std::vector<uint8_t> packet;
                packet.reserve(data.size() + 1);
                packet.push_back(channel);
                packet.insert(packet.end(), data.begin(), data.end());
                link_.sendPacket(packet);
            }

            bool poll(TransportEvent &ev, int waitMs) override
            {
                if (closed_)
                    return false;
                if (!link_.ready())
                {
                    std::string err;
                    if (link_.connect(url_, 5000, err))
                        ev = {TransportEvent::Kind::Connected, {}};
                    else
                        ev = {TransportEvent::Kind::Closed, {}}, closed_ = true;
                    return true;
                }
                std::vector<uint8_t> packet;
                for (int pass = 0; pass < 2; ++pass)
                {
                    const bool alive = link_.pump();
                    if (link_.nextPacket(packet))
                    {
                        ev = {TransportEvent::Kind::Message, std::vector<uint8_t>(packet.begin() + (packet.empty() ? 0 : 1), packet.end())};
                        return true;
                    }
                    if (!alive)
                    {
                        closed_ = true;
                        ev = {TransportEvent::Kind::Closed, {}};
                        return true;
                    }
                    const auto now = std::chrono::steady_clock::now();
                    if (now - lastPing_ >= std::chrono::milliseconds(500))
                    {
                        link_.ping();
                        lastPing_ = now;
                    }
                    if (pass == 0 && waitMs > 0)
                        link_.waitReadable(waitMs);
                }
                return false;
            }

            uint32_t rttMs() const override { return link_.rttMs(); }

            void close() override { link_.close(); }

        private:
            std::string url_;
            WsLink link_;
            bool closed_ = false;
            std::chrono::steady_clock::time_point lastPing_{};
        };
    }

    std::unique_ptr<Transport> makeWebSocketTransport() { return std::make_unique<WsTransport>(); }
}
#endif
