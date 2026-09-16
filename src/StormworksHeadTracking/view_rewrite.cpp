#include "view_rewrite.h"

#include <cmath>

#include "cameraunlock/camera/zoom_compensation.h"
#include "mat4.h"

namespace stormworks_ht {

namespace {
constexpr double kUnitTolerance = 1e-3;
constexpr double kMinDepthOffset = 1e-6;
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

// OpenGL eye space: +X right, +Y up, camera looking down -Z. The processed pose
// is positive yaw = looking right, positive pitch = looking up; its z is
// negative for a forward lean, which is already the GL eye-space sign.
constexpr double kYawSign = -1.0;
constexpr double kPitchSign = 1.0;
constexpr double kRollSign = 1.0;
constexpr double kPosXSign = 1.0;
constexpr double kPosYSign = 1.0;
constexpr double kPosZSign = 1.0;

// ScaleAngleForZoom is only defined inside +/-90 and tends to +/-90 at that edge,
// so an angle a tracker curve maps past it is kept as is and the result stays
// continuous.
double ZoomedAngle(float degrees, float zoom_factor) {
    return std::fabs(degrees) < 90.0f ? cameraunlock::camera::ScaleAngleForZoom(degrees, zoom_factor)
                                      : degrees;
}

void Row(const double* m, int r, double out[4]) {
    out[0] = m[r];
    out[1] = m[4 + r];
    out[2] = m[8 + r];
    out[3] = m[12 + r];
}

double Dot3(const double* a, const double* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double Det3(const double* r0, const double* r1, const double* r2) {
    return r0[0] * (r1[1] * r2[2] - r1[2] * r2[1]) -
           r0[1] * (r1[0] * r2[2] - r1[2] * r2[0]) +
           r0[2] * (r1[0] * r2[1] - r1[1] * r2[0]);
}

bool Orthonormal(const double* r0, const double* r1, const double* r2) {
    return std::fabs(Dot3(r0, r0) - 1.0) < kUnitTolerance &&
           std::fabs(Dot3(r1, r1) - 1.0) < kUnitTolerance &&
           std::fabs(Dot3(r2, r2) - 1.0) < kUnitTolerance &&
           std::fabs(Dot3(r0, r1)) < kUnitTolerance && std::fabs(Dot3(r0, r2)) < kUnitTolerance &&
           std::fabs(Dot3(r1, r2)) < kUnitTolerance &&
           std::fabs(std::fabs(Det3(r0, r1, r2)) - 1.0) < kUnitTolerance;
}

void SetRow(double* m, int r, const double v[4]) {
    m[r] = v[0];
    m[4 + r] = v[1];
    m[8 + r] = v[2];
    m[12 + r] = v[3];
}
}  // namespace

bool SplitViewProj(const double* m, PerspectiveSplit& out) {
    double m0[4], m1[4], m2[4], m3[4];
    Row(m, 0, m0);
    Row(m, 1, m1);
    Row(m, 2, m2);
    Row(m, 3, m3);

    // Rows of P * V: m3 = -v2, m0 = a*v0 + jx*v2, m1 = b*v1 + jy*v2,
    // m2 = c*v2 + d*(0,0,0,1).
    double v2[4] = {-m3[0], -m3[1], -m3[2], -m3[3]};
    if (std::fabs(Dot3(v2, v2) - 1.0) > kUnitTolerance) return false;

    const double jx = Dot3(m0, v2);
    const double jy = Dot3(m1, v2);
    double v0[4], v1[4];
    for (int i = 0; i < 4; ++i) {
        v0[i] = m0[i] - jx * v2[i];
        v1[i] = m1[i] - jy * v2[i];
    }
    const double a = std::sqrt(Dot3(v0, v0));
    const double b = std::sqrt(Dot3(v1, v1));
    if (a <= 0.0 || b <= 0.0) return false;
    for (int i = 0; i < 4; ++i) {
        v0[i] /= a;
        v1[i] /= b;
    }
    if (!Orthonormal(v0, v1, v2)) return false;

    const double c = Dot3(m2, v2);
    const double d = m2[3] - c * v2[3];
    const double residual[3] = {m2[0] - c * v2[0], m2[1] - c * v2[1], m2[2] - c * v2[2]};
    if (std::sqrt(Dot3(residual, residual)) > kUnitTolerance * (std::fabs(c) + 1.0)) return false;
    // det(P) = a * b * d, and ViewProjCorrection inverts P. d is -2 * near for
    // an infinite far plane (-0.05 in game), and a d recovered from a singular
    // P comes back as rounding noise rather than 0.
    if (std::fabs(d) < kMinDepthOffset) return false;

    mat4::Identity(out.V);
    SetRow(out.V, 0, v0);
    SetRow(out.V, 1, v1);
    SetRow(out.V, 2, v2);

    for (int i = 0; i < 16; ++i) out.P[i] = 0.0;
    out.P[0] = a;
    out.P[8] = jx;
    out.P[5] = b;
    out.P[9] = jy;
    out.P[10] = c;
    out.P[14] = d;
    out.P[11] = -1.0;
    out.aspect = b / a;
    return true;
}

bool IsRigid(const double* v) {
    if (v[3] != 0.0 || v[7] != 0.0 || v[11] != 0.0 || std::fabs(v[15] - 1.0) > 1e-9) return false;
    double r0[4], r1[4], r2[4];
    Row(v, 0, r0);
    Row(v, 1, r1);
    Row(v, 2, r2);
    return Orthonormal(r0, r1, r2);
}

namespace {
void EyeToWorld(const CameraBasis& clean, const double eye[3], double out[3]) {
    for (int i = 0; i < 3; ++i) {
        out[i] = eye[0] * clean.right[i] + eye[1] * clean.up[i] - eye[2] * clean.forward[i];
    }
}

void Normalize(double v[3]) {
    const double len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len <= 0.0) return;
    for (int i = 0; i < 3; ++i) v[i] /= len;
}
}  // namespace

void TrackedCameraBasis(const double* delta, const CameraBasis& clean, CameraBasis& tracked) {
    const double right_eye[3] = {delta[0], delta[4], delta[8]};
    const double up_eye[3] = {delta[1], delta[5], delta[9]};
    const double forward_eye[3] = {-delta[2], -delta[6], -delta[10]};
    double eye_offset[3];
    for (int i = 0; i < 3; ++i) {
        eye_offset[i] = -(delta[i * 4 + 0] * delta[12] + delta[i * 4 + 1] * delta[13] +
                          delta[i * 4 + 2] * delta[14]);
    }

    EyeToWorld(clean, right_eye, tracked.right);
    EyeToWorld(clean, up_eye, tracked.up);
    EyeToWorld(clean, forward_eye, tracked.forward);
    Normalize(tracked.right);
    Normalize(tracked.up);
    Normalize(tracked.forward);

    double world_offset[3];
    EyeToWorld(clean, eye_offset, world_offset);
    for (int i = 0; i < 3; ++i) tracked.position[i] = clean.position[i] + world_offset[i];
}

bool IsMenuFov(double vertical_degrees) {
    constexpr double kMenuDegrees = 180.0 / 3.14159265358979323846;  // 1 radian
    // Wide enough for the decompose's low bits, far narrower than the gap to
    // the nearest whole degree a setting can hold (57 and 58 are both outside
    // the 60-110 range anyway, so the nearest real neighbour is 60).
    constexpr double kTolerance = 0.05;
    return std::fabs(vertical_degrees - kMenuDegrees) < kTolerance;
}

bool UnzoomedFov::Observe(double p11) {
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    constexpr double kSettingMinDeg = 60.0;
    constexpr double kSettingMaxDeg = 110.0;
    constexpr double kWholeDegreeTolerance = 0.01;
    // P[1][1] comes out of a decompose of float uploads, so it wobbles in its low
    // bits as the camera turns; a zoom animation moves it by far more per frame.
    constexpr double kSteadyTolerance = 1e-5;
    constexpr int kSteadyFramesRequired = 3;

    if (std::fabs(p11 - m_last_p11) < kSteadyTolerance * p11) {
        ++m_steady_frames;
    } else {
        m_steady_frames = 1;
    }
    m_last_p11 = p11;
    if (m_steady_frames != kSteadyFramesRequired) return false;

    const double degrees = 2.0 * std::atan(1.0 / p11) * kRadToDeg;
    const double whole = std::round(degrees);
    if (std::fabs(degrees - whole) > kWholeDegreeTolerance || whole < kSettingMinDeg ||
        whole > kSettingMaxDeg || whole == m_degrees)
        return false;
    m_degrees = whole;
    m_tan_half = std::tan(0.5 * whole / kRadToDeg);
    return true;
}

void EyeOf(const double* v, double eye[3]) {
    // eye = -R^T t, with R in the upper 3x3 and t in column 3.
    for (int i = 0; i < 3; ++i) {
        eye[i] = -(v[i * 4 + 0] * v[12] + v[i * 4 + 1] * v[13] + v[i * 4 + 2] * v[14]);
    }
}

void ViewProjCorrection(const double* P, const double* delta, double* out) {
    double p_inv[16], hp[16];
    mat4::Invert(p_inv, P);
    mat4::Mul(hp, delta, p_inv);
    mat4::Mul(out, P, hp);
}

bool Near3(const double* v, const double* ref, double tolerance) {
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(v[i] - ref[i]) > tolerance + 1e-5 * std::fabs(ref[i])) return false;
    }
    return true;
}

void BuildHeadDelta(const HeadPose& pose, bool world_space_yaw, const double* clean_view,
                    double zoom_factor, double* out) {
    // The head turns the camera by Q = Yaw * RotX(pitch) * RotZ(roll) after
    // moving the eye by the offset in the clean camera's frame, so the camera
    // transform becomes C * T(o) * Q and the view V' = Q^-1 * T(-o) * V.
    // Camera-local yaw is RotY, about the clean camera's up. World-space yaw
    // turns about the world's up instead, which the clean view carries into eye
    // space as its second column (Stormworks' world is +Y up). The two agree
    // whenever the game camera is level.
    const float zoom = static_cast<float>(zoom_factor);
    const double yaw_rad = -ZoomedAngle(pose.yaw, zoom) * kDegToRad * kYawSign;
    double ry[16], rx[16], rz[16], tmp[16], inv_rot[16], trans[16];
    if (world_space_yaw) {
        double ux = clean_view[4], uy = clean_view[5], uz = clean_view[6];
        const double len = std::sqrt(ux * ux + uy * uy + uz * uz);
        mat4::RotAxis(ry, ux / len, uy / len, uz / len, yaw_rad);
    } else {
        mat4::RotY(ry, yaw_rad);
    }
    mat4::RotX(rx, -ZoomedAngle(pose.pitch, zoom) * kDegToRad * kPitchSign);
    mat4::RotZ(rz, -pose.roll * kDegToRad * kRollSign);
    mat4::Mul(tmp, rz, rx);
    mat4::Mul(inv_rot, tmp, ry);

    mat4::Translate(trans, -kPosXSign * pose.x * zoom_factor, -kPosYSign * pose.y * zoom_factor,
                    -kPosZSign * pose.z * zoom_factor);

    mat4::Mul(out, inv_rot, trans);
}

}  // namespace stormworks_ht
