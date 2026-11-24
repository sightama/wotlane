// ABOUTME: Edge cases of the native VU0 arithmetic that recorded calls rarely reach (a madd past the largest float saturates, as the
// ABOUTME: VU does) and the thread's floating-point mode it needs. Includes wotm_vu0_native.cpp; vu0bench.sh runs it native and wasm.
#include "wotm_vu0_native.cpp"
#include <cfenv>
#include <cstdio>

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
    // a thread left in `mode` (and flushing subnormals when asked): the guard of the next code generation's lookup puts it back
    int modeIsRestored(int mode, bool flush, const char *what, uint64_t generation)
    {
        std::fesetround(mode);
        if (flush)
            _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        const bool changed = !ieeeDefaultsHold();
        static const uint8_t code[4096] = {};
        wotmVu0NativeLookup(code, sizeof code, generation, 0);
        const bool ok = changed && ieeeDefaultsHold();
        std::printf("%s a thread left %s is set back to round to nearest with subnormals\n", ok ? "ok  " : "FAIL", what);
        return !ok;
    }
#endif
}

int main()
{
    int bad = 0;
    // a madd's float product, or its sum, rounding past the largest float: the largest float of its sign, never an infinity. With the
    // product saturated first, -MAX + 2*MAX gives 0 while the flags (from the exact value) say a normal result, as round toward zero did
    bad += check(-FLT_MAX, FLT_MAX, 2.0f, false, 0.0f, "product past the largest float, accumulator of the other sign");
    bad += check(FLT_MAX, -FLT_MAX, 2.0f, false, 0.0f, "negative product past the largest float");
    bad += check(FLT_MAX, std::nextafter(FLT_MAX, 0.f), 1.0f + 0x1p-23f, false, FLT_MAX, "sum rounds up past the largest float");
    bad += check(-FLT_MAX, std::nextafter(FLT_MAX, 0.f), 1.0f + 0x1p-23f, true, -FLT_MAX, "msub: sum rounds down past -largest");
    bad += check(FLT_MAX, FLT_MAX, 2.0f, true, 0.0f, "msub: product past the largest float");
#if !defined(__EMSCRIPTEN__)
    bad += modeIsRestored(FE_TOWARDZERO, true, "rounding toward zero and flushing subnormals", 0x5eed1);
    bad += modeIsRestored(FE_UPWARD, false, "rounding upward", 0x5eed2);
    bad += modeIsRestored(FE_DOWNWARD, false, "rounding downward", 0x5eed3);
#endif
    std::printf("%d failure(s)\n", bad);
    return bad ? 1 : 0;
}
