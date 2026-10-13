#include "common.h"

/* State of smoothEasyOutEasyIn: drives *cur toward target with a velocity that accelerates and brakes. */
struct _smoothDat {
    float *cur;     // 0x0
    float target;   // 0x4
    float result;   // 0x8 *cur + *vel after the step
    float decel;    // 0xC
    float accel;    // 0x10
    float max;      // 0x14 top speed
    float delta;    // 0x18 target - *cur
    float *vel;     // 0x1C
    int isAngle;    // 0x20 wrap to [-pi, pi]
};

extern "C" float powf(float x, float y);
int timerGetUpdateRate(void);
void boundEulerAngle(float *a);
int smoothCanIncreaseToMax(float v, float decel, float dist);
int smoothCanDecreaseToMinusMax(float v, float decel, float dist);

#ifdef NON_MATCHING
/* equivalent (untuned): 1/8 words, size 0x20 vs 0x20 */
/* 1/8 words: store order of the struct fields differs */
void smoothInit(_smoothDat *d, float *p, float a, float b, float c, float e, float *q, bool flag)
{
    d->isAngle = flag;
    d->cur = p;
    d->target = a;
    d->decel = b;
    d->accel = c;
    d->max = e;
    d->vel = q;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/smooth", smoothInit__FP10_smoothDatPfffffT1b);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 11/46 words, size 0xb8 vs 0x8c */
#define HALF_PI 1.5707964f
/* One step from cur toward target: a fraction `rate` of the distance, but at least minStep, never overshooting. */
float smoothEasyIn(float cur, float target, float rate, float minStep)
{
    float step, n;

    if (cur == target)
        return target;
    step = (target - cur) * rate;
    if ((step < 0.0f ? -step : step) < minStep)
        step = step < 0.0f ? -minStep : minStep;
    n = cur + step;
    if (cur < target) {
        if (target < n)
            return target;
    } else if (n < target) {
        n = target;
    }
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/smooth", smoothEasyIn__Fffff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 11/96 words, size 0x180 vs 0x14c */
/* smoothEasyIn on an angle: the difference is wrapped to [-pi, pi], and a target across the +-pi seam is unwrapped. */
float smoothEasyInAngle(float cur, float target, float rate, float minStep)
{
    float d, step, res;

    if (cur == target) {
        res = target;
        return res;
    }
    d = target - cur;
    boundEulerAngle(&d);
    step = d * rate;
    if ((step < 0.0f ? -step : step) < minStep)
        step = step < 0.0f ? -minStep : minStep;
    if ((HALF_PI < cur && target < -HALF_PI) || (cur < -HALF_PI && HALF_PI < target))
        target = cur + d;
    res = cur + step;
    if (cur < target) {
        if (target < res)
            res = target;
    } else if (res < target) {
        res = target;
    }
    boundEulerAngle(&res);
    return res;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/smooth", smoothEasyInAngle__Fffff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 21/26 words, size 0x64 vs 0x68 */
/* 17/26 words: retail uses bc1f with the result set in the delay slot (no branch-likely) */
int smoothCanIncreaseToMax(float a, float b, float c)
{
    int r = 1;

    if (!(a < b)) {
        float d = a - b;
        float n = (float)(int)(d / b + 1.0f);

        r = (d * 0.5f + b) * n < c;
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/smooth", smoothCanIncreaseToMax__Ffff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 21/26 words, size 0x64 vs 0x68 */
/* 17/26 words: same as smoothCanIncreaseToMax */
int smoothCanDecreaseToMinusMax(float a, float b, float c)
{
    int r = 1;

    if (!(b < a)) {
        float d = a - b;
        float n = (float)(int)(d / b + 1.0f);

        r = c < (d * 0.5f + b) * n;
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/smooth", smoothCanDecreaseToMinusMax__Ffff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 10/218 words, size 0x350 vs 0x368 */
/* Accelerate/brake *vel (accel, decel, top speed max) so cur reaches target without overshoot; result = cur + vel. */
void smoothEasyOutEasyIn(_smoothDat *d)
{
    float *v;
    float res;

    d->delta = d->target - *d->cur;
    if (d->delta == 0.0f) {
        d->result = d->target;
        *d->vel = 0.0f;
        return;
    }
    if (d->isAngle) {
        boundEulerAngle(&d->delta);
        if ((HALF_PI < *d->cur && d->target < -HALF_PI) || (*d->cur < -HALF_PI && HALF_PI < d->target))
            d->target = *d->cur + d->delta;
    }
    v = d->vel;
    if (0.0f < d->delta) {
        if (smoothCanIncreaseToMax(*v, d->decel, d->delta)) {
            if (*v < d->max) {
                *v += d->accel;
                if (d->max < *v)
                    *v = d->max;
            } else if (d->max < *v) {
                *v -= d->decel;
                if (*v < d->max)
                    *v = d->max;
            }
        } else if (d->decel < *v) {
            *v -= d->decel;
            if (*v < d->decel)
                *v = d->decel;
        } else if (*v < d->decel) {
            *v += d->decel;
            if (d->decel < *v)
                *v = d->decel;
        }
    } else {
        if (smoothCanDecreaseToMinusMax(*v, d->decel, d->delta)) {
            if (-d->max < *v) {
                *v -= d->accel;
                if (*v < -d->max)
                    *v = -d->max;
            } else if (*v < -d->max) {
                *v += d->decel;
                if (-d->max < *v)
                    *v = -d->max;
            }
        } else if (*v < -d->decel) {
            *v += d->decel;
            if (-d->decel < *v)
                *v = -d->decel;
        } else if (-d->decel < *v) {
            *v -= d->decel;
            if (*v < -d->decel)
                *v = -d->decel;
        }
    }
    res = *d->cur + *d->vel;
    d->result = res;
    if (res == d->target) {
        *d->vel = 0.0f;
    } else if (d->target < res ? *d->cur <= d->target : d->target <= *d->cur) {
        /* passed the target this step */
        d->result = d->target;
        *d->vel = 0.0f;
    }
    if (d->isAngle)
        boundEulerAngle(&d->result);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/smooth", smoothEasyOutEasyIn__FP10_smoothDat);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/44 words, size 0xa8 vs 0xb0 */
/* The per-field rate/minStep scaled to the update rate (they are tuned for 60 Hz). */
float smoothEasyInTC(float cur, float target, float rate, float minStep)
{
    float k = 60.0f / timerGetUpdateRate();

    if (1.0f < k) {
        minStep *= k;
        rate = 1.0f - powf(1.0f - rate, k);
    }
    return smoothEasyIn(cur, target, rate, minStep);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/smooth", smoothEasyInTC__Fffff);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 1/44 words, size 0xa8 vs 0xb0 */
float smoothEasyInAngleTC(float cur, float target, float rate, float minStep)
{
    float k = 60.0f / timerGetUpdateRate();

    if (1.0f < k) {
        minStep *= k;
        rate = 1.0f - powf(1.0f - rate, k);
    }
    return smoothEasyInAngle(cur, target, rate, minStep);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/smooth", smoothEasyInAngleTC__Fffff);
#endif
