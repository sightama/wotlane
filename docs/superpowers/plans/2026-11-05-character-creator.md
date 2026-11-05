# Character Creator (recolor skin) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A CREATE row on the single player list opens a native panel that tints a monster costume's textures with a live 3D
preview and saves the result as a mod skin that is in the costume list at once.

**Architecture:** Two pure headers (`native/wotm_tint.hpp`: HSV shift; `native/wotm_creator.hpp`: id, env parsing,
`skin.json`, PNG names) tested without raylib. One new `native/runtime/wotm_creator.inc` included by `wotm_mainmenu.inc`
holds the EE-thread state, the rows, the save and the window-thread preview; `wotm_mainmenu.inc` gets the row and a creator
branch in its panel functions. `Registry::addSkin` and `texmods::Index::add` register the saved skin without a restart.

**Tech Stack:** C++17 (runtime, native tests), raylib (`ExportImage`, model renderer), PowerShell checks (`live_check.ps1`,
`net_check.ps1`).

**Spec:** `docs/superpowers/specs/2026-11-05-character-creator-design.md` (read it first; this plan follows its sections).

## Global constraints

- English everywhere; every new file starts with two `// ABOUTME:` lines; no Portuguese left in a file you touch.
- Never commit game files (`ISO/`, `disc/`, `runtime/`, `recomp/`, `*.elf`). Commits touching docs run `python tools/doclint.py`
  first (the pre-commit hook does it when `core.hooksPath` is `tools/githooks`).
- Commit messages end with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- Build the runtime from the PowerShell tool with CMake and Ninja on PATH:
  `$env:PATH = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"; cmd /c C:\Projects\wotlane-src\build_runtime.bat`
  (about 40 s). Native tests: `cmd /c C:\Projects\wotlane-src\tools\test_native.bat` (348 checks before this plan).
- Live checks need `$env:PS2X_CD_IMAGE = 'C:\Projects\wotlane\ISO\War of the Monsters (USA).iso'` and run the game on the
  left monitor; never inject real mouse or keyboard input (Jake may be at the keyboard).
- Nothing in this plan touches guest RAM or the lockstep state: the creator is native drawing plus files on disk.

## Review focus

1. Alpha round trip: a PNG the creator writes must decode back (`texmods::loadPng`) to the pixels the preview showed. Textures
   whose raw alpha goes above 0x80 (`hiAlpha`) are reloaded with alpha doubled, so the writer halves theirs (Task 3, Task 7).
2. The panel gates (`v.panel`, `g_open` reset, `cur == 1`) must accept screen 2 only for the creator; the options panel must
   still close when the screen leaves 1 (Task 5).
3. `Registry::addSkin` must never move an existing skin's index or an existing file's sector range (Task 4).
4. `texmods::Index` is read by the window thread while the EE thread adds: every access goes through its mutex (Task 4).
5. The saved folder must load on the next boot through the unchanged `scanSkins` (Task 8's second live check proves it).

---

### Task 1: HSV shift

**Files:** Create `native/wotm_tint.hpp`; Test `native/tests/wotm_native_test.cpp`.

- [ ] **Step 1: write the failing test.** Append to `wotm_native_test.cpp` (include `"wotm_tint.hpp"` at the top):

```cpp
TEST(tint_identity_and_hue)
{
    using wotm::tint::shift;
    std::vector<uint32_t> px = {0xFF0000FFu, 0x80FF0000u, 0x00808080u, 0xC0123456u};   // A B G R per byte, little endian: R | G<<8 | B<<16 | A<<24
    std::vector<uint32_t> same = px;
    shift(same, 0.f, 0.f, 0.f);
    CHECK(same == px);   // all zero is the identity, bit for bit
    std::vector<uint32_t> hue = {0xFF0000FFu};   // pure red, alpha 255
    shift(hue, 120.f, 0.f, 0.f);
    CHECK(hue[0] == 0xFF00FF00u);   // pure green, alpha kept
    std::vector<uint32_t> back = {0xFF0000FFu};
    shift(back, -240.f, 0.f, 0.f);
    CHECK(back[0] == 0xFF00FF00u);   // wraps
    std::vector<uint32_t> gray = {0x80FF0000u};   // pure blue, alpha 0x80
    shift(gray, 0.f, -100.f, 0.f);
    CHECK(gray[0] == 0x80FFFFFFu);   // saturation gone: white at the same value, alpha kept
    std::vector<uint32_t> bright = {0xFF0000C0u};
    shift(bright, 0.f, 0.f, 100.f);
    CHECK(bright[0] == 0xFF0000FFu);   // clamps at 255
    std::vector<uint32_t> dark = {0xFF0000C0u};
    shift(dark, 0.f, 0.f, -50.f);
    CHECK(dark[0] == 0xFF000060u);
}
```

- [ ] **Step 2: run it, expect a compile failure** (`wotm_tint.hpp` missing): `cmd /c tools\test_native.bat`.

- [ ] **Step 3: write `native/wotm_tint.hpp`:**

```cpp
// ABOUTME: Hue, saturation and brightness shift of RGBA pixels (R | G << 8 | B << 16 | A << 24), the tint tool of the character
// ABOUTME: creator (and later the map creator). Pure: no raylib, no game. All three zero is the identity, bit for bit.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace wotm::tint {

// hueDeg: degrees added to the hue (wraps); sat and bright: percent, -100..+100 (saturation and value scaled by 1 + x/100);
// alpha untouched. Values outside those ranges are clamped.
inline void shift(std::vector<uint32_t> &rgba, float hueDeg, float sat, float bright) {
    if (hueDeg == 0.f && sat == 0.f && bright == 0.f) return;
    const float sk = 1.f + std::clamp(sat, -100.f, 100.f) / 100.f, vk = 1.f + std::clamp(bright, -100.f, 100.f) / 100.f;
    float hd = std::fmod(hueDeg, 360.f);
    if (hd < 0.f) hd += 360.f;
    for (uint32_t &px : rgba) {
        const float r = float(px & 0xFF) / 255.f, g = float((px >> 8) & 0xFF) / 255.f, b = float((px >> 16) & 0xFF) / 255.f;
        const uint32_t a = px & 0xFF000000u;
        const float mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
        float h = 0.f;
        if (d > 0.f) {
            if (mx == r) h = 60.f * std::fmod((g - b) / d + 6.f, 6.f);
            else if (mx == g) h = 60.f * ((b - r) / d + 2.f);
            else h = 60.f * ((r - g) / d + 4.f);
        }
        float s = mx > 0.f ? d / mx : 0.f, v = mx;
        h = std::fmod(h + hd, 360.f);
        s = std::clamp(s * sk, 0.f, 1.f);
        v = std::clamp(v * vk, 0.f, 1.f);
        const float c = v * s, x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f)), m = v - c;
        float rr, gg, bb;
        if (h < 60.f) { rr = c; gg = x; bb = 0.f; }
        else if (h < 120.f) { rr = x; gg = c; bb = 0.f; }
        else if (h < 180.f) { rr = 0.f; gg = c; bb = x; }
        else if (h < 240.f) { rr = 0.f; gg = x; bb = c; }
        else if (h < 300.f) { rr = x; gg = 0.f; bb = c; }
        else { rr = c; gg = 0.f; bb = x; }
        auto u8 = [](float f) { return uint32_t(std::lround(std::clamp(f, 0.f, 1.f) * 255.f)); };
        px = a | u8(rr + m) | u8(gg + m) << 8 | u8(bb + m) << 16;
    }
}

} // namespace wotm::tint
```

- [ ] **Step 4: run the tests, expect PASS** (349+ checks, 0 failures).
- [ ] **Step 5: commit:** `git add native/wotm_tint.hpp native/tests/wotm_native_test.cpp && git commit -m "native: HSV shift of RGBA pixels (wotm_tint.hpp) for the creator"` (plus the Co-Authored-By line).

---

### Task 2: pure creator helpers

**Files:** Create `native/wotm_creator.hpp`; Test `native/tests/wotm_native_test.cpp`.

- [ ] **Step 1: write the failing tests** (include `"wotm_creator.hpp"`; it includes `wotm_json.hpp` and `wotm_mods.hpp`):

```cpp
TEST(creator_id_from_name)
{
    using wotm::creator::idFromName;
    CHECK(idFromName("TOGERA 2") == "togera_2");
    CHECK(idFromName("  Green!! Togera  ") == "green_togera");
    CHECK(idFromName("") == "skin");
    CHECK(idFromName("---") == "skin");
    CHECK(idFromName(std::string(40, 'A')).size() == 32);
}

TEST(creator_env_parse)
{
    using wotm::creator::Script;
    using wotm::creator::parseScript;
    Script s;
    std::string err;
    CHECK(parseScript("togera:1:green:120", s, err) && s.monster == 3 && s.costume == 1 && s.id == "green" && s.hue == 120.f && s.sat == 0.f && s.bright == 0.f);
    CHECK(parseScript("ultra-v:3:x", s, err) && s.monster == 4 && s.hue == 0.f);
    CHECK(parseScript("congar:0:blue:-90:50:-20", s, err) && s.sat == 50.f && s.bright == -20.f);
    CHECK(!parseScript("bogus:1:x", s, err) && err.find("monster") != std::string::npos);
    CHECK(!parseScript("togera:9:x", s, err) && err.find("costume") != std::string::npos);
    CHECK(!parseScript("togera:1", s, err));
    CHECK(!parseScript("togera:1:Bad Id", s, err) && err.find("id") != std::string::npos);
}

TEST(creator_skin_json_and_png_names)
{
    const std::string text = wotm::creator::skinJson("togera", "green", "GREEN TOGERA", 1);
    wotm::json::Value v;
    CHECK(wotm::json::parse(text, v) && v.get("monster")->s == "togera" && v.get("id")->s == "green" && v.get("name")->s == "GREEN TOGERA");
    CHECK(v.get("base")->asInt(-1) == 1 && v.get("ai")->asBool(true) == false);
    CHECK(wotm::creator::pngName(4, false) == "t0004.png");
    CHECK(wotm::creator::pngName(11, true) == "rtx_t0011.png");
}
```

- [ ] **Step 2: run, expect a compile failure.**

- [ ] **Step 3: write `native/wotm_creator.hpp`:**

```cpp
// ABOUTME: Pure parts of the character creator (CREATE in the menu, WOTM_CREATOR): the skin id from the typed name, the
// ABOUTME: WOTM_CREATOR script, the skin.json text and the PNG names the registry accepts. No raylib, no game: unit tested.
#pragma once
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "wotm_json.hpp"
#include "wotm_mods.hpp"
#include "net/wotm_monsters.hpp"

namespace wotm::creator {

constexpr int kNameLen = 11;   // the HUD's letters
constexpr const char *kMod = "Creator";   // mods/Creator/skins/<id>/

// "TOGERA 2" -> "togera_2": lowercase, runs of anything outside a-z0-9 become one _, edges trimmed, up to 32 (skin.json's rule);
// empty -> "skin"
inline std::string idFromName(const std::string &name) {
    std::string out;
    bool gap = false;
    for (char ch : name) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (std::isalnum(c)) {
            if (gap && !out.empty()) out += '_';
            gap = false;
            out += char(std::tolower(c));
        } else gap = true;
    }
    if (out.size() > 32) out.resize(32);
    return out.empty() ? "skin" : out;
}

inline std::string pngName(uint32_t texId, bool rtx) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%st%04u.png", rtx ? "rtx_" : "", texId);
    return buf;
}

inline std::string skinJson(const std::string &monster, const std::string &id, const std::string &name, int base) {
    json::Value v = json::Value::object();
    v.at("monster") = json::Value::string(monster);
    v.at("id") = json::Value::string(id);
    v.at("name") = json::Value::string(name);
    v.at("base") = json::Value::integer(base);
    v.at("ai") = json::Value::boolean(false);
    return json::dump(v) + "\n";
}

// WOTM_CREATOR=<monster>:<costume>:<id>[:<hue>[:<sat>[:<bright>]]]
struct Script { int monster = 0; int costume = 0; std::string id; float hue = 0.f, sat = 0.f, bright = 0.f; };
inline bool parseScript(const std::string &text, Script &out, std::string &err) {
    std::vector<std::string> f;
    for (size_t i = 0;;) {
        const size_t j = text.find(':', i);
        f.push_back(text.substr(i, j == std::string::npos ? std::string::npos : j - i));
        if (j == std::string::npos) break;
        i = j + 1;
    }
    if (f.size() < 3 || f.size() > 6) { err = "expected <monster>:<costume>:<id>[:<hue>[:<sat>[:<bright>]]]"; return false; }
    const mods::MonsterInfo *mi = mods::monsterInfo(f[0]);
    if (!mi) { err = "unknown monster \"" + f[0] + "\""; return false; }
    out.monster = mi->type;
    char *end = nullptr;
    const long c = std::strtol(f[1].c_str(), &end, 10);
    if (f[1].empty() || *end || c < 0 || c >= wotm_monsters::costumes(mi->type)) { err = "costume \"" + f[1] + "\" is not on the disc"; return false; }
    out.costume = int(c);
    if (f[2].empty() || f[2].size() > 32 || f[2].find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos) {
        err = "id must be lowercase letters, digits and _ (up to 32)"; return false;
    }
    out.id = f[2];
    float *vals[3] = {&out.hue, &out.sat, &out.bright};
    out.hue = out.sat = out.bright = 0.f;
    for (size_t k = 3; k < f.size(); ++k) {
        const float x = std::strtof(f[k].c_str(), &end);
        if (f[k].empty() || *end) { err = "number expected at field " + std::to_string(k + 1); return false; }
        *vals[k - 3] = x;
    }
    return true;
}

} // namespace wotm::creator
```

Note: `json::Value::at(key)` creates the key at the end if missing (`wotm_json.hpp`); `asInt`/`asBool` exist (`scanSkins`
uses them); `json::dump` writes. Do not add a second JSON writer.

- [ ] **Step 4: run the tests, expect PASS.**
- [ ] **Step 5: commit** (`native: pure creator helpers (id, WOTM_CREATOR, skin.json, PNG names)`).

---

### Task 3: model texture ids and replaceable textures

**Files:** Modify `native/wotm_model.hpp` (`texIndex`, members near `texs_`, public API near `textures()`).

- [ ] **Step 1:** next to `std::vector<TexImage> texs_;` add `std::vector<TexInfo> texInfo_;` with, before `class Model`:

```cpp
// Where each entry of Model::textures() came from: the disc texId, whether it is in the .RTX, and whether the raw alpha goes above
// 0x80 (then texmods::loadPng doubles a PNG's alpha on reload, so a PNG written from these pixels halves it: wotm_creator.inc)
struct TexInfo { uint16_t id = 0; bool rtx = false; bool hiAlpha = false; };
```

- [ ] **Step 2:** in `texIndex`, where `decodeTex(id, t0, img)` succeeds, also push `TexInfo{id, p.rtx, hiAlpha}`. `decodeTex`
  knows `rawAmax`; give it an optional `TexInfo *info` out-parameter (default `nullptr`) that it fills with `{id, p.rtx, rawAmax > 0x80}`.
  Keep `decodeTex`'s existing callers compiling (the viewer and `monview` call it).

- [ ] **Step 3:** public API next to `textures()`:

```cpp
    const std::vector<TexInfo> &texInfo() const { return texInfo_; }
    // Replaces the decoded textures (same count and order as textures()); bumps gen() so a Renderer reuploads them
    void setTextures(std::vector<TexImage> t) {
        static uint32_t s_gen = 0;   // shared with load(): make load()'s static a class static (s_gen_) and use it here
        texs_ = std::move(t);
        gen_ = ++s_gen_;
    }
```

Make `s_gen` a `static inline uint32_t s_gen_ = 0;` member used by both `load()` and `setTextures()` (so generations never
collide). `load()` must also clear `texInfo_` where it clears `texKey_`/`texs_` (check: `load` does not clear `texs_` today
because a Model is loaded once; add `texs_.clear(); texInfo_.clear(); texKey_.clear();` at the top of `load` so a reused
Model is safe).

- [ ] **Step 4:** build the runtime (nothing uses the new API yet; it must compile) and `monview` is not built by
  `build_runtime.bat`, so also run `cmd /c native\build.bat` if that script builds the viewer; otherwise skip.
- [ ] **Step 5: commit** (`native: Model::texInfo() and setTextures() for the creator preview`).

---

### Task 4: register a skin and its PNGs after boot

**Files:** Modify `native/wotm_mods.hpp` (`scanSkins`), `native/wotm_texmods.hpp` (`Index`); Test `native/tests/wotm_native_test.cpp`.

- [ ] **Step 1: write the failing test** (uses a temp folder; `std::filesystem` is already in the test's includes through `wotm_mods.hpp`):

```cpp
TEST(mods_add_skin_after_scan)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "wotm_test_addskin";
    fs::remove_all(root);
    fs::create_directories(root / "Creator" / "skins" / "green");
    {
        std::ofstream o(root / "Creator" / "skins" / "green" / "skin.json");
        o << wotm::creator::skinJson("togera", "green", "GREEN TOGERA", 1);
        std::ofstream p(root / "Creator" / "skins" / "green" / "t0004.png");
        p << "not a real png";
    }
    wotm::mods::Registry reg;   // a fresh one, not get(): the singleton stays untouched
    reg.scan(root / "nothing_here");   // ready, empty
    CHECK(reg.ready() && reg.skins().empty());
    std::string why;
    const wotm::mods::Skin *k = reg.addSkin(root / "Creator" / "skins" / "green", why);
    CHECK(k && k->monster == 3 && k->id == "green" && k->index == 4 && k->base == 1 && k->mod == "Creator");
    CHECK(reg.skinByIndex(3, 4) == k);
    CHECK(reg.aliasOf("MON/LIZARD4.NGP") == "MON/LIZARD1.NGP");
    CHECK(reg.skinPngs().size() == 1 && reg.skinPngs()[0].stem == "LIZARD4");
    const wotm::mods::Skin *again = reg.addSkin(root / "Creator" / "skins" / "green", why);   // overwrite: same index
    CHECK(again && again->index == 4 && reg.skins().size() == 1 && reg.skinPngs().size() == 1);
    fs::create_directories(root / "Creator" / "skins" / "bad");
    CHECK(!reg.addSkin(root / "Creator" / "skins" / "bad", why) && why.find("skin.json") != std::string::npos);
    fs::remove_all(root);
}
```

This needs `Registry` constructible outside `get()`: if the constructor is private, make it public (the singleton stays the
one `get()` returns).

- [ ] **Step 2: run, expect a compile failure** (`addSkin` missing).

- [ ] **Step 3: factor `scanSkins`.** Split its body into two private functions and keep its behavior:
  - `bool readSkin(const std::filesystem::path &folder, const std::string &mod, Skin &k, std::string &why)`: the per-folder part
    (skin.json parse, monster/custom lookup, id, name, base, ai, `k.mod`, `k.dir`); the `[mods] skin ... ignored: ...` messages
    become `why` and the caller prints them (so `scanSkins` prints exactly what it prints today).
  - `bool placeSkin(Skin &k, int index, std::string &why)`: the per-skin part after sorting (stem from `index`, the 8-letter and
    99 checks, own files, NGP/PTR pairing, `m_files`/`m_alias` entries, `m_skinPngs`, the `[mods] skin ... = costume N` log).
  - `scanSkins` = loop `readSkin` into `byKey`, sort, then for each skin `placeSkin(k, next[monster]++)` and keep it on success.
- [ ] **Step 4: add the public `addSkin`:**

```cpp
    // Registers one skin folder after the scan (the character creator's save): the next free costume index of its monster, or the
    // index it already has when the same monster/id came from this folder before (overwrite). Nothing already registered moves.
    // Returns the skin, or nullptr with the reason in `why`.
    const Skin *addSkin(const std::filesystem::path &folder, std::string &why) {
        Skin k;
        const std::string mod = folder.parent_path().parent_path().filename().string();   // mods/<mod>/skins/<folder>
        if (!readSkin(folder, mod, k, why)) return nullptr;
        int index = -1;
        for (size_t i = 0; i < m_skins.size(); ++i)
            if (m_skins[i].monsterName == k.monsterName && m_skins[i].id == k.id) {
                if (m_skins[i].dir != folder) { why = "a skin " + k.monsterName + "/" + k.id + " already comes from " + m_skins[i].mod; return nullptr; }
                index = m_skins[i].index;
                m_skinPngs.erase(std::remove_if(m_skinPngs.begin(), m_skinPngs.end(), [&](const SkinPng &p) { return p.stem == k.prefix + std::to_string(index); }), m_skinPngs.end());
                m_skins.erase(m_skins.begin() + long(i));
                break;
            }
        if (index < 0) {
            index = 4;
            for (const Skin &s : m_skins) if (s.monster == k.monster) index = std::max(index, s.index + 1);
        }
        if (!placeSkin(k, index, why)) return nullptr;
        m_skins.push_back(k);
        return &m_skins.back();
    }
```

`placeSkin` must not add sector ranges: the creator's folder brings PNGs only, and `placeSkin` only touches `m_files` for
`.NGP/.PTR/.TEX/.RTX` it finds. If it finds one after boot, log `[mods] skin ...: model files in a skin added after boot are
ignored until restart` and skip the `m_files` entry (the sector table `m_byLsn` is fixed after `scan`).

- [ ] **Step 5: `texmods::Index`:** add `std::mutex m_m;` (include `<mutex>`), lock it in `find`, `empty` and `build`
  (`build` is called with the lock held by `find`: make a private `buildLocked()`), and add:

```cpp
    // A skin PNG registered after boot (Registry::addSkin): same naming rules as build()'s skin loop
    void add(const mods::SkinPng &sp) {
        std::lock_guard<std::mutex> lk(m_m);
        buildLocked();
        addSkinPngLocked(sp);
    }
```

with the skin-PNG loop body of `build` moved into `addSkinPngLocked(const mods::SkinPng &)` so both use it.

- [ ] **Step 6: run the tests, expect PASS** (the test exercises `addSkin` only; `Index` needs raylib and is covered live in Task 8).
  Build the runtime too (the `Index` change compiles there).
- [ ] **Step 7: commit** (`native: Registry::addSkin and texmods::Index::add register a skin after boot`).

---

### Task 5: the CREATE row, panel state, input and publish (EE thread)

**Files:** Create `native/runtime/wotm_creator.inc`; Modify `native/runtime/wotm_mainmenu.inc` (`kScreens`, `View`, the
screen-2 cursor code around lines 1140-1200, the panel gates around lines 1049-1052 and 1130-1145, `panelInput`,
`publishPause`'s sibling that sets `g_view`).

- [ ] **Step 0: texture groups (pure, tested).** A costume's `Model::textures()` can hold the same disc texture twice (same
  texId under two TEX0 words), and a fight looks mod PNGs up by `(rtx, texId)` only. So the creator works on *groups*: one per
  distinct `(rtx, id)`, in first-seen order of `textures()`. Add to `native/wotm_creator.hpp`:

```cpp
// One entry of Model::textures() as the creator sees it (Model::TexInfo without raylib)
struct TexKey { uint16_t id = 0; bool rtx = false; };
// Indices of textures() per distinct (rtx, id), in first-seen order: the TEXTURE row, the tints and the PNGs are per group
inline std::vector<std::vector<int>> groupTextures(const std::vector<TexKey> &keys) {
    std::vector<std::vector<int>> out;
    std::vector<TexKey> seen;
    for (int i = 0; i < int(keys.size()); ++i) {
        size_t g = 0;
        while (g < seen.size() && !(seen[g].id == keys[size_t(i)].id && seen[g].rtx == keys[size_t(i)].rtx)) ++g;
        if (g == seen.size()) { seen.push_back(keys[size_t(i)]); out.emplace_back(); }
        out[g].push_back(i);
    }
    return out;
}
```

  Test `creator_texture_groups`: keys `{4,false},{11,true},{4,false},{4,true}` give `{{0,2},{1},{3}}`; empty gives empty. Run the
  native tests, commit with Step 6's commit or on its own (`native: creator texture groups by (rtx, texId)`).

No other unit test in this task (EE-thread code against the game); the check is the live run in Task 8 plus a build.

- [ ] **Step 1: the row.** `constexpr int kCreatorRow = -4;` next to `kTrainingRow`; screen 2 in `kScreens` becomes
  `{2, 0, 5, {{1, "ADVENTURE"}, {2, "FREE-FOR-ALL"}, {3, "ENDURANCE"}, {kTrainingRow, "TRAINING"}, {kCreatorRow, "CREATE"}}}`.
  In the two places that write `kFreeForAll` into `currentSelection` for `kTrainingRow` (mouse hover and up/down), do the
  same for `kCreatorRow` (`item == kTrainingRow || item == kCreatorRow`), so the game's cursor rests on FREE-FOR-ALL while
  our row is lit. In the `code == 6 && cur == 2` branch, before the training logic: `if (g_extraRow == kCreatorRow) { wotm_creator::open(); openPanel(); wotm_padsel::sfx(rdram, ctx, runtime, 8); out = 0; }` and skip the mode logic (`else` the existing code).

- [ ] **Step 2: the gates.** `View` gets `bool creator = false;`. In the function that publishes `g_view`:
  `if (rd32(rdram, wotm::sym::currScreen) != 1 && !(g_creatorOpen && rd32(...) == 2)) g_open = false;` and
  `v.panel = (v.screen == 1 || (v.screen == 2 && g_creatorOpen)) ? g_panel : 0.f; v.creator = g_creatorOpen;`.
  The input gate `if (cur == 1 && (g_open || g_suppress > 0))` becomes `if ((cur == 1 || (cur == 2 && g_creatorOpen)) && (g_open || g_suppress > 0))`.
  `g_creatorOpen` is a `static bool` in `wotm_mainmenu` set by `wotm_creator::open()` and cleared when the panel closes
  (`panelInput` returns true) or when `currScreen` is neither 1 nor 2.

- [ ] **Step 3: `wotm_creator.inc` (EE part).** Included from `wotm_mainmenu.inc` right after the `kTrainDefs` block (it uses
  `panelSfx`, `g_m`, `View`; put its functions in `namespace wotm_mainmenu::wotm_creator`). Contents:

```cpp
// ABOUTME: Character creator panel (CREATE on the single player list; WOTM_CREATOR scripted save): picks a monster and costume, tints
// ABOUTME: each texture (hue, saturation, brightness) with the selection's 3D model as a live preview, and saves a mod skin.
// EE thread: state, rows, input, save. Window thread: Preview (the model, tinted textures, render). Spec:
// docs/superpowers/specs/2026-11-05-character-creator-design.md
namespace wotm_creator
{
enum Row { RMonster, RCostume, RTexture, RHue, RSat, RBright, RName, RSave, RBack, kRows };
struct Tint { float hue = 0.f, sat = 0.f, bright = 0.f; bool zero() const { return hue == 0.f && sat == 0.f && bright == 0.f; } };
constexpr int kMaxTex = 32;   // texture groups per costume the creator can tint (more are left as they are, with a log line)
constexpr const char *kLetters = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
constexpr const char *kHints[kRows] = {
    "WHICH MONSTER TO RECOLOR", "THE COSTUME WHOSE TEXTURES ARE THE STARTING POINT", "WHICH PART OF THE SKIN THE THREE SLIDERS BELOW CHANGE",
    "TURNS THE COLORS OF THIS TEXTURE AROUND THE WHEEL", "GRAYER TO THE LEFT, STRONGER TO THE RIGHT", "DARKER TO THE LEFT, LIGHTER TO THE RIGHT",
    "THE COSTUME NAME IN THE SELECTION. X TO EDIT", "WRITES THE SKIN TO MODS/CREATOR/SKINS AND ADDS IT TO THE COSTUME LIST", "LEAVES WITHOUT SAVING"};
constexpr const char *kNameHint = "LEFT AND RIGHT MOVE, UP AND DOWN CHANGE THE LETTER, X DONE";
// What the window thread needs (published under g_m inside View::creatorState)
struct State
{
    int monster = 0;            // index into wotm::mods::kMonsters
    int costume = 0;            // disc costume 0..3
    int tex = 0;                // texture row's index into the costume's texture groups (groupTextures)
    int texCount = 0;           // how many groups the costume has (0 until the window thread loaded it; capped at kMaxTex)
    Tint tint[kMaxTex];
    uint32_t tintGen = 1;       // bumped on every slider change
    char name[wotm::creator::kNameLen + 1] = {};
    int cursor = 0;
    bool editing = false;
    int row = RMonster;
    std::string saveValue;      // "", "SAVED", "OVERWRITE?", "SAVED, RESTART TO USE", "CANNOT WRITE"
    double saveUntil = 0.0;     // GetTime() until which saveValue shows (0: until the next change)
    bool confirmOverwrite = false;
};
static State g_st;
static std::atomic<int> g_texCountShared{0};   // group count: written by the window thread, read by the EE thread
...
}
```

Functions (all EE thread unless noted):
  - `open()`: resets `g_st.row = RMonster`, `editing = false`, `saveValue.clear()`, sets `g_creatorOpen = true`; keeps the
    last monster/costume/tints (reopening continues the work).
  - `defaultName(State&)`: `<MONSTER NAME> N` where N starts at 2 and grows while `mods/Creator/skins/<idFromName>` exists
    (`Registry::get().dir() / kMod / "skins" / id`), cut to 11 letters.
  - `resetFor(State&, bool monsterChanged)`: tints to zero, `tex = 0`, `tintGen++`, and on a monster change `costume = 0` +
    `defaultName`.
  - `input(const bool e[kPanelActs], uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime) -> bool closes`: the spec's table.
    Up/down move the row (sound 2/4) unless editing (then they roll the letter under the cursor through `kLetters`); left/right
    step the row's value (monster through the 10 `kMonsters`; costume through `wotm_monsters::costumes(type)`; texture through
    `g_texCountShared` (ignore when 0); hue ±15 wrapping in −180..180; sat ±10 in −100..100; bright ±10 in −60..60; name
    editing: cursor ±1 in 0..10); X on NAME toggles `editing`; X on SAVE calls `save()` (Task 7; in this task it only sets
    `saveValue = "SAVED"` placeholder so the panel is testable); X on BACK, or triangle anywhere (when editing, triangle only
    leaves editing), returns true. Keyboard letters while editing: for `KEY_A..KEY_Z`, `KEY_ZERO..KEY_NINE`, `KEY_SPACE`,
    `KEY_MINUS` use `wotm_pad::inputDown(code)` with a `static bool prev[128]` edge filter, set the slot and advance the cursor.
    Moving off SAVE clears `confirmOverwrite` and a `saveValue` with `saveUntil == 0`.
  - `publish(View &v)`: `v.creator = true; v.opt = g_st.row; v.creatorState = g_st;` (`View` gets `wotm_creator::State creatorState;`
    declared after this namespace, so put the `State` struct in a small `wotm_creator` block *before* `struct View` and the
    functions after `kTrainDefs`; or forward-declare: simplest is to place the whole state block above `View`).
- [ ] **Step 4: `panelInput` branch.** Right after the `g_capture` check: `if (g_creatorOpen) return wotm_creator::input(e, rdram, ctx, runtime);`
  but first apply the mouse block for the creator: hovering `hv.prow` sets `g_st.row` (when not editing) and a click on
  `HRowLeft/HRowRight/HRow` sets `e[PLeft]/e[PRight]/e[POk]`, as the existing block does for `selRef`. Easiest: let the
  existing mouse block run with `selRef = g_st.row` and `selRows = kRows` when `g_creatorOpen` (extend the two ternaries), then
  branch.
- [ ] **Step 5: build the runtime; the row must show.** Run the game by hand (`run_live.bat` or the launcher's Play, on the
  left monitor), go to 1 PLAYER, confirm CREATE is the fifth row and X opens an empty-ish panel (Task 6 draws it; before that the
  options panel body shows: fine, close with triangle). `live.err.log` must show no `[CRASH]`.
- [ ] **Step 6: commit** (`native: CREATE row on the single player list and the creator panel's state and input`).

---

### Task 6: drawing the panel and the live preview (window thread)

**Files:** Modify `native/runtime/wotm_creator.inc` (Preview), `native/runtime/wotm_mainmenu.inc` (`panelLayout`, `renderPanel`,
the legend block, `panelHit`).

- [ ] **Step 1: layout.** `panelLayout(tab, preview, training, creator = false)`: with `creator`, return the VIDEO-with-preview
  layout but `vis = wotm_creator::kRows` and `y0 = lineH * 1.85f` (no tabs; the title sits where TRAINING OPTIONS puts it), box
  on the right as in the VIDEO layout but `bh = bw * 1.1f` (the monster stands taller than wide), vertically centered on the rows.
  Every caller that passes `v.training || v.moves` passes `v.creator` as the fourth argument.
- [ ] **Step 2: rows in `renderPanel`.** With `v.creator`: title `CREATE`; no tabs, no L1/R1 badges (spec says they stay; keep
  them drawn but inert, as `badge()` does today for the options panel: simply do not guard them out); then the rows from
  `v.creatorState` (`s`):

```cpp
    else if (v.creator)
    {
        const auto &s = v.creatorState;
        using wotm::mods::kMonsters;
        auto num = [](float x, const char *suffix) { char b[16]; std::snprintf(b, sizeof b, "%+d%s", int(x), suffix); return std::string(b); };
        const bool texOk = s.texCount > 0 && s.tex < int(g_previewTexIds.size());
        std::string texLabel = texOk ? "T" + std::to_string(g_previewTexIds[size_t(s.tex)]) + " " + std::to_string(s.tex + 1) + "/" + std::to_string(s.texCount) : "LOADING";
        std::string monName = kMonsters[s.monster].name; for (char &c : monName) c = char(std::toupper((unsigned char)c));
        rows[n++] = PRow{"MONSTER", monName, kHints[RMonster], true, KOpt};
        rows[n++] = PRow{"COSTUME", std::to_string(s.costume + 1), kHints[RCostume], true, KOpt};
        rows[n++] = PRow{"TEXTURE", texLabel, kHints[RTexture], s.texCount > 1, KOpt};
        const Tint &t = s.tint[std::clamp(s.tex, 0, kMaxTex - 1)];
        rows[n++] = PRow{"HUE", num(t.hue, "\xB0"), kHints[RHue], true, KOpt};        // check the game font has °; else use "DEG"
        rows[n++] = PRow{"SATURATION", num(t.sat, ""), kHints[RSat], true, KOpt};
        rows[n++] = PRow{"BRIGHTNESS", num(t.bright, ""), kHints[RBright], true, KOpt};
        rows[n++] = PRow{"NAME", nameValue(s), s.editing ? kNameHint : kHints[RName], s.editing, KOpt};
        rows[n++] = PRow{"SAVE", s.saveValue, kHints[RSave], false, KReset};
        rows[n++] = PRow{"BACK", "", kHints[RBack], false, KReset};
    }
```

`nameValue` pads the name to 11 with `·`-like dots (use `.` if the game font lacks a middle dot) and, while editing, marks the
cursor slot by drawing an underline rectangle under that letter after the value label (compute its x from `textWidth` of the
prefix and `glyphAdvance`). Centering of SAVE/BACK: `KReset` rows already draw the name only; that is acceptable (the mock
centered them; left-aligned like RESTORE DEFAULTS is the panel's own convention, keep that).

- [ ] **Step 3: Preview** (window thread, in `wotm_creator.inc`):

```cpp
struct Preview
{
    wotm::model::Model model;
    wotm::model::Renderer ren;
    std::vector<wotm::model::TexImage> original;   // decoded textures before any tint
    int monster = -1, costume = -1;
    uint32_t tintGen = 0;
    double t0 = 0.0;
    bool ok = false;
};
static Preview g_preview;   // window thread only
static std::vector<std::vector<int>> g_previewGroups;   // textures() indices per group (window thread only)
static std::vector<uint16_t> g_previewTexIds;   // each group's texId, for the TEXTURE row's label (window thread only: renderPanel)

// Called from renderPanel with v.creator: loads, tints, poses and renders into g_preview.ren's target
static void renderPreview(const State &s, int w, int h)
{
    static const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
    const wotm::mods::MonsterInfo &mi = wotm::mods::kMonsters[s.monster];
    if (g_preview.monster != s.monster || g_preview.costume != s.costume)
    {
        g_preview = Preview{};   // ~Renderer releases its GPU textures and target
        g_preview.monster = s.monster; g_preview.costume = s.costume; g_preview.t0 = GetTime();
        const std::string iso = io.cdImage.string();
        g_preview.ok = g_preview.model.load(io.cdRoot, iso, std::string(mi.prefix) + std::to_string(s.costume));
        g_previewGroups.clear();
        g_previewTexIds.clear();
        if (g_preview.ok)
        {
            g_preview.model.decodeAllTextures();   // right after load, before any pose: same order as the save's Model
            g_preview.model.computeFraming(wotm::model::kIdle);
            g_preview.original = g_preview.model.textures();
            g_previewGroups = groupsOf(g_preview.model);
            for (const auto &g : g_previewGroups) g_previewTexIds.push_back(g_preview.model.texInfo()[size_t(g[0])].id);
        }
        g_texCountShared.store(int(g_previewGroups.size()), std::memory_order_relaxed);
        g_preview.tintGen = 0;
    }
    if (!g_preview.ok) return;
    if (g_preview.tintGen != s.tintGen)
    {
        std::vector<wotm::model::TexImage> t = g_preview.original;
        for (size_t g = 0; g < g_previewGroups.size(); ++g)
            for (int i : g_previewGroups[g]) wotm::tint::shift(t[size_t(i)].rgba, s.tint[g].hue, s.tint[g].sat, s.tint[g].bright);
        g_preview.model.setTextures(std::move(t));
        g_preview.tintGen = s.tintGen;
    }
    const float frame = float((GetTime() - g_preview.t0) * 60.0);
    if (!g_preview.model.pose(wotm::model::kIdle, frame)) g_preview.model.pose(-1, 0.f);
    using D = wotm::savepics::Drawer;
    const D::Framing fr = D::framingOf(uint32_t(mi.type) | uint32_t(s.costume & 3) << 8);
    g_preview.ren.shiftX = fr.shiftX; g_preview.ren.shiftY = fr.shiftY; g_preview.ren.fwd = fr.fwd;
    g_preview.ren.render(g_preview.model, w, h, 0.f, 8.f, fr.zoom * 0.95f, [] {});
}
```

`groupsOf(const Model &)` (in `wotm_creator.inc`, shared by the preview and the save): maps `texInfo()` to `TexKey`s, calls
`wotm::creator::groupTextures`, and keeps the first `kMaxTex` groups (logging `[creator] <name>: N texture groups, tinting the
first 32` once when cut). Textures that decode later during poses are not in any group and stay untinted (the walk in
`decodeAllTextures` makes that empty in practice).

`renderPanel` calls `renderPreview(v.creatorState, int(L.box.width), int(L.box.height))` **before** `BeginTextureMode(g_prt)`
(the Renderer has its own target; nesting texture modes is not allowed, the VIDEO preview does the same with `prepare`), and
inside the panel draws the target into `L.box` the way `wotm_padsel.inc` draws `s.ren.target().texture` (premultiplied blend,
flipped source rectangle), then restores `BLEND_CUSTOM_SEPARATE`. A caption under the box: the monster's name.

- [ ] **Step 4: legend and hint.** In the legend block: `if (v.creator) { if (s.editing) post({{left, right, "MOVE"}, {Icon up/down..., "LETTER"}, {ok, {}, "DONE"}}); else if (kind == KReset) post({{ok, {}, "SELECT"}, {back, {}, "BACK"}}); else post({{left, right, "CHANGE"}, {back, {}, "BACK"}}); }`
  (use the icons the training branch uses; for up/down use `bind(st::BUp)`/`bind(st::BDown)` if they exist, else drop that
  pair). The hint line already comes from `rows[v.opt].hint`.
- [ ] **Step 5: `panelHit`.** Pass `v.creator` to `panelLayout` and use `n = kRows` when `v.creator`; skip the tab strip test
  when `v.creator` (like `training`).
- [ ] **Step 6: build, run by hand:** open CREATE, see Togera (default monster index 2) idle in the box; move HUE: the model
  recolors; change MONSTER: the model swaps; TEXTURE shows `T<id> k/N`; NAME editing shows the cursor. Take the screenshot for the
  issue now (`docs/screenshots/2026-11-0X/..._creator_panel.png`, left monitor, the whole window).
- [ ] **Step 7: commit** (`native: creator panel drawing with the tinted 3D preview`).

---

### Task 7: save, register, `WOTM_CREATOR`

**Files:** Modify `native/runtime/wotm_creator.inc`, `native/runtime/wotm_overrides.cpp` (install after `wotmModsInstall`).

- [ ] **Step 1: `save(State &s, bool script)`** (EE thread):
  1. `id = wotm::creator::idFromName(s.name)`; `dir = Registry::get().dir() / kMod / "skins" / id`. If `Registry::get().dir()`
     is empty (mods off), `saveValue = "MODS ARE OFF"`, log and return.
  2. Name taken by another monster: if `dir/skin.json` exists and its `monster` is not this monster (parse it with
     `json::parse`), `saveValue = "NAME IN USE"`, log `[creator] <id> is a <other> skin; pick another name`, return (the player
     renames; a skin folder holds one monster's skin). Overwrite: if `dir` exists and `!s.confirmOverwrite && !script`:
     `saveValue = "OVERWRITE?"; confirmOverwrite = true; return;`.
  3. Load `wotm::model::Model m` from `MON/<prefix><costume>` (EE thread's own instance) and call `m.decodeAllTextures()` right
     after `load` (textures decode lazily; this walk decodes every OBJECT slot in the same order as the preview's Model), then
     `groups = groupsOf(m)`, `m.textures()` and `m.texInfo()`.
  4. `std::error_code ec; fs::create_directories(dir, ec);` on error: `saveValue = "CANNOT WRITE"`, log `[creator] cannot write <dir>: <ec.message()>`, return.
  5. For each group `g` with `!s.tint[g].zero()` (script: every group): take the group's first texture `i = groups[g][0]`, copy
     its `rgba`, `tint::shift`, and if `texInfo[i].hiAlpha` halve every alpha byte (`a = a >= 255 ? 0x80 : a / 2`, the inverse
     of `loadPng`'s doubling); write with `Image img{px.data(), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8}; ExportImage(img, (dir / pngName(texInfo[i].id, texInfo[i].rtx)).string().c_str())`
     (returns false on failure: `CANNOT WRITE`). One PNG per group: a fight replaces a texId with its PNG whatever the palette.
     Then remove the PNGs of groups whose tint is zero now (an earlier save of the same skin); never remove a name another group
     just wrote (names are per group, so they cannot collide).
  6. Write `skin.json` (`std::ofstream`, binary) from `skinJson(monsterName, id, name, costume)`; name = `s.name` trimmed
     (script: the id in uppercase).
  7. `std::string why; const Skin *k = Registry::get().addSkin(dir, why);` on success, for each `SkinPng` of the registry whose
     `stem == k->prefix + std::to_string(k->index)` call `texmods::Index::get().add(sp)`; `saveValue = "SAVED"`; else
     `saveValue = "SAVED, RESTART TO USE"` and log `[creator] not registered: <why>`.
  8. `std::fprintf(stderr, "[creator] saved %s (%zu png)\n", <"mods/Creator/skins/" + id>, nPng)`; `saveUntil = GetTime() + 3`;
     `confirmOverwrite = false`; the creator's `defaultName` for the *next* skin is not changed (the player may keep tuning).
  Note `GetTime()` is raylib and fine from any thread after the window exists; in the script path (before the window) use 0.
- [ ] **Step 2: `WOTM_CREATOR`.** `void wotmCreatorInstall()` in the `.inc` (called from `wotm_overrides.cpp` right after
  `wotmModsInstall(runtime, elfPath)`): parse the env with `parseScript`; on error log `[creator] WOTM_CREATOR ignored: <err>`;
  else fill a `State` (monster index from the type, costume, every `tint[i] = {hue, sat, bright}`, name = id upper) and call
  `save(st, true)`. Since `wotm_creator.inc` lives inside `wotm_mainmenu.inc` (window TU, `wotm_live.inc`) and
  `wotm_overrides.cpp` is another TU, declare `void wotmCreatorInstall();` in `wotm_overrides.cpp` and define it at global scope
  at the end of `wotm_creator.inc` (the same pattern as `wotmMainMenuInstall`). Confirm the ISO paths are available at that
  point (`PS2Runtime::getIoPaths()` is what `selModel` uses; `wotmModsInstall` runs with `elfPath`, so they are).
- [ ] **Step 3: NATIVO env row.** Add to the `<!-- WOTM_ENV -->` table in `docs/passos/NATIVO.md`:
  `| \`WOTM_CREATOR=<monster>:<costume>:<id>[:<hue>[:<sat>[:<bright>]]]\` | at boot, saves a recolor skin of that costume to \`mods/Creator/skins/<id>/\` (every texture shifted by the same values) and registers it, then boots normally ("Runtime: character creator" section) |`
  and run `python tools/wotm_env.py` (must list nothing missing).
- [ ] **Step 4: build; scripted run by hand first:** in PowerShell, `$env:WOTM_MODS = "$env:TEMP\wotm_creator_mods"; New-Item -ItemType Directory -Force $env:WOTM_MODS`
  then `tools\live_check.ps1 -Env "WOTM_CREATOR=togera:1:green:120;WOTM_MODS=$env:TEMP\wotm_creator_mods" -Secs 25 -Expect '\[creator\] saved mods/Creator/skins/green \(\d+ png\)|||\[mods\] skin togera/green \("GREEN", Creator\) = costume 4'`.
  Expected: both lines present, exit 0; `mods/Creator/skins/green/` holds `skin.json` and the PNGs; open one PNG and confirm
  it is green-shifted Togera skin (Togera costume 1 is the blue one; +120° turns blue toward red-magenta: eyeball, any hue change is fine).
- [ ] **Step 5: second boot loads it:** `tools\live_check.ps1 -Env "WOTM_MODS=$env:TEMP\wotm_creator_mods" -Secs 25 -Expect '\[mods\] skin togera/green .* = costume 4 \(LIZARD4\); base 1, \d+ png'`. Exit 0.
- [ ] **Step 6: in-game save by hand:** open CREATE, tint, SAVE: the row says SAVED, the log has both lines; go to a 2-player
  selection: Togera's costume list ends with the new name; pick it, start a fight, the fight shows the tint. Then SAVE again with
  the same name: OVERWRITE?, X again: SAVED, no duplicate in the list.
- [ ] **Step 7: commit** (`native: creator save writes the skin, registers it live, WOTM_CREATOR scripted save`; docs change in
  the same commit, doclint passes).

---

### Task 8: checks and docs

**Files:** Modify `tools/net_checks.ps1`, `docs/passos/NATIVO.md`, `docs/passos/MODS.md`; optionally `tools/live_checks.ps1` if
such a list exists (check `ls tools/*checks*`).

- [ ] **Step 0: the two live checks as entries.** Add to `tools/live_checks.ps1` (same shape as the entries there; `$env:TEMP`
  expands inside a double-quoted string):
  `'creator_script' = @{ Env = "WOTM_CREATOR=togera:1:green:120;WOTM_MODS=$env:TEMP\wotm_creator_mods"; Secs = 25; Expect = '\[creator\] saved mods/Creator/skins/green \(\d+ png\)|||\[mods\] skin togera/green \("GREEN", Creator\) = costume 4' }`
  and `'creator_reload' = @{ Env = "WOTM_MODS=$env:TEMP\wotm_creator_mods"; Secs = 25; Expect = '\[mods\] skin togera/green .* = costume 4 \(LIZARD4\); base 1, \d+ png' }`,
  with a comment that `creator_reload` needs `creator_script` first and that the folder is created by the first one
  (`live_checks.ps1` runs the entries in order, so keep them adjacent in that order). Run `tools\live_checks.ps1 -Only creator_script`
  then `-Only creator_reload`: both exit 0.
- [ ] **Step 1: lockstep with the folder on both PCs.** Add to `tools/net_checks.ps1`:
  `'creator_skin_folder' = @{ ExtraEnv = "WOTM_MODS=$env:TEMP\wotm_creator_mods"; Secs = 60 }` (the entry assumes Task 7's
  folder exists; document that in the entry's comment: "run the WOTM_CREATOR live check first"). Run
  `tools\net_checks.ps1 -Only creator_skin_folder` with `PS2X_CD_IMAGE` set: both players report identical state (the check's
  normal pass). Also run `-Only mods_diferentes` to confirm the refusal path still passes.
- [ ] **Step 2: NATIVO section.** After "Runtime: training mode", add `## Runtime: character creator (CREATE in the menu, \`WOTM_CREATOR\`)`:
  what the row does, the nine rows, the preview, where the skin goes, the live registration and its fallback, the log lines,
  the scripted variable, and the code map (`wotm_creator.inc` inside `wotm_mainmenu.inc`, `wotm_creator.hpp`, `wotm_tint.hpp`,
  `Registry::addSkin`, `texmods::Index::add`). Present tense, no dates, no "tested".
- [ ] **Step 3: MODS.md.** In "Skins", after the kit bullet: a bullet "**In game:** CREATE on the 1 player modes screen makes a
  recolor skin of any disc costume and writes it to `mods/Creator/skins/<id>/` (NATIVO.md, "Runtime: character creator"); it is
  the same folder format, so the kit opens it and friends copy it."
- [ ] **Step 4:** `python tools/doclint.py`, `python tools/wotm_env.py`, `cmd /c tools\test_native.bat` (all green), build.
- [ ] **Step 5: commit** (`docs: character creator section, MODS skins note, creator_skin_folder net check`).

---

### Task 9: finish the branch

- [ ] **Step 1:** `git log --oneline origin/wotlane/mac..creator` shows the spec, plan and the seven code/docs commits.
- [ ] **Step 2:** push `creator` to origin (`git push -u origin creator`) and open the PR to `wotlane/mac` with
  `gh pr create --base wotlane/mac`: title "Character creator step 1: recolor skins in game (#42)", body with what changes,
  the log lines, how it was checked (the two live checks, the net check, the native tests count), the screenshot from Task 6
  committed under `docs/screenshots/<date>/`, "Closes nothing (step 1 of #42)", and the `🤖 Generated with [Claude Code](https://claude.com/claude-code)` line.
- [ ] **Step 3:** comment on #42 with the PR link and the screenshot.
- [ ] **Step 4:** rebuild the runtime, server and launcher at `origin/wip/mouse-online` (9005ccc) so Jake's install matches
  Lane's server again (memory: the launcher tests need the server from the same branch).
