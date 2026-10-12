#include "common.h"
#include "hieri_types.h"
#include "vecmath.h"

extern int visLength, doParaLights, visualizeLights;
extern float paraL[4][4];
extern float paraLightR[3], paraLightG[3], paraLightB[3], paraLightH[3], paraLightP[3];
extern float paraTODLightR[3], paraTODLightG[3], paraTODLightB[3], paraTODLightH[3], paraTODLightP[3];
extern float specularColor;
extern float vu1LightColor[4][4] __asm__("D_0025B238");
extern unsigned char paraLightColor1[4], paraLightColor2[4], paraLightColor3[4];
typedef unsigned int u128 __attribute__((mode(TI)));
int todActive(void);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rph);
void mathfHPtoVector(_fvector *v, float h, float p);
_cs *viewGetViewCs(int view);
class DebugDraw {
public:
    static void drawNormal(_fvector &from, _fvector &dir, float len, unsigned color);
};

void plightUpdateParaLights(void);

void plightInit(void)
{
    visLength = 10;
    doParaLights = 0;
    visualizeLights = 0;
    plightUpdateParaLights();
}
void plightParaActivate(void)
{
    doParaLights = 1;
}
int plightParaActive(void)
{
    return doParaLights;
}
void plightParaDeactivate(void)
{
    doParaLights = 0;
}
float (*plightGetParaLight(void))[4]
{
    return paraL;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 6/158 words, size 0x218 vs 0x278 */
#define DEG2RAD 0.017453292f
/* The three directional ("para") lights: colors (1/128 units) into the VU1 light color matrix, directions (heading/pitch,
   x32) into paraL's columns. The time-of-day set replaces both when the TOD system is active. */
void plightUpdateParaLights(void)
{
    FVECTOR up, rph, d;
    float m[4][4];
    float *R, *G, *B;
    int i;

    *(u128 *)&up = 0;
    up.w = 1.0f;
    up.y = 1.0f;
    if (todActive()) {
        R = paraTODLightR;
        G = paraTODLightG;
        B = paraTODLightB;
    } else {
        R = paraLightR;
        G = paraLightG;
        B = paraLightB;
    }
    for (i = 0; i < 3; i++) {
        vu1LightColor[i][0] = R[i] * 0.0078125f;
        vu1LightColor[i][1] = G[i] * 0.0078125f;
        vu1LightColor[i][2] = B[i] * 0.0078125f;
    }
    for (i = 0; i < 3; i++) {
        float p, h;

        if (todActive()) {
            p = paraTODLightP[i];
            h = paraTODLightH[i];
        } else {
            p = paraLightP[i];
            h = paraLightH[i];
        }
        rph.y = 0.0f;
        rph.x = p * DEG2RAD;
        rph.z = h * DEG2RAD;
        mathfRotMatrixRPH(m, &rph);
        vecApplyRows(&d, m, &up);
        paraL[0][i] = d.x * 32.0f;
        paraL[1][i] = d.y * 32.0f;
        paraL[2][i] = d.z * 32.0f;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/plight", plightUpdateParaLights__Fv);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 5/76 words, size 0x124 vs 0x130 */
/* Specular: the half vector between light 0 and the view direction of `view` goes into paraL's fourth column, the
   specular color into the fourth row of the VU1 light colors. */
void pLightCompSpecular(int view)
{
    FVECTOR down, e, l, h;
    _cs *cs;
    float inv;

    *(u128 *)&down = 0;
    down.y = -1.0f;
    down.w = 1.0f;
    cs = viewGetViewCs(view);
    vecApplyRows(&e, cs->mat, &down);
    l.x = paraL[0][0] * 0.03125f;
    l.y = paraL[1][0] * 0.03125f;
    l.z = paraL[2][0] * 0.03125f;
    vecAdd(&h, &l, &e);
    inv = 1.0f / eeSqrtf(h.x * h.x + h.y * h.y + h.z * h.z);
    vu1LightColor[3][3] = 0.0f;
    vu1LightColor[3][2] = specularColor;
    vu1LightColor[3][0] = specularColor;
    vu1LightColor[3][1] = specularColor;
    paraL[2][3] = h.z * inv * 32.0f;
    paraL[0][3] = h.x * inv * 32.0f;
    paraL[1][3] = h.y * inv * 32.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/plight", pLightCompSpecular__Fi);
#endif
void plightGetParaLight(int i, float *r, float *g, float *b, float *h, float *p)
{
    *r = paraLightR[i];
    *g = paraLightG[i];
    *b = paraLightB[i];
    *h = paraLightH[i];
    *p = paraLightP[i];
}
void plightSetParaLight(int i, float r, float g, float b, float h, float p)
{
    paraLightR[i] = r;
    paraLightG[i] = g;
    paraLightB[i] = b;
    paraLightH[i] = h;
    paraLightP[i] = p;
}
void plightSetTODParaLight(int i, float r, float g, float b, float h, float p)
{
    if ((unsigned)i < 3) {
        paraTODLightR[i] = r;
        paraTODLightG[i] = g;
        paraTODLightB[i] = b;
        paraTODLightH[i] = h;
        paraTODLightP[i] = p;
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 4/193 words, size 0x304 vs 0x2c4 */
/* Debug: draws the world axes and the three light directions (in their colors) at the origin. */
void plightUpdateLights(void)
{
    FVECTOR o, ax, ay, az, d[3];
    int i;

    if (!visualizeLights)
        return;
    o.x = 0.0f;
    o.y = 0.0f;
    o.z = 0.0f;
    ax.x = 1.0f;
    ax.y = 0.0f;
    ax.z = 0.0f;
    ay.x = 0.0f;
    ay.y = 1.0f;
    ay.z = 0.0f;
    az.x = 0.0f;
    az.y = 0.0f;
    az.z = 1.0f;
    DebugDraw::drawNormal(o, ax, visLength, 0xFF202080);
    DebugDraw::drawNormal(o, ay, visLength, 0x20FF2080);
    DebugDraw::drawNormal(o, az, visLength, 0x2020FF80);
    plightUpdateParaLights();
    if (todActive()) {
        for (i = 0; i < 3; i++)
            mathfHPtoVector(&d[i], paraTODLightH[i], paraTODLightP[i]);
    } else {
        for (i = 0; i < 3; i++)
            mathfHPtoVector(&d[i], paraLightH[i], paraLightP[i]);
    }
    paraLightColor1[3] = (int)paraLightR[0];
    paraLightColor1[1] = (int)paraLightB[0];
    paraLightColor1[2] = (int)paraLightG[0];
    paraLightColor2[3] = (int)paraLightR[1];
    paraLightColor2[2] = (int)paraLightG[1];
    paraLightColor2[1] = (int)paraLightB[1];
    paraLightColor3[3] = (int)paraLightR[2];
    paraLightColor3[2] = (int)paraLightG[2];
    paraLightColor3[1] = (int)paraLightB[2];
    DebugDraw::drawNormal(o, d[0], visLength, *(unsigned *)paraLightColor1);
    DebugDraw::drawNormal(o, d[1], visLength, *(unsigned *)paraLightColor2);
    DebugDraw::drawNormal(o, d[2], visLength, *(unsigned *)paraLightColor3);
}
#else
INCLUDE_ASM("asm/nonmatchings/common/plight", plightUpdateLights__Fv);
#endif
void pLightSetSpecularIntens(float f)
{
    specularColor = f;
}
