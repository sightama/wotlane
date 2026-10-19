// Microprogramas do VU0 do WotM em C++ nativo, no lugar do interpretador (VU1Interpreter, modelo de pipeline exato).
// O corpo de cada programa e gerado por tools/vu0native.py (repo WoTM) a partir do microcodigo no ELF e incluido abaixo;
// sem o .inc a tabela fica vazia e tudo segue no interpretador. Resultado identico bit a bit ao do interpretador
// (mesma aritmetica: operandos normalizados, arredondamento para zero, resultado classificado pelo valor em double, flags
// MAC/status no ciclo em que o pipeline as publicaria); a bancada vu0bench confere contra amostras reais e entradas
// perturbadas. Cada programa so roda se o hash dos seus bytes na memoria de codigo do VU0 bater com o do gerador.
#include "wotm_vu0_native.h"

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <immintrin.h>

// sem contracao em FMA nem reassociacao, mesmo se o arquivo cair num build com /fp:fast (o CMake ja pede /fp:precise)
#ifdef _MSC_VER
#pragma float_control(precise, on)
#pragma fp_contract(off)
#endif

namespace
{
    inline float N(float value)
    {
        uint32_t bits;
        std::memcpy(&bits, &value, 4);
        const uint32_t exponent = (bits >> 23) & 0xFFu;
        if (exponent == 0u)
            bits &= 0x80000000u;
        else if (exponent == 0xFFu)
            bits = (bits & 0x80000000u) | 0x7F7FFFFFu;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    inline float B(uint32_t bits)
    {
        float value;
        std::memcpy(&value, &bits, 4);
        return value;
    }

    // Flags de uma operacao FMAC: mac (ZSUO por lane), cur (ZSUO juntas), ext (sticky extra do produto em madd/msub)
    struct Fm
    {
        uint32_t mac = 0, cur = 0, ext = 0;
    };

    // Z=1 S=2 U=4 O=8 do valor exato; resultado como VU1Interpreter::normalizeFmacExactResult
    inline uint32_t classify(double exact, uint32_t &bits)
    {
        uint32_t flags = std::signbit(exact) ? 2u : 0u;
        const double magnitude = std::fabs(exact);
        bits = flags ? 0x80000000u : 0u;
        if (magnitude == 0.0)
            return flags | 1u;
        if (magnitude > (double)FLT_MAX)
        {
            bits |= 0x7F7FFFFFu;
            return flags | 8u;
        }
        if (magnitude < (double)FLT_MIN)
            return flags | 5u;
        return flags | 0x10u;   // normal: fica o resultado em float
    }

    // lane = 8 (x), 4 (y), 2 (z), 1 (w)
    inline float L(float result, double exact, uint32_t lane, Fm &f)
    {
        uint32_t bits;
        uint32_t flags = classify(exact, bits);
        if (!(flags & 0x10u))
            std::memcpy(&result, &bits, 4);
        flags &= 0xFu;
        f.cur |= flags;
        f.mac |= (flags & 1u ? lane : 0u) | (flags & 2u ? lane << 4 : 0u) | (flags & 4u ? lane << 8 : 0u) | (flags & 8u ? lane << 12 : 0u);
        return result;
    }

    // sticky do produto (calculateFmacProductSticky)
    inline void P(float a, float b, Fm &f)
    {
        uint32_t bits;
        f.ext |= classify((double)a * (double)b, bits) & 0xFu;
    }

    inline void commit(VU1State &s, const Fm &f)
    {
        s.mac = f.mac;
        s.status = (s.status & 0xFF0u) | f.cur | ((f.cur | f.ext) << 6);
    }

    // ---- As mesmas contas nas 4 lanes de uma vez (SSE para o float, AVX para o valor exato em double). D = lanes do dest com
    // x no bit 0; as lanes fora de D sao calculadas e descartadas.
    inline __m128 LD(const float *p) { return _mm_loadu_ps(p); }

    inline __m128 NV(__m128 v)
    {
        const __m128i b = _mm_castps_si128(v);
        const __m128i e = _mm_and_si128(b, _mm_set1_epi32(0x7F800000));
        const __m128i sign = _mm_and_si128(b, _mm_set1_epi32((int)0x80000000u));
        __m128i r = _mm_blendv_epi8(b, sign, _mm_cmpeq_epi32(e, _mm_setzero_si128()));
        r = _mm_blendv_epi8(r, _mm_or_si128(sign, _mm_set1_epi32(0x7F7FFFFF)), _mm_cmpeq_epi32(e, _mm_set1_epi32(0x7F800000)));
        return _mm_castsi128_ps(r);
    }

    // bit c (x = 0) -> bit da lane no MAC (x = 8)
    constexpr uint32_t kRev[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

    struct Cls
    {
        int z, s, u, o;   // mascaras por lane (x = bit 0), so dentro de D
    };

    template <int D>
    inline Cls classify4(__m256d exact)
    {
        const __m256d m = _mm256_andnot_pd(_mm256_set1_pd(-0.0), exact);
        Cls c;
        c.z = _mm256_movemask_pd(_mm256_cmp_pd(m, _mm256_setzero_pd(), _CMP_EQ_OQ)) & D;
        c.o = _mm256_movemask_pd(_mm256_cmp_pd(m, _mm256_set1_pd((double)FLT_MAX), _CMP_GT_OQ)) & D;
        c.u = _mm256_movemask_pd(_mm256_cmp_pd(m, _mm256_set1_pd((double)FLT_MIN), _CMP_LT_OQ)) & D & ~c.z;
        c.s = _mm256_movemask_pd(exact) & D;
        return c;
    }

    inline uint32_t flagsOf(const Cls &c)
    {
        return ((c.z | c.u) ? 1u : 0u) | (c.s ? 2u : 0u) | (c.u ? 4u : 0u) | (c.o ? 8u : 0u);
    }

    inline __m128i laneMask(int m)
    {
        return _mm_set_epi32(m & 8 ? -1 : 0, m & 4 ? -1 : 0, m & 2 ? -1 : 0, m & 1 ? -1 : 0);
    }

    // resultado como normalizeFmacExactResult (zero/underflow: so o sinal; overflow: +-max) e flags no Fm
    template <int D>
    inline __m128 fin4(__m128 r, __m256d exact, Fm &f)
    {
        const Cls c = classify4<D>(exact);
        f.cur |= flagsOf(c);
        f.mac |= kRev[c.z | c.u] | (kRev[c.s] << 4) | (kRev[c.u] << 8) | (kRev[c.o] << 12);
        if ((c.z | c.u | c.o) == 0)
            return r;
        const __m128i sign = _mm_and_si128(_mm_castps_si128(_mm256_cvtpd_ps(exact)), _mm_set1_epi32((int)0x80000000u));
        __m128i bits = _mm_blendv_epi8(_mm_castps_si128(r), sign, laneMask(c.z | c.u));
        bits = _mm_blendv_epi8(bits, _mm_or_si128(sign, _mm_set1_epi32(0x7F7FFFFF)), laneMask(c.o));
        return _mm_castsi128_ps(bits);
    }

    inline __m256d W(__m128 v) { return _mm256_cvtps_pd(v); }

    template <int D>
    inline __m128 ADD4(__m128 a, __m128 b, Fm &f) { return fin4<D>(_mm_add_ps(a, b), _mm256_add_pd(W(a), W(b)), f); }
    template <int D>
    inline __m128 SUB4(__m128 a, __m128 b, Fm &f) { return fin4<D>(_mm_sub_ps(a, b), _mm256_sub_pd(W(a), W(b)), f); }
    template <int D>
    inline __m128 MUL4(__m128 a, __m128 b, Fm &f) { return fin4<D>(_mm_mul_ps(a, b), _mm256_mul_pd(W(a), W(b)), f); }

    // madd/msub: acc +- a*b (dois arredondamentos no float), e o sticky do produto (calculateFmacProductSticky)
    template <int D>
    inline __m128 MADD4(__m128 c, __m128 a, __m128 b, Fm &f, bool sub)
    {
        const __m256d p = _mm256_mul_pd(W(a), W(b));
        const __m128 pf = _mm_mul_ps(a, b);
        const __m128 r = sub ? _mm_sub_ps(c, pf) : _mm_add_ps(c, pf);
        const __m256d e = sub ? _mm256_sub_pd(W(c), p) : _mm256_add_pd(W(c), p);
        f.ext |= flagsOf(classify4<D>(p));
        return fin4<D>(r, e, f);
    }

    // escreve as lanes de D (x = bit 0)
    template <int D>
    inline void ST(float *dst, __m128 v) { _mm_storeu_ps(dst, _mm_blend_ps(_mm_loadu_ps(dst), v, D)); }

    inline uint32_t addr16(int32_t vi, int32_t imm) { return ((uint32_t)(vi + imm) * 16u) & 0xFFFu; }

    // FTOI0 (vuFloatToInt com escala 1): satura no int32 e trunca
    inline int32_t F2I(float value)
    {
        const double scaled = (double)value;
        if (scaled >= 2147483647.0)
            return 2147483647;
        if (scaled <= -2147483648.0)
            return (int32_t)0x80000000u;
        return (int32_t)scaled;
    }

    inline void finish(VU1State &s, uint32_t pc, uint64_t cycles)
    {
        s.vf[0][0] = 0.0f;
        s.vf[0][1] = 0.0f;
        s.vf[0][2] = 0.0f;
        s.vf[0][3] = 1.0f;
        s.vi[0] = 0;
        s.pc = pc;
        s.cycles = cycles;
    }
}

#if __has_include("wotm_vu0_native.gen.inc")
#include "wotm_vu0_native.gen.inc"
#else
static const WotmVu0Program kPrograms[] = {{0, 0, 0, 0, nullptr}};
static constexpr size_t kProgramCount = 0;
#endif

const uint8_t *wotmVu0CodeImage()
{
#ifdef WOTM_VU0_HAVE_IMAGE
    return kVu0CodeImage;
#else
    return nullptr;
#endif
}

namespace
{
    uint64_t fnv1a(const uint8_t *p, uint32_t n)
    {
        uint64_t h = 0xcbf29ce484222325ull;
        for (uint32_t k = 0; k < n; ++k)
            h = (h ^ p[k]) * 0x100000001b3ull;
        return h;
    }
}

WotmVu0Fn wotmVu0NativeLookup(const uint8_t *vu0Code, uint32_t codeSize, uint64_t generation, uint32_t pc)
{
    static WotmVu0Fn s_byPc[512];
    static uint64_t s_generation = ~0ull;
    static const bool s_disabled = std::getenv("WOTM_VU0_INTERP") != nullptr;
    if (s_disabled || pc >= codeSize)
        return nullptr;
    if (generation != s_generation)
    {
        s_generation = generation;
        std::memset(s_byPc, 0, sizeof(s_byPc));
        for (size_t k = 0; k < kProgramCount; ++k)
        {
            const WotmVu0Program &p = kPrograms[k];
            if (p.hashStart + p.length <= codeSize && fnv1a(vu0Code + p.hashStart, p.length) == p.hash)
                s_byPc[(p.start >> 3) & 511u] = p.fn;
        }
    }
    return s_byPc[(pc >> 3) & 511u];
}

void wotmVu0NativeRun(WotmVu0Fn fn, VU1State &state, uint8_t *vu0Data)
{
    // arredondamento para zero, como o VU (e como VU1Interpreter::run): so o MXCSR, a conta e toda em SSE
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0x6000u);
    fn(state, vu0Data);
    _mm_setcsr(csr);
}
