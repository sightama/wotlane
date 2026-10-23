// wotm_msaa.hpp: MSAA do quadro nativo (opcao ANTI-ALIASING da aba VIDEO, WOTM_MSAA). O raylib so faz render texture de uma amostra;
// aqui um framebuffer com cor e profundidade multiamostra recebe o desenho (BeginTextureMode(target.render())) e, no fim, e resolvido
// (glBlitFramebuffer) para a textura do quadro de sempre, que a apresentacao, as fotos e o texId 0 (quadro anterior) continuam usando.
// O teste de oclusao do lens flare le a profundidade com glReadPixels, que nao le de um framebuffer multiamostra: readDepth() resolve
// a profundidade num framebuffer de uma amostra e o deixa como o de leitura (o desenho segue no multiamostra).
// As funcoes do GL 3 vem dos ponteiros do glad que o raylib carrega (raylib estatico, simbolos C).
#pragma once

#include <algorithm>
#include <cstdio>

#include "raylib.h"
#include "rlgl.h"

extern "C" {
extern void (*glad_glGenFramebuffers)(int, unsigned *);
extern void (*glad_glDeleteFramebuffers)(int, const unsigned *);
extern void (*glad_glBindFramebuffer)(unsigned, unsigned);
extern unsigned (*glad_glCheckFramebufferStatus)(unsigned);
extern void (*glad_glGenRenderbuffers)(int, unsigned *);
extern void (*glad_glDeleteRenderbuffers)(int, const unsigned *);
extern void (*glad_glBindRenderbuffer)(unsigned, unsigned);
extern void (*glad_glRenderbufferStorage)(unsigned, unsigned, int, int);
extern void (*glad_glRenderbufferStorageMultisample)(unsigned, int, unsigned, int, int);
extern void (*glad_glFramebufferRenderbuffer)(unsigned, unsigned, unsigned, unsigned);
extern void (*glad_glBlitFramebuffer)(int, int, int, int, int, int, int, int, unsigned, unsigned);
extern void (*glad_glGetIntegerv)(unsigned, int *);
extern void (*glad_glDrawBuffer)(unsigned);
extern void (*glad_glReadBuffer)(unsigned);
}

namespace wotm::msaa
{
constexpr unsigned kFramebuffer = 0x8D40, kReadFb = 0x8CA8, kDrawFb = 0x8CA9, kRenderbuffer = 0x8D41, kColor0 = 0x8CE0, kDepthAtt = 0x8D00,
                   kRgba8 = 0x8058, kDepth24 = 0x81A6, kColorBit = 0x4000, kDepthBit = 0x100, kNearest = 0x2600, kComplete = 0x8CD5,
                   kMaxSamples = 0x8D57;

// Maior numero de amostras do driver (0 = sem MSAA)
inline int maxSamples()
{
    static int m = -1;
    if (m < 0)
    {
        m = 0;
        if (glad_glGetIntegerv) glad_glGetIntegerv(kMaxSamples, &m);
    }
    return m;
}

struct Target
{
    unsigned fbo = 0, color = 0, depth = 0;   // multiamostra
    unsigned dfbo = 0, ddepth = 0;            // profundidade resolvida (teste do lens flare)
    int w = 0, h = 0, samples = 0;

    bool on() const { return fbo != 0; }
    void release()
    {
        if (fbo) glad_glDeleteFramebuffers(1, &fbo);
        if (dfbo) glad_glDeleteFramebuffers(1, &dfbo);
        if (color) glad_glDeleteRenderbuffers(1, &color);
        if (depth) glad_glDeleteRenderbuffers(1, &depth);
        if (ddepth) glad_glDeleteRenderbuffers(1, &ddepth);
        *this = Target{};
    }
    // Pronto para w x h com `samples` amostras (limitado ao do driver); samples <= 1 libera e devolve false
    bool ensure(int nw, int nh, int ns, const char *what = "quadro")
    {
        ns = std::min(ns, maxSamples());
        if (ns <= 1 || nw <= 0 || nh <= 0 || !glad_glRenderbufferStorageMultisample || !glad_glBlitFramebuffer)
        {
            if (fbo) release();
            return false;
        }
        if (fbo && w == nw && h == nh && samples == ns) return true;
        release();
        rlDrawRenderBatchActive();
        glad_glGenFramebuffers(1, &fbo);
        glad_glGenRenderbuffers(1, &color);
        glad_glGenRenderbuffers(1, &depth);
        glad_glBindRenderbuffer(kRenderbuffer, color);
        glad_glRenderbufferStorageMultisample(kRenderbuffer, ns, kRgba8, nw, nh);
        glad_glBindRenderbuffer(kRenderbuffer, depth);
        glad_glRenderbufferStorageMultisample(kRenderbuffer, ns, kDepth24, nw, nh);
        glad_glBindFramebuffer(kFramebuffer, fbo);
        glad_glFramebufferRenderbuffer(kFramebuffer, kColor0, kRenderbuffer, color);
        glad_glFramebufferRenderbuffer(kFramebuffer, kDepthAtt, kRenderbuffer, depth);
        const bool ok = glad_glCheckFramebufferStatus(kFramebuffer) == kComplete;
        // profundidade de uma amostra para o glReadPixels (so profundidade: sem buffer de cor para desenhar ou ler)
        glad_glGenFramebuffers(1, &dfbo);
        glad_glGenRenderbuffers(1, &ddepth);
        glad_glBindRenderbuffer(kRenderbuffer, ddepth);
        glad_glRenderbufferStorage(kRenderbuffer, kDepth24, nw, nh);
        glad_glBindFramebuffer(kFramebuffer, dfbo);
        glad_glFramebufferRenderbuffer(kFramebuffer, kDepthAtt, kRenderbuffer, ddepth);
        glad_glDrawBuffer(0);
        glad_glReadBuffer(0);
        const bool dok = glad_glCheckFramebufferStatus(kFramebuffer) == kComplete;
        glad_glBindRenderbuffer(kRenderbuffer, 0);
        glad_glBindFramebuffer(kFramebuffer, 0);
        w = nw;
        h = nh;
        samples = ns;
        std::fprintf(stderr, "[msaa] %s %dx%d com %d amostras%s\n", what, nw, nh, ns, ok && dok ? "" : ": framebuffer incompleto, sem MSAA");
        if (!ok || !dok)
        {
            release();
            return false;
        }
        return true;
    }
    // Para o BeginTextureMode do raylib: so usa o id e o tamanho
    RenderTexture2D render() const
    {
        RenderTexture2D r{};
        r.id = fbo;
        r.texture.width = w;
        r.texture.height = h;
        return r;
    }
    // Depois do EndTextureMode: a cor resolvida no framebuffer da textura do quadro (mesmo tamanho)
    void resolve(unsigned dstFbo) const
    {
        if (!fbo) return;
        glad_glBindFramebuffer(kReadFb, fbo);
        glad_glBindFramebuffer(kDrawFb, dstFbo);
        glad_glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, kColorBit, kNearest);
        glad_glBindFramebuffer(kFramebuffer, 0);
    }
    // No meio do desenho (framebuffer multiamostra em uso): resolve a profundidade e a deixa como leitura
    void readDepth() const
    {
        if (!fbo) return;
        rlDrawRenderBatchActive();
        glad_glBindFramebuffer(kReadFb, fbo);
        glad_glBindFramebuffer(kDrawFb, dfbo);
        glad_glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, kDepthBit, kNearest);
        glad_glBindFramebuffer(kDrawFb, fbo);
        glad_glBindFramebuffer(kReadFb, dfbo);
    }
};

// Alvo multiamostra em uso (entre o BeginTextureMode e o EndTextureMode dele); o lens flare chama readDepth nele
inline const Target *g_current = nullptr;
inline void readDepthIfMsaa()
{
    if (g_current) g_current->readDepth();
}
} // namespace wotm::msaa
