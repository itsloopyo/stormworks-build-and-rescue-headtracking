# Changelog

## [Unreleased]

### Changed
- Stopped keeping a centre of the mod's own, so the tracker pose is applied as absolute. Every tracker app centres itself, so a mod-side centre sat in series with the tracker's and the two drifted apart. Centre in your tracker app instead.
- Made the crosshair follow your aim when you turn your head, so it keeps marking what you will interact with or shoot rather than staying at screen centre. Leaning moves the view but does not yet move the crosshair.
- Took the GL matrix dump and the list of every GL entry point the game requests out of the log. It now reports when the camera and the crosshair are found, plus a 30-second line with how many camera uploads were adjusted.
- Time-gated the pose heartbeat at 30 seconds instead of every 600 frames. A frame-count gate ran at the player's frame rate (10s at 60Hz, 4s at 144Hz).
- Kept one previous generation of the log. It already started fresh on every launch; the session before a crash is now kept as `StormworksHeadTracking.prev.log`.
- Split smoothing into two `[Tracking]` keys: `LocalSmoothing` (default `0.0`, tracker running on this PC) and `RemoteSmoothing` (default `0.15`, tracker on a remote network device). The value is picked per connection from the packet source address and covers both rotation and position.
- Held the last pose when the tracker stops sending instead of returning the view to the game camera, so a dropped face or a paused tracker app no longer snaps the view away and back.
- Stopped applying head tracking to the menus. The menu backdrop is a camera like any other and used to turn while you were clicking buttons.
- Added `[Position] PositionLimitYDown`, so the downward travel of a lean can be tighter than the upward travel. It defaults to 0.2, the same as before.
- Reported a camera uniform the game uploads but the mod never adjusts, so a pass that quietly renders from the untracked camera shows up in the log instead of only on screen.

### Fixed
- Fixed head tracking not moving the view. The mod only ever adjusted the old fixed-function matrix stack, which Stormworks does not use, so the tracker pose arrived and nothing on screen changed. It now applies the pose to the camera matrices the game hands its shaders, covering the scene, sky, water, lighting and shadows, and your aim and interactions still follow the mouse.
- Fixed a mistyped hotkey and a commented-out on/off setting being accepted silently. `ToggleKey=End`, `ToggleKey=35` and `PositionEnabled=0 ; no lean` all used to bind or set something other than what was written; each is now refused by name in the log and the default is used.
- Fixed `Init` reporting "Receiver started on UDP port N" without checking whether the bind succeeded, so a port conflict looked like a clean start in the log. It now logs the bind result, warning when the background retry is active.

### Removed
- Removed the recentre hotkey and the `[Hotkeys] RecenterKey` entry.
- Removed `[Tracking] Smoothing` and `[Position] PositionSmoothing`.
- Removed the hidden 0.15 baseline smoothing floor, so a local tracker now gets unsmoothed tracking by default.
