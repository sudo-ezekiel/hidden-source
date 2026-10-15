"""Show how one function differs between two builds, after the same normalisation compare_builds uses.

Usage:
    python3 -I fn_diff.py <reference.so> <candidate.so> <name> [--raw]

<name> is a mangled symbol or a substring of the demangled name (the first unique match is used;
all matches are listed when there are several). --raw prints both disassemblies without diffing.
"""
import argparse
import difflib
import subprocess
import sys

sys.path.insert(0, __import__("os").path.dirname(__file__))
from compare_builds import function_symbols, HEX, TARGET  # noqa: E402


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
    args = ap.parse_args()
    rs, cs = function_symbols(args.ref), function_symbols(args.cand)
    name = resolve(rs, args.name)
    a = normalised(args.ref, *rs[name])
    if name not in cs:
        print("\n".join(a))
        sys.exit(f"{name} is not in the candidate build")
    b = normalised(args.cand, *cs[name])
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
