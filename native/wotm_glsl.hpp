// ABOUTME: Adapts the native layer's GLSL 330 shader sources to the GPU API in use: unchanged on desktop OpenGL, GLSL ES 3.00 in the
// ABOUTME: browser (WebGL2), which needs its own version line and default precisions. Pure string work, unit-tested.
#pragma once

#include <string>
#include <string_view>

#ifdef __EMSCRIPTEN__
#define WOTM_GLSL_ES 1
#else
#define WOTM_GLSL_ES 0
#endif

namespace wotm_glsl
{
    // `src` starts with "#version 330" (the native shaders); for GLSL ES that line becomes "#version 300 es" plus the precisions
    // desktop GLSL doesn't need. A source without that line is returned as is.
    inline std::string forTarget(std::string_view src, bool es)
    {
        constexpr std::string_view kDesktop = "#version 330\n";
        if (!es || src.substr(0, kDesktop.size()) != kDesktop)
            return std::string(src);
        return "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n" +
               std::string(src.substr(kDesktop.size()));
    }
}
