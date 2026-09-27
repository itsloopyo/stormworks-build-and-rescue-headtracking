#pragma once

#include <cstdint>
#include <string>

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/config_table.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace stormworks_ht {

constexpr const char* kConfigFileName = "CameraUnlock.ini";
// The file the dev pre-release read, beside kConfigFileName. Imported once while kConfigFileName
// is absent, and never written.
constexpr const char* kLegacyConfigFileName = "StormworksHeadTracking.ini";
// The game's name as cameraunlock-core's data/games.json spells it.
constexpr const char* kConfigDisplayName = "Stormworks: Build and Rescue";

struct Config {
    uint16_t port = 4242;
    bool enable_on_startup = true;
    // true: head yaw turns about the world's up axis (horizon-locked).
    // false: about the camera's own up axis. The yaw mode hotkey saves it.
    bool world_space_yaw = true;
    // The startup tracking mode. The mode hotkey saves both.
    bool rotation_enabled = true;
    bool position_enabled = true;
    int data_freshness_ms = 500;

    // Smoothing is picked per connection from the packet source address: a
    // tracker on this machine (loopback) uses local_smoothing, a remote network
    // device uses remote_smoothing. Both cover rotation and position.
    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    // The limits; the sensitivities and inversions stay at identity; the tracker shapes the pose.
    cameraunlock::PositionSettings position;

    std::string toggle_key = "End, Ctrl+Shift+Y";
    std::string cycle_tracking_mode_key = "PageUp, Ctrl+Shift+G";
    std::string yaw_mode_key = "PageDown, Ctrl+Shift+H";
};

// The rows of CameraUnlock.ini. The tracking mode pair and WorldSpaceYaw are Writable: their
// hotkeys save the player's choice, and End changes the session only.
cameraunlock::config::ConfigTable<Config> MakeConfigTable();

// StormworksHeadTracking.ini as the dev build read it (legacy_config/), mapped into Config.
cameraunlock::config::LegacyImport<Config> MakeLegacyImport();

// The owner's options for the files in `folder` (with its trailing separator): the settings in
// CameraUnlock.ini, imported once from StormworksHeadTracking.ini. The mod passes
// DefaultsFile::PerUser() and a test a scratch file.
cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults);

}  // namespace stormworks_ht
