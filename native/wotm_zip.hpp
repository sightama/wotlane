// ABOUTME: Host versions of the game's inflate hot loop and CRC-32 (zipInflateCodes and zipCrc32 in src/common/zip.cpp), working on
// ABOUTME: guest RAM with the same reads, writes and results, so that a stage's 512 KB .TEX/.RTX blocks unzip in milliseconds.
#pragma once

#include <cstdint>
#include <cstring>
#include <functional>

namespace wotm::zip
{
constexpr uint32_t kReadBuf = 0x01F7F840u;   // ZIP_BUF: the 512 KB read window at the top of RAM
constexpr uint32_t kReadBufSize = 0x80000u;
constexpr uint32_t kWindow = 0x80000u;   // WSIZE: output flushed by zipFlush every 512 KB
constexpr uint32_t kRamMask = 0x03FFFFFFu;   // the runtime's 64 MB of guest RAM (PS2_RAM_MASK): the stages unzip above 32 MB

// The game's inflate globals (common/zip.cpp) that zipInflateCodes and zipFlush read and write
struct State
{
    uint32_t bitBucket = 0, bitCount = 0, windowPos = 0;   // G_bitBucket, G_bitCount, G_windowPos
    uint32_t fileBuf = 0;   // zipFileBuf: next compressed byte in the read window
    uint32_t outBuf = 0;    // outFileBuf: start of the current output window
    uint32_t outWindow = 0;   // outFileWindow: the window the last copy read from
    uint64_t crc = 0, outSize = 0;   // G_crc32val, G_outSize (unsigned long: 64-bit on the EE)
};

// zipCrc32: zlib's CRC-32 over guest bytes with the game's table (crc_32_tab, 64-bit entries)
inline uint64_t crc32(const uint64_t *table, uint64_t crc, const uint8_t *ram, uint32_t addr, int64_t len)
{
    if (addr == 0)
        return 0;
    crc ^= 0xFFFFFFFFull;
    if (len) do
    {
        crc = table[(static_cast<int32_t>(crc) ^ ram[addr++ & kRamMask]) & 0xFF] ^ (crc >> 8);
    } while (--len);
    return crc ^ 0xFFFFFFFFull;
}

// zipFlush: adds the w bytes at outBuf to the CRC and moves the output on
inline void flush(const uint8_t *ram, const uint64_t *table, State &s, uint32_t w)
{
    s.crc = crc32(table, s.crc, ram, s.outBuf, w);
    s.outBuf += w;
    s.outSize -= w;
}

// zipInflateCodes(tl, td, bl, bd): decodes one deflate block with the Huffman tables at guest addresses tl/td (struct huft, 8 bytes: e, b,
// then v.n or v.t at +4) and the game's mask_bits at guest address maskBits. `refill` is zipGetChar's read of the next 512 KB block
// into kReadBuf (fileReads with whichHalfMeg++); fileBuf goes back to kReadBuf after it. Returns 1 on an invalid code (leaving the
// bit state and window position unsaved, as the game does) and 0 at the end of the block.
inline int inflateCodes(uint8_t *ram, const uint64_t *crcTable, uint32_t maskBits, State &s, uint32_t tl, uint32_t td, int bl,
                        int bd, const std::function<void()> &refill)
{
    auto u8 = [ram](uint32_t a) { return ram[a & kRamMask]; };
    // little-endian, byte by byte so that an entry at the end of RAM wraps like the guest's reads do
    auto u16 = [&u8](uint32_t a) { return static_cast<uint16_t>(u8(a) | u8(a + 1) << 8); };
    auto u32 = [&u8](uint32_t a) { return static_cast<uint32_t>(u8(a) | u8(a + 1) << 8 | u8(a + 2) << 16 | uint32_t(u8(a + 3)) << 24); };
    auto maskBit = [&](uint32_t n) -> uint32_t { return u16(maskBits + 2u * n); };   // mask_bits[n]
    auto nextByte = [&]() -> uint64_t {
        if (s.fileBuf >= kReadBuf + kReadBufSize)
        {
            refill();
            s.fileBuf = kReadBuf;
        }
        return u8(s.fileBuf++);
    };

    uint64_t b = s.bitBucket;
    uint32_t k = s.bitCount, w = s.windowPos;
    auto needBits = [&](uint32_t n) {
        while (k < n)
        {
            b |= nextByte() << (k & 63);   // 64-bit shifts take the low 6 bits of the count, as on the EE
            k += 8;
        }
    };
    auto dumpBits = [&](uint32_t n) {
        b >>= n & 63;
        k -= n;
    };
    // a table lookup with its sub-tables (e > 16); false on the invalid code 99
    auto decode = [&](uint32_t table, uint32_t mask, uint32_t &t, uint32_t &e) {
        t = table + 8u * (static_cast<uint32_t>(b) & mask);
        while ((e = u8(t)) > 16)
        {
            if (e == 99)
                return false;
            dumpBits(u8(t + 1));
            e -= 16;
            needBits(e);
            t = u32(t + 4) + 8u * (static_cast<uint32_t>(b) & maskBit(e));
        }
        dumpBits(u8(t + 1));
        return true;
    };

    const uint32_t ml = maskBit(static_cast<uint32_t>(bl)), md = maskBit(static_cast<uint32_t>(bd));
    uint32_t e, t;
    for (;;)
    {
        needBits(static_cast<uint32_t>(bl));
        if (!decode(tl, ml, t, e))
            return 1;
        if (e == 16)   // a literal
        {
            ram[(s.outBuf + w++) & kRamMask] = static_cast<uint8_t>(u16(t + 4));
            if (w == kWindow)
            {
                flush(ram, crcTable, s, w);
                w = 0;
            }
            continue;
        }
        if (e == 15)   // end of block
            break;

        // a length, then its distance
        needBits(e);
        uint32_t n = u16(t + 4) + (static_cast<uint32_t>(b) & maskBit(e));
        dumpBits(e);
        needBits(static_cast<uint32_t>(bd));
        if (!decode(td, md, t, e))
            return 1;
        needBits(e);
        uint32_t d = w - u16(t + 4) - (static_cast<uint32_t>(b) & maskBit(e));
        dumpBits(e);

        // the copy; the window before the current one sits right below outBuf
        do
        {
            s.outWindow = (d &= kWindow - 1) >= w ? s.outBuf - kWindow : s.outBuf;
            e = kWindow - (d > w ? d : w);
            if (e > n)
                e = n;
            n -= e;
            const uint32_t to = (s.outBuf + w) & kRamMask, from = (s.outWindow + d) & kRamMask;
            if (w - d >= e && to + e <= kRamMask + 1 && from + e <= kRamMask + 1)   // no overlap, and neither range wraps
            {
                std::memmove(ram + to, ram + from, e);
                w += e;
                d += e;
            }
            else
            {
                do
                    ram[(s.outBuf + w++) & kRamMask] = ram[(s.outWindow + d++) & kRamMask];
                while (--e);
            }
            if (w == kWindow)
            {
                flush(ram, crcTable, s, w);
                w = 0;
            }
        } while (n);
    }

    s.windowPos = w;
    s.bitBucket = static_cast<uint32_t>(b);
    s.bitCount = k;
    return 0;
}
}
