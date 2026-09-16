#pragma once

#include "build_profile.h"

namespace stormworks_ht::builds {

enum class MatchResult {
    Matched,
    ReadFailed,   // the PE header could not be read
    HostNewer,    // TimeDateStamp later than the newest known build
    HostOlder,    // TimeDateStamp earlier than the newest known build
    HostDiffers,  // same TimeDateStamp, different size or checksum
};

// Fingerprints the module at host_base against every known profile and, on a
// match, makes that profile active. Logs the running fingerprint, each profile
// compared, and the outcome in words. Nothing may touch game memory unless this
// returned Matched.
MatchResult SelectProfile(void* host_base);

bool HasActiveProfile();
const BuildProfile& ActiveProfile();

}  // namespace stormworks_ht::builds
