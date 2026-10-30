// Resources of native/wotm.rc (the button atlas and the Oswald font) on builds without Windows resources. On Windows the loaders
// read them from the executable with FindResourceA; elsewhere tools/build_runtime.sh turns the same files into byte arrays
// (tools/embed_resources.py -> <build>/wotm_gen/wotm_resources.gen.h). Without the generated header the lookup finds nothing and
// each loader takes its existing fallback (the game's icons, raylib's font).
#pragma once

#if __has_include("wotm_resources.gen.h")
#include "wotm_resources.gen.h"
#endif
#include <cstring>

// Bytes of the resource called `name` (the name in wotm.rc), or nullptr with size 0
inline const unsigned char *wotmEmbeddedResource(const char *name, int &size)
{
#ifdef WOTM_HAVE_EMBEDDED_RESOURCES
    for (const wotm_embedded::Resource &r : wotm_embedded::kResources)
        if (std::strcmp(r.name, name) == 0)
        {
            size = r.size;
            return r.data;
        }
#endif
    size = 0;
    return nullptr;
}
