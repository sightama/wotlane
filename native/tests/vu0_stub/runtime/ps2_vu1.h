// ABOUTME: Stand-in for PS2Recomp's runtime/ps2_vu1.h in the unit tests (which build without PS2Recomp): a VU1State with only the
// ABOUTME: fields wotm_vu0_native.cpp touches, so wotm_vu0_test.cpp can build its arithmetic helpers and mode guard.
#pragma once

#include <cstdint>

struct VU1State
{
    float vf[32][4];
    int32_t vi[16];
    uint32_t pc;
    uint32_t mac;
    uint32_t status;
    uint64_t cycles;
};
