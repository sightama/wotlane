// ABOUTME: Unit tests of the native VU0 arithmetic's edge cases that recorded calls rarely reach (a madd past the largest float
// ABOUTME: saturates, as the VU does) and of the floating-point mode guard. Built with wotm_vu0_native.cpp by native/tests/CMakeLists.txt.
#include "wotm_vu0_native.cpp"
#include <cfenv>
#include <cstdio>
#include <string>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace
{
    uint32_t bitsOf(float f)
    {
        uint32_t u;
        std::memcpy(&u, &f, 4);
        return u;
    }

    // one madd (or msub) on all 4 lanes; the x lane must hold exactly `want`
    int check(float c, float a, float b, bool sub, float want, const char *what)
    {
        Fm f;
        float out[4];
        _mm_storeu_ps(out, MADD4<15>(_mm_set1_ps(c), _mm_set1_ps(a), _mm_set1_ps(b), f, sub));
        const bool bad = bitsOf(out[0]) != bitsOf(want);
        std::printf("%s %s: %a, want %a (mac %x status %x)\n", bad ? "FAIL" : "ok  ", what, out[0], want, f.mac, f.cur);
        return bad;
    }

    // Every arithmetic helper the generated programs use, on a grid of edge values (normal floats, as the programs see them after
    // normalizing: zeros, FLT_MIN, values around 1, ties near the largest float) in two lane masks, folded into one hash of every
    // result's bits and flags. It must be the same on every build (native x86 and ARM, wasm): CI compares it with kSweepDigest
    uint64_t sweepDigest()
    {
        const float g[] = {0.0f, -0.0f, FLT_MIN, -FLT_MIN, 0x1.000002p-126f, 1.0f, -1.0f, 0x1.000002p+0f, 0x1.fffffep-1f, 0.1f,
                           -0.75f, 3.0f, 1.5f, 0x1p103f, -0x1p103f, 3.0f * 0x1p50f, 5592409.0f * 0x1p53f, 0x1.fffffcp+127f, FLT_MAX,
                           -FLT_MAX, 16777203.0f * 0x1p103f, 12345.678f, -1e-30f, 1e30f};
        const size_t n = sizeof g / sizeof g[0];
        uint64_t h = 1469598103934665603ull;   // FNV-1a
        auto mix = [&h](uint32_t v) {
            for (int i = 0; i < 4; ++i)
                h = (h ^ ((v >> (8 * i)) & 0xFFu)) * 1099511628211ull;
        };
        auto vec = [&](size_t k) { return _mm_setr_ps(g[k % n], g[(k + 7) % n], g[(k + 13) % n], g[(k + 19) % n]); };
        auto out = [&](__m128 v, const Fm &f) {
            float o[4];
            _mm_storeu_ps(o, v);
            for (float x : o)
                mix(bitsOf(x));
            mix(f.mac), mix(f.cur), mix(f.ext);
        };
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j)
            {
                const __m128 a = vec(i), b = vec(j);
                { Fm f; out(ADD4<15>(a, b, f), f); }
                { Fm f; out(SUB4<15>(a, b, f), f); }
                { Fm f; out(MUL4<15>(a, b, f), f); }
                { Fm f; out(ADD4<5>(a, b, f), f); }
                { Fm f; out(MUL4<10>(a, b, f), f); }
                for (size_t k = 0; k < n; k += 3)
                {
                    { Fm f; out(MADD4<15>(vec(k), a, b, f, false), f); }
                    { Fm f; out(MADD4<15>(vec(k), a, b, f, true), f); }
                }
            }
        for (int32_t v : {0, 1, -1, 16777217, -16777219, 2147483647, static_cast<int32_t>(0x80000000u), 123456789})
            mix(bitsOf(I2F(v)));
        for (float x : g)
            mix(static_cast<uint32_t>(F2I(x)));
        return h;
    }
    constexpr uint64_t kSweepDigest = 0x0dd7492b4fc44e33ull;   // the Mac (ARM through sse2neon), wasm and x86 AVX2 (Rosetta) agree

#if !defined(__EMSCRIPTEN__)
    // the C runtime's descriptor calls, by their Windows or POSIX names
#if defined(_WIN32)
    int fdOf(FILE *f) { return _fileno(f); }
    int fdDup(int fd) { return _dup(fd); }
    int fdDup2(int from, int to) { return _dup2(from, to); }
    void fdClose(int fd) { _close(fd); }
#else
    int fdOf(FILE *f) { return fileno(f); }
    int fdDup(int fd) { return dup(fd); }
    int fdDup2(int from, int to) { return dup2(from, to); }
    void fdClose(int fd) { close(fd); }
#endif

    // what `f` writes to stderr (the guard's [vu0] lines), captured in a file next to the test instead of shown
    template <class F> std::string stderrOf(F f)
    {
        const char *path = "wotm_vu0_test_stderr.txt";
        std::fflush(stderr);
        FILE *file = std::fopen(path, "w+");
        if (!file)
            return "(stderr not captured: can't create the file in the current folder)";
        const int saved = fdDup(fdOf(stderr));
        if (saved < 0 || fdDup2(fdOf(file), fdOf(stderr)) < 0)
        {
            if (saved >= 0)
                fdClose(saved);
            std::fclose(file);
            std::remove(path);
            return "(stderr not captured: dup failed)";
        }
        f();
        std::fflush(stderr);
        fdDup2(saved, fdOf(stderr));
        fdClose(saved);
        std::string text;
        std::rewind(file);
        for (int c; (c = std::fgetc(file)) != EOF;)
            text += static_cast<char>(c);
        std::fclose(file);
        std::remove(path);
        return text;
    }
    const char *const kSetBack = "[vu0] this thread's floating point was not round to nearest with subnormals: set back\n";

    // a thread left in `mode` (and flushing subnormals when asked): the guard of the next code generation's lookup puts it back
    int modeIsRestored(int mode, bool flush, const char *what, uint64_t generation)
    {
        std::fesetround(mode);
        if (flush)
            _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        const bool changed = !ieeeDefaultsHold();
        static const uint8_t code[4096] = {};
        const std::string log = stderrOf([&] { wotmVu0NativeLookup(code, sizeof code, generation, 0); });
        const bool ok = changed && ieeeDefaultsHold() && log == kSetBack;
        std::printf("%s a thread left %s is set back to round to nearest with subnormals, with its [vu0] line\n", ok ? "ok  " : "FAIL", what);
        return !ok;
    }

    // the same within one code generation: the periodic check (every 256 lookups) puts the mode back
    int modeIsRestoredWithoutNewGeneration()
    {
        static const uint8_t code[4096] = {};
        wotmVu0NativeLookup(code, sizeof code, 0x5eed9, 0);   // the generation is now current
        std::fesetround(FE_TOWARDZERO);
        bool ok = !ieeeDefaultsHold();
        const std::string log = stderrOf([&] {
            for (int k = 0; k < 256; ++k)
                wotmVu0NativeLookup(code, sizeof code, 0x5eed9, 0);
        });
        ok = ok && ieeeDefaultsHold() && log == kSetBack;
        std::printf("%s within one code generation, a changed mode is set back within 256 lookups\n", ok ? "ok  " : "FAIL");
        return !ok;
    }
#endif
}

int main()
{
    int bad = 0;
#if defined(_WIN32)
    _putenv("WOTM_VU0_INTERP=");   // the lookup (and its guard) is off with it
#else
    unsetenv("WOTM_VU0_INTERP");
#endif
    // a madd's float product rounding past the largest float: the largest float of its sign, never an infinity. With the product
    // saturated first, -MAX + 2*MAX gives 0 while the flags (from the exact value) say a normal result
    bad += check(-FLT_MAX, FLT_MAX, 2.0f, false, 0.0f, "product past the largest float, accumulator of the other sign");
    bad += check(FLT_MAX, -FLT_MAX, 2.0f, false, 0.0f, "negative product past the largest float");
    bad += check(FLT_MAX, FLT_MAX, 2.0f, true, 0.0f, "msub: product past the largest float");
    // a sum rounding past the largest float: saturated too. Its exact value is usually past it as well (flagged as an overflow), but
    // not always: here a*b = 16777227 * 2^103 ties up to an even float, so the float sum ties up to infinity while the exact sum
    // c + a*b is exactly the largest float (no overflow flag)
    bad += check(FLT_MAX, 0x1p103f, 1.0f, false, FLT_MAX, "sum alone rounds up past the largest float (half an ulp, a tie)");
    bad += check(std::ldexp(16777203.0f, 103), 3.0f * 0x1p50f, std::ldexp(5592409.0f, 53), false, FLT_MAX,
                 "product ties up, then the sum ties up past the largest float while the exact sum is the largest float");
    bad += check(FLT_MAX, std::nextafter(FLT_MAX, 0.f), 1.0f + 0x1p-23f, false, FLT_MAX, "product and sum past the largest float");
    bad += check(-FLT_MAX, std::nextafter(FLT_MAX, 0.f), 1.0f + 0x1p-23f, true, -FLT_MAX, "msub: product and sum past -largest");
    {
        const uint64_t digest = sweepDigest();
        const bool ok = digest == kSweepDigest;
        std::printf("%s every helper on the edge-value grid gives the same bits as on every other build (digest %016llx, want "
                    "%016llx)\n", ok ? "ok  " : "FAIL", static_cast<unsigned long long>(digest),
                    static_cast<unsigned long long>(kSweepDigest));
        bad += !ok;
    }
    // the mode guard (WebAssembly has no rounding or flush mode to change, so these run natively only)
#if !defined(__EMSCRIPTEN__)
    bad += modeIsRestored(FE_TOWARDZERO, true, "rounding toward zero and flushing subnormals", 0x5eed1);
    bad += modeIsRestored(FE_UPWARD, false, "rounding upward", 0x5eed2);
    bad += modeIsRestored(FE_DOWNWARD, false, "rounding downward", 0x5eed3);
    bad += modeIsRestoredWithoutNewGeneration();
#endif
    std::printf("%d failure(s)\n", bad);
    return bad ? 1 : 0;
}
