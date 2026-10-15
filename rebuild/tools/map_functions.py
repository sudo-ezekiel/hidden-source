"""Map every function in an unstripped GCC 3.4 server_i486.so to the source file it was compiled from.

The symbol table keeps one FILE symbol per object file, followed by that object's local
symbols. Each object's code is laid out contiguously in link order, and GCC puts the
object's static-initializer function (_GLOBAL__I_...) at the end of its .text. So the local
functions of each file give an address range, and every global or local function belongs to the
file whose range it falls in. Weak (inline or template) functions follow the .text of the object
that emitted them, so they belong to the file that ended just before them.

Usage:
    python3 -I map_functions.py <server_i486.so> [--tsv out.tsv] [--summary]

TSV columns: file, address (hex), size, binding, mangled name, demangled name.
"""
import argparse
import bisect
import subprocess
import sys


def read_symbols(path):
    out = subprocess.run(["readelf", "-sW", path], capture_output=True, text=True, check=True).stdout
    sections = subprocess.run(["readelf", "-SW", path], capture_output=True, text=True, check=True).stdout
    text_ndx = None
    for line in sections.splitlines():
        line = line.replace("[ ", "[")
        parts = line.split()
        if len(parts) > 2 and parts[1] == ".text":
            text_ndx = parts[0].strip("[]")
    syms = []
    in_symtab = False
    for line in out.splitlines():
        if line.startswith("Symbol table '.symtab'"):
            in_symtab = True
            continue
        if line.startswith("Symbol table") and in_symtab:
            break
        if not in_symtab:
            continue
        parts = line.split(None, 7)
        if len(parts) < 7 or not parts[0].rstrip(":").isdigit():
            continue
        num, value, size, typ, bind, vis, ndx = parts[:7]
        name = parts[7] if len(parts) > 7 else ""
        size = int(size, 0) if not size.startswith("0x") else int(size, 16)
        syms.append((int(num.rstrip(":")), int(value, 16), size, typ, bind, ndx, name))
    return syms, text_ndx


def demangle(names):
    res = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True, check=True)
    return res.stdout.split("\n")[: len(names)]


def build_map(path):
    syms, text_ndx = read_symbols(path)
    files = []  # (name, [local func addrs])
    current = None
    for _, value, size, typ, bind, ndx, name in syms:
        if typ == "FILE":
            current = [name, []]
            files.append(current)
        elif current is not None and typ == "FUNC" and bind == "LOCAL" and ndx == text_ndx and value:
            current[1].append((value, value + size))
    # A file's range ends at the end of its last local function (normally _GLOBAL__I_).
    ranges = []
    for name, funcs in files:
        if funcs:
            ranges.append((max(e for _, e in funcs), min(s for s, _ in funcs), name))
    ranges.sort()
    ends = [r[0] for r in ranges]
    funcs = [s for s in syms if s[3] == "FUNC" and s[5] == text_ndx and s[1]]
    funcs.sort(key=lambda s: s[1])
    seen = set()
    rows = []
    for _, value, size, typ, bind, ndx, name in funcs:
        if (value, name) in seen:
            continue
        seen.add((value, name))
        i = bisect.bisect_right(ends, value)
        if bind == "WEAK" and i > 0:
            # Inline and template code sits in .gnu.linkonce.t sections, which the linker places
            # after the object's .text, so a weak function belongs to the file that ended before it.
            owner = ranges[i - 1][2]
        else:
            owner = ranges[i][2] if i < len(ranges) else "<after last file>"
        rows.append([owner, value, size, bind, name])
    names = demangle([r[4] for r in rows])
    for r, d in zip(rows, names):
        r.append(d)
    return rows, ranges


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("so")
    ap.add_argument("--tsv")
    ap.add_argument("--summary", action="store_true")
    args = ap.parse_args()
    rows, ranges = build_map(args.so)
    # Check the layout assumption: ranges must not overlap.
    overlaps = sum(1 for a, b in zip(ranges, ranges[1:]) if b[1] < a[0])
    print(f"{len(rows)} functions, {len(ranges)} files with code, {overlaps} overlapping ranges", file=sys.stderr)
    if args.tsv:
        with open(args.tsv, "w") as f:
            for owner, value, size, bind, name, dem in rows:
                f.write(f"{owner}\t{value:08x}\t{size}\t{bind}\t{name}\t{dem}\n")
    if args.summary:
        counts = {}
        for owner, value, size, *_ in rows:
            c = counts.setdefault(owner, [0, 0])
            c[0] += 1
            c[1] += size
        for owner, (n, total) in sorted(counts.items(), key=lambda kv: -kv[1][1]):
            print(f"{owner}\t{n}\t{total}")


if __name__ == "__main__":
    main()
