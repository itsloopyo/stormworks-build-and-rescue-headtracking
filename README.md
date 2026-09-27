# Stormworks Head Tracking

![Stormworks: Build and Rescue running with this mod](https://raw.githubusercontent.com/itsloopyo/stormworks-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Stormworks: Build and Rescue that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - head tracking moves the view; aim stays on your mouse
- **6DOF positional tracking** - lean and peek with head position
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- [Stormworks: Build and Rescue](https://store.steampowered.com/app/573090/) on Steam, running the 64-bit `stormworks64.exe`. The Steam copy is the one this mod is tested on and the one the installer finds by itself.
- A head tracking source: [OpenTrack](https://github.com/opentrack/opentrack/releases) with a webcam, a VR headset, or a phone app (see [Setting Up OpenTrack](#setting-up-opentrack))
- Windows 10 or 11, 64-bit

## Installation

### Lopari

Download [Lopari](https://lopari.app), choose **Stormworks: Build and Rescue**, and click
**Play with head tracking**.

### Standalone Installer

1. Download `StormworksHeadTracking-v<version>-installer.zip` from [Releases](https://github.com/itsloopyo/stormworks-headtracking/releases).
2. Extract it anywhere.
3. Double-click `install.cmd`.
4. Configure OpenTrack to output UDP to `127.0.0.1:4242` (see [Setting Up OpenTrack](#setting-up-opentrack)).
5. Launch the game.

The installer places `opengl32.dll` next to `stormworks64.exe`, and copies your own `C:\Windows\System32\opengl32.dll` alongside it as `opengl32_real.dll`. If a file called `opengl32.dll` was already in the game folder, it is kept as `opengl32.dll.backup` and put back on uninstall.

If the installer can't find your game, either set an environment variable:

```powershell
$env:STORMWORKS_PATH = "D:\Games\Stormworks"
.\install.cmd
```

or pass the game folder as the first argument:

```powershell
.\install.cmd "D:\Games\Stormworks"
```

The installer targets one copy of the game. If you have more than one, run it again with the other folder as the argument.

### Manual Installation

Mod managers do not deploy this mod: it needs a copy of your own Windows `opengl32.dll` beside it, which no mod archive can contain. To place the files by hand:

1. Open the installer ZIP and copy `plugins\opengl32.dll` into the game folder, next to `stormworks64.exe`. If an `opengl32.dll` is already there, rename it first.
2. Copy `C:\Windows\System32\opengl32.dll` into the same folder and rename the copy to `opengl32_real.dll`.
3. Launch the game. `CameraUnlock.ini` and `StormworksHeadTracking.log` appear next to `stormworks64.exe`.

After a Windows update, repeat step 2 so `opengl32_real.dll` matches the rest of your system.

## Setting Up OpenTrack

The mod listens for OpenTrack pose data on UDP port `4242`, on every network
interface. One datagram is six little-endian 64-bit floats in the order
`x, y, z, yaw, pitch, roll`: position in centimeters, rotation in degrees, 48
bytes in total. OpenTrack's **UDP over network** output sends exactly this.

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
2. Pick a tracker under **Input**, using the sections below.
3. Set **Output** to **UDP over network**, host `127.0.0.1`, port `4242`.
4. Press **Start**. Tracking and the game can start in either order.

Centering is done in your tracker: OpenTrack's **Center** bind, the CENTER button in a phone app, or SteamVR's reset. The mod applies the pose exactly as it arrives, so centering the tracker centers the view.

### VR Headset Setup

1. Connect the headset to the PC over Air Link, Virtual Desktop or a link cable.
2. Start SteamVR.
3. In OpenTrack, set **Input** to the SteamVR tracker.
4. Leave **Output** on **UDP over network**, `127.0.0.1:4242`.

### Webcam Setup

OpenTrack ships a **neuralnet tracker** input that reads a plain webcam and needs
no markers and no IR hardware. Select it under **Input**, pick your camera in its
settings, and use the output settings above. How well it tracks depends on your
camera and your lighting, so try it before buying anything.

### Phone App Setup

The mod accepts one thing: the OpenTrack UDP datagram described above, on port
`4242`. A phone app can drive it only if it sends that protocol itself or ships a
PC-side companion that does. Plenty of phone trackers speak something else, so
check yours for an OpenTrack or UDP output option first.

For an app that does send it, what decides the wiring is how much filtering the
app does on the phone before the packet leaves:

- **Filters on-device:** point the app straight at this PC's IP address (run
  `ipconfig` to find it) on port `4242`. No OpenTrack needed on the PC.
- **Raw or lightly filtered feed:** the mod's smoothing is sized to take the edge
  off a clean signal rather than to rescue a noisy one, so a raw feed sent direct
  will jitter. Point the app at OpenTrack's **UDP over network** input on a spare
  port, say `5252`, and let OpenTrack's filters and curves clean it up before its
  output forwards to `127.0.0.1:4242`.

The test is simple: send direct, hold your head still, and if the view drifts or
shakes, route it through OpenTrack instead. I made [Headcam](https://headcam.app)
so decent tracking was free for anybody with a phone already in their pocket. It
filters on-device, so it can send direct, and any app that filters enough noise
works the same way.

Anything arriving from outside `127.0.0.0/8` counts as a remote connection and is
smoothed with `RemoteSmoothing` rather than `LocalSmoothing`. That includes a
phone on WiFi, and also a tracker on this same PC that sends to the machine's own
LAN address instead of `127.0.0.1`, because the mod reads the source address and
not the machine.

## Controls

Two equivalent binding sets - use whichever your keyboard has:

| Action              | Nav-cluster | Chord           |
|---------------------|-------------|-----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y`  |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G`  |
| Toggle yaw mode     | `Page Down` | `Ctrl+Shift+H`  |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

`Page Down` / `Ctrl+Shift+H` switches head yaw between world-locked, where
turning your head always turns about the vertical so the horizon stays level, and
camera-local, where it turns about the camera's own up axis.

The tracking mode and the yaw mode you pick are saved in `CameraUnlock.ini` and
come back at the next start. `End` changes the current session only: head
tracking starts on or off as `EnableOnStartup` says.

## Configuration

Apart from creating `CameraUnlock.ini` at startup when there is none, the mod writes to it only when the mode hotkey or the yaw hotkey changes the tracking mode or the yaw mode. Edit it with the game closed.

<!-- cameraunlock:config -->
The mod reads its settings from `CameraUnlock.ini` in the game folder, and creates the file when it starts and finds none. Edit it with any text editor.

A setting set to `default` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.

`Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.

When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that. Edit it with any text editor.

The built-in value of each setting set to `default` below:

- `UdpPort=4242`
- `EnableOnStartup=true`
- `WorldSpaceYaw=true`
- `RotationEnabled=true`
- `DataFreshnessMs=500`
- `LocalSmoothing=0.0`
- `RemoteSmoothing=0.15`
- `PositionEnabled=true`
- `PositionLimitX=0.3`
- `PositionLimitY=0.2`
- `PositionLimitYDown=0.2`
- `PositionLimitZ=0.4`
- `PositionLimitZBack=0.1`
- `ToggleKey=End, Ctrl+Shift+Y`
- `CycleTrackingModeKey=PageUp, Ctrl+Shift+G`
- `YawModeKey=PageDown, Ctrl+Shift+H`

With every setting at its default, the file reads:

```ini
; Stormworks: Build and Rescue head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.
; A setting set to default takes its value from Defaults.ini, which every head tracking mod
; that keeps its settings in CameraUnlock.ini reads: %AppData%\CameraUnlock\Defaults.ini on
; Windows, $XDG_CONFIG_HOME/CameraUnlock/Defaults.ini (normally ~/.config/CameraUnlock) on
; Linux, under Wine and Proton too, and ~/Library/Application Support/CameraUnlock/Defaults.ini
; on macOS. The log names the file it read. Write a value instead of default to change that
; setting for this game only.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=default

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=default
; true: yaw turns around the world's up axis. false: around the camera's own up axis.
WorldSpaceYaw=default
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=default
; Milliseconds a tracker packet stays current. Once the tracker has sent nothing
; for this long, the mod stops following it until data arrives again.
DataFreshnessMs=default

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=default
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=default

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=default
; How far, in metres, leaning left or right can move the view.
PositionLimitX=default
; How far, in metres, raising your head can move the view.
PositionLimitY=default
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=default
; How far, in metres, leaning forward can move the view.
PositionLimitZ=default
; How far, in metres, leaning back can move the view.
PositionLimitZBack=default

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=default
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=default
; Switches yaw between the world's up axis and the camera's own (WorldSpaceYaw).
YawModeKey=default
```
<!-- /cameraunlock:config -->

## Troubleshooting

`StormworksHeadTracking.log` is written next to `stormworks64.exe`. It starts
fresh on every launch, and the session before is kept as
`StormworksHeadTracking.prev.log`. Attach both to any bug report.

**Mod not loading:**

- If there is no `StormworksHeadTracking.log` after launching, `opengl32.dll` is not next to `stormworks64.exe`. Run `install.cmd` again, passing the game folder if it found the wrong one.
- If the log has `FATAL: could not load opengl32_real.dll`, the copy of the Windows DLL is missing. Run `install.cmd` again, or redo step 2 of [Manual Installation](#manual-installation).
- A working start logs `StormworksHeadTracking attached` and `Real GL resolved from opengl32_real.dll`.

**No tracking response:**

- Check the log for `Receiver bound to UDP port 4242`. A `WARN: UDP receiver did not bind immediately` line means the port could not be bound, and quotes the Windows error: error 10048 is another program holding it, and closing that program lets the mod bind on its own within a second.
- Check for `Tracker data receiving`. If it never appears, confirm OpenTrack is started and its output is **UDP over network** to `127.0.0.1:4242`, or that your phone app is sending to this PC's IP on port `4242`.
- Press `End` (or `Ctrl+Shift+Y`) in case tracking was toggled off. The log records `Tracking enabled` / `Tracking disabled`.
- `Head tracking reached the camera` in the log confirms the pose is being applied in game.
- Head tracking is not applied on the menus, so test it in a loaded save rather than on the title screen. The log says `Menu camera` and `Gameplay camera` as that changes.
- If the tracker stops sending, the view holds the last pose rather than snapping back to the game camera. The log records `Tracker data lost`.

**Jittery / unstable tracking:**

- For a phone app sending direct, route it through OpenTrack as described in [Phone App Setup](#phone-app-setup).
- Raise `RemoteSmoothing` (network senders) or `LocalSmoothing` (a tracker sending to `127.0.0.1`) in `CameraUnlock.ini`.
- Improve lighting for a webcam tracker.

**View moves the wrong way on an axis:**

- Invert the axis in your tracker's settings, so the same profile behaves the same in every game.

**Yaw feels wrong when looking up or down at extreme angles:**

- Toggle between world-locked and camera-local yaw with `Page Down`. World-locked (default) keeps the horizon level; camera-local follows the camera's current up axis.

**Crosshair is off target while leaning:**

- The crosshair follows head rotation. Leaning moves the view without moving the crosshair, so center your lean when precision matters.

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes `opengl32.dll` and `opengl32_real.dll` from the
game folder and restores any `opengl32.dll` the installer backed up, and removes
both log files. It leaves `CameraUnlock.ini` in place, so a reinstall keeps your
settings. This mod uses no separate mod loader, so `uninstall.cmd /force` removes
the same files.

## Building from Source

Prerequisites: [Git](https://git-scm.com/), [pixi](https://pixi.sh), and Visual Studio with the C++ desktop workload. The game is not needed to build.

```bash
git clone --recursive https://github.com/itsloopyo/stormworks-headtracking.git
cd stormworks-headtracking
pixi run build     # build/Release/opengl32.dll
pixi run test      # characterization tests
pixi run install   # deploy to every Stormworks install found on this PC
pixi run package   # release/StormworksHeadTracking-v<version>-installer.zip
```

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

## Credits

- Geometa - developer and publisher of Stormworks: Build and Rescue
- [cameraunlock-core](https://github.com/itsloopyo/cameraunlock-core) - shared head tracking runtime, compiled into the mod (MIT)
- [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu, and Hacker Disassembler Engine 32/64 by Vyacheslav Patkov - the hooking library compiled into the mod (BSD-2-Clause)
- [OpenTrack](https://github.com/opentrack/opentrack) - head tracking software and UDP protocol (ISC)

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Geometa. Use at your own risk.
