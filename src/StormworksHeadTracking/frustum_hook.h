#pragma once

namespace stormworks_ht {

// Hooks the game's view-frustum builder when the running EXE matches a known
// build profile. Leaves the game untouched otherwise.
void InstallFrustumHook();

// Publishes this frame's eye-space delta and the main camera's viewport aspect
// and vertical FOV in degrees, so the cull can pick the player's camera out of
// the shadow and reflection cameras that share the builder. A null delta means
// nothing is being injected and the game's own frustum stands. Called on the GL
// thread; the cull reads it from whichever thread the renderer culls on.
void SetTrackedCamera(const double* delta, double main_aspect, double main_vertical_fov_deg);

}  // namespace stormworks_ht
