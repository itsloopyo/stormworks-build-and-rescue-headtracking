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
#include <stdexcept>
#include <utility>

#include "cameraunlock/input/key_binding_registration.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/tracking/tracking_mode.h"

namespace stormworks_ht {

namespace {
constexpr float kFirstFrameDeltaSeconds = 1.0f / 60.0f;
constexpr float kMaxDeltaSeconds = 0.1f;
constexpr int kHotkeyPollIntervalMs = 16;
constexpr unsigned long long kHeartbeatIntervalMs = 30000;

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

// The table read every list through the hotkey codec, so a list that does not parse here is a
// bug, not a player's typo.
void Register(cameraunlock::input::HotkeyPoller& poller, const std::string& list, const char* key,
              std::function<void()> action) {
    const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(list);
    if (!parsed.ok()) {
        throw std::logic_error(std::string("[Hotkeys] ") + key + "=" + list + " does not parse: " + parsed.error);
    }
    cameraunlock::input::RegisterKeyBindings(poller, parsed.bindings, std::move(action));
}
}  // namespace

void HeadTrackingMod::Init(const std::wstring& exe_dir) {
    LoadConfig(exe_dir);

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

// Reads CameraUnlock.ini beside the game's exe, importing StormworksHeadTracking.ini once while
// it is absent.
void HeadTrackingMod::LoadConfig(const std::wstring& exe_dir) {
    cameraunlock::config::ConfigOwnerOptions<Config> options =
        MakeConfigOwnerOptions(exe_dir + L"\\", cameraunlock::config::DefaultsFile::PerUser());
    // The mod has no overlay, so the player's one-line messages (an import that did not run,
    // Defaults.ini that cannot be read, a save that failed) go to the log, the only place they
    // can be seen.
    options.status_sink = [](const std::string& message) {
        cameraunlock::logging::Line("Config: %s", message.c_str());
    };
    m_owner = std::make_unique<cameraunlock::config::ConfigOwner<Config>>(std::move(options));
    const cameraunlock::config::ConfigLoadResult<Config> loaded = m_owner->Load();
    for (const std::string& line : loaded.log) cameraunlock::logging::Line("Config: %s", line.c_str());
    cameraunlock::logging::Line("Config: %s %s", kConfigFileName,
                                cameraunlock::config::ConfigLoadStatusName(loaded.status));
    m_config = loaded.config;
    cameraunlock::logging::Line(
        "Config: port=%u enabled=%d yaw=%s mode=(rotation %d, position %d) freshness=%dms "
        "smoothing=(local %.2f, remote %.2f) limits=(x %.2f, y %.2f/%.2f, z %.2f/%.2f)",
        m_config.port, m_config.enable_on_startup ? 1 : 0, YawModeName(m_config.world_space_yaw),
        m_config.rotation_enabled ? 1 : 0, m_config.position_enabled ? 1 : 0, m_config.data_freshness_ms,
        m_config.local_smoothing, m_config.remote_smoothing, m_config.position.limit_x,
        m_config.position.limit_y, m_config.position.limit_y_down, m_config.position.limit_z,
        m_config.position.limit_z_back);
}

void HeadTrackingMod::SaveConfig(const char* what, std::function<void(Config&)> change) {
    const cameraunlock::config::ConfigSaveResult saved = m_owner->Save(std::move(change));
    for (const std::string& line : saved.log) cameraunlock::logging::Line("Config: %s", line.c_str());
    if (saved.status != cameraunlock::config::ConfigSaveStatus::Saved) {
        cameraunlock::logging::Line("Config: %s not saved (%s)", what,
                                    cameraunlock::config::ConfigSaveStatusName(saved.status));
    }
}

void HeadTrackingMod::ConfigurePipeline() {
    // The limits come from the config; the sensitivities and inversions stay at
    // PositionSettings' identity, because the tracker shapes the pose.
    m_session.GetPositionProcessor().SetSettings(m_config.position);

    // Smoothing goes in after SetSettings, which would otherwise overwrite it.
    // The session feeds both the rotation and the position processor - there is
    // no separate position smoothing setting - and picks between the two values
    // per connection from the receiver's IsRemoteConnection(), re-read on every
    // Update().
    static_assert(decltype(m_session)::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection() or smoothing silently stays local");
    m_session.SetLocalSmoothing(m_config.local_smoothing);
    m_session.SetRemoteSmoothing(m_config.remote_smoothing);

    // The table reads a pair that names no mode as its defaults, so the pair always decodes.
    const cameraunlock::TrackingMode mode =
        cameraunlock::DecodeTrackingMode(m_config.rotation_enabled, m_config.position_enabled).value();
    m_session.SetMode(mode);
    m_applied_mode.store(static_cast<int>(mode));
    m_desired_mode.store(static_cast<int>(mode));
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
    // A binding without modifiers does not fire while Ctrl and Shift are both
    // held, so a Ctrl+Shift+<nav> press cannot fire an action through both its
    // nav key and its chord.
    Register(m_hotkeys, m_config.toggle_key, "ToggleKey", [this]() { ToggleEnabled(); });
    Register(m_hotkeys, m_config.cycle_tracking_mode_key, "CycleTrackingModeKey", [this]() { CycleMode(); });
    Register(m_hotkeys, m_config.yaw_mode_key, "YawModeKey", [this]() { ToggleYawMode(); });
    m_hotkeys.Start(kHotkeyPollIntervalMs);
    cameraunlock::logging::Line("Hotkeys: toggle=%s, cycle mode=%s, yaw mode=%s",
                                m_config.toggle_key.c_str(), m_config.cycle_tracking_mode_key.c_str(),
                                m_config.yaw_mode_key.c_str());
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
    ApplyDesiredMode();
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

// End changes the session only and never saves.
void HeadTrackingMod::ToggleEnabled() {
    bool now = !m_enabled.load();
    m_enabled.store(now);
    cameraunlock::logging::Line("Tracking %s", now ? "enabled" : "disabled");
}

void HeadTrackingMod::ToggleYawMode() {
    bool now = !m_world_space_yaw.load();
    m_world_space_yaw.store(now);
    cameraunlock::logging::Line("Yaw mode: %s", YawModeName(now));
    SaveConfig("yaw mode", [now](Config& c) { c.world_space_yaw = now; });
}

// The next mode is computed from the one the render thread last applied, so two
// presses inside one frame move one step, and saved here, off the render thread.
void HeadTrackingMod::CycleMode() {
    const auto next = static_cast<cameraunlock::TrackingMode>((m_applied_mode.load() + 1) % 3);
    m_desired_mode.store(static_cast<int>(next));
    const cameraunlock::TrackingModeChannels mode = cameraunlock::EncodeTrackingMode(next);
    SaveConfig("tracking mode", [mode](Config& c) {
        c.rotation_enabled = mode.rotation_enabled;
        c.position_enabled = mode.position_enabled;
    });
}

// SetMode resets the position processor's smoothing and the position
// interpolator, both plain floats the render thread reads and writes inside
// Update(). Applying the mode here keeps every write to that state on this
// thread; the hotkey thread only ever stores the desired mode.
void HeadTrackingMod::ApplyDesiredMode() {
    const int desired = m_desired_mode.load();
    if (desired == m_applied_mode.load()) return;
    const auto mode = static_cast<cameraunlock::TrackingMode>(desired);
    m_session.SetMode(mode);
    m_applied_mode.store(desired);
    cameraunlock::logging::Line("Mode: %s", TrackingModeName(mode));
}

}  // namespace stormworks_ht
