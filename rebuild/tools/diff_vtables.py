"""Compare the vtables of one or more classes between two GCC 3.4 builds.

A GCC vtable symbol _ZTV<class> starts with offset-to-top and the typeinfo pointer; the virtual
function pointers follow. Each slot is resolved to the function symbol at that address, so a
virtual added, removed or moved shows up as a slot whose name differs.

Usage:
    python3 -I diff_vtables.py <reference.so> <candidate.so> <class> [<class> ...]
    python3 -I diff_vtables.py <reference.so> <candidate.so> --all   (summary for every class)
"""
import argparse
import struct
import subprocess

from dump_datamaps import Elf


def relocations(path):
    """Slot address -> symbol name for relocations that carry a symbol (the slot itself stores 0)."""
    out = subprocess.run(["readelf", "-rW", path], capture_output=True, text=True, check=True).stdout
    rel = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 5 and parts[2] in ("R_386_32", "R_386_GLOB_DAT"):
            try:
                rel[int(parts[0], 16)] = parts[4]
            except ValueError:
                pass
    return rel


def load(path):
    out = subprocess.run(["nm", "--defined-only", "-S", path], capture_output=True, text=True, check=True).stdout
    funcs = {}
    vtables = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) != 4:
            continue
        addr, size, kind, name = int(parts[0], 16), int(parts[1], 16), parts[2], parts[3]
        if kind in "tTwW":
            funcs.setdefault(addr, name)
        elif name.startswith("_ZTV"):
            vtables[name[4:]] = (addr, size)
    return Elf(path), funcs, vtables, relocations(path)


def slots(elf, funcs, vt, rel):
    addr, size = vt
    raw = elf.read(addr, size) or b""
    words = struct.unpack(f"<{len(raw) // 4}I", raw[: len(raw) // 4 * 4])
    # Primary vtable only: stop at the next offset-to-top/typeinfo pair (secondary vtables).
    out = []
    for i, w in enumerate(words[2:]):
        slot = addr + 8 + i * 4
        if slot in rel:
            out.append(rel[slot])
        elif w in funcs:
            out.append(funcs[w])
        elif w == 0:
            out.append("<null>")
        else:
            break
    return out


def demangle(names):
    res = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True, check=True)
    return res.stdout.split("\n")[: len(names)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ref")
    ap.add_argument("cand")
    ap.add_argument("classes", nargs="*")
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args()
    ref = load(args.ref)
    cand = load(args.cand)
    if args.all:
        names = sorted(set(ref[2]) & set(cand[2]))
        for cls in names:
            a, b = slots(ref[0], ref[1], ref[2][cls], ref[3]), slots(cand[0], cand[1], cand[2][cls], cand[3])
            if a != b:
                first = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
                print(f"{cls}\t{len(a)}\t{len(b)}\tfirst difference at slot {first}")
        return
    for cls in args.classes:
        a, b = slots(ref[0], ref[1], ref[2][cls], ref[3]), slots(cand[0], cand[1], cand[2][cls], cand[3])
        da, db = demangle(a), demangle(b)
        print(f"== {cls}: reference {len(a)} slots, candidate {len(b)} slots")
        import difflib
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
            if tag == "equal":
                continue
            for i in range(i1, i2):
                print(f"  - ref slot {i} (+{(i) * 4:#x}): {da[i]}")
            for j in range(j1, j2):
                print(f"  + cand slot {j} (+{(j) * 4:#x}): {db[j]}")


if __name__ == "__main__":
    main()
