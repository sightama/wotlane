// Reads textures from GS VRAM (4 MB, the PS2's native layout) given a TEX0.
// The addressing tables (page/block/column) are the GS's and come from PS2Recomp (ps2xRuntime/src/lib/gs/ps2_gs_memory.cpp, GPL-3.0).
#pragma once

#include <cstdint>
#include <vector>

namespace wotm {
namespace gs {

struct Vram {
    const uint8_t *p = nullptr;
    size_t size = 0;
    bool ok() const { return p && size >= 0x400000; }
};

enum Psm : uint32_t { CT32 = 0x00, CT24 = 0x01, CT16 = 0x02, CT16S = 0x0A, T8 = 0x13, T4 = 0x14, T8H = 0x1B, T4HL = 0x24, T4HH = 0x2C };

static constexpr uint8_t kBlockC32[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
static constexpr uint8_t kBlockC16[8][4] = {{0, 2, 8, 10}, {1, 3, 9, 11}, {4, 6, 12, 14}, {5, 7, 13, 15}, {16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31}};
static constexpr uint8_t kBlockP8[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
static constexpr uint8_t kBlockP4[8][4] = {{0, 2, 8, 10}, {1, 3, 9, 11}, {4, 6, 12, 14}, {5, 7, 13, 15}, {16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31}};

static constexpr uint8_t kCol32[8][8] = {
    {0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}, {16, 17, 20, 21, 24, 25, 28, 29}, {18, 19, 22, 23, 26, 27, 30, 31},
    {32, 33, 36, 37, 40, 41, 44, 45}, {34, 35, 38, 39, 42, 43, 46, 47}, {48, 49, 52, 53, 56, 57, 60, 61}, {50, 51, 54, 55, 58, 59, 62, 63}};
static constexpr uint8_t kCol16[8][16] = {
    {0, 2, 8, 10, 16, 18, 24, 26, 1, 3, 9, 11, 17, 19, 25, 27}, {4, 6, 12, 14, 20, 22, 28, 30, 5, 7, 13, 15, 21, 23, 29, 31},
    {32, 34, 40, 42, 48, 50, 56, 58, 33, 35, 41, 43, 49, 51, 57, 59}, {36, 38, 44, 46, 52, 54, 60, 62, 37, 39, 45, 47, 53, 55, 61, 63},
    {64, 66, 72, 74, 80, 82, 88, 90, 65, 67, 73, 75, 81, 83, 89, 91}, {68, 70, 76, 78, 84, 86, 92, 94, 69, 71, 77, 79, 85, 87, 93, 95},
    {96, 98, 104, 106, 112, 114, 120, 122, 97, 99, 105, 107, 113, 115, 121, 123}, {100, 102, 108, 110, 116, 118, 124, 126, 101, 103, 109, 111, 117, 119, 125, 127}};
static constexpr uint8_t kCol8[16][16] = {
    {0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54}, {8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
    {33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23}, {41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31},
    {96, 100, 112, 116, 64, 68, 80, 84, 98, 102, 114, 118, 66, 70, 82, 86}, {104, 108, 120, 124, 72, 76, 88, 92, 106, 110, 122, 126, 74, 78, 90, 94},
    {65, 69, 81, 85, 97, 101, 113, 117, 67, 71, 83, 87, 99, 103, 115, 119}, {73, 77, 89, 93, 105, 109, 121, 125, 75, 79, 91, 95, 107, 111, 123, 127},
    {128, 132, 144, 148, 160, 164, 176, 180, 130, 134, 146, 150, 162, 166, 178, 182}, {136, 140, 152, 156, 168, 172, 184, 188, 138, 142, 154, 158, 170, 174, 186, 190},
    {161, 165, 177, 181, 129, 133, 145, 149, 163, 167, 179, 183, 131, 135, 147, 151}, {169, 173, 185, 189, 137, 141, 153, 157, 171, 175, 187, 191, 139, 143, 155, 159},
    {224, 228, 240, 244, 192, 196, 208, 212, 226, 230, 242, 246, 194, 198, 210, 214}, {232, 236, 248, 252, 200, 204, 216, 220, 234, 238, 250, 254, 202, 206, 218, 222},
    {193, 197, 209, 213, 225, 229, 241, 245, 195, 199, 211, 215, 227, 231, 243, 247}, {201, 205, 217, 221, 233, 237, 249, 253, 203, 207, 219, 223, 235, 239, 251, 255}};
static constexpr uint16_t kCol4[16][32] = {
    {0, 8, 32, 40, 64, 72, 96, 104, 2, 10, 34, 42, 66, 74, 98, 106, 4, 12, 36, 44, 68, 76, 100, 108, 6, 14, 38, 46, 70, 78, 102, 110},
    {16, 24, 48, 56, 80, 88, 112, 120, 18, 26, 50, 58, 82, 90, 114, 122, 20, 28, 52, 60, 84, 92, 116, 124, 22, 30, 54, 62, 86, 94, 118, 126},
    {65, 73, 97, 105, 1, 9, 33, 41, 67, 75, 99, 107, 3, 11, 35, 43, 69, 77, 101, 109, 5, 13, 37, 45, 71, 79, 103, 111, 7, 15, 39, 47},
    {81, 89, 113, 121, 17, 25, 49, 57, 83, 91, 115, 123, 19, 27, 51, 59, 85, 93, 117, 125, 21, 29, 53, 61, 87, 95, 119, 127, 23, 31, 55, 63},
    {192, 200, 224, 232, 128, 136, 160, 168, 194, 202, 226, 234, 130, 138, 162, 170, 196, 204, 228, 236, 132, 140, 164, 172, 198, 206, 230, 238, 134, 142, 166, 174},
    {208, 216, 240, 248, 144, 152, 176, 184, 210, 218, 242, 250, 146, 154, 178, 186, 212, 220, 244, 252, 148, 156, 180, 188, 214, 222, 246, 254, 150, 158, 182, 190},
    {129, 137, 161, 169, 193, 201, 225, 233, 131, 139, 163, 171, 195, 203, 227, 235, 133, 141, 165, 173, 197, 205, 229, 237, 135, 143, 167, 175, 199, 207, 231, 239},
    {145, 153, 177, 185, 209, 217, 241, 249, 147, 155, 179, 187, 211, 219, 243, 251, 149, 157, 181, 189, 213, 221, 245, 253, 151, 159, 183, 191, 215, 223, 247, 255},
    {256, 264, 288, 296, 320, 328, 352, 360, 258, 266, 290, 298, 322, 330, 354, 362, 260, 268, 292, 300, 324, 332, 356, 364, 262, 270, 294, 302, 326, 334, 358, 366},
    {272, 280, 304, 312, 336, 344, 368, 376, 274, 282, 306, 314, 338, 346, 370, 378, 276, 284, 308, 316, 340, 348, 372, 380, 278, 286, 310, 318, 342, 350, 374, 382},
    {321, 329, 353, 361, 257, 265, 289, 297, 323, 331, 355, 363, 259, 267, 291, 299, 325, 333, 357, 365, 261, 269, 293, 301, 327, 335, 359, 367, 263, 271, 295, 303},
    {337, 345, 369, 377, 273, 281, 305, 313, 339, 347, 371, 379, 275, 283, 307, 315, 341, 349, 373, 381, 277, 285, 309, 317, 343, 351, 375, 383, 279, 287, 311, 319},
    {448, 456, 480, 488, 384, 392, 416, 424, 450, 458, 482, 490, 386, 394, 418, 426, 452, 460, 484, 492, 388, 396, 420, 428, 454, 462, 486, 494, 390, 398, 422, 430},
    {464, 472, 496, 504, 400, 408, 432, 440, 466, 474, 498, 506, 402, 410, 434, 442, 468, 476, 500, 508, 404, 412, 436, 444, 470, 478, 502, 510, 406, 414, 438, 446},
    {385, 393, 417, 425, 449, 457, 481, 489, 387, 395, 419, 427, 451, 459, 483, 491, 389, 397, 421, 429, 453, 461, 485, 493, 391, 399, 423, 431, 455, 463, 487, 495},
    {401, 409, 433, 441, 465, 473, 497, 505, 403, 411, 435, 443, 467, 475, 499, 507, 405, 413, 437, 445, 469, 477, 501, 509, 407, 415, 439, 447, 471, 479, 503, 511}};

// Byte address (or nibble index for T4) of a pixel. bp in 256-byte blocks; bw in units of 64 pixels.
inline uint32_t addrCT32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) {
    const uint32_t page = (y >> 5) * bw + (x >> 6);
    const uint32_t block = kBlockC32[(y >> 3) & 3][(x >> 3) & 7];
    return bp * 256 + page * 8192 + block * 256 + kCol32[y & 7][x & 7] * 4;
}
inline uint32_t addrCT16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) {
    const uint32_t page = (y >> 6) * bw + (x >> 6);
    const uint32_t block = kBlockC16[(y >> 3) & 7][(x >> 4) & 3];
    return bp * 256 + page * 8192 + block * 256 + kCol16[y & 7][x & 15] * 2;
}
inline uint32_t addrT8(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) {
    const uint32_t pw = bw >> 1 ? bw >> 1 : 1;
    const uint32_t page = (y >> 6) * pw + (x >> 7);
    const uint32_t block = kBlockP8[(y >> 4) & 3][(x >> 4) & 7];
    return bp * 256 + page * 8192 + block * 256 + kCol8[y & 15][x & 15];
}
inline uint32_t nibT4(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) {
    const uint32_t pw = bw >> 1 ? bw >> 1 : 1;
    const uint32_t page = (y >> 7) * pw + (x >> 7);
    const uint32_t block = kBlockP4[(y >> 4) & 7][(x >> 5) & 3];
    return (bp * 256 + page * 8192 + block * 256) * 2 + kCol4[y & 15][x & 31];
}

inline uint32_t rd32(const Vram &v, uint32_t a) { uint32_t r = 0; if (a + 4 <= v.size) { r = v.p[a] | (v.p[a + 1] << 8) | (v.p[a + 2] << 16) | (uint32_t(v.p[a + 3]) << 24); } return r; }
inline uint32_t rd16(const Vram &v, uint32_t a) { return a + 2 <= v.size ? uint32_t(v.p[a] | (v.p[a + 1] << 8)) : 0; }
inline uint32_t rd8(const Vram &v, uint32_t a) { return a < v.size ? v.p[a] : 0; }

// GS RGBA8 (r in byte 0) -> RGBA with alpha in 0..255 (the GS uses 0x80 = 1.0)
inline uint32_t fixAlpha(uint32_t c) {
    uint32_t a = (c >> 24) & 0xFF;
    a = a >= 128 ? 255 : a * 2;
    return (c & 0x00FFFFFF) | (a << 24);
}
inline uint32_t from16(uint32_t h) {   // A1B5G5R5
    const uint32_t r = (h & 31) * 255 / 31, g = ((h >> 5) & 31) * 255 / 31, b = ((h >> 10) & 31) * 255 / 31;
    return r | (g << 8) | (b << 16) | ((h & 0x8000) ? 0xFF000000u : 0u);
}
// Raw GS alpha (0x80 = 1.0, up to 0xFF ~ 2.0), for code that blends like the GS (As = At * Av / 16384, Scene::draw's shader):
// CT32 as is; CT24 and bit 1 of CT16 give 0x80 (TA0/TA1 = 1.0), bit 0 of CT16 gives 0.
inline uint32_t from16Raw(uint32_t h) { return (from16(h) & 0x00FFFFFFu) | ((h & 0x8000) ? 0x80000000u : 0u); }

struct Tex0 {
    uint32_t tbp0, tbw, psm, tw, th, tcc, tfx, cbp, cpsm, csm, csa;
    static Tex0 decode(uint64_t v) {
        Tex0 t;
        t.tbp0 = uint32_t(v & 0x3FFF); t.tbw = uint32_t((v >> 14) & 0x3F); t.psm = uint32_t((v >> 20) & 0x3F);
        t.tw = uint32_t((v >> 26) & 0xF); t.th = uint32_t((v >> 30) & 0xF); t.tcc = uint32_t((v >> 34) & 1);
        t.tfx = uint32_t((v >> 35) & 3); t.cbp = uint32_t((v >> 37) & 0x3FFF); t.cpsm = uint32_t((v >> 51) & 0xF);
        t.csm = uint32_t((v >> 55) & 1); t.csa = uint32_t((v >> 56) & 0x1F);
        return t;
    }
};

// 256-entry palette (CSM1) of an indexed TEX0, read from VRAM: it sits in linear memory with entries 8..15 and 16..23
// of each group of 32 swapped.
inline void readPalette(const Vram &v, const Tex0 &t, uint32_t pal[256]) {
    for (uint32_t i = 0; i < 256; ++i) {
        const uint32_t k = (i & 0xE7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1);
        if (t.cpsm == 0 || t.cpsm == 1) pal[i] = fixAlpha(rd32(v, t.cbp * 256 + k * 4));
        else pal[i] = from16(rd16(v, t.cbp * 256 + k * 2));
    }
}

// Decodes pixels from a linear upload (the order the `.TEX` stores them in: rows of `w` pixels, T4 with the low nibble first).
// rawAlpha: raw GS alpha for the direct formats (see from16Raw); the palette comes ready from the caller.
inline bool decodeUpload(const uint8_t *pix, size_t avail, uint32_t w, uint32_t h, uint32_t psm, uint32_t csa,
                         const uint32_t pal[256], std::vector<uint32_t> &out, bool rawAlpha = false) {
    if (w == 0 || h == 0 || w > 1024 || h > 1024) return false;
    out.assign(size_t(w) * h, 0);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const size_t n = size_t(y) * w + x;
            uint32_t c = 0;
            switch (psm) {
            case T8: if (n >= avail) return false; c = pal[pix[n]]; break;
            case T4: { if (n / 2 >= avail) return false; const uint32_t b = pix[n / 2]; c = pal[(csa * 16 + ((n & 1) ? (b >> 4) : (b & 15))) & 255]; break; }
            case CT32: {
                if (n * 4 + 4 > avail) return false;
                c = uint32_t(pix[n * 4] | (pix[n * 4 + 1] << 8) | (pix[n * 4 + 2] << 16) | (uint32_t(pix[n * 4 + 3]) << 24));
                if (!rawAlpha) c = fixAlpha(c);
                break;
            }
            case CT24: { if (n * 4 + 4 > avail) return false; c = uint32_t(pix[n * 4] | (pix[n * 4 + 1] << 8) | (pix[n * 4 + 2] << 16)) | (rawAlpha ? 0x80000000u : 0xFF000000u); break; }
            case CT16: case CT16S: {
                if (n * 2 + 2 > avail) return false;
                const uint32_t v = uint32_t(pix[n * 2] | (pix[n * 2 + 1] << 8));
                c = rawAlpha ? from16Raw(v) : from16(v);
                break;
            }
            default: return false;
            }
            out[n] = c;
        }
    return true;
}

// Decodes the texture to RGBA8 (r,g,b,a in consecutive bytes). Returns false if the format isn't supported.
inline bool decodeTexture(const Vram &v, uint64_t tex0, std::vector<uint32_t> &out, uint32_t &w, uint32_t &h) {
    if (!v.ok()) return false;
    const Tex0 t = Tex0::decode(tex0);
    w = 1u << t.tw; h = 1u << t.th;
    if (w > 1024 || h > 1024) return false;
    out.assign(size_t(w) * h, 0);
    uint32_t pal[256];
    const bool indexed = t.psm == T8 || t.psm == T4 || t.psm == T8H || t.psm == T4HL || t.psm == T4HH;
    if (indexed) {
        // CSM1: the 256-entry palette is in linear memory with entries 8-15 and 16-23 of each group of 32 swapped
        const uint32_t n = (t.psm == T4 || t.psm == T4HL || t.psm == T4HH) ? 16 : 256;
        for (uint32_t i = 0; i < 256; ++i) {
            const uint32_t k = (i & 0xE7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1);
            if (t.cpsm == 0 || t.cpsm == 1) pal[i] = fixAlpha(rd32(v, t.cbp * 256 + k * 4));
            else pal[i] = from16(rd16(v, t.cbp * 256 + k * 2));
        }
        (void)n;
    }
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t c = 0;
            switch (t.psm) {
            case CT32: c = fixAlpha(rd32(v, addrCT32(t.tbp0, t.tbw, x, y))); break;
            case CT24: c = rd32(v, addrCT32(t.tbp0, t.tbw, x, y)) | 0xFF000000u; break;
            case CT16: case CT16S: c = from16(rd16(v, addrCT16(t.tbp0, t.tbw, x, y))); break;
            case T8: c = pal[rd8(v, addrT8(t.tbp0, t.tbw, x, y))]; break;
            case T4: {
                const uint32_t n = nibT4(t.tbp0, t.tbw, x, y);
                const uint32_t b = rd8(v, n >> 1);
                c = pal[(t.csa * 16 + ((n & 1) ? (b >> 4) : (b & 15))) & 255];
                break;
            }
            default: return false;
            }
            out[size_t(y) * w + x] = c;
        }
    return true;
}

}  // namespace gs
}  // namespace wotm
