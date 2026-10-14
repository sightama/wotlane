#ifndef VECMATH_H
#define VECMATH_H

/* VU0 macro-mode vector helpers. Their asm matches what the retail build inlines everywhere.
   The .set noreorder keeps GNU as from adding the cop1->cop2 hazard nop that ps2eeas leaves out. */

struct _fvector;

static inline void vecAdd(_fvector *dst, _fvector *a, _fvector *b)
{
    __asm__ volatile("lqc2 $vf11, 0x0(%1)\n\t"
                     "lqc2 $vf12, 0x0(%2)\n\t"
                     "vadd.xyz $vf11, $vf11, $vf12\n\t"
                     "sqc2 $vf11, %0"
                     : "=m"(*dst) : "r"(a), "r"(b));
}
static inline void vecSub(_fvector *dst, _fvector *a, _fvector *b)
{
    __asm__ volatile("lqc2 $vf11, 0x0(%1)\n\t"
                     "lqc2 $vf12, 0x0(%2)\n\t"
                     "vsub.xyz $vf11, $vf11, $vf12\n\t"
                     "sqc2 $vf11, %0"
                     : "=m"(*dst) : "r"(a), "r"(b));
}
static inline float vecLenSq(_fvector *v)
{
    float *f = (float *)v;
    register float x __asm__("$f2");
    register float y __asm__("$f1");
    register float z __asm__("$f0");

    __asm__ volatile("lwc1 %0, %3
	"
                     "lwc1 %1, %4
	"
                     "lwc1 %2, %5
	"
                     "mula.s %0, %0
	"
                     "madda.s %1, %1
	"
                     "madd.s %2, %2, %2"
                     : "=f"(x), "=f"(y), "=f"(z) : "m"(f[0]), "m"(f[1]), "m"(f[2]));
    return z;
}
static inline void vecScale(_fvector *dst, _fvector *a, float s)
{
    register int t __asm__("$2");

    __asm__ volatile(".set push\n\t.set noreorder\n\t"
                     "lqc2 $vf11, 0x0(%2)\n\t"
                     "mfc1 %1, %3\n\t"
                     "qmtc2.ni %1, $vf12\n\t"
                     "vmulx.xyz $vf11, $vf11, $vf12x\n\t"
                     "sqc2 $vf11, %0\n\t"
                     ".set pop"
                     : "=m"(*dst), "=r"(t) : "r"(a), "f"(s));
}

/* a . b (xyz) on the FPU accumulator, registers left to the compiler (vecLenSq pins them). Volatile like retail's: HomingBug::findTarget
   computes one and never reads it. */
static inline float vecDot(_fvector *a, _fvector *b)
{
    float *p = (float *)a, *q = (float *)b;
    float r;

    __asm__ volatile("mula.s %1, %4\n\t"
                     "madda.s %2, %5\n\t"
                     "madd.s %0, %3, %6"
                     : "=f"(r) : "f"(p[0]), "f"(p[1]), "f"(p[2]), "f"(q[0]), "f"(q[1]), "f"(q[2]));
    return r;
}
/* a / sqrt(b) in one rsqrt.s */
static inline float eeRsqrtf(float a, float b)
{
    float r;

    __asm__("rsqrt.s %0, %1, %2" : "=f"(r) : "f"(a), "f"(b));
    return r;
}
/* *dst = *src with one lq/sq through $2 */
static inline void vecCopy(_fvector *dst, _fvector *src)
{
    register int t __asm__("$2");

    __asm__ volatile("lq %1, 0(%2)\n\t"
                     "sq %1, %0"
                     : "=m"(*dst), "=r"(t) : "r"(src));
}

/* dst = a x b */
static inline void vecCross(_fvector *dst, _fvector *a, _fvector *b)
{
    __asm__ volatile("lqc2 $vf11, 0x0(%1)\n\t"
                     "lqc2 $vf12, 0x0(%2)\n\t"
                     "vopmula.xyz $ACC, $vf11, $vf12\n\t"
                     "vopmsub.xyz $vf11, $vf12, $vf11\n\t"
                     "sqc2 $vf11, %0"
                     : "=m"(*dst) : "r"(a), "r"(b), "m"(*a), "m"(*b));
}
/* dst = m[0] * v.x + m[1] * v.y + m[2] * v.z (xyz; the rows of m are the axes) */
static inline void vecApplyRows(_fvector *dst, float (*m)[4], _fvector *v)
{
    __asm__ volatile("lqc2 $vf11, 0x0(%1)\n\t"
                     "lqc2 $vf12, 0x0(%2)\n\t"
                     "lqc2 $vf13, 0x10(%2)\n\t"
                     "lqc2 $vf14, 0x20(%2)\n\t"
                     "vmulax.xyz $ACC, $vf12, $vf11x\n\t"
                     "vmadday.xyz $ACC, $vf13, $vf11y\n\t"
                     "vmaddz.xyz $vf11, $vf14, $vf11z\n\t"
                     "sqc2 $vf11, %0"
                     : "=m"(*dst) : "r"(v), "r"(m), "m"(*v));
}
/* A 4x4 float matrix as one 64-byte value: assigning it copies the four rows with lq/sq like retail's matrix copies. */
struct QMat {
    float m[4][4];
} __attribute__((aligned(16)));
#define matCopy(dst, src) (*(QMat *)(dst) = *(QMat *)(src))

/* dst = (0, 0, 0, 1): stores vf0 */
static inline void vecStoreUnitW(_fvector *dst)
{
    __asm__ volatile("sqc2 $vf0, %0" : "=m"(*dst));
}

#ifdef NON_MATCHING
/* R5900 SQRT.S fd, ft takes its source from ft (fs = 0, e.g. retail 0x46020084). GNU as encodes sqrt.s (and ee-gcc's inline sqrtf) in the
   MIPS32 form with the source in fs, which the EE reads as $f0 (the z term was silently dropped in computeCostEstimate). .word 0x46040104 = sqrt.s $f4,$f4. */
static inline float eeSqrtf(float v)
{
    register float x __asm__("$f4") = v;
    __asm__(".word 0x46040104" : "+f"(x));
    return x;
}
/* Running bounding box in vf30 (min) / vf31 (max), read back by mathfVectorMinMaxGetMac (common/mathf, asm). Retail keeps it in VU0
   across calls (particleEmit, the particle movers, peds, Debris), so the C++ has to use the same registers. */
static inline void vu0BBoxInit(_fvector *p)
{
    __asm__ volatile("lqc2 $vf30, 0x0(%0)\n\t"
                     "lqc2 $vf31, 0x0(%0)" : : "r"(p) : "memory");
}
static inline void vu0BBoxAdd(_fvector *p)
{
    __asm__ volatile("lqc2 $vf11, 0x0(%0)\n\t"
                     "vmini.xyzw $vf30, $vf30, $vf11\n\t"
                     "vmax.xyzw $vf31, $vf31, $vf11" : : "r"(p) : "memory");
}
#endif

#endif
