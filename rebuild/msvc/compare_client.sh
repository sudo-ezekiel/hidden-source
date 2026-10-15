#!/bin/sh
# Compare our MSVC client.dll with Hidden's, function by function (CLIENT.md section 5).
#
#   sh rebuild/msvc/compare_client.sh <work dir> [Hidden's client.dll]
#
# Needs a build_client.sh run in the same work dir. Writes <work dir>/cmp_client.tsv (one row per
# function) and prints the mapping coverage, totals, and the objects with the most changed code.
# NAME=<part of our function name, or a reference address> prints those functions' diffs instead.
set -e
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="${1:?usage: compare_client.sh <work dir> [client.dll]}"
REF="${2:-$HOME/.cache/subject51764/hsrebuild/builds/client_2006-06-14_4bfull.dll}"
DECOMP="${CLIENT_DECOMP:-$HOME/.cache/subject51764/hsrebuild/decomp/client0614}"
OUT="$WORK/out/msvc/client"
if [ -n "$NAME" ]; then
    exec python3 -I "$REPO/rebuild/tools/msvc_compare.py" "$REF" "$OUT/client.dll" "$OUT/client.map" "$DECOMP" \
        --out /dev/null --show "$NAME"
fi
python3 -I "$REPO/rebuild/tools/msvc_compare.py" "$REF" "$OUT/client.dll" "$OUT/client.map" "$DECOMP" \
    --out "$WORK/cmp_client.tsv"
awk -F'\t' '$2 == "changed" {n[$1]++; b[$1] += $4} END {for (f in n) printf "%7d bytes %5d funcs  %s\n", b[f], n[f], f}' \
    "$WORK/cmp_client.tsv" | sort -rn | head -25
