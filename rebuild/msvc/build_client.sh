#!/bin/sh
# Build client.dll with MSVC 7.1 (cl 13.10.3077, link 7.10.3077) under Wine, the way Hidden built it.
#
#   JOBS=6 sh rebuild/msvc/build_client.sh <work dir> [file.cpp ...]
#
# The repo is mirrored into <work dir>/src. vgui_controls.lib is compiled first from
# vgui2/controls/vgui_controls.vcproj ("Release|Win32"; the repo's lib/public/*.lib are Git LFS
# pointers), then every source of cl_dll/client_sdk.vcproj "Release SDK|Win32" with that
# configuration's switches and per-file settings, then client.dll is linked with /MAP. With file
# arguments (paths from the repo root) only those client objects are rebuilt before the link.
#
# Output in <work dir>/out/msvc/client: client.dll, client.map, client.pdb, <name>.obj.log per file,
# client.dll.log (link), build.tsv (file, OK/FAIL, first error). vgui_controls in out/msvc/vgui_controls.
#
# Environment: JOBS (parallel compiles, default 6), MSVC71 (toolchain tree from unpack.sh), WINEPREFIX,
# SDKLIB (directory with the import libraries tier0.lib and vstdlib.lib, and nvtc.lib; default the
# 2006 SDK's lib-vc7/public, see MSVC.md).
set -e
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="${1:?usage: build_client.sh <work dir> [file.cpp ...]}"
shift
mkdir -p "$WORK"
WORK="$(cd "$WORK" && pwd)"
HS="$HOME/.cache/subject51764/hsrebuild"
MSVC71="${MSVC71:-$HS/msvc71/tree}"
export WINEPREFIX="${WINEPREFIX:-$HS/msvc71/wineprefix}"
export WINEDEBUG=-all
SDKLIB="${SDKLIB:-$HS/hl2sdk-episode1/lib-vc7/public}"
JOBS="${JOBS:-6}"
for f in "$MSVC71/vc71/bin/cl.exe" "$SDKLIB/tier0.lib" "$SDKLIB/vstdlib.lib" "$SDKLIB/nvtc.lib"; do
    [ -f "$f" ] || { echo "missing $f"; exit 1; }
done
winpath() { printf 'Z:%s' "$1" | tr / '\\'; }
# rebuild/msvc/include: ddraw.h, which came with the DirectX SDK (see the file).
export INCLUDE="$(winpath "$MSVC71/vc71/include");$(winpath "$MSVC71/psdk/Include");$(winpath "$WORK/src/rebuild/msvc/include")"
export LIB="$(winpath "$MSVC71/vc71/lib");$(winpath "$MSVC71/psdk/Lib")"
MSCL="wine $MSVC71/vc71/bin/cl.exe"
MSLINK="wine $MSVC71/vc71/bin/link.exe"
OUT="$WORK/out/msvc"
mkdir -p "$WORK/src" "$OUT"

# The makefiles have no header dependencies, so any changed header means a clean build.
CHANGED=$(rsync -a --delete --exclude .git --itemize-changes "$REPO/" "$WORK/src/" | awk '{print $2}')
if echo "$CHANGED" | grep -qiE '\.(h|hpp|inl)$'; then
    echo "headers changed, rebuilding everything"
    rm -rf "$OUT/vgui_controls" "$OUT/client"
fi

# One persistent wineserver for all compiles. Wine's first start prints graphics driver noise, so
# a bare cl.exe run (its banner) takes it.
wineserver -p60 2>/dev/null || true
$MSCL >/dev/null 2>&1 || true

GEN="python3 -I $REPO/rebuild/msvc/vcproj_to_msvc.py"
run_make() {
    make -k -j"$JOBS" -f "$1" "MSCL=$MSCL" "MSLINK=$MSLINK" "FLOCK=flock" $2
}

# Valve's prebuilt vgui_controls.lib, which Hidden linked, was compiled with /GL although the
# vcproj does not say so: client.dll's Rich header counts 37 Utc1310_LTCG_CPP objects, its
# vgui_controls code comes first in .text (the linker generates LTCG code before the rest) and is
# inlined across files (vgui::ipanel() in Panel::SetBorder). link.exe then restarts with /LTCG.
$GEN "$WORK/src/vgui2/controls/vgui_controls.vcproj" "Release|Win32" "$OUT/vgui_controls" \
    "$OUT/vgui_controls.mk" --host-root "$WORK/src" --kind lib --slots 1 --lib "$SDKLIB/tier0.lib" --extra-cl /GL
run_make "$OUT/vgui_controls.mk" "objs vgui_controls.lib" > "$OUT/vgui_controls.log" 2>&1 || {
    cat "$OUT/vgui_controls.log"; exit 1; }

$GEN "$WORK/src/cl_dll/client_sdk.vcproj" "Release SDK|Win32" "$OUT/client" "$OUT/client.mk" \
    --host-root "$WORK/src" --slots "$JOBS" --lib "$SDKLIB/nvtc.lib" --lib "$SDKLIB/tier0.lib" \
    --lib "$OUT/vgui_controls/vgui_controls.lib" --lib "$SDKLIB/vstdlib.lib"
for f in "$@"; do
    f="${f#./}"
    rel=$(realpath -m --relative-to="$REPO/cl_dll" "$REPO/$f")
    o=$(awk -F'\t' -v f="$rel" 'tolower($1) == tolower(f) { print $2 }' "$OUT/client.mk.files")
    [ -n "$o" ] || { echo "not in the Release SDK build: $f"; exit 1; }
    rm -f "$OUT/client/$o"
done
run_make "$OUT/client.mk" objs > "$OUT/client.log" 2>&1 || true

# Per file: OK, or FAIL with the first error line.
: > "$OUT/client/build.tsv"
while IFS="	" read -r src obj mode; do
    if [ -f "$OUT/client/$obj" ]; then
        printf '%s\tOK\t\n' "$src" >> "$OUT/client/build.tsv"
    else
        err=$(grep -m1 -E ' (error|fatal error) [A-Z]+[0-9]+' "$OUT/client/$obj.log" 2>/dev/null | tr -d '\r' || true)
        printf '%s\tFAIL\t%s\n' "$src" "${err:-no object}" >> "$OUT/client/build.tsv"
    fi
done < "$OUT/client.mk.files"
ok=$(grep -c '	OK	' "$OUT/client/build.tsv" || true)
bad=$(grep -c '	FAIL	' "$OUT/client/build.tsv" || true)
echo "compiled $ok, failed $bad (of $(wc -l < "$OUT/client.mk.files"))"
grep '	FAIL	' "$OUT/client/build.tsv" | cut -f1,3 | head -40
[ "$bad" -eq 0 ] || exit 1

rm -f "$OUT/client/client.dll"
if run_make "$OUT/client.mk" client.dll >> "$OUT/client.log" 2>&1; then
    ls -la "$OUT/client/client.dll"
else
    tr -d '\r' < "$OUT/client/client.dll.log" | grep -E 'error|unresolved' | head -60
    echo "link failed: $OUT/client/client.dll.log"
    exit 1
fi
