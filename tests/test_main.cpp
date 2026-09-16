// Characterization tests for the parts of the mod that run without a game.
//
// The hooks only mean anything inside Stormworks. What is left over is the
// matrix recognition and rewrite maths every camera uniform passes through,
// and the config file the player edits. A sign or tolerance error in the maths
// gives a picture that moves plausibly and wrongly, which gets through a play
// test, so it is pinned here.
//
// These lock CURRENT behaviour. If a change here fails, the question is whether
// the behaviour was meant to change, not whether the test is inconvenient.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "camera_uniforms.h"
#include "config.h"
#include "hud_reticle.h"
#include "mat4.h"
#include "view_rewrite.h"

namespace {

using namespace stormworks_ht;

int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const char* what, const char* file, int line) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("FAIL %s:%d  %s\n", file, line, what);
}

void CheckNear(double got, double want, double tol, const char* what, const char* file, int line) {
    ++g_checks;
    if (std::isfinite(got) && std::fabs(got - want) <= tol) return;
    ++g_failures;
    std::printf("FAIL %s:%d  %s: got %.9f, want %.9f (tol %g)\n", file, line, what, got, want, tol);
}

void CheckMat(const double* got, const double* want, double tol, const char* what, const char* file,
              int line) {
    for (int i = 0; i < 16; ++i) {
        char label[160];
        std::snprintf(label, sizeof(label), "%s [%d]", what, i);
        CheckNear(got[i], want[i], tol, label, file, line);
    }
}

#define CHECK(cond) Check((cond), #cond, __FILE__, __LINE__)
#define CHECK_NEAR(got, want, tol) CheckNear((got), (want), (tol), #got, __FILE__, __LINE__)
#define CHECK_MAT(got, want, tol) CheckMat((got), (want), (tol), #got, __FILE__, __LINE__)

constexpr double kPi = 3.14159265358979323846;
constexpr double kTol = 1e-12;
constexpr double kDeg = kPi / 180.0;

double Dot(const double* a, const double* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// OpenGL perspective in the shape the game uploads: infinite far plane, w row
// (0, 0, -1, 0), optional sub-pixel jitter in column 2.
void Perspective(double* p, double a, double b, double jx, double jy) {
    for (int i = 0; i < 16; ++i) p[i] = 0.0;
    p[0] = a;
    p[5] = b;
    p[8] = jx;
    p[9] = jy;
    p[10] = -1.0;
    p[11] = -1.0;
    p[14] = -0.05;
}

// A mirrored view like Stormworks' own (world Z flipped, determinant -1), turned
// and moved off the origin.
void MirroredView(double* v) {
    double mirror[16], rot[16], trans[16], tmp[16];
    mat4::Identity(mirror);
    mirror[10] = -1.0;
    mat4::RotAxis(rot, 0.0, 0.6, 0.8, 0.7);
    mat4::Translate(trans, -1200.0, -35.0, 860.0);
    mat4::Mul(tmp, mirror, rot);
    mat4::Mul(v, tmp, trans);
}

void TestMat4() {
    double id[16], rot[16], out[16];
    mat4::Identity(id);
    mat4::RotX(rot, 0.3);
    mat4::Mul(out, id, rot);
    CHECK_MAT(out, rot, kTol);

    // RotX(+90) takes +Y to +Z; RotY(+90) takes +Z to +X; RotZ(+90) takes +X to +Y.
    mat4::RotX(rot, kPi / 2);
    CHECK_NEAR(rot[6], 1.0, kTol);
    mat4::RotY(rot, kPi / 2);
    CHECK_NEAR(rot[8], 1.0, kTol);
    mat4::RotZ(rot, kPi / 2);
    CHECK_NEAR(rot[1], 1.0, kTol);

    double axis[16];
    mat4::RotY(rot, 0.42);
    mat4::RotAxis(axis, 0.0, 1.0, 0.0, 0.42);
    CHECK_MAT(axis, rot, kTol);

    double trans[16], m[16], inv[16], back[16];
    mat4::Translate(trans, 3.0, -4.0, 5.0);
    CHECK(trans[12] == 3.0 && trans[13] == -4.0 && trans[14] == 5.0 && trans[15] == 1.0);
    mat4::Mul(m, rot, trans);
    CHECK(mat4::Invert(inv, m));
    mat4::Mul(back, m, inv);
    CHECK_MAT(back, id, 1e-12);

    double singular[16] = {0};
    CHECK(!mat4::Invert(inv, singular));
}

void TestSplitViewProj() {
    double p[16], v[16], m[16];
    Perspective(p, 1.0 / (1.7777777 * 0.7), 1.0 / 0.7, 3e-4, -2e-4);
    MirroredView(v);
    mat4::Mul(m, p, v);

    PerspectiveSplit split;
    CHECK(SplitViewProj(m, split));
    CHECK_MAT(split.P, p, 1e-9);
    CHECK_MAT(split.V, v, 1e-9);
    CHECK_NEAR(split.aspect, 1.7777777, 1e-9);

    // An orthographic shadow cascade: w row is (0, 0, 0, 1).
    double ortho[16];
    mat4::Identity(ortho);
    ortho[0] = 0.01;
    ortho[5] = 0.01;
    ortho[10] = -0.002;
    mat4::Mul(m, ortho, v);
    CHECK(!SplitViewProj(m, split));

    // A full-screen pass's identity.
    double id[16];
    mat4::Identity(id);
    CHECK(!SplitViewProj(id, split));

    // A view with scale in it is not a camera.
    double scaled[16];
    for (int i = 0; i < 16; ++i) scaled[i] = v[i];
    for (int i = 0; i < 12; ++i) scaled[i] *= 1.01;
    mat4::Mul(m, p, scaled);
    CHECK(!SplitViewProj(m, split));

    // Depth row off the perspective shape.
    Perspective(p, 1.0, 1.0, 0.0, 0.0);
    p[2] = 0.5;
    mat4::Mul(m, p, v);
    CHECK(!SplitViewProj(m, split));

    // No depth offset: P is singular, and ViewProjCorrection would invert it.
    Perspective(p, 1.0, 1.0, 0.0, 0.0);
    p[14] = 0.0;
    mat4::Mul(m, p, v);
    CHECK(!SplitViewProj(m, split));
}

void TestIsRigidAndEye() {
    double v[16];
    MirroredView(v);
    CHECK(IsRigid(v));

    double eye[3];
    EyeOf(v, eye);
    CHECK_NEAR(eye[0], 1200.0, 1e-9);
    CHECK_NEAR(eye[1], 35.0, 1e-9);
    CHECK_NEAR(eye[2], -860.0, 1e-9);

    double bad[16];
    for (int i = 0; i < 16; ++i) bad[i] = v[i];
    bad[3] = 0.1;
    CHECK(!IsRigid(bad));
    for (int i = 0; i < 16; ++i) bad[i] = v[i];
    bad[15] = 1.001;
    CHECK(!IsRigid(bad));
    for (int i = 0; i < 16; ++i) bad[i] = v[i];
    bad[0] *= 1.1;
    CHECK(!IsRigid(bad));
}

void TestViewProjCorrection() {
    double p[16], v[16], m[16], id[16], c[16], out[16];
    Perspective(p, 0.8, 1.42, 3e-4, -2e-4);
    MirroredView(v);
    mat4::Mul(m, p, v);

    mat4::Identity(id);
    ViewProjCorrection(p, id, c);
    CHECK_MAT(c, id, 1e-12);

    // C * M == P * H * V.
    double h[16], hv[16], want[16];
    mat4::RotAxis(h, 0.0, 0.8, 0.6, 0.3);
    h[12] = 0.1;
    h[14] = -0.2;
    ViewProjCorrection(p, h, c);
    mat4::Mul(out, c, m);
    mat4::Mul(hv, h, v);
    mat4::Mul(want, p, hv);
    CHECK_MAT(out, want, 1e-9);
}

void TestNear3() {
    const double ref[3] = {1000.0, 0.0, -2.0};
    const double inside[3] = {1000.0 + 0.01 + 1e-5 * 1000.0 - 1e-9, 0.01, -2.0 - 0.01};
    const double outside[3] = {1000.0 + 0.01 + 1e-5 * 1000.0 + 1e-6, 0.0, -2.0};
    CHECK(Near3(inside, ref, 0.01));
    CHECK(!Near3(outside, ref, 0.01));
}

// The camera the cull frustum is built from. The clean basis is the game's own
// world convention: right cross up is forward, world +Y up.
void TestTrackedCameraBasis() {
    CameraBasis clean;
    clean.right[0] = 1.0;   clean.right[1] = 0.0;   clean.right[2] = 0.0;
    clean.up[0] = 0.0;      clean.up[1] = 1.0;      clean.up[2] = 0.0;
    clean.forward[0] = 0.0; clean.forward[1] = 0.0; clean.forward[2] = 1.0;
    clean.position[0] = 100.0; clean.position[1] = 25.0; clean.position[2] = -3.0;

    // No head movement leaves the game's own camera exactly as it was, so the
    // frustum is the one the game would have built.
    CameraBasis tracked;
    double delta[16];
    mat4::Identity(delta);
    TrackedCameraBasis(delta, clean, tracked);
    CHECK_NEAR(tracked.forward[2], 1.0, 1e-12);
    CHECK_NEAR(tracked.right[0], 1.0, 1e-12);
    CHECK_NEAR(tracked.up[1], 1.0, 1e-12);
    CHECK_NEAR(tracked.position[0], 100.0, 1e-12);
    CHECK_NEAR(tracked.position[1], 25.0, 1e-12);
    CHECK_NEAR(tracked.position[2], -3.0, 1e-12);

    // Yawing the head right turns the culled camera right too: the frustum has
    // to cover what the player is now looking at, not what the game aims at.
    HeadPose pose;
    pose.yaw = 30.0f;
    BuildHeadDelta(pose, false, clean.forward, 1.0, delta);
    TrackedCameraBasis(delta, clean, tracked);
    CHECK_NEAR(tracked.forward[0], std::sin(30.0 * kDeg), 1e-6);
    CHECK_NEAR(tracked.forward[2], std::cos(30.0 * kDeg), 1e-6);
    CHECK_NEAR(tracked.forward[1], 0.0, 1e-6);
    CHECK_NEAR(tracked.up[1], 1.0, 1e-6);

    // Pitching up lifts the far edge of the frustum, and the basis stays
    // orthonormal so the plane builder keeps working.
    pose = HeadPose();
    pose.pitch = 20.0f;
    BuildHeadDelta(pose, false, clean.forward, 1.0, delta);
    TrackedCameraBasis(delta, clean, tracked);
    CHECK_NEAR(tracked.forward[1], std::sin(20.0 * kDeg), 1e-6);
    CHECK_NEAR(Dot(tracked.forward, tracked.up), 0.0, 1e-9);
    CHECK_NEAR(Dot(tracked.right, tracked.up), 0.0, 1e-9);
    CHECK_NEAR(Dot(tracked.forward, tracked.forward), 1.0, 1e-9);

    // A lean moves the eye the frustum is built from, in the camera's own
    // frame: negative z is the forward lean, so the eye moves along forward.
    pose = HeadPose();
    pose.z = -0.25f;
    pose.x = 0.10f;
    BuildHeadDelta(pose, false, clean.forward, 1.0, delta);
    TrackedCameraBasis(delta, clean, tracked);
    // The pose carries floats, so the lean lands within a float's worth of the metre.
    CHECK_NEAR(tracked.position[0], 100.0 + 0.10, 1e-7);
    CHECK_NEAR(tracked.position[1], 25.0, 1e-7);
    CHECK_NEAR(tracked.position[2], -3.0 + 0.25, 1e-7);

    // The same pose against a camera that faces world +X: every axis is
    // expressed through the clean camera's own basis, never world axes.
    CameraBasis facing_x;
    facing_x.right[0] = 0.0;   facing_x.right[1] = 0.0;   facing_x.right[2] = -1.0;
    facing_x.up[0] = 0.0;      facing_x.up[1] = 1.0;      facing_x.up[2] = 0.0;
    facing_x.forward[0] = 1.0; facing_x.forward[1] = 0.0; facing_x.forward[2] = 0.0;
    facing_x.position[0] = 0.0; facing_x.position[1] = 0.0; facing_x.position[2] = 0.0;
    pose = HeadPose();
    pose.yaw = 30.0f;
    BuildHeadDelta(pose, false, facing_x.forward, 1.0, delta);
    TrackedCameraBasis(delta, facing_x, tracked);
    CHECK_NEAR(tracked.forward[0], std::cos(30.0 * kDeg), 1e-6);
    CHECK_NEAR(tracked.forward[2], -std::sin(30.0 * kDeg), 1e-6);
}

// The same pose and view the pre-refactor HeadTrackingMod::BuildViewDelta was
// run on; the numbers are what it produced.
void TestBuildHeadDelta() {
    double v[16], rot[16], trans[16], mirror[16], tmp[16];
    mat4::Identity(mirror);
    mirror[10] = -1.0;
    mat4::RotAxis(rot, 0.6, 0.0, 0.8, 0.5);
    mat4::Translate(trans, -1200.0, -35.0, 860.0);
    mat4::Mul(tmp, mirror, rot);
    mat4::Mul(v, tmp, trans);

    HeadPose pose;
    pose.yaw = 20.0f;
    pose.pitch = -15.0f;
    pose.roll = 10.0f;
    pose.x = 0.1f;
    pose.y = -0.05f;
    pose.z = -0.2f;

    const double local[16] = {
        0.94078814549940604,   -0.075999422127130761,  -0.33036608954935215, 0,
        0.16773125949652062,   0.95125124256419769,    0.25881904510252074,  0,
        0.29459105532160884,   -0.29890660975698075,   0.90767337119036873,  0,
        -0.026774040909757095, -0.0046188176792989338, 0.22751223883833149,  1};
    const double world[16] = {
        0.92743720403180774,   -0.20280480053986172, -0.31421401218858791, 0,
        0.2488849409191238,    0.96182217817707794,  0.11381732623105162,  0,
        0.2791353054727177,    -0.18376155866962013, 0.94250473250481048,  0,
        -0.024472412627348401, 0.031619277700079981, 0.22561321739327234,  1};

    double out[16];
    // Zoom factor 1 still sends yaw and pitch through the float tangent round
    // trip in ScaleAngleForZoom, which moves the delta by a few 1e-8.
    BuildHeadDelta(pose, false, v, 1.0, out);
    CHECK_MAT(out, local, 1e-7);
    BuildHeadDelta(pose, true, v, 1.0, out);
    CHECK_MAT(out, world, 1e-7);

    // Signs: +yaw looks right, so the delta turns the world left about eye +Y.
    HeadPose yaw_only;
    yaw_only.yaw = 90.0f;
    double level[16];
    mat4::Identity(level);
    BuildHeadDelta(yaw_only, false, level, 1.0, out);
    CHECK_NEAR(out[8], 1.0, 1e-7);
    double world_level[16];
    BuildHeadDelta(yaw_only, true, level, 1.0, world_level);
    CHECK_MAT(world_level, out, 1e-12);

    HeadPose lean;
    lean.x = 0.3f;
    lean.z = -0.4f;
    BuildHeadDelta(lean, false, level, 1.0, out);
    CHECK_NEAR(out[12], -0.3, 1e-7);
    CHECK_NEAR(out[14], 0.4, 1e-7);
}

void TestMenuFov() {
    // The menu renders at exactly 1 radian; the measured constants either side
    // of it are the FOV slider's range and the two zooms.
    CHECK(IsMenuFov(57.29578));
    CHECK(IsMenuFov(57.296));
    CHECK(!IsMenuFov(57.0));
    CHECK(!IsMenuFov(60.0));
    CHECK(!IsMenuFov(70.0));
    CHECK(!IsMenuFov(110.0));
    CHECK(!IsMenuFov(22.918));  // Zoom, 0.4 rad
    CHECK(!IsMenuFov(5.730));   // binoculars, 0.1 rad
}

void TestClassifyUniform() {
    CHECK(ClassifyUniform("mat_view_proj") == CameraUniform::ViewProj);
    CHECK(ClassifyUniform("mat_view_proj_frag") == CameraUniform::ViewProj);
    CHECK(ClassifyUniform("mat_view_proj_prev") == CameraUniform::ViewProjPrev);
    CHECK(ClassifyUniform("mat_view_proj_next") == CameraUniform::ViewProjNext);
    CHECK(ClassifyUniform("mat_view_proj_inverse") == CameraUniform::ViewProjInverse);
    CHECK(ClassifyUniform("mat_view") == CameraUniform::View);
    CHECK(ClassifyUniform("camera_position") == CameraUniform::CameraPosition);
    CHECK(ClassifyUniform("camera_position_world") == CameraUniform::CameraPosition);
    CHECK(ClassifyUniform("camera_pos") == CameraUniform::CameraPosition);
    CHECK(ClassifyUniform("camera_direction") == CameraUniform::CameraDirection);
    CHECK(ClassifyUniform("mat_world") == CameraUniform::World);
    CHECK(ClassifyUniform("mat_proj") == CameraUniform::None);
    CHECK(ClassifyUniform("MAT_VIEW") == CameraUniform::None);
    CHECK(ClassifyUniform("") == CameraUniform::None);
}

void TestHudReticle() {
    double hud[16], sx = -1.0, sy = -1.0;
    mat4::Identity(hud);
    hud[0] = 0.5625;
    CHECK(HudProjectionScale(hud, sx, sy));
    CHECK(sx == 0.5625 && sy == 1.0);

    hud[12] = 0.1;
    CHECK(!HudProjectionScale(hud, sx, sy));
    CHECK(sx == 0.0 && sy == 0.0);
    hud[12] = 0.0;
    hud[5] = 0.9;
    CHECK(!HudProjectionScale(hud, sx, sy));
    hud[5] = 1.0;
    hud[0] = 0.0;
    CHECK(!HudProjectionScale(hud, sx, sy));

    float quad[16] = {0};
    quad[0] = quad[5] = 0.03f;
    quad[10] = quad[15] = 1.0f;
    quad[12] = quad[13] = -0.015f;
    CHECK(IsCentredReticleQuad(quad));
    float wide[16];
    for (int i = 0; i < 16; ++i) wide[i] = quad[i];
    wide[0] = 0.04f;
    CHECK(!IsCentredReticleQuad(wide));
    for (int i = 0; i < 16; ++i) wide[i] = quad[i];
    wide[0] = wide[5] = 0.1f;
    wide[12] = wide[13] = -0.05f;
    CHECK(!IsCentredReticleQuad(wide));
    for (int i = 0; i < 16; ++i) wide[i] = quad[i];
    wide[12] = 0.0f;
    CHECK(!IsCentredReticleQuad(wide));

    double p[16], h[16];
    Perspective(p, 0.8, 1.42, 0.0, 0.0);
    double ndc_x = 9.0, ndc_y = 9.0;
    mat4::Identity(h);
    CHECK(ProjectCleanForward(p, h, ndc_x, ndc_y));
    CHECK(ndc_x == 0.0 && ndc_y == 0.0);

    // The head yawed 20 degrees: the clean forward lands at tan(20) off centre.
    HeadPose pose;
    pose.yaw = 20.0f;
    mat4::Identity(h);
    double level[16];
    mat4::Identity(level);
    BuildHeadDelta(pose, false, level, 1.0, h);
    CHECK(ProjectCleanForward(p, h, ndc_x, ndc_y));
    CHECK_NEAR(ndc_x, -0.8 * std::tan(20.0 * kPi / 180.0), 1e-6);
    CHECK_NEAR(ndc_y, 0.0, 1e-12);

    pose.yaw = 120.0f;
    BuildHeadDelta(pose, false, level, 1.0, h);
    CHECK(!ProjectCleanForward(p, h, ndc_x, ndc_y));
}

std::string TempDir() {
    char buf[MAX_PATH];
    GetTempPathA(MAX_PATH, buf);
    std::string dir = std::string(buf) + "stormworks_ht_tests_" + std::to_string(GetCurrentProcessId());
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

// Text mode: the writer opens its file in text mode, so on disk it is CRLF.
std::string ReadFile(const std::string& path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void WriteFile(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

// Byte for byte what a first run writes next to the game.
const char* const kDefaultIni =
    ";  Stormworks Head Tracking configuration\n"
    ";  Decoupled look (head moves the view; mouse/keyboard still aim).\n"
    "\n"
    "[Tracking]\n"
    "Port=4242\n"
    "EnableOnStartup=1\n"
    ";  Yaw mode: 1 = horizon-locked yaw (default), 0 = camera-local\n"
    "WorldSpaceYaw=1\n"
    "YawSensitivity=1\n"
    "PitchSensitivity=1\n"
    "RollSensitivity=1\n"
    "InvertYaw=0\n"
    "InvertPitch=0\n"
    "InvertRoll=0\n"
    ";  Smoothing 0.0 = lightest, 1.0 = heaviest. Covers rotation and position.\n"
    ";  The value is picked per connection from the packet source address:\n"
    ";  LocalSmoothing for a tracker sending to 127.0.0.1 on this PC,\n"
    ";  RemoteSmoothing for anything else, including a tracker on this PC\n"
    ";  that sends to this machine's LAN address instead of 127.0.0.1.\n"
    "LocalSmoothing=0\n"
    "RemoteSmoothing=0.15\n"
    "\n"
    "[Position]\n"
    "PositionEnabled=1\n"
    "PositionSensitivityX=1\n"
    "PositionSensitivityY=1\n"
    "PositionSensitivityZ=1\n"
    "PositionLimitX=0.3\n"
    ";  How far the view may move, in metres. Y is up and YDown is down.\n"
    "PositionLimitY=0.2\n"
    "PositionLimitYDown=0.2\n"
    "PositionLimitZForward=0.4\n"
    "PositionLimitZBack=0.1\n"
    "InvertPositionX=0\n"
    "InvertPositionY=0\n"
    "InvertPositionZ=0\n"
    "\n"
    "[Hotkeys]\n"
    ";  Windows virtual key codes. End=toggle PageUp=cycle mode PageDown=yaw mode.\n"
    ";  Chord alternatives Ctrl+Shift+Y / G / H are always active too.\n"
    "ToggleKey=0x23\n"
    "CycleModeKey=0x21\n"
    "YawModeKey=0x22\n"
    "\n"
    "[Advanced]\n"
    "DataFreshnessMs=500\n";

void CheckDefaults(const Config& c, const char* file, int line) {
    const Config d;
    Check(c.port == 4242 && d.port == 4242, "port", file, line);
    Check(c.enable_on_startup && c.world_space_yaw, "startup flags", file, line);
    Check(c.yaw_sensitivity == 1.0f && c.pitch_sensitivity == 1.0f && c.roll_sensitivity == 1.0f,
          "sensitivities", file, line);
    Check(!c.invert_yaw && !c.invert_pitch && !c.invert_roll, "inverts", file, line);
    Check(c.local_smoothing == 0.0f && c.remote_smoothing == 0.15f, "smoothing", file, line);
    Check(c.position_enabled, "position enabled", file, line);
    Check(c.position_sensitivity_x == 1.0f && c.position_sensitivity_y == 1.0f &&
              c.position_sensitivity_z == 1.0f,
          "position sensitivities", file, line);
    Check(c.position_limit_x == 0.30f && c.position_limit_y == 0.20f &&
              c.position_limit_y_down == 0.20f && c.position_limit_z == 0.40f &&
              c.position_limit_z_back == 0.10f,
          "position limits", file, line);
    Check(!c.invert_position_x && !c.invert_position_y && !c.invert_position_z, "position inverts",
          file, line);
    Check(c.toggle_key == 0x23 && c.cycle_mode_key == 0x21 && c.yaw_mode_key == 0x22, "hotkeys", file,
          line);
    Check(c.data_freshness_ms == 500, "freshness", file, line);
}

void TestConfig() {
    const std::string dir = TempDir();
    const std::string path = dir + "\\StormworksHeadTracking.ini";
    DeleteFileA(path.c_str());

    Config missing;
    CHECK(!missing.Load(path));
    CheckDefaults(missing, __FILE__, __LINE__);

    CHECK(WriteDefaultConfig(path));
    CHECK(ReadFile(path) == kDefaultIni);
    CHECK(!WriteDefaultConfig(path));

    Config loaded;
    CHECK(loaded.Load(path));
    CheckDefaults(loaded, __FILE__, __LINE__);

    WriteFile(path, "[Tracking]\n"
                    "Port=5000\n"
                    "EnableOnStartup=0\n"
                    "WorldSpaceYaw=0\n"
                    "YawSensitivity=2.5\n"
                    "InvertPitch=1\n"
                    "LocalSmoothing=0.4\n"
                    "RemoteSmoothing=0.6\n"
                    "[Position]\n"
                    "PositionEnabled=0\n"
                    "PositionSensitivityZ=0.5\n"
                    "PositionLimitX=0.25\n"
                    "PositionLimitY=0.15\n"
                    "PositionLimitZForward=0.35\n"
                    "PositionLimitZBack=0.05\n"
                    "InvertPositionY=1\n"
                    "[Hotkeys]\n"
                    "ToggleKey=0x70\n"
                    "CycleModeKey=0x71\n"
                    "YawModeKey=0x72\n"
                    "[Advanced]\n"
                    "DataFreshnessMs=750\n");
    Config custom;
    CHECK(custom.Load(path));
    CHECK(custom.port == 5000);
    CHECK(!custom.enable_on_startup);
    CHECK(!custom.world_space_yaw);
    CHECK(custom.yaw_sensitivity == 2.5f);
    CHECK(custom.invert_pitch && !custom.invert_yaw);
    CHECK(custom.local_smoothing == 0.4f);
    CHECK(custom.remote_smoothing == 0.6f);
    CHECK(!custom.position_enabled);
    CHECK(custom.position_sensitivity_z == 0.5f);
    CHECK(custom.position_limit_x == 0.25f);
    CHECK(custom.position_limit_y == 0.15f);
    CHECK(custom.position_limit_z == 0.35f);
    CHECK(custom.position_limit_z_back == 0.05f);
    CHECK(custom.invert_position_y);
    CHECK(custom.toggle_key == 0x70 && custom.cycle_mode_key == 0x71 && custom.yaw_mode_key == 0x72);
    CHECK(custom.data_freshness_ms == 750);

    // Ports below 1024 or above 65535 keep the previous value.
    WriteFile(path, "[Tracking]\nPort=80\n");
    Config low;
    low.Load(path);
    CHECK(low.port == 4242);
    WriteFile(path, "[Tracking]\nPort=70000\n");
    Config high;
    high.Load(path);
    CHECK(high.port == 4242);

    // A freshness window of zero or less keeps the previous value.
    WriteFile(path, "[Advanced]\nDataFreshnessMs=0\n");
    Config stale;
    stale.Load(path);
    CHECK(stale.data_freshness_ms == 500);

    // Smoothing: out of range clamps to the nearest end, non-finite falls back
    // to that key's own default.
    WriteFile(path, "[Tracking]\nLocalSmoothing=1.5\nRemoteSmoothing=-0.2\n");
    Config clamped;
    clamped.Load(path);
    CHECK(clamped.local_smoothing == 1.0f);
    CHECK(clamped.remote_smoothing == 0.0f);
    WriteFile(path, "[Tracking]\nLocalSmoothing=nan\nRemoteSmoothing=inf\n");
    Config nonfinite;
    nonfinite.Load(path);
    CHECK(nonfinite.local_smoothing == 0.0f);
    CHECK(nonfinite.remote_smoothing == 0.15f);

    // A decimal comma parsed as a prefix used to read as 0.
    WriteFile(path, "[Tracking]\nRemoteSmoothing=0,4\n");
    Config comma;
    comma.Load(path);
    CHECK(comma.remote_smoothing == 0.15f);

    // A trailing comment on a number still reads.
    WriteFile(path, "[Tracking]\nYawSensitivity=1.5 ; boost\n");
    Config commented;
    commented.Load(path);
    CHECK(commented.yaw_sensitivity == 1.5f);

    // Non-finite sensitivities keep the default; negative stays an inversion.
    WriteFile(path, "[Tracking]\nYawSensitivity=nan\nPitchSensitivity=-2\nRollSensitivity=1e400\n"
                    "[Position]\nPositionSensitivityX=inf\n");
    Config sens;
    sens.Load(path);
    CHECK(sens.yaw_sensitivity == 1.0f);
    CHECK(sens.pitch_sensitivity == -2.0f);
    CHECK(sens.roll_sensitivity == 1.0f);
    CHECK(sens.position_sensitivity_x == 1.0f);

    // A negative limit would invert the processor's clamp; a non-finite one keeps the default.
    WriteFile(path, "[Position]\nPositionLimitX=-0.3\nPositionLimitY=nan\nPositionLimitZBack=0.05\n");
    Config limits;
    limits.Load(path);
    CHECK(limits.position_limit_x == 0.0f);
    CHECK(limits.position_limit_y == 0.20f);
    CHECK(limits.position_limit_z_back == 0.05f);

    // A key GetAsyncKeyState cannot poll, or a chord modifier, keeps the default.
    WriteFile(path, "[Hotkeys]\nToggleKey=0x230\nCycleModeKey=0x11\nYawModeKey=0x70\n");
    Config keys;
    keys.Load(path);
    CHECK(keys.toggle_key == 0x23);
    CHECK(keys.cycle_mode_key == 0x21);
    CHECK(keys.yaw_mode_key == 0x70);

    // A key name, and a code written in decimal, both used to parse as a prefix
    // and bind something the player never asked for (0x0E and the digit-5 key).
    WriteFile(path, "[Hotkeys]\nToggleKey=End\nCycleModeKey=35\nYawModeKey=0x70 ; F1\n");
    Config key_text;
    key_text.Load(path);
    CHECK(key_text.toggle_key == 0x23);
    CHECK(key_text.cycle_mode_key == 0x21);
    CHECK(key_text.yaw_mode_key == 0x70);

    // A bool with a trailing comment, or a word the reader does not know, used
    // to keep the default silently. Recognised spellings still read.
    WriteFile(path, "[Tracking]\nEnableOnStartup=0 ; off for now\nWorldSpaceYaw=maybe\n"
                    "[Position]\nPositionEnabled=FALSE\nInvertPositionX=yes\n");
    Config bools;
    bools.Load(path);
    CHECK(!bools.enable_on_startup);
    CHECK(bools.world_space_yaw);
    CHECK(!bools.position_enabled);
    CHECK(bools.invert_position_x);

    // The two vertical limits are independent, so a tighter crouch range is
    // configurable on its own.
    WriteFile(path, "[Position]\nPositionLimitY=0.3\nPositionLimitYDown=0.05\n");
    Config vertical;
    vertical.Load(path);
    CHECK(vertical.position_limit_y == 0.30f);
    CHECK(vertical.position_limit_y_down == 0.05f);

    DeleteFileA(path.c_str());
    RemoveDirectoryA(dir.c_str());
}

}  // namespace

int main() {
    TestMat4();
    TestSplitViewProj();
    TestIsRigidAndEye();
    TestViewProjCorrection();
    TestNear3();
    TestBuildHeadDelta();
    TestTrackedCameraBasis();
    TestMenuFov();
    TestClassifyUniform();
    TestHudReticle();
    TestConfig();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
