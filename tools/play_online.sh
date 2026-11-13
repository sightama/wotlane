#!/bin/sh
# ABOUTME: Mac/Linux way to play an online match until the Mac launcher exists: hosts a lobby on the shared game server (or on a server
# ABOUTME: here with --local) and prints the code, or joins one, then opens the game in a normal window with your keyboard, mouse and pads.
set -eu

usage() {
    cat <<EOF
usage: sh tools/play_online.sh --host [options]      host a game on the shared server and print its 4-letter code
       sh tools/play_online.sh --join <CODE> [options]   join a friend's game by its code (host:port/CODE for another server)
  --name N       your player name (default: your login name)
  --monster M    your monster id (default 1; tools/net_lobby.py and GLOSSARY.md list them)
  host only:
  --local        run the server on this computer instead (LAN, or friends with port --port forwarded)
  --port P       with --local: server port (default 7777)   --stage S    stage (default 1)
  --wins W       wins to end the match (0 = endless)  --mods L     gameplay modifiers: comma list of jump, fast, turbo
  --players N    start once N players are in the lobby (default 2)
  --bot          also join one computer-driven opponent from this computer (scripted pad, small muted window), for testing
Camera, sensitivity and the rest come from your settings (settings.ini / the in-game "Recompiled options" panel, set offline).
Logs: runtime/live.err.log (the game), runtime/logs/wotm_server.log (the server this script started), runtime/logs/bot.err.log.
EOF
}

mode= invite= name=${USER:-Player} monster=1 port=7777 stage=1 wins=0 mods= players=2 bot=0 local=0
while [ $# -gt 0 ]; do
    case $1 in
        -h|--help) usage; exit 0 ;;
        --host) mode=host; shift ;;
        --join) mode=join; invite=$2; shift 2 ;;
        --name) name=$2; shift 2 ;;
        --monster) monster=$2; shift 2 ;;
        --port) port=$2; shift 2 ;;
        --stage) stage=$2; shift 2 ;;
        --wins) wins=$2; shift 2 ;;
        --mods) mods=$2; shift 2 ;;
        --players) players=$2; shift 2 ;;
        --bot) bot=1; shift ;;
        --local) local=1; shift ;;
        *) usage >&2; exit 2 ;;
    esac
done
[ -n "$mode" ] || { usage >&2; exit 2; }

. "$(dirname "$0")/posix_env.sh"
repo=$REPO
rt=$(runtime_dir)   # WOTM_RECOMP_DIR, .wotm_recomp_dir or <repo>/runtime, as the other scripts
logs=$rt/logs
mkdir -p "$logs"
server_bin=$repo/native/net/build/wotm_server
work=$(mktemp -d)
server_pid= bot_pid=
cleanup() {
    [ -z "$bot_pid" ] || kill "$bot_pid" 2>/dev/null || true
    [ -z "$server_pid" ] || kill "$server_pid" 2>/dev/null || true
    rm -rf "$work"   # the bot's folder holds a copy of the game ELF
}
trap cleanup EXIT INT TERM

if [ "$mode" = join ]; then
    # <CODE> (a lobby on the shared server) or <host>:<port>/<CODE> (also "host:port CODE", or no port = 7777)
    invite=$(printf '%s' "$invite" | sed 's|^ *||; s| *$||; s|^wotm://||')
    code=$(printf '%s' "$invite" | sed 's|^.*[/ ]||' | tr '[:lower:]' '[:upper:]')
    case $invite in
        */*|*\ *) hostport=$(printf '%s' "$invite" | sed 's|[/ ].*$||')
                  case $hostport in *:*) ;; *) hostport=$hostport:7777 ;; esac
                  server_arg="--server $hostport" where=$hostport ;;
        *) server_arg= where="the shared server" ;;
    esac
    printf '%s' "$code" | grep -Eq '^[A-Z0-9]{4}$' || { echo "error: '$invite' is not an invite (expected the 4-letter code)" >&2; exit 2; }
    echo "Joining lobby $code on $where. Waiting for the host to start the match..."
    # shellcheck disable=SC2086
    python3 "$repo/tools/net_lobby.py" $server_arg --code "$code" --name "$name" --monster "$monster" >"$work/env.txt"
elif [ "$local" -eq 0 ]; then
    server_arg=
else
    server_arg="--server 127.0.0.1:$port"
    if ! nc -z -w 1 127.0.0.1 "$port" 2>/dev/null; then
        [ -x "$server_bin" ] || { echo "error: $server_bin not built (sh tools/test_net.sh builds it)" >&2; exit 1; }
        "$server_bin" --port "$port" >"$logs/wotm_server.log" 2>&1 &
        server_pid=$!
        sleep 1
        kill -0 "$server_pid" 2>/dev/null || { echo "error: wotm_server didn't start; see $logs/wotm_server.log" >&2; exit 1; }
        echo "Server started on port $port (stops when this script ends)"
    fi
fi
if [ "$mode" = host ]; then
    lobby=$(printf '%s' "$name" | tr -c 'A-Za-z0-9_' '_' | cut -c1-16)   # lobby names: letters, digits and _, up to 16
    # shellcheck disable=SC2086
    python3 "$repo/tools/net_lobby.py" $server_arg --create "$lobby" --name "$name" --monster "$monster" \
        --stage "$stage" --wins "$wins" --mods "$mods" --start-with "$players" >"$work/env.txt" 2>"$work/lobby.err" &
    lobby_pid=$!
    while ! code=$(sed -n 's/^lobby created, code //p' "$work/lobby.err") || [ -z "$code" ]; do
        kill -0 "$lobby_pid" 2>/dev/null || { cat "$work/lobby.err" >&2; exit 1; }
        sleep 0.2
    done
    if [ "$local" -eq 0 ]; then
        echo "Invite: $code   (friends type this code in the launcher's Join a game, or: sh tools/play_online.sh --join $code)"
    else
        lan=$(ipconfig getifaddr en0 2>/dev/null || hostname -I 2>/dev/null | cut -d' ' -f1 || echo 127.0.0.1)
        echo "Invite (same network): $lan:$port/$code"
        echo "Over the internet, friends use your public IP instead, with port $port forwarded to $lan (docs/ONLINE.md)."
    fi
    if [ "$bot" -eq 1 ]; then
        # shellcheck disable=SC2086
        python3 "$repo/tools/net_lobby.py" $server_arg --code "$code" --name Bot --monster 2 >"$work/bot_env.txt"
        mkdir -p "$work/bot"
        cp "$rt/discroot/SCUS_971.97" "$work/bot/"   # own folder: own save, like the network checks
        (
            while IFS= read -r line; do case $line in *=*) export "$line" ;; esac; done <"$work/bot_env.txt"
            export PS2X_NO_VU1=1 PS2X_NO_VIF1=1 PS2X_NO_GS=1 PS2X_SCENE=1 PS2X_FMV=1 WOTM_MUTE=1 WOTM_FULLSCREEN=0 WOTM_RES=360 \
                WOTM_NOINPUT=1 WOTM_SETTINGS=0 WOTM_REPO=$repo PS2X_CD_IMAGE="$(cat "$rt/.wotm_iso")" \
                PS2X_PAD='19:5:up;21:1:triangle;23:2:right;26:1:square;31:1:circle;36:4:left;40:1:cross;47:1:square;55:5:up;57:1:square;60:3:left;62:1:triangle'
            cd "$work/bot" && exec "$rt/build_dev/ps2xRuntime/wotm" "$work/bot/SCUS_971.97" 2>"$logs/bot.err.log" >"$logs/bot.out.log"
        ) &
        bot_pid=$!
    else
        echo "Waiting for $((players - 1)) more player(s) to join..."
    fi
    wait "$lobby_pid" || { cat "$work/lobby.err" >&2; exit 1; }
fi

while IFS= read -r line; do case $line in *=*) export "$line" ;; esac; done <"$work/env.txt"
unset PS2X_PAD   # your own keyboard, mouse and pads
echo "Match starting. Close the game window to leave."
sh "$rt/run_live.sh" --auto 0 || true
echo "Game closed. Its log: $rt/live.err.log"
