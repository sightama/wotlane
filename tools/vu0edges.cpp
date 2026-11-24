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

    // the probes the runtime's guard uses, here from the test's side
    bool roundsToNearest()
    {
        volatile float one = 1.0f, step = 0x1.8p-24f;
        return one + step == 0x1.000002p+0f;
    }
    bool keepsSubnormals()
    {
        volatile float tiny = FLT_MIN, sub = 0x1p-140f;
        return tiny * 0.5f != 0.0f && sub * 2.0f != 0.0f;
    }
}

int main()
{
    int bad = 0;
    // a madd's float product, or its sum, rounding past the largest float: the largest float of its sign, never an infinity
    bad += check(-FLT_MAX, FLT_MAX, 2.0f, false, 0.0f, "product past the largest float, accumulator of the other sign");
    bad += check(FLT_MAX, -FLT_MAX, 2.0f, false, 0.0f, "negative product past the largest float");
    bad += check(FLT_MAX, std::nextafter(FLT_MAX, 0.f), 1.0f + 0x1p-23f, false, FLT_MAX, "sum rounds up past the largest float");
    bad += check(-FLT_MAX, std::nextafter(FLT_MAX, 0.f), 1.0f + 0x1p-23f, true, -FLT_MAX, "msub: sum rounds down past -largest");
    bad += check(FLT_MAX, FLT_MAX, 2.0f, true, 0.0f, "msub: product past the largest float");
#if !defined(__EMSCRIPTEN__)
    // a thread left rounding toward zero and flushing subnormals: the next lookup of a new code generation puts it back
    std::fesetround(FE_TOWARDZERO);
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    const bool changed = !roundsToNearest() && !keepsSubnormals();
    static const uint8_t code[4096] = {};
    wotmVu0NativeLookup(code, sizeof code, 0x5eed, 0);
    const bool restored = roundsToNearest() && keepsSubnormals();
    std::printf("%s a thread left in another mode is set back to round to nearest with subnormals\n", changed && restored ? "ok  " : "FAIL");
    bad += !(changed && restored);
#endif
    std::printf("%d failure(s)\n", bad);
    return bad ? 1 : 0;
}
