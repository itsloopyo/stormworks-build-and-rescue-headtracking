#include "config.h"

#include <sys/stat.h>

#include <string>

#include "cameraunlock/config/ini_reader.h"
#include "legacy_config/legacy_config.h"

namespace stormworks_ht {

bool Config::Load(const std::string& path) {
    legacy::Config c;
    if (legacy::Read(path.c_str(), c) == legacy::ReadStatus::Absent) {
        return false;
    }
    port = c.port;
    enable_on_startup = c.enable_on_startup;
    world_space_yaw = c.world_space_yaw;
    yaw_sensitivity = c.yaw_sensitivity;
    pitch_sensitivity = c.pitch_sensitivity;
    roll_sensitivity = c.roll_sensitivity;
    invert_yaw = c.invert_yaw;
    invert_pitch = c.invert_pitch;
    invert_roll = c.invert_roll;
    local_smoothing = c.local_smoothing;
    remote_smoothing = c.remote_smoothing;
    position_enabled = c.position_enabled;
    position_sensitivity_x = c.position_sensitivity_x;
    position_sensitivity_y = c.position_sensitivity_y;
    position_sensitivity_z = c.position_sensitivity_z;
    position_limit_x = c.position_limit_x;
    position_limit_y = c.position_limit_y;
    position_limit_y_down = c.position_limit_y_down;
    position_limit_z = c.position_limit_z;
    position_limit_z_back = c.position_limit_z_back;
    invert_position_x = c.invert_position_x;
    invert_position_y = c.invert_position_y;
    invert_position_z = c.invert_position_z;
    toggle_key = c.toggle_key;
    cycle_mode_key = c.cycle_mode_key;
    yaw_mode_key = c.yaw_mode_key;
    data_freshness_ms = c.data_freshness_ms;
    return true;
}

bool WriteDefaultConfig(const std::string& path) {
    struct _stat st;
    if (_stat(path.c_str(), &st) == 0) {
        return false;  // already exists
    }

    cameraunlock::IniWriter w;
    if (!w.Open(path)) {
        return false;
    }

    const Config d;

    w.WriteComment(" Stormworks Head Tracking configuration");
    w.WriteComment(" Decoupled look (head moves the view; mouse/keyboard still aim).");
    w.WriteBlankLine();

    w.WriteSection("Tracking");
    w.WriteInt("Port", d.port);
    w.WriteBool("EnableOnStartup", d.enable_on_startup);
    w.WriteComment(" Yaw mode: 1 = horizon-locked yaw (default), 0 = camera-local");
    w.WriteBool("WorldSpaceYaw", d.world_space_yaw);
    w.WriteDouble("YawSensitivity", d.yaw_sensitivity);
    w.WriteDouble("PitchSensitivity", d.pitch_sensitivity);
    w.WriteDouble("RollSensitivity", d.roll_sensitivity);
    w.WriteBool("InvertYaw", d.invert_yaw);
    w.WriteBool("InvertPitch", d.invert_pitch);
    w.WriteBool("InvertRoll", d.invert_roll);
    w.WriteComment(" Smoothing 0.0 = lightest, 1.0 = heaviest. Covers rotation and position.");
    w.WriteComment(" The value is picked per connection from the packet source address:");
    w.WriteComment(" LocalSmoothing for a tracker sending to 127.0.0.1 on this PC,");
    w.WriteComment(" RemoteSmoothing for anything else, including a tracker on this PC");
    w.WriteComment(" that sends to this machine's LAN address instead of 127.0.0.1.");
    w.WriteDouble("LocalSmoothing", d.local_smoothing);
    w.WriteDouble("RemoteSmoothing", d.remote_smoothing);
    w.WriteBlankLine();

    w.WriteSection("Position");
    w.WriteBool("PositionEnabled", d.position_enabled);
    w.WriteDouble("PositionSensitivityX", d.position_sensitivity_x);
    w.WriteDouble("PositionSensitivityY", d.position_sensitivity_y);
    w.WriteDouble("PositionSensitivityZ", d.position_sensitivity_z);
    w.WriteDouble("PositionLimitX", d.position_limit_x);
    w.WriteComment(" How far the view may move, in metres. Y is up and YDown is down.");
    w.WriteDouble("PositionLimitY", d.position_limit_y);
    w.WriteDouble("PositionLimitYDown", d.position_limit_y_down);
    w.WriteDouble("PositionLimitZForward", d.position_limit_z);
    w.WriteDouble("PositionLimitZBack", d.position_limit_z_back);
    w.WriteBool("InvertPositionX", d.invert_position_x);
    w.WriteBool("InvertPositionY", d.invert_position_y);
    w.WriteBool("InvertPositionZ", d.invert_position_z);
    w.WriteBlankLine();

    w.WriteSection("Hotkeys");
    w.WriteComment(" Windows virtual key codes. End=toggle PageUp=cycle mode PageDown=yaw mode.");
    w.WriteComment(" Chord alternatives Ctrl+Shift+Y / G / H are always active too.");
    w.WriteHex("ToggleKey", d.toggle_key);
    w.WriteHex("CycleModeKey", d.cycle_mode_key);
    w.WriteHex("YawModeKey", d.yaw_mode_key);
    w.WriteBlankLine();

    w.WriteSection("Advanced");
    w.WriteInt("DataFreshnessMs", d.data_freshness_ms);
    w.Close();
    return true;
}

}  // namespace stormworks_ht
