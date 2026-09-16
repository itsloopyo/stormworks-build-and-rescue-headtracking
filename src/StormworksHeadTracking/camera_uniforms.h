#pragma once

namespace stormworks_ht {

// The shader uniforms that carry the camera (rom/graphics/shaders), by role.
enum class CameraUniform : unsigned char {
    None,
    ViewProj,         // mat_view_proj, mat_view_proj_frag
    ViewProjPrev,     // mat_view_proj_prev
    ViewProjNext,     // mat_view_proj_next
    ViewProjInverse,  // mat_view_proj_inverse
    View,             // mat_view
    CameraPosition,   // camera_position, camera_position_world, camera_pos
    CameraDirection,  // camera_direction
    World,            // mat_world
};

CameraUniform ClassifyUniform(const char* name);

}  // namespace stormworks_ht
