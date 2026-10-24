---
name: device-test
description: Test the Android build on real phones/tablets over wireless adb - UI sweep of every card, soak with the demo bot, frame rate and memory, power draw. Use when asked to test on the phone/tablet, soak test, or measure battery or frame rate.
---

# Test on devices

Scripts in `Tools/AndroidTests/` (they read `PM_BUILDS`, `PM_APK`, `PM_OUT` and `PM_DEVICE`; the defaults are the `PuzzleMagic_Builds`
folder next to the repo, and every connected device):
- `python Tools\AndroidTests\soak.py <hours> [serial ...]` installs the APK, then per device: a UI sweep (every card in both
  orientations, via the debug command `PuzzleUiCard`), then cycles of orientation flip, restart check and a 12-minute DEMO-bot soak
  sampling fps and memory and scanning logcat for crashes and ANRs. Output: `AndroidTests/run_<time>/<device>/` with `report.md`,
  screenshots and `sheet_*.png` contact sheets. `NO_SWEEP=1` skips the sweep.
- `python Tools\AndroidTests\analyze_ui.py <device folder>` flags text over the cards' frame lines and builds contact sheets.
- `python Tools\AndroidTests\power_test.py <minutes>` compares the power draw of fixed settings (see below).

Pairing: `adb pair <ip:port> <code>` then `adb devices`. Wireless debugging needs the phone and the PC on the same network (or a VPN
such as Tailscale). Always pass `-s <serial>` once two devices are connected.

Rules that cost hours to learn
- Look at the screenshots, not only the numbers: view the contact sheets and at least one real-play shot per device and orientation.
  An earlier "3 hour soak" sat on the Options card for its whole demo phase and nobody noticed from the logs.
- `debug.puzzle.exec` (a system property) runs a console command once per distinct text **and again every time the game starts**.
  Clear it with `setprop debug.puzzle.exec ''` before launching, or a stale command (for example "open the options card") re-runs.
- adb cannot open a secure lock screen. Ask the user to unlock the phone and set `screen_off_timeout` high for the run (restore it after).
  Do not put a device to sleep during a test: it re-locks and screenshots come out black.
- Do not use the devices, start builds or start a second test on the same device while a run is going.
- Battery: the phone's current cannot be read over adb. Measure **unplugged** with the battery charge counter (`dumpsys battery`,
  about 5 mAh steps; fit a line over 12-minute runs; alternate the settings A B C C B A). On a charger the charger's own current
  drifts by +-100 mA, as large as the effects, and the numbers are meaningless. Fix the brightness for the test.
- Frame rate: `dumpsys SurfaceFlinger --latency "<layer>"`; the layer name differs between Android versions (try the `SurfaceView[...]` ones).
- Touch drags lift the piece well above the finger; to place a piece by script, sweep release points and check the tray slot empties.
- When a test is finished: `adb shell am force-stop com.valhallastudios.puzzlemagic` on every device, confirm with `pidof`, restore the
  settings you changed (timeout, brightness, rotation), clear `debug.puzzle.exec`, and tell the user what state you left each device in.
- Test commands change the saved campaign progress on the device (not the best score); mention it.
- Confirm that a device is the one you mean before changing its settings.
