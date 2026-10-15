"""Score an msvc_compare.py result on a subset of functions given by their GCC names.

The subset is a TSV of GCC functions (mangled, demangled, source file), such as stable_may_aug.tsv:
the functions whose source did not change between Hidden's May and August servers. Rows of the
comparison are joined to it by name and arguments, written without spaces and without
class/struct/enum keywords on both sides ((void) as (), __int64 as long long).

Usage:
    python3 -I msvc_subset.py <cmp.tsv> <subset.tsv> [--out <joined.tsv>]

Prints how many subset functions our build has, and of those, how many are same, changed and
unmapped, then the source files with the most changed subset code. --out writes the joined rows:
source file, then the comparison row.
"""
import argparse
import os
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from msvc_compare import undecorated_parts  # noqa: E402


def canonical(text):
    t = text.replace(" ", "").replace("(void)", "()").replace("__int64", "longlong").replace("__cdecl", "")
    return t.replace("`anonymousnamespace'", "(anonymousnamespace)")


def without_return_type(gcc):
    """GCC writes the return type of function template instances ("datamap_t* DataMapInit<C>(C*)");
    MSVC's undecorated name has none of it."""
    rest = gcc
    while rest.endswith(" const"):
        rest = rest[:-6]
    if not rest.endswith(")"):
        return gcc
    depth = 0
    for i in range(len(rest) - 1, -1, -1):
        depth += {")": 1, "(": -1}.get(rest[i], 0)
        if depth == 0:
            break
    depth = 0
    for j in range(i - 1, -1, -1):
        c = rest[j]
        depth += {">": 1, "<": -1, ")": 1, "(": -1}.get(c, 0)
        if c == " " and depth == 0 and not rest[:j].endswith("operator"):
            return gcc[j + 1:]
    return gcc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmp")
    ap.add_argument("subset")
    ap.add_argument("--out")
    args = ap.parse_args()

    subset = {}
    with open(args.subset) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) >= 3:
                subset.setdefault(canonical(without_return_type(p[1])), p[2])
    rows = defaultdict(list)
    with open(args.cmp) as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if not p[5]:
                continue
            name, rest = undecorated_parts(p[5], p[6] or p[5])
            if name is None:
                continue
            key = canonical(name + rest)
            if key in subset:
                rows[key].append(p)

    status = Counter()
    sizes = Counter()
    per_file = defaultdict(Counter)
    out = open(args.out, "w") if args.out else None
    for key, src in subset.items():
        for p in rows.get(key, []):
            s = p[1] if p[7] else "unmapped"
            status[s] += 1
            sizes[s] += int(p[3]) or int(p[4])
            per_file[src][s] += 1
            if s == "changed":
                per_file[src]["bytes"] += int(p[3])
            if out:
                out.write(src + "\t" + "\t".join(p) + "\n")
    if out:
        out.close()
    found = sum(1 for k in subset if k in rows)
    print(f"subset: {len(subset)} functions, {found} in our build ({len(subset) - found} not: inlined "
          f"everywhere by MSVC, or not a function there)")
    for s in ("same", "changed", "unmapped"):
        print(f"  {s}\t{status[s]} functions\t{sizes[s]} bytes")
    top = sorted(per_file.items(), key=lambda x: -x[1]["bytes"])[:20]
    for src, c in top:
        if c["changed"]:
            print(f"  {c['bytes']:7d} bytes {c['changed']:5d} changed {c['same']:5d} same  {src}")


if __name__ == "__main__":
    main()
