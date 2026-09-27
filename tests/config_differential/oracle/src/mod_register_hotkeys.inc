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
