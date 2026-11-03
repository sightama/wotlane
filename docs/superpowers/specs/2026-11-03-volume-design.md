# Separate music and effects volume: design

Date: 2026-11-03. Branch: volume (on wotlane/mac). Status: written for Lane's review; closes issue #7.

## Goal

Issue #7 (friends package, Discord): separate volume controls for music and sound effects. The game already has two sliders
(`m_sfxVolume`, `m_musicVolume` in `screen.cpp`, through `snd_SetMasterVolume` per group: 0, 3 and 5 effects, 1 music), but online
everyone plays with the game's default options (ONLINE.md), so there is no way to turn the music down in a network match. The fix
is a per-PC factor outside the game state.

## Settings

- Two keys in `wotm_settings.hpp`: `VolSfx` (`WOTM_VOL_SFX`) and `VolMusic` (`WOTM_VOL_MUSIC`), 0..100 %, default 100; `parse`
  clamps and refuses non-numbers. Stored in `settings.ini` like every other option, not in `kNetKeys` (free in a network match) and
  not in `netConfigHash()`: it changes what this PC hears, not the simulation.
- The launcher adds the two names to `SettingsKeys` (removed from the environment when it opens the game, like the others).

## Engine (`wotm_snd.hpp`)

- `Engine::user_[2]` (effects, music), `setUserVolume(slot, pct)` under the engine mutex, refreshing the sounds of that slot;
  `userSlot(group)` maps group 1 to music and every other group to effects; `userVolume(slot)` reads back.
- `adjustToGroup` multiplies the factor into the master volume before 989snd's quadratic curve, so 50 sounds like the game's own
  slider at half. Groups 15 and up stay untouched, as before.
- `groupGain(group)` gives the resulting 0..1 gain; the narrator (`wotm_narrator.inc`, raylib sounds) uses it instead of the
  master alone, so the voice follows EFFECTS VOLUME too. Movies (`wotm_fmv.hpp`) are left alone; `WOTM_MUTE` still silences all.

## Runtime and panel

- `wotm_snd.inc` `applyUserVolumes()` pushes both settings into the engine and logs `[snd] volume: effects N%, music N%`; called
  from `snd_StartSoundSystem` and from the panel on each change (`#ifdef WOTM_HAVE_SND`, like `playPanelSfx`).
- FEATURES tab: EFFECTS VOLUME and MUSIC VOLUME after SKIP INTRO MOVIES (7 rows, the tab's limit before scrolling), values 0 (OFF)
  to 100 in tens (the game's font has no %, so the value is the bare number, like SENSITIVITY).

## Tests

- `native/tests/wotm_native_test.cpp` `user_volume_settings_and_engine_factor`: defaults, names, parse and format, free online; the
  engine math (half the factor is a quarter of the amplitude, effects groups 0/3/5 together, music apart, group 15 untouched,
  clamping, `groupGain`).
- Live: `tools\live_check.ps1` with `WOTM_VOL_MUSIC=40` expects the `[snd] volume` line; the panel change logs
  `[settings] MUSIC VOLUME = 40 (WOTM_VOL_MUSIC=40)`.
- Docs: `docs/en/steps/NATIVE.md` and `docs/passos/NATIVO.md`: the panel paragraph, the `WOTM_SETTINGS` row and a new
  `WOTM_VOL_SFX` / `WOTM_VOL_MUSIC` row (the touched Portuguese lines now in English, per the language rule).
