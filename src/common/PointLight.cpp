#include "common.h"
#include "hieri_types.h"

void mathfMulVec(float (*m)[4], _fvector *v, _fvector *out);
_fvector *viewGetWeTrans(int view);
typedef unsigned int u128 __attribute__((mode(TI)));

class PointLight {
public:
    int active;
    char pad4[0xC];
    float x, y, z;
    float range;
    float r, g, b;
    float falloff;
    int f30;
    float intensity;
    int f38;
    int f3C;

    void Init(void);
    void SetPos(_fvector *p);
    void SetPos(float px, float py, float pz);
    void CompIntensity(float i);
    void SetRange(float rng);
    void CompIntensity(void);
    void SetColor(float cr, float cg, float cb);
};

class PointLights {
public:
    PointLight lights[32];
    int closest[8];         // 0x800 the 8 lights nearest the camera (GetClosestPts), -1 = none
    int count;
    int f824;
    char pad828[8];
    char pkt[0x10];         // 0x830 VU1 light packet: offset x[4] (here), y[4], z[4] (transposed), colors, falloffs
    float relY[4];          // 0x840
    float relZ[4];          // 0x850
    float color[3][4];      // 0x860 rgb of up to 3 lights
    float falloff[4];       // 0x890

    void Init(void);
    int Create(void);
    char *GetPktPtr(void);
    int RenderObject(void);
    void IntersectObject(float (*m)[4], _fvector *center, _fvector *eye, float radius, _fvector *off, _cs *cs);
    void GetClosestPts(int view);
};

#ifdef NON_MATCHING
/* equivalent (untuned): 21/23 words, size 0x5c vs 0x5c */
/* 21/23 words: order of the seven constant stores differs */
void PointLight::Init(void)
{
    z = 150.0f;
    g = 255.0f;
    falloff = 64.0f;
    range = 400.0f;
    intensity = 80.0f;
    b = 0;
    y = 0;
    r = 0;
    x = 0;
    active = 0;
    f38 = 0;
    f30 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/PointLight", Init__10PointLight);
#endif
void PointLight::SetPos(_fvector *p)
{
    x = p->x;
    y = p->y;
    z = p->z;
}
void PointLight::SetPos(float px, float py, float pz)
{
    x = px;
    y = py;
    z = pz;
}
void PointLight::CompIntensity(float i)
{
    intensity = i;
    falloff = range * range / (i * 16.0f);
}
void PointLight::SetRange(float rng)
{
    range = rng;
    falloff = rng * rng / (intensity * 16.0f);
}
void PointLight::CompIntensity(void)
{
    falloff = range * range / (intensity * 16.0f);
}
void PointLight::SetColor(float cr, float cg, float cb)
{
    r = cr;
    g = cg;
    b = cb;
}
void PointLights::Init(void)
{
    int i;

    count = 0;
    for (i = 0; i < 32; i++)
        lights[i].Init();
}
int PointLights::Create(void)
{
    int i;

    for (i = 0; i < 32; i++) {
        if (!lights[i].active) {
            lights[i].active = 1;
            return i;
        }
    }
    return -1;
}
char *PointLights::GetPktPtr(void)
{
    return pkt;
}
int PointLights::RenderObject(void)
{
    if (count) {
        if (f824)
            return 1;
    }
    return 0;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 2/164 words, size 0x290 vs 0x25c */
/* Fills the light packet for one object (bounding sphere center/radius in m's space, off: the object's offset) with up
   to 3 of the closest lights; lit (f824) says whether any of them reaches it. The offsets are written to slot j but read
   back from slot k (the closest[] index), as in retail: they differ only after an empty closest[] entry. */
void PointLights::IntersectObject(float (*m)[4], _fvector *center, _fvector *eye, float radius, _fvector *off, _cs *cs)
{
    float *relX = (float *)pkt;
    int i, j, k;

    f824 = 0;
    if (!count)
        return;
    if (cs && !cs->ptLightMe)
        return;
    if (4000.0f < radius)
        return;
    for (i = 0; i < 3; i++) {
        color[0][i] = 0.0f;
        color[1][i] = 0.0f;
        color[2][i] = 0.0f;
        falloff[i] = 0.0f;
    }
    j = 0;
    for (k = 0; k < 8; k++) {
        PointLight *l;
        FVECTOR w, t, d;

        if (!(j < 3) || closest[k] < 0)
            continue;
        l = &lights[closest[k]];
        w.x = off->x + l->x;
        w.y = off->y + l->y;
        w.z = off->z + l->z;
        mathfMulVec(m, &w, &t);
        relX[j] = eye->x - t.x;
        relY[j] = eye->y - t.y;
        relZ[j] = eye->z - t.z;
        d.x = center->x + relX[k];
        d.y = center->y + relY[k];
        d.z = center->z + relZ[k];
        if (d.x * d.x + d.y * d.y + d.z * d.z < radius * radius + l->range * l->range) {
            color[j][0] = l->r;
            color[j][1] = l->g;
            color[j][2] = l->b;
            falloff[j] = l->falloff;
            f824 = 1;
        }
        j++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/PointLight", IntersectObject__11PointLightsPA3_fP8_fvectorT2fT2P3_cs);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 19/150 words, size 0x258 vs 0x23c */
/* Picks the 8 enabled lights nearest the camera of `view` into closest[] and uploads their positions and colors to VU1
   data memory (count at 0x110042E0, positions from 0x110041E0, colors from 0x11004260). */
void PointLights::GetClosestPts(int view)
{
    _fvector *eye = viewGetWeTrans(view);
    float d2[32];
    int used[32];
    float best = 160000000.0f;
    int i, k, n;

    closest[0] = -1;
    for (i = 0; i < 32; i++) {
        used[i] = 0;
        if (lights[i].active && lights[i].f38) {
            FVECTOR v;

            v.x = eye->x + lights[i].x;
            v.y = eye->y + lights[i].y;
            v.z = eye->z + lights[i].z;
            d2[i] = v.x * v.x + v.y * v.y + v.z * v.z;
            if (d2[i] < best) {
                best = d2[i];
                closest[0] = i;
            }
        } else {
            d2[i] = -1.0f;
        }
    }
    if (closest[0] < 0) {
        *(volatile unsigned *)0x110042E0 = 0;
        return;
    }
    used[closest[0]] = 1;
    n = 1;
    for (k = 1; k < 8; k++) {
        best = 160000000.0f;
        closest[k] = -1;
        for (i = 0; i < 32; i++) {
            if (0.0f < d2[i] && d2[i] < best && !used[i]) {
                closest[k] = i;
                best = d2[i];
            }
        }
        if (closest[k] >= 0) {
            n++;
            used[closest[k]] = 1;
        }
    }
    *(volatile unsigned *)0x110042E0 = n;
    {
        u128 *pos = (u128 *)0x110041E0;
        u128 *col = (u128 *)0x11004260;

        for (k = 0; k < n; k++) {
            if (closest[k] < 0)
                continue;
            *pos++ = *(u128 *)&lights[closest[k]].x;
            *col++ = *(u128 *)&lights[closest[k]].r;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/PointLight", GetClosestPts__11PointLightsi);
#endif
