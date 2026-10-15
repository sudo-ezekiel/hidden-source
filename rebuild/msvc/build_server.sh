#!/bin/sh
# Build server.dll with MSVC 7.1 (cl 13.10.3077, link 7.10.3077) under Wine, the way Hidden built it.
#
#   JOBS=6 sh rebuild/msvc/build_server.sh <work dir> [file.cpp ...]
#
# The repo is mirrored into <work dir>/src. Every source of dlls/hl_sdk.vcproj "Release SDK|Win32" is
# compiled with that configuration's switches and per-file settings, then server.dll is linked with
# /MAP, the objects in the order of Hidden's PDB (server_link_order.txt; files the PDB lacks go to
# their alphabetical place in their filter). With file arguments (paths from the repo root) only
# those objects are rebuilt before the link.
#
# Output in <work dir>/out/msvc/server: server.dll, server.map, server.pdb, <name>.obj.log per file,
# server.dll.log (link), build.tsv (file, OK/FAIL, first error).
#
# Environment: JOBS (parallel compiles, default 6), MSVC71 (toolchain tree from unpack.sh), WINEPREFIX,
# SDKLIB (directory with the import libraries tier0.lib and vstdlib.lib; default the 2006 SDK's
# lib-vc7/public, see MSVC.md).
set -e
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="${1:?usage: build_server.sh <work dir> [file.cpp ...]}"
shift
mkdir -p "$WORK"
WORK="$(cd "$WORK" && pwd)"
HS="$HOME/.cache/subject51764/hsrebuild"
MSVC71="${MSVC71:-$HS/msvc71/tree}"
export WINEPREFIX="${WINEPREFIX:-$HS/msvc71/wineprefix}"
export WINEDEBUG=-all
SDKLIB="${SDKLIB:-$HS/hl2sdk-episode1/lib-vc7/public}"
JOBS="${JOBS:-6}"
for f in "$MSVC71/vc71/bin/cl.exe" "$SDKLIB/tier0.lib" "$SDKLIB/vstdlib.lib"; do
    [ -f "$f" ] || { echo "missing $f"; exit 1; }
done
winpath() { printf 'Z:%s' "$1" | tr / '\\'; }
export INCLUDE="$(winpath "$MSVC71/vc71/include");$(winpath "$MSVC71/psdk/Include")"
export LIB="$(winpath "$MSVC71/vc71/lib");$(winpath "$MSVC71/psdk/Lib")"
MSCL="wine $MSVC71/vc71/bin/cl.exe"
MSLINK="wine $MSVC71/vc71/bin/link.exe"
OUT="$WORK/out/msvc"
mkdir -p "$WORK/src" "$OUT"

# The makefile has no header dependencies, so any changed header means a clean build.
CHANGED=$(rsync -a --delete --exclude .git --itemize-changes "$REPO/" "$WORK/src/" | awk '{print $2}')
if echo "$CHANGED" | grep -qiE '\.(h|hpp|inl)$'; then
    echo "headers changed, rebuilding everything"
    rm -rf "$OUT/server"
fi

# One persistent wineserver for all compiles; a bare cl.exe run takes Wine's first-start noise.
wineserver -p60 2>/dev/null || true
$MSCL >/dev/null 2>&1 || true

run_make() {
    make -k -j"$JOBS" -f "$1" "MSCL=$MSCL" "MSLINK=$MSLINK" "FLOCK=flock" $2
}

python3 -I "$REPO/rebuild/msvc/vcproj_to_msvc.py" "$WORK/src/dlls/hl_sdk.vcproj" "Release SDK|Win32" \
    "$OUT/server" "$OUT/server.mk" --host-root "$WORK/src" --slots "$JOBS" \
    --lib "$SDKLIB/tier0.lib" --lib "$SDKLIB/vstdlib.lib" --link-order "$WORK/src/rebuild/msvc/server_link_order.txt"
for f in "$@"; do
    f="${f#./}"
    rel=$(realpath -m --relative-to="$REPO/dlls" "$REPO/$f")
    o=$(awk -F'\t' -v f="$rel" 'tolower($1) == tolower(f) { print $2 }' "$OUT/server.mk.files")
    [ -n "$o" ] || { echo "not in the Release SDK build: $f"; exit 1; }
    rm -f "$OUT/server/$o"
done
run_make "$OUT/server.mk" objs > "$OUT/server.log" 2>&1 || true

# Per file: OK, or FAIL with the first error line.
: > "$OUT/server/build.tsv"
while IFS="	" read -r src obj mode; do
    if [ -f "$OUT/server/$obj" ]; then
        printf '%s\tOK\t\n' "$src" >> "$OUT/server/build.tsv"
    else
        err=$(grep -m1 -E ' (error|fatal error) [A-Z]+[0-9]+' "$OUT/server/$obj.log" 2>/dev/null | tr -d '\r' || true)
        printf '%s\tFAIL\t%s\n' "$src" "${err:-no object}" >> "$OUT/server/build.tsv"
    fi
done < "$OUT/server.mk.files"
ok=$(grep -c '	OK	' "$OUT/server/build.tsv" || true)
bad=$(grep -c '	FAIL	' "$OUT/server/build.tsv" || true)
echo "compiled $ok, failed $bad (of $(wc -l < "$OUT/server.mk.files"))"
grep '	FAIL	' "$OUT/server/build.tsv" | cut -f1,3 | head -40
[ "$bad" -eq 0 ] || exit 1

rm -f "$OUT/server/server.dll"
if run_make "$OUT/server.mk" server.dll >> "$OUT/server.log" 2>&1; then
    ls -la "$OUT/server/server.dll"
else
    tr -d '\r' < "$OUT/server/server.dll.log" | grep -E 'error|unresolved' | head -60
    echo "link failed: $OUT/server/server.dll.log"
    exit 1
fi
