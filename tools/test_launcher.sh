#!/bin/sh
# ABOUTME: macOS/Linux counterpart of tools\launcher\test.bat + build.bat: runs LobbyTest (the launcher's lobby and server logic against
# ABOUTME: the real wotm_server) with the .NET SDK and compile-checks the WinForms launcher, both in C# 5. Exit 0 when all pass.
set -eu
. "$(dirname "$0")/posix_env.sh"

case ${1-} in
    -h|--help) echo "usage: sh tools/test_launcher.sh   (needs dotnet: brew install dotnet; logs: native/net/build/launcher/*.log)"; exit 0 ;;
esac
need dotnet
export DOTNET_ROOT=${DOTNET_ROOT:-$(dirname "$(readlink -f "$(command -v dotnet)")")}
proj=$REPO/tools/launcher/dotnet
out=$REPO/native/net/build/launcher

sh "$REPO/tools/test_net.sh" notest   # wotm_server, which LobbyTest starts
mkdir -p "$out"
run_logged "$out/lobbytest_build.log" dotnet build "$proj/LobbyTest.csproj" -v q
run_logged "$out/launcher_build.log" dotnet build "$proj/Launcher.csproj" -v q
status=0
dotnet "$out/bin/Debug/net10.0/LobbyTest.dll" "$REPO/native/net/build/wotm_server" >"$out/lobbytest.log" 2>&1 || status=$?
grep -v '^ok ' "$out/lobbytest.log" || true   # failures and the totals line; the full list is in lobbytest.log
echo "launcher (WinForms, .NET Framework 4.8): compiles"
exit "$status"
