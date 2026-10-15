"""Show how one function differs between two builds, after the same normalisation compare_builds uses.

Usage:
    python3 -I fn_diff.py <reference.so> <candidate.so> <name> [--raw] [--loose]

<name> is a mangled symbol or a substring of the demangled name (the first unique match is used;
all matches are listed when there are several). --raw prints both disassemblies without diffing.
The strict normalisation is the default, so a changed constant shows as its value (f32:62,
"string", $0x46004003, a symbol name); --loose shows the original one, with ADDR for every hex
number above 0xFFFF.
"""
import argparse
import difflib
import re
import subprocess
import sys

sys.path.insert(0, __import__("os").path.dirname(__file__))
from compare_builds import canon, disassemble_strict, function_symbols, HEX, TARGET  # noqa: E402
from elf_refs import Refs  # noqa: E402

MANGLED = re.compile(r"\b_Z[\w.]+")


def strict(path, addr, size):
    """compare_builds' strict lines for one function, demangled for reading."""
    lines = disassemble_strict(path, Refs(path, canon), addr, addr + size).get(addr, [])
    lines = [f"{op:8} {args}" for op, _, args in (line.partition(" ") for line in lines)]
    # Demangle the names only: c++filt run over whole lines also eats the "$" of an immediate.
    names = sorted({m for line in lines for m in MANGLED.findall(line)})
    out = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout.split("\n")
    plain = dict(zip(names, out))
    return [MANGLED.sub(lambda m: plain.get(m.group(0), m.group(0)), line) for line in lines]


def normalised(path, addr, size):
    out = subprocess.run(["objdump", "-d", "--no-show-raw-insn", "-C", f"--start-address={addr}",
                          f"--stop-address={addr + size}", path], capture_output=True, text=True).stdout
    lines = []
    for line in out.splitlines():
        if ":\t" not in line or line.endswith(">:"):
            continue
        _, _, ins = line.partition(":\t")
        ins = ins.strip()
        op, _, args = ins.partition(" ")
        args = args.strip()
        m = TARGET.match(args)
        if m:
            target, name = int(m.group(1), 16), m.group(2)
            args = f"<{name.split('+')[0]}>" if ("+" not in name or op.startswith("call")) else f"<+{target - addr:#x}>"
        else:
            args = HEX.sub(lambda h: "ADDR" if int(h.group(0), 16) > 0xFFFF else h.group(0), args)
        lines.append(f"{op:8} {args}")
    return lines


def resolve(syms, want):
    if want in syms:
        return want
    names = list(syms)
    dem = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout.split("\n")
    hits = [n for n, d in zip(names, dem) if want in d]
    if len(hits) != 1:
        for n, d in zip(names, dem):
            if want in d:
                print(f"  {n}  {d}", file=sys.stderr)
        sys.exit(f"{len(hits)} matches for {want!r}" if hits else f"no match for {want!r}")
    return hits[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ref")
    ap.add_argument("cand")
    ap.add_argument("name")
    ap.add_argument("--raw", action="store_true")
    ap.add_argument("--loose", action="store_true", help="the original normalisation (hex above 0xFFFF is ADDR)")
    args = ap.parse_args()
    norm = normalised if args.loose else strict
    rs, cs = function_symbols(args.ref), function_symbols(args.cand)
    name = resolve(rs, args.name)
    a = norm(args.ref, *rs[name])
    if name not in cs:
        print("\n".join(a))
        sys.exit(f"{name} is not in the candidate build")
    b = norm(args.cand, *cs[name])
    if args.raw:
        print("== reference\n" + "\n".join(a) + "\n== candidate\n" + "\n".join(b))
        return
    if a == b:
        print(f"{name}: identical ({len(a)} instructions)")
        return
    print(f"{name}: reference {len(a)} instructions, candidate {len(b)}")
    sys.stdout.writelines(l + "\n" for l in difflib.unified_diff(a, b, "reference", "candidate", n=3, lineterm=""))


if __name__ == "__main__":
    main()
