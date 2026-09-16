#include "gl_proxy.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "camera_uniforms.h"
#include "cameraunlock/camera/zoom_compensation.h"
#include "cameraunlock/logging/file_log.h"
#include "frustum_hook.h"
#include "gl_forwards.h"  // /EXPORT forwarder pragmas to opengl32_real.dll
#include "hud_reticle.h"
#include "mat4.h"
#include "mod.h"
#include "view_rewrite.h"

// Minimal GL typedefs - deliberately NOT including <GL/gl.h>, whose dllimport
// prototypes would clash with our own exported definitions below.
typedef unsigned int GLuint;
typedef float GLfloat;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef char GLchar;

// Stormworks draws everything through GLSL programs and never touches the
// fixed-function matrix stack. The camera reaches the GPU as named uniforms
// (rom/graphics/shaders): mat_view_proj and its _prev / _next / _inverse
// variants, mat_view, camera_position and camera_direction. Head tracking is
// applied by rewriting those uploads, so only what is drawn changes and the
// game's own camera, aim and interaction raycasts never see the pose.
//
// The same uniform names also carry shadow cascades (orthographic), spot light
// shadows (1:1 aspect) and full-screen passes (identity). Only a perspective
// whose aspect matches mat_view_proj_next is the player's camera: nothing but
// the main camera draws motion vectors.
namespace {
using stormworks_ht::CameraUniform;
using stormworks_ht::PerspectiveSplit;
namespace mat4 = stormworks_ht::mat4;

using PFN_wglGetProcAddress = PROC(APIENTRY*)(LPCSTR);
using PFN_wglSwapBuffers = BOOL(APIENTRY*)(HDC);
using PFN_glUniformMatrix4fv = void(APIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*);
using PFN_glUniform3fv = void(APIENTRY*)(GLint, GLsizei, const GLfloat*);
using PFN_glUniform3f = void(APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat);
using PFN_glUseProgram = void(APIENTRY*)(GLuint);
using PFN_glGetUniformLocation = GLint(APIENTRY*)(GLuint, const GLchar*);

std::wstring g_realGlPath;
std::once_flag g_forwardingOnce;
bool g_forwardingResolved = false;

PFN_wglGetProcAddress g_realWglGetProcAddress = nullptr;
PFN_wglSwapBuffers g_realSwapBuffers = nullptr;
PFN_glUniformMatrix4fv g_realUniformMatrix4fv = nullptr;
PFN_glUniform3fv g_realUniform3fv = nullptr;
PFN_glUniform3f g_realUniform3f = nullptr;
PFN_glUseProgram g_realUseProgram = nullptr;
PFN_glGetUniformLocation g_realGetUniformLocation = nullptr;

std::atomic<stormworks_ht::HeadTrackingMod*> g_mod{nullptr};

// Relative: a shadow or reflection camera with a different viewport is not the player's.
constexpr double kMainAspectTolerance = 1e-3;
// Relative: how far mat_view_proj_next's aspect must move to count as a new main camera.
constexpr double kMainAspectChange = 1e-4;
constexpr double kEyeMatchTolerance = 1e-2;
constexpr double kDirectionMatchTolerance = 1e-3;
constexpr unsigned long long kNoCameraWarnMs = 10000;
constexpr unsigned long long kHeartbeatIntervalMs = 30000;

// Indexed [program][location]. GL program names and uniform locations are
// small dense integers, and this lookup sits on every uniform upload.
std::vector<std::vector<CameraUniform>> g_kinds;
GLuint g_program = 0;

CameraUniform CurrentKind(GLint loc) {
    if (loc < 0 || g_program >= g_kinds.size()) return CameraUniform::None;
    const std::vector<CameraUniform>& locs = g_kinds[g_program];
    return static_cast<size_t>(loc) < locs.size() ? locs[loc] : CameraUniform::None;
}

// Everything below is touched on the GL thread only.
double g_delta[16];
double g_deltaInverse[16];
double g_prevDelta[16];  // last frame's delta, for the previous-tick matrix
bool g_inject = false;      // this frame has a pose; its delta waits for the main view
bool g_havePrevDelta = false;
double g_mainAspect = 0.0;  // 0 until the first mat_view_proj_next upload
bool g_haveView = false;    // this frame's main camera has been decomposed
double g_mainP[16];         // this frame's main projection
double g_mainPUnjittered[16];
double g_cleanEye[3];
double g_trackedEye[3];
double g_cleanForward[3];
double g_trackedForward[3];

int g_frameRewrites = 0;
unsigned long long g_lastMatchedOrIdleMs = 0;  // last frame that rewrote, or had no pose to inject
unsigned long long g_lastHeartbeatMs = 0;
bool g_loggedFirstRewrite = false;
bool g_warnedNoCamera = false;
bool g_loggedReticle = false;
bool g_inMenuCamera = false;

// One bit per CameraUniform: the kinds the game has uploaded while a pose was
// live, and the kinds that were actually rewritten. A single rewrite counter
// cannot tell "everything is being adjusted" from "the 96 mat_view_proj shaders
// are and the 6 mat_view ones silently are not", which renders SSAO and the
// ocean from the untracked view with nothing in the log.
unsigned g_kindsUploaded = 0;
unsigned g_kindsRewritten = 0;
unsigned g_loggedUnmatchedKinds = 0;

unsigned KindBit(CameraUniform kind) { return 1u << static_cast<unsigned>(kind); }

const char* KindName(CameraUniform kind) {
    switch (kind) {
        case CameraUniform::ViewProj: return "mat_view_proj";
        case CameraUniform::ViewProjPrev: return "mat_view_proj_prev";
        case CameraUniform::ViewProjNext: return "mat_view_proj_next";
        case CameraUniform::ViewProjInverse: return "mat_view_proj_inverse";
        case CameraUniform::View: return "mat_view";
        case CameraUniform::CameraPosition: return "camera_position";
        case CameraUniform::CameraDirection: return "camera_direction";
        case CameraUniform::World: return "mat_world";
        case CameraUniform::None: return "none";
    }
    return "none";
}

stormworks_ht::UnzoomedFov g_unzoomedFov;
double g_zoomFactor = 1.0;     // this frame's, applied to the pose
double g_liveVerticalDeg = 0.0;
double g_loggedZoomFactor = 1.0;
unsigned long long g_lastZoomLogMs = 0;
bool g_loggedFirstFov = false;

// Per program: the scale of its last mat_view_proj when that was the HUD's
// orthographic projection, else 0.
struct HudProjection {
    double sx = 0.0;
    double sy = 0.0;
};
std::vector<HudProjection> g_hud;

bool IsMainCamera(const PerspectiveSplit& s) {
    return g_mainAspect > 0.0 && std::fabs(s.aspect - g_mainAspect) < kMainAspectTolerance * g_mainAspect;
}

// Tracked view-projection for a main-camera matrix m already split into split.
void TrackSplit(const PerspectiveSplit& split, const double* m, const double* delta, double* out) {
    double c[16];
    stormworks_ht::ViewProjCorrection(split.P, delta, c);
    mat4::Mul(out, c, m);
}

// Tracked view-projection for a main-camera matrix; false leaves the upload as is.
bool TrackViewProj(const double* m, const double* delta, double* out) {
    PerspectiveSplit split;
    if (!stormworks_ht::SplitViewProj(m, split) || !IsMainCamera(split)) return false;
    TrackSplit(split, m, delta, out);
    return true;
}

// Both tangents are vertical: P[1][1] is 1 / tan(vfov/2), and the game's FOV
// settings are vertical degrees (slider 70 renders P[1][1] = 1 / tan(35 deg)).
void ObserveFov(const PerspectiveSplit& split) {
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    const double tan_half = 1.0 / split.P[5];
    g_liveVerticalDeg = 2.0 * std::atan(tan_half) * kRadToDeg;
    const bool reference_changed = g_unzoomedFov.Observe(split.P[5]);

    // Stormworks only ever narrows below its setting, so a wider view means the
    // reference is stale: the setting moved or the camera mode changed, and the
    // new value has not held still for long enough yet. Scaling up there would
    // swell the head's reach for the length of the widening animation and then
    // snap back.
    g_zoomFactor = g_unzoomedFov.Known()
                       ? std::min(1.0, static_cast<double>(cameraunlock::camera::FovZoomFactor(
                                           static_cast<float>(tan_half),
                                           static_cast<float>(g_unzoomedFov.TanHalf()))))
                       : 1.0;

    const unsigned long long now = GetTickCount64();
    if (!g_loggedFirstFov) {
        g_loggedFirstFov = true;
        cameraunlock::logging::Line(
            "FOV: live vertical %.3f deg (P[1][1] %.5f), horizontal %.3f deg at aspect %.4f. No "
            "un-zoomed reference yet (waiting for a steady whole-degree FOV in 60-110), zoom factor 1.0000",
            g_liveVerticalDeg, split.P[5], 2.0 * std::atan(tan_half * split.aspect) * kRadToDeg,
            split.aspect);
    }
    if (reference_changed) {
        cameraunlock::logging::Line(
            "FOV: un-zoomed reference %.0f deg vertical (tan half %.5f); live vertical %.3f deg, "
            "horizontal %.3f deg at aspect %.4f; zoom factor %.4f",
            g_unzoomedFov.Degrees(), g_unzoomedFov.TanHalf(), g_liveVerticalDeg,
            2.0 * std::atan(tan_half * split.aspect) * kRadToDeg, split.aspect, g_zoomFactor);
        g_loggedZoomFactor = g_zoomFactor;
        g_lastZoomLogMs = now;
    } else if (std::fabs(g_zoomFactor - g_loggedZoomFactor) > 0.01 && now - g_lastZoomLogMs >= 250) {
        cameraunlock::logging::Line("FOV: zoom factor %.4f (live vertical %.3f deg, un-zoomed %.0f deg)",
                                    g_zoomFactor, g_liveVerticalDeg, g_unzoomedFov.Degrees());
        g_loggedZoomFactor = g_zoomFactor;
        g_lastZoomLogMs = now;
    }
}

void RecordMainView(const PerspectiveSplit& split) {
    const double* clean_view = split.V;
    if (!g_haveView) ObserveFov(split);

    // Menus and the loading screens behind them render at the menu FOV. The
    // pose is dropped for those frames rather than the view turning while the
    // player clicks buttons; a loading screen with no main camera at all never
    // gets here, so it is covered by g_haveView staying false.
    if (!g_haveView) {
        const bool menu = stormworks_ht::IsMenuFov(g_liveVerticalDeg);
        if (menu != g_inMenuCamera) {
            g_inMenuCamera = menu;
            cameraunlock::logging::Line(
                menu ? "Menu camera (vertical FOV %.3f deg); no pose is applied here"
                     : "Gameplay camera (vertical FOV %.3f deg)",
                g_liveVerticalDeg);
        }
        if (menu) g_inject = false;
    }

    if (!g_haveView && g_inject) {
        // Once per frame, from the first main camera, so every pass of the frame
        // shares one delta and the forward and inverse uploads stay exact inverses.
        g_mod.load()->BuildViewDelta(clean_view, g_zoomFactor, g_delta);
        mat4::Invert(g_deltaInverse, g_delta);
        // A frame with no previous delta has no head motion to report; reusing
        // this frame's keeps the velocity buffer from seeing a jump from identity.
        if (!g_havePrevDelta) mat4::Copy(g_prevDelta, g_delta);
    }
    mat4::Copy(g_mainP, split.P);
    mat4::Copy(g_mainPUnjittered, split.P);
    g_mainPUnjittered[8] = 0.0;
    g_mainPUnjittered[9] = 0.0;
    double tracked[16];
    mat4::Mul(tracked, g_delta, clean_view);
    stormworks_ht::EyeOf(clean_view, g_cleanEye);
    stormworks_ht::EyeOf(tracked, g_trackedEye);
    for (int i = 0; i < 3; ++i) {
        g_cleanForward[i] = -clean_view[i * 4 + 2];
        g_trackedForward[i] = -tracked[i * 4 + 2];
    }
    g_haveView = true;

    // The renderer builds its cull planes before this frame's first camera
    // uniform, so what it reads here is the previous frame's delta. The frustum
    // hook widens the tangents to cover a frame of head motion.
    stormworks_ht::SetTrackedCamera(g_inject ? g_delta : nullptr, g_mainAspect, g_liveVerticalDeg);
}

void CountRewrite(CameraUniform kind) {
    ++g_frameRewrites;
    g_kindsRewritten |= KindBit(kind);
    if (!g_loggedFirstRewrite) {
        g_loggedFirstRewrite = true;
        cameraunlock::logging::Line("Head tracking reached the camera (first camera uniform rewritten)");
    }
}

bool RewriteMatrix(CameraUniform kind, const double* m, double* out) {
    switch (kind) {
        case CameraUniform::ViewProj: {
            PerspectiveSplit split;
            if (!stormworks_ht::SplitViewProj(m, split) || !IsMainCamera(split)) return false;
            RecordMainView(split);
            if (!g_inject) return false;
            TrackSplit(split, m, g_delta, out);
            return true;
        }
        case CameraUniform::ViewProjPrev:
            return g_inject && g_haveView && TrackViewProj(m, g_prevDelta, out);
        case CameraUniform::ViewProjNext:
            return g_inject && g_haveView && TrackViewProj(m, g_delta, out);
        case CameraUniform::ViewProjInverse: {
            // Inverted only to recognise it. The correction's P comes from this
            // frame's forward mat_view_proj: a P recovered from inverting the
            // float inverse is off in its depth terms by enough to shimmer the
            // lighting whenever the head is yawed.
            PerspectiveSplit split;
            double vp[16], c_inv[16];
            if (!g_inject || !g_haveView || !mat4::Invert(vp, m) ||
                !stormworks_ht::SplitViewProj(vp, split) || !IsMainCamera(split))
                return false;
            stormworks_ht::ViewProjCorrection(g_mainP, g_deltaInverse, c_inv);
            mat4::Mul(out, m, c_inv);
            return true;
        }
        case CameraUniform::View: {
            if (!g_inject || !g_haveView || !stormworks_ht::IsRigid(m)) return false;
            double eye[3];
            stormworks_ht::EyeOf(m, eye);
            if (!stormworks_ht::Near3(eye, g_cleanEye, kEyeMatchTolerance)) return false;
            mat4::Mul(out, g_delta, m);
            return true;
        }
        default:
            return false;
    }
}

// The crosshair marks the clean camera's forward, so with the head turned it
// moves to where that direction lands in the tracked view, through the same P
// and H the camera rewrite used.
//
// Direction, not point: nothing at the GL layer says how far away the aimed-at
// surface is, so a lean leaves an error of lean / distance - a few degrees at
// arm's length, under one across a room. The lean term needs that distance;
// do not stand a fixed range in for it.
bool MoveReticle(const GLfloat* v, GLfloat* out) {
    if (!g_inject || !g_haveView || g_program >= g_hud.size() || g_hud[g_program].sx == 0.0)
        return false;
    if (!stormworks_ht::IsCentredReticleQuad(v)) return false;

    for (int i = 0; i < 16; ++i) out[i] = v[i];
    double ndc_x, ndc_y;
    if (stormworks_ht::ProjectCleanForward(g_mainPUnjittered, g_delta, ndc_x, ndc_y)) {
        out[12] += static_cast<GLfloat>(ndc_x / g_hud[g_program].sx);
        out[13] += static_cast<GLfloat>(ndc_y / g_hud[g_program].sy);
    } else {
        out[0] = out[5] = 0.0f;
    }
    if (!g_loggedReticle) {
        g_loggedReticle = true;
        cameraunlock::logging::Line("Crosshair found (HUD quad size %.4f); it now follows the aim", v[0]);
    }
    return true;
}

void RecordHudProjection(const double* m) {
    if (g_program >= g_hud.size()) g_hud.resize(g_program + 1);
    stormworks_ht::HudProjectionScale(m, g_hud[g_program].sx, g_hud[g_program].sy);
}

void LearnMainAspect(const double* view_proj_next) {
    PerspectiveSplit split;
    if (stormworks_ht::SplitViewProj(view_proj_next, split) &&
        std::fabs(split.aspect - g_mainAspect) > kMainAspectChange * split.aspect) {
        cameraunlock::logging::Line("Main camera found: aspect %.4f", split.aspect);
        g_mainAspect = split.aspect;
    }
}

// Replaces this frame's clean camera position or forward with the tracked one.
bool RewriteVec3(CameraUniform kind, const GLfloat* v, GLfloat* out) {
    if (!g_inject) return false;
    g_kindsUploaded |= KindBit(kind);
    if (!g_haveView) return false;
    const double in[3] = {v[0], v[1], v[2]};
    const double* tracked = nullptr;
    if (kind == CameraUniform::CameraPosition && stormworks_ht::Near3(in, g_cleanEye, kEyeMatchTolerance)) {
        tracked = g_trackedEye;
    } else if (kind == CameraUniform::CameraDirection &&
               stormworks_ht::Near3(in, g_cleanForward, kDirectionMatchTolerance)) {
        tracked = g_trackedForward;
    } else {
        return false;
    }
    for (int i = 0; i < 3; ++i) out[i] = static_cast<GLfloat>(tracked[i]);
    CountRewrite(kind);
    return true;
}

void APIENTRY Hook_glUseProgram(GLuint program) {
    g_program = program;
    g_realUseProgram(program);
}

GLint APIENTRY Hook_glGetUniformLocation(GLuint program, const GLchar* name) {
    GLint loc = g_realGetUniformLocation(program, name);
    if (loc < 0 || !name) return loc;
    if (program >= g_kinds.size()) g_kinds.resize(program + 1);
    std::vector<CameraUniform>& locs = g_kinds[program];
    if (static_cast<size_t>(loc) >= locs.size()) locs.resize(loc + 1, CameraUniform::None);
    locs[loc] = stormworks_ht::ClassifyUniform(name);
    return loc;
}

void APIENTRY Hook_glUniformMatrix4fv(GLint loc, GLsizei count, GLboolean transpose,
                                      const GLfloat* v) {
    CameraUniform kind = CurrentKind(loc);
    if (kind == CameraUniform::None || count != 1 || transpose || !v) {
        g_realUniformMatrix4fv(loc, count, transpose, v);
        return;
    }

    if (kind == CameraUniform::World) {
        GLfloat moved[16];
        if (MoveReticle(v, moved)) {
            g_realUniformMatrix4fv(loc, 1, transpose, moved);
        } else {
            g_realUniformMatrix4fv(loc, count, transpose, v);
        }
        return;
    }

    double m[16], out[16];
    if (g_inject) g_kindsUploaded |= KindBit(kind);
    for (int i = 0; i < 16; ++i) m[i] = v[i];
    if (kind == CameraUniform::ViewProj) RecordHudProjection(m);
    if (kind == CameraUniform::ViewProjNext) LearnMainAspect(m);

    if (!RewriteMatrix(kind, m, out)) {
        g_realUniformMatrix4fv(loc, count, transpose, v);
        return;
    }
    CountRewrite(kind);
    GLfloat f[16];
    for (int i = 0; i < 16; ++i) f[i] = static_cast<GLfloat>(out[i]);
    g_realUniformMatrix4fv(loc, 1, transpose, f);
}

void APIENTRY Hook_glUniform3fv(GLint loc, GLsizei count, const GLfloat* v) {
    CameraUniform kind = CurrentKind(loc);
    GLfloat out[3];
    if (kind != CameraUniform::None && count == 1 && v && RewriteVec3(kind, v, out)) {
        g_realUniform3fv(loc, 1, out);
        return;
    }
    g_realUniform3fv(loc, count, v);
}

void APIENTRY Hook_glUniform3f(GLint loc, GLfloat x, GLfloat y, GLfloat z) {
    CameraUniform kind = CurrentKind(loc);
    const GLfloat v[3] = {x, y, z};
    GLfloat out[3];
    if (kind != CameraUniform::None && RewriteVec3(kind, v, out)) {
        g_realUniform3f(loc, out[0], out[1], out[2]);
        return;
    }
    g_realUniform3f(loc, x, y, z);
}

// Reports the last frame's rewrites. The failure this watches for is a game
// patch renaming or restructuring the camera uniforms: the pose arrives and
// nothing on screen moves.
void LogCameraHealth(unsigned long long now) {
    // Timed from the last idle frame as well as the last match, so a build whose
    // uniforms never match at all still warns.
    if (g_frameRewrites > 0 || !g_inject) g_lastMatchedOrIdleMs = now;

    if (g_inject && g_frameRewrites == 0 && now - g_lastMatchedOrIdleMs > kNoCameraWarnMs) {
        if (!g_warnedNoCamera) {
            g_warnedNoCamera = true;
            cameraunlock::logging::Line(
                "WARN: tracker pose is live but no camera uniform has matched for 10s. Expected "
                "on a loading screen; if it lasts in game, the game's shaders have changed.");
        }
    } else if (g_warnedNoCamera && g_frameRewrites > 0) {
        g_warnedNoCamera = false;
        cameraunlock::logging::Line("Camera uniforms matching again");
    }

    if (now - g_lastHeartbeatMs >= kHeartbeatIntervalMs) {
        g_lastHeartbeatMs = now;
        // Which kinds have never been rewritten, out of the ones the game has
        // uploaded with a pose live. A kind that stays here while others match
        // is a shader whose camera the mod is not reaching: the frame count
        // alone looks healthy while that pass renders from the clean view.
        const unsigned unmatched = g_kindsUploaded & ~g_kindsRewritten;
        if (g_kindsRewritten != 0 && unmatched != g_loggedUnmatchedKinds) {
            g_loggedUnmatchedKinds = unmatched;
            if (unmatched == 0) {
                cameraunlock::logging::Line("render: every camera uniform the game uploads is being adjusted");
            } else {
                std::string names;
                for (unsigned k = 1; k <= static_cast<unsigned>(CameraUniform::World); ++k) {
                    if (!(unmatched & (1u << k))) continue;
                    if (!names.empty()) names += ", ";
                    names += KindName(static_cast<CameraUniform>(k));
                }
                cameraunlock::logging::Line(
                    "WARN: uploaded but never adjusted this session: %s. Those passes render from "
                    "the game's own camera while the rest follow the head.",
                    names.c_str());
            }
        }
        cameraunlock::logging::Line(
            "render: %d camera uniforms rewritten last frame, main aspect %.4f, live vertical FOV %.3f deg, "
            "un-zoomed %.0f deg, zoom factor %.4f",
            g_frameRewrites, g_mainAspect, g_liveVerticalDeg, g_unzoomedFov.Degrees(), g_zoomFactor);
    }
}

// Runs between two presents, before the next frame issues any draw, so every
// pass of a frame is built from one pose.
void BeginFrame() {
    LogCameraHealth(GetTickCount64());
    g_frameRewrites = 0;

    g_havePrevDelta = g_inject && g_haveView;
    if (g_havePrevDelta) mat4::Copy(g_prevDelta, g_delta);
    g_haveView = false;

    stormworks_ht::HeadTrackingMod* mod = g_mod.load();
    g_inject = mod && mod->IsInjecting();
}
}  // namespace

namespace stormworks_ht {

void SetForwardingDir(const std::wstring& exe_dir) {
    g_realGlPath = exe_dir + L"\\opengl32_real.dll";
}

bool InitForwarding() {
    // Every other opengl32 export is forwarded to opengl32_real.dll by the
    // loader, and the hooked GL 2.0+ entry points are handed out through
    // wglGetProcAddress, so only the two exports defined here need their real
    // pointers. Loading by absolute path avoids any search-order ambiguity,
    // and the new base name sidesteps the "two opengl32.dll" loader collision.
    std::call_once(g_forwardingOnce, [] {
        HMODULE real = LoadLibraryW(g_realGlPath.c_str());
        if (!real) {
            cameraunlock::logging::Line("FATAL: could not load opengl32_real.dll (err %lu)",
                                        GetLastError());
            return;
        }

        g_realWglGetProcAddress =
            reinterpret_cast<PFN_wglGetProcAddress>(GetProcAddress(real, "wglGetProcAddress"));
        g_realSwapBuffers = reinterpret_cast<PFN_wglSwapBuffers>(GetProcAddress(real, "wglSwapBuffers"));
        if (!g_realWglGetProcAddress || !g_realSwapBuffers) {
            cameraunlock::logging::Line("FATAL: missing intercepted GL export in opengl32_real");
            return;
        }

        g_forwardingResolved = true;
        cameraunlock::logging::Line("Real GL resolved from opengl32_real.dll");
    });
    return g_forwardingResolved;
}

void SetMod(HeadTrackingMod* mod) {
    g_mod.store(mod);
}

}  // namespace stormworks_ht

// ---- Intercepted exports (exported by __declspec; the rest are forwarded
// via the /EXPORT pragmas in gl_forwards.h) ----

// Both exports resolve their real pointers themselves rather than trusting the
// init thread to have got there first: nothing orders that thread against the
// game's first GL call. Failure is already logged as FATAL, and each returns
// the call's own failure value rather than jumping through a null pointer.
extern "C" __declspec(dllexport) PROC APIENTRY wglGetProcAddress(LPCSTR name) {
    if (!stormworks_ht::InitForwarding()) return nullptr;
    if (!name) return nullptr;
    PROC real = g_realWglGetProcAddress(name);
    if (!real) return real;

    if (std::strcmp(name, "glUniformMatrix4fv") == 0) {
        g_realUniformMatrix4fv = reinterpret_cast<PFN_glUniformMatrix4fv>(real);
        return reinterpret_cast<PROC>(Hook_glUniformMatrix4fv);
    }
    if (std::strcmp(name, "glUniform3fv") == 0) {
        g_realUniform3fv = reinterpret_cast<PFN_glUniform3fv>(real);
        return reinterpret_cast<PROC>(Hook_glUniform3fv);
    }
    if (std::strcmp(name, "glUniform3f") == 0) {
        g_realUniform3f = reinterpret_cast<PFN_glUniform3f>(real);
        return reinterpret_cast<PROC>(Hook_glUniform3f);
    }
    if (std::strcmp(name, "glUseProgram") == 0) {
        g_realUseProgram = reinterpret_cast<PFN_glUseProgram>(real);
        return reinterpret_cast<PROC>(Hook_glUseProgram);
    }
    if (std::strcmp(name, "glGetUniformLocation") == 0) {
        g_realGetUniformLocation = reinterpret_cast<PFN_glGetUniformLocation>(real);
        return reinterpret_cast<PROC>(Hook_glGetUniformLocation);
    }
    return real;
}

extern "C" __declspec(dllexport) BOOL APIENTRY wglSwapBuffers(HDC hdc) {
    if (!stormworks_ht::InitForwarding()) return FALSE;
    static bool first = true;
    if (first) {
        first = false;
        cameraunlock::logging::Line("First present - GL render path reached (tid %lu)",
                                    GetCurrentThreadId());
    }
    BOOL presented = g_realSwapBuffers(hdc);
    stormworks_ht::HeadTrackingMod* mod = g_mod.load();
    if (mod) mod->OnFrameTick();
    BeginFrame();
    return presented;
}
