#include "common.h"
#include "game/game.h"
#include "game/enemy_info.h"
#include "task_manager.h"
#include "hieri_types.h"
#include "vecmath.h"

/* 8 bytes; copied with its own operator= */
struct PitchChange {
    float a, b;
    PitchChange &operator=(PitchChange &o);
};
/* 0x30 bytes. Its operator= skips the word at 0xC. */
struct ShakeData {
    int m_mode;      /* 0x0 */
    float m_04;      /* 0x4 */
    float m_08;      /* 0x8 */
    int m_0C;        /* 0xC: not copied */
    float m_10[4];   /* 0x10 */
    FVECTOR m_20;    /* 0x20 */
    ShakeData &operator=(ShakeData &o);
};
/* Per-POV camera parameters a monster carries (Monster +0x6CF0, 0xA0 bytes each; SetCameraPOV loads them into the camera).
   Its operator= skips 0x98 (an optional override: SetCameraPOV only takes it when nonzero). */
struct MonsterCameraData {
    FVECTOR m_00;            /* 0x00 -> camera +0xD10 */
    FVECTOR m_10;            /* 0x10 -> +0xD50 */
    FVECTOR m_20;            /* 0x20 -> +0xD60 */
    PitchChange m_pitch[3];  /* 0x30 -> +0xD70, +0xD78, +0xD80 */
    float m_48, m_4C, m_50, m_54;
    int m_58;
    float m_5C, m_60, m_64;
    int m_68, m_6C;
    float m_70, m_74, m_78, m_7C;
    int m_80, m_84, m_88;
    float m_8C, m_90, m_94, m_98, m_9C;
    MonsterCameraData &operator=(MonsterCameraData &o);
};
typedef char _size_MonsterCameraData[sizeof(MonsterCameraData) == 0xA0 ? 1 : -1];
_cs *viewGetViewCs(int view);
float (*viewGetMat(int view))[4];
_fvector *viewGetTrans(int view);
int viewGetScreenDisplay(void);
unsigned timerGetCurTics(void);
extern "C" float sinf(float);
extern "C" float cosf(float);
class MotionBlurAA {
public:
    static void setAlpha(float alpha, int view);
    static void setEnable(bool on);
};
/* td.cpp */
struct _tdResult {
    void *trigger;            /* 0x0: HierCollisionTrigger; +0x8 item count, +0x10 item pointers */
    unsigned short hatId1;    /* 0x4 */
    unsigned short hatId2;    /* 0x6 */
};
int tdTest(_fvector *p, _tdResult *res, int max);
float smoothEasyInAngle(float cur, float target, float rate, float minStep);
float hdHatTest(_fvector *pos, _fvector *normal, float up, bool option, float down);
extern int sShakeEnabled;
__asm__("#SNFIX_SMALL sShakeEnabled");
typedef unsigned int u_long128_t __attribute__((mode(TI)));
float mathfHeadingFromPointToPoint(_fvector *from, _fvector *to);
extern "C" int printf(const char *, ...);
#define FMIN(a, b) ({ float _r; __asm__ volatile("min.s %0,%1,%2" : "=f"(_r) : "f"(a), "f"(b) : "memory"); _r; })
void mathfRotationFromPointToPoint(_fvector *rot, _fvector *from, _fvector *to);
/* float (*)[4]: gcc 2.x mangles the bound minus one, so retail's FPA3_f */
void mathfRotMatrixRPH(float (*m)[4], _fvector *rph);
extern char camerasObj[] __asm__("_7Cameras$m_cameras");
extern int camerasNum __asm__("_7Cameras$m_numCameras");
extern int leaveTriggered __asm__("_7Cameras$m_leaveTriggered");
extern unsigned ticsAtUnifiedInit __asm__("_7Cameras$m_ticsAtUnifiedInit");
extern float currentFadeAlpha __asm__("_7Cameras$m_currentFadeAlpha");
extern unsigned leaveReason __asm__("_7Cameras$m_leaveReason");
/* Cameras::m_cameras starts with one Camera (0xEB0 bytes) per view. Camera fields used here:
     +0x004 int     state (3 = following a monster)
     +0x070/080/090 direction vectors reset when a monster is followed
     +0xCF0 _cs*    the camera's cs          +0xCF4 Monster* followed monster
     +0xCF8 int     POV                       +0xCFC int previous POV (ReattachCamera)
     +0xD20 FVECTOR detached position         +0xD88 float shake, +0xD8C shake falloff */
#define CAM(v) (camerasObj + (v) * 0xEB0)
#define CI(c, o) (*(int *)((c) + (o)))
#define CF(c, o) (*(float *)((c) + (o)))
#define CAM_CS(c) (*(_cs **)((c) + 0xCF0))
#define CAM_MONSTER(c) (*(Monster **)((c) + 0xCF4))
#define SET_XZY(v, X, Y, Z) do { _fvector *p_ = (v); p_->x = (X); p_->z = (Z); p_->y = (Y); } while (0)
#define SET_XYZ(v, X, Y, Z) do { _fvector *p_ = (v); p_->x = (X); p_->y = (Y); p_->z = (Z); } while (0)
#define vecNormalize3(v) do { float r_ = eeRsqrtf(1.0f, vecDot(v, v)); (v)->z *= r_; (v)->x *= r_; (v)->y *= r_; } while (0)
#define MI(p, o) (*(int *)((char *)(p) + (o)))
#define MF(p, o) (*(float *)((char *)(p) + (o)))

INCLUDE_ASM("asm/nonmatchings/game/Cameras", _6Camera$UPDATE_FUNK);
PitchChange &PitchChange::operator=(PitchChange &o)
{
    a = o.a;
    b = o.b;
    return *this;
}
#ifdef NON_MATCHING
/* equivalente: 18/19 palavras; o retail copia m_20 com o endereço da origem num registrador (addiu + lq 0), o nosso
   ee-gcc dobra o deslocamento no lq */
ShakeData &ShakeData::operator=(ShakeData &o)
{
    m_mode = o.m_mode;
    m_04 = o.m_04;
    m_08 = o.m_08;
    m_10[0] = o.m_10[0];
    m_10[1] = o.m_10[1];
    m_10[2] = o.m_10[2];
    m_10[3] = o.m_10[3];
    m_20 = o.m_20;
    return *this;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", __as__9ShakeDataR9ShakeData);
#endif
#ifdef NON_MATCHING
/* equivalente: mesmas cópias; o retail copia m_10/m_20 com o endereço da origem num registrador (addiu + lq 0) */
MonsterCameraData &MonsterCameraData::operator=(MonsterCameraData &o)
{
    m_00 = o.m_00;
    m_10 = o.m_10;
    m_20 = o.m_20;
    m_pitch[0] = o.m_pitch[0];
    m_pitch[1] = o.m_pitch[1];
    m_pitch[2] = o.m_pitch[2];
    m_48 = o.m_48;
    m_4C = o.m_4C;
    m_50 = o.m_50;
    m_54 = o.m_54;
    m_58 = o.m_58;
    m_5C = o.m_5C;
    m_60 = o.m_60;
    m_64 = o.m_64;
    m_68 = o.m_68;
    m_6C = o.m_6C;
    m_70 = o.m_70;
    m_74 = o.m_74;
    m_78 = o.m_78;
    m_7C = o.m_7C;
    m_80 = o.m_80;
    m_84 = o.m_84;
    m_88 = o.m_88;
    m_8C = o.m_8C;
    m_90 = o.m_90;
    m_94 = o.m_94;
    m_9C = o.m_9C;
    return *this;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", __as__17MonsterCameraDataR17MonsterCameraData);
#endif
/* Seconds (at 576 tics per second) since the unified view started; 0 when it is off. The tic counter runs down. */
#ifdef NON_MATCHING
/* equivalente: 12/31 palavras; mesmo código, o b do caminho com sinal fica antes do cvt.s.w */
float Cameras::GetUnifiedTime(void)
{
    if (gUseUnifiedView)
        return (float)(ticsAtUnifiedInit - timerGetCurTics()) / 576.0f;
    return 0.0f;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", GetUnifiedTime__7Cameras);
#endif
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
/* Makes the camera of `view` chase `m`: resets its three direction vectors and reloads the current POV from m's table. */
#ifdef NON_MATCHING
/* equivalente: 6/35 palavras; mesmas gravações, o retail copia view para a2 e calcula os três endereços de vetor antes */
void Cameras::SetCameraToFollowMonster(int view, Monster *m)
{
    char *c = CAM(view);

    CAM_MONSTER(c) = m;
    m->m_cameraFollows = 1;
    m->m_cameraView = view;
    SET_XZY((_fvector *)(c + 0x70), 0.0f, -1.0f, 0.0f);
    SET_XZY((_fvector *)(c + 0x80), -1.0f, 0.0f, 0.0f);
    SET_XZY((_fvector *)(c + 0x90), 1.0f, 0.0f, 0.0f);
    CI(c, 0x4) = 3;
    SetCameraPOV(view, (Camera::CameraPOV)CI(c, 0xCF8));
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", SetCameraToFollowMonster__7CamerasiP7Monster);
#endif
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
/* Like AlignCsWithView, but relative to the camera's own cs. */
#ifdef NON_MATCHING
/* equivalente: 30/44 palavras; ordem das gravações de v e registradores */
void Cameras::AlignCsWithCamera(int view, _cs *cs, float dist, float x, float z)
{
    FVECTOR v;
    FVECTOR t;
    _cs *cam = CAM_CS(CAM(view));

    v.x = dist / 1.5f * x;
    v.y = dist;
    v.z = dist * 0.5f * z;
    v.w = 0.0f;
    matCopy(cs->mat, cam->mat);
    vecApplyRows(&t, cam->mat, &v);
    vecAdd(&cs->trans, &t, &cam->trans);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", AlignCsWithCamera__7CamerasiP3_csfff);
#endif
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
/* Leaves the camera of `view` parked at `pos` (or where it is, when pos is null) in POV 3, looking at the followed monster. */
#ifdef NON_MATCHING
/* equivalente: 8/39 palavras; o retail copia _fvector com o endereço da origem num registrador (addiu + lq 0), o nosso ee-gcc dobra o deslocamento no lq */
void Cameras::DetachCamera(int view, _fvector *pos)
{
    char *c = CAM(view);

    if (pos == 0) {
        *(FVECTOR *)(c + 0xD20) = CAM_CS(c)->trans;
    } else {
        CAM_CS(c)->trans = *pos;
        *(FVECTOR *)(c + 0xD20) = *pos;
    }
    SetCameraPOV(view, Camera::POV_3);
    mathfRotationFromPointToPoint(&CAM_CS(c)->rot, &CAM_CS(c)->trans, &CAM_MONSTER(c)->m_cs->trans);
    CAM_CS(c)->rot.y = 0.0f;
    mathfRotMatrixRPH(CAM_CS(c)->mat, &CAM_CS(c)->rot);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", DetachCamera__7CamerasiP8_fvector);
#endif
void Cameras::ReattachCamera(int view)
{
    char *c = CAM(view);

    SetCameraPOV(view, (Camera::CameraPOV)CI(c, 0xCFC));
    CI(c, 0xD98) = 1;
}
/* Shakes every camera that has `pos` in view. */
void Cameras::StartShake(_fvector &pos, float f)
{
    int i;
    char *c = CAM(0);

    for (i = 0; i < camerasNum; i++, c += 0xEB0) {
        if (((Camera *)c)->posInFOV(pos, 0, true))
            CF(c, 0xD88) = f;
    }
}
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
/* Close-combat ("unified") camera: camera 2 frames a and b in POV 1. The first time the unified view starts, it either
   fades in (two-monster games) or jumps from camera 0's place. Player-1 monsters get the +0x6728 flag. */
#ifdef NON_MATCHING
/* equivalente: 45/81 palavras; o retail copia _fvector com o endereço da origem num registrador (addiu + lq 0), o nosso ee-gcc dobra o deslocamento no lq */
void Cameras::InitCCCamera(Monster *a, Monster *b)
{
    char *c = CAM(2);

    if (a->m_camUnify == 0 || b->m_camUnify == 0)
        return;
    *(Monster **)(c + 0xE0C) = a;
    CAM_MONSTER(c) = a;
    *(Monster **)(c + 0xE10) = b;
    SetCameraPOV(2, Camera::POV_1);
    ((Camera *)c)->checkNewPosition();
    currentFadeAlpha = 100.0f;
    if (gUseUnifiedView == 0) {
        gUseUnifiedView = 1;
        ticsAtUnifiedInit = timerGetCurTics();
        if (game->m_numMonsters == 2) {
            TaskManager::global.add(CCFadeIn, 0, 1);
        } else {
            CI(c, 0xD98) = 0;
            CI(c, 0xD9C) = 0;
            CAM_CS(c)->trans = CAM_CS(CAM(0))->trans;
            CAM_CS(c)->rot = CAM_CS(CAM(0))->rot;
            mathfRotMatrixRPH(CAM_CS(c)->mat, &CAM_CS(c)->rot);
            CI(c, 0xDE8) = 0;
            CI(c, 0xDE0) = 0;
            CI(c, 0xDE4) = 0;
        }
    }
    if (a->m_playerNum == 1)
        MI(a, 0x6728) = 1;
    if (b->m_playerNum == 1)
        MI(b, 0x6728) = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", InitCCCamera__7CamerasP7MonsterT1);
#endif
/* InitCCCamera after a throw: the thrower (a, or b when a->m_index is set) leads, and +0xE20 gets the direction
   perpendicular to a's heading (to its left or right). */
#ifdef NON_MATCHING
/* equivalente: 13/122 palavras; o retail copia _fvector com o endereço da origem num registrador (addiu + lq 0), o nosso ee-gcc dobra o deslocamento no lq; agendamento do sinf/cosf */
void Cameras::InitCCCameraFromThrow(Monster *a, Monster *b)
{
    char *c = CAM(2);

    if (a->m_camUnify == 0 || b->m_camUnify == 0)
        return;
    SetCameraPOV(2, Camera::POV_1);
    if (a->m_index == 0) {
        float s = sinf(a->m_cs->rot.z + 1.5707964f);
        SET_XYZ((_fvector *)(c + 0xE20), s, cosf(a->m_cs->rot.z + 1.5707964f), 0.0f);
        *(Monster **)(c + 0xE0C) = a;
        CAM_MONSTER(c) = a;
        *(Monster **)(c + 0xE10) = b;
    } else {
        float s = sinf(a->m_cs->rot.z - 1.5707964f);
        SET_XYZ((_fvector *)(c + 0xE20), s, cosf(a->m_cs->rot.z - 1.5707964f), 0.0f);
        *(Monster **)(c + 0xE0C) = b;
        CAM_MONSTER(c) = b;
        *(Monster **)(c + 0xE10) = a;
    }
    currentFadeAlpha = 100.0f;
    if (gUseUnifiedView == 0) {
        gUseUnifiedView = 1;
        ticsAtUnifiedInit = timerGetCurTics();
        if (game->m_numMonsters == 2) {
            TaskManager::global.add(CCFadeIn, 0, 1);
        } else {
            CI(c, 0xD98) = 0;
            CI(c, 0xD9C) = 0;
            CAM_CS(c)->trans = CAM_CS(CAM(0))->trans;
            CAM_CS(c)->rot = CAM_CS(CAM(0))->rot;
            mathfRotMatrixRPH(CAM_CS(c)->mat, &CAM_CS(c)->rot);
            CI(c, 0xDE8) = 0;
            CI(c, 0xDE0) = 0;
            CI(c, 0xDE4) = 0;
        }
    }
    if (a->m_playerNum == 1)
        MI(a, 0x6728) = 1;
    if (b->m_playerNum == 1)
        MI(b, 0x6728) = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", InitCCCameraFromThrow__7CamerasP7MonsterT1);
#endif
/* Sets the close-combat pair of camera 2 (either may be null); refused when a non-null one cannot unify. */
#ifdef NON_MATCHING
/* equivalente: 29/33 palavras; ordem das gravações de E10/E0C/CF4 */
void Cameras::InitCCMonsters(Monster *a, Monster *b)
{
    char *c = CAM(2);

    if (*(Monster **)(c + 0xE0C))
        MI(*(Monster **)(c + 0xE0C), 0x6728) = 0;
    if (*(Monster **)(c + 0xE10))
        MI(*(Monster **)(c + 0xE10), 0x6728) = 0;
    if (a && a->m_camUnify == 0)
        return;
    if (b && b->m_camUnify == 0)
        return;
    *(Monster **)(c + 0xE10) = b;
    *(Monster **)(c + 0xE0C) = a;
    CAM_MONSTER(c) = a;
    if (a && a->m_playerNum == 1)
        MI(a, 0x6728) = 1;
    if (b && b->m_playerNum == 1)
        MI(b, 0x6728) = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", InitCCMonsters__7CamerasP7MonsterT1);
#endif
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
/* Task: fades the motion blur of view 0 out by 4 per frame from m_currentFadeAlpha; stops when it reaches 0. */
unsigned Cameras::CCFadeIn(void)
{
    MotionBlurAA::setAlpha(currentFadeAlpha, 0);
    currentFadeAlpha -= 4.0f;
    if (currentFadeAlpha > 0.0f) {
        MotionBlurAA::setEnable(true);
        return 1;
    }
    MotionBlurAA::setEnable(false);
    return 0;
}
/* Same for both views. */
unsigned Cameras::CCFadeOut(void)
{
    MotionBlurAA::setAlpha(currentFadeAlpha, 0);
    MotionBlurAA::setAlpha(currentFadeAlpha, 1);
    currentFadeAlpha -= 4.0f;
    if (currentFadeAlpha > 0.0f) {
        MotionBlurAA::setEnable(true);
        return 1;
    }
    MotionBlurAA::setEnable(false);
    return 0;
}
/* Runs each camera's POV update, then (when collision is on, +0xD90) checks collision for cameras following player 1
   or a monster targeting player 1, and advances the POV clock (+0xE34). */
void Cameras::Update(void)
{
    int i;
    char *c = CAM(0);

    for (i = 0; i < camerasNum; i++, c += 0xEB0) {
        Camera *cam = (Camera *)c;

        (cam->*Camera::UPDATE_FUNK[CI(c, 0xCF8)])();
        if (CI(c, 0xD90)) {
            Monster *m = CAM_MONSTER(c);

            if (m) {
                if (m->m_playerNum == 1 || (m->m_target && ((Monster *)m->m_target)->m_playerNum == 1))
                    cam->checkCollision(i);
            }
        }
        CF(c, 0xE34) += 1.0f;
    }
}
/* Ends the close-combat view (except in game mode 8): camera 2 goes back to POV 3 and camera 0 either fades back
   (two-monster games) or takes over camera 2's place at once, cancelling a pending LeaveUnifiedTask. */
#ifdef NON_MATCHING
/* equivalente: 22/83 palavras; o retail copia _fvector com o endereço da origem num registrador (addiu + lq 0), o nosso ee-gcc dobra o deslocamento no lq */
void Cameras::LeaveUnifiedView(unsigned reason)
{
    if (game->m_gameMode == 8)
        return;
    leaveReason = reason;
    gUseUnifiedView = 0;
    InitCCMonsters(0, 0);
    SetCameraPOV(2, Camera::POV_3);
    currentFadeAlpha = 100.0f;
    if (game->m_numMonsters == 2) {
        TaskManager::global.add(CCFadeOut, 0, 1);
        return;
    }
    CAM_CS(CAM(0))->trans = CAM_CS(CAM(2))->trans;
    CAM_CS(CAM(0))->rot = CAM_CS(CAM(2))->rot;
    mathfRotMatrixRPH(CAM_CS(CAM(0))->mat, &CAM_CS(CAM(0))->rot);
    CI(CAM(0), 0xE60) = 1;
    CF(CAM(0), 0xE5C) = 1.0f;
    CF(CAM(0), 0xE40) = 1.0f / CF(CAM(0), 0xE44);
    CF(CAM(0), 0xE38) = CF(CAM(0), 0xDC4) / CF(CAM(0), 0xE44);
    CF(CAM(0), 0xE3C) = CF(CAM(0), 0xDCC) / CF(CAM(0), 0xE44);
    CI(CAM(0), 0xDE0) = 0;
    CI(CAM(0), 0xDE4) = 0;
    CI(CAM(0), 0xDE8) = 0;
    CI(CAM(0), 0xE34) = 0;
    if (*(void **)(CAM(2) + 0xE68)) {
        leaveTriggered = 0;
        TaskManager::global.remove(*(void **)(CAM(2) + 0xE68));
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", LeaveUnifiedView__7CamerasUi);
#endif
/* Task: waits until camera 2's monster is idle (or pinning a target while not falling), then leaves the close-combat
   view like LeaveUnifiedView without the fade. */
#ifdef NON_MATCHING
/* equivalente: 2/67 palavras; o retail copia _fvector com o endereço da origem num registrador (addiu + lq 0), o nosso ee-gcc dobra o deslocamento no lq; o retail guarda a base de m_cameras em s1 */
unsigned Cameras::LeaveUnifiedTask(void)
{
    Monster *m = CAM_MONSTER(CAM(2));

    if (!m->isIdle(7)) {
        if (!m->hasPinTarget())
            return 1;
        if (m->m_freeFalling)
            return 1;
    }
    gUseUnifiedView = 0;
    InitCCMonsters(0, 0);
    SetCameraPOV(2, Camera::POV_3);
    CAM_CS(CAM(0))->trans = CAM_CS(CAM(2))->trans;
    CAM_CS(CAM(0))->rot = CAM_CS(CAM(2))->rot;
    mathfRotMatrixRPH(CAM_CS(CAM(0))->mat, &CAM_CS(CAM(0))->rot);
    CI(CAM(0), 0xE60) = 1;
    CF(CAM(0), 0xE5C) = 1.0f;
    CF(CAM(0), 0xE40) = 1.0f / CF(CAM(0), 0xE44);
    CF(CAM(0), 0xE38) = CF(CAM(0), 0xDC4) / CF(CAM(0), 0xE44);
    CF(CAM(0), 0xE3C) = CF(CAM(0), 0xDCC) / CF(CAM(0), 0xE44);
    *(void **)(CAM(2) + 0xE68) = 0;
    CI(CAM(0), 0xDE0) = 0;
    CI(CAM(0), 0xDE4) = 0;
    CI(CAM(0), 0xDE8) = 0;
    CI(CAM(0), 0xE34) = 0;
    leaveTriggered = 0;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", LeaveUnifiedTask__7Cameras);
#endif
/* Nonzero when the close-combat pair has drifted apart: with two monsters, farther than `dist` (0: camera 2's +0xE00);
   otherwise when no living AI is within that range of the player-1 monster of the pair. */
#ifdef NON_MATCHING
/* equivalente: 18/130 palavras; agendamento do laço e dos testes de dist */
int Cameras::ccMonsNoLongerInRange(float dist)
{
    Camera cam = *(Camera *)CAM(2);
    char *c = cam.m_raw;
    Monster *a = *(Monster **)(c + 0xE0C);
    Monster *b = *(Monster **)(c + 0xE10);
    Monster *p;
    bool inRange;
    int i;

    if (a == 0 || b == 0)
        return 1;
    if (game->m_numMonsters == 2) {
        FVECTOR d;
        float r;

        vecSub(&d, &a->m_cs->trans, &b->m_cs->trans);
        r = dist;
        if (dist == 0.0f)
            r = CF(c, 0xE00);
        if (vecDot(&d, &d) < r * r)
            return 0;
        return 1;
    }
    if (CAM_MONSTER(c)->m_playerNum == 1)
        p = CAM_MONSTER(c);
    else if (b->m_playerNum == 1)
        p = b;
    else
        return 1;
    inRange = false;
    for (i = 0; i < game->m_numAIs; i++) {
        Monster *m = game->m_lists.ais[i];
        EnemyInfo::Info *info = EnemyInfo::getInfo(*p, *m);

        if (dist == 0.0f) {
            if (info->dist < CF(c, 0xE00) && !m->m_dead)
                inRange = true;
        } else {
            if (info->dist < dist && !m->m_dead)
                inRange = true;
        }
    }
    return !inRange;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", ccMonsNoLongerInRange__7Camerasf);
#endif
/* Nonzero when either monster of camera 2's pair moves (+0x240 > 0) towards the other, within 30 degrees
   (velocity direction at +0x1E0). */
#ifdef NON_MATCHING
/* equivalente: 4/134 palavras; registradores do produto escalar e da normalização */
int Cameras::ccMonsMovingTogether(void)
{
    Camera cam = *(Camera *)CAM(2);
    char *c = cam.m_raw;
    FVECTOR d;
    FVECTOR e;
    float dot;
    bool aMoving = false;
    bool bMoving = false;

#define CC_A (*(Monster **)(c + 0xE0C))
#define CC_B (*(Monster **)(c + 0xE10))
    vecSub(&d, &CC_B->m_cs->trans, &CC_A->m_cs->trans);
    vecNormalize3(&d);
    dot = vecDot((_fvector *)((char *)CC_A + 0x1E0), &d);
    if (cosf(0.5235988f) < dot && MF(CC_A, 0x240) > 0.0f)
        aMoving = true;
    vecSub(&e, &CC_A->m_cs->trans, &CC_B->m_cs->trans);
    vecNormalize3(&e);
    dot = vecDot((_fvector *)((char *)CC_B + 0x1E0), &e);
    if (cosf(0.5235988f) < dot && MF(CC_B, 0x240) > 0.0f)
        bMoving = true;
#undef CC_A
#undef CC_B
    return aMoving || bMoving;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", ccMonsMovingTogether__7Cameras);
#endif
/* Plant boss: camera 0 goes to POV 3 at the marker (item type 0xF) of the trigger volume it stands in, looking at
   its monster with its pitch offset (+0xD00); without a marker it stays put. */
#ifdef NON_MATCHING
/* equivalente: 69/85 palavras; o retail copia _fvector com o endereço da origem num registrador (addiu + lq 0), o nosso ee-gcc dobra o deslocamento no lq */
void Cameras::InitPlantBossCamera(void)
{
    char *c = CAM(0);
    FVECTOR pos;
    _tdResult res[8];
    int found = 0;
    int n;
    int i;
    int j;

    SetCameraPOV(0, Camera::POV_3);
    n = tdTest(&CAM_CS(c)->trans, res, 8);
    for (i = 0; i < n; i++) {
        char *t = (char *)res[i].trigger;

        for (j = 0; j < *(int *)(t + 0x8); j++) {
            int *item = ((int **)(t + 0x10))[j];

            if ((item[0] & 0x3F) == 0xF) {
                found = 1;
                pos.x = ((float *)item)[1];
                pos.y = ((float *)item)[2];
                pos.z = ((float *)item)[3];
            }
        }
    }
    if (found) {
        *(FVECTOR *)(c + 0xD20) = pos;
        CAM_CS(c)->trans = pos;
        mathfRotationFromPointToPoint(&CAM_CS(c)->rot, &CAM_CS(c)->trans, &CAM_MONSTER(c)->m_cs->trans);
        CAM_CS(c)->rot.x += CF(c, 0xD00);
        mathfRotMatrixRPH(CAM_CS(c)->mat, &CAM_CS(c)->rot);
    } else {
        *(FVECTOR *)(c + 0xD20) = CAM_CS(c)->trans;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", InitPlantBossCamera__7Cameras);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Cameras", closeCombatUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", roamUpdate__6Camera);
/* POV 3 (detached): eases the camera to its parked position (+0xD20) and turns it towards the followed monster. */
void Camera::staticUpdate(void)
{
    char *c = m_raw;
    FVECTOR rot;
    _cs *mcs;

    if (CAM_CS(c) == 0 || CAM_MONSTER(c) == 0) {
        CF(c, 0xD88) = 0.0f;
        return;
    }
    mcs = CAM_MONSTER(c)->m_cs;
    CAM_CS(c)->trans.x = smoothEasyIn(CAM_CS(c)->trans.x, CF(c, 0xD20), CF(c, 0xDC4), 0.1f);
    CAM_CS(c)->trans.y = smoothEasyIn(CAM_CS(c)->trans.y, CF(c, 0xD24), CF(c, 0xDC4), 0.1f);
    CAM_CS(c)->trans.z = smoothEasyIn(CAM_CS(c)->trans.z, CF(c, 0xD28), CF(c, 0xDC4), 0.1f);
    mathfRotationFromPointToPoint(&rot, &CAM_CS(c)->trans, &mcs->trans);
    CAM_CS(c)->rot.x = smoothEasyInAngle(CAM_CS(c)->rot.x, rot.x, CF(c, 0xDCC), 0.001f);
    CAM_CS(c)->rot.z = smoothEasyInAngle(CAM_CS(c)->rot.z, rot.z, CF(c, 0xDD0), 0.001f);
    CAM_CS(c)->rot.y = 0.0f;
    mathfRotMatrixRPH(CAM_CS(c)->mat, &CAM_CS(c)->rot);
    shakeCamera();
}
/* POV 4: follows the monster at the POV offset (+0xD10) in the frame at +0xE70, ramping the position and rotation
   rates (+0xDE0..0xDE8) up to their targets over the POV change (+0xE34 of +0xE44 ticks). */
#ifdef NON_MATCHING
/* equivalente: 105/123 palavras; alocação de FPR dos min.s */
void Camera::getupUpdate(void)
{
    char *c = m_raw;
    _cs *mcs;
    FVECTOR t;

    if (CAM_CS(c) == 0 || CAM_MONSTER(c) == 0) {
        CF(c, 0xD88) = 0.0f;
        return;
    }
    mcs = CAM_MONSTER(c)->m_cs;
    if (CF(c, 0xE34) < CF(c, 0xE44) && CI(c, 0xD9C) == 0) {
        CF(c, 0xDE0) = FMIN(CF(c, 0xDC4), CF(c, 0xDE0) + CF(c, 0xE38));
        CF(c, 0xDE4) = FMIN(CF(c, 0xDCC), CF(c, 0xDE4) + CF(c, 0xE3C));
        CF(c, 0xDE8) = FMIN(1.0f, CF(c, 0xDE8) + CF(c, 0xE40));
    } else if (CI(c, 0xD9C) == 0) {
        CF(c, 0xDE0) = CF(c, 0xDC4);
        CF(c, 0xDE4) = CF(c, 0xDCC);
        CF(c, 0xDE8) = 1.0f;
    } else if (CI(c, 0xD9C) == 1) {
        CI(c, 0xD9C) = 0;
        CF(c, 0xDE8) = 1.0f;
        CF(c, 0xDE0) = 1.0f;
        CF(c, 0xDE4) = 1.0f;
    }
    vecApplyRows(&t, (float (*)[4])(c + 0xE70), (_fvector *)(c + 0xD10));
    vecAdd((_fvector *)(c + 0xD20), &mcs->trans, &t);
    translateCamera(*(_fvector *)(c + 0xD20), CF(c, 0xDE0), false);
    mathfRotationFromPointToPoint(&t, &CAM_CS(c)->trans, &CAM_MONSTER(c)->m_cs->trans);
    t.x += CF(c, 0xD00);
    CAM_CS(c)->rot.x = smoothEasyInAngle(CAM_CS(c)->rot.x, t.x, CF(c, 0xDE4), 0.001f);
    CAM_CS(c)->rot.z = smoothEasyInAngle(CAM_CS(c)->rot.z, t.z, CF(c, 0xDE8), 0.001f);
    CAM_CS(c)->rot.y = 0.0f;
    mathfRotMatrixRPH(CAM_CS(c)->mat, &CAM_CS(c)->rot);
    shakeCamera();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", getupUpdate__6Camera);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Cameras", knockbackUpdate__6Camera);
/* POVs 6 and 8: looks straight down from the monster plus the POV offset (+0xD10, in the monster's frame unless
   monster +0xF8 is set), with the monster's heading. */
void Camera::topdownUpdate(void)
{
    char *c = m_raw;

    if (CAM_CS(c) == 0 || CAM_MONSTER(c) == 0) {
        CF(c, 0xD88) = 0.0f;
        return;
    }
    if (*((signed char *)CAM_MONSTER(c) + 0xF8)) {
        CAM_CS(c)->trans.x = smoothEasyIn(CAM_CS(c)->trans.x, CAM_MONSTER(c)->m_cs->trans.x + CF(c, 0xD10), CF(c, 0xDC4), 0.01f);
        CAM_CS(c)->trans.y = smoothEasyIn(CAM_CS(c)->trans.y, CAM_MONSTER(c)->m_cs->trans.y + CF(c, 0xD14), CF(c, 0xDC4), 0.01f);
        CAM_CS(c)->trans.z = smoothEasyIn(CAM_CS(c)->trans.z, CAM_MONSTER(c)->m_cs->trans.z + CF(c, 0xD18), CF(c, 0xDC4), 0.01f);
    } else {
        vecApplyRows((_fvector *)(c + 0xD20), CAM_MONSTER(c)->m_cs->mat, (_fvector *)(c + 0xD10));
        CAM_CS(c)->trans.x = smoothEasyIn(CAM_CS(c)->trans.x, CAM_MONSTER(c)->m_cs->trans.x + CF(c, 0xD20), CF(c, 0xDC4), 0.01f);
        CAM_CS(c)->trans.y = smoothEasyIn(CAM_CS(c)->trans.y, CAM_MONSTER(c)->m_cs->trans.y + CF(c, 0xD24), CF(c, 0xDC4), 0.01f);
        CAM_CS(c)->trans.z = smoothEasyIn(CAM_CS(c)->trans.z, CAM_MONSTER(c)->m_cs->trans.z + CF(c, 0xD28), CF(c, 0xDC4), 0.01f);
    }
    CAM_CS(c)->rot.x = smoothEasyInAngle(CAM_CS(c)->rot.x, -1.5707964f, CF(c, 0xDCC), 0.001f);
    CAM_CS(c)->rot.y = 0.0f;
    CAM_CS(c)->rot.z = CAM_MONSTER(c)->m_cs->rot.z;
    mathfRotMatrixRPH(CAM_CS(c)->mat, &CAM_CS(c)->rot);
    shakeCamera();
}
/* POV 7: places the camera at the POV offset (+0xD10) in a frame built from the monster's forward axis and world up,
   kept above the ground, looking at the monster's +0x3E30 point. */
#ifdef NON_MATCHING
/* equivalente: 25/95 palavras; o retail copia o eixo com o endereço num registrador (addiu + lq 0) e aloca outros FPR na normalização */
void Camera::victoryUpdate(void)
{
    char *c = m_raw;
    FVECTOR axes[4];
    FVECTOR up;

    if (CAM_CS(c) == 0 || CAM_MONSTER(c) == 0)
        return;
    *(u_long128_t *)&up = 0;
    up.z = 1.0f;
    axes[0] = *(FVECTOR *)CAM_MONSTER(c)->m_cs->mat[0];
    vecCross(&axes[1], &up, &axes[0]);
    vecNormalize3(&axes[1]);
    vecCross(&axes[2], &axes[0], &axes[1]);
    vecApplyRows((_fvector *)(c + 0xD20), (float (*)[4])axes, (_fvector *)(c + 0xD10));
    vecAdd(&CAM_CS(c)->trans, &CAM_MONSTER(c)->m_cs->trans, (_fvector *)(c + 0xD20));
    {
        float h = hdHatTest(&CAM_CS(c)->trans, (_fvector *)CAM_CS(c)->mat[2], 100.0f, false, 4096.0f);

        if (h < 0.0f)
            CAM_CS(c)->trans.z -= h;
    }
    mathfRotationFromPointToPoint(&CAM_CS(c)->rot, &CAM_CS(c)->trans, (_fvector *)((char *)CAM_MONSTER(c) + 0x3E30));
    mathfRotMatrixRPH(CAM_CS(c)->mat, &CAM_CS(c)->rot);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", victoryUpdate__6Camera);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Cameras", crushUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", normalUpdate__6Camera);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", checkCollision__6Camerai);
/* Close-combat camera: +0xE20 becomes the direction across the pair. With a single monster, the camera offset
   (+0xD10, mirrored to the side the other monster is on) is put in the first monster's frame instead. */
#ifdef NON_MATCHING
/* equivalente: 9/125 palavras; o retail calcula o ponto médio (não usado) e agenda os sinf/cosf de outro jeito */
void Camera::checkNewPosition(void)
{
    char *c = m_raw;
    Monster *a = *(Monster **)(c + 0xE0C);
    Monster *b = *(Monster **)(c + 0xE10);
    FVECTOR mid;
    FVECTOR d;
    float heading;

    mid.x = (a->m_cs->trans.x + b->m_cs->trans.x) * 0.5f;
    mid.y = (a->m_cs->trans.y + b->m_cs->trans.y) * 0.5f;
    mid.z = (a->m_cs->trans.z + b->m_cs->trans.z) * 0.5f;
    heading = mathfHeadingFromPointToPoint(&a->m_cs->trans, &b->m_cs->trans) + 1.5707964f;
    {
        float s = sinf(heading);

        SET_XYZ((_fvector *)(c + 0xE20), s, cosf(heading), 0.0f);
    }
    if (game->m_numMonsters == 1) {
        vecApplyRows((_fvector *)(c + 0xE20), a->m_cs->mat, (_fvector *)(c + 0xD10));
        vecSub(&d, &b->m_cs->trans, &a->m_cs->trans);
        if (0.0f < a->m_cs->mat[1][0] * d.y - a->m_cs->mat[1][1] * d.x) {
            CF(c, 0xD10) = -CF(c, 0xD10);
            vecApplyRows((_fvector *)(c + 0xE20), a->m_cs->mat, (_fvector *)(c + 0xD10));
            CF(c, 0xD10) = -CF(c, 0xD10);
        }
        if (CF(c, 0xE28) < 0.0f)
            printf("Trying to place unified cam below ground\n");
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Cameras", checkNewPosition__6Camera);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Cameras", posInFOV__6CameraR8_fvectorib);
INCLUDE_ASM("asm/nonmatchings/game/Cameras", translateCamera__6CameraR8_fvectorfb);
/* While the shake amount (+0xD88) is above 1 (and shaking is enabled): jolts the camera sideways by a random amount
   in the camera's frame (offset kept at +0xD30) plus a random rise, then lets the amount fall off (+0xD8C). */
void Camera::shakeCamera(void)
{
    char *c = m_raw;
    FVECTOR v;
    float z;

    if (1.0f < CF(c, 0xD88) && sShakeEnabled) {
        CF(c, 0xD34) = 0.0f;
        CF(c, 0xD30) = mathfRandf(-CF(c, 0xD88), CF(c, 0xD88));
        CF(c, 0xD38) = 0.0f;
        vecApplyRows(&v, CAM_CS(c)->mat, (_fvector *)(c + 0xD30));
        z = CF(c, 0xD38);
        v.z += mathfRandf(z, CF(c, 0xD88) * 0.5f);
        vecAdd(&CAM_CS(c)->trans, &CAM_CS(c)->trans, &v);
        CF(c, 0xD88) = smoothEasyIn(CF(c, 0xD88), z, CF(c, 0xD8C), 0.0001f);
    }
}
unsigned Cameras::CCFadeIn(void *p)
{
    return ((Cameras *)p)->CCFadeIn();
}
unsigned Cameras::CCFadeOut(void *p)
{
    return ((Cameras *)p)->CCFadeOut();
}
unsigned Cameras::LeaveUnifiedTask(void *p)
{
    return ((Cameras *)p)->LeaveUnifiedTask();
}
