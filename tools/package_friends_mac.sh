#!/bin/sh
# ABOUTME: Builds the Mac friends package (Apple Silicon): "War of the Monsters.app" with the game, its ffmpeg libraries, the wotm_lobby
# ABOUTME: client and the repo's mods, plus a README. No disc file goes in: the app takes the ELF from each friend's own ISO on first run.
set -eu
. "$(dirname "$0")/posix_env.sh"

case ${1-} in
    -h|--help) echo "usage: sh tools/package_friends_mac.sh   (output: dist/wotlane-mac-<buildid>/ and .zip; check: sh tools/package_check_mac.sh)"; exit 0 ;;
esac
need dylibbundler codesign ditto python3
rt=$(runtime_dir)
bin=$rt/build_dev/ps2xRuntime
[ -x "$bin/wotm" ] && [ -f "$bin/wotm.buildid" ] || fail "missing $bin/wotm or wotm.buildid (sh tools/build_runtime.sh)"
id=$(tr -d '\r\n' <"$bin/wotm.buildid")
# the exe must be built from the current code: the server rejects different builds, and the package would be from another commit
now=$(python3 "$REPO/tools/build_id.py" --runtime "$rt" | tail -1)
[ "$now" = "$id" ] || fail "wotm is from other code ($id; current is $now): run sh tools/build_runtime.sh"
sh "$REPO/tools/test_net.sh" notest   # wotm_lobby
lobby=$REPO/native/net/build/wotm_lobby

out=$REPO/dist/wotlane-mac-$id
app="$out/War of the Monsters.app"
rm -rf "$out" "$out.zip"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources/game/libs" "$app/Contents/Resources/mods"
cp "$bin/wotm" "$bin/wotm.buildid" "$lobby" "$app/Contents/Resources/game/"
cp "$REPO/tools/friends/mac/wotlane.sh" "$app/Contents/MacOS/wotlane"
chmod +x "$app/Contents/MacOS/wotlane"
for m in "$REPO"/mods/*/; do
    case $(basename "$m") in _*|.*) continue ;; esac   # disabled mods (same rule as sync_mods)
    cp -R "${m%/}" "$app/Contents/Resources/mods/"
done
cat >"$app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>War of the Monsters</string>
    <key>CFBundleDisplayName</key><string>War of the Monsters</string>
    <key>CFBundleIdentifier</key><string>io.github.sightama.wotlane</string>
    <key>CFBundleExecutable</key><string>wotlane</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$id</string>
    <key>LSMinimumSystemVersion</key><string>13.0</string>
    <key>LSArchitecturePriority</key><array><string>arm64</string></array>
    <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
EOF
# the Homebrew ffmpeg libraries the game links go inside the app, found next to it (@executable_path/libs)
run_logged "$REPO/dist/dylibbundler.log" dylibbundler -of -b -cd -x "$app/Contents/Resources/game/wotm" -d "$app/Contents/Resources/game/libs" \
    -p @executable_path/libs/
left=$(otool -L "$app/Contents/Resources/game/wotm" "$app"/Contents/Resources/game/libs/*.dylib | grep -E '/opt/homebrew|/usr/local' || true)
[ -z "$left" ] || fail "libraries still point outside the app: $left"
# changing the libraries breaks their signatures; arm64 needs every binary signed (ad hoc here: no Apple developer ID)
run_logged "$REPO/dist/codesign.log" codesign --force --deep --sign - "$app"
cp "$REPO/tools/friends/mac/README.txt" "$out/"
printf 'wotlane build %s, commit %s\n' "$id" "$(git -C "$REPO" rev-parse --short HEAD)" >"$out/BUILD.txt"

bad=$(find "$out" -type f \( -name 'SCUS*' -o -name '*.iso' -o -name '*.elf' -o -name '*.rom' \) | head -3)
[ -z "$bad" ] || fail "disc file in the package: $bad"
(cd "$REPO/dist" && ditto -c -k --keepParent "wotlane-mac-$id" "wotlane-mac-$id.zip")
echo "package: $out.zip ($(du -m "$out.zip" | cut -f1) MB)"
echo "$out"
