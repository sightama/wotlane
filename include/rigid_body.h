#ifndef RIGID_BODY_H
#define RIGID_BODY_H

#include "hieri_types.h"

/* Rigid-body dynamics on top of a cs (common/RigidBody): the cs holds the orientation and the origin, the body its center of mass
   in local (cm) and world (x) space. State is momentum based (P, L); v and w are derived by computeAuxiliarys. Thrown debris and
   the StateThrowBack pieces use it. */
class RigidBody {
public:
    float invMass;              // 0x0
    unsigned pad04[3];          // 0x4
    FVECTOR cm;                 // 0x10 center of mass in cs space
    FVECTOR invInertia;         // 0x20 inverse of the diagonal body-space inertia tensor
    _cs *cs;                    // 0x30
    unsigned pad34[3];          // 0x34
    FVECTOR x;                  // 0x40 world center of mass
    FVECTOR P;                  // 0x50 linear momentum
    FVECTOR L;                  // 0x60 angular momentum
    FMATRIX Iinv;               // 0x70 world-space inverse inertia (3x3 used)
    FVECTOR v;                  // 0xB0 linear velocity (P * invMass)
    FVECTOR w;                  // 0xC0 angular velocity (Iinv * L)
    FVECTOR force;              // 0xD0 accumulated this step
    FVECTOR torque;             // 0xE0
    FVECTOR savedTrans;         // 0xF0 checkpoint/rollback copies
    FMATRIX savedMat;           // 0x100
    FVECTOR savedX;             // 0x140
    FVECTOR savedP;             // 0x150
    FVECTOR savedL;             // 0x160
    FMATRIX savedIinv;          // 0x170
    FVECTOR savedV;             // 0x1B0
    FVECTOR savedW;             // 0x1C0

    RigidBody();
    void setCenterOfMass(_fvector &c);
    void setCenterOfMass(float x, float y, float z);
    void setTensor(float ix, float iy, float iz);
    void updateFromCS(void);
    void setTrans(_fvector &t);
    void deltaTrans(_fvector &d);
    void setMatrix(float (&m)[4][4]);
    void initBlock(float sx, float sy, float sz, float mass);
    void initSphere(float r, float mass);
    void initCylindrical(float r, float h, int axis, float mass);
    void initEllipsoid(float sx, float sy, float sz, float mass);
    void reset(void);
    void clearForces(void);
    void applyGravForce(float g);
    void applyWorldForce(_fvector &f);
    void applyWorldForce(_fvector &f, _fvector &at);
    void applyLocalForce(_fvector &f);
    void applyLocalForce(_fvector &f, _fvector &at);
    void applyLocalTorque(_fvector &f, _fvector &at);
    void applyWorldTorque(_fvector &f, _fvector &at);
    void applyWorldImpulse(_fvector &j);
    void applyWorldImpulse(_fvector &j, _fvector &at);
    void applyLocalImpulse(_fvector &j);
    void applyLocalImpulse(_fvector &j, _fvector &at);
    int resolveCollision(RigidBody &o, _fvector &at, _fvector &n, float e, float mu1, float mu2);
    int resolveCollision(_fvector &at, _fvector &n, float e, float mu1, float mu2);
    int resolveCollision(_fvector &vOther, _fvector &at, _fvector &n, float e, float mu1, float mu2);
    int resolveFriction(_fvector &at, _fvector &n, float mu1, float mu2);
    void resolveForces(float dt);
    void computeAuxiliarys(void);
    void computeX(void);
    void getPointVelocity(_fvector &out, _fvector &at);
    void setLinearVelocity(_fvector &vel);
    void createSkewSymmetric(float (*m)[4], _fvector &a);
    void checkpoint(void);
    void rollback(void);
};
typedef char _size_RigidBody[sizeof(RigidBody) == 0x1D0 ? 1 : -1];

#endif
