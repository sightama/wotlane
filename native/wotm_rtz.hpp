// ABOUTME: Float and double arithmetic rounded toward zero in software, for WebAssembly (it has no rounding-mode control): the VU0
// ABOUTME: microprograms (runtime/wotm_vu0_native.cpp) must give the bits the native builds get with the CPU set to round toward zero.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

// Each function returns the IEEE result in round-toward-zero mode using only round-to-nearest operations: compute the nearest result,
// and when it lies farther from zero than the exact one, take the next value toward zero. Truncation nests (truncating to double,
// then to float, is truncating to float), so the float operations go through the exact or truncated double.
// It needs every operation rounded on its own: include it only from files built with -ffp-contract=off and without fast math.
namespace wotm::rtz
{
    // the next value toward zero (x nonzero; from infinity, the largest finite value)
    inline double stepTowardZero(double x)
    {
        uint64_t bits;
        std::memcpy(&bits, &x, 8);
        --bits;
        std::memcpy(&x, &bits, 8);
        return x;
    }
    inline float stepTowardZero(float x)
    {
        uint32_t bits;
        std::memcpy(&bits, &x, 4);
        --bits;
        std::memcpy(&x, &bits, 4);
        return x;
    }

    // a + b: the nearest sum and its exact error (TwoSum; no overflow is possible for the values the VU0 code adds, at most about
    // 2^256). An error of the opposite sign means the nearest sum went past the exact one, away from zero.
    inline double add(double a, double b)
    {
        const double sum = a + b;
        const double bPart = sum - a;
        const double error = (a - (sum - bPart)) + (b - bPart);
        return error != 0.0 && std::signbit(error) != std::signbit(sum) ? stepTowardZero(sum) : sum;
    }
    inline double sub(double a, double b) { return add(a, -b); }

    inline float toFloat(double x)
    {
        const float nearest = (float)x;
        return std::fabs((double)nearest) > std::fabs(x) ? stepTowardZero(nearest) : nearest;
    }

    inline float add(float a, float b) { return toFloat(add((double)a, (double)b)); }
    inline float sub(float a, float b) { return toFloat(sub((double)a, (double)b)); }
    inline float mul(float a, float b) { return toFloat((double)a * (double)b); }   // the product of two floats is exact in double
    inline float fromInt(int32_t i) { return toFloat((double)i); }
}
