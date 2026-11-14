// ABOUTME: Browser build only: the glad_gl* function pointers the native layer calls (wotm_msaa.hpp, wotm_fx.hpp), bound to WebGL2.
// ABOUTME: raylib on the web links GLES3 directly and has no glad loader, so these point at the GLES3 entry points Emscripten provides.
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <cstddef>

namespace
{
    // desktop GL calls WebGL2 lacks, expressed with what it has
    void drawBuffer(GLenum buf) { glDrawBuffers(1, &buf); }
    void getQueryObjectui64v(GLuint id, GLenum pname, unsigned long long *out)
    {
        GLuint v = 0;
        glGetQueryObjectuiv(id, pname, &v);
        *out = v;
    }
    void *fenceSync(GLenum cond, GLbitfield flags) { return glFenceSync(cond, flags); }
    GLenum clientWaitSync(void *s, GLbitfield flags, unsigned long long t) { return glClientWaitSync(static_cast<GLsync>(s), flags, t); }
    void waitSync(void *s, GLbitfield flags, unsigned long long t) { glWaitSync(static_cast<GLsync>(s), flags, t); }
    void deleteSync(void *s) { glDeleteSync(static_cast<GLsync>(s)); }
}

extern "C"
{
    unsigned (*glad_glCheckFramebufferStatus)(unsigned) = glCheckFramebufferStatus;
    unsigned (*glad_glClientWaitSync)(void *, unsigned, unsigned long long) = clientWaitSync;
    void (*glad_glActiveTexture)(unsigned) = glActiveTexture;
    void (*glad_glBeginQuery)(unsigned, unsigned) = glBeginQuery;
    void (*glad_glBindBuffer)(unsigned, unsigned) = glBindBuffer;
    void (*glad_glBindFramebuffer)(unsigned, unsigned) = glBindFramebuffer;
    void (*glad_glBindRenderbuffer)(unsigned, unsigned) = glBindRenderbuffer;
    void (*glad_glBindTexture)(unsigned, unsigned) = glBindTexture;
    void (*glad_glBindVertexArray)(unsigned) = glBindVertexArray;
    void (*glad_glBlitFramebuffer)(int, int, int, int, int, int, int, int, unsigned, unsigned) = glBlitFramebuffer;
    void (*glad_glBufferData)(unsigned, std::ptrdiff_t, const void *, unsigned) = glBufferData;
    void (*glad_glBufferSubData)(unsigned, std::ptrdiff_t, std::ptrdiff_t, const void *) = glBufferSubData;
    void (*glad_glDeleteFramebuffers)(int, const unsigned *) = glDeleteFramebuffers;
    void (*glad_glDeleteRenderbuffers)(int, const unsigned *) = glDeleteRenderbuffers;
    void (*glad_glDeleteSync)(void *) = deleteSync;
    void (*glad_glDrawArrays)(unsigned, int, int) = glDrawArrays;
    void (*glad_glDrawBuffer)(unsigned) = drawBuffer;
    void (*glad_glEnableVertexAttribArray)(unsigned) = glEnableVertexAttribArray;
    void (*glad_glEndQuery)(unsigned) = glEndQuery;
    void (*glad_glFlush)(void) = glFlush;
    void (*glad_glFramebufferRenderbuffer)(unsigned, unsigned, unsigned, unsigned) = glFramebufferRenderbuffer;
    void (*glad_glGenBuffers)(int, unsigned *) = glGenBuffers;
    void (*glad_glGenFramebuffers)(int, unsigned *) = glGenFramebuffers;
    void (*glad_glGenQueries)(int, unsigned *) = glGenQueries;
    void (*glad_glGenRenderbuffers)(int, unsigned *) = glGenRenderbuffers;
    void (*glad_glGenVertexArrays)(int, unsigned *) = glGenVertexArrays;
    void (*glad_glGetIntegerv)(unsigned, int *) = glGetIntegerv;
    void (*glad_glGetQueryObjectui64v)(unsigned, unsigned, unsigned long long *) = getQueryObjectui64v;
    void (*glad_glReadBuffer)(unsigned) = glReadBuffer;
    void (*glad_glRenderbufferStorage)(unsigned, unsigned, int, int) = glRenderbufferStorage;
    void (*glad_glRenderbufferStorageMultisample)(unsigned, int, unsigned, int, int) = glRenderbufferStorageMultisample;
    void (*glad_glUniform1i)(int, int) = glUniform1i;
    void (*glad_glUniform4f)(int, float, float, float, float) = glUniform4f;
    void (*glad_glUniformMatrix4fv)(int, int, unsigned char, const float *) = glUniformMatrix4fv;
    void (*glad_glUseProgram)(unsigned) = glUseProgram;
    void (*glad_glVertexAttribPointer)(unsigned, int, unsigned, unsigned char, int, const void *) = glVertexAttribPointer;
    void (*glad_glWaitSync)(void *, unsigned, unsigned long long) = waitSync;
    void *(*glad_glFenceSync)(unsigned, unsigned) = fenceSync;
    // WebGL2 cannot map buffers: null tells wotm_fx.hpp's async readback to fall back to the direct read
    void *(*glad_glMapBuffer)(unsigned, unsigned) = nullptr;
    unsigned char (*glad_glUnmapBuffer)(unsigned) = nullptr;
}
#endif
