#include "common.h"
#include "hieri_types.h"

extern float fogMinRange, fogMaxRange, fogMaxVal, fogFarClip, fogFA, fogFB;
extern int fogColorRed, fogColorGreen, fogColorBlue, fogChanged, fogBackgroundColorIsSlaved;
extern float fogTODMinRange, fogTODMaxRange, fogTODMaxVal, fogTODFarClip, fogTODFA, fogTODFB;
extern int fogTODColorRed, fogTODColorGreen, fogTODColorBlue;

int todActive(void);
void fogCalcFogValues(float maxRange, float minRange, float maxVal, float *fa, float *fb);
void fogInitStats(void);
void fogGetFogValues(QwData *q);
float todGetCurrentTOD(void);
void viewSetBgColor(unsigned long r, unsigned long g, unsigned long b, unsigned long a);
extern "C" void FlushCache(int mode);
extern float fogTOD;
extern unsigned long fogColorCtx[6];
/* fog volumes (4 slots) */
extern float fogVolMinRange[4] __asm__("D_00735750");
extern float fogVolMaxRange[4] __asm__("D_00735760");
extern float fogVolMaxVal[4] __asm__("D_00735770");
extern float fogVolBlend[4] __asm__("D_00735780");
extern int fogVolRed[4] __asm__("D_00735790");
extern int fogVolGreen[4] __asm__("D_007357A0");
extern int fogVolBlue[4] __asm__("D_007357B0");
extern int fogVolChanged[4] __asm__("D_007357C0");
/* fog constants inside the VU1 programs data */
extern float vu1FogParams[4] __asm__("D_0025B280");
extern float vu1FogParams2[4] __asm__("D_0025B870");
extern float vu1FogFA __asm__("D_00256BDC");
extern float vu1FogFB __asm__("D_00256BEC");
extern float vu1FogMaxVal __asm__("D_00256C0C");

#ifdef NON_MATCHING
/* equivalent (untuned): 64/88 words, size 0x160 vs 0x160 */
void fogInit(void)
{
    fogChanged = 1;
    fogBackgroundColorIsSlaved = 1;
    fogMinRange = 300.0f;
    fogMaxRange = 2000.0f;
    fogMaxVal = 220.0f;
    fogFarClip = 2000.0f;
    fogColorRed = 60;
    fogColorGreen = 60;
    fogColorBlue = 60;
    fogCalcFogValues(2000.0f, 300.0f, 220.0f, &fogFA, &fogFB);
    fogTODMaxRange = 8000.0f;
    fogTODMaxVal = 255.0f;
    fogTODFarClip = 8000.0f;
    fogTODColorBlue = 60;
    fogTODMinRange = 300.0f;
    fogTODColorRed = 60;
    fogTODColorGreen = 60;
    fogCalcFogValues(8000.0f, 300.0f, 255.0f, &fogTODFA, &fogTODFB);
    fogInitStats();
    /* DMA tag (end, 2 qwords), GIF tag (1 A+D, EOP), then the FOGCOL write filled by fogUpdate */
    fogColorCtx[0] = 0;
    fogColorCtx[1] = 0;
    *(unsigned *)&fogColorCtx[0] = 0x70000002;
    ((unsigned *)&fogColorCtx[2])[0] = 0x8001; /* 32-bit halves: snfix has no expansion for a 64-bit li */
    ((unsigned *)&fogColorCtx[2])[1] = 0x10000000;
    fogColorCtx[3] = 0xE;
    fogVolBlend[0] = 0.0f;
    fogVolBlend[3] = 0.0f;
    fogVolBlend[1] = 0.0f;
    fogVolBlend[2] = 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/fog", fogInit__Fv);
#endif
void fogInitStats(void)
{
}
void fogSetChanged(void)
{
    fogCalcFogValues(fogMaxRange, fogMinRange, fogMaxVal, &fogFA, &fogFB);
    fogChanged = 1;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 4/182 words, size 0x2c8 vs 0x2d8 */
/* When the fog changed (or volume i asked for it): blend the base fog toward volume i by its weight, push the fog
   constants to the VU1 programs and FOGCOL to the GS (a direct DMA on the GIF channel). */
void fogUpdate(int i)
{
    float maxR, minR, a, k, red, green, blue, nMax, nMin;
    int r, g, b;

    if (fogChanged || fogVolChanged[i]) {
        maxR = fogMaxRange;
        minR = fogMinRange;
        fogChanged = 0;
        fogVolChanged[i] = 0;
        if (maxR <= minR)
            return;
        a = fogVolBlend[i];
        k = 1.0f - a;
        blue = a * fogVolBlue[i] + k * fogColorBlue;
        red = a * fogVolRed[i] + k * fogColorRed;
        green = a * fogVolGreen[i] + k * fogColorGreen;
        nMax = a * fogVolMaxRange[i] + k * maxR;
        nMin = a * fogVolMinRange[i] + k * minR;
        r = (int)red;
        g = (int)green;
        b = (int)blue;
        if (0.0f < a) {
            /* restore the base fog next frame */
            fogVolBlend[i] = 0.0f;
            fogVolChanged[i] = 1;
        }
        fogCalcFogValues(nMax, nMin, fogMaxVal, &fogFA, &fogFB);
        fogGetFogValues((QwData *)vu1FogParams);
        vu1FogFA = fogFA;
        vu1FogFB = fogFB;
        vu1FogMaxVal = fogMaxVal;
        vu1FogParams2[0] = vu1FogParams[0];
        vu1FogParams2[3] = vu1FogParams[3];
        vu1FogParams2[1] = vu1FogParams[1];
        vu1FogParams2[2] = vu1FogParams[2];
        if (fogBackgroundColorIsSlaved)
            viewSetBgColor(r, g, b, 0);
        fogColorCtx[4] = (unsigned long)r | ((unsigned long)g << 8) | ((unsigned long)b << 16);
        fogColorCtx[5] = 0x3D;
        FlushCache(0);
        *(volatile unsigned *)0x1000A020 = 0;                 /* GIF channel QWC */
        *(volatile unsigned *)0x1000A030 = (unsigned)fogColorCtx; /* TADR */
        *(volatile unsigned *)0x1000E010 = 4;                 /* D_STAT: clear channel 2 */
        *(volatile unsigned *)0x1000A000 = 0x145;             /* CHCR: chain, start */
    }
    fogTOD = todGetCurrentTOD();
}
#else
INCLUDE_ASM("asm/nonmatchings/common/fog", fogUpdate__Fi);
#endif
void fogCalcFogValues(float maxRange, float minRange, float maxVal, float *fa, float *fb)
{
    float v = (maxVal - 1.0f) * (minRange * maxRange) / (maxRange - minRange);

    *fb = v;
    *fa = 1.0f - v / maxRange;
}
void fogGetFogValues(QwData *q)
{
    q->fVec[0] = fogFA;
    q->fVec[1] = fogMaxVal;
    q->fVec[2] = 1.0f;
    q->fVec[3] = fogFB;
}
void fogGetFogParms(float *minR, float *maxR, float *maxV, float *farClip, int *r, int *g, int *b)
{
    *minR = fogMinRange;
    *maxR = fogMaxRange;
    *maxV = fogMaxVal;
    *farClip = fogFarClip;
    *r = fogColorRed;
    *g = fogColorGreen;
    *b = fogColorBlue;
}
float fogGetFogMinRange(void)
{
    return fogMinRange;
}
void fogSetFogParms(float minR, float maxR, float maxV, float farClip, int r, int g, int b)
{
    fogColorRed = r;
    fogColorGreen = g;
    fogFarClip = farClip;
    fogColorBlue = b;
    fogChanged = 1;
    fogMinRange = minR;
    fogMaxRange = maxR;
    fogMaxVal = maxV;
    fogCalcFogValues(maxR, minR, maxV, &fogFA, &fogFB);
}
/* Fog volume i (4 slots): blend weight, ranges and color; applied by the next fogUpdate(i). */
void fogSetVolumeParms(int i, float blend, float minR, float maxR, float maxVal, int r, int g, int b)
{
    fogVolMinRange[i] = minR;
    fogVolMaxRange[i] = maxR;
    fogVolMaxVal[i] = maxVal;
    fogVolBlend[i] = blend;
    fogVolRed[i] = r;
    fogVolGreen[i] = g;
    fogVolBlue[i] = b;
    fogVolChanged[i] = 1;
}
float fogGetFarClipRange(void)
{
    if (todActive())
        return fogTODFarClip;
    return fogFarClip;
}
int fogGetIsSlavedToBg(void)
{
    return fogBackgroundColorIsSlaved;
}
void fogGetTODFogValues(QwData *q)
{
    q->fVec[0] = fogTODFA;
    q->fVec[1] = fogTODMaxVal;
    q->fVec[2] = 1.0f;
    q->fVec[3] = fogTODFB;
}
void fogGetTODFogParms(float *minR, float *maxR, float *maxV, float *farClip, int *r, int *g, int *b)
{
    *minR = fogTODMinRange;
    *maxR = fogTODMaxRange;
    *maxV = fogTODMaxVal;
    *farClip = fogTODFarClip;
    *r = fogTODColorRed;
    *g = fogTODColorGreen;
    *b = fogTODColorBlue;
}
void fogSetTODFogParms(float minR, float maxR, float maxV, float farClip, int r, int g, int b)
{
    fogTODColorRed = r;
    fogTODColorGreen = g;
    fogTODFarClip = farClip;
    fogTODColorBlue = b;
    fogChanged = 1;
    fogTODMinRange = minR;
    fogTODMaxRange = maxR;
    fogTODMaxVal = maxV;
    fogCalcFogValues(maxR, minR, maxV, &fogTODFA, &fogTODFB);
}
void fogAdjust(float k)
{
    fogChanged = 1;
    fogMinRange *= k;
    fogFarClip *= k;
    fogMaxRange *= k;
}
