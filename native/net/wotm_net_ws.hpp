// ABOUTME: WebSocket protocol pieces (RFC 6455) with no sockets: SHA-1 and base64 for the handshake, request and answer parsing,
// ABOUTME: and frame encoding and parsing. The browser version's netcode speaks WebSocket to wotm_server (wotm_net_wslink.hpp).
#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wotm_net::ws
{
    constexpr uint8_t kContinuation = 0x0, kText = 0x1, kBinary = 0x2, kClose = 0x8, kPing = 0x9, kPong = 0xA;
    // the largest message accepted: lobby lines and match packets are a few hundred bytes; anything this big is a broken or hostile peer
    constexpr size_t kMaxMessage = 64 * 1024;

    inline std::array<uint8_t, 20> sha1(std::string_view data)
    {
        uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
        std::vector<uint8_t> m(data.begin(), data.end());
        const uint64_t bits = static_cast<uint64_t>(m.size()) * 8u;
        m.push_back(0x80);
        while (m.size() % 64 != 56)
            m.push_back(0);
        for (int i = 7; i >= 0; --i)
            m.push_back(static_cast<uint8_t>(bits >> (i * 8)));
        auto rol = [](uint32_t v, int n) { return (v << n) | (v >> (32 - n)); };
        for (size_t off = 0; off < m.size(); off += 64)
        {
            uint32_t w[80];
            for (int i = 0; i < 16; ++i)
                w[i] = uint32_t(m[off + i * 4]) << 24 | uint32_t(m[off + i * 4 + 1]) << 16 | uint32_t(m[off + i * 4 + 2]) << 8 | m[off + i * 4 + 3];
            for (int i = 16; i < 80; ++i)
                w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
            for (int i = 0; i < 80; ++i)
            {
                uint32_t f, k;
                if (i < 20)
                    f = (b & c) | (~b & d), k = 0x5A827999u;
                else if (i < 40)
                    f = b ^ c ^ d, k = 0x6ED9EBA1u;
                else if (i < 60)
                    f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDCu;
                else
                    f = b ^ c ^ d, k = 0xCA62C1D6u;
                const uint32_t t = rol(a, 5) + f + e + k + w[i];
                e = d, d = c, c = rol(b, 30), b = a, a = t;
            }
            h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
        }
        std::array<uint8_t, 20> out{};
        for (int i = 0; i < 20; ++i)
            out[static_cast<size_t>(i)] = static_cast<uint8_t>(h[i / 4] >> (24 - (i % 4) * 8));
        return out;
    }

    inline std::string base64(const uint8_t *data, size_t n)
    {
        static const char *t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        for (size_t i = 0; i < n; i += 3)
        {
            const uint32_t v = uint32_t(data[i]) << 16 | (i + 1 < n ? uint32_t(data[i + 1]) << 8 : 0u) | (i + 2 < n ? data[i + 2] : 0u);
            out += t[v >> 18 & 63];
            out += t[v >> 12 & 63];
            out += i + 1 < n ? t[v >> 6 & 63] : '=';
            out += i + 2 < n ? t[v & 63] : '=';
        }
        return out;
    }

    // Sec-WebSocket-Accept for a client's Sec-WebSocket-Key
    inline std::string acceptKey(std::string_view clientKey)
    {
        const auto d = sha1(std::string(clientKey) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
        return base64(d.data(), d.size());
    }

    struct Handshake
    {
        std::string path, key;
    };

    // the value of header `name` (any case) in an HTTP head, or nullopt
    inline std::optional<std::string> header(std::string_view head, std::string_view name)
    {
        size_t pos = head.find("\r\n");
        while (pos != std::string_view::npos && pos + 2 < head.size())
        {
            const size_t start = pos + 2, end = head.find("\r\n", start);
            const std::string_view line = head.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
            const size_t colon = line.find(':');
            if (colon == name.size())
            {
                bool same = true;
                for (size_t i = 0; i < colon && same; ++i)
                    same = std::tolower(static_cast<unsigned char>(line[i])) == std::tolower(static_cast<unsigned char>(name[i]));
                if (same)
                {
                    size_t v = colon + 1;
                    while (v < line.size() && line[v] == ' ')
                        ++v;
                    return std::string(line.substr(v));
                }
            }
            pos = end;
        }
        return std::nullopt;
    }

    inline bool equalsNoCase(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        return true;
    }

    // a client's opening request (everything up to the blank line): GET <path> with Upgrade: websocket and a key
    inline std::optional<Handshake> parseHandshake(std::string_view request)
    {
        if (request.substr(0, 4) != "GET ")
            return std::nullopt;
        const size_t pathEnd = request.find(' ', 4);
        if (pathEnd == std::string_view::npos)
            return std::nullopt;
        const auto upgrade = header(request, "Upgrade"), key = header(request, "Sec-WebSocket-Key");
        if (!upgrade || !equalsNoCase(*upgrade, "websocket") || !key || key->empty())
            return std::nullopt;
        return Handshake{std::string(request.substr(4, pathEnd - 4)), *key};
    }

    inline std::string handshakeAnswer(std::string_view clientKey)
    {
        return "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " +
               acceptKey(clientKey) + "\r\n\r\n";
    }

    inline std::string handshakeRequest(std::string_view host, std::string_view path, std::string_view key)
    {
        return "GET " + std::string(path) + " HTTP/1.1\r\nHost: " + std::string(host) +
               "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + std::string(key) +
               "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    }

    // a frame; `mask` = client to server (RFC 6455 5.3), with a fixed key: masking is a proxy-cache safeguard, not secrecy
    inline std::vector<uint8_t> encode(uint8_t opcode, const uint8_t *data, size_t n, bool mask, bool fin = true)
    {
        std::vector<uint8_t> out;
        out.push_back(static_cast<uint8_t>((fin ? 0x80 : 0) | opcode));
        const uint8_t m = mask ? 0x80 : 0;
        if (n < 126)
            out.push_back(static_cast<uint8_t>(m | n));
        else if (n <= 0xFFFF)
        {
            out.push_back(static_cast<uint8_t>(m | 126));
            out.push_back(static_cast<uint8_t>(n >> 8));
            out.push_back(static_cast<uint8_t>(n));
        }
        else
        {
            out.push_back(static_cast<uint8_t>(m | 127));
            for (int i = 7; i >= 0; --i)
                out.push_back(static_cast<uint8_t>(static_cast<uint64_t>(n) >> (i * 8)));
        }
        const uint8_t key[4] = {0x37, 0xFA, 0x21, 0x3D};
        if (mask)
            out.insert(out.end(), key, key + 4);
        for (size_t i = 0; i < n; ++i)
            out.push_back(mask ? static_cast<uint8_t>(data[i] ^ key[i % 4]) : data[i]);
        return out;
    }

    struct Frame
    {
        uint8_t opcode = 0;
        std::vector<uint8_t> payload;
    };

    // Turns received bytes into whole messages (fragments joined; control frames come out on their own)
    class FrameParser
    {
    public:
        enum class Result
        {
            Ok,
            NeedMore,
            Error
        };
        // requireMask: the server side (a client must mask every frame)
        explicit FrameParser(bool requireMask) : requireMask_(requireMask) {}

        void feed(const uint8_t *data, size_t n) { buf_.insert(buf_.end(), data, data + n); }

        Result next(Frame &out)
        {
            for (;;)
            {
                if (buf_.size() < 2)
                    return Result::NeedMore;
                const bool fin = (buf_[0] & 0x80) != 0, masked = (buf_[1] & 0x80) != 0;
                const uint8_t opcode = buf_[0] & 0x0F;
                if ((buf_[0] & 0x70) != 0 || masked != requireMask_)
                    return Result::Error;
                uint64_t len = buf_[1] & 0x7F;
                size_t at = 2;
                if (len == 126)
                {
                    if (buf_.size() < 4)
                        return Result::NeedMore;
                    len = uint64_t(buf_[2]) << 8 | buf_[3];
                    at = 4;
                }
                else if (len == 127)
                {
                    if (buf_.size() < 10)
                        return Result::NeedMore;
                    len = 0;
                    for (int i = 0; i < 8; ++i)
                        len = len << 8 | buf_[2 + static_cast<size_t>(i)];
                    at = 10;
                }
                const bool control = (opcode & 0x8) != 0;
                if (len > kMaxMessage || (control && (len > 125 || !fin)) || partial_.size() + len > kMaxMessage)
                    return Result::Error;
                const size_t need = at + (masked ? 4 : 0) + static_cast<size_t>(len);
                if (buf_.size() < need)
                    return Result::NeedMore;
                std::vector<uint8_t> payload(buf_.begin() + static_cast<long>(need - len), buf_.begin() + static_cast<long>(need));
                if (masked)
                    for (size_t i = 0; i < payload.size(); ++i)
                        payload[i] ^= buf_[at + i % 4];
                buf_.erase(buf_.begin(), buf_.begin() + static_cast<long>(need));
                if (control)
                {
                    out.opcode = opcode;
                    out.payload = std::move(payload);
                    return Result::Ok;
                }
                if (opcode == kContinuation)
                {
                    if (!fragmented_)
                        return Result::Error;
                }
                else
                {
                    if (fragmented_ || (opcode != kText && opcode != kBinary))
                        return Result::Error;
                    partialOpcode_ = opcode;
                    partial_.clear();
                }
                partial_.insert(partial_.end(), payload.begin(), payload.end());
                fragmented_ = !fin;
                if (fin)
                {
                    out.opcode = partialOpcode_;
                    out.payload = std::move(partial_);
                    partial_.clear();
                    return Result::Ok;
                }
            }
        }

    private:
        bool requireMask_;
        std::vector<uint8_t> buf_, partial_;
        uint8_t partialOpcode_ = 0;
        bool fragmented_ = false;
    };
}
