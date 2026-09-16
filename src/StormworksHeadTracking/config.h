#pragma once

#include <cstdint>
#include <string>

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace stormworks_ht {

// Parsed StormworksHeadTracking.ini. Defaults match the CameraUnlock doctrine
// configuration table. Missing file or keys fall back to these defaults; that
// is the one boundary where absence is legitimate (no config yet), not a
// swallowed error.
struct Config {
    // [Tracking]
    uint16_t port = 4242;
    bool enable_on_startup = true;
    // true: head yaw turns about the world's up axis (horizon-locked).
    // false: about the camera's own up axis.
    bool world_space_yaw = true;
    float yaw_sensitivity = 1.0f;
    float pitch_sensitivity = 1.0f;
    float roll_sensitivity = 1.0f;
    bool invert_yaw = false;
    bool invert_pitch = false;
    bool invert_roll = false;
    // Smoothing is picked per connection from the packet source address: a
    // tracker on this machine (loopback) uses local_smoothing, a remote network
    // device uses remote_smoothing. Both cover rotation and position.
    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    // [Position]
    bool position_enabled = true;
    float position_sensitivity_x = 1.0f;
    float position_sensitivity_y = 1.0f;
    float position_sensitivity_z = 1.0f;
    float position_limit_x = cameraunlock::PositionSettings{}.limit_x;
    float position_limit_y = cameraunlock::PositionSettings{}.limit_y;
    float position_limit_y_down = cameraunlock::PositionSettings{}.limit_y_down;
    float position_limit_z = cameraunlock::PositionSettings{}.limit_z;
    float position_limit_z_back = cameraunlock::PositionSettings{}.limit_z_back;
    bool invert_position_x = false;
    bool invert_position_y = false;
    bool invert_position_z = false;

    // [Hotkeys] - Windows virtual key codes (nav cluster defaults).
    int toggle_key = 0x23;       // End
    int cycle_mode_key = 0x21;   // Page Up
    int yaw_mode_key = 0x22;     // Page Down

    // [Advanced]
    int data_freshness_ms = 500;

    // Loads from the given INI path. Returns false if the file does not exist
    // (defaults are left in place); true if it was read.
    bool Load(const std::string& path);
};

// Writes a default INI, holding Config's own defaults, to the path if none
// exists. Returns true if it created the file. Used so first-run users get a
// documented config to edit.
bool WriteDefaultConfig(const std::string& path);

}  // namespace stormworks_ht
