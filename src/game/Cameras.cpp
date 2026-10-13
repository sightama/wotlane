#include "common.h"
#include "task_manager.h"
#include "hieri_types.h"
#include "vecmath.h"

class Monster;
struct Camera {
    enum CameraPOV { POV_0, POV_1, POV_2, POV_3 };
};
/* 8 bytes; copied with its own (asm) operator= */
struct PitchChange {
    float a, b;
    PitchChange &operator=(PitchChange &o);
};
_cs *viewGetViewCs(int view);
float (*viewGetMat(int view))[4];
_fvector *viewGetTrans(int view);
int viewGetScreenDisplay(void);
/* Task callbacks (Pv) forward the task argument as `this` of the real member. */
class Cameras {
public:
    void CCFadeIn(void);
    void CCFadeOut(void);
    void LeaveUnifiedTask(void);
    static void CCFadeIn(void *p);
    static void CCFadeOut(void *p);
    static void LeaveUnifiedTask(void *p);
    static _cs *GetCsCameraFollows(int view);
    static void SetCameraPOV(int view, Camera::CameraPOV pov);
    static void ReattachCamera(int view);
    static void ResetPOVChange(int view, float t);
    static void SetCameraMonster(int view, Monster *m);
    static void StartShake(float f);
    static void ShakeFalloff(float f);
    static void TogglePOV(int view);
    static void InitCrushMonsters(Monster *a, Monster *b);
    static void Init(void);
    static void AlignCsWithView(int view, _cs *cs, float dist, float x, float z);
};
extern char camerasObj[] __asm__("_7Cameras$m_cameras");
extern int camerasNum __asm__("_7Cameras$m_numCameras");
extern int leaveTriggered __asm__("_7Cameras$m_leaveTriggered");
#define CAM(v) (camerasObj + (v) * 0xEB0)
#define CI(c, o) (*(int *)((c) + (o)))
#define CF(c, o) (*(float *)((c) + (o)))

INCLUDE_ASM("asm/nonmatchings/game/Cameras", _6Camera$UPDATE_FUNK);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", __as__11PitchChangeR11PitchChange);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", __as__9ShakeDataR9ShakeData);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", __as__17MonsterCameraDataR17MonsterCameraData);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", GetUnifiedTime__7Cameras);
#ifdef NON_MATCHING
/* 7/8 words: address arithmetic order */
_cs *Cameras::GetCsCameraFollows(int view)
{
    return *(_cs **)(*(char **)(camerasObj + view * 0xEB0 + 0xCF4) + 0xC);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", GetCsCameraFollows__7Camerasi);
#endif
/* Up to four cameras (one per view): each starts on its view's cs, following nobody, in POV 0, with default chase
   distances (300/450), angles (pi/4, pi/2), smoothing rates (0.09) and no shake. */
#ifdef NON_MATCHING
/* 29/112 palavras; mesmas gravações, ordem e registradores das constantes diferentes */
void Cameras::Init(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        char *c = CAM(i);
        _cs *cs;

        CI(c, 0xD98) = 1;
        CI(c, 0xD9C) = 0;
        CI(c, 0xDA0) = 0;
        cs = viewGetViewCs(i);
        *(_cs **)c = cs;
        *(_cs **)(c + 0xCF0) = cs;
        *(Monster **)(c + 0xCF4) = 0;
        *(Monster **)(c + 0xE0C) = 0;
        *(Monster **)(c + 0xE10) = 0;
        CI(c, 0xCF8) = 0;
        SetCameraPOV(i, Camera::POV_0);
        CI(c, 0xDA8) = 300;
        CI(c, 0xDAC) = 60;
        CF(c, 0xDB0) = 200.0f;
        CF(c, 0xDD0) = 0.018f;
        CI(c, 0xDF8) = 30;
        CF(c, 0xDFC) = 300.0f;
        CF(c, 0xE00) = 450.0f;
        CF(c, 0xE04) = 0.7853982f;
        CF(c, 0xE30) = 1.5707964f;
        CF(c, 0xE44) = 30.0f;
        CF(c, 0xD74) = 0.0f;
        CF(c, 0xD70) = 0.0f;
        CF(c, 0xD7C) = 0.0f;
        CF(c, 0xD78) = 0.0f;
        CF(c, 0xD84) = 0.0f;
        CF(c, 0xD80) = 0.0f;
        CF(c, 0xD88) = 0.0f;   /* shake */
        CF(c, 0xD8C) = 0.05f;  /* shake falloff */
        CI(c, 0xD90) = 1;
        CI(c, 0xD94) = 0;
        CI(c, 0xDA4) = 0;
        CF(c, 0xDB4) = 1000.0f;
        CF(c, 0xDB8) = 0.0f;
        CF(c, 0xDBC) = 0.17453292f; /* 10 degrees */
        CF(c, 0xDC0) = 0.17453292f;
        CF(c, 0xE48) = 1000.0f;
        CF(c, 0xDC4) = 0.09f;
        CF(c, 0xDC8) = 0.09f;
        CF(c, 0xDCC) = 0.09f;
        CF(c, 0xDD4) = 0.09f;
        CI(c, 0xDF4) = 0;
        CF(c, 0xE08) = 1000.0f;
        CF(c, 0xE34) = 0.0f;
        *(void **)(c + 0xE68) = 0;
    }
    leaveTriggered = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", Init__7Cameras);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Cameras", SetCameraToFollowMonster__7CamerasiP7Monster);
#ifdef NON_MATCHING
void Cameras::SetCameraMonster(int view, Monster *m)
{
    leaveTriggered = 0;
    *(Monster **)(CAM(view) + 0xCF4) = m;
    TaskManager::global.remove(*(void **)(CAM(view) + 0xE68));
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", SetCameraMonster__7CamerasiP7Monster);
#endif
/* Switches the camera of `view` to `pov` and loads that POV's parameters from the followed monster's table
   (Monster +0x6CF0: 0xA0 bytes per POV, a second set of 0x640 bytes for split screen). */
#ifdef NON_MATCHING
/* 14/149 palavras; mesmo fluxo, o retail usa um registrador salvo a menos */
void Cameras::SetCameraPOV(int view, Camera::CameraPOV pov)
{
    char *c = CAM(view);
    char *m = *(char **)(c + 0xCF4);

    if ((view == 2 && CI(c, 0xCF8) != 3 && pov != 3) || pov == 4 || CI(c, 0xCF8) == 6) {
        CI(c, 0xDE0) = 0;
        CI(c, 0xDE4) = 0;
        CI(c, 0xDE8) = 0;
        CI(c, 0xD98) = 0;
        CI(c, 0xD9C) = 0;
    } else {
        CI(c, 0xD98) = CI(c, 0xD9C) = 1;
    }
    CI(c, 0xE60) = 1;
    CI(c, 0xCFC) = CI(c, 0xCF8); /* previous POV, for ReattachCamera */
    CI(c, 0xCF8) = pov;
    CI(c, 0xE34) = 0;
    CI(c, 0xE64) = 0;
    CI(c, 0xDEC) = 0;
    CI(c, 0xDF0) = 0;
    CI(c, 0xD40) = 0;
    CI(c, 0xDA4) = 0;
    if (m) {
        m += 0x6CF0 + (viewGetScreenDisplay() != 0) * 0x640 + pov * 0xA0;

        CF(c, 0xD00) = CF(m, 0x48);
        *(FVECTOR *)(c + 0xD10) = *(FVECTOR *)m;
        *(FVECTOR *)(c + 0xD50) = *(FVECTOR *)(m + 0x10);
        *(FVECTOR *)(c + 0xD60) = *(FVECTOR *)(m + 0x20);
        *(PitchChange *)(c + 0xD70) = *(PitchChange *)(m + 0x30);
        *(PitchChange *)(c + 0xD78) = *(PitchChange *)(m + 0x38);
        *(PitchChange *)(c + 0xD80) = *(PitchChange *)(m + 0x40);
        CF(c, 0xDB8) = CF(m, 0x4C);
        CF(c, 0xDBC) = CF(m, 0x50);
        CF(c, 0xDC0) = CF(m, 0x54);
        CF(c, 0xDB4) = CF(m, 0x64);
        CF(c, 0xDB0) = CF(m, 0x60);
        CI(c, 0xDA8) = CI(m, 0x68);
        CI(c, 0xDAC) = CI(m, 0x6C);
        CF(c, 0xDC4) = CF(m, 0x74);
        CF(c, 0xDCC) = CF(m, 0x70);
        CF(c, 0xDD4) = CF(m, 0x78);
        CF(c, 0xDD0) = CF(m, 0x7C);
        CF(c, 0xE48) = CF(m, 0x5C);
        CI(c, 0xDF8) = CI(m, 0x88);
        CF(c, 0xDFC) = CF(m, 0x8C);
        CF(c, 0xE00) = CF(m, 0x90);
        CF(c, 0xE04) = CF(m, 0x94);
        if (CF(m, 0x98) != 0.0f)
            CF(c, 0xE08) = CF(m, 0x98);
        CF(c, 0xE44) = CF(m, 0x9C);
    }
    /* POV 1 with both crush monsters set: a monster whose m_playerNum is 1 gets +0x6728 = 1 */
    if (pov == 1) {
        char *a = *(char **)(c + 0xE0C);

        if (a && *(char **)(c + 0xE10)) {
            if (CI(a, 0x18) == pov)
                CI(a, 0x6728) = pov;
            a = *(char **)(c + 0xE10);
            if (CI(a, 0x18) == pov)
                CI(a, 0x6728) = pov;
        }
    }
    /* ResetPOVChange(view, +0xE44) */
    CF(c, 0xE5C) = 1.0f;
    CF(c, 0xE40) = 1.0f / CF(c, 0xE44);
    CF(c, 0xE38) = CF(c, 0xDC4) / CF(c, 0xE44);
    CF(c, 0xE3C) = CF(c, 0xDCC) / CF(c, 0xE44);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", SetCameraPOV__7CamerasiQ26Camera9CameraPOV);
#endif
/* Starts a POV transition lasting `t` ticks: weight 1, per-tick steps for the two interpolated values. */
void Cameras::ResetPOVChange(int view, float t)
{
    char *c = CAM(view);

    CF(c, 0xE5C) = 1.0f;
    CF(c, 0xE44) = t;
    CF(c, 0xE40) = 1.0f / t;
    CF(c, 0xE38) = CF(c, 0xDC4) / t;
    CF(c, 0xE3C) = CF(c, 0xDCC) / t;
}
INCLUDE_ASM("asm/nonmatchings/game/Cameras", AlignCsWithCamera__7CamerasiP3_csfff);
/* Puts cs in front of view's camera, oriented like it: `dist` ahead, x across (scaled by dist / 1.5) and z up
   (scaled by dist / 2) in screen units. */
#ifdef NON_MATCHING
/* 31/49 palavras; view e cs trocam de registrador salvo */
void Cameras::AlignCsWithView(int view, _cs *cs, float dist, float x, float z)
{
    FVECTOR v;
    FVECTOR t;

    v.x = dist / 1.5f * x;
    v.y = dist;
    v.z = dist * 0.5f * z;
    v.w = 0.0f;
    matCopy(cs->mat, viewGetMat(view));
    vecApplyRows(&t, viewGetMat(view), &v);
    vecAdd(&cs->trans, &t, viewGetTrans(view));
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", AlignCsWithView__7CamerasiP3_csfff);
#endif
/* Flips the camera of `view` between POV 0 and POV 3. */
void Cameras::TogglePOV(int view)
{
    char *c = CAM(view);

    CI(c, 0xD98) = 1;
    if (CI(c, 0xCF8) == 0)
        CI(c, 0xCF8) = 3;
    else
        CI(c, 0xCF8) = 0;
    SetCameraPOV(view, (Camera::CameraPOV)CI(c, 0xCF8));
}
INCLUDE_ASM("asm/nonmatchings/game/Cameras", DetachCamera__7CamerasiP8_fvector);
void Cameras::ReattachCamera(int view)
{
    char *c = CAM(view);

    SetCameraPOV(view, (Camera::CameraPOV)CI(c, 0xCFC));
    CI(c, 0xD98) = 1;
}
INCLUDE_ASM("asm/nonmatchings/game/Cameras", StartShake__7CamerasR8_fvectorf);
#ifdef NON_MATCHING
void Cameras::StartShake(float f)
{
    int i;

    for (i = 0; i < camerasNum; i++)
        CF(CAM(i), 0xD88) = f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", StartShake__7Camerasf);
#endif
#ifdef NON_MATCHING
void Cameras::ShakeFalloff(float f)
{
    int i;

    for (i = 0; i < camerasNum; i++)
        CF(CAM(i), 0xD8C) = f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", ShakeFalloff__7Camerasf);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Cameras", InitCCCamera__7CamerasP7MonsterT1);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", InitCCCameraFromThrow__7CamerasP7MonsterT1);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", InitCCMonsters__7CamerasP7MonsterT1);
#ifdef NON_MATCHING
/* 2/9 words: store order */
void Cameras::InitCrushMonsters(Monster *a, Monster *b)
{
    *(Monster **)(camerasObj + 0xE10) = b;
    *(Monster **)(camerasObj + 0xE0C) = a;
    *(Monster **)(camerasObj + 0xCF4) = a;
    *(int *)((char *)a + 0x6728) = 1;
    *(int *)((char *)b + 0x6728) = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", InitCrushMonsters__7CamerasP7MonsterT1);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Cameras", CCFadeIn__7Cameras);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", CCFadeOut__7Cameras);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", Update__7Cameras);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", LeaveUnifiedView__7CamerasUi);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", LeaveUnifiedTask__7Cameras);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", ccMonsNoLongerInRange__7Camerasf);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", ccMonsMovingTogether__7Cameras);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", InitPlantBossCamera__7Cameras);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", closeCombatUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", roamUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", staticUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", getupUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", knockbackUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", topdownUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", victoryUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", crushUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", normalUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", checkCollision__6Camerai);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", checkNewPosition__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", posInFOV__6CameraR8_fvectorib);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", translateCamera__6CameraR8_fvectorfb);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", shakeCamera__6Camera);
void Cameras::CCFadeIn(void *p)
{
    ((Cameras *)p)->CCFadeIn();
}
void Cameras::CCFadeOut(void *p)
{
    ((Cameras *)p)->CCFadeOut();
}
void Cameras::LeaveUnifiedTask(void *p)
{
    ((Cameras *)p)->LeaveUnifiedTask();
}
