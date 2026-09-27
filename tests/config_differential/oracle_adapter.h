#pragma once

// The oracle: the config reader and hotkey registration of the dev pre-release (5516c25), the
// newest published build, compiled from oracle/ with the core sources they included at its pin
// (c480d8a). Two libraries build it, each with its namespaces renamed at compile time so it links
// beside the current core: the reader, and RegisterHotkeys against oracle_fake's recording poller.
// This header names no core type, so the test includes it without the renaming.

#include <array>
#include <string>
#include <vector>

namespace sw_oracle_view {

struct OracleConfig {
    int port;
    bool enable_on_startup;
    bool world_space_yaw;
    float yaw_sensitivity, pitch_sensitivity, roll_sensitivity;
    bool invert_yaw, invert_pitch, invert_roll;
    float local_smoothing, remote_smoothing;
    bool position_enabled;
    float position_sensitivity_x, position_sensitivity_y, position_sensitivity_z;
    float position_limit_x, position_limit_y, position_limit_y_down, position_limit_z, position_limit_z_back;
    bool invert_position_x, invert_position_y, invert_position_z;
    int toggle_key, cycle_mode_key, yaw_mode_key;
    int data_freshness_ms;
};

// What the dev build's HeadTrackingMod::Init ran on for the file at `path`: WriteDefaultConfig,
// then Config::Load from a default Config, whose defaults stand when the file cannot be read. It
// creates the file when there is none, as that build did. The dev build never refused a file.
OracleConfig RunOracle(const std::string& path);

// Which actions a key press fires, for every key a binding can name (0x01-0xFE) under every set
// of held modifiers. Entry (vk - kFirstKey) * kHeldStates + held counts the toggle, cycle and yaw
// mode actions fired, in that order. held: 1 Ctrl, 2 Shift, 4 Alt.
constexpr int kFirstKey = 0x01;
constexpr int kLastKey = 0xFE;
constexpr int kHeldStates = 8;
using FireTable = std::vector<std::array<int, 3>>;

// The dev build's RegisterHotkeys run on the three codes, pressing each key under each held set.
FireTable OracleFires(int toggle_key, int cycle_mode_key, int yaw_mode_key);

}  // namespace sw_oracle_view
