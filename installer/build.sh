#!/bin/sh
# Make the Windows installer: Hidden: Source built from this source, with GHSI's Enhanced Edition files,
# in one .exe.
#
#   installer/build.sh [out.exe]      default $WORK/HiddenSourceInstaller.exe
#
# 1. Builds client.dll, server.dll and server_i486.so with rebuild/build_all.sh in $WORK (default
#    ~/.cache/hsrebuild-install). SKIP_BUILD=1 uses the ones already there.
# 2. Stages the mod exactly as `ghsi.py client` installs it (GHSI's files, ghsi/hidden, the built
#    binaries), and the server browser fix's files for Source SDK Base without engine.dll (the
#    installer patches the player's own engine.dll, as ghsi.py does). GHSI_CACHE is ghsi.py's download
#    cache (default ~/.cache/hidden-source-ghsi).
# 3. Compiles installer/main.go for Windows with Go and appends the staged files as a tar.gz:
#    <exe><tar.gz><offset u64 LE><size u64 LE>"HSRBPAY1".
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${WORK:-$HOME/.cache/hsrebuild-install}"
GC="${GHSI_CACHE:-$HOME/.cache/hidden-source-ghsi}"
OUT="${1:-$WORK/HiddenSourceInstaller.exe}"
CL="$WORK/client/out/msvc/client/client.dll"
SV="$WORK/server/out/msvc/server/server.dll"
SO="$WORK/linux/out/server_i486.so"
[ -n "$SKIP_BUILD" ] || sh "$REPO/rebuild/build_all.sh" "$WORK"
for f in "$CL" "$SV" "$SO"; do [ -f "$f" ] || { echo "missing $f (build it, or unset SKIP_BUILD)"; exit 1; }; done
COMMIT=$(git -C "$REPO" rev-parse --short HEAD)
[ -z "$(git -C "$REPO" status --porcelain)" ] || COMMIT="$COMMIT+changes"

STAGE="$WORK/installer-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/sourcemods" "$STAGE/sdkbase"
python3 -I "$REPO/ghsi/ghsi.py" --cache "$GC" fetch
python3 -I "$REPO/ghsi/ghsi.py" --cache "$GC" client "$STAGE/sourcemods" --bin "$CL" --bin "$SV" --bin "$SO"
(cd "$GC/browser_fix" && find . -type f ! -name .ghsi-complete ! -path ./bin/engine.dll) | while read -r f; do
    mkdir -p "$STAGE/sdkbase/$(dirname "$f")"
    cp -p "$GC/browser_fix/$f" "$STAGE/sdkbase/$f"
done
cp -rp "$REPO/ghsi/sdkbase/." "$STAGE/sdkbase/"

echo "packing $(du -sh "$STAGE" | cut -f1) of game files"
tar --sort=name --owner=0 --group=0 --numeric-owner -C "$STAGE" -cf - sdkbase sourcemods \
    | gzip -6 > "$WORK/installer-payload.tar.gz"
(cd "$REPO/installer" && CGO_ENABLED=0 GOOS=windows GOARCH=386 \
    go build -trimpath -ldflags "-s -w -X main.version=$COMMIT" -o "$WORK/installer-stub.exe" .)
python3 -I - "$WORK/installer-stub.exe" "$WORK/installer-payload.tar.gz" "$OUT" <<'EOF'
import shutil, struct, sys
stub, payload, out = sys.argv[1:]
with open(out, "wb") as o:
    with open(stub, "rb") as f:
        shutil.copyfileobj(f, o)
    off = o.tell()
    with open(payload, "rb") as f:
        shutil.copyfileobj(f, o, 1 << 20)
    o.write(struct.pack("<QQ", off, o.tell() - off) + b"HSRBPAY1")
EOF
rm -f "$WORK/installer-stub.exe" "$WORK/installer-payload.tar.gz"
ls -la "$OUT"
sha256sum "$OUT"
