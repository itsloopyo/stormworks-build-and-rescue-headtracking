#include "build_profile.h"

// Steam Win64 builds of stormworks64.exe. Append a new profile for a new build;
// never edit or remove one below.

namespace stormworks_ht::builds {

extern const BuildProfile kSteamProfile_20260904;

// v1.15.23, Steam buildid 25122525, PE TimeDateStamp 2026-09-04.
//
// BuildFrustum is called with the camera in RDX and the viewport aspect in
// XMM2. It takes the vertical half-angle as tan(fov_deg * pi/360), scales the
// side planes by it (up) and by it times aspect (right), and places the near and
// far planes along forward at near_clip and far_clip from position.
const BuildProfile kSteamProfile_20260904 = {
    /* name        */ "steam-win64-20260904",
    /* fingerprint */ {0x6a9a9e20u, 0x00d8b000u, 0x00000000u},
    /* offsets     */
    {
        /* build_frustum_rva */ 0x2f8060,
        /* camera */
        {
            /* position  */ 0x68,
            /* forward   */ 0x248,
            /* up        */ 0x254,
            /* right     */ 0x260,
            /* near_clip */ 0x26c,
            /* far_clip  */ 0x270,
            /* fov_deg   */ 0x278,
            /* bytes     */ 0x27c,
        },
    },
};

}  // namespace stormworks_ht::builds
