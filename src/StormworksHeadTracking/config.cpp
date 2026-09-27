#include "config.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <utility>
#include <vector>

#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"
#include "legacy_config/legacy_config.h"

namespace stormworks_ht {

namespace {

namespace cfg = cameraunlock::config;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;
using cfg::DroppedValue;
using cfg::ImportResult;
using cfg::LegacyFollowsDefaultsIni;
using cfg::LegacyInput;
using cfg::LegacyPoseShaping;
using cfg::PoseShapingValue;
using cfg::schema::Concept;

// A legacy hotkey code and the Ctrl+Shift chord the dev build always registered beside it, as one
// key list: the code's binding (none for a code no hotkey can hold, N1 and N3), then the chord.
std::string KeyList(int vk, char letter, const char* key, std::vector<DroppedValue>& dropped) {
    const std::string code = cfg::LegacyVirtualKeyToBindings(vk, "Hotkeys", key, dropped);
    const std::string chord = cameraunlock::input::FormatKeyBindings(
        std::vector<KeyBinding>{{KeyModifiers::kCtrl | KeyModifiers::kShift, letter}});
    return code.empty() ? chord : code + ", " + chord;
}

// The ANSI path the dev build opened the legacy file by (NarrowPath in its mod.cpp): the wide path
// in the active code page, where a character the code page lacks becomes '?', which names no file.
std::string DevBuildAnsiPath(const std::wstring& wide) {
    int need = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrow(need > 0 ? need - 1 : 0, '\0');
    if (need > 0) {
        WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, &narrow[0], need, nullptr, nullptr);
    }
    return narrow;
}

ImportResult Import(const LegacyInput& input, Config& out) {
    legacy::Config c;
    const legacy::ReadStatus read = legacy::Read(DevBuildAnsiPath(input.path).c_str(), c);
    const legacy::Config shipped;

    std::vector<DroppedValue> dropped;
    std::vector<PoseShapingValue> shaping;

    out.port = c.port;
    out.enable_on_startup = c.enable_on_startup;
    out.world_space_yaw = c.world_space_yaw;
    out.data_freshness_ms = c.data_freshness_ms;

    // [Position] PositionEnabled chose only the startup mode: the cycle key reached every mode
    // either way.
    const cameraunlock::TrackingModeChannels mode = cameraunlock::EncodeTrackingMode(
        c.position_enabled ? cameraunlock::TrackingMode::RotationAndPosition
                           : cameraunlock::TrackingMode::RotationOnly);
    out.rotation_enabled = mode.rotation_enabled;
    out.position_enabled = mode.position_enabled;

    out.local_smoothing = c.local_smoothing;
    out.remote_smoothing = c.remote_smoothing;

    out.position.limit_x = c.position_limit_x;
    out.position.limit_y = c.position_limit_y;
    out.position.limit_y_down = c.position_limit_y_down;
    out.position.limit_z = c.position_limit_z;
    out.position.limit_z_back = c.position_limit_z_back;

    // Every sensitivity and inversion shipped at identity, so nothing folds and a value the player
    // changed is dropped.
    LegacyPoseShaping(c.yaw_sensitivity, shipped.yaw_sensitivity, "Tracking", "YawSensitivity", shaping, dropped);
    LegacyPoseShaping(c.pitch_sensitivity, shipped.pitch_sensitivity, "Tracking", "PitchSensitivity", shaping, dropped);
    LegacyPoseShaping(c.roll_sensitivity, shipped.roll_sensitivity, "Tracking", "RollSensitivity", shaping, dropped);
    LegacyPoseShaping(c.invert_yaw, shipped.invert_yaw, "Tracking", "InvertYaw", shaping, dropped);
    LegacyPoseShaping(c.invert_pitch, shipped.invert_pitch, "Tracking", "InvertPitch", shaping, dropped);
    LegacyPoseShaping(c.invert_roll, shipped.invert_roll, "Tracking", "InvertRoll", shaping, dropped);
    LegacyPoseShaping(c.position_sensitivity_x, shipped.position_sensitivity_x, "Position", "PositionSensitivityX",
                      shaping, dropped);
    LegacyPoseShaping(c.position_sensitivity_y, shipped.position_sensitivity_y, "Position", "PositionSensitivityY",
                      shaping, dropped);
    LegacyPoseShaping(c.position_sensitivity_z, shipped.position_sensitivity_z, "Position", "PositionSensitivityZ",
                      shaping, dropped);
    LegacyPoseShaping(c.invert_position_x, shipped.invert_position_x, "Position", "InvertPositionX", shaping, dropped);
    LegacyPoseShaping(c.invert_position_y, shipped.invert_position_y, "Position", "InvertPositionY", shaping, dropped);
    LegacyPoseShaping(c.invert_position_z, shipped.invert_position_z, "Position", "InvertPositionZ", shaping, dropped);

    out.toggle_key = KeyList(c.toggle_key, 'Y', "ToggleKey", dropped);
    out.cycle_tracking_mode_key = KeyList(c.cycle_mode_key, 'G', "CycleModeKey", dropped);
    out.yaw_mode_key = KeyList(c.yaw_mode_key, 'H', "YawModeKey", dropped);

    // A row still at what the dev build ran on with no file is no player's choice, so it follows
    // Defaults.ini. The chords were fixed in code, so each hotkey's code decides alone.
    LegacyFollowsDefaultsIni follows;
    follows.Setting(Concept::UdpPort, c.port, shipped.port);
    follows.Setting(Concept::EnableOnStartup, c.enable_on_startup, shipped.enable_on_startup);
    follows.Setting(Concept::WorldSpaceYaw, c.world_space_yaw, shipped.world_space_yaw);
    follows.TrackingMode(c.position_enabled, shipped.position_enabled);
    follows.Setting(Concept::DataFreshnessMs, c.data_freshness_ms, shipped.data_freshness_ms);
    follows.Setting(Concept::LocalSmoothing, c.local_smoothing, shipped.local_smoothing);
    follows.Setting(Concept::RemoteSmoothing, c.remote_smoothing, shipped.remote_smoothing);
    follows.Setting(Concept::PositionLimitX, c.position_limit_x, shipped.position_limit_x);
    follows.Setting(Concept::PositionLimitY, c.position_limit_y, shipped.position_limit_y);
    follows.Setting(Concept::PositionLimitYDown, c.position_limit_y_down, shipped.position_limit_y_down);
    follows.Setting(Concept::PositionLimitZ, c.position_limit_z, shipped.position_limit_z);
    follows.Setting(Concept::PositionLimitZBack, c.position_limit_z_back, shipped.position_limit_z_back);
    follows.Setting(Concept::ToggleKey, c.toggle_key, shipped.toggle_key);
    follows.Setting(Concept::CycleTrackingModeKey, c.cycle_mode_key, shipped.cycle_mode_key);
    follows.Setting(Concept::YawModeKey, c.yaw_mode_key, shipped.yaw_mode_key);

    return read == legacy::ReadStatus::Absent
               ? ImportResult::Absent(std::move(dropped), std::move(shaping), follows.Concepts())
               : ImportResult::Imported(std::move(dropped), std::move(shaping), follows.Concepts());
}

}  // namespace

cfg::ConfigTable<Config> MakeConfigTable() {
    cfg::ConfigTable<Config> table{Config{}};
    table.Concept<Concept::UdpPort>(&Config::port)
        .Concept<Concept::EnableOnStartup>(&Config::enable_on_startup)
        .Concept<Concept::WorldSpaceYaw>(&Config::world_space_yaw)
        .Writable()
        .Concept<Concept::RotationEnabled>(&Config::rotation_enabled)
        .Writable()
        .Concept<Concept::DataFreshnessMs>(&Config::data_freshness_ms)
        .Concept<Concept::LocalSmoothing>(&Config::local_smoothing)
        .Concept<Concept::RemoteSmoothing>(&Config::remote_smoothing)
        .Concept<Concept::PositionEnabled>(&Config::position_enabled)
        .Writable()
        .Concept<Concept::PositionLimitX>([](const Config& c) { return c.position.limit_x; },
                                          [](Config& c, float v) { c.position.limit_x = v; })
        .Concept<Concept::PositionLimitY>([](const Config& c) { return c.position.limit_y; },
                                          [](Config& c, float v) { c.position.limit_y = v; })
        .Concept<Concept::PositionLimitYDown>([](const Config& c) { return c.position.limit_y_down; },
                                              [](Config& c, float v) { c.position.limit_y_down = v; })
        .Concept<Concept::PositionLimitZ>([](const Config& c) { return c.position.limit_z; },
                                          [](Config& c, float v) { c.position.limit_z = v; })
        .Concept<Concept::PositionLimitZBack>([](const Config& c) { return c.position.limit_z_back; },
                                              [](Config& c, float v) { c.position.limit_z_back = v; })
        .Concept<Concept::ToggleKey>(&Config::toggle_key)
        .Concept<Concept::CycleTrackingModeKey>(&Config::cycle_tracking_mode_key)
        .Concept<Concept::YawModeKey>(&Config::yaw_mode_key);
    return table;
}

cfg::LegacyImport<Config> MakeLegacyImport() {
    return {&Import, legacy::ReadKeys()};
}

cfg::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder, cfg::DefaultsFile defaults) {
    const auto wide = [](const char* name) { return std::wstring(name, name + std::char_traits<char>::length(name)); };
    cfg::ConfigOwnerOptions<Config> options;
    options.path = folder + wide(kConfigFileName);
    options.legacy_path = folder + wide(kLegacyConfigFileName);
    options.table = MakeConfigTable();
    options.import = MakeLegacyImport();
    options.header.display_name = kConfigDisplayName;
    options.defaults = std::move(defaults);
    return options;
}

}  // namespace stormworks_ht
