# Shared helpers for the macOS/Linux runtime scripts (setup_runtime.sh, build_runtime.sh, sync_mods.sh, regen_runtime_patch.sh).
# Source it, don't run it: . "$(dirname "$0")/posix_env.sh"
# Sets REPO (repo root) and puts Homebrew on PATH (non-login shells, such as an agent's or Finder's, often don't have it).

REPO=$(cd "$(dirname "$0")/.." && pwd)
for d in /opt/homebrew/bin /usr/local/bin; do
    case ":$PATH:" in *":$d:"*) ;; *) [ -d "$d" ] && PATH="$d:$PATH" ;; esac
done
export PATH

step() { printf '== %s\n' "$*"; }
fail() { printf 'error: %s\n' "$*" >&2; exit 1; }
need() { for t in "$@"; do command -v "$t" >/dev/null 2>&1 || fail "missing tool: $t (brew install $t)"; done; }

# run_logged <log file> <command...>: runs the command with all output going to the log. On failure, prints the last 30 lines and
# the log path, then exits.
run_logged() {
    log=$1; shift
    mkdir -p "$(dirname "$log")"
    if ! "$@" >"$log" 2>&1; then
        tail -n 30 "$log" >&2
        fail "'$1' failed; full log: $log"
    fi
}

# runtime_dir: the runtime folder (PS2Recomp clone, build dirs, discroot). Same lookup as the Windows scripts: WOTM_RECOMP_DIR, then
# .wotm_recomp_dir (written by setup_runtime), then <repo>/runtime.
runtime_dir() {
    if [ -n "${WOTM_RECOMP_DIR:-}" ]; then printf '%s\n' "$WOTM_RECOMP_DIR"
    elif [ -f "$REPO/.wotm_recomp_dir" ]; then tr -d '\r\n' <"$REPO/.wotm_recomp_dir"; printf '\n'
    else printf '%s\n' "$REPO/runtime"; fi
}
