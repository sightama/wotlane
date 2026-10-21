// ABOUTME: Matematica da interpolacao de quadros (so desenho): mistura de matrizes por escala/rotacao/translacao, camera com deteccao
// ABOUTME: de corte, HUD rebaseado na camera interpolada e o relogio de exibicao. Sem raylib nem jogo; testado em native/tests.
//
// Convencao das matrizes: vetores-linha, como o M4 de wotm_native.hpp (linhas 0-2 = eixos com a escala, linha 3 = translacao).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace wotm::interp
{
    using Mat = float[4][4];

    struct Quat
    {
        float w, x, y, z;
    };

    struct Camera
    {
        float pos[3] = {0, 0, 0};
        float fwd[3] = {0, 1, 0};
        float up[3] = {0, 0, 1};
        float fovy = 63.7f;
    };

    inline float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
    inline void cross3(const float *a, const float *b, float *o)
    {
        o[0] = a[1] * b[2] - a[2] * b[1];
        o[1] = a[2] * b[0] - a[0] * b[2];
        o[2] = a[0] * b[1] - a[1] * b[0];
    }
    inline void normalize3(float *v)
    {
        const float n = std::sqrt(dot3(v, v));
        if (n > 1e-12f)
            v[0] /= n, v[1] /= n, v[2] /= n;
    }

    // a primeiro, depois b (vetores-linha)
    inline void mul(const Mat a, const Mat b, Mat out)
    {
        Mat r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
            {
                float s = 0.f;
                for (int k = 0; k < 4; ++k)
                    s += a[i][k] * b[k][j];
                r[i][j] = s;
            }
        std::memcpy(out, r, sizeof(Mat));
    }

    inline Quat slerp(Quat a, Quat b, float t)
    {
        float d = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
        if (d < 0.f)   // caminho curto
            b = {-b.w, -b.x, -b.y, -b.z}, d = -d;
        float ka = 1.f - t, kb = t;
        if (d < 0.9995f)
        {
            const float th = std::acos(std::min(d, 1.f)), sn = std::sin(th);
            ka = std::sin((1.f - t) * th) / sn;
            kb = std::sin(t * th) / sn;
        }
        Quat r{ka * a.w + kb * b.w, ka * a.x + kb * b.x, ka * a.y + kb * b.y, ka * a.z + kb * b.z};
        const float n = std::sqrt(r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z);
        if (n > 0.f)
            r.w /= n, r.x /= n, r.y /= n, r.z /= n;
        return r;
    }

    // Rotacao de uma matriz com linhas ortonormais (linha i = imagem do eixo i) para quaternio e de volta
    inline Quat rowsToQuat(const float R[3][3])
    {
        float A[3][3];   // matriz-coluna = transposta
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                A[r][c] = R[c][r];
        const float tr = A[0][0] + A[1][1] + A[2][2];
        if (tr > 0.f)
        {
            const float t = std::sqrt(tr + 1.f) * 2.f;
            return {0.25f * t, (A[2][1] - A[1][2]) / t, (A[0][2] - A[2][0]) / t, (A[1][0] - A[0][1]) / t};
        }
        if (A[0][0] > A[1][1] && A[0][0] > A[2][2])
        {
            const float t = std::sqrt(1.f + A[0][0] - A[1][1] - A[2][2]) * 2.f;
            return {(A[2][1] - A[1][2]) / t, 0.25f * t, (A[0][1] + A[1][0]) / t, (A[0][2] + A[2][0]) / t};
        }
        if (A[1][1] > A[2][2])
        {
            const float t = std::sqrt(1.f + A[1][1] - A[0][0] - A[2][2]) * 2.f;
            return {(A[0][2] - A[2][0]) / t, (A[0][1] + A[1][0]) / t, 0.25f * t, (A[1][2] + A[2][1]) / t};
        }
        const float t = std::sqrt(1.f + A[2][2] - A[0][0] - A[1][1]) * 2.f;
        return {(A[1][0] - A[0][1]) / t, (A[0][2] + A[2][0]) / t, (A[1][2] + A[2][1]) / t, 0.25f * t};
    }

    inline void quatToRows(const Quat &q, float R[3][3])
    {
        const float x = q.x, y = q.y, z = q.z, w = q.w;
        const float A[3][3] = {{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
                               {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
                               {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                R[i][j] = A[j][i];
    }

    // Matriz = escala por linha * rotacao (+ espelho) + translacao. Falso se as linhas nao forem perpendiculares (cisalhamento) ou
    // alguma for nula: ai a mistura e por componente.
    struct Parts
    {
        float s[3];
        Quat q;
        bool mirror;
    };
    inline bool decompose(const Mat m, Parts &p)
    {
        float R[3][3];
        for (int i = 0; i < 3; ++i)
        {
            p.s[i] = std::sqrt(m[i][0] * m[i][0] + m[i][1] * m[i][1] + m[i][2] * m[i][2]);
            if (!(p.s[i] > 1e-6f))
                return false;
            for (int j = 0; j < 3; ++j)
                R[i][j] = m[i][j] / p.s[i];
        }
        for (int i = 0; i < 3; ++i)
            if (std::fabs(dot3(R[i], R[(i + 1) % 3])) > 0.02f)
                return false;
        float c[3];
        cross3(R[0], R[1], c);
        p.mirror = dot3(c, R[2]) < 0.f;
        if (p.mirror)
            for (int j = 0; j < 3; ++j)
                R[0][j] = -R[0][j];
        p.q = rowsToQuat(R);
        return true;
    }

    // Mistura de A (t = 0) a B (t = 1); nas pontas devolve a entrada exata (alfa 1 desenha o mesmo que sem interpolar)
    inline void lerpMatrix(const Mat a, const Mat b, float t, Mat out)
    {
        if (t <= 0.f)
            return void(std::memcpy(out, a, sizeof(Mat)));
        if (t >= 1.f)
            return void(std::memcpy(out, b, sizeof(Mat)));
        if (std::memcmp(a, b, sizeof(Mat)) == 0)
            return void(std::memcpy(out, b, sizeof(Mat)));
        Mat r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                r[i][j] = a[i][j] + (b[i][j] - a[i][j]) * t;   // translacao, coluna 3 e o caminho por componente
        Parts pa, pb;
        if (decompose(a, pa) && decompose(b, pb) && pa.mirror == pb.mirror)
        {
            float R[3][3];
            quatToRows(slerp(pa.q, pb.q, t), R);
            for (int i = 0; i < 3; ++i)
            {
                const float s = (pa.s[i] + (pb.s[i] - pa.s[i]) * t) * (i == 0 && pa.mirror ? -1.f : 1.f);
                for (int j = 0; j < 3; ++j)
                    r[i][j] = R[i][j] * s;
            }
        }
        std::memcpy(out, r, sizeof(Mat));
    }

    // vetor unitario de a para b pelo arco (slerp de vetores)
    inline void slerp3(const float *a, const float *b, float t, float *out)
    {
        const float d = std::clamp(dot3(a, b), -1.f, 1.f);
        float ka = 1.f - t, kb = t;
        if (d < 0.9995f)
        {
            const float th = std::acos(d), sn = std::sin(th);
            if (sn > 1e-6f)
                ka = std::sin((1.f - t) * th) / sn, kb = std::sin(t * th) / sn;
        }
        for (int i = 0; i < 3; ++i)
            out[i] = ka * a[i] + kb * b[i];
        normalize3(out);
    }

    // Corte de camera (sem mistura): salto de posicao, giro brusco ou zoom brusco entre dois quadros do jogo
    inline bool cameraCut(const Camera &a, const Camera &b)
    {
        float d[3] = {b.pos[0] - a.pos[0], b.pos[1] - a.pos[1], b.pos[2] - a.pos[2]};
        return dot3(d, d) > 400.f * 400.f || dot3(a.fwd, b.fwd) < 0.7f || std::fabs(b.fovy - a.fovy) > 5.f;
    }

    inline Camera lerpCamera(const Camera &a, const Camera &b, float t)
    {
        if (t >= 1.f)
            return b;
        if (t <= 0.f)
            return a;
        Camera r;
        for (int i = 0; i < 3; ++i)
            r.pos[i] = a.pos[i] + (b.pos[i] - a.pos[i]) * t;
        slerp3(a.fwd, b.fwd, t, r.fwd);
        slerp3(a.up, b.up, t, r.up);
        // cima perpendicular a frente
        const float k = dot3(r.up, r.fwd);
        for (int i = 0; i < 3; ++i)
            r.up[i] -= k * r.fwd[i];
        normalize3(r.up);
        r.fovy = a.fovy + (b.fovy - a.fovy) * t;
        return r;
    }

    // camera -> mundo: linhas = direita, frente, cima, posicao
    inline void cameraToWorld(const Camera &c, Mat V)
    {
        float right[3], up[3];
        cross3(c.fwd, c.up, right);
        normalize3(right);
        cross3(right, c.fwd, up);
        normalize3(up);
        for (int j = 0; j < 3; ++j)
            V[0][j] = right[j], V[1][j] = c.fwd[j], V[2][j] = up[j], V[3][j] = c.pos[j];
        V[0][3] = V[1][3] = V[2][3] = 0.f;
        V[3][3] = 1.f;
    }

    // inversa de rotacao + translacao
    inline void invertRigid(const Mat V, Mat out)
    {
        Mat r{};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                r[i][j] = V[j][i];
        for (int j = 0; j < 3; ++j)
            r[3][j] = -(V[3][0] * r[0][j] + V[3][1] * r[1][j] + V[3][2] * r[2][j]);
        r[3][3] = 1.f;
        std::memcpy(out, r, sizeof(Mat));
    }

    // HUD e o que o jogo cola na camera: M' = M_B * inv(V_B) * V_I (mesma pose relativa, na camera interpolada)
    inline void rebase(const Mat m, const Camera &b, const Camera &i, Mat out)
    {
        Mat vb, ivb, vi, t;
        cameraToWorld(b, vb);
        invertRigid(vb, ivb);
        cameraToWorld(i, vi);
        mul(m, ivb, t);
        mul(t, vi, out);
    }

    // Matriz mundo -> tela de B (W_B = inv(V_B) * C, C = camera -> tela) para a camera I: W_I = inv(V_I) * V_B * W_B. As particulas
    // projetam com a worldToScreenMat do jogo; trocada por esta, seguem a camera misturada como o resto da cena.
    inline void reprojectToCamera(const Mat wb, const Camera &b, const Camera &i, Mat out)
    {
        Mat vb, vi, ivi, t;
        cameraToWorld(b, vb);
        cameraToWorld(i, vi);
        invertRigid(vi, ivi);
        mul(ivi, vb, t);
        mul(t, wb, out);
    }

    // Posicao do que se mostra, em fields do jogo, entre dois instantaneos: 0 = A, 1 = B, sem extrapolar
    inline float alphaBetween(double position, uint32_t fieldsA, uint32_t fieldsB)
    {
        if (fieldsB <= fieldsA)
            return 1.f;
        return static_cast<float>(std::clamp((position - static_cast<double>(fieldsA)) / static_cast<double>(fieldsB - fieldsA), 0.0, 1.0));
    }

    // Relogio de conteudo (em fields do jogo): o que cada apresentacao mostra. Com vsync, cada apresentacao e um vblank, entao ele anda
    // um passo fixo por apresentacao (60 / Hz da tela, vezes as apresentacoes por redesenho) e o movimento sai uniforme por construcao;
    // um ajuste lento (5% por apresentacao) o mantem `lag` fields atras do quadro mais novo que ja chegou, e a chegada irregular dos
    // quadros vira uma correcao pequena. Passou do mais novo: segura (nunca extrapola). Erro de mais de 3 fields (carga, pausa): pula.
    class ContentClock
    {
    public:
        double advance(double step, double newest, double lag)
        {
            const double target = newest - lag;
            if (!init_ || std::fabs(target - p_) > 3.0)
                p_ = target, init_ = true;
            else
            {
                p_ += step;
                p_ += 0.05 * (target - p_);
            }
            p_ = std::min(p_, newest);
            return p_;
        }
        void reset() { init_ = false; }

    private:
        bool init_ = false;
        double p_ = 0.0;
    };

    // Folga do relogio de exibicao ajustada pelo que acontece: a cada 300 redesenhos, se mais de 3% seguraram o quadro mais novo
    // (chegou depois do relogio), +2 ms; se menos de 0,5%, -0,5 ms; entre lo e hi. O que atrasa a chegada depende da maquina e da
    // resolucao (a captura so entra quando a janela termina o redesenho em curso).
    class DelayTuner
    {
    public:
        DelayTuner(double start, double lo, double hi) : ms_(start), lo_(lo), hi_(hi) {}
        void onRedraw(bool held)
        {
            held_ += held ? 1 : 0;
            if (++n_ < 300)
                return;
            if (held_ * 100 > n_ * 3)
                ms_ = std::min(hi_, ms_ + 2.0);
            else if (held_ * 1000 < n_ * 5)
                ms_ = std::max(lo_, ms_ - 0.5);
            n_ = held_ = 0;
        }
        double ms() const { return ms_; }

    private:
        double ms_, lo_, hi_;
        int n_ = 0, held_ = 0;
    };
}
