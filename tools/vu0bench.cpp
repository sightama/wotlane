// ABOUTME: Replays recorded VU0 microprogram calls through the native VU0 code and checks that every build (native, wasm) gives the
// ABOUTME: same bits on them; with repeats it also times each microprogram. Built and run by tools/vu0bench.sh.
// The samples come from PS2X_DUMP_VU0 (vu0_samples.bin + vu0_code.bin, written by the VU0 interpreter in the ps2EntryRunner): each
// is a call's input state and the interpreter's result. The native microprograms (native/runtime/wotm_vu0_native.cpp + the generated
// .inc) round to nearest and the interpreter rounds toward zero like the PS2, so the two differ in the last bit of many results; that
// count is printed for information only. What must hold is that every build of the native code gives the same result as every other:
// --save writes this build's results, --expect compares this build's results with a file another build saved, field by field.
// Usage: vu0bench <vu0_code.bin> <vu0_samples.bin> [repeats] [--save <file> | --expect <file>]
// With repeats, every sample then runs that many more times and the mean time per call of each microprogram is printed (to compare
// two versions of the native code on the same machine).
#include "wotm_vu0_native.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
    // The parts of a call's result the check compares (registers, flags, cycles and VU0 data), as bytes: equal bytes are equal bits
    std::vector<uint8_t> resultBytes(const VU1State &s, const std::vector<uint8_t> &data)
    {
        std::vector<uint8_t> out;
        auto put = [&out](const void *p, size_t n) {
            const auto *b = static_cast<const uint8_t *>(p);
            out.insert(out.end(), b, b + n);
        };
        put(s.vf, sizeof s.vf), put(s.vi, sizeof s.vi), put(s.acc, sizeof s.acc), put(&s.q, 4), put(&s.p, 4), put(&s.i, 4);
        put(&s.r, 4), put(&s.pc, 4), put(&s.mac, 4), put(&s.clip, 4), put(&s.status, 4), put(&s.cycles, 8);
        const uint32_t n = static_cast<uint32_t>(data.size());
        put(&n, 4), put(data.data(), data.size());
        return out;
    }

    void printDifference(const char *what, uint32_t pc, long sample, const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
    {
        std::printf("pc=0x%x sample %ld differs from %s", pc, sample, what);
        if (a.size() != b.size())
            return (void)std::printf(": result size %zu/%zu\n", a.size(), b.size());
        std::printf("\n");
        for (size_t r = 0; r < 32; ++r)
        {
            float x[4], y[4];
            std::memcpy(x, a.data() + 16 * r, 16);
            std::memcpy(y, b.data() + 16 * r, 16);
            if (std::memcmp(x, y, 16))
                std::printf("   vf%02zu this %a %a %a %a | %s %a %a %a %a\n", r, x[0], x[1], x[2], x[3], what, y[0], y[1], y[2], y[3]);
        }
        for (size_t k = 512; k < a.size(); ++k)
            if (a[k] != b[k])
                return (void)std::printf("   first other difference at result byte %zu (past the vf registers)\n", k);
    }
}

int main(int argc, char **argv)
{
    const char *usage = "usage: vu0bench <vu0_code.bin> <vu0_samples.bin> [repeats] [--save <file> | --expect <file>]\n";
    if (argc < 3) { std::fprintf(stderr, "%s", usage); return 2; }
    long repeats = 0;
    std::string savePath, expectPath;
    for (int k = 3; k < argc; ++k)
    {
        if (!std::strcmp(argv[k], "--save") && k + 1 < argc)
            savePath = argv[++k];
        else if (!std::strcmp(argv[k], "--expect") && k + 1 < argc)
            expectPath = argv[++k];
        else if (argv[k][0] != '-')
            repeats = std::atol(argv[k]);
        else { std::fprintf(stderr, "%s", usage); return 2; }
    }
    if (!savePath.empty() && !expectPath.empty()) { std::fprintf(stderr, "%s", usage); return 2; }
    struct Sample { uint32_t pc; WotmVu0Fn fn; VU1State in; std::vector<uint8_t> data; };
    std::vector<Sample> kept;
    std::vector<uint8_t> code(4096);
    FILE *cf = std::fopen(argv[1], "rb");
    if (!cf || std::fread(code.data(), 1, code.size(), cf) != code.size()) { std::fprintf(stderr, "bad code file\n"); return 2; }
    std::fclose(cf);
    FILE *sf = std::fopen(argv[2], "rb");
    if (!sf) { std::fprintf(stderr, "bad samples file\n"); return 2; }
    FILE *save = savePath.empty() ? nullptr : std::fopen(savePath.c_str(), "wb");
    FILE *expect = expectPath.empty() ? nullptr : std::fopen(expectPath.c_str(), "rb");
    if ((!savePath.empty() && !save) || (!expectPath.empty() && !expect)) { std::fprintf(stderr, "can't open the results file\n"); return 2; }
    uint32_t hdr[3];
    long n = 0, noNative = 0, offInterp = 0, offExpected = 0, missing = 0;
    struct Count { long samples = 0, offInterp = 0, offExpected = 0; };
    std::map<uint32_t, Count> perPc;
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
        if (repeats > 0)
            kept.push_back({hdr[0], fn, in, din});
        VU1State s = in;
        wotmVu0NativeRun(fn, s, din.data());
        const std::vector<uint8_t> mine = resultBytes(s, din);
        Count &pc = perPc[hdr[0]];
        ++pc.samples;
        if (mine != resultBytes(out, dout))
            ++pc.offInterp, ++offInterp;
        if (save)
        {
            const uint32_t size = static_cast<uint32_t>(mine.size());
            std::fwrite(&size, 4, 1, save);
            std::fwrite(mine.data(), 1, mine.size(), save);
        }
        if (expect)
        {
            uint32_t size = 0;
            std::vector<uint8_t> theirs;
            if (std::fread(&size, 4, 1, expect) == 1 && size <= (1u << 20))
            {
                theirs.resize(size);
                if (std::fread(theirs.data(), 1, size, expect) != size)
                    theirs.clear();
            }
            if (theirs.empty())
                ++missing;
            else if (mine != theirs)
            {
                ++pc.offExpected;
                if (++offExpected <= 8)
                    printDifference("the expected build", hdr[0], n, mine, theirs);
            }
        }
    }
    if (save)
        std::fclose(save);
    for (auto &[pc, c] : perPc)
        std::printf("pc 0x%03x: %ld samples, %ld differ from the interpreter%s\n", pc, c.samples, c.offInterp,
                    expect ? (", " + std::to_string(c.offExpected) + " from the expected build").c_str() : "");
    if (repeats > 0)
    {
        std::map<uint32_t, std::pair<double, long>> timed;   // pc -> (seconds, calls)
        for (Sample &k : kept)
        {
            std::vector<uint8_t> data = k.data;
            const auto t0 = std::chrono::steady_clock::now();
            for (long r = 0; r < repeats; ++r)
            {
                VU1State s = k.in;
                std::memcpy(data.data(), k.data.data(), data.size());
                wotmVu0NativeRun(k.fn, s, data.data());
            }
            const auto t1 = std::chrono::steady_clock::now();
            // the same copies without the program, taken off the time
            volatile uint8_t sink = 0;
            for (long r = 0; r < repeats; ++r)
            {
                VU1State s = k.in;
                std::memcpy(data.data(), k.data.data(), data.size());
                sink = sink + data[r % data.size()] + (uint8_t)s.vi[r & 15];
            }
            const auto t2 = std::chrono::steady_clock::now();
            auto &t = timed[k.pc];
            t.first += std::chrono::duration<double>((t1 - t0) - (t2 - t1)).count();
            t.second += repeats;
        }
        double all = 0;
        long calls = 0;
        for (auto &[pc, t] : timed)
        {
            std::printf("pc 0x%03x: %.1f ns per call\n", pc, 1e9 * t.first / t.second);
            all += t.first, calls += t.second;
        }
        std::printf("time: %.1f ns per call over %ld calls (the copies of each sample's state and data taken off)\n", 1e9 * all / calls, calls);
    }
    std::printf("total: %ld samples, %ld without a native program, %ld differ from the interpreter (it rounds toward zero, like the PS2)",
                n, noNative, offInterp);
    bool differs = false;
    if (expect)
    {
        uint32_t extra;
        const bool longer = std::fread(&extra, 4, 1, expect) == 1;
        std::printf(", %ld differ from the expected build%s%s", offExpected, missing ? " (its file ended early)" : "",
                    longer ? " (its file has more samples)" : "");
        differs = offExpected || missing || longer;
    }
    std::printf("\n");
    const bool empty = n == noNative;
    if (empty)
        std::printf("error: no sample ran through a native program, so nothing was checked\n");
    return differs || empty ? 1 : 0;
}
