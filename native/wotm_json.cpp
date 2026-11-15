// ABOUTME: The float parts of wotm_json.hpp (decimal text to double and back), compiled on their own with strict float rules
// ABOUTME: (like wotm_detmath.cpp) so every build and every machine reads and writes the same bits.
#include "wotm_json.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace wotm::json
{
    Value Value::number(double v)
    {
        Value r;
        r.type = Int;
        r.num = v;
        r.frac = true;
        r.i = std::isnan(v) ? 0 : v >= 9.2e18 ? INT64_MAX : v <= -9.2e18 ? INT64_MIN : static_cast<int64_t>(v);
        return r;
    }

    namespace detail
    {
        double decimal(uint64_t digits, int exp10)
        {
            static const double kPow10[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
                                            1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
            double v = static_cast<double>(digits);
            if (digits == 0)
                return 0;
            if (digits < (1ull << 53) && exp10 >= -22 && exp10 <= 22)
                return exp10 >= 0 ? v * kPow10[exp10] : v / kPow10[-exp10];
            for (; exp10 > 0 && !std::isinf(v); --exp10)
                v *= 10;
            for (; exp10 < 0 && v != 0; ++exp10)
                v /= 10;
            return v;
        }

        namespace
        {
            void dots(char *b)
            {
                for (char *c = b; *c; ++c)
                    if (*c == ',')
                        *c = '.';
            }

            bool readsBack(const char *b, double d)
            {
                const std::string t = b;
                Parser ps{t.data(), t.data() + t.size(), {}};
                Value back;
                if (!ps.value(back, 0) || ps.p != ps.end)
                    return false;
                const double v = back.asNum();
                return v == d && std::signbit(v) == std::signbit(d);
            }
        }

        void number(std::string &out, double d)
        {
            if (!std::isfinite(d))
            {
                out += "null";   // JSON has no inf or nan
                return;
            }
            if (d == 0)
            {
                out += std::signbit(d) ? "-0.0" : "0.0";
                return;
            }
            char buf[64] = {};
            int prec = 1;
            for (; prec < 17; ++prec)
            {
                std::snprintf(buf, sizeof buf, "%.*g", prec, d);
                dots(buf);
                if (readsBack(buf, d))
                    break;
            }
            std::snprintf(buf, sizeof buf, "%.*g", prec, d);
            dots(buf);
            // the same digits in plain decimal when the exponent is small enough to read
            char e[64];
            std::snprintf(e, sizeof e, "%.*e", prec - 1, d);
            const char *x = std::strchr(e, 'e');
            const int exp10 = x ? std::atoi(x + 1) : 0;
            if (exp10 >= -5 && exp10 <= 15)
            {
                char f[64];
                std::snprintf(f, sizeof f, "%.*f", prec - 1 - exp10 > 0 ? prec - 1 - exp10 : 0, d);
                dots(f);
                if (readsBack(f, d))
                    std::snprintf(buf, sizeof buf, "%s", f);
            }
            const std::string t = buf;
            out += t;
            if (t.find_first_of(".e") == std::string::npos)
                out += ".0";   // read back as a number with a fraction
        }
    }
}
