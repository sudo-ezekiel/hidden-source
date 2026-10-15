#!/bin/sh
# Compare RecvTables: client.dll against Hidden's server, and our client source against client.dll
# (rebuild/CLIENT.md).
#
#   rebuild/client_recv.sh <work dir> [DT_name ...]
#
# Reads our source from <work dir>/src (the mirror client_check.sh keeps) and only the files in the
# client build. With table names, prints those tables from all three sides and their status.
# Without, writes <work dir>/out/recv/{recv_report.tsv,recv_report_ours.tsv,*_recvtables.txt}.
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:?usage: client_recv.sh <work dir> [DT_name ...]}"
shift
H="$HOME/.cache/subject51764/hsrebuild"
SRC="$WORK/src"
FILES="$WORK/out/client.mk.files"
if [ ! -f "$FILES" ]; then
    echo "run rebuild/client_check.sh $WORK first (it mirrors the source and lists the build's files)"
    exit 1
fi
ARGS=""
for t in "$@"; do ARGS="$ARGS --table $t"; done
python3 -I "$REPO/rebuild/tools/recv_tables.py" \
    --client-decomp "$H/decomp/client0614/decomp.c" --client-dll "$H/builds/client_2006-06-14_4bfull.dll" \
    --server-decomp "$H/decomp/0818/decomp.c" --server-so "$H/builds/so_2006-08-18_4b1.so" \
    --source "$SRC" --build-files "$FILES" --out "$WORK/out/recv" $ARGS
