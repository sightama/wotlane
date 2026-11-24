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

    // the same within one code generation: the periodic check (every 4096 lookups) puts the mode back
    int modeIsRestoredWithoutNewGeneration()
    {
        static const uint8_t code[4096] = {};
        wotmVu0NativeLookup(code, sizeof code, 0x5eed9, 0);   // the generation is now current
        std::fesetround(FE_TOWARDZERO);
        bool ok = !ieeeDefaultsHold();
        const std::string log = stderrOf([&] {
            for (int k = 0; k < 4096; ++k)
                wotmVu0NativeLookup(code, sizeof code, 0x5eed9, 0);
        });
        ok = ok && ieeeDefaultsHold() && log == kSetBack;
        std::printf("%s within one code generation, a changed mode is set back within 4096 lookups\n", ok ? "ok  " : "FAIL");
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
    // saturated first, -MAX + 2*MAX gives 0 while the flags (from the exact value) say a normal result, as round toward zero did
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
#if !defined(__EMSCRIPTEN__)
    bad += modeIsRestored(FE_TOWARDZERO, true, "rounding toward zero and flushing subnormals", 0x5eed1);
    bad += modeIsRestored(FE_UPWARD, false, "rounding upward", 0x5eed2);
    bad += modeIsRestored(FE_DOWNWARD, false, "rounding downward", 0x5eed3);
    bad += modeIsRestoredWithoutNewGeneration();
#endif
    std::printf("%d failure(s)\n", bad);
    return bad ? 1 : 0;
}
