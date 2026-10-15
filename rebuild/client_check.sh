#!/bin/sh
# Compile check for client.dll: build the cl_dll objects with GCC 3.4 in Docker, no link.
#
#   rebuild/client_check.sh <work dir> [file.cpp ...]
#
# Hidden's client.dll was built with MSVC 7.1, which is not available here, so the client is checked
# by compiling every "Release SDK|Win32" source of cl_dll/client_sdk.vcproj with -DCLIENT_DLL and the
# project's defines. Windows-only pieces come from rebuild/client_shims/ (forced include plus a few
# stand-in headers). With file arguments (paths relative to the repo root) only those files are
# recompiled; without, everything out of date is.
#
# Writes <work dir>/out/client_check.tsv (file, PASS/FAIL, first error) and prints a summary.
# Each object's compiler output is <work dir>/out/obj/<dir>/<name>.o.log.
#
# JOBS=<n> limits parallel compiles (default 6). CLASSDUMP=1 adds -fdump-class-hierarchy; the dumps
# land next to the objects as <name>.cpp.class (used by tools/vtable_names.py).
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:?usage: client_check.sh <work dir> [file.cpp ...]}"
shift
mkdir -p "$WORK/src" "$WORK/out"
# The generated makefile has no header dependencies, so any changed header means a clean build.
CHANGED=$(rsync -a --delete --exclude .git --itemize-changes "$REPO/" "$WORK/src/" | awk '{print $2}')
if echo "$CHANGED" | grep -qiE '\.(h|hpp|inl)$'; then
    echo "headers changed, rebuilding everything"
    rm -rf "$WORK/out/obj"
fi
# -fno-operator-names: MSVC has no alternative tokens, and weapon_selection.h names a parameter "or".
EXTRA="-fno-operator-names -I/src/rebuild/client_shims -I/case -include /src/rebuild/client_shims/client_shim.h"
STAMP="$WORK/out/classdump.on"
if [ -n "$CLASSDUMP" ]; then
    EXTRA="$EXTRA -fdump-class-hierarchy"
    # objects built without the dump have no .class file: rebuild them once
    [ -f "$STAMP" ] || rm -rf "$WORK/out/obj"
    touch "$STAMP"
else
    rm -f "$STAMP"
fi
python3 -I "$REPO/rebuild/tools/vcproj_to_make.py" "$WORK/src/cl_dll/client_sdk.vcproj" "Release SDK|Win32" /src \
    "$WORK/out/client.mk" --host-root "$WORK/src" --name client --objects-only --extra-cflags "$EXTRA" >/dev/null
# Windows-cased #include names resolve through symlinks in <work dir>/case (mounted at /case).
INCDIRS=$(sed -n 's/^INCLUDES=//p' "$WORK/out/client.mk" | tr ' ' '\n' | sed -n 's|^-I/src/||p')
python3 -I "$REPO/rebuild/tools/case_includes.py" "$WORK/src" /src "$WORK/case" $INCDIRS >/dev/null
TARGETS=""
for f in "$@"; do
    f="${f#./}"
    o=$(awk -F'\t' -v f="$f" '$1 == f { print $2 }' "$WORK/out/client.mk.files")
    if [ -z "$o" ]; then
        echo "not in the Release SDK build: $f"
        continue
    fi
    rm -f "$WORK/out/obj/$o"
    TARGETS="$TARGETS /out/obj/$o"
done
if [ $# -gt 0 ] && [ -z "$TARGETS" ]; then
    exit 1
fi
docker run --rm --platform linux/386 -u "$(id -u):$(id -g)" -v "$WORK/src":/src:ro -v "$WORK/out":/out -v "$WORK/case":/case:ro -w /out \
    hsrebuild-gcc34 make -k -j"${JOBS:-6}" -f /out/client.mk OBJ=/out/obj ${TARGETS:-all} \
    > "$WORK/out/client_check.log" 2>&1 || true
python3 -I "$REPO/rebuild/tools/client_check_report.py" "$WORK/out" "$REPO/rebuild/client_shims/msvc_only.txt" "$@"
