// ABOUTME: A WebSocket connection over a non-blocking TCP socket: the server side (accepted, answers the browser's handshake) and a
// ABOUTME: client side (native tests and tools). Text messages are lobby lines (LineLink); binary messages are match packets.
#pragma once

#include "wotm_net_socket.hpp"
#include "wotm_net_ws.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace wotm_net
{
    class WsLink : public LineLink
    {
    public:
        // client side: connect() opens it
        WsLink() = default;
        // server side: an accepted connection; pump() answers the handshake, then path() says what it is for
        explicit WsLink(LineSocket accepted) : sock_(std::move(accepted)), server_(true), parser_(true) {}

        // ws://host:port/path; waits up to `timeoutMs` for the server's answer
        bool connect(const std::string &url, int timeoutMs, std::string &err)
        {
            const std::string scheme = "ws://";
            if (url.compare(0, scheme.size(), scheme) != 0)
                return err = "only ws:// URLs are supported natively: " + url, false;
            const size_t slash = url.find('/', scheme.size());
            const std::string hostPort = url.substr(scheme.size(), slash == std::string::npos ? std::string::npos : slash - scheme.size());
            const std::string path = slash == std::string::npos ? "/" : url.substr(slash);
            const size_t colon = hostPort.rfind(':');
            const std::string host = colon == std::string::npos ? hostPort : hostPort.substr(0, colon);
            const uint16_t port = colon == std::string::npos ? 80 : static_cast<uint16_t>(std::atoi(hostPort.c_str() + colon + 1));
            if (!sock_.connect(host, port, timeoutMs))
                return err = "could not connect to " + hostPort, false;
            uint8_t nonce[16];
            std::random_device rd;
            for (uint8_t &b : nonce)
                b = static_cast<uint8_t>(rd());
            const std::string key = ws::base64(nonce, sizeof nonce);
            const std::string req = ws::handshakeRequest(hostPort, path, key);
            if (!sock_.sendBytes(req.data(), req.size()))
                return err = "could not send the WebSocket request", false;
            std::string head;
            const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
            while (head.find("\r\n\r\n") == std::string::npos)
            {
                const bool alive = sock_.pump();
                head += sock_.takeReceived();
                if (head.find("\r\n\r\n") != std::string::npos)
                    break;
                if (!alive || std::chrono::steady_clock::now() >= until || head.size() > kMaxHead)
                {
                    sock_.close();
                    return err = "no WebSocket answer from " + hostPort, false;
                }
                sock_.waitReadable(20);
            }
            const size_t end = head.find("\r\n\r\n");
            const auto accept = ws::header(std::string_view(head).substr(0, end + 2), "Sec-WebSocket-Accept");
            if (head.compare(0, 12, "HTTP/1.1 101") != 0 || !accept || *accept != ws::acceptKey(key))
            {
                sock_.close();
                return err = "the server refused the WebSocket: " + head.substr(0, head.find("\r\n")), false;
            }
            path_ = path;
            ready_ = true;
            const std::string rest = head.substr(end + 4);
            parser_.feed(reinterpret_cast<const uint8_t *>(rest.data()), rest.size());
            return true;
        }

        bool open() const override { return sock_.open(); }
        // the handshake is done (a server-side link before that has no path yet)
        bool ready() const { return ready_; }
        const std::string &path() const { return path_; }

        bool pump() override
        {
            if (!sock_.open())
                return false;
            const bool alive = sock_.pump();
            const std::string in = sock_.takeReceived();
            if (!ready_)
            {
                head_ += in;
                const size_t end = head_.find("\r\n\r\n");
                if (end == std::string::npos)
                {
                    if (head_.size() > kMaxHead)
                        return fail();
                    return alive;
                }
                const auto h = ws::parseHandshake(std::string_view(head_).substr(0, end + 2));
                if (!h)
                {
                    const std::string no = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
                    sock_.sendBytes(no.data(), no.size());
                    return fail();
                }
                const std::string answer = ws::handshakeAnswer(h->key);
                if (!sock_.sendBytes(answer.data(), answer.size()))
                    return fail();
                path_ = h->path;
                ready_ = true;
                const std::string rest = head_.substr(end + 4);
                head_.clear();
                parser_.feed(reinterpret_cast<const uint8_t *>(rest.data()), rest.size());
            }
            else
                parser_.feed(reinterpret_cast<const uint8_t *>(in.data()), in.size());
            ws::Frame f;
            for (;;)
            {
                const ws::FrameParser::Result r = parser_.next(f);
                if (r == ws::FrameParser::Result::NeedMore)
                    break;
                if (r == ws::FrameParser::Result::Error)
                    return fail();
                switch (f.opcode)
                {
                case ws::kText:
                    lines_.emplace_back(f.payload.begin(), f.payload.end());
                    break;
                case ws::kBinary:
                    packets_.push_back(std::move(f.payload));
                    break;
                case ws::kPing:
                    sendFrame(ws::kPong, f.payload.data(), f.payload.size());
                    break;
                case ws::kPong:
                    if (f.payload.size() == 8)
                    {
                        uint64_t sent = 0;
                        for (uint8_t b : f.payload)
                            sent = sent << 8 | b;
                        const uint64_t now = micros();
                        if (now >= sent)
                            rttMs_ = static_cast<uint32_t>((now - sent + 500) / 1000);
                    }
                    break;
                case ws::kClose:
                    sendFrame(ws::kClose, nullptr, 0);
                    sock_.close();
                    return false;
                default:
                    break;
                }
            }
            return alive && sock_.open();
        }

        bool nextLine(std::string &line) override
        {
            if (lines_.empty())
                return false;
            line = std::move(lines_.front());
            lines_.pop_front();
            if (!line.empty() && line.back() == '\n')
                line.pop_back();
            return true;
        }

        bool sendLine(const std::string &line) override { return ready_ && sendFrame(ws::kText, reinterpret_cast<const uint8_t *>(line.data()), line.size()); }

        void waitReadable(int ms) override { sock_.waitReadable(ms); }

        bool nextPacket(std::vector<uint8_t> &packet)
        {
            if (packets_.empty())
                return false;
            packet = std::move(packets_.front());
            packets_.pop_front();
            return true;
        }

        bool sendPacket(const std::vector<uint8_t> &packet) { return ready_ && sendFrame(ws::kBinary, packet.data(), packet.size()); }

        // a ping carrying the send time; the pong gives rttMs() (the server measures each browser this way, for the input delay)
        void ping()
        {
            const uint64_t now = micros();
            uint8_t b[8];
            for (int i = 0; i < 8; ++i)
                b[i] = static_cast<uint8_t>(now >> ((7 - i) * 8));
            sendFrame(ws::kPing, b, sizeof b);
        }
        uint32_t rttMs() const { return rttMs_; }

        void close()
        {
            if (sock_.open() && ready_)
                sendFrame(ws::kClose, nullptr, 0);
            sock_.close();
        }

    private:
        static constexpr size_t kMaxHead = 8192;

        static uint64_t micros()
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        bool sendFrame(uint8_t opcode, const uint8_t *data, size_t n)
        {
            const std::vector<uint8_t> f = ws::encode(opcode, data, n, !server_);
            return sock_.sendBytes(f.data(), f.size());
        }

        bool fail()
        {
            sock_.close();
            return false;
        }

        LineSocket sock_;
        bool server_ = false, ready_ = false;
        ws::FrameParser parser_{false};
        std::string head_, path_;
        std::deque<std::string> lines_;
        std::deque<std::vector<uint8_t>> packets_;
        uint32_t rttMs_ = 0;
    };
}
