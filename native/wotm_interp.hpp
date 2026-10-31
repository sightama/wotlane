// ABOUTME: Row-vector matrices (M4), the game camera read from RAM (GameCamera) and the math for frame interpolation
// ABOUTME: (Scene::showBlend): no raylib, for the unit tests (native/tests). Included by wotm_native.hpp.
#pragma once

#include <cmath>

namespace wotm {

// ---------------------------------------------------------------- matrices (row vectors)

struct M4 {
    float m[4][4];
};

inline M4 ident() {
    M4 r{};
    for (int i = 0; i < 4; ++i) r.m[i][i] = 1.f;
    return r;
}

// Game camera (readGameCamera, wotm_native.hpp): eye, forward and up axes, vertical FOV
struct GameCamera {
    bool ok = false;
    float pos[3] = {0, 0, 0};
    float fwd[3] = {0, 1, 0};
    float up[3] = {0, 0, 1};
    float fovy = 63.7f;   // degrees
};

// ---------------------------------------------------------------- frame interpolation (Scene::showBlend)
// The logic runs at 60 and presentation at the monitor's rate: between two game frames the drawing blends the last two snapshots
// (A older, B newer). Matrix: scale per row (the code's convention: scaleM first), rotation by slerp, translation linear.
namespace interp {
struct Quat { float w, x, y, z; };

// Rows 0-2 = axes with scale. False if the normalized rows aren't orthogonal (shear): then the caller does a plain lerp.
// det < 0 (mirror): row 2 goes in with negative scale.
inline bool decompose(const M4 &m, Quat &q, float s[3]) {
    float R[3][3];
    for (int i = 0; i < 3; ++i) {
        s[i] = std::sqrt(m.m[i][0] * m.m[i][0] + m.m[i][1] * m.m[i][1] + m.m[i][2] * m.m[i][2]);
        if (!(s[i] > 1e-6f)) return false;
        for (int j = 0; j < 3; ++j) R[i][j] = m.m[i][j] / s[i];
    }
    for (int i = 0; i < 3; ++i) {
        const int j = (i + 1) % 3;
        if (std::fabs(R[i][0] * R[j][0] + R[i][1] * R[j][1] + R[i][2] * R[j][2]) > 0.02f) return false;
    }
    const float det = R[0][0] * (R[1][1] * R[2][2] - R[1][2] * R[2][1]) - R[0][1] * (R[1][0] * R[2][2] - R[1][2] * R[2][0]) +
                      R[0][2] * (R[1][0] * R[2][1] - R[1][1] * R[2][0]);
    if (det < 0.f) {
        s[2] = -s[2];
        for (int j = 0; j < 3; ++j) R[2][j] = -R[2][j];
    }
    // A = R^T (column matrix): A[r][c] = R[c][r]
    auto A = [&](int r, int c) { return R[c][r]; };
    const float tr = A(0, 0) + A(1, 1) + A(2, 2);
    if (tr > 0.f) { const float t = std::sqrt(tr + 1.f) * 2.f; q = {0.25f * t, (A(2, 1) - A(1, 2)) / t, (A(0, 2) - A(2, 0)) / t, (A(1, 0) - A(0, 1)) / t}; }
    else if (A(0, 0) > A(1, 1) && A(0, 0) > A(2, 2)) { const float t = std::sqrt(1.f + A(0, 0) - A(1, 1) - A(2, 2)) * 2.f; q = {(A(2, 1) - A(1, 2)) / t, 0.25f * t, (A(0, 1) + A(1, 0)) / t, (A(0, 2) + A(2, 0)) / t}; }
    else if (A(1, 1) > A(2, 2)) { const float t = std::sqrt(1.f + A(1, 1) - A(0, 0) - A(2, 2)) * 2.f; q = {(A(0, 2) - A(2, 0)) / t, (A(0, 1) + A(1, 0)) / t, 0.25f * t, (A(1, 2) + A(2, 1)) / t}; }
    else { const float t = std::sqrt(1.f + A(2, 2) - A(0, 0) - A(1, 1)) * 2.f; q = {(A(1, 0) - A(0, 1)) / t, (A(0, 2) + A(2, 0)) / t, (A(1, 2) + A(2, 1)) / t, 0.25f * t}; }
    return true;
}

inline Quat slerp(Quat a, Quat b, float t) {
    float d = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (d < 0.f) { b = {-b.w, -b.x, -b.y, -b.z}; d = -d; }
    float ka, kb;
    if (d > 0.9995f) { ka = 1.f - t; kb = t; }
    else { const float th = std::acos(d), sn = std::sin(th); ka = std::sin((1.f - t) * th) / sn; kb = std::sin(t * th) / sn; }
    Quat r{ka * a.w + kb * b.w, ka * a.x + kb * b.x, ka * a.y + kb * b.y, ka * a.z + kb * b.z};
    const float n = std::sqrt(r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z);
    if (n > 0.f) { r.w /= n; r.x /= n; r.y /= n; r.z /= n; }
    return r;
}

inline M4 lerpM4(const M4 &a, const M4 &b, float t) {
    M4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) r.m[i][j] = a.m[i][j] + (b.m[i][j] - a.m[i][j]) * t;   // translation, column 3 and the no-decomposition case
    Quat qa, qb;
    float sa[3], sb[3];
    if (decompose(a, qa, sa) && decompose(b, qb, sb) && (sa[2] < 0.f) == (sb[2] < 0.f)) {
        const Quat q = slerp(qa, qb, t);
        const float x = q.x, y = q.y, z = q.z, w = q.w;
        const float A[3][3] = {{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
                               {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
                               {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}};
        for (int i = 0; i < 3; ++i) {
            const float s = sa[i] + (sb[i] - sa[i]) * t;
            for (int j = 0; j < 3; ++j) r.m[i][j] = A[j][i] * s;   // back to row vectors
        }
    }
    return r;
}

inline void orthonormal(float fwd[3], float up[3]) {
    float n = std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
    if (n > 1e-6f) for (int i = 0; i < 3; ++i) fwd[i] /= n;
    const float d = up[0] * fwd[0] + up[1] * fwd[1] + up[2] * fwd[2];
    for (int i = 0; i < 3; ++i) up[i] -= d * fwd[i];
    n = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    if (n > 1e-6f) for (int i = 0; i < 3; ++i) up[i] /= n;
}

// Camera cut between A and B (no blending): teleport, sudden turn, FOV changing
inline bool cameraCut(const GameCamera &a, const GameCamera &b) {
    if (!a.ok || !b.ok) return true;
    const float dx = b.pos[0] - a.pos[0], dy = b.pos[1] - a.pos[1], dz = b.pos[2] - a.pos[2];
    const float dotf = a.fwd[0] * b.fwd[0] + a.fwd[1] * b.fwd[1] + a.fwd[2] * b.fwd[2];
    return dx * dx + dy * dy + dz * dz > 400.f * 400.f || dotf < 0.7f || std::fabs(a.fovy - b.fovy) > 5.f;
}

// Position and FOV linear, axes by nlerp and re-orthonormalized (good enough for one frame's turn)
inline GameCamera lerpCamera(const GameCamera &a, const GameCamera &b, float t) {
    GameCamera r = b;
    for (int i = 0; i < 3; ++i) {
        r.pos[i] = a.pos[i] + (b.pos[i] - a.pos[i]) * t;
        r.fwd[i] = a.fwd[i] + (b.fwd[i] - a.fwd[i]) * t;
        r.up[i] = a.up[i] + (b.up[i] - a.up[i]) * t;
    }
    orthonormal(r.fwd, r.up);
    r.fovy = a.fovy + (b.fovy - a.fovy) * t;
    return r;
}

// Camera -> world (row vectors): rows = right, forward, up, eye
inline M4 cameraToWorld(const GameCamera &c) {
    float f[3] = {c.fwd[0], c.fwd[1], c.fwd[2]}, u[3] = {c.up[0], c.up[1], c.up[2]};
    orthonormal(f, u);
    M4 m = ident();
    const float r[3] = {f[1] * u[2] - f[2] * u[1], f[2] * u[0] - f[0] * u[2], f[0] * u[1] - f[1] * u[0]};
    for (int i = 0; i < 3; ++i) { m.m[0][i] = r[i]; m.m[1][i] = f[i]; m.m[2][i] = u[i]; m.m[3][i] = c.pos[i]; }
    return m;
}

// Inverse of a rigid matrix (orthonormal rotation + translation, row vectors)
inline M4 rigidInverse(const M4 &m) {
    M4 r = ident();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) r.m[i][j] = m.m[j][i];
    for (int j = 0; j < 3; ++j) r.m[3][j] = -(m.m[3][0] * r.m[0][j] + m.m[3][1] * r.m[1][j] + m.m[3][2] * r.m[2][j]);
    return r;
}

// Focal length of the game's projection: GS px (horizontal) and lines (vertical) per unit of tangent. From worldToScreenMat (16 floats, rows):
// row 0 = fh*right + c*forward, row 1 = -fv*up + c*forward, row 3 = forward; the focal length is the part perpendicular to row 3.
struct Focus { float fh = 384.f, fv = 180.48f; };
inline bool focusOf(const float *M, Focus &f) {
    const float *r3 = M + 12;
    const float n3 = r3[0] * r3[0] + r3[1] * r3[1] + r3[2] * r3[2];
    if (!(n3 > 0.5f) || !std::isfinite(n3)) return false;
    auto perp = [&](const float *r) {
        const float k = (r[0] * r3[0] + r[1] * r3[1] + r[2] * r3[2]) / n3;
        float s = 0.f;
        for (int i = 0; i < 3; ++i) s += (r[i] - k * r3[i]) * (r[i] - k * r3[i]);
        return std::sqrt(s);
    };
    const float fh = perp(M), fv = perp(M + 4);
    if (!(fh > 1.f && fv > 1.f) || !std::isfinite(fh) || !std::isfinite(fv)) return false;
    f.fh = fh;
    f.fv = fv;
    return true;
}
// world -> camera (right, forward, up) via cameraToWorld C; point = including the translation
inline void toCam(const M4 &C, const float p[3], bool point, float q[3]) {
    const float d[3] = {p[0] - (point ? C.m[3][0] : 0.f), p[1] - (point ? C.m[3][1] : 0.f), p[2] - (point ? C.m[3][2] : 0.f)};
    for (int k = 0; k < 3; ++k) q[k] = d[0] * C.m[k][0] + d[1] * C.m[k][1] + d[2] * C.m[k][2];
}
inline void fromCam(const M4 &C, const float q[3], bool point, float p[3]) {
    for (int i = 0; i < 3; ++i) p[i] = (point ? C.m[3][i] : 0.f) + q[0] * C.m[0][i] + q[1] * C.m[1][i] + q[2] * C.m[2][i];
}
// World point -> offset from the screen center (GS px, lines); false when behind the camera
inline bool project(const M4 &C, const Focus &f, const float w[3], float &x, float &y) {
    float q[3];
    toCam(C, w, true, q);
    if (q[1] < 1.f) return false;
    x = f.fh * q[0] / q[1];
    y = -f.fv * q[2] / q[1];
    return true;
}
}   // namespace interp

}  // namespace wotm
