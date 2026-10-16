#!/bin/sh
# Build client.dll, server.dll and server_i486.so from this source, side by side.
#
#   rebuild/build_all.sh [work dir]      default ~/.cache/hsrebuild-install
#
# client.dll and server.dll are built with Visual C++ .NET 2003 under Wine (rebuild/msvc/MSVC.md),
# server_i486.so with GCC 3.4 in Docker (rebuild/docker/README.md). JOBS (default 4) is the parallel
# compiles of each build. The results:
#   <work>/client/out/msvc/client/client.dll
#   <work>/server/out/msvc/server/server.dll
#   <work>/linux/out/server_i486.so
# Logs in <work>/{client,server,linux}.log.
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:-$HOME/.cache/hsrebuild-install}"
export JOBS="${JOBS:-4}"
CL="$WORK/client/out/msvc/client/client.dll"
SV="$WORK/server/out/msvc/server/server.dll"
SO="$WORK/linux/out/server_i486.so"
mkdir -p "$WORK"
rm -f "$CL" "$SV" "$SO"

echo "building client.dll, server.dll and server_i486.so in $WORK"
sh "$REPO/rebuild/msvc/build_client.sh" "$WORK/client" > "$WORK/client.log" 2>&1 & c=$!
sh "$REPO/rebuild/msvc/build_server.sh" "$WORK/server" > "$WORK/server.log" 2>&1 & s=$!
sh "$REPO/rebuild/build.sh" "$WORK/linux" > "$WORK/linux.log" 2>&1 & l=$!
fail=0
wait $c || { echo "client.dll failed, see $WORK/client.log"; fail=1; }
wait $s || { echo "server.dll failed, see $WORK/server.log"; fail=1; }
wait $l || { echo "server_i486.so failed, see $WORK/linux.log"; fail=1; }
[ $fail = 0 ] || exit 1
for f in "$CL" "$SV" "$SO"; do [ -f "$f" ] || { echo "missing $f"; exit 1; }; done
