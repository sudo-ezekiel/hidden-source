"""Compare two GCC 3.4 server_i486.so builds function by function.

For every function symbol, the disassembly is normalised so that differences caused only by
layout disappear, and two functions with the same normalised body are treated as compiled from
the same source. There are two normalisations:

- strict (the default): the dynamic relocations say which operands are addresses (both builds
  are non-PIC with text relocations). Each one becomes a token for what it points at: a symbol
  name plus offset, or for read-only data without a symbol its content (float and double
  constants, strings, jump tables as offsets into the function); see elf_refs.py. Every operand
  no relocation covers is a constant and is kept as it is, however large. Calls to exported
  functions (R_386_PC32) are named after the callee.
- --loose: the original normalisation. Call and jump targets become symbol names (or offsets
  inside the function) and every hex number above 0xFFFF becomes ADDR, so large constants and the
  data behind addresses are not compared, and a call through an R_386_PC32 relocation reads as
  a call to the calling function itself. Kept so older result tables stay comparable.

Usage:
    python3 -I compare_builds.py <reference.so> <candidate.so> --map <map.tsv> --out <result.tsv> [--loose]

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

# python3 -I drops the script directory from sys.path; elf_refs.py sits next to this file.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elf_refs import Refs  # noqa: E402

HEX = re.compile(r"\b0x[0-9a-f]+\b")
TARGET = re.compile(r"^([0-9a-f]+) <([^>]+)>$")
NUMBER = re.compile(r"(\$?)(-?0x[0-9a-f]+)\b")
PC_THUNK = "call <__i686.get_pc_thunk.bx>"

# GCC names anonymous namespaces and file-keyed static initialisers after the source path plus two
# hashes, the second one random per compile: "_GLOBAL__N_.._dlls_x.cpp_FDF1F060_3F9FE640" in
# Hidden's build is "_GLOBAL__N__src_dlls_x.cpp_FDF1F060_A80A4E1D" in ours. Both sides are keyed
# by file name and the first hash instead, so the same function lines up under one name.
ANON_NS = re.compile(r"\d+_GLOBAL__N_[A-Za-z0-9_.]*?([A-Za-z0-9]+\.cpp)_([0-9A-F]{8})_[0-9A-F]{8}")
FILE_INIT = re.compile(r"^_GLOBAL__I_[A-Za-z0-9_.]*?([A-Za-z0-9]+\.cpp)_([0-9A-F]{8})_[0-9A-F]{8}$")


def canon(name):
    name = ANON_NS.sub(lambda m: f"_GLOBAL__N_{m.group(1)}_{m.group(2)}", name)
    return FILE_INIT.sub(lambda m: f"_GLOBAL__I_{m.group(1)}_{m.group(2)}", name)


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
            base = canon(name.split("+")[0])
            if "+" not in name or op.startswith("call"):
                args = f"<{base}>"
            else:
                args = f"<+{target - start:#x}>"
        else:
            args = HEX.sub(lambda h: "ADDR" if int(h.group(0), 16) > 0xFFFF else h.group(0), args)
        cur.append(f"{op} {args}")
    proc.wait()
    return blocks


def strict_args(refs, op, args, addr, end, func):
    """Operands of one instruction at [addr, end) with every relocated field replaced by a token."""
    rels = [off for off in refs.relocs_in(addr, end) if off + 4 <= end]
    m = TARGET.match(args)
    if m:
        is_call = op.startswith("call")
        if rels and refs.rel[rels[0]][0] == "PC32":
            target, undefined = refs.reloc_target(rels[0], end)
            if undefined is not None:
                return f"<{undefined}>"
        else:
            target = int(m.group(1), 16)
        sec = refs.section(target)
        tok = refs.symbol_token(target, None if is_call else func) if sec and sec[5] else None
        return f"<{tok if tok is not None else canon(m.group(2).split('+')[0])}>"
    if not rels:
        return args
    numbers = [[n, False] for n in NUMBER.finditer(args)]
    subs = []
    for off in rels:
        field = refs.word(off)
        hits = [h for h in numbers if not h[1] and int(h[0].group(2), 16) & 0xFFFFFFFF == field]
        if not hits:
            subs.append((len(args), len(args), " {" + refs.reloc_token(off, op, args, func=func) + "}"))
            continue
        # The immediate is the last field of an instruction, after any displacement.
        imm = [h for h in hits if h[0].group(1)]
        mem = [h for h in hits if not h[0].group(1)]
        hit = imm[0] if imm and (off == end - 4 or not mem) else (mem or imm)[0]
        hit[1] = True
        n = hit[0]
        is_imm = bool(n.group(1))
        indexed = not is_imm and args[n.end():n.end() + 1] == "("
        tok = refs.reloc_token(off, op, args, imm=is_imm, indexed=indexed, func=func)
        subs.append((n.start(), n.end(), n.group(1) + tok))
    for start, stop, text in sorted(subs, reverse=True):
        args = args[:start] + text + args[stop:]
    return args


def disassemble_strict(path, refs, start_address=None, stop_address=None):
    """Like disassemble(), with relocated operands named by strict_args() and other numbers kept."""
    cmd = ["objdump", "-d", "--no-show-raw-insn", "-j", ".text"]
    if start_address is not None:
        cmd += [f"--start-address={start_address}", f"--stop-address={stop_address}"]
    proc = subprocess.Popen(cmd + [path], stdout=subprocess.PIPE, text=True, bufsize=1 << 20)
    blocks = {}
    cur = None
    func = None
    pending = None  # (block, address, op, args, func) waiting for the next address as its end

    def flush(end):
        block, addr, op, args, fn = pending
        block.append(f"{op} {strict_args(refs, op, args, addr, end, fn)}")

    for line in proc.stdout:
        if line.endswith(">:\n"):
            start = int(line.split()[0], 16)
            cur = blocks.setdefault(start, [])
            sym = refs.symbol_at(start)
            func = (start, sym[2] if sym and sym[1] == start and sym[2] else 0)
            continue
        if cur is None or ":\t" not in line:
            continue
        addr_s, _, ins = line.partition(":\t")
        ins = ins.strip()
        if not ins:
            continue
        addr = int(addr_s, 16)
        if pending is not None:
            flush(addr)
        op, _, args = ins.partition(" ")
        pending = (cur, addr, op, args.strip(), func)
    if pending is not None:
        text = next(s for s in refs.secs if s[3] == ".text")
        flush(min(text[1], pending[1] + 16) if stop_address is None else stop_address)
    proc.wait()
    # The C runtime's PIC helpers address the GOT through %ebx; those offsets depend on layout.
    for start, ins in blocks.items():
        if PC_THUNK in ins:
            blocks[start] = [HEX.sub(lambda h: "ADDR" if int(h.group(0), 16) > 0xFFFF else h.group(0), i) for i in ins]
    return blocks


def disassemble_cached(path, strict=True):
    """The reference build never changes, so keep its normalised disassembly next to the work dir."""
    st = os.stat(path)
    version = "v2"
    if strict:  # the strict tokens still change as the tool learns; key them by its source too
        here = os.path.dirname(os.path.abspath(__file__))
        tools = b"".join(open(os.path.join(here, n), "rb").read() for n in ("compare_builds.py", "elf_refs.py"))
        version = "v3-strict-" + hashlib.sha1(tools).hexdigest()[:12]
    key = hashlib.sha1(f"{version}:{os.path.abspath(path)}:{st.st_size}:{st.st_mtime_ns}".encode()).hexdigest()[:16]
    cache = os.path.join(os.path.expanduser("~/.cache/hsrebuild-disasm"), key + ".pickle")
    if os.path.exists(cache):
        with open(cache, "rb") as f:
            return pickle.load(f)
    blocks = disassemble_strict(path, Refs(path, canon)) if strict else disassemble(path)
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
    ap.add_argument("--loose", action="store_true", help="the original normalisation (hex above 0xFFFF is ADDR)")
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
    ref_blocks = disassemble_cached(args.ref, strict=not args.loose)
    cand_blocks = disassemble(args.cand) if args.loose else disassemble_strict(args.cand, Refs(args.cand, canon))

    # Pair functions by canonical name; rows keep the reference's own name.
    cand_by_canon = {canon(n): n for n in cand_syms}
    ref_canon = {canon(n) for n in ref_syms}
    rows = []
    for name, (addr, size) in ref_syms.items():
        cname = cand_by_canon.get(canon(name))
        if cname is not None:
            a = body(ref_blocks, addr, size)
            b = body(cand_blocks, *cand_syms[cname])
            if a == b:
                status, ratio = "same", 1.0
            else:
                status = "changed"
                ratio = difflib.SequenceMatcher(None, a, b, autojunk=False).ratio() if len(a) + len(b) < 20000 else 0.0
            rows.append((owner.get(name, "?"), status, ratio, size, cand_syms[cname][1], name))
        else:
            rows.append((owner.get(name, "?"), "only-ref", 0.0, size, 0, name))
    for name, (addr, size) in cand_syms.items():
        if canon(name) not in ref_canon:
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
