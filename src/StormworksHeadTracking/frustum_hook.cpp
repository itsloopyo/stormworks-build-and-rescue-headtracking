#include "frustum_hook.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <intrin.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>

#include "builds/build_registry.h"
#include "cameraunlock/hooks/hook_manager.h"
#include "cameraunlock/logging/file_log.h"
#include "view_rewrite.h"

// The renderer decides what to draw by testing the world against six planes it
// builds from its own camera, before any of it reaches the GPU. Head tracking
// turns the view in the shader uniforms alone, so without this hook the game
// keeps culling to the un-turned view and whatever the head turned towards was
// already dropped: geometry disappears at the edge the head moves into.
//
// So the planes are built from the camera the player is actually looking
// through. The detour hands the game's own builder a copy of the camera with
// the head's rotation and lean applied, and leaves the real camera untouched -
// it is what aim, interaction and physics read.

namespace stormworks_ht {

namespace {
using BuildFrustumFn = void(__fastcall*)(void* planes, const void* camera, float aspect);

constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
// The planes are built from the previous frame's pose (the renderer culls
// before the frame's first camera uniform reaches us), and a fast head turn
// moves several degrees in that time. Widening the tangents covers the gap plus
// the parallax a lean adds at close range. Costs a few objects' worth of draws
// at the edge; too little and they pop at the edge of a quick turn.
constexpr double kTangentMargin = 1.15;
constexpr double kAspectTolerance = 1e-3;
constexpr double kFovTolerance = 0.02;

BuildFrustumFn g_original = nullptr;
std::uintptr_t g_exeBase = 0;

// Written on the GL thread once per frame, read on whichever thread culls.
std::mutex g_poseMutex;
bool g_havePose = false;         // a tracked pose is being applied this frame
double g_delta[16];              // eye-space H: a tracked view is H * clean view
double g_mainAspect = 0.0;       // the player's viewport, as the GL side measured it
double g_mainVerticalFov = 0.0;  // radians, the FOV the main camera is rendering

struct CallerStats {
    std::uintptr_t rva = 0;
    unsigned long long calls = 0;
    unsigned long long adjusted = 0;
    unsigned long long last_log_ms = 0;
};
std::mutex g_statsMutex;
CallerStats g_callers[16];

float* FloatsAt(void* camera, std::size_t offset) {
    return reinterpret_cast<float*>(static_cast<char*>(camera) + offset);
}

const float* FloatsAt(const void* camera, std::size_t offset) {
    return reinterpret_cast<const float*>(static_cast<const char*>(camera) + offset);
}

double* DoublesAt(void* camera, std::size_t offset) {
    return reinterpret_cast<double*>(static_cast<char*>(camera) + offset);
}

const double* DoublesAt(const void* camera, std::size_t offset) {
    return reinterpret_cast<const double*>(static_cast<const char*>(camera) + offset);
}

// The camera the renderer culls with, rather than a shadow cascade, a spot
// light or a reflection: the player's viewport aspect and the FOV the main
// camera is rendering this frame, both measured from the uniforms the game
// uploads.
bool IsMainCamera(const void* camera, float aspect, const builds::OffsetTable& offsets) {
    if (g_mainAspect <= 0.0 || g_mainVerticalFov <= 0.0) return false;
    if (std::fabs(aspect - g_mainAspect) > kAspectTolerance * g_mainAspect) return false;
    const double fov = *FloatsAt(camera, offsets.camera.fov_deg);
    return std::fabs(fov - g_mainVerticalFov) <= kFovTolerance * g_mainVerticalFov;
}

// Rewrites camera_copy in place so it stands where the head put it, looking
// where the head looks, with the tangents widened by the margin above.
void ApplyHeadPose(void* camera_copy, const builds::OffsetTable& offsets) {
    const auto& o = offsets.camera;
    float* right = FloatsAt(camera_copy, o.right);
    float* up = FloatsAt(camera_copy, o.up);
    float* forward = FloatsAt(camera_copy, o.forward);
    double* position = DoublesAt(camera_copy, o.position);

    CameraBasis clean;
    CameraBasis tracked;
    for (int i = 0; i < 3; ++i) {
        clean.right[i] = right[i];
        clean.up[i] = up[i];
        clean.forward[i] = forward[i];
        clean.position[i] = position[i];
    }
    TrackedCameraBasis(g_delta, clean, tracked);
    for (int i = 0; i < 3; ++i) {
        right[i] = static_cast<float>(tracked.right[i]);
        up[i] = static_cast<float>(tracked.up[i]);
        forward[i] = static_cast<float>(tracked.forward[i]);
        position[i] = tracked.position[i];
    }

    // The field is the full vertical angle in radians; the builder halves it.
    float* fov = FloatsAt(camera_copy, o.fov_deg);
    *fov = static_cast<float>(2.0 * std::atan(std::tan(*fov * 0.5) * kTangentMargin));
}

void LogCall(std::uintptr_t rva, const void* camera, float aspect, bool main, bool adjusted,
             const builds::OffsetTable& offsets) {
    const auto& o = offsets.camera;
    const unsigned long long now = GetTickCount64();
    std::lock_guard<std::mutex> lock(g_statsMutex);
    CallerStats* slot = nullptr;
    for (CallerStats& c : g_callers) {
        if (c.rva == rva || c.rva == 0) {
            slot = &c;
            break;
        }
    }
    if (!slot) return;
    slot->rva = rva;
    ++slot->calls;
    if (adjusted) ++slot->adjusted;
    if (now - slot->last_log_ms < 1000) return;
    slot->last_log_ms = now;
    const float* fwd = FloatsAt(camera, o.forward);
    const double* pos = DoublesAt(camera, o.position);
    cameraunlock::logging::Line(
        "frustum: caller +0x%llx tid %lu calls %llu adjusted %llu %s fov %.4f rad aspect %.4f near %.3f "
        "far %.0f pos (%.2f %.2f %.2f) fwd (%.3f %.3f %.3f)",
        static_cast<unsigned long long>(rva), GetCurrentThreadId(), slot->calls, slot->adjusted,
        main ? "MAIN" : "other", *FloatsAt(camera, o.fov_deg), aspect, *FloatsAt(camera, o.near_clip),
        *FloatsAt(camera, o.far_clip), pos[0], pos[1], pos[2], fwd[0], fwd[1], fwd[2]);
}

void __fastcall Detour_BuildFrustum(void* planes, const void* camera, float aspect) {
    const std::uintptr_t rva = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - g_exeBase;
    const builds::OffsetTable& offsets = builds::ActiveProfile().offsets;

    alignas(16) char copy[0x400];
    bool adjusted = false;
    bool main = false;
    {
        std::lock_guard<std::mutex> lock(g_poseMutex);
        main = IsMainCamera(camera, aspect, offsets);
        if (main && g_havePose && offsets.camera.bytes <= sizeof(copy)) {
            std::memcpy(copy, camera, offsets.camera.bytes);
            ApplyHeadPose(copy, offsets);
            adjusted = true;
        }
    }
    LogCall(rva, camera, aspect, main, adjusted, offsets);
    g_original(planes, adjusted ? copy : camera, aspect);
}
}  // namespace

void InstallFrustumHook() {
    using cameraunlock::hooks::HookManager;
    using cameraunlock::hooks::HookStatus;
    using cameraunlock::hooks::HookStatusToString;

    HMODULE exe = GetModuleHandleW(nullptr);
    if (builds::SelectProfile(exe) != builds::MatchResult::Matched) return;
    g_exeBase = reinterpret_cast<std::uintptr_t>(exe);

    HookManager& hooks = HookManager::Instance();
    HookStatus status = hooks.Initialize();
    if (status != HookStatus::Ok) {
        cameraunlock::logging::Line("frustum: MinHook init failed (%s); the view still turns but the "
                                    "game keeps culling to the un-turned view",
                                    HookStatusToString(status));
        return;
    }
    void* target = reinterpret_cast<void*>(g_exeBase + builds::ActiveProfile().offsets.build_frustum_rva);
    status = hooks.CreateHook(target, reinterpret_cast<void*>(&Detour_BuildFrustum),
                              reinterpret_cast<void**>(&g_original));
    if (status == HookStatus::Ok) status = hooks.EnableHook(target);
    if (status != HookStatus::Ok) {
        cameraunlock::logging::Line("frustum: hooking the frustum builder at %p failed (%s); the view "
                                    "still turns but the game keeps culling to the un-turned view",
                                    target, HookStatusToString(status));
        return;
    }
    cameraunlock::logging::Line("frustum: culling follows the head (frustum builder hooked at %p)", target);
}

void SetTrackedCamera(const double* delta, double main_aspect, double main_vertical_fov_deg) {
    std::lock_guard<std::mutex> lock(g_poseMutex);
    g_havePose = delta != nullptr;
    if (delta) std::memcpy(g_delta, delta, sizeof(g_delta));
    g_mainAspect = main_aspect;
    g_mainVerticalFov = main_vertical_fov_deg * kDegToRad;
}

}  // namespace stormworks_ht
