#pragma once

#include <atomic>
#include <string>

#include "cameraunlock/input/hotkey_poller.h"
#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/tracking/head_tracking_session.h"
#include "config.h"
#include "view_rewrite.h"

namespace stormworks_ht {

// Owns the full head-tracking runtime: OpenTrack UDP receiver, the shared
// processing pipeline (HeadTrackingSession), hotkeys, and the eye-space delta
// the GL proxy premultiplies onto the game's camera view matrices.
//
// Threading: OnFrameTick()/IsInjecting()/BuildViewDelta() run on the render
// (GL) thread. ToggleEnabled()/CycleMode()/ToggleYawMode() fire from the hotkey
// poller thread and write atomics only; the mode change itself is applied at
// the top of the next frame, because HeadTrackingSession::SetMode resets the
// position processor's plain-float smoothing state and the render thread is
// inside that same state for most of a frame.
class HeadTrackingMod {
public:
    HeadTrackingMod() : m_session(m_receiver) {}

    // Loads config, configures the pipeline, starts the receiver and hotkeys.
    void Init(const std::wstring& exe_dir);

    // Called once per presented frame, before its first draw. Advances the
    // pipeline and snapshots the pose, so every pass of a frame sees one pose.
    void OnFrameTick();

    // False when nothing should be injected this frame.
    bool IsInjecting() const { return m_inject; }

    // Column-major view-space delta H for this frame's pose: a tracked view is
    // H * V. World-space yaw turns about the world's up axis as the clean view
    // sees it, which is why the delta needs that view. zoom_factor is
    // tan(fov/2) of this frame over the un-zoomed one (1 when not zoomed); yaw,
    // pitch and position shrink by it so the head moves the picture as far as it
    // would un-zoomed. Roll rotates the picture the same at any FOV and is left alone.
    void BuildViewDelta(const double* clean_view, double zoom_factor, double* out) const;

    void ToggleEnabled();
    void CycleMode();
    void ToggleYawMode();

private:
    void ConfigurePipeline();
    void StartReceiver();
    void RegisterHotkeys();

    float ComputeDeltaTime();

    // True while the newest packet is younger than Config::data_freshness_ms.
    bool IsPoseFresh() const;

    void ApplyPendingModeCycles();

    void LogHeartbeat();

    cameraunlock::UdpReceiver m_receiver;
    cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver> m_session;
    cameraunlock::input::HotkeyPoller m_hotkeys;

    Config m_config;
    std::atomic<bool> m_enabled{true};
    std::atomic<bool> m_world_space_yaw{true};
    std::atomic<int> m_pending_mode_cycles{0};

    bool m_pose_fresh = false;

    // This frame's snapshot, taken in OnFrameTick.
    bool m_inject = false;
    bool m_frame_world_space_yaw = true;
    HeadPose m_pose;

    long long m_qpc_freq = 0;
    long long m_qpc_last = 0;
    bool m_qpc_primed = false;
    unsigned long long m_last_heartbeat_ms = 0;
};

}  // namespace stormworks_ht
