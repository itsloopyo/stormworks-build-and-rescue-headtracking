#pragma once

// The config reader of the dev pre-release (5516c25), the last build that read
// StormworksHeadTracking.ini, frozen so a player updating from it is converted exactly as that
// build read the file. Nothing in this folder is ever edited. Three things differ from the reader
// it was taken from: it fills this frozen copy of that build's Config and defaults rather than the
// runtime type, it never writes the file (a missing file reads as the defaults, which is what the
// old reader read from the file it created there), and it reports an absent file apart from one
// it read. The core default values the old Config took from PositionSettings and
// smoothing_utils.h are written out here as numbers, so a later core cannot move what an old file
// converts to.

#include <cstdint>
#include <vector>

#include "cameraunlock/config/legacy_import.h"

namespace stormworks_ht::legacy {

enum class ReadStatus {
    Read,
    // No file at the path, or none the old reader could open. Config holds the defaults.
    Absent,
};

struct Config {
    // [Tracking]
    uint16_t port = 4242;
    bool enable_on_startup = true;
    bool world_space_yaw = true;
    float yaw_sensitivity = 1.0f;
    float pitch_sensitivity = 1.0f;
    float roll_sensitivity = 1.0f;
    bool invert_yaw = false;
    bool invert_pitch = false;
    bool invert_roll = false;
    float local_smoothing = 0.0f;
    float remote_smoothing = 0.15f;

    // [Position]
    bool position_enabled = true;
    float position_sensitivity_x = 1.0f;
    float position_sensitivity_y = 1.0f;
    float position_sensitivity_z = 1.0f;
    float position_limit_x = 0.30f;
    float position_limit_y = 0.20f;
    float position_limit_y_down = 0.20f;
    float position_limit_z = 0.40f;
    float position_limit_z_back = 0.10f;
    bool invert_position_x = false;
    bool invert_position_y = false;
    bool invert_position_z = false;

    // [Hotkeys]
    int toggle_key = 0x23;       // End
    int cycle_mode_key = 0x21;   // Page Up
    int yaw_mode_key = 0x22;     // Page Down

    // [Advanced]
    int data_freshness_ms = 500;
};

// Reads the file at `path`, the ANSI path the dev build opened it by, into a default-constructed
// `c`.
ReadStatus Read(const char* path, Config& c);

// Every section and key Read reads, in the order it reads them.
std::vector<cameraunlock::config::LegacyKey> ReadKeys();

}  // namespace stormworks_ht::legacy
