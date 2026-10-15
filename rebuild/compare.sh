#!/bin/sh
# Compare our build with Hidden's shipped server, function by function.
#
#   rebuild/compare.sh [--loose] <Hidden's server_i486.so> [work dir]
#
# Writes <work dir>/cmp.tsv (one row per function) and prints totals plus the source files with
# the most differing code. The default is the strict comparison (large constants, strings, float
# constants and the symbol behind every address are compared, see rebuild/STRICT-COMPARE.md);
# --loose is the original one, which masks every hex number above 0xFFFF, for comparing with
# older result tables.
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
MODE=
if [ "$1" = "--loose" ]; then MODE=--loose; shift; fi
REF="$1"
WORK="${2:-$HOME/.cache/hsrebuild-work}"
T="$REPO/rebuild/tools"
[ -f "$WORK/ref_map.tsv" ] && [ "$WORK/ref_map.tsv" -nt "$REF" ] || python3 -I "$T/map_functions.py" "$REF" --tsv "$WORK/ref_map.tsv"
python3 -I "$T/compare_builds.py" "$REF" "$WORK/out/server_i486.so" --map "$WORK/ref_map.tsv" --out "$WORK/cmp.tsv" $MODE
awk -F'\t' '$2!="same" && $2!="only-cand" {n[$1]++; b[$1]+=$4} END {for (f in n) printf "%7d bytes %5d funcs  %s\n", b[f], n[f], f}' "$WORK/cmp.tsv" | sort -rn | head -25
