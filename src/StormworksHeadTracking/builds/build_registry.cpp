#include "build_registry.h"

#include <array>

#include "cameraunlock/logging/file_log.h"

namespace stormworks_ht::builds {

extern const BuildProfile kSteamProfile_20260904;

namespace {
// Newest first: the first entry labels an unmatched build as newer or older.
constexpr std::array<const BuildProfile*, 1> kKnownProfiles = {
    &kSteamProfile_20260904,
};

const BuildProfile* g_active = nullptr;

// A profile whose fingerprint was recorded before its offsets were derived.
bool IsComplete(const BuildProfile& p) {
    return p.offsets.build_frustum_rva != 0;
}
}  // namespace

MatchResult SelectProfile(void* host_base) {
    using cameraunlock::logging::Line;
    PeFingerprint running{};
    if (!cameraunlock::memory::ReadPeFingerprint(host_base, running)) {
        Line("build-check: could not read the game EXE's PE header; frustum hook not installed");
        return MatchResult::ReadFailed;
    }
    Line("build-check: running ts=0x%08x size=0x%08x csum=0x%08x", running.TimeDateStamp,
         running.SizeOfImage, running.CheckSum);

    for (const BuildProfile* p : kKnownProfiles) {
        const bool complete = IsComplete(*p);
        Line("build-check: profile %s ts=0x%08x size=0x%08x csum=0x%08x%s", p->name,
             p->fingerprint.TimeDateStamp, p->fingerprint.SizeOfImage, p->fingerprint.CheckSum,
             complete ? "" : " (offsets not derived yet)");
        if (!running.Matches(p->fingerprint)) continue;
        if (!complete) {
            Line("build-check: matches %s, whose offsets are not derived yet; frustum hook not installed",
                 p->name);
            return MatchResult::HostDiffers;
        }
        g_active = p;
        Line("build-check: matched %s", p->name);
        return MatchResult::Matched;
    }

    switch (cameraunlock::memory::ClassifyMismatch(running, kKnownProfiles.front()->fingerprint)) {
        case cameraunlock::memory::FingerprintMismatch::Newer:
            Line("build-check: this game build is newer than any this mod knows. Head tracking still "
                 "works, but objects at the screen edge can vanish while the head is turned until an "
                 "updated mod adds this build.");
            return MatchResult::HostNewer;
        case cameraunlock::memory::FingerprintMismatch::Older:
            Line("build-check: this game build is older than any this mod knows; let Steam finish "
                 "updating. Head tracking still works, but objects at the screen edge can vanish while "
                 "the head is turned.");
            return MatchResult::HostOlder;
        case cameraunlock::memory::FingerprintMismatch::Differs:
            break;
    }
    Line("build-check: stormworks64.exe has a known build date but a different size or checksum (modified "
         "EXE); frustum hook not installed");
    return MatchResult::HostDiffers;
}

bool HasActiveProfile() {
    return g_active != nullptr;
}

const BuildProfile& ActiveProfile() {
    return *g_active;
}

}  // namespace stormworks_ht::builds
