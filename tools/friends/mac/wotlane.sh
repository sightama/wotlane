#!/bin/sh
# ABOUTME: The "War of the Monsters" Mac app (built by tools/package_friends_mac.sh): first run takes the ELF from the friend's own ISO,
# ABOUTME: then plain dialogs for Host a game (a 4-letter code) / Join a game (type the code) / Play offline, and starts the game.
#
# Every question can come from the environment instead of a dialog (the package check uses this): WOTLANE_ISO, WOTLANE_NAME,
# WOTLANE_MONSTER (id), WOTLANE_ACTION (host|join|offline), WOTLANE_CODE, WOTLANE_STAGE (id), WOTLANE_PLAYERS, WOTLANE_HOME (data folder),
# WOTLANE_SERVER (another lobby server). Game variables (WOTM_*, PS2X_PAD) set by the caller reach the game as they are.
set -u

here=$(cd "$(dirname "$0")" && pwd)
res=$(cd "$here/../Resources" && pwd)
game=$res/game
home=${WOTLANE_HOME:-$HOME/Library/Application Support/wotlane}
logs=$home/logs
disc=$home/discroot
elf_sha1=1ed7f8457436a9170d8f85386e267369c7fcdd4f   # SCUS_971.97 of the USA disc (SCUS-97197), as tools/friends/setup.ps1
mkdir -p "$disc" "$logs"
settings=$home/settings.txt
name= monster=5 stage=9 iso=
[ -f "$settings" ] && . "$settings"

monster_ids="1 2 3 4 5 7 8 9 10 11"
monster_names="Congar,Robo-47,Togera,Ultra V,Preytor,Kineticlops,Magmo,Raptros,Agamo,Zorgulon"
stage_ids="1 2 3 4 5 6 7 8 9 10 11 13 14 15 16"
stage_names="Midtown Park,Gamblers Gulch,Rosdale Canyon,Metro City,Century Airfield,Atomic Island,Baytown,Club Caldera,Tsunopolis,UFO,Capitol,Lake Powell,Mini Baytown,Volcano,Tokyo Deathmatch"

# ---- dialogs (osascript); each prints the answer, or nothing when cancelled ----
osa() { osascript -e "$1" 2>/dev/null; }
quote() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }
tell() {   # a message with an OK button; also printed for the logs and the package check
    printf '%s\n' "$1"
    [ -n "${WOTLANE_ACTION:-}" ] || osa "display dialog \"$(quote "$1")\" with title \"War of the Monsters\" buttons {\"OK\"} default button 1" >/dev/null
}
fail() { tell "$1"; exit 1; }
tell_while() {   # a message that stays up while something runs (a background dialog; untell closes it)
    printf '%s\n' "$1"
    tell_pid=
    if [ -z "${WOTLANE_ACTION:-}" ]; then
        osa "display dialog \"$(quote "$1")\" with title \"War of the Monsters\" buttons {\"OK\"} default button 1" >/dev/null &
        tell_pid=$!
    fi
}
untell() { [ -z "${tell_pid:-}" ] || kill "$tell_pid" 2>/dev/null; tell_pid=; }
ask_text() {   # question, default
    osa "text returned of (display dialog \"$(quote "$1")\" with title \"War of the Monsters\" default answer \"$(quote "$2")\")"
}
ask_list() {   # prompt, comma list, default item: prints the 1-based position of the pick
    pick=$(osa "set l to {$(printf '%s' "$2" | sed 's/[^,]*/"&"/g')}
set r to choose from list l with title \"War of the Monsters\" with prompt \"$(quote "$1")\" default items {\"$(quote "$3")\"}
if r is false then return \"\"
set p to item 1 of r
repeat with i from 1 to count of l
if item i of l is p then return i
end repeat")
    printf '%s' "$pick"
}
nth() { printf '%s' "$2" | tr "${3:- }" '\n' | sed -n "${1}p"; }
position() { printf '%s' "$2" | tr ' ' '\n' | grep -nx "$1" | cut -d: -f1; }

sq() { printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"; }   # single-quoted for the shell
save_settings() {
    printf 'name=%s\nmonster=%s\nstage=%s\niso=%s\n' "$(sq "$name")" "$(sq "$monster")" "$(sq "$stage")" "$(sq "$iso")" >"$settings"
}

# ---- first run: the ELF from the friend's own ISO (nothing from the disc ships in the package) ----
iso=${WOTLANE_ISO:-$iso}
if [ ! -f "$disc/SCUS_971.97" ] || [ ! -f "$iso" ]; then
    if [ ! -f "$iso" ]; then
        iso=$(osa 'POSIX path of (choose file with prompt "Pick your War of the Monsters (USA) disc image (.iso)" of type {"iso", "public.iso-image"})')
        [ -n "$iso" ] || exit 0
    fi
    tar -xOf "$iso" SCUS_971.97 >"$disc/SCUS_971.97.part" 2>/dev/null
    if [ "$(shasum "$disc/SCUS_971.97.part" 2>/dev/null | cut -c1-40)" != "$elf_sha1" ]; then
        rm -f "$disc/SCUS_971.97.part"
        fail "That file is not War of the Monsters for the USA (SCUS-97197). Only the USA disc works. Open the app again to pick another file."
    fi
    mv "$disc/SCUS_971.97.part" "$disc/SCUS_971.97"
    save_settings
fi
rm -rf "$disc/mods" && cp -R "$res/mods" "$disc/mods"   # the package's mods, as they came (everyone must have the same ones)

# ---- what to do ----
action=${WOTLANE_ACTION:-}
if [ -z "$action" ]; then
    action=$(osa 'button returned of (display dialog "War of the Monsters" with title "War of the Monsters" buttons {"Play offline", "Join a game", "Host a game"} default button 3)')
fi
case $action in
    "Host a game"|host) action=host ;; "Join a game"|join) action=join ;; "Play offline"|offline) action=offline ;; *) exit 0 ;;
esac

if [ "$action" != offline ]; then
    name=${WOTLANE_NAME:-$(ask_text "Your name (letters and digits):" "${name:-$(id -un)}")}
    name=$(printf '%s' "$name" | tr -c 'A-Za-z0-9_-' '_' | cut -c1-16)
    [ -n "$name" ] || exit 0
    if [ -n "${WOTLANE_MONSTER:-}" ]; then monster=$WOTLANE_MONSTER
    else
        k=$(ask_list "Your monster:" "$monster_names" "$(nth "$(position "$monster" "$monster_ids")" "$monster_names" ,)")
        [ -n "$k" ] || exit 0
        monster=$(nth "$k" "$monster_ids")
    fi
fi
server=${WOTLANE_SERVER:-}
lobby() { "$game/wotm_lobby" ${server:+--server "$server"} "$@"; }

envfile=$home/match_env.txt
rm -f "$envfile"
if [ "$action" = host ]; then
    if [ -n "${WOTLANE_STAGE:-}" ]; then stage=$WOTLANE_STAGE
    else
        k=$(ask_list "Stage:" "$stage_names" "$(nth "$(position "$stage" "$stage_ids")" "$stage_names" ,)")
        [ -n "$k" ] || exit 0
        stage=$(nth "$k" "$stage_ids")
    fi
    players=${WOTLANE_PLAYERS:-}
    if [ -z "$players" ]; then
        k=$(ask_list "How many players (you included)? The game starts when they are all in." "2 players,3 players,4 players" "2 players")
        [ -n "$k" ] || exit 0
        players=$((k + 1))
    fi
    save_settings
    lobby --create "$(printf '%s' "$name" | tr '-' '_')" --name "$name" --monster "$monster" --stage "$stage" --start-with "$players" \
        >"$envfile" 2>"$logs/lobby.log" &
    lobby_pid=$!
    code=
    while [ -z "$code" ] && kill -0 "$lobby_pid" 2>/dev/null; do
        sleep 0.2
        code=$(sed -n 's/^lobby created, code //p' "$logs/lobby.log")
    done
    [ -n "$code" ] || fail "Couldn't make the game on the server: $(sed -n 's/^error: //p' "$logs/lobby.log"). Check your internet connection."
    printf '%s' "$code" | pbcopy 2>/dev/null
    tell_while "Your game code is $code (already copied: paste it to your friends). The game starts by itself when $((players - 1)) friend(s) have joined."
    wait "$lobby_pid" || { untell; fail "The game didn't start: $(sed -n 's/^error: //p' "$logs/lobby.log")"; }
    untell
elif [ "$action" = join ]; then
    code=${WOTLANE_CODE:-$(ask_text "The 4-letter code your friend sent you:" "")}
    code=$(printf '%s' "$code" | tr -d ' ' | tr '[:lower:]' '[:upper:]')
    printf '%s' "$code" | grep -Eq '^[A-Z0-9]{4}$' || fail "A game code is 4 letters or digits, like K3GA."
    save_settings
    lobby --code "$code" --name "$name" --monster "$monster" >"$envfile" 2>"$logs/lobby.log" &
    lobby_pid=$!
    sleep 1
    kill -0 "$lobby_pid" 2>/dev/null && tell_while "You're in game $code. It starts by itself when the host is ready."
    wait "$lobby_pid"
    joined=$?
    untell
    if [ "$joined" -ne 0 ]; then
        why=$(sed -n 's/^error: //p' "$logs/lobby.log")
        case $why in
            *"no such lobby"*) fail "There is no game $code right now. Ask your friend for a new code." ;;
            *"build differs"*|*protocol*) fail "Your copy of the game is a different version from your friend's. Both of you need the same download." ;;
            *"costume"*) fail "Someone in that game already has that monster. Open the app again and pick another one." ;;
            *) fail "Couldn't join game $code: $why" ;;
        esac
    fi
fi

# ---- the game ----
export PS2X_CD_IMAGE="$iso" PS2X_NO_VU1=1 PS2X_NO_VIF1=1 PS2X_NO_GS=1 PS2X_SCENE=1 PS2X_FMV=1
if [ -f "$envfile" ] && [ "$action" != offline ]; then
    while IFS= read -r line; do case $line in *=*) export "$line" ;; esac; done <"$envfile"
fi
cd "$disc" || exit 1
"$game/wotm" "$disc/SCUS_971.97" 2>"$logs/live.err.log" >"$logs/live.out.log"
status=$?
grep -q '\[net\] stopped: ' "$logs/live.err.log" 2>/dev/null && \
    tell "The online match stopped: $(sed -n 's/^\[net\] stopped: //p' "$logs/live.err.log" | head -1)"
exit $status
