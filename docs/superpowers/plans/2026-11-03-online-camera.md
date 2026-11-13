# Online camera Implementation Plan

> **For agentic workers:** Steps use checkbox (`- [ ]`) syntax for tracking. Executed inline by the Mac and Windows chats in
> parallel on branch `wip/mouse-online` (Lane: "get this done before game night").

**Goal:** each online player steers their own modern camera with the mouse or right stick using their own settings, with no desync.

**Architecture:** `PadState` carries mouse motion and the sender's packed camera settings; the orbit applies slot N's input with
slot N's settings. Input recordings get a versioned header; old headerless files still load.

**Tech Stack:** C++17 (native layer, runtime hooks), the repo's own test runners, Python (net_lobby.py), C# (launcher).

**Spec:** docs/superpowers/specs/2026-11-03-online-camera-design.md

## Global Constraints

- Protocol 3; `PadState` 12 bytes on the wire: buttons u16, rx, ry, lx, ly u8, mouseX, mouseY i16, cam u16.
- `cam` bits: 0 present, 1 Camera, 2 CamRecenter, 3 CamInvertY, 4..7 CamSens (1..10), 8..15 zero.
- Recording header: the 7 bytes `WOTMINP` + version byte 2, then 20-byte records (u32 index, u32 controller, 12-byte PadState),
  little-endian, field by field. Headerless file = 16-byte records (14 bytes + 2 padding), mouse 0, cam 0.
- `LockToggle` (0) and `CloseCam` (1) stay forced online; the four camera keys leave `kNetKeys`.
- Comments and names in English, ABOUTME header on new files, no em dashes.

## Review Focus

- A recording with an odd tail (truncated last record) loads the whole records and ignores the tail.
- `cam` with CamSens out of 1..10 (corrupt or future input) is clamped by the orbit, never crashes.
- Changing a camera setting mid-fight changes only the next sampled frame, on every PC alike.
- The non-fixed-step offline path (normal offline play) behaves exactly as today.
- Players 5..8 online: no orbit, no mouse capture.

---

### Task 1 (Mac): camera settings packing

**Files:** Create `native/net/wotm_camsettings.hpp`; Test `native/tests/wotm_native_test.cpp`.

**Produces:** `namespace wotm::camsettings { struct Settings { bool modern, recenter, invertY; int sens; };
uint16_t pack(const Settings &); bool unpack(uint16_t, Settings &) /* false when not present */; }`

- [ ] Test `camsettings_pack_roundtrip`: every sens 1..10 x every flag combination round-trips; `unpack(0)` returns false;
      `pack` sets bit 0; sens 0 or 11 in packs to the clamped 1 or 10.
- [ ] Run `sh tools/test_native.sh`: build fails (header missing).
- [ ] Implement; run: all pass. Commit.

### Task 2 (Mac): PadState `cam`, protocol 3

**Files:** Modify `native/net/wotm_lockstep.hpp`, `native/net/wotm_net_proto.hpp`; Test `native/net/wotm_net_test.cpp`.

- [ ] Extend `proto_mouse_motion_travels_with_the_input` with `cam` (0, 0xFFFF... only valid packs, and a change in `cam` alone
      makes a new state); update the byte-count test to 12 per new state.
- [ ] Run `sh tools/test_net.sh`: fails. Implement `cam` in `PadState` (==, encode, decode). Run: pass. Commit.

### Task 3 (Mac): recording file format

**Files:** Create `native/net/wotm_inputfile.hpp` (`struct InputRecord { uint32_t index, controller; PadState state; };
std::vector<uint8_t> encodeInputHeader(); std::vector<uint8_t> encodeInputRecord(const InputRecord &);
bool readInputRecords(const std::vector<uint8_t> &, std::vector<InputRecord> &, std::string &err)`);
Modify `native/runtime/wotm_sync.inc` (write header at open, write records with `encodeInputRecord`, load with
`readInputRecords`); Test `native/tests/wotm_native_test.cpp`.

- [ ] Tests `inputfile_reads_headerless_records` (16-byte records, mouse 0, cam 0, truncated tail ignored),
      `inputfile_roundtrips_versioned_records`, `inputfile_rejects_unknown_version`.
- [ ] Run: fails. Implement; run: pass. Wire into wotm_sync.inc; build the runtime. Commit.

### Task 4 (Mac): network profile and protocol constants

**Files:** Modify `native/wotm_settings.hpp` (`kNetKeys = {LockToggle, CloseCam}`, `kNetValues = {0, 1}`),
`native/tests/wotm_native_test.cpp` (`netplay_profile_forces_gameplay_settings_only`: camera keys keep the player's values online),
check `tools/net_lobby.py` and `tools/launcher/LobbyClient.cs` say protocol 3.

- [ ] Update the test first; run: fails. Change the profile; run: pass. Commit.

### Task 5 (Windows): runtime sampling and orbit

**Files:** `native/runtime/wotm_sync.inc` (`sampleInputs`: `cam` = pack(this PC's settings) on every sampled controller; mouse on
the keyboard player's controller), `native/runtime/wotm_orbit.inc` (`camSettingsFor(slot)` replaces every read of Camera,
CamRecenter, CamSens, CamInvertY; `enabled()` becomes per slot), `native/runtime/wotm_pad.inc` (3eca24a), `tools/mouse_check.ps1`.

- [ ] Build both runtimes; mouse_check records a fight with sens 8 and invert on.

### Task 6 (both): checks from the spec's Section 3

1. Mac: stage-2 and modifiers replays (old format) give their old hashes.
2. Windows records with the mouse (sens 8, inverted); Mac replays with its own settings at sens 2: statediff identical.
3. Mac local online match, 2 clients, different settings: no desync.
4. Mac x Windows online match: no desync. Then rebuild the server on the Windows PC (protocol 3) for Lane and Jake.
