"""Compare two GCC 3.4 server_i486.so builds function by function.

For every function symbol, the disassembly is normalised so that differences caused only by
layout disappear: call and jump targets become symbol names (or offsets inside the function),
and absolute addresses become ADDR. Two functions with the same normalised body are treated
as compiled from the same source.

Usage:
    python3 -I compare_builds.py <reference.so> <candidate.so> --map <map.tsv> --out <result.tsv>

<reference.so> is the original (Hidden's) build, <candidate.so> the one we compiled; <map.tsv>
is map_functions.py output for the reference, used to attribute functions to source files.
Result columns: file, status (same / changed / only-ref / only-cand), similarity, ref size,
cand size, mangled name, demangled name.
"""
import argparse
import difflib
import hashlib
import os
import pickle
import re
import subprocess
import sys
from collections import defaultdict

HEX = re.compile(r"\b0x[0-9a-f]+\b")
TARGET = re.compile(r"^([0-9a-f]+) <([^>]+)>$")


def function_symbols(path):
    out = subprocess.run(["nm", "--defined-only", "-S", path], capture_output=True, text=True, check=True).stdout
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[2] in "tTwW":
            syms.setdefault(parts[3], (int(parts[0], 16), int(parts[1], 16)))
    return syms


def disassemble(path):
    """Return {start address: [normalised instructions]} for every symbol-labelled block in .text."""
    proc = subprocess.Popen(["objdump", "-d", "--no-show-raw-insn", "-j", ".text", path],
                            stdout=subprocess.PIPE, text=True, bufsize=1 << 20)
    blocks = {}
    cur = None
    start = 0
    for line in proc.stdout:
        if line.endswith(">:\n"):
            start = int(line.split()[0], 16)
            cur = blocks.setdefault(start, [])
            continue
        if cur is None or ":\t" not in line:
            continue
        addr_s, _, ins = line.partition(":\t")
        ins = ins.strip()
        if not ins:
            continue
        op, _, args = ins.partition(" ")
        args = args.strip()
        m = TARGET.match(args)
        if m:
            target = int(m.group(1), 16)
            name = m.group(2)
            base = name.split("+")[0]
            if "+" not in name or op.startswith("call"):
                args = f"<{base}>"
            else:
                args = f"<+{target - start:#x}>"
        else:
            args = HEX.sub(lambda h: "ADDR" if int(h.group(0), 16) > 0xFFFF else h.group(0), args)
        cur.append(f"{op} {args}")
    proc.wait()
    return blocks


def disassemble_cached(path):
    """The reference build never changes, so keep its normalised disassembly next to the work dir."""
    st = os.stat(path)
    key = hashlib.sha1(f"{os.path.abspath(path)}:{st.st_size}:{st.st_mtime_ns}".encode()).hexdigest()[:16]
    cache = os.path.join(os.path.expanduser("~/.cache/hsrebuild-disasm"), key + ".pickle")
    if os.path.exists(cache):
        with open(cache, "rb") as f:
            return pickle.load(f)
    blocks = disassemble(path)
    os.makedirs(os.path.dirname(cache), exist_ok=True)
    with open(cache + ".tmp", "wb") as f:
        pickle.dump(blocks, f)
    os.replace(cache + ".tmp", cache)
    return blocks


def body(blocks, addr, size):
    ins = blocks.get(addr)
    return ins if ins is not None else []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ref")
    ap.add_argument("cand")
    ap.add_argument("--map", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    owner = {}
    demangled = {}
    with open(args.map) as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            owner.setdefault(parts[4], parts[0])
            demangled[parts[4]] = parts[5]

    ref_syms = function_symbols(args.ref)
    cand_syms = function_symbols(args.cand)
    ref_blocks = disassemble_cached(args.ref)
    cand_blocks = disassemble(args.cand)

    rows = []
    for name, (addr, size) in ref_syms.items():
        if name in cand_syms:
            a = body(ref_blocks, addr, size)
            b = body(cand_blocks, *cand_syms[name])
            if a == b:
                status, ratio = "same", 1.0
            else:
                status = "changed"
                ratio = difflib.SequenceMatcher(None, a, b, autojunk=False).ratio() if len(a) + len(b) < 20000 else 0.0
            rows.append((owner.get(name, "?"), status, ratio, size, cand_syms[name][1], name))
        else:
            rows.append((owner.get(name, "?"), "only-ref", 0.0, size, 0, name))
    for name, (addr, size) in cand_syms.items():
        if name not in ref_syms:
            rows.append(("?", "only-cand", 0.0, 0, size, name))

    names = [r[5] for r in rows]
    dem = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout.split("\n")
    with open(args.out, "w") as f:
        for r, d in zip(rows, dem):
            f.write(f"{r[0]}\t{r[1]}\t{r[2]:.3f}\t{r[3]}\t{r[4]}\t{r[5]}\t{d}\n")

    totals = defaultdict(lambda: [0, 0])
    for r in rows:
        totals[r[1]][0] += 1
        totals[r[1]][1] += r[3] or r[4]
    for status, (n, size) in sorted(totals.items()):
        print(f"{status}\t{n} functions\t{size} bytes", file=sys.stderr)


if __name__ == "__main__":
    main()
