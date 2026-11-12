// ABOUTME: The match transport over ENet (UDP, reliable control channel and unreliable inputs): native builds' connection to wotm_server.
// ABOUTME: Also picks the transport for an address (makeTransport): WebSocket for ws:// and wss://, ENet otherwise.
#include "wotm_net_proto.hpp"
#include "wotm_net_transport.hpp"

#include <enet/enet.h>

#include <chrono>

namespace wotm_net
{
    std::unique_ptr<Transport> makeWebSocketTransport();   // wotm_net_transport_ws.cpp (native) or wotm_net_transport_web.cpp (browser)

    namespace
    {
        class EnetTransport : public Transport
        {
        public:
            ~EnetTransport() override { close(); }

            bool open(const std::string &host, uint16_t port, std::string &err) override
            {
                if (enet_initialize() != 0)
                    return err = "enet_initialize failed", false;
                up_ = true;
                host_ = enet_host_create(nullptr, 1, proto::kChannels, 0, 0);
                if (!host_)
                    return err = "could not open a UDP socket", false;
                ENetAddress a;
                if (enet_address_set_host(&a, host.c_str()) != 0)
                    return err = "invalid address: " + host, false;
                a.port = port;
                peer_ = enet_host_connect(host_, &a, proto::kChannels, 0);
                if (!peer_)
                    return err = "enet_host_connect failed", false;
                return true;
            }

            void send(uint8_t channel, const std::vector<uint8_t> &data, bool reliable) override
            {
                if (peer_)
                    enet_peer_send(peer_, channel, enet_packet_create(data.data(), data.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0));
            }

            bool poll(TransportEvent &ev, int waitMs) override
            {
                if (!host_)
                    return false;
                ENetEvent e;
                while (enet_host_service(host_, &e, static_cast<enet_uint32>(waitMs)) > 0)
                {
                    switch (e.type)
                    {
                    case ENET_EVENT_TYPE_CONNECT:
                        enet_peer_ping_interval(peer_, 100);
                        enet_peer_timeout(peer_, 0, 10000, 20000);
                        ev = {TransportEvent::Kind::Connected, {}};
                        return true;
                    case ENET_EVENT_TYPE_RECEIVE:
                        ev = {TransportEvent::Kind::Message, std::vector<uint8_t>(e.packet->data, e.packet->data + e.packet->dataLength)};
                        enet_packet_destroy(e.packet);
                        return true;
                    case ENET_EVENT_TYPE_DISCONNECT:
                        peer_ = nullptr;
                        ev = {TransportEvent::Kind::Closed, {}};
                        return true;
                    default:
                        break;
                    }
                }
                return false;
            }

            uint32_t rttMs() const override { return peer_ ? peer_->roundTripTime : 0u; }

            void close() override
            {
                if (peer_)
                {
                    enet_peer_disconnect(peer_, 0);
                    ENetEvent e;
                    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                    while (std::chrono::steady_clock::now() < until && enet_host_service(host_, &e, 10) >= 0)
                    {
                        if (e.type == ENET_EVENT_TYPE_RECEIVE)
                            enet_packet_destroy(e.packet);
                        else if (e.type == ENET_EVENT_TYPE_DISCONNECT)
                            break;
                    }
                    enet_peer_reset(peer_);
                    peer_ = nullptr;
                }
                if (host_)
                {
                    enet_host_destroy(host_);
                    host_ = nullptr;
                }
                if (up_)
                {
                    enet_deinitialize();
                    up_ = false;
                }
            }

        private:
            ENetHost *host_ = nullptr;
            ENetPeer *peer_ = nullptr;
            bool up_ = false;
        };
    }

    bool isWebSocketAddress(const std::string &host) { return host.rfind("ws://", 0) == 0 || host.rfind("wss://", 0) == 0; }

    std::unique_ptr<Transport> makeTransport(const std::string &host)
    {
        if (isWebSocketAddress(host))
            return makeWebSocketTransport();
        return std::make_unique<EnetTransport>();
    }
}
