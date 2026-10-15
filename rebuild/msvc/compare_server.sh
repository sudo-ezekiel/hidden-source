#!/bin/sh
# Compare our MSVC server.dll with Hidden's, function by function (MSVC.md, "Comparing server.dll").
#
#   sh rebuild/msvc/compare_server.sh <work dir> [Hidden's server.dll]
#
# Needs a build_server.sh run in the same work dir. The default reference is the May 2006 server.dll,
# whose PDB gives every function's name and extent (procs.tsv); the result is <work dir>/cmp_server.tsv,
# and the functions whose source did not change between May and August (stable_may_aug.tsv) are
# scored on their own in cmp_server_stable.tsv. Another reference (the August 4.1 DLL) is mapped
# without names, as the client is, into cmp_server_<name>.tsv.
# NAME=<part of our function name, or a reference address> prints those functions' diffs instead.
# PROCS= (empty) maps the May DLL without its PDB.
set -e
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="${1:?usage: compare_server.sh <work dir> [server.dll]}"
HS="$HOME/.cache/subject51764/hsrebuild"
PDBDIR="$HS/decomp/server_pdb0506"
MAY="$HS/builds/dll_2006-05-06_patch.dll"
REF="${2:-$MAY}"
if [ -z "${PROCS+set}" ]; then
    [ "$(realpath "$REF")" = "$(realpath "$MAY")" ] && PROCS="$PDBDIR/procs.tsv" || PROCS=
fi
if [ "$(realpath "$REF")" = "$(realpath "$MAY")" ]; then
    TSV="$WORK/cmp_server.tsv"
else
    TSV="$WORK/cmp_server_$(basename "$REF" .dll).tsv"
fi
OUT="$WORK/out/msvc/server"
set -- "$REF" "$OUT/server.dll" "$OUT/server.map"
[ -n "$PROCS" ] && set -- "$@" --ref-procs "$PROCS"
if [ -n "$NAME" ]; then
    exec python3 -I "$REPO/rebuild/tools/msvc_compare.py" "$@" --out /dev/null --show "$NAME"
fi
python3 -I "$REPO/rebuild/tools/msvc_compare.py" "$@" --out "$TSV"
awk -F'\t' '$2 == "changed" {n[$1]++; b[$1] += $4} END {for (f in n) printf "%7d bytes %5d funcs  %s\n", b[f], n[f], f}' \
    "$TSV" | sort -rn | head -25
echo "stable May-August subset:"
python3 -I "$REPO/rebuild/tools/msvc_subset.py" "$TSV" "$PDBDIR/stable_may_aug.tsv" --out "${TSV%.tsv}_stable.tsv"
