# Window at screen size, resizable windows: design

Date: 2026-11-03. Branch: window-size (on wotlane/mac). Status: option 1 chosen by Jake (issue #6 lists the options), written for Lane's review.

## Goal

Issue #6: the game window opened at 1280x720 on every screen, because `fitWindow()` sized the window to the render resolution (`Res`,
default 720 lines). Jake also wants both windows, the launcher and the game, to behave as normal resizable windows when windowed.

## The game window

- `fitWindow()` (`native/runtime/wotm_live.inc`) opens the window at the largest size with the frame's aspect ratio (fixed `WOTM_ASPECT`,
  else 16:9) inside 90% of the current monitor, centered, and logs one `[native] window WxH on monitor n (MxN), aspect a` line.
- The render resolution is decoupled from the window: `Res` keeps its default of 720 lines (Lane's choice for the 4-player budget) and
  is upscaled on present; `Res` 0 ("window") makes the frame the window's height, which is now the screen-fitted one. Changing `Res` in
  the VIDEO tab no longer moves the window; changing the aspect ratio still does.
- Returning from fullscreen (F11, Display option) goes through the same function, so it lands at the fitted size.
- The runtime still opens at 960x720 until the first present (`HOST_WINDOW_WIDTH/HEIGHT` in the runtime patch); under a second, left
  alone to keep the patch unchanged.
- Resizable already: `FLAG_WINDOW_RESIZABLE` in the runtime patch; verified by the window style (WS_THICKFRAME, maximize box).

## The launcher window

- `WotmLauncher.cs` built the form with `AutoSize` + `GrowAndShrink` and a fixed border, so it could not be resized or maximized, and a
  laptop screen shorter than the form cut it off.
- Now: `FormBorderStyle.Sizable` with the maximize box; `AutoSize` stays on for the first layout only. On `Load` the form keeps that
  natural size, turns `AutoSize` off, turns `AutoScroll` on (scrollbars when smaller than its contents), sets a 360x240 minimum and
  clamps itself to the screen's working area, re-centering when clamped.

## Tests

- Game: `tools\live_check.ps1` run on monitor 1 with the window measured from outside (Win32): 1728x972 for a 1920x1080 monitor, client
  area equal to the logged size, WS_THICKFRAME set, frame still 1280x720.
- Launcher: Win32 style check (thick frame, maximize) and a programmatic shrink and grow that must stick.
- Docs: the `WOTM_RES` / `WOTM_FULLSCREEN` row of `docs/en/steps/NATIVE.md` and `docs/passos/NATIVO.md` (that row now in English).
