#include "common.h"

#include "gen_rand.h"

GenRand::GenRand()
{
}
GenRand::GenRand(unsigned s)
{
    seed(s);
}
#ifdef NON_MATCHING
/* 49/119 words: SN multiplies via multu/mflo/mfhi (64-bit path) and orders loads differently */
void GenRand::seed(unsigned s)
{
    mt[0] = s;
    for (mti = 1; mti < 624; mti++)
        mt[mti] = (unsigned long)1812433253 * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti;
    for (mti = 0; mti < 227; mti++) {
        unsigned y = (mt[mti] & 0x80000000) | (mt[mti + 1] & 0x7FFFFFFF);

        mt[mti] = mt[mti + 397] ^ (y >> 1) ^ ((y & 1) ? 0x9908B0DF : 0);
    }
    for (; mti < 623; mti++) {
        unsigned y = (mt[mti] & 0x80000000) | (mt[mti + 1] & 0x7FFFFFFF);

        mt[mti] = mt[mti - 227] ^ (y >> 1) ^ ((y & 1) ? 0x9908B0DF : 0);
    }
    {
        unsigned y = (mt[623] & 0x80000000) | (mt[0] & 0x7FFFFFFF);

        mt[623] = mt[396] ^ (y >> 1) ^ ((y & 1) ? 0x9908B0DF : 0);
    }
    mti = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/GenRand", seed__7GenRandUi);
#endif
#ifdef NON_MATCHING
/* 52/69 words: register allocation / load order differ */
unsigned GenRand::rand(void)
{
    unsigned y = mt[mti];

    if (mti < 623) {
        unsigned z = (mt[mti] & 0x80000000) | (mt[mti + 1] & 0x7FFFFFFF);

        mt[mti] = mt[mti < 227 ? mti + 397 : mti - 227] ^ (z >> 1) ^ ((z & 1) ? 0x9908B0DF : 0);
        mti++;
    } else {
        unsigned z = (mt[623] & 0x80000000) | (mt[0] & 0x7FFFFFFF);

        mt[623] = mt[396] ^ (z >> 1) ^ ((z & 1) ? 0x9908B0DF : 0);
        mti = 0;
    }
    y ^= y >> 11;
    y ^= (y << 7) & 0x9D2C5680;
    y ^= (y << 15) & 0xEFC60000;
    y ^= y >> 18;
    return y;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/GenRand", rand__7GenRand);
#endif
