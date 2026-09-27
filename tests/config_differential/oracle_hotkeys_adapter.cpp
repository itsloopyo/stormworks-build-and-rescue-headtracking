// Compiled into the hotkey oracle library only, with `cameraunlock` and `stormworks_ht` renamed.
// The dev build registered its hotkeys in HeadTrackingMod::RegisterHotkeys, in a mod.cpp that also
// starts the receiver and the tracking session, so that one function is copied verbatim into
// oracle/src/mod_register_hotkeys.inc and compiled here into a HeadTrackingMod holding only the
// members it uses, over oracle_fake's recording poller.
#include "cameraunlock/input/hotkey_poller.h"
#include "cameraunlock/input/chord_hotkeys.h"
#include "config.h"
#include "oracle_adapter.h"

namespace stormworks_ht {

namespace {
constexpr int kHotkeyPollIntervalMs = 16;
}  // namespace

class HeadTrackingMod {
public:
    explicit HeadTrackingMod(std::array<int, 3>& fired) : m_fired(fired) {}

    void RegisterHotkeys();
    void ToggleEnabled() { ++m_fired[0]; }
    void CycleMode() { ++m_fired[1]; }
    void ToggleYawMode() { ++m_fired[2]; }

    cameraunlock::input::HotkeyPoller m_hotkeys;
    Config m_config;

private:
    std::array<int, 3>& m_fired;
};

#include "mod_register_hotkeys.inc"

}  // namespace stormworks_ht

namespace sw_oracle_view {

FireTable OracleFires(int toggle_key, int cycle_mode_key, int yaw_mode_key) {
    namespace input = cameraunlock::input;
    std::array<int, 3> fired{};
    input::FakeRegistrations().clear();
    stormworks_ht::HeadTrackingMod mod(fired);
    mod.m_config.toggle_key = toggle_key;
    mod.m_config.cycle_mode_key = cycle_mode_key;
    mod.m_config.yaw_mode_key = yaw_mode_key;
    mod.RegisterHotkeys();
    const std::vector<input::FakeRegistration> registered = input::FakeRegistrations();

    // The dev build's poller: a callback runs when its key goes down.
    FireTable table;
    table.reserve((kLastKey - kFirstKey + 1) * kHeldStates);
    for (int vk = kFirstKey; vk <= kLastKey; ++vk) {
        for (int held = 0; held < kHeldStates; ++held) {
            fired = {};
            input::FakeHeld() = held;
            for (const input::FakeRegistration& r : registered) {
                if (r.vk == vk && r.callback) r.callback();
            }
            table.push_back(fired);
        }
    }
    input::FakeHeld() = 0;
    return table;
}

}  // namespace sw_oracle_view
