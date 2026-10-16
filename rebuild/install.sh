#!/bin/sh
# Build client.dll, server.dll and server_i486.so from this source and install them into the Hidden mod
# folder, with GHSI's Enhanced Edition files (ghsi/) when the folder does not have them yet.
#
#   rebuild/install.sh <sourcemods folder>      e.g. ~/.local/share/Steam/steamapps/sourcemods
#
# The three builds run side by side in $WORK (default ~/.cache/hsrebuild-install): client.dll and
# server.dll with Visual C++ .NET 2003 under Wine (rebuild/msvc/MSVC.md), server_i486.so with GCC 3.4 in
# Docker (rebuild/docker/README.md). JOBS (default 4) is the parallel compiles of each build.
# GHSI_CACHE is where ghsi.py keeps GHSI's downloads (default ~/.cache/hidden-source-ghsi).
# The DLLs are replaced by rename, so the game may be running; it loads them at its next start.
# The Source SDK Base server browser fix is a separate step: ghsi/README.md.
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SM="${1:?usage: install.sh <sourcemods folder>}"
WORK="${WORK:-$HOME/.cache/hsrebuild-install}"
export JOBS="${JOBS:-4}"
GC="${GHSI_CACHE:-$HOME/.cache/hidden-source-ghsi}"
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

if [ -f "$SM/hidden/cfg/GHS_config.cfg" ]; then
    # GHSI is installed already: only the binaries change
    for f in "$CL" "$SV" "$SO"; do
        n=$(basename "$f")
        cp "$f" "$SM/hidden/bin/.$n.new"
        mv -f "$SM/hidden/bin/.$n.new" "$SM/hidden/bin/$n"
    done
else
    python3 -I "$REPO/ghsi/ghsi.py" --cache "$GC" fetch
    python3 -I "$REPO/ghsi/ghsi.py" --cache "$GC" client "$SM" --bin "$CL" --bin "$SV" --bin "$SO"
fi
echo "installed in $SM/hidden/bin, built from $(git -C "$REPO" rev-parse --short HEAD):"
sha256sum "$SM/hidden/bin/client.dll" "$SM/hidden/bin/server.dll" "$SM/hidden/bin/server_i486.so"
