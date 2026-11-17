// Replays the VU0 interpreter samples written by PS2X_DUMP_VU0 (vu0_samples.bin + vu0_code.bin, from the ps2EntryRunner) through the
// native VU0 microprograms (native/runtime/wotm_vu0_native.cpp + the generated .inc) and compares the result with the interpreter's,
// field by field (as the runtime's PS2X_VU0_VERIFY does). Proves the native VU0 is bit-exact on a new compiler/CPU (e.g. arm64 through
// sse2neon). Built and run by tools/vu0bench.sh. Usage: vu0bench <vu0_code.bin> <vu0_samples.bin>
#include "wotm_vu0_native.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

int main(int argc, char **argv)
{
    if (argc != 3) { std::fprintf(stderr, "usage: vu0bench <vu0_code.bin> <vu0_samples.bin>\n"); return 2; }
    std::vector<uint8_t> code(4096);
    FILE *cf = std::fopen(argv[1], "rb");
    if (!cf || std::fread(code.data(), 1, code.size(), cf) != code.size()) { std::fprintf(stderr, "bad code file\n"); return 2; }
    std::fclose(cf);
    FILE *sf = std::fopen(argv[2], "rb");
    if (!sf) { std::fprintf(stderr, "bad samples file\n"); return 2; }
    uint32_t hdr[3];
    long n = 0, bad = 0, noNative = 0;
    std::map<uint32_t, std::pair<long, long>> perPc;   // pc -> (samples, mismatches)
    while (std::fread(hdr, sizeof hdr, 1, sf) == 1)
    {
        if (hdr[1] != sizeof(VU1State)) { std::fprintf(stderr, "VU1State size %u in file, %zu here\n", hdr[1], sizeof(VU1State)); return 2; }
        VU1State in{}, out{};
        std::vector<uint8_t> din(hdr[2]), dout(hdr[2]);
        if (std::fread(&in, sizeof in, 1, sf) != 1 || std::fread(&out, sizeof out, 1, sf) != 1 ||
            std::fread(din.data(), 1, din.size(), sf) != din.size() || std::fread(dout.data(), 1, dout.size(), sf) != dout.size())
            break;
        ++n;
        WotmVu0Fn fn = wotmVu0NativeLookup(code.data(), (uint32_t)code.size(), 1, hdr[0]);
        if (!fn) { ++noNative; continue; }
        VU1State s = in;
        wotmVu0NativeRun(fn, s, din.data());
        const VU1State &a = s, &b = out;
        const bool same = std::memcmp(a.vf, b.vf, sizeof(a.vf)) == 0 && std::memcmp(a.vi, b.vi, sizeof(a.vi)) == 0 &&
                          std::memcmp(a.acc, b.acc, sizeof(a.acc)) == 0 && std::memcmp(&a.q, &b.q, 4) == 0 && std::memcmp(&a.p, &b.p, 4) == 0 &&
                          std::memcmp(&a.i, &b.i, 4) == 0 && a.r == b.r && a.pc == b.pc && a.mac == b.mac && a.clip == b.clip &&
                          a.status == b.status && a.cycles == b.cycles && din == dout;
        auto &pc = perPc[hdr[0]];
        ++pc.first;
        if (!same)
        {
            ++pc.second;
            if (++bad <= 8)
            {
                std::printf("pc=0x%x sample %ld differs: mac %x/%x clip %x/%x status %x/%x cycles %llu/%llu data %s\n", hdr[0], n, a.mac, b.mac,
                            a.clip, b.clip, a.status, b.status, (unsigned long long)a.cycles, (unsigned long long)b.cycles, din == dout ? "same" : "DIFF");
                for (int r = 0; r < 32; ++r)
                    if (std::memcmp(a.vf[r], b.vf[r], 16))
                        std::printf("   vf%02d native %a %a %a %a | interp %a %a %a %a\n", r, a.vf[r][0], a.vf[r][1], a.vf[r][2], a.vf[r][3], b.vf[r][0],
                                    b.vf[r][1], b.vf[r][2], b.vf[r][3]);
            }
        }
    }
    for (auto &[pc, c] : perPc)
        std::printf("pc 0x%03x: %ld samples, %ld mismatches\n", pc, c.first, c.second);
    std::printf("total: %ld samples, %ld without a native program, %ld mismatches\n", n, noNative, bad);
    return bad ? 1 : 0;
}
