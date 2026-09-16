#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

#include "cameraunlock/logging/file_log.h"
#include "frustum_hook.h"
#include "gl_proxy.h"
#include "mod.h"

namespace {
std::wstring g_exeDir;
stormworks_ht::HeadTrackingMod* g_modInstance = nullptr;

std::wstring DirOf(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

// Everything that must NOT run under the loader lock: resolving the real
// opengl32 (LoadLibrary deadlocks in DllMain when the proxy is a very early
// static import), reading and writing the config, socket + hotkey threads. This
// thread runs the instant the loader lock releases - long before the game
// creates its GL context and issues the first forwarded call.
DWORD WINAPI InitThread(LPVOID) {
    if (!stormworks_ht::InitForwarding()) {
        cameraunlock::logging::Line("FATAL: forwarding init failed; GL will not work");
        return 1;
    }
    g_modInstance = new stormworks_ht::HeadTrackingMod();
    g_modInstance->Init(g_exeDir);
    stormworks_ht::SetMod(g_modInstance);
    stormworks_ht::InstallFrustumHook();
    return 0;
}
}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);

        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        g_exeDir = DirOf(exe);

        // Open() moves the previous launch to StormworksHeadTracking.prev.log and
        // truncates, so the log holds this session and one generation back. It
        // is the one piece of file IO done here rather than on the init thread:
        // it loads no DLL, and without it a failure in this function has nowhere
        // to be reported.
        cameraunlock::logging::Open(g_exeDir + L"\\StormworksHeadTracking.log");
        cameraunlock::logging::Line("StormworksHeadTracking attached");
        stormworks_ht::SetForwardingDir(g_exeDir);

        // Defer the rest of the work (including LoadLibrary of the real
        // opengl32) to a thread that runs after the loader lock releases. Doing
        // it here would deadlock the process during early import resolution.
        HANDLE t = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (t) {
            CloseHandle(t);
        } else {
            cameraunlock::logging::Line("FATAL: could not start the init thread (err %lu); GL will not work",
                                        GetLastError());
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        stormworks_ht::SetMod(nullptr);
        // A non-null lpReserved means the process is exiting, which Windows does
        // by terminating every other thread first. The receiver's supervisor
        // logs on a 500ms clock and the hotkey thread logs on every toggle, so
        // one of them can be holding the log mutex when it dies, and taking that
        // mutex here would hang the game on exit with no window and nothing in
        // the log. The OS closes the handle either way.
        if (!reserved) {
            cameraunlock::logging::Line("StormworksHeadTracking detached");
            cameraunlock::logging::Close();
        }
    }
    return TRUE;
}
