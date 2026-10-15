#!/bin/sh
# Build server_i486.so with GCC 3.4 in Docker, the way Hidden: Source's Linux server was built.
#
#   rebuild/build.sh [work dir]
#
# JOBS=<n> limits parallel compiles (default: all cores); use it when several builds run at once.
# GCC_IMAGE=<image> picks the compiler image (default hsrebuild-gcc34; see rebuild/docker/README.md).
#
# The repo is mirrored into the work dir first (Docker Desktop only shares $HOME), the makefile
# is generated from dlls/hl_sdk.vcproj ("Release SDK|Win32"), and the result is
# <work dir>/server_i486.so. Needs the hsrebuild-gcc34 image (rebuild/docker/README.md).
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:-$HOME/.cache/hsrebuild-work}"
mkdir -p "$WORK/src" "$WORK/out"
# The generated makefile has no header dependencies, so any changed header means a clean build.
CHANGED=$(rsync -a --delete --exclude .git --itemize-changes "$REPO/" "$WORK/src/" | awk '{print $2}')
if echo "$CHANGED" | grep -qiE '\.(h|hpp|inl)$'; then
    echo "headers changed, rebuilding everything"
    rm -rf "$WORK/out/obj"
fi
python3 -I "$REPO/rebuild/tools/vcproj_to_make.py" "$WORK/src/dlls/hl_sdk.vcproj" "Release SDK|Win32" /src \
    "$WORK/out/Makefile" --host-root "$WORK/src"
docker run --rm --platform linux/386 -u "$(id -u):$(id -g)" -v "$WORK/src":/src:ro -v "$WORK/out":/out -w /out \
    "${GCC_IMAGE:-hsrebuild-gcc34}" make -k -j"${JOBS:-$(nproc)}" -f /out/Makefile OBJ=/out/obj \
    'LDFLAGS=-lm -ldl /src/rebuild/lib/tier0_i486.so /src/rebuild/lib/vstdlib_i486.so' > "$WORK/out/build.log" 2>&1 \
    || { grep -E "error|Error" "$WORK/out/build.log" | head -40; exit 1; }
ls -la "$WORK/out/server_i486.so"
