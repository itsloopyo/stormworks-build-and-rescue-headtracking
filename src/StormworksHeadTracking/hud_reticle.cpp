#include "hud_reticle.h"

#include <cmath>

namespace stormworks_ht {

namespace {
constexpr float kMaxReticleQuadSize = 0.1f;
constexpr float kQuadCentringTolerance = 1e-5f;
constexpr double kMinClipW = 1e-3;
}  // namespace

bool HudProjectionScale(const double* m, double& sx, double& sy) {
    const bool hud = m[15] == 1.0 && m[3] == 0.0 && m[7] == 0.0 && m[11] == 0.0 && m[12] == 0.0 &&
                     m[13] == 0.0 && m[1] == 0.0 && m[4] == 0.0 && m[5] == 1.0 && m[0] > 0.0;
    sx = hud ? m[0] : 0.0;
    sy = hud ? m[5] : 0.0;
    return hud;
}

bool IsCentredReticleQuad(const float* v) {
    return v[0] == v[5] && v[0] > 0.0f && v[0] < kMaxReticleQuadSize && v[1] == 0.0f && v[2] == 0.0f &&
           v[4] == 0.0f && v[6] == 0.0f && std::fabs(v[12] + 0.5f * v[0]) < kQuadCentringTolerance &&
           std::fabs(v[13] + 0.5f * v[5]) < kQuadCentringTolerance;
}

bool ProjectCleanForward(const double* projection, const double* delta, double& ndc_x, double& ndc_y) {
    // Clean forward in eye space is (0, 0, -1, 0); its tracked clip position
    // is P * H * forward, which is minus column 2 of H pushed through P.
    double eye_dir[4], clip[4];
    for (int r = 0; r < 4; ++r) eye_dir[r] = -delta[8 + r];
    for (int r = 0; r < 4; ++r) {
        clip[r] = 0.0;
        for (int c = 0; c < 4; ++c) clip[r] += projection[c * 4 + r] * eye_dir[c];
    }
    if (clip[3] < kMinClipW) return false;
    ndc_x = clip[0] / clip[3];
    ndc_y = clip[1] / clip[3];
    return true;
}

}  // namespace stormworks_ht
