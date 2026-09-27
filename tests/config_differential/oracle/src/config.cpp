#include "config.h"

#include <sys/stat.h>

#include <cctype>
#include <cstdlib>
#include <string>

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/config/value_guards.h"
#include "cameraunlock/logging/file_log.h"

namespace stormworks_ht {

namespace {

constexpr int kMinPort = 1024;
constexpr int kMaxPort = 65535;

namespace guards = cameraunlock::config;

// strtod parses "nan", "inf" and 1e400, none of which any range check catches,
// and a prefix, so "0,15" read as 0. Any of them reaching the pose poisons
// every camera uniform for the session with nothing in the log. The guards
// require the whole value to parse, reject non-finite, clamp to the key's
// range and log what they changed. A finite in-range value comes back
// untouched, so a configured 0.0 stays 0.0.
float ReadFraction(const cameraunlock::IniReader& ini, const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, "Tracking", key, fallback, 0.0f, 1.0f,
                                    cameraunlock::logging::Line);
}

// Negative is a legitimate inversion, so the range is symmetric.
float ReadSensitivity(const cameraunlock::IniReader& ini, const char* section, const char* key,
                      float fallback) {
    return guards::ReadFloatChecked(ini, section, key, fallback, -guards::kMaxSensitivity,
                                    guards::kMaxSensitivity, cameraunlock::logging::Line);
}

// A negative limit inverts the processor's clamp bounds and pins the lean at a
// fixed offset, so zero is the floor.
float ReadPositionLimit(const cameraunlock::IniReader& ini, const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, "Position", key, fallback, 0.0f, guards::kMaxPositionLimit,
                                    cameraunlock::logging::Line);
}

// IniReader::ReadBool compares the whole value, and GetPrivateProfileStringA
// leaves an inline comment attached, so "PositionEnabled=0 ; no lean" reads
// back as the default with nothing in the log. Parse the comment-stripped text
// instead and name a value that is not a boolean at all.
bool ReadBoolChecked(const cameraunlock::IniReader& ini, const char* section, const char* key,
                     bool fallback) {
    const std::string raw = guards::ReadRawValue(ini, section, key);
    if (raw.empty()) return fallback;
    std::string lowered;
    lowered.reserve(raw.size());
    for (char c : raw) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") return true;
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") return false;
    cameraunlock::logging::Line("config: [%s] %s=%s is not 0 or 1, using %d", section, key,
                                raw.c_str(), fallback ? 1 : 0);
    return fallback;
}

// GetAsyncKeyState only defines 0x01..0xFE, and Ctrl / Shift / Alt are what the
// chord guard tests, so any other value registers a hotkey that never fires.
//
// The whole value also has to be 0x-prefixed hex, which is the form this file
// is written in. ReadHex parses a prefix in either base, so a key NAME reads as
// whatever leading hex digits it happens to contain ("End" is 0x0E, an
// unassigned code that can never fire) and "35", End's code written in decimal,
// reads as the digit-5 key, which then toggles tracking whenever the player
// types a 5. Neither can be told apart from a deliberate setting once parsed,
// so both are refused here and named in the log instead.
int ReadHotkey(const cameraunlock::IniReader& ini, const char* key, int fallback) {
    const std::string raw = guards::ReadRawValue(ini, "Hotkeys", key);
    if (raw.empty()) return fallback;
    if (raw.size() > 2 && raw[0] == '0' && (raw[1] == 'x' || raw[1] == 'X')) {
        const char* digits = raw.c_str() + 2;
        char* end = nullptr;
        const long vk = std::strtol(digits, &end, 16);
        if (*end == '\0' && guards::IsBindableVirtualKey(static_cast<int>(vk))) {
            return static_cast<int>(vk);
        }
    }
    cameraunlock::logging::Line(
        "config: [Hotkeys] %s=%s is not a key this mod can watch, using 0x%02X. Keys are "
        "Windows virtual key codes written as 0x followed by hex digits.",
        key, raw.c_str(), static_cast<unsigned>(fallback));
    return fallback;
}

// The old value is deliberately NOT migrated into the new keys. The single
// Smoothing value carried a hidden 0.15 floor, so the number in an existing
// config does not mean what it used to: copying it across would hand a local
// user smoothing they never chose under the new semantics, and copying it into
// only one of the two keys would be a guess about which connection they were on.
void WarnRetiredSmoothingKey(const cameraunlock::IniReader& reader,
                             const char* section, const char* key) {
    if (reader.ReadString(section, key, "").empty()) return;
    cameraunlock::logging::Line(
        "WARNING: Config key [%s] %s has been retired and is IGNORED. Smoothing is "
        "now two keys: LocalSmoothing (default 0, applies to a tracker on this "
        "machine) and RemoteSmoothing (default 0.15, applies to a tracker on the "
        "network). The old value is not migrated because the semantics changed - it "
        "carried a hidden 0.15 floor that no longer exists. Set the two new keys.",
        section, key);
}

}  // namespace

bool Config::Load(const std::string& path) {
    cameraunlock::IniReader ini;
    if (!ini.Open(path)) {
        return false;
    }

    int p = ini.ReadInt("Tracking", "Port", port);
    if (p >= kMinPort && p <= kMaxPort) {
        port = static_cast<uint16_t>(p);
    } else {
        cameraunlock::logging::Line("config: [Tracking] Port %d is outside %d-%d, using %u", p, kMinPort,
                                    kMaxPort, port);
    }
    enable_on_startup = ReadBoolChecked(ini, "Tracking", "EnableOnStartup", enable_on_startup);
    world_space_yaw = ReadBoolChecked(ini, "Tracking", "WorldSpaceYaw", world_space_yaw);
    yaw_sensitivity = ReadSensitivity(ini, "Tracking", "YawSensitivity", yaw_sensitivity);
    pitch_sensitivity = ReadSensitivity(ini, "Tracking", "PitchSensitivity", pitch_sensitivity);
    roll_sensitivity = ReadSensitivity(ini, "Tracking", "RollSensitivity", roll_sensitivity);
    invert_yaw = ReadBoolChecked(ini, "Tracking", "InvertYaw", invert_yaw);
    invert_pitch = ReadBoolChecked(ini, "Tracking", "InvertPitch", invert_pitch);
    invert_roll = ReadBoolChecked(ini, "Tracking", "InvertRoll", invert_roll);
    local_smoothing = ReadFraction(ini, "LocalSmoothing", local_smoothing);
    remote_smoothing = ReadFraction(ini, "RemoteSmoothing", remote_smoothing);

    WarnRetiredSmoothingKey(ini, "Tracking", "Smoothing");

    position_enabled = ReadBoolChecked(ini, "Position", "PositionEnabled", position_enabled);
    position_sensitivity_x = ReadSensitivity(ini, "Position", "PositionSensitivityX", position_sensitivity_x);
    position_sensitivity_y = ReadSensitivity(ini, "Position", "PositionSensitivityY", position_sensitivity_y);
    position_sensitivity_z = ReadSensitivity(ini, "Position", "PositionSensitivityZ", position_sensitivity_z);
    position_limit_x = ReadPositionLimit(ini, "PositionLimitX", position_limit_x);
    position_limit_y = ReadPositionLimit(ini, "PositionLimitY", position_limit_y);
    position_limit_y_down = ReadPositionLimit(ini, "PositionLimitYDown", position_limit_y_down);
    position_limit_z = ReadPositionLimit(ini, "PositionLimitZForward", position_limit_z);
    position_limit_z_back = ReadPositionLimit(ini, "PositionLimitZBack", position_limit_z_back);
    invert_position_x = ReadBoolChecked(ini, "Position", "InvertPositionX", invert_position_x);
    invert_position_y = ReadBoolChecked(ini, "Position", "InvertPositionY", invert_position_y);
    invert_position_z = ReadBoolChecked(ini, "Position", "InvertPositionZ", invert_position_z);
    // No position smoothing key: position uses the same LocalSmoothing /
    // RemoteSmoothing pair as rotation.
    WarnRetiredSmoothingKey(ini, "Position", "PositionSmoothing");

    toggle_key = ReadHotkey(ini, "ToggleKey", toggle_key);
    cycle_mode_key = ReadHotkey(ini, "CycleModeKey", cycle_mode_key);
    yaw_mode_key = ReadHotkey(ini, "YawModeKey", yaw_mode_key);

    // Zero or less would make every packet stale and tracking could never engage.
    const int freshness = ini.ReadInt("Advanced", "DataFreshnessMs", data_freshness_ms);
    if (freshness > 0) {
        data_freshness_ms = freshness;
    } else {
        cameraunlock::logging::Line("config: [Advanced] DataFreshnessMs %d must be above 0, using %d",
                                    freshness, data_freshness_ms);
    }
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
