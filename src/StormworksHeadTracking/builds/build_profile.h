#pragma once

#include <cstddef>
#include <cstdint>

#include "cameraunlock/memory/pe_fingerprint.h"

// One BuildProfile per shipped stormworks64.exe: the PE fingerprint that
// identifies it and every per-build constant the mod reads. The registry is
// append-only. A game patch adds a profile and never edits one, so a player who
// has not taken the patch keeps matching the old build.

namespace stormworks_ht::builds {

using PeFingerprint = ::cameraunlock::memory::PeFingerprint;

struct OffsetTable {
    // void BuildFrustum(Plane planes[6], const mm_camera* camera, float aspect).
    // Fills the six planes the renderer culls draws against, from the camera
    // fields below and nothing else.
    std::uintptr_t build_frustum_rva;

    // mm_camera fields BuildFrustum reads.
    struct {
        std::size_t position;  // double[3], world
        std::size_t forward;   // float[3], world, unit
        std::size_t up;        // float[3], world, unit
        std::size_t right;     // float[3], world, unit
        std::size_t near_clip; // float
        std::size_t far_clip;  // float
        std::size_t fov_deg;   // float, vertical degrees
        std::size_t bytes;     // end of the last field read
    } camera;
};

struct BuildProfile {
    const char* name;
    PeFingerprint fingerprint;
    OffsetTable offsets;
};

}  // namespace stormworks_ht::builds
