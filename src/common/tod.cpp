#include "common.h"
#include "hieri_types.h"

/* Time of day. todInfo[12] holds one key per two hours (format "1.4": _todInfo14); every frame todUpdate advances
 * the clock by todTimeOfDayMinutesPerSecond, interpolates the two keys around it and feeds the result to the ambient
 * light, the sun (directional) and back parallel lights, the fog, the clear color and the sky objects (cross-fading
 * two sky domes when the keys name different skies). Level data can replace the keys in older, smaller formats
 * (todSetTOD, versions 1.0 to 1.4). Angles are in degrees. */

struct _todInfo14 {
    float hour;              // 0x00 start of the key (0, 2, .. 22)
    float ambient[3];        // 0x04
    float fogMinRange;       // 0x10
    float fogMaxRange;       // 0x14
    float fogMaxVal;         // 0x18
    float farClip;           // 0x1C
    float fogColor[3];       // 0x20
    float bgColor[3];        // 0x2C clear color
    float dirHeading;        // 0x38 sun direction
    float dirPitch;          // 0x3C
    float dirColor[3];       // 0x40
    float backPercent;       // 0x4C back light = sun * percent / 100 (-1: use backColor)
    float backColor[3];      // 0x50
    int sky;                 // 0x5C todSky index
    float skyFogBurnThru;    // 0x60
    float skyMod[3];         // 0x64
    float cloudMod[4];       // 0x70 RGBA
};
typedef char _size_todInfo14[sizeof(_todInfo14) == 0x80 ? 1 : -1];

/* a sky dome: GROUP node whose object children are tinted; colors and UV params of up to 3 of them kept */
struct TodSky {
    HierHead *node;          // 0x00
    int pad[3];
    float color[3][4];       // 0x10 objects' RGBA at load
    float params[3][4];      // 0x40 objects' +0x20, +0x24, +0x2C, +0x28 at load
};
typedef char _size_TodSky[sizeof(TodSky) == 0x70 ? 1 : -1];

struct _LinkNode {
    int prev, next, a, b;    // -1 = free
    void *data;
};
/* unfinished keyframe list over todInfo (only the constructor and AddEntry do anything) */
class CTODLinkList {
public:
    int numEntries;          // 0x0
    int head;                // 0x4
    int tail;                // 0x8
    _LinkNode nodes[12];     // 0xC

    CTODLinkList();
    ~CTODLinkList();
    int AddEntry(void);
    void DeleteEntry(int i);
    int GetNumEntries(void);
    int FindNodeByTime(float t);
    int FindFreeNode(void);
    void SortList(void);
    void ClearNode(_LinkNode *n);
};

extern _todInfo14 todInfo[12];
extern TodSky todSky[10];
extern int todNumSkys;
extern int todSkySetFound;
extern int todTimeOfDayOn;
extern int todSetTimeOfDay;
extern float todTimeOfDay;
extern float todTimeOfDayMinutesPerSecond;
extern int todFrame, todAddFrame, todDeleteFrame;
extern float todAmbientRed, todAmbientGreen, todAmbientBlue;
extern float todDirHeading, todDirPitch, todDirRed, todDirGreen, todDirBlue;
extern float todBackIntensity, todBackHeading, todBackPitch, todBackRed, todBackGreen, todBackBlue;
extern float todFogMinRange, todFogMaxRange, todFogMaxVal, todFogFarClip;
extern int todFogRed, todFogGreen, todFogBlue;
extern int todBackgroundRed, todBackgroundGreen, todBackgroundBlue;
extern float todSkyRedMod, todSkyGreenMod, todSkyBlueMod, todSkyFogBurnThru;
extern float todCloudRedMod, todCloudGreenMod, todCloudBlueMod, todCloudAlphaMod;
extern int s_todPlightsEnabled[];
extern float vuLightParams[] __asm__("D_0025B238");  // [12..14] ambient sent to VU1
/* todUpdate's local statics */
extern float todStartTOD __asm__("startTOD.144");
extern int todClockInited __asm__("_$tmp_0.146");
extern float todClock __asm__("D_006F8DE8");

extern "C" float cosf(float);
void fogSetChanged(void);
void fogSetTODFogParms(float minRange, float maxRange, float maxVal, float farClip, int r, int g, int b);
void plightUpdateParaLights(void);
void plightSetTODParaLight(int i, float r, float g, float b, float heading, float pitch);
void viewSetSkyNode(_hierhead *node, int which);
void viewSetTODBgColor(unsigned long r, unsigned long g, unsigned long b, unsigned long a);
void mathfHPtoVector(_fvector *v, float h, float p);
void mathfRotationFromVector(_fvector *rot, _fvector *v);
float todCalculateValue(int key, float lo, float hi);
void todSetSkyObjects(int idx, float alpha, float uvOffset, float burn, float r, float g, float b);
void todBoundAngle(float *a);
int todActive(void);

#ifdef NON_MATCHING
static inline float lerpf(float a, float b, float t) { return a + t * (b - a); }
/* shortest-way interpolation of two angles in degrees */
static inline float lerpAngle(float a, float b, float t)
{
    float d;
    todBoundAngle(&a);
    todBoundAngle(&b);
    d = b - a;
    todBoundAngle(&d);
    return a + t * d;
}
static inline unsigned short skyKids(HierHead *n) { return *(unsigned short *)((char *)n + 8); }
static inline HierHead *skyKid(HierHead *n, int i) { return ((HierHead **)((char *)n + 0x20))[i]; }
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 9/42 words, size 0xa8 vs 0x98 */
/* wraps to (-180, 180) */
void todBoundAngle(float *a)
{
    if (*a >= 180.0f) {
        float v = *a;
        do
            v -= 360.0f;
        while (v >= 180.0f);
        *a = v;
    }
    if (*a <= -180.0f) {
        float v = *a;
        do
            v += 360.0f;
        while (v <= -180.0f);
        *a = v;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/tod", todBoundAngle__FPf);
#endif
/* default curve over the 12 keys: lo + cos((key / 5.5 - 1) * pi / 2) * (hi - lo), peaking at noon (key 5.5) */
float todCalculateValue(int key, float lo, float hi)
{
    return lo + cosf(((float)key / 5.5f - 1.0f) * 1.5707964f) * (hi - lo);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 10/267 words, size 0x30c vs 0x42c */
/* default day: dim ambient at night, sun going around from 270 degrees at midnight */
void todInit(void)
{
    static const float hours[12] = { 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22 };
    static const float sunPitch[12] = { 270, 300, 330, 0, 30, 60, 90, 120, 150, 180, 210, 240 };
    static const int skies[12] = { 2, 2, 2, 3, 3, 0, 0, 0, 0, 1, 1, 2 };
    int i, k;

    todNumSkys = 0;
    todTimeOfDayMinutesPerSecond = 30.0f;
    todSkySetFound = 0;
    todTimeOfDayOn = 0;
    todSetTimeOfDay = 0;
    todTimeOfDay = 0.0f;
    todFrame = 0;
    todAddFrame = 0;
    todDeleteFrame = 0;
    for (i = 9; i >= 0; i--)
        todSky[i].node = 0;
    for (i = 0; i < 12; i++) {
        _todInfo14 *r = &todInfo[i];
        for (k = 0; k < 3; k++)
            r->ambient[k] = todCalculateValue(i, 10.0f, 30.0f);
        r->fogMinRange = 3000.0f;
        r->fogMaxRange = 8000.0f;
        r->fogMaxVal = 255.0f;
        r->farClip = 8000.0f;
        for (k = 0; k < 3; k++)
            r->fogColor[k] = todCalculateValue(i, 20.0f, 50.0f);
        for (k = 0; k < 3; k++)
            r->bgColor[k] = todCalculateValue(i, 20.0f, 90.0f);
        *(int *)&r->dirHeading = 0;
        r->dirColor[0] = todCalculateValue(i, 70.0f, 70.0f);
        r->dirColor[1] = todCalculateValue(i, 70.0f, 200.0f);
        r->dirColor[2] = todCalculateValue(i, 70.0f, 200.0f);
        r->backPercent = 20.0f;
        r->backColor[0] = 128.0f;
        r->backColor[1] = 128.0f;
        r->backColor[2] = 128.0f;
        *(int *)&r->skyFogBurnThru = 0;
        for (k = 0; k < 3; k++)
            r->skyMod[k] = 1.0f;
        for (k = 0; k < 4; k++)
            r->cloudMod[k] = 1.0f;
    }
    for (i = 0; i < 12; i++) {
        todInfo[i].hour = hours[i];
        todInfo[i].dirPitch = sunPitch[i];
        todInfo[i].sky = skies[i];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/tod", todInit__Fv);
#endif
void todInitStats(void)
{
}
#ifdef NON_MATCHING
/* equivalent (untuned): 11/803 words, size 0xa84 vs 0xc8c */
/* Advances the clock and applies the interpolated key to the lights (every view) and, for view 0, to the fog,
 * clear color and sky. Switching the time of day off restores neutral lights and sky. */
void todUpdate(int view)
{
    _todInfo14 *a, *b;
    _fvector dir, rot;
    float next, t;
    int i, j, saved, enabled;

    if (!todClockInited) {
        todClockInited = 1;
        todClock = todStartTOD + 0.0f;
    }
    if (todSetTimeOfDay) {
        if (!todTimeOfDayOn)
            todTimeOfDayOn = 1;
    } else {
        if (!todTimeOfDayOn)
            return;
        todTimeOfDayOn = 0;
        fogSetChanged();
        plightUpdateParaLights();
        todSetSkyObjects(0, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        viewSetSkyNode(todSky[0].node, 0);
        viewSetSkyNode(0, 1);
        if (todSky[4].node)
            todSetSkyObjects(4, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (!todTimeOfDayOn)
        return;

    todClock += todTimeOfDayMinutesPerSecond / 60.0f / 60.0f;
    if (24.0f < todClock)
        todClock -= 24.0f;
    todTimeOfDay = todClock;
    i = (int)(todClock * 0.5f);
    j = i + 1;
    if (!(j < 12))
        j = 0;
    next = j ? todInfo[j].hour : 24.0f;
    a = &todInfo[i];
    b = &todInfo[j];
    t = (todClock - a->hour) / (next - a->hour);

    todAmbientRed = lerpf(a->ambient[0], b->ambient[0], t);
    todAmbientGreen = lerpf(a->ambient[1], b->ambient[1], t);
    todAmbientBlue = lerpf(a->ambient[2], b->ambient[2], t);
    vuLightParams[14] = todAmbientBlue;
    vuLightParams[12] = todAmbientRed;
    vuLightParams[13] = todAmbientGreen;

    /* sun */
    todDirHeading = lerpAngle(a->dirHeading, b->dirHeading, t);
    todDirPitch = lerpAngle(a->dirPitch, b->dirPitch, t);
    mathfHPtoVector(&dir, todDirHeading, todDirPitch);
    dir.x = -dir.x;
    dir.y = -dir.y;
    dir.z = -dir.z;
    mathfRotationFromVector(&rot, &dir);
    todBackIntensity = lerpf(a->backPercent, b->backPercent, t) / 100.0f;
    todDirRed = lerpf(a->dirColor[0], b->dirColor[0], t);
    todDirGreen = lerpf(a->dirColor[1], b->dirColor[1], t);
    todDirBlue = lerpf(a->dirColor[2], b->dirColor[2], t);
    todBackPitch = rot.x * 57.29578f;
    todBackHeading = rot.z * 57.29578f;
    plightSetTODParaLight(0, todDirRed, todDirGreen, todDirBlue, todDirHeading, todDirPitch);

    /* back light: a share of the sun, or the own color of the keys whose percent is -1 */
    if (a->backPercent != -1.0f && b->backPercent != -1.0f) {
        todBackRed = todDirRed * todBackIntensity;
        todBackGreen = todDirGreen * todBackIntensity;
        todBackBlue = todDirBlue * todBackIntensity;
    } else if (a->backPercent != -1.0f) {
        float k = a->backPercent / 100.0f;
        todDirBlue = a->dirColor[2] * k;
        todDirRed = a->dirColor[0] * k;
        todDirGreen = a->dirColor[1] * k;
        todBackBlue = lerpf(todDirBlue, b->backColor[2], t);
        todBackRed = lerpf(todDirRed, b->backColor[0], t);
        todBackGreen = lerpf(todDirGreen, b->backColor[1], t);
    } else if (b->backPercent != -1.0f) {
        float k = b->backPercent / 100.0f;
        todDirBlue = b->dirColor[2] * k;
        todDirGreen = b->dirColor[1] * k;
        todDirRed = b->dirColor[0] * k;
        todBackBlue = lerpf(a->backColor[2], todDirBlue, t);
        todBackGreen = lerpf(a->backColor[1], todDirGreen, t);
        todBackRed = lerpf(a->backColor[0], todDirRed, t);
    } else {
        todBackRed = lerpf(a->backColor[0], b->backColor[0], t);
        todBackGreen = lerpf(a->backColor[1], b->backColor[1], t);
        todBackBlue = lerpf(a->backColor[2], b->backColor[2], t);
    }
    plightSetTODParaLight(1, todBackRed, todBackGreen, todBackBlue, todBackHeading, todBackPitch);
    plightSetTODParaLight(2, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    saved = todTimeOfDayOn;
    enabled = s_todPlightsEnabled[view];
    s_todPlightsEnabled[view] = 1;
    todTimeOfDayOn = enabled;
    plightUpdateParaLights();
    todTimeOfDayOn = saved;
    if (view > 0)
        return;

    /* fog and clear color */
    todFogMinRange = lerpf(a->fogMinRange, b->fogMinRange, t);
    todFogMaxRange = lerpf(a->fogMaxRange, b->fogMaxRange, t);
    todFogMaxVal = lerpf(a->fogMaxVal, b->fogMaxVal, t);
    todFogFarClip = lerpf(a->farClip, b->farClip, t);
    todFogRed = (int)lerpf(a->fogColor[0], b->fogColor[0], t);
    todFogGreen = (int)lerpf(a->fogColor[1], b->fogColor[1], t);
    todFogBlue = (int)lerpf(a->fogColor[2], b->fogColor[2], t);
    fogSetTODFogParms(todFogMinRange, todFogMaxRange, todFogMaxVal, todFogFarClip, todFogRed, todFogGreen, todFogBlue);
    todBackgroundRed = (int)lerpf(a->bgColor[0], b->bgColor[0], t);
    todBackgroundGreen = (int)lerpf(a->bgColor[1], b->bgColor[1], t);
    todBackgroundBlue = (int)lerpf(a->bgColor[2], b->bgColor[2], t);
    viewSetTODBgColor(todBackgroundRed, todBackgroundGreen, todBackgroundBlue, 0);

    /* sky */
    if (todSkySetFound == 1) {
        int sa, sb;
        todSkyRedMod = lerpf(a->skyMod[0], b->skyMod[0], t);
        todSkyGreenMod = lerpf(a->skyMod[1], b->skyMod[1], t);
        todSkyBlueMod = lerpf(a->skyMod[2], b->skyMod[2], t);
        todSkyFogBurnThru = lerpf(a->skyFogBurnThru, b->skyFogBurnThru, t);
        todCloudRedMod = lerpf(a->cloudMod[0], b->cloudMod[0], t);
        todCloudGreenMod = lerpf(a->cloudMod[1], b->cloudMod[1], t);
        todCloudBlueMod = lerpf(a->cloudMod[2], b->cloudMod[2], t);
        todCloudAlphaMod = lerpf(a->cloudMod[3], b->cloudMod[3], t);
        todSetSkyObjects(4, todCloudAlphaMod, 0.0f, 1.0f, todCloudRedMod, todCloudGreenMod, todCloudBlueMod);
        sa = a->sky;
        sb = b->sky;
        if (sa == sb) {
            todSetSkyObjects(sb, 1.0f, 0.0f, todSkyFogBurnThru, todSkyRedMod, todSkyGreenMod, todSkyBlueMod);
            if (!todSky[sb].node)
                return;
            viewSetSkyNode(todSky[sb].node, 0);
            viewSetSkyNode(0, 1);
        } else {
            /* cross-fade: the leaving sky is drawn as the main dome, the coming one as the second dome */
            float fa = 1.0f - t;
            float fb = t;
            if (fa < 0.0f)
                fa = 0.0f;
            if (1.0f < fb)
                fb = 1.0f;
            if (todSky[sa].node) {
                todSetSkyObjects(sa, fa, 2.9802e-7f, todSkyFogBurnThru, todSkyRedMod, todSkyGreenMod, todSkyBlueMod);
                viewSetSkyNode(todSky[sa].node, 0);
            }
            if (todSky[sb].node) {
                todSetSkyObjects(sb, fb, 0.0f, todSkyFogBurnThru, todSkyRedMod, todSkyGreenMod, todSkyBlueMod);
                viewSetSkyNode(todSky[sb].node, 1);
            }
        }
    } else if (todNumSkys > 0) {
        /* no full set of skies: the first one found stays */
        TodSky *s;
        for (s = todSky; s < todSky + 10; s++) {
            if (s->node) {
                viewSetSkyNode(s->node, 0);
                viewSetSkyNode(0, 1);
                break;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/tod", todUpdate__Fi);
#endif
int todActive(void)
{
    return todTimeOfDayOn;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/139 words, size 0xe0 vs 0x22c */
void todGetTOD(int *on, float *minutesPerSecond, _todInfo14 *keys)
{
    int i;
    *on = todTimeOfDayOn;
    *minutesPerSecond = todTimeOfDayMinutesPerSecond;
    for (i = 0; i < 12; i++)
        keys[i] = todInfo[i];
}
#else
INCLUDE_ASM("asm/nonmatchings/common/tod", todGetTOD__FPiPfP10_todInfo14);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 0/394 words, size 0x304 vs 0x628 */
/* {offset in the file record, offset in _todInfo14} of the older key formats */
static const unsigned char todMap10[][2] = {
    { 0x04, 0x04 }, { 0x08, 0x08 }, { 0x0C, 0x0C }, { 0x1C, 0x10 }, { 0x10, 0x20 }, { 0x14, 0x24 }, { 0x18, 0x28 },
    { 0x20, 0x5C },
};
static const unsigned char todMap11[][2] = {
    { 0x04, 0x04 }, { 0x08, 0x08 }, { 0x0C, 0x0C }, { 0x10, 0x20 }, { 0x14, 0x24 }, { 0x18, 0x28 }, { 0x1C, 0x10 },
    { 0x20, 0x5C }, { 0x2C, 0x60 }, { 0x30, 0x64 }, { 0x34, 0x68 }, { 0x38, 0x6C },
};
static const unsigned char todMap12[][2] = {
    { 0x04, 0x04 }, { 0x08, 0x08 }, { 0x0C, 0x0C }, { 0x10, 0x10 }, { 0x14, 0x14 }, { 0x18, 0x18 }, { 0x1C, 0x1C },
    { 0x20, 0x20 }, { 0x24, 0x24 }, { 0x28, 0x28 }, { 0x2C, 0x38 }, { 0x30, 0x3C }, { 0x34, 0x40 }, { 0x38, 0x44 },
    { 0x3C, 0x48 }, { 0x40, 0x5C }, { 0x4C, 0x60 }, { 0x50, 0x64 }, { 0x54, 0x68 }, { 0x58, 0x6C },
};
static const unsigned char todMap13[][2] = {
    { 0x04, 0x04 }, { 0x08, 0x08 }, { 0x0C, 0x0C }, { 0x10, 0x10 }, { 0x14, 0x14 }, { 0x18, 0x18 }, { 0x1C, 0x1C },
    { 0x20, 0x20 }, { 0x24, 0x24 }, { 0x28, 0x28 }, { 0x2C, 0x38 }, { 0x30, 0x3C }, { 0x34, 0x40 }, { 0x38, 0x44 },
    { 0x3C, 0x48 }, { 0x40, 0x4C }, { 0x44, 0x5C }, { 0x48, 0x60 }, { 0x4C, 0x64 }, { 0x50, 0x68 }, { 0x54, 0x6C },
    { 0x58, 0x70 }, { 0x5C, 0x74 }, { 0x60, 0x78 }, { 0x64, 0x7C },
};
static inline void todCopyKeys(void *data, int size, const unsigned char (*map)[2], int n)
{
    int i, k;
    for (i = 0; i < 12; i++) {
        unsigned *src = (unsigned *)((char *)data + i * size);
        unsigned *dst = (unsigned *)&todInfo[i];
        for (k = 0; k < n; k++)
            dst[map[k][1] / 4] = src[map[k][0] / 4];
    }
}
/* Loads the level's time of day: on (0/1), speed in game minutes per second (0..60, else 30) and, by `version`,
 * 12 keys in that format. The hour of each key is never loaded: the defaults of todInit stay. */
void todSetTOD(int on, float minutesPerSecond, void *data, float version)
{
    todSetTimeOfDay = (unsigned)on < 2 ? on : 0;
    if (minutesPerSecond <= 60.0f && 0.0f <= minutesPerSecond)
        todTimeOfDayMinutesPerSecond = minutesPerSecond;
    else
        todTimeOfDayMinutesPerSecond = 30.0f;
    if (version == 1.0f)
        todCopyKeys(data, 0x24, todMap10, sizeof(todMap10) / 2);
    if (version == 1.1f)
        todCopyKeys(data, 0x3C, todMap11, sizeof(todMap11) / 2);
    if (version == 1.2f)
        todCopyKeys(data, 0x5C, todMap12, sizeof(todMap12) / 2);
    if (version == 1.3f)
        todCopyKeys(data, 0x68, todMap13, sizeof(todMap13) / 2);
    if (version == 1.4f) {
        int i, k;
        for (i = 0; i < 12; i++)
            for (k = 1; k < 32; k++)
                ((unsigned *)&todInfo[i])[k] = ((unsigned *)((char *)data + i * 0x80))[k];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/tod", todSetTOD__FifPvf);
#endif
/* game hours (0..24), -1 while off */
float todGetCurrentTOD(void)
{
    if (todActive())
        return todTimeOfDay;
    return -1.0f;
}
void todGetAmbient(float *r, float *g, float *b)
{
    *r = todAmbientRed;
    *g = todAmbientGreen;
    *b = todAmbientBlue;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 66/75 words, size 0x12c vs 0x12c */
/* registers a sky GROUP node (todSky[id2]) and keeps the load-time colors of its first 3 objects */
void todSetSkyEntry(_hierhead *node)
{
    if (todNumSkys < 10 && node->opcode == 1) {
        TodSky *s = &todSky[node->id2];
        int k;
        s->node = node;
        for (k = 0; k < skyKids(node); k++) {
            char *o = (char *)skyKid(node, k);
            if (((HierHead *)o)->opcode == 0 && k < 3) {
                s->color[k][0] = *(float *)(o + 0x40);
                s->color[k][1] = *(float *)(o + 0x44);
                s->color[k][2] = *(float *)(o + 0x48);
                s->color[k][3] = *(float *)(o + 0x4C);
                s->params[k][0] = *(float *)(o + 0x20);
                s->params[k][1] = *(float *)(o + 0x24);
                s->params[k][2] = *(float *)(o + 0x2C);
                s->params[k][3] = *(float *)(o + 0x28);
            }
        }
        todNumSkys++;
    }
    if (todSky[0].node && todSky[1].node && todSky[2].node && todSky[3].node)
        todSkySetFound = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/tod", todSetSkyEntry__FP9_hierhead);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 12/49 words, size 0xc4 vs 0xbc */
/* tints the objects of sky `idx` (no 3-object limit here, unlike todSetSkyEntry) */
void todSetSkyObjects(int idx, float alpha, float uvOffset, float burn, float r, float g, float b)
{
    TodSky *s = &todSky[idx];
    HierHead *n = s->node;
    int k;
    if (!n || n->opcode != 1)
        return;
    for (k = 0; k < skyKids(n); k++) {
        char *o = (char *)skyKid(n, k);
        if (((HierHead *)o)->opcode != 0)
            continue;
        *(float *)(o + 0x2C) = s->params[k][2] + burn;
        *(float *)(o + 0x28) = s->params[k][3] + uvOffset;
        *(float *)(o + 0x40) = s->color[k][0] * r;
        *(float *)(o + 0x44) = s->color[k][1] * g;
        *(float *)(o + 0x48) = s->color[k][2] * b;
        *(float *)(o + 0x4C) = s->color[k][3] * alpha;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/tod", todSetSkyObjects__Fiffffff);
#endif
void todSetPlightsActive(int view, int on)
{
    s_todPlightsEnabled[view] = on;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 0/36 words, size 0x80 vs 0x90 */
CTODLinkList::CTODLinkList()
{
    int i;
    _LinkNode *n = nodes;
    _todInfo14 *k = todInfo;
    tail = -1;
    numEntries = 0;
    head = -1;
    for (i = 11; i >= 0; i--, n++, k++) {
        ClearNode(n);
        n->data = k;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/tod", __12CTODLinkList);
#endif
CTODLinkList::~CTODLinkList()
{
}
#ifdef NON_MATCHING
/* equivalent (untuned): 25/27 words, size 0x6c vs 0x6c */
/* only ever takes the first node (head == -1) */
int CTODLinkList::AddEntry(void)
{
    if (numEntries < 12) {
        int i = FindFreeNode();
        if (i != -1 && head == -1) {
            tail = i;
            head = i;
            numEntries++;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/tod", AddEntry__12CTODLinkList);
#endif
void CTODLinkList::DeleteEntry(int)
{
}
int CTODLinkList::GetNumEntries(void)
{
    return numEntries;
}
int CTODLinkList::FindNodeByTime(float)
{
    return 0;
}
int CTODLinkList::FindFreeNode(void)
{
    int i;
    for (i = 0; i < 12; i++)
        if (nodes[i].prev == -1)
            return i;
    return -1;
}
void CTODLinkList::SortList(void)
{
}
#ifdef NON_MATCHING
/* equivalent (untuned): 3/7 words, size 0x1c vs 0x1c */
void CTODLinkList::ClearNode(_LinkNode *n)
{
    n->data = 0;
    n->b = -1;
    n->prev = -1;
    n->next = -1;
    n->a = -1;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/tod", ClearNode__12CTODLinkListP9_LinkNode);
#endif
INCLUDE_ASM("asm/nonmatchings/common/tod", __static_initialization_and_destruction_0_00225688);
INCLUDE_ASM("asm/nonmatchings/common/tod", _GLOBAL_$I$todLink);
INCLUDE_ASM("asm/nonmatchings/common/tod", _GLOBAL_$D$todLink);
