#!/bin/sh
# Check our client classes' vtables against client.dll's, slot by slot (rebuild/CLIENT.md).
#
#   rebuild/client_vtables.sh <work dir> [Class ...]
#
# Needs class dumps from a CLASSDUMP=1 rebuild/client_check.sh run in the same work dir. With class
# names, prints each class's vftables side by side (reference address, our function, notes). Without,
# writes <work dir>/out/vtables/{vtable_report.tsv,names.tsv,vtables_byclass/} for every class.
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:?usage: client_vtables.sh <work dir> [Class ...]}"
shift
DLL="${CLIENT_DLL:-$HOME/.cache/subject51764/hsrebuild/builds/client_2006-06-14_4bfull.dll}"
DECOMP="${CLIENT_DECOMP:-$HOME/.cache/subject51764/hsrebuild/decomp/client0614}"
if ! find "$WORK/out/obj" -name '*.class' 2>/dev/null | head -1 | grep -q .; then
    echo "no class dumps in $WORK/out/obj: run CLASSDUMP=1 rebuild/client_check.sh $WORK first"
    exit 1
fi
if [ $# -gt 0 ]; then
    ARGS=""
    for c in "$@"; do ARGS="$ARGS --class $c"; done
    exec python3 -I "$REPO/rebuild/tools/vtable_names.py" "$DLL" "$DECOMP" --dumps "$WORK/out/obj" $ARGS
fi
mkdir -p "$WORK/out/vtables"
python3 -I "$REPO/rebuild/tools/vtable_names.py" "$DLL" "$DECOMP" --dumps "$WORK/out/obj" \
    --out "$WORK/out/vtables" --no-decomp
echo "report: $WORK/out/vtables/vtable_report.tsv"
