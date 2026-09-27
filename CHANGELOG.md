# Changelog

## [Unreleased]

### Changed
- Settings move to `CameraUnlock.ini`, beside `stormworks64.exe`. Earlier versions of the mod kept these settings in `StormworksHeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `StormworksHeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `StormworksHeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when you never changed it from the default earlier versions used, because `StormworksHeadTracking.ini` does not hold it or holds that default. It then follows `Defaults.ini`, so it takes the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none, which can differ from the default earlier versions used. A setting you changed is written with the value imported for it, or as `default` where that value equals its default at that start.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - A sensitivity or axis inversion you changed from its default. Set these in your tracker instead.
- An older version of the mod reads `StormworksHeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `StormworksHeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `StormworksHeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`.
- Several settings have the fleet's names and sections: `[Tracking] Port` is `[Network] UdpPort`; `EnableOnStartup` and `WorldSpaceYaw` are under `[General]`, and so is `DataFreshnessMs`, which was under `[Advanced]`; `LocalSmoothing` and `RemoteSmoothing` are under `[Smoothing]`; `PositionLimitZForward` is `PositionLimitZ`; and `CycleModeKey` is `CycleTrackingModeKey`.
- The tracking mode the mode hotkey picks and the yaw mode the yaw hotkey picks are saved in `CameraUnlock.ini` and come back at the next start.
- Uninstalling leaves `CameraUnlock.ini` and `StormworksHeadTracking.ini` in place, so a reinstall keeps your settings. Earlier versions removed `StormworksHeadTracking.ini`.
- Stopped keeping a centre of the mod's own, so the tracker pose is applied as absolute. Every tracker app centres itself, so a mod-side centre sat in series with the tracker's and the two drifted apart. Centre in your tracker app instead.
- Made the crosshair follow your aim when you turn your head, so it keeps marking what you will interact with or shoot rather than staying at screen centre. Leaning moves the view but does not yet move the crosshair.
- Took the GL matrix dump and the list of every GL entry point the game requests out of the log. It now reports when the camera and the crosshair are found, plus a 30-second line with how many camera uploads were adjusted.
- Time-gated the pose heartbeat at 30 seconds instead of every 600 frames. A frame-count gate ran at the player's frame rate (10s at 60Hz, 4s at 144Hz).
- Kept one previous generation of the log. It already started fresh on every launch; the session before a crash is now kept as `StormworksHeadTracking.prev.log`.
- Split smoothing into two `[Smoothing]` keys: `LocalSmoothing` (default `0.0`, tracker running on this PC) and `RemoteSmoothing` (default `0.15`, tracker on a remote network device). The value is picked per connection from the packet source address and covers both rotation and position.
- Held the last pose when the tracker stops sending instead of returning the view to the game camera, so a dropped face or a paused tracker app no longer snaps the view away and back.
- Stopped applying head tracking to the menus. The menu backdrop is a camera like any other and used to turn while you were clicking buttons.
- Added `[Position] PositionLimitYDown`, so the downward travel of a lean can be tighter than the upward travel. It defaults to 0.2, the same as before.
- Reported a camera uniform the game uploads but the mod never adjusts, so a pass that quietly renders from the untracked camera shows up in the log instead of only on screen.

### Added
- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.

### Fixed
- Fixed head tracking not moving the view. The mod only ever adjusted the old fixed-function matrix stack, which Stormworks does not use, so the tracker pose arrived and nothing on screen changed. It now applies the pose to the camera matrices the game hands its shaders, covering the scene, sky, water, lighting and shadows, and your aim and interactions still follow the mouse.
- Fixed a mistyped value and a value with a comment after it being accepted silently. `ToggleKey=35` and `PositionEnabled=0 ; no lean` used to bind or set something other than what was written; each is now refused by name in the log and the default is used.
- Fixed `Init` reporting "Receiver started on UDP port N" without checking whether the bind succeeded, so a port conflict looked like a clean start in the log. It now logs the bind result, warning when the background retry is active.

### Removed
- The sensitivity and axis inversion settings. Set these in your tracker app instead.
- With these settings at their shipped defaults the camera moves as it did before.
- Removed the recentre hotkey and the `[Hotkeys] RecenterKey` entry.
- Removed `[Tracking] Smoothing` and `[Position] PositionSmoothing`.
- Removed the hidden 0.15 baseline smoothing floor, so a local tracker now gets unsmoothed tracking by default.
