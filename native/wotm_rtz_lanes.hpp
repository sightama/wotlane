// ABOUTME: Round-toward-zero arithmetic of wotm_rtz.hpp on 2 doubles or 4 floats at a time (SSE intrinsics, which Emscripten maps
// ABOUTME: onto wasm SIMD): the browser's VU0 microprograms (runtime/wotm_vu0_native.cpp) get the same bits as one value at a time.
#pragma once

#if defined(USE_SSE2NEON)   // ARM: the unit tests build these with sse2neon, as the runtime builds the game's SSE code
#include "sse2neon.h"
#else
#include <immintrin.h>
#endif

// The same method as wotm_rtz.hpp: the nearest result, then one step toward zero in the lanes where it lies farther from zero than
// the exact one. Stepping toward zero is subtracting 1 from the bits, so a lane's all-ones compare mask (-1) is added to them.
// Include it only from files built with -ffp-contract=off and without fast math, with round to nearest in effect.
namespace wotm::rtz
{
    // a + b per double lane (TwoSum: the nearest sum and its exact error; an error of the sign opposite to the sum's means the
    // nearest sum went past the exact one, away from zero)
    inline __m128d addLanes(__m128d a, __m128d b)
    {
        const __m128d sum = _mm_add_pd(a, b);
        const __m128d bPart = _mm_sub_pd(sum, a);
        const __m128d error = _mm_add_pd(_mm_sub_pd(a, _mm_sub_pd(sum, bPart)), _mm_sub_pd(b, bPart));
        const __m128i sign = _mm_set1_epi64x((long long)0x8000000000000000ull);
        const __m128i opposite = _mm_cmpeq_epi64(_mm_and_si128(_mm_castpd_si128(_mm_xor_pd(error, sum)), sign), sign);
        const __m128i past = _mm_and_si128(opposite, _mm_castpd_si128(_mm_cmpneq_pd(error, _mm_setzero_pd())));
        return _mm_castsi128_pd(_mm_add_epi64(_mm_castpd_si128(sum), past));
    }
    inline __m128d subLanes(__m128d a, __m128d b) { return addLanes(a, _mm_xor_pd(b, _mm_set1_pd(-0.0))); }

    // 4 floats from 4 doubles (x, y in lo; z, w in hi), each truncated; past the largest float, the largest float
    inline __m128 toFloatLanes(__m128d lo, __m128d hi)
    {
        const __m128 nearest = _mm_movelh_ps(_mm_cvtpd_ps(lo), _mm_cvtpd_ps(hi));
        const __m128d sign = _mm_set1_pd(-0.0);
        const __m128d backLo = _mm_cvtps_pd(nearest), backHi = _mm_cvtps_pd(_mm_movehl_ps(nearest, nearest));
        const __m128d fartherLo = _mm_cmpgt_pd(_mm_andnot_pd(sign, backLo), _mm_andnot_pd(sign, lo));
        const __m128d fartherHi = _mm_cmpgt_pd(_mm_andnot_pd(sign, backHi), _mm_andnot_pd(sign, hi));
        const __m128i farther =
            _mm_castps_si128(_mm_shuffle_ps(_mm_castpd_ps(fartherLo), _mm_castpd_ps(fartherHi), _MM_SHUFFLE(2, 0, 2, 0)));
        return _mm_castsi128_ps(_mm_add_epi32(_mm_castps_si128(nearest), farther));
    }
}
