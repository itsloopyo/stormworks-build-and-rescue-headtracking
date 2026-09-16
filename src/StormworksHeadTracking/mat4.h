#pragma once

#include <cmath>

// Column-major 4x4 helpers in double: element (row r, col c) lives at index
// c*4 + r, the layout glUniformMatrix4fv consumes with transpose == GL_FALSE.
// Double because the game uploads world-space matrices, and a camera a few
// kilometres from the origin loses visible precision in a float decompose.
namespace stormworks_ht::mat4 {

inline void Mul(double* out, const double* a, const double* b) {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            out[c * 4 + r] =
                a[0 * 4 + r] * b[c * 4 + 0] +
                a[1 * 4 + r] * b[c * 4 + 1] +
                a[2 * 4 + r] * b[c * 4 + 2] +
                a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
}

inline void Copy(double* out, const double* m) {
    for (int i = 0; i < 16; ++i) out[i] = m[i];
}

inline void Identity(double* m) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0;
    m[0] = m[5] = m[10] = m[15] = 1.0;
}

inline void Translate(double* m, double x, double y, double z) {
    Identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

inline void RotX(double* m, double rad) {
    Identity(m);
    double c = std::cos(rad), s = std::sin(rad);
    m[5] = c;  m[9] = -s;
    m[6] = s;  m[10] = c;
}

inline void RotY(double* m, double rad) {
    Identity(m);
    double c = std::cos(rad), s = std::sin(rad);
    m[0] = c;  m[8] = s;
    m[2] = -s; m[10] = c;
}

inline void RotZ(double* m, double rad) {
    Identity(m);
    double c = std::cos(rad), s = std::sin(rad);
    m[0] = c;  m[4] = -s;
    m[1] = s;  m[5] = c;
}

// Right-handed rotation about the unit axis (x, y, z); RotY for (0, 1, 0).
inline void RotAxis(double* m, double x, double y, double z, double rad) {
    Identity(m);
    double c = std::cos(rad), s = std::sin(rad), t = 1.0 - c;
    m[0] = t * x * x + c;      m[4] = t * x * y - s * z;  m[8] = t * x * z + s * y;
    m[1] = t * x * y + s * z;  m[5] = t * y * y + c;      m[9] = t * y * z - s * x;
    m[2] = t * x * z - s * y;  m[6] = t * y * z + s * x;  m[10] = t * z * z + c;
}

// General inverse by cofactors. Returns false for a singular matrix.
inline bool Invert(double* out, const double* m) {
    double inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
             m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
              m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
             m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0.0 || !std::isfinite(det)) return false;
    double inv_det = 1.0 / det;
    for (int i = 0; i < 16; ++i) out[i] = inv[i] * inv_det;
    return true;
}

}  // namespace stormworks_ht::mat4
