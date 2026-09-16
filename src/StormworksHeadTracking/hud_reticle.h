#pragma once

namespace stormworks_ht {

// Scale of the HUD's orthographic mat_view_proj (x = 1/aspect, y = 1, no
// translation). Returns false, with both scales 0, for any other projection.
bool HudProjectionScale(const double* m, double& sx, double& sy);

// The crosshair is a HUD quad: the unit square scaled by s and translated by
// -s/2 so it sits centred on screen. True when the mat_world v is that quad.
bool IsCentredReticleQuad(const float* v);

// Where the clean camera's forward lands in the tracked view: the NDC of
// projection * delta * (0, 0, -1, 0). Returns false when the direction points
// out of the tracked view and there is nowhere on screen to mark.
bool ProjectCleanForward(const double* projection, const double* delta, double& ndc_x, double& ndc_y);

}  // namespace stormworks_ht
