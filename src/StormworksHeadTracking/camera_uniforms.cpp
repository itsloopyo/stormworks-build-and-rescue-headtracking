#include "camera_uniforms.h"

#include <cstring>

namespace stormworks_ht {

CameraUniform ClassifyUniform(const char* name) {
    if (std::strcmp(name, "mat_view_proj") == 0 || std::strcmp(name, "mat_view_proj_frag") == 0)
        return CameraUniform::ViewProj;
    if (std::strcmp(name, "mat_view_proj_prev") == 0) return CameraUniform::ViewProjPrev;
    if (std::strcmp(name, "mat_view_proj_next") == 0) return CameraUniform::ViewProjNext;
    if (std::strcmp(name, "mat_view_proj_inverse") == 0) return CameraUniform::ViewProjInverse;
    if (std::strcmp(name, "mat_view") == 0) return CameraUniform::View;
    if (std::strcmp(name, "camera_position") == 0 || std::strcmp(name, "camera_position_world") == 0 ||
        std::strcmp(name, "camera_pos") == 0)
        return CameraUniform::CameraPosition;
    if (std::strcmp(name, "camera_direction") == 0) return CameraUniform::CameraDirection;
    if (std::strcmp(name, "mat_world") == 0) return CameraUniform::World;
    return CameraUniform::None;
}

}  // namespace stormworks_ht
