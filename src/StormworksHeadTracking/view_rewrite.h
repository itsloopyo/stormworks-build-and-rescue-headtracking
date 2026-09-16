#pragma once

namespace stormworks_ht {

// A view-projection M = P * V split back into its projection and its rigid
// view. P is an OpenGL perspective whose w row is (0, 0, -1, 0), with optional
// sub-pixel jitter in P[0][2] / P[1][2]; V is an orthonormal 3x3 of either
// handedness plus a translation.
struct PerspectiveSplit {
    double P[16];
    double V[16];
    double aspect;  // P[1][1] / P[0][0]; the viewport aspect for the main camera
};

// Returns false when m is not of that shape: an orthographic shadow cascade, a
// screen-space quad's identity, anything whose view is not orthonormal, or a
// projection with no depth offset, which is singular.
bool SplitViewProj(const double* m, PerspectiveSplit& out);

// True when v is a rigid transform (orthonormal rotation plus translation,
// bottom row 0 0 0 1). Stormworks' world is mirrored against GL eye space, so
// its view rotations have determinant -1 and both signs are accepted.
bool IsRigid(const double* v);

// Eye position in world space of a rigid view matrix.
void EyeOf(const double* v, double eye[3]);

// C = P * delta * P^-1, so C * M equals P * delta * V for M = P * V without
// rebuilding the matrix from its parts. A rebuild, and above all re-inverting
// the game's own mat_view_proj_inverse, perturbs the low bits of every upload,
// and the lighting pass shows that as shadows and black patches flickering
// frame to frame even with the head centred. C is exactly identity there.
void ViewProjCorrection(const double* P, const double* delta, double* out);

// True when every component of v is within tolerance of ref. Float uniforms of
// a camera kilometres from the origin carry that much absolute error, so the
// match widens with distance.
bool Near3(const double* v, const double* ref, double tolerance);

// A processed tracker pose: degrees, positive yaw looking right, positive pitch
// looking up; position in metres with negative z a forward lean.
struct HeadPose {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Column-major view-space delta H for the pose: a tracked view is H * V.
// World-space yaw turns about the world's up axis as the clean view sees it,
// which is why the delta needs that view. zoom_factor is tan(fov/2) of this
// frame over the un-zoomed one (1 when not zoomed); yaw, pitch and position
// shrink by it so the head moves the picture as far as it would un-zoomed. Roll
// rotates the picture the same at any FOV and is left alone.
void BuildHeadDelta(const HeadPose& pose, bool world_space_yaw, const double* clean_view,
                    double zoom_factor, double* out);

// A camera in the engine's world space: an orthonormal basis where right cross
// up is forward, plus the eye position. It is how the game states its camera to
// the code that builds the cull frustum.
struct CameraBasis {
    double right[3];
    double up[3];
    double forward[3];
    double position[3];
};

// The camera the head is looking through, given the clean camera and the
// eye-space delta H (a tracked view is H * clean view). GL eye space is +x
// right, +y up, looking down -z, so the tracked camera's own axes are the rows
// of H's rotation and its eye sits at -R^T t, each carried into world space
// through the clean camera's basis.
void TrackedCameraBasis(const double* delta, const CameraBasis& clean, CameraBasis& tracked);

// The game's un-zoomed vertical FOV, read off the main camera's projection.
//
// Stormworks stores its three FOV settings (First Person, First Person Vehicle,
// Third Person) as whole degrees from 60 to 110 and renders them exactly. Every
// narrower view it draws is a radian constant instead: the Zoom action 0.4 rad
// (22.918 degrees), binoculars 0.1 rad (5.730), the main menu 1 rad (57.296).
// So a projection that holds still at a whole degree inside the settings range
// is the setting for whichever camera mode is live, and it stays the reference
// while a zoom narrows the view below it.
// True for the vertical FOV Stormworks renders its menus at, 1 radian exactly.
// The menu backdrop is a perspective camera at the window's aspect like any
// other, so without this head tracking turns the view while the player is
// clicking menu buttons. No gameplay FOV can collide with it: the three sliders
// are whole degrees from 60 to 110 and the zooms are their own radian
// constants, all listed on UnzoomedFov below.
bool IsMenuFov(double vertical_degrees);

class UnzoomedFov {
public:
    // Feed P[1][1] of the frame's main camera, once per frame. Returns true
    // when that changed the reference.
    bool Observe(double p11);

    bool Known() const { return m_tan_half > 0.0; }
    double TanHalf() const { return m_tan_half; }
    double Degrees() const { return m_degrees; }

private:
    double m_last_p11 = 0.0;
    int m_steady_frames = 0;
    double m_tan_half = 0.0;
    double m_degrees = 0.0;
};


}  // namespace stormworks_ht
