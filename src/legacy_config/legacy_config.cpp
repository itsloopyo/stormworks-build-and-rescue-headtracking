#include "legacy_config/legacy_config.h"

#include <cctype>
#include <cstdlib>
#include <string>

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/config/value_guards.h"
#include "cameraunlock/logging/file_log.h"

namespace stormworks_ht::legacy {

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

ReadStatus Read(const char* path, Config& c) {
    cameraunlock::IniReader ini;
    if (!ini.Open(path)) {
        return ReadStatus::Absent;
    }

    int p = ini.ReadInt("Tracking", "Port", c.port);
    if (p >= kMinPort && p <= kMaxPort) {
        c.port = static_cast<uint16_t>(p);
    } else {
        cameraunlock::logging::Line("config: [Tracking] Port %d is outside %d-%d, using %u", p, kMinPort,
                                    kMaxPort, c.port);
    }
    c.enable_on_startup = ReadBoolChecked(ini, "Tracking", "EnableOnStartup", c.enable_on_startup);
    c.world_space_yaw = ReadBoolChecked(ini, "Tracking", "WorldSpaceYaw", c.world_space_yaw);
    c.yaw_sensitivity = ReadSensitivity(ini, "Tracking", "YawSensitivity", c.yaw_sensitivity);
    c.pitch_sensitivity = ReadSensitivity(ini, "Tracking", "PitchSensitivity", c.pitch_sensitivity);
    c.roll_sensitivity = ReadSensitivity(ini, "Tracking", "RollSensitivity", c.roll_sensitivity);
    c.invert_yaw = ReadBoolChecked(ini, "Tracking", "InvertYaw", c.invert_yaw);
    c.invert_pitch = ReadBoolChecked(ini, "Tracking", "InvertPitch", c.invert_pitch);
    c.invert_roll = ReadBoolChecked(ini, "Tracking", "InvertRoll", c.invert_roll);
    c.local_smoothing = ReadFraction(ini, "LocalSmoothing", c.local_smoothing);
    c.remote_smoothing = ReadFraction(ini, "RemoteSmoothing", c.remote_smoothing);

    WarnRetiredSmoothingKey(ini, "Tracking", "Smoothing");

    c.position_enabled = ReadBoolChecked(ini, "Position", "PositionEnabled", c.position_enabled);
    c.position_sensitivity_x = ReadSensitivity(ini, "Position", "PositionSensitivityX", c.position_sensitivity_x);
    c.position_sensitivity_y = ReadSensitivity(ini, "Position", "PositionSensitivityY", c.position_sensitivity_y);
    c.position_sensitivity_z = ReadSensitivity(ini, "Position", "PositionSensitivityZ", c.position_sensitivity_z);
    c.position_limit_x = ReadPositionLimit(ini, "PositionLimitX", c.position_limit_x);
    c.position_limit_y = ReadPositionLimit(ini, "PositionLimitY", c.position_limit_y);
    c.position_limit_y_down = ReadPositionLimit(ini, "PositionLimitYDown", c.position_limit_y_down);
    c.position_limit_z = ReadPositionLimit(ini, "PositionLimitZForward", c.position_limit_z);
    c.position_limit_z_back = ReadPositionLimit(ini, "PositionLimitZBack", c.position_limit_z_back);
    c.invert_position_x = ReadBoolChecked(ini, "Position", "InvertPositionX", c.invert_position_x);
    c.invert_position_y = ReadBoolChecked(ini, "Position", "InvertPositionY", c.invert_position_y);
    c.invert_position_z = ReadBoolChecked(ini, "Position", "InvertPositionZ", c.invert_position_z);
    // No position smoothing key: position uses the same LocalSmoothing /
    // RemoteSmoothing pair as rotation.
    WarnRetiredSmoothingKey(ini, "Position", "PositionSmoothing");

    c.toggle_key = ReadHotkey(ini, "ToggleKey", c.toggle_key);
    c.cycle_mode_key = ReadHotkey(ini, "CycleModeKey", c.cycle_mode_key);
    c.yaw_mode_key = ReadHotkey(ini, "YawModeKey", c.yaw_mode_key);

    // Zero or less would make every packet stale and tracking could never engage.
    const int freshness = ini.ReadInt("Advanced", "DataFreshnessMs", c.data_freshness_ms);
    if (freshness > 0) {
        c.data_freshness_ms = freshness;
    } else {
        cameraunlock::logging::Line("config: [Advanced] DataFreshnessMs %d must be above 0, using %d",
                                    freshness, c.data_freshness_ms);
    }
    return ReadStatus::Read;
}

std::vector<cameraunlock::config::LegacyKey> ReadKeys() {
    return {
        {"Tracking", "Port"},
        {"Tracking", "EnableOnStartup"},
        {"Tracking", "WorldSpaceYaw"},
        {"Tracking", "YawSensitivity"},
        {"Tracking", "PitchSensitivity"},
        {"Tracking", "RollSensitivity"},
        {"Tracking", "InvertYaw"},
        {"Tracking", "InvertPitch"},
        {"Tracking", "InvertRoll"},
        {"Tracking", "LocalSmoothing"},
        {"Tracking", "RemoteSmoothing"},
        {"Tracking", "Smoothing"},
        {"Position", "PositionEnabled"},
        {"Position", "PositionSensitivityX"},
        {"Position", "PositionSensitivityY"},
        {"Position", "PositionSensitivityZ"},
        {"Position", "PositionLimitX"},
        {"Position", "PositionLimitY"},
        {"Position", "PositionLimitYDown"},
        {"Position", "PositionLimitZForward"},
        {"Position", "PositionLimitZBack"},
        {"Position", "InvertPositionX"},
        {"Position", "InvertPositionY"},
        {"Position", "InvertPositionZ"},
        {"Position", "PositionSmoothing"},
        {"Hotkeys", "ToggleKey"},
        {"Hotkeys", "CycleModeKey"},
        {"Hotkeys", "YawModeKey"},
        {"Advanced", "DataFreshnessMs"},
    };
}

}  // namespace stormworks_ht::legacy
