#include "common.h"
#include "hieri_types.h"

/* Animation curves (one per animated channel). A curve starts with AnimCurveHeader; curveType picks the key
 * kind (0 hermite, 1 broken tangents, 2 linear, 3 static, 4 smooth, 5 stepped) and dataType the storage
 * (0 float, 1 ushort, 2 uchar). Compressed curves (and every linear one) are preceded by AnimNormData:
 * time = raw * deltaTime + baseTime, value = raw * deltaVal + baseVal. Linear curves store only values,
 * one per uniform time step. */

typedef float (*AnimEvalFn)(AnimCurveHeader *, float, unsigned short *);

/* [curveType][dataType], filled by the static initializer below (still asm) */
extern AnimEvalFn animEvalTable[6][3] __asm__("D_007356C8");

float evalStatic(AnimCurveHeader *curve, float, unsigned short *)
{
    return ((AnimCurveStatic *)curve)->value;
}

float animCurveEvaluate(AnimCurveHeader *curve, float t, unsigned short *cache)
{
    return animEvalTable[curve->curveType][curve->dataType](curve, t, cache);
}

#ifdef NON_MATCHING
/* Every template instance below is equivalent, not matched (untuned; the retail emits them interleaved in
 * instantiation order, scores in the commit notes). */
template <class K> struct AnimCurveData {
    AnimCurveHeader head;  // 0x0
    short numKeys;  // 0x4
    K keys[1];  // 0x6 (0x8 for float keys)
};
template <class K> struct AnimCurveNormData : AnimNormData, AnimCurveData<K> {};

template <class T> struct AnimKeyLinear { T value; };
template <class T> struct AnimKeyStepped { T time; T value; };
template <class T, class S> struct AnimKeySmooth { T time; T value; S slope; };
template <class T, class S> struct AnimKeyBroken { T time; T value; S slopeOut; S slopeIn; };
template <class T, class S> struct AnimKeyHermite { T time; T value; S coef[3]; };  // value + t*(c2 + t*(c1 + t*c0))

/* time type used by the key search: float keys compare times directly, compressed keys in raw units */
template <class T> struct AnimTimeType;
template <> struct AnimTimeType<float> { typedef float Type; };
template <> struct AnimTimeType<unsigned short> { typedef int Type; };
template <> struct AnimTimeType<unsigned char> { typedef int Type; };
template <class TT> struct AnimKeyTime;
template <> struct AnimKeyTime<float> {
    static inline float fromTime(AnimNormData *, float t) { return t; }
};
template <> struct AnimKeyTime<int> {
    static inline int fromTime(AnimNormData *n, float t) { return (int)((t - n->baseTime) / n->deltaTime); }
};

template <class K> static inline AnimNormData *animNorm(AnimCurveData<K> *d)
{
    return static_cast<AnimCurveNormData<K> *>(d);
}

static inline float decodeTime(AnimNormData *, float t) { return t; }
static inline float decodeTime(AnimNormData *n, unsigned short t) { return t * n->deltaTime + n->baseTime; }
static inline float decodeTime(AnimNormData *n, unsigned char t) { return t * n->deltaTime + n->baseTime; }
static inline float decodeValue(AnimNormData *, float v) { return v; }
static inline float decodeValue(AnimNormData *n, unsigned short v) { return v * n->deltaVal + n->baseVal; }
static inline float decodeValue(AnimNormData *n, unsigned char v) { return v * n->deltaVal + n->baseVal; }
static inline float decodeSlope(float s) { return s; }
/* 2.14 fixed point; the two outer quarters of the range encode steep slopes as 16384 / (+-32768 - s) */
static inline float decodeSlope(short s)
{
    int hi = s >> 14;
    if (hi == -2)
        return 16384.0f / (-32768.0f - s);
    if (hi == 1)
        return 16384.0f / (32768.0f - s);
    return s * (1.0f / 16384.0f);
}

#define KEY_TIME(d, i) decodeTime(animNorm(d), (d)->keys[i].time)
#define KEY_VALUE(d, i) decodeValue(animNorm(d), (d)->keys[i].value)

template <class T> static inline float keyTime(AnimCurveData<AnimKeyLinear<T> > *d, int i)
{
    AnimNormData *n = animNorm(d);
    return i * n->deltaTime + n->baseTime;
}
template <class T> static inline float keyTime(AnimCurveData<AnimKeyStepped<T> > *d, int i) { return KEY_TIME(d, i); }
template <class T, class S> static inline float keyTime(AnimCurveData<AnimKeySmooth<T, S> > *d, int i) { return KEY_TIME(d, i); }
template <class T, class S> static inline float keyTime(AnimCurveData<AnimKeyBroken<T, S> > *d, int i) { return KEY_TIME(d, i); }
template <class T, class S> static inline float keyTime(AnimCurveData<AnimKeyHermite<T, S> > *d, int i) { return KEY_TIME(d, i); }

/* Index of the key that starts the segment holding t; *cache keeps the last hit (curves are usually
 * sampled with increasing time, so the neighbours are tried before the bisection). */
template <class K, class TT> int binarySearch(AnimCurveData<K> *d, float t, unsigned short *cache)
{
    int n = d->numKeys;
    int last = n - 1;
    int i = *cache;
    int lo, hi, mid;
    TT kt;
    if (i >= last)
        i = n >> 1;
    kt = AnimKeyTime<TT>::fromTime(animNorm(d), t);
    if (d->keys[i + 1].time <= kt) {
        if (kt < d->keys[i + 2].time) {
            i = i + 1;
            *cache = i;
            return i;
        }
        lo = i + 2;
        hi = last;
        if (i == 0)
            mid = n - 2;
        else
            mid = (lo + hi) >> 1;
    } else {
        if (!(kt < d->keys[i].time))
            return i;
        hi = i - 1;
        if (d->keys[i - 1].time <= kt) {
            i = i - 1;
            *cache = i;
            return i;
        }
        lo = 0;
        if (i == n - 2)
            mid = 1;
        else
            mid = hi >> 1;
    }
    while (hi - lo >= 2) {
        if (kt < d->keys[mid].time)
            hi = mid;
        else
            lo = mid;
        mid = (lo + hi) >> 1;
    }
    *cache = lo;
    return lo;
}

template <class T> float evaluateBetweenKeyframes(AnimCurveData<AnimKeyLinear<T> > *d, float t, int i)
{
    AnimNormData *n = animNorm(d);
    float v0 = KEY_VALUE(d, i);
    float v1 = KEY_VALUE(d, i + 1);
    return v0 + (v1 - v0) * ((t - n->baseTime) / n->deltaTime - i);
}

template <class T> float evaluateBetweenKeyframes(AnimCurveData<AnimKeyStepped<T> > *d, float, int i)
{
    return KEY_VALUE(d, i);
}

template <class T, class S> float evaluateBetweenKeyframes(AnimCurveData<AnimKeyHermite<T, S> > *d, float t, int i)
{
    AnimKeyHermite<T, S> *k = &d->keys[i];
    float x = t - k->time;
    return x * (x * (x * k->coef[0] + k->coef[1]) + k->coef[2]) + k->value;
}

/* cubic hermite on the normalized segment parameter; the tangents are stored per segment */
template <class T, class S> float evaluateBetweenKeyframes(AnimCurveData<AnimKeyBroken<T, S> > *d, float t, int i)
{
    float t0 = KEY_TIME(d, i);
    float v0 = KEY_VALUE(d, i);
    float dv = KEY_VALUE(d, i + 1) - v0;
    float m0 = decodeSlope(d->keys[i].slopeOut);
    float m1 = decodeSlope(d->keys[i].slopeIn);
    float s = (t - t0) / (KEY_TIME(d, i + 1) - t0);
    return s * (s * (s * (m0 + m1 - 2.0f * dv) + (2.0f * dv + dv - (2.0f * m0 + m1))) + m0) + v0;
}

/* cubic hermite in curve time with one shared slope (value per time unit) per key */
template <class T, class S> float evaluateBetweenKeyframes(AnimCurveData<AnimKeySmooth<T, S> > *d, float t, int i)
{
    float t0 = KEY_TIME(d, i);
    float v0 = KEY_VALUE(d, i);
    float dt = KEY_TIME(d, i + 1) - t0;
    float dv = KEY_VALUE(d, i + 1) - v0;
    float m0 = decodeSlope(d->keys[i].slope);
    float m1 = decodeSlope(d->keys[i + 1].slope);
    float inv = 1.0f / dt;
    float a = m0 * dt;
    float b = m1 * dt;
    float inv2 = inv * inv;
    float x = t - t0;
    return x * (x * (x * ((a + b - 2.0f * dv) * (inv2 * inv)) + (2.0f * dv + dv - (2.0f * a + b)) * inv2) + m0) + v0;
}

template <class T> static inline int findKey(AnimCurveData<AnimKeyLinear<T> > *d, float t, unsigned short *)
{
    AnimNormData *n = animNorm(d);
    return (int)((t - n->baseTime) / n->deltaTime);
}
template <class T> static inline int findKey(AnimCurveData<AnimKeyStepped<T> > *d, float t, unsigned short *c)
{
    return binarySearch<AnimKeyStepped<T>, typename AnimTimeType<T>::Type>(d, t, c);
}
template <class T, class S> static inline int findKey(AnimCurveData<AnimKeySmooth<T, S> > *d, float t, unsigned short *c)
{
    return binarySearch<AnimKeySmooth<T, S>, typename AnimTimeType<T>::Type>(d, t, c);
}
template <class T, class S> static inline int findKey(AnimCurveData<AnimKeyBroken<T, S> > *d, float t, unsigned short *c)
{
    return binarySearch<AnimKeyBroken<T, S>, typename AnimTimeType<T>::Type>(d, t, c);
}
template <class T, class S> static inline int findKey(AnimCurveData<AnimKeyHermite<T, S> > *d, float t, unsigned short *c)
{
    return binarySearch<AnimKeyHermite<T, S>, typename AnimTimeType<T>::Type>(d, t, c);
}

/* Infinity modes (pre/post): 0 hold the end key, 1 zero, 2 cycle, 3 cycle with value offset, 4 oscillate.
 * Retail quirk kept: pre-infinity oscillate maps t to (first - t) - n * range, without adding first back. */
template <class K> float animCurveEval(AnimCurveData<K> *d, float t, unsigned short *cache)
{
    float offset = 0.0f;
    int last = d->numKeys - 1;
    float first = keyTime(d, 0);
    float end = keyTime(d, last);
    if (t < first) {
        float dist = first - t;
        float range = end - first;
        switch (d->head.preInfinity) {
        case 0:
            return KEY_VALUE(d, 0);
        case 1:
            return 0.0f;
        case 2: {
            long n = (long)(dist / range) + 1;
            t += (float)n * range;
            break;
        }
        case 3: {
            long n = (long)(dist / range) + 1;
            t += (float)n * range;
            offset = (KEY_VALUE(d, 0) - KEY_VALUE(d, last)) * (float)n;
            break;
        }
        case 4: {
            long n = (long)(dist / range);
            if ((int)(n & 1))
                t += (float)(n + 1) * range;
            else
                t = dist - (float)n * range;
            break;
        }
        }
    } else if (end <= t) {
        float range = end - first;
        float dist = t - end;
        switch (d->head.postInfinity) {
        case 0:
            return KEY_VALUE(d, last);
        case 1:
            return 0.0f;
        case 2: {
            long n = (long)(dist / range) + 1;
            t -= (float)n * range;
            break;
        }
        case 3: {
            long n = (long)(dist / range) + 1;
            t -= (float)n * range;
            offset = (KEY_VALUE(d, last) - KEY_VALUE(d, 0)) * (float)n;
            break;
        }
        case 4: {
            long n = (long)(dist / range);
            if ((int)(n & 1))
                t -= (float)(n + 1) * range;
            else
                t = (end - t) + (float)n * range;
            break;
        }
        }
    }
    return evaluateBetweenKeyframes(d, t, findKey(d, t, cache)) + offset;
}

#define ANIM_EVAL(K) template float animCurveEval<K >(AnimCurveData<K > *, float, unsigned short *)
ANIM_EVAL(AnimKeyLinear<unsigned char>);
typedef AnimKeySmooth<unsigned char, short> SmoothUc;
ANIM_EVAL(SmoothUc);
typedef AnimKeyHermite<float, float> HermiteF;
ANIM_EVAL(HermiteF);
typedef AnimKeyBroken<float, float> BrokenF;
ANIM_EVAL(BrokenF);
typedef AnimKeyBroken<unsigned short, short> BrokenUs;
ANIM_EVAL(BrokenUs);
typedef AnimKeyBroken<unsigned char, short> BrokenUc;
ANIM_EVAL(BrokenUc);
ANIM_EVAL(AnimKeyLinear<float>);
ANIM_EVAL(AnimKeyLinear<unsigned short>);
typedef AnimKeySmooth<float, float> SmoothF;
ANIM_EVAL(SmoothF);
typedef AnimKeySmooth<unsigned short, short> SmoothUs;
ANIM_EVAL(SmoothUs);
ANIM_EVAL(AnimKeyStepped<float>);
ANIM_EVAL(AnimKeyStepped<unsigned short>);
ANIM_EVAL(AnimKeyStepped<unsigned char>);
#else
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", binarySearch__H2Zt14AnimKeyStepped1ZfZf_Pt13AnimCurveData1ZX01fPUs_i);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H1ZUc_Pt13AnimCurveData1Zt13AnimKeyLinear1ZX01fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt13AnimKeyLinear1ZUc_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", binarySearch__H2Zt13AnimKeySmooth2ZUcZsZi_Pt13AnimCurveData1ZX01fPUs_i);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H2ZUcZs_Pt13AnimCurveData1Zt13AnimKeySmooth2ZX01ZX11fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt13AnimKeySmooth2ZUcZs_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", binarySearch__H2Zt14AnimKeyHermite2ZfZfZf_Pt13AnimCurveData1ZX01fPUs_i);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H2ZfZf_Pt13AnimCurveData1Zt14AnimKeyHermite2ZX01ZX11fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt14AnimKeyHermite2ZfZf_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", binarySearch__H2Zt13AnimKeyBroken2ZfZfZf_Pt13AnimCurveData1ZX01fPUs_i);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H2ZfZf_Pt13AnimCurveData1Zt13AnimKeyBroken2ZX01ZX11fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt13AnimKeyBroken2ZfZf_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", binarySearch__H2Zt13AnimKeyBroken2ZUsZsZi_Pt13AnimCurveData1ZX01fPUs_i);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H2ZUsZs_Pt13AnimCurveData1Zt13AnimKeyBroken2ZX01ZX11fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt13AnimKeyBroken2ZUsZs_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", binarySearch__H2Zt13AnimKeyBroken2ZUcZsZi_Pt13AnimCurveData1ZX01fPUs_i);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H2ZUcZs_Pt13AnimCurveData1Zt13AnimKeyBroken2ZX01ZX11fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt13AnimKeyBroken2ZUcZs_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H1Zf_Pt13AnimCurveData1Zt13AnimKeyLinear1ZX01fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt13AnimKeyLinear1Zf_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H1ZUs_Pt13AnimCurveData1Zt13AnimKeyLinear1ZX01fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt13AnimKeyLinear1ZUs_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", binarySearch__H2Zt13AnimKeySmooth2ZfZfZf_Pt13AnimCurveData1ZX01fPUs_i);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H2ZfZf_Pt13AnimCurveData1Zt13AnimKeySmooth2ZX01ZX11fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt13AnimKeySmooth2ZfZf_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", binarySearch__H2Zt13AnimKeySmooth2ZUsZsZi_Pt13AnimCurveData1ZX01fPUs_i);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H2ZUsZs_Pt13AnimCurveData1Zt13AnimKeySmooth2ZX01ZX11fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt13AnimKeySmooth2ZUsZs_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H1Zf_Pt13AnimCurveData1Zt14AnimKeyStepped1ZX01fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt14AnimKeyStepped1Zf_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", binarySearch__H2Zt14AnimKeyStepped1ZUsZi_Pt13AnimCurveData1ZX01fPUs_i);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H1ZUs_Pt13AnimCurveData1Zt14AnimKeyStepped1ZX01fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt14AnimKeyStepped1ZUs_Pt13AnimCurveData1ZX01fPUs_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", binarySearch__H2Zt14AnimKeyStepped1ZUcZi_Pt13AnimCurveData1ZX01fPUs_i);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", evaluateBetweenKeyframes__H1ZUc_Pt13AnimCurveData1Zt14AnimKeyStepped1ZX01fi_f);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", animCurveEval__H1Zt14AnimKeyStepped1ZUc_Pt13AnimCurveData1ZX01fPUs_f);
#endif
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", __static_initialization_and_destruction_0_001F6070);
INCLUDE_ASM("asm/nonmatchings/common/AnimCurve", _GLOBAL_$I$linearChar);
