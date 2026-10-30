// macOS/Linux pieces of the native layer that need system headers kept away from raylib's translation unit (wotm_live.inc lives in
// ps2_runtime.cpp's unity batch): the crash stack trace and, on macOS, hiding the menu bar and Dock in borderless fullscreen.
// Compiled only outside Windows (the CMake of the runtime patch adds it with if(NOT WIN32)).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

// Path of the running executable, with Win32 GetModuleFileNameA's contract (module must be null: this executable): writes a
// NUL-terminated path and returns its length, or 0 on failure. netBuildId (wotm_sync.inc) uses it to find wotm.buildid next to the exe.
unsigned long GetModuleFileNameA(void *module, char *path, unsigned long size)
{
    if (module || !path || size == 0)
        return 0;
#ifdef __APPLE__
    uint32_t len = static_cast<uint32_t>(size);
    if (_NSGetExecutablePath(path, &len) != 0)
        return 0;
    return static_cast<unsigned long>(std::strlen(path));
#else
    const ssize_t n = readlink("/proc/self/exe", path, size - 1);
    if (n <= 0)
        return 0;
    path[n] = '\0';
    return static_cast<unsigned long>(n);
#endif
}

// Crash stack trace, called once by crashSignal (wotm_live.inc) from the signal handler: the first frames are the handler's own.
// Each frame is module+offset with the nearest exported symbol; for exact names: atos -o wotm -l <load address> <address> (macOS)
// or addr2line -f -C -e wotm <offset> (Linux).
void wotmCrashTrace(void *signalContext)
{
    (void)signalContext;
    void *frames[48];
    const int n = backtrace(frames, 48);
    Dl_info self{};
    if (dladdr(reinterpret_cast<void *>(&wotmCrashTrace), &self) && self.dli_fbase)
        std::fprintf(stderr, "[CRASH] %s loaded at %p\n", self.dli_fname ? self.dli_fname : "?", self.dli_fbase);
    for (int i = 0; i < n; ++i)
    {
        Dl_info info{};
        if (!dladdr(frames[i], &info) || !info.dli_fbase)
        {
            std::fprintf(stderr, "[CRASH]   #%-2d %p\n", i, frames[i]);
            continue;
        }
        const char *path = info.dli_fname ? info.dli_fname : "?";
        const char *modName = std::strrchr(path, '/') ? std::strrchr(path, '/') + 1 : path;
        const unsigned long long rel = static_cast<unsigned long long>(static_cast<char *>(frames[i]) - static_cast<char *>(info.dli_fbase));
        if (info.dli_sname && info.dli_saddr)
        {
            int st = -1;
            char *name = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &st);
            std::fprintf(stderr, "[CRASH]   #%-2d %s+0x%llx  %s+0x%llx\n", i, modName, rel, st == 0 && name ? name : info.dli_sname,
                         static_cast<unsigned long long>(static_cast<char *>(frames[i]) - static_cast<char *>(info.dli_saddr)));
            std::free(name);
        }
        else
            std::fprintf(stderr, "[CRASH]   #%-2d %s+0x%llx\n", i, modName, rel);
    }
    std::fflush(stderr);
}

#ifdef __APPLE__
// Borderless fullscreen on macOS (setFullscreen in wotm_live.inc): the window covers the screen, but the menu bar and the Dock stay
// on top of it unless the app hides them, and raylib/GLFW only do that for exclusive fullscreen. macOS applies these options only
// while the game is the active app (switching to another app brings the Dock back). Called through the Objective-C runtime, so
// this file stays C++.
void wotmMacHideMenuBarAndDock(bool hide)
{
    using SendId = id (*)(id, SEL);
    using SendOptions = void (*)(id, SEL, unsigned long);
    constexpr unsigned long kHideDock = 1ul << 1, kHideMenuBar = 1ul << 3;   // NSApplicationPresentationHideDock / HideMenuBar
    id app = reinterpret_cast<SendId>(objc_msgSend)(reinterpret_cast<id>(objc_getClass("NSApplication")), sel_registerName("sharedApplication"));
    if (app)
        reinterpret_cast<SendOptions>(objc_msgSend)(app, sel_registerName("setPresentationOptions:"), hide ? (kHideDock | kHideMenuBar) : 0ul);
}
#endif
