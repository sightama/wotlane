// ABOUTME: Edge cases of the native VU0 arithmetic that recorded calls rarely reach: no lane may be left at infinity (the VU saturates
// ABOUTME: to the largest float). Includes wotm_vu0_native.cpp for its helpers; built and run, native and wasm, by tools/vu0bench.sh.
#include "wotm_vu0_native.cpp"
#include <cstdio>

static int check(float c, float a, float b, bool sub, const char *what)
{
    Fm f;
    float out[4];
    _mm_storeu_ps(out, MADD4<15>(_mm_set1_ps(c), _mm_set1_ps(a), _mm_set1_ps(b), f, sub));
    const bool bad = std::isinf(out[0]);
    std::printf("%s %s: %a (mac %x status %x)\n", bad ? "FAIL" : "ok  ", what, out[0], f.mac, f.cur);
    return bad;
}

int main()
{
    int bad = 0;
    bad += check(-FLT_MAX, FLT_MAX, 2.0f, false, "product past the largest float, accumulator of the other sign");
    bad += check(FLT_MAX, std::nextafter(FLT_MAX, 0.f), 1.0f + 0x1p-23f, false, "sum rounds up past the largest float");
    bad += check(FLT_MAX, FLT_MAX, 2.0f, true, "msub: product past the largest float");
    std::printf("%d failure(s)\n", bad);
    return bad ? 1 : 0;
}
