#!/bin/sh
# Compare our MSVC server.dll with Hidden's, function by function (MSVC.md, "Comparing server.dll").
#
#   sh rebuild/msvc/compare_server.sh <work dir> [Hidden's server.dll]
#
# Needs a build_server.sh run in the same work dir. The default reference is the May 2006 server.dll,
# whose PDB gives every function's name and extent (procs.tsv); the result is <work dir>/cmp_server.tsv,
# and the functions whose source did not change between May and August (stable_may_aug.tsv) are
# scored on their own in cmp_server_stable.tsv. Another reference (the 4b1 DLL of 2006-08-18, or the
# 4.1 DLL) has no PDB: it is mapped to the May DLL first, the May names of the functions that pair
# are carried over (names_<name>.tsv; NONAMES=1 skips this), and then it is mapped to ours by them
# and as the client is, into cmp_server_<name>.tsv.
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
NAMES=
if [ -z "$PROCS" ] && [ -z "${NONAMES}" ]; then
    # Names for the reference's functions, carried over from the May DLL's PDB: map the reference
    # to the May DLL (both without a map file), keep the pairs that are the same or at least half
    # alike, and give them the PDB's names.
    STEM="$(basename "$REF" .dll)"
    NAMES="$WORK/names_$STEM.tsv"
    if [ ! -s "$NAMES" ] || [ "$REF" -nt "$NAMES" ] || [ "$REPO/rebuild/tools/msvc_compare.py" -nt "$NAMES" ]; then
        python3 -I "$REPO/rebuild/tools/msvc_compare.py" "$REF" "$MAY" "$PDBDIR/procs.tsv" --ours-procs \
            --out "$WORK/may_to_$STEM.tsv" 2>/dev/null
        awk -F'\t' '$8 && $9 && ($2 == "same" || $3 >= 0.5) {print $8 "\t1\tG\t" $1 "\t" $6}' \
            "$WORK/may_to_$STEM.tsv" > "$NAMES"
    fi
    echo "$(wc -l < "$NAMES") reference names carried over from the May DLL"
fi
set -- "$REF" "$OUT/server.dll" "$OUT/server.map"
[ -n "$PROCS" ] && set -- "$@" --ref-procs "$PROCS"
[ -n "$NAMES" ] && set -- "$@" --ref-names "$NAMES"
if [ -n "$NAME" ]; then
    exec python3 -I "$REPO/rebuild/tools/msvc_compare.py" "$@" --out /dev/null --show "$NAME"
fi
python3 -I "$REPO/rebuild/tools/msvc_compare.py" "$@" --out "$TSV"
awk -F'\t' '$2 == "changed" {n[$1]++; b[$1] += $4} END {for (f in n) printf "%7d bytes %5d funcs  %s\n", b[f], n[f], f}' \
    "$TSV" | sort -rn | head -25
echo "stable May-August subset:"
python3 -I "$REPO/rebuild/tools/msvc_subset.py" "$TSV" "$PDBDIR/stable_may_aug.tsv" --out "${TSV%.tsv}_stable.tsv"
