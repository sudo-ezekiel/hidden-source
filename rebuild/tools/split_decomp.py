"""Split a DumpAll.java decompile of server_i486.so into one file per original source file.

Ghidra loads the .so at image base 0x10000, so a Ghidra address is the nm address + 0x10000.
map_functions.py output says which source file each nm address came from.

Usage:
    python3 -I split_decomp.py <decomp.c> <map.tsv> <out dir> [--base 0x10000]
"""
import argparse
import os
import re

HEADER = re.compile(r"^//==== FUNC ([0-9a-f]+) ")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("decomp")
    ap.add_argument("map")
    ap.add_argument("out")
    ap.add_argument("--base", default="0x10000")
    args = ap.parse_args()
    base = int(args.base, 16)

    owner = {}
    with open(args.map) as f:
        for line in f:
            parts = line.split("\t")
            owner.setdefault(int(parts[1], 16), parts[0])

    os.makedirs(args.out, exist_ok=True)
    chunks = {}
    current = None
    with open(args.decomp, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = HEADER.match(line)
            if m:
                addr = int(m.group(1), 16) - base
                current = chunks.setdefault(owner.get(addr, "_unmapped"), [])
            if current is not None:
                current.append(line)
    for name, lines in chunks.items():
        safe = name.replace("/", "_")
        with open(os.path.join(args.out, safe + ".c"), "w") as f:
            f.writelines(lines)
    print(f"{len(chunks)} files written to {args.out}")


if __name__ == "__main__":
    main()
