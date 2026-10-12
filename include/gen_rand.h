#ifndef GEN_RAND_H
#define GEN_RAND_H

/* Mersenne Twister (MT19937). The engine keeps one global instance, gRand (common/mathf). */
class GenRand {
public:
    unsigned mt[624];
    int mti;

    GenRand();
    GenRand(unsigned s);
    void seed(unsigned s);
    unsigned rand(void);
};

extern GenRand gRand;

#endif
