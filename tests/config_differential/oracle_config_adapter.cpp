// Compiled into the config oracle library only, with `cameraunlock` and `stormworks_ht` renamed,
// so "config.h" here is the dev build's (oracle/src/config.h).
#include "config.h"
#include "oracle_adapter.h"

namespace sw_oracle_view {

OracleConfig RunOracle(const std::string& path) {
    stormworks_ht::WriteDefaultConfig(path);
    stormworks_ht::Config c;
    c.Load(path);
    OracleConfig o{};
    o.port = c.port;
    o.enable_on_startup = c.enable_on_startup;
    o.world_space_yaw = c.world_space_yaw;
    o.yaw_sensitivity = c.yaw_sensitivity;
    o.pitch_sensitivity = c.pitch_sensitivity;
    o.roll_sensitivity = c.roll_sensitivity;
    o.invert_yaw = c.invert_yaw;
    o.invert_pitch = c.invert_pitch;
    o.invert_roll = c.invert_roll;
    o.local_smoothing = c.local_smoothing;
    o.remote_smoothing = c.remote_smoothing;
    o.position_enabled = c.position_enabled;
    o.position_sensitivity_x = c.position_sensitivity_x;
    o.position_sensitivity_y = c.position_sensitivity_y;
    o.position_sensitivity_z = c.position_sensitivity_z;
    o.position_limit_x = c.position_limit_x;
    o.position_limit_y = c.position_limit_y;
    o.position_limit_y_down = c.position_limit_y_down;
    o.position_limit_z = c.position_limit_z;
    o.position_limit_z_back = c.position_limit_z_back;
    o.invert_position_x = c.invert_position_x;
    o.invert_position_y = c.invert_position_y;
    o.invert_position_z = c.invert_position_z;
    o.toggle_key = c.toggle_key;
    o.cycle_mode_key = c.cycle_mode_key;
    o.yaw_mode_key = c.yaw_mode_key;
    o.data_freshness_ms = c.data_freshness_ms;
    return o;
}

}  // namespace sw_oracle_view
