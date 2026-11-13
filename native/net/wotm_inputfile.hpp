// ABOUTME: Input recording file (WOTM_INPUTREC writes it, WOTM_INPUTPLAY replays it): {frame, controller, pad state} records, with a
// ABOUTME: versioned header; files without the header (pad state without mouse or camera settings) still load. No game dependency.
#pragma once

#include "wotm_lockstep.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace wotm_net
{
struct InputRecord
{
    uint32_t index = 0;        // frame since the level load
    uint32_t controller = 0;   // port + 2 * multitap slot
    PadState state;
};

// Version 2: "WOTMINP" + version byte, then 20-byte little-endian records {u32 index, u32 controller, u16 buttons, u8 rx, ry, lx, ly,
// i16 mouseX, mouseY, u16 cam}. Without the header: 16-byte records {u32 index, u32 controller, u16 buttons, u8 rx, ry, lx, ly,
// 2 bytes of padding}.
inline constexpr char kInputMagic[7] = {'W', 'O', 'T', 'M', 'I', 'N', 'P'};
inline constexpr uint8_t kInputVersion = 2;
inline constexpr size_t kInputRecordSize = 20, kHeaderlessRecordSize = 16;

inline std::vector<uint8_t> encodeInputHeader()
{
    std::vector<uint8_t> b(kInputMagic, kInputMagic + sizeof kInputMagic);
    b.push_back(kInputVersion);
    return b;
}

inline std::vector<uint8_t> encodeInputRecord(const InputRecord &r)
{
    std::vector<uint8_t> b;
    auto put = [&](uint32_t v, int bytes) {
        for (int i = 0; i < bytes; ++i)
            b.push_back(static_cast<uint8_t>(v >> (8 * i)));
    };
    const PadState &s = r.state;
    put(r.index, 4), put(r.controller, 4), put(s.buttons, 2), put(s.rx, 1), put(s.ry, 1), put(s.lx, 1), put(s.ly, 1);
    put(static_cast<uint16_t>(s.mouseX), 2), put(static_cast<uint16_t>(s.mouseY), 2), put(s.cam, 2);
    return b;
}

// Every whole record of the file (a truncated last record is ignored); false with `err` and no records on an unknown version
inline bool readInputRecords(const std::vector<uint8_t> &bytes, std::vector<InputRecord> &out, std::string &err)
{
    out.clear();
    err.clear();
    const bool versioned = bytes.size() >= sizeof kInputMagic + 1 && std::memcmp(bytes.data(), kInputMagic, sizeof kInputMagic) == 0;
    if (versioned && bytes[sizeof kInputMagic] != kInputVersion)
    {
        err = "input recording version " + std::to_string(bytes[sizeof kInputMagic]) + " is not supported (this build reads version " +
              std::to_string(kInputVersion) + " and files without a header)";
        return false;
    }
    const size_t start = versioned ? sizeof kInputMagic + 1 : 0, size = versioned ? kInputRecordSize : kHeaderlessRecordSize;
    for (size_t at = start; at + size <= bytes.size(); at += size)
    {
        const uint8_t *p = bytes.data() + at;
        auto get = [&](int offset, int n) {
            uint32_t v = 0;
            for (int i = 0; i < n; ++i)
                v |= static_cast<uint32_t>(p[offset + i]) << (8 * i);
            return v;
        };
        InputRecord r;
        r.index = get(0, 4), r.controller = get(4, 4);
        r.state.buttons = static_cast<uint16_t>(get(8, 2));
        r.state.rx = p[10], r.state.ry = p[11], r.state.lx = p[12], r.state.ly = p[13];
        if (versioned)
        {
            r.state.mouseX = static_cast<int16_t>(get(14, 2)), r.state.mouseY = static_cast<int16_t>(get(16, 2));
            r.state.cam = static_cast<uint16_t>(get(18, 2));
        }
        out.push_back(r);
    }
    return true;
}
} // namespace wotm_net
