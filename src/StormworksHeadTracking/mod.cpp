#include "mod.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <cstdint>

#include "cameraunlock/input/chord_hotkeys.h"
#include "cameraunlock/logging/file_log.h"

namespace stormworks_ht {

namespace {
constexpr float kFirstFrameDeltaSeconds = 1.0f / 60.0f;
constexpr float kMaxDeltaSeconds = 0.1f;
constexpr int kHotkeyPollIntervalMs = 16;
constexpr unsigned long long kHeartbeatIntervalMs = 30000;

std::string NarrowPath(const std::wstring& wide) {
    int need = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrow(need > 0 ? need - 1 : 0, '\0');
    if (need > 0) {
        WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, &narrow[0], need, nullptr, nullptr);
    }
    return narrow;
}

const char* YawModeName(bool world_space) {
    return world_space ? "world-space (horizon-locked)" : "camera-local";
}

const char* TrackingModeName(cameraunlock::TrackingMode mode) {
    switch (mode) {
        case cameraunlock::TrackingMode::RotationAndPosition: return "full 6DOF";
        case cameraunlock::TrackingMode::RotationOnly: return "rotation only";
        case cameraunlock::TrackingMode::PositionOnly: return "position only";
    }
    return "unknown";
}
}  // namespace

void HeadTrackingMod::Init(const std::wstring& exe_dir) {
    const std::string ini_path = NarrowPath(exe_dir + L"\\StormworksHeadTracking.ini");
    if (WriteDefaultConfig(ini_path)) {
        cameraunlock::logging::Line("Wrote default config to %s", ini_path.c_str());
    }
    // Both fail on a folder the game cannot write to, or a path with characters
    // outside the ANSI code page, and the player's edits would be ignored unseen.
    if (!m_config.Load(ini_path)) {
        cameraunlock::logging::Line("WARN: could not read or create %s; running on built-in defaults",
                                    ini_path.c_str());
    }

    m_enabled.store(m_config.enable_on_startup);
    m_world_space_yaw.store(m_config.world_space_yaw);

    ConfigurePipeline();
    StartReceiver();
    RegisterHotkeys();

    cameraunlock::logging::Line(
        "Init complete. Tracking %s, mode %s, yaw %s",
        m_enabled.load() ? "enabled" : "disabled",
        TrackingModeName(m_session.GetMode()),
        YawModeName(m_world_space_yaw.load()));
}

void HeadTrackingMod::ConfigurePipeline() {
    cameraunlock::SensitivitySettings sens;
    sens.yaw = m_config.yaw_sensitivity;
    sens.pitch = m_config.pitch_sensitivity;
    sens.roll = m_config.roll_sensitivity;
    sens.invert_yaw = m_config.invert_yaw;
    sens.invert_pitch = m_config.invert_pitch;
    sens.invert_roll = m_config.invert_roll;
    m_session.GetProcessor().SetSensitivity(sens);

    cameraunlock::PositionSettings pos;
    pos.sensitivity_x = m_config.position_sensitivity_x;
    pos.sensitivity_y = m_config.position_sensitivity_y;
    pos.sensitivity_z = m_config.position_sensitivity_z;
    pos.limit_x = m_config.position_limit_x;
    // The clamp is [-limit_y_down, +limit_y], and the two are configured
    // separately so a player can hold a tighter budget for ducking than for
    // standing up.
    pos.limit_y = m_config.position_limit_y;
    pos.limit_y_down = m_config.position_limit_y_down;
    pos.limit_z = m_config.position_limit_z;
    pos.limit_z_back = m_config.position_limit_z_back;
    pos.invert_x = m_config.invert_position_x;
    pos.invert_y = m_config.invert_position_y;
    pos.invert_z = m_config.invert_position_z;
    m_session.GetPositionProcessor().SetSettings(pos);

    // Smoothing goes in after SetSettings, which would otherwise overwrite it.
    // The session feeds both the rotation and the position processor - there is
    // no separate position smoothing setting - and picks between the two values
    // per connection from the receiver's IsRemoteConnection(), re-read on every
    // Update().
    static_assert(decltype(m_session)::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection() or smoothing silently stays local");
    m_session.SetLocalSmoothing(m_config.local_smoothing);
    m_session.SetRemoteSmoothing(m_config.remote_smoothing);

    m_session.SetMode(m_config.position_enabled
                          ? cameraunlock::TrackingMode::RotationAndPosition
                          : cameraunlock::TrackingMode::RotationOnly);
}

void HeadTrackingMod::StartReceiver() {
    m_receiver.SetLog([](const std::string& s) {
        cameraunlock::logging::Line("[udp] %s", s.c_str());
    });
    if (m_receiver.Start(m_config.port)) {
        cameraunlock::logging::Line("Receiver bound to UDP port %u", m_config.port);
    } else {
        cameraunlock::logging::Line(
            "WARN: UDP receiver did not bind immediately on port %u; background retry active",
            m_config.port);
    }
}

void HeadTrackingMod::RegisterHotkeys() {
    using cameraunlock::input::ChordGuarded;
    using cameraunlock::input::NavGuarded;
    auto toggle = [this]() { ToggleEnabled(); };
    auto cycle = [this]() { CycleMode(); };
    auto yaw_mode = [this]() { ToggleYawMode(); };

    m_hotkeys.AddHotkey(m_config.toggle_key, NavGuarded(toggle));
    m_hotkeys.AddHotkey(m_config.cycle_mode_key, NavGuarded(cycle));
    m_hotkeys.AddHotkey(m_config.yaw_mode_key, NavGuarded(yaw_mode));
    m_hotkeys.AddHotkey('Y', ChordGuarded(toggle));
    m_hotkeys.AddHotkey('G', ChordGuarded(cycle));
    m_hotkeys.AddHotkey('H', ChordGuarded(yaw_mode));
    m_hotkeys.Start(kHotkeyPollIntervalMs);
}

float HeadTrackingMod::ComputeDeltaTime() {
    if (m_qpc_freq == 0) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        m_qpc_freq = f.QuadPart;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (!m_qpc_primed) {
        m_qpc_primed = true;
        m_qpc_last = now.QuadPart;
        return kFirstFrameDeltaSeconds;
    }
    float dt = static_cast<float>(now.QuadPart - m_qpc_last) / static_cast<float>(m_qpc_freq);
    m_qpc_last = now.QuadPart;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > kMaxDeltaSeconds) dt = kMaxDeltaSeconds;
    return dt;
}

bool HeadTrackingMod::IsPoseFresh() const {
    const std::int64_t lastUs = m_receiver.GetLastReceiveTimestamp();
    if (lastUs == 0) {
        return false;
    }
    const std::int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return (nowUs - lastUs) / 1000 < m_config.data_freshness_ms;
}

void HeadTrackingMod::OnFrameTick() {
    float dt = ComputeDeltaTime();
    ApplyPendingModeCycles();
    const bool fresh = IsPoseFresh();
    if (fresh != m_pose_fresh) {
        m_pose_fresh = fresh;
        if (fresh) {
            cameraunlock::logging::Line("Tracker data receiving (%s connection)",
                                        m_receiver.IsRemoteConnection() ? "remote" : "local");
        } else {
            cameraunlock::logging::Line("Tracker data lost (no packet for %d ms); holding the last pose",
                                        m_config.data_freshness_ms);
        }
    }
    // The pose is held, never dropped, when packets stop: cutting injection
    // snaps the view to the game camera mid-turn and snaps back on the next
    // packet. The receiver keeps reporting its last pose, so the pipeline runs
    // on a constant input and a resumed tracker blends in through the smoothing.
    // Update() returns false only while no packet has ever arrived.
    const bool have_data = m_session.Update(dt);
    m_inject = m_enabled.load() && have_data;
    if (m_inject) {
        m_session.GetRotation(m_pose.yaw, m_pose.pitch, m_pose.roll);
        m_session.GetPositionOffset(m_pose.x, m_pose.y, m_pose.z);
        m_frame_world_space_yaw = m_world_space_yaw.load();
    }
    LogHeartbeat();
}

// The one line that separates "tracker sending nothing" from "pose arriving but
// not injected". Time-gated rather than tick-gated because a tick count runs at
// the player's frame rate - 600 frames is 10s at 60Hz but 4s at 144Hz.
void HeadTrackingMod::LogHeartbeat() {
    const unsigned long long now_ms = GetTickCount64();
    if (now_ms - m_last_heartbeat_ms < kHeartbeatIntervalMs) return;
    m_last_heartbeat_ms = now_ms;
    HeadPose live;
    m_session.GetRotation(live.yaw, live.pitch, live.roll);
    m_session.GetPositionOffset(live.x, live.y, live.z);
    cameraunlock::logging::Line(
        "pose yaw=%.1f pitch=%.1f roll=%.1f pos=(%.3f,%.3f,%.3f) enabled=%d inject=%d",
        live.yaw, live.pitch, live.roll, live.x, live.y, live.z, m_enabled.load() ? 1 : 0,
        m_inject ? 1 : 0);
}

void HeadTrackingMod::BuildViewDelta(const double* clean_view, double zoom_factor, double* out) const {
    BuildHeadDelta(m_pose, m_frame_world_space_yaw, clean_view, zoom_factor, out);
}

void HeadTrackingMod::ToggleEnabled() {
    bool now = !m_enabled.load();
    m_enabled.store(now);
    cameraunlock::logging::Line("Tracking %s", now ? "enabled" : "disabled");
}

void HeadTrackingMod::ToggleYawMode() {
    bool now = !m_world_space_yaw.load();
    m_world_space_yaw.store(now);
    cameraunlock::logging::Line("Yaw mode: %s", YawModeName(now));
}

void HeadTrackingMod::CycleMode() {
    m_pending_mode_cycles.fetch_add(1);
}

// SetMode resets the position processor's smoothing and the position
// interpolator, both plain floats the render thread reads and writes inside
// Update(). Applying the cycle here keeps every write to that state on this
// thread; the hotkey thread only ever bumps the counter.
void HeadTrackingMod::ApplyPendingModeCycles() {
    int pending = m_pending_mode_cycles.exchange(0);
    while (pending-- > 0) {
        cameraunlock::logging::Line("Mode: %s", TrackingModeName(m_session.CycleMode()));
    }
}

}  // namespace stormworks_ht
