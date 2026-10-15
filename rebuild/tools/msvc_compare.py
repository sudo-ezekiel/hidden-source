"""Compare our MSVC client.dll with Hidden's, function by function.

Hidden's client.dll has no symbols, so its functions are first mapped to ours, then each mapped
pair is compared after the normalisation compare_builds.py uses for the server: branch targets
inside the function become offsets, calls and jumps out of it become names. Jump and index tables
that MSVC places after a function's code are cut off and compared as in-function offsets.
Operands, two ways:
- strict (the default): the PE base relocations say which operands are addresses; each becomes a
  token for what it points at (Tokens: a mapped function, a vftable, an import slot, writable data
  paired through the mapping, read-only data by its content, pe_refs.py), and every other number
  is a constant and stays. After the mapping, refine() pairs again functions the strict tokens
  show were swapped (STRICT-COMPARE.md section 6).
- --loose: the original normalisation, addresses inside the image become ADDR (import table
  slots IMP:<name>), .rdata is not read. Its result is the same as before the strict mode.
The mapping steps below are the same in both modes and never use the strict tokens; only the
strict mode's refine() changes pairs after them.

Functions: ours from the /MAP file; the reference's from every vftable slot, direct call target
and import thunk, plus the Ghidra decompile's starts (decomp.c) and direct jmp targets (tail calls)
where the code before them cannot fall through and they are not inside a jump table. objdump's
linear sweep is redone from any start it ran past. After a first mapping, the starts it implies
are added (where our branch goes to a function start, or our body is a strict prefix of the
reference's) and the mapping runs again.
Mapping, each step repeated until nothing changes:
- vftable: classes whose vftable (same class, same subobject offset) has the same slot count in
  both binaries, slot by slot (RTTI, pe_rtti.py); the export and the entry point;
- names: names.tsv names of confidence high or medium that are unique among our functions;
- callgraph: a mapped pair whose bodies line up (difflib on the bodies with every external target
  masked) maps the call and jump targets and the function pointers at the same positions;
  a target is taken when both sides agree and no other candidate comes close;
- order: between two mapped functions that are in the same order in both binaries, unmapped
  functions with the same masked body, then remaining ones in order when the counts agree and
  the bodies are at least 60% alike (objects are linked in project order, and functions within
  an object in the compiler's order);
- string: the same sequence of string literals, unique on both sides;
- body: the same masked body (8 instructions or more), unique on both sides.

With --ref-procs (a PDB's function list: hex address, size, G/L, module, undecorated name without
arguments, as procs.tsv for Hidden's May server.dll), the reference's functions are the PDB's, and
they are paired with ours by name before any other step: our map names are undecorated and cut to
the same form (no arguments, no spaces, no class/struct keywords). A name that is unique on both
sides pairs at once; overloads and same-named statics ($E5 initialisers) pair by object, then by
identical masked body, then by the most similar body.
Without a decompile (the August server.dll has neither PDB nor decompile), the reference's starts
are the strong ones plus every code address .rdata or .data points at and every 16-byte aligned
address after int3 padding, kept where the code before cannot fall through.

--ref-names takes names for some of the reference's functions in procs.tsv's columns (the size is
not used) and pairs by them the same way, without the PDB's extents: names carried over from the
May DLL to a later one. --ours-procs makes the third argument a procs.tsv instead of a map file, so
that one reference DLL can be mapped to another that has a PDB (the August DLL to the May DLL);
the result's "our name" column then holds the PDB's names.

Usage:
    python3 -I msvc_compare.py <reference.dll> <our dll> <our map> [<decomp dir>]
        --out <result.tsv> [--ref-procs procs.tsv] [--ref-names names.tsv] [--ours-procs] [--show NAME]
        [--loose]

Result columns: object (lib:object from the map, ours), status (same / changed / unmapped /
only-ref / only-ours), similarity, ref size, our size, our name (mangled), demangled name,
ref address, our address, how it was mapped, the names.tsv name. Unmapped reference functions are
only-ref when they sit in a vftable of a class our build does not have; our unmapped functions are
only-ours when they sit in a vftable of a class Hidden's build does not have. Sizes are the code
up to the next function, without trailing int3 padding. --show prints the normalised diff of the
functions whose our name contains NAME, or of the reference function at a hex address (several,
comma separated).
"""
import argparse
import bisect
import difflib
import hashlib
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_refs  # noqa: E402
import pe_rtti  # noqa: E402
from pe_seeds import relocations, section_of  # noqa: E402
from vtable_names import msvc_simple_demangle  # noqa: E402

HEX = re.compile(r"0x[0-9a-f]+")
BRANCH = re.compile(r"^0x([0-9a-f]+)$")
MEMBASE = re.compile(r"(?<![\$\w])0x([0-9a-f]+)\(")
PAD_OPS = {"int3", "nop", "xchg %ax,%ax", "lea 0x0(%esi),%esi", "lea 0x0(%esi,%eiz,1),%esi",
           "lea 0x0(%edi),%edi", "mov %edi,%edi"}


class Image:
    """One PE: disassembly of .text, import slots, function starts and names."""

    def __init__(self, path):
        self.path = path
        self.pe = pe_rtti.Pe(path)
        self.lo = self.pe.base
        self.hi = max(s[1] + s[2] for s in self.pe.sections)
        text = self.pe.section(".text")
        self.text = (text[1], text[1] + text[2])
        self.addrs, self.ins = disassemble(path)
        self.index = {a: i for i, a in enumerate(self.addrs)}
        self.imports = imports(path, self.lo)
        self.starts = []
        self.names = {}
        self._rtti = self._refs = None

    def rtti(self):
        if self._rtti is None:
            self._rtti = pe_rtti.rtti(self.pe)
        return self._rtti

    def refs(self):
        """Base relocations and read-only content (pe_refs.py), read on first use."""
        if self._refs is None:
            self._refs = pe_refs.Refs(self.pe)
        return self._refs

    def finish(self, starts):
        starts = sorted(a for a in set(starts) if self.text[0] <= a < self.text[1])
        self.resync([a for a in starts if a not in self.index], starts)
        self.starts = sorted(a for a in starts if a in self.index)
        self.start_set = set(self.starts)

    def resync(self, lost, starts):
        """objdump's linear sweep can run through a function start (after a jump table, say):
        disassemble again from each such start up to the next one and splice the result in."""
        for a in lost:
            i = bisect.bisect_right(starts, a)
            stop = starts[i] if i < len(starts) else self.text[1]
            out = subprocess.run(["objdump", "-d", "--no-show-raw-insn", "-j", ".text", f"--start-address={a:#x}",
                                  f"--stop-address={stop:#x}", self.path], capture_output=True, text=True).stdout
            addrs, ins = parse_disassembly(out.splitlines(True))
            lo, hi = bisect.bisect_left(self.addrs, a), bisect.bisect_left(self.addrs, stop)
            self.addrs[lo:hi], self.ins[lo:hi] = addrs, ins
        if lost:
            self.index = {x: i for i, x in enumerate(self.addrs)}

    def extent(self, addr):
        i = bisect.bisect_right(self.starts, addr)
        end = self.starts[i] if i < len(self.starts) else self.text[1]
        return addr, end

    def owner(self, addr):
        i = bisect.bisect_right(self.starts, addr) - 1
        return self.starts[i] if i >= 0 else None


def disassemble(path):
    proc = subprocess.Popen(["objdump", "-d", "--no-show-raw-insn", "-j", ".text", path],
                            stdout=subprocess.PIPE, text=True, bufsize=1 << 20)
    addrs, ins = parse_disassembly(proc.stdout)
    proc.wait()
    return addrs, ins


def parse_disassembly(lines):
    addrs, ins = [], []
    for line in lines:
        if ":\t" not in line:
            continue
        a, _, i = line.partition(":\t")
        i = " ".join(i.split())
        if not i:
            continue
        try:
            addrs.append(int(a, 16))
        except ValueError:
            continue
        ins.append(i)
    return addrs, ins


def imports(path, base):
    """IAT slot address -> imported name."""
    out = subprocess.run(["objdump", "-p", path], capture_output=True, text=True, check=True).stdout
    slots = {}
    dll = None
    for line in out.splitlines():
        if "DLL Name:" in line:
            dll = line.split("DLL Name:")[1].strip()
            continue
        parts = line.split()
        if dll and len(parts) >= 4 and parts[1] == "<none>":
            try:
                slots[base + int(parts[0], 16)] = parts[3]
            except ValueError:
                pass
    return slots


def read_map(path):
    """Our functions: address -> (name, lib:object), from the /MAP file (publics and statics)."""
    funcs = {}
    entry = None
    with open(path, errors="replace") as f:
        for line in f:
            parts = line.split()
            if line.startswith(" entry point at"):
                entry = parts[-1]
            if len(parts) < 4 or not parts[0].startswith("0001:") or parts[1].startswith("."):
                continue
            try:
                va = int(parts[2], 16)
            except ValueError:
                continue
            # A vector deleting destructor (??_E) that shares the scalar one's body (??_G) at one
            # address: the PDB names it after the scalar one.
            if va not in funcs or (funcs[va][0].startswith("??_E") and parts[1].startswith("??_G")):
                funcs[va] = (parts[1], parts[-1])
    return funcs


def ref_starts(decomp, img):
    starts = set()
    with open(os.path.join(decomp, "decomp.c"), errors="replace") as f:
        for line in f:
            if line.startswith("//==== FUNC "):
                starts.add(int(line.split()[2], 16))
    return starts


def read_procs(path):
    """A PDB's functions: address -> (name, module, size), without the zero-size labels ($$$00001)."""
    out = {}
    with open(path, errors="replace") as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            if len(p) < 5 or not int(p[1]):
                continue
            out.setdefault(int(p[0], 16), (p[4], p[3], int(p[1])))
    return out


def pointer_seeds(img):
    """Code addresses that .rdata or .data point at (vftables, function tables, callbacks)."""
    out = set()
    for site in relocations(img.pe):
        t = img.pe.u32(site)
        if t is not None and img.text[0] <= t < img.text[1] and section_of(img.pe, site) in (".rdata", ".data"):
            out.add(t)
    return out


def padding_starts(img):
    """16-byte aligned addresses that end a run of int3 padding: objdump's linear sweep decodes an
    int3 right before them (a 0xcc byte alone can be the end of an address operand)."""
    d, out = img.pe.data, set()
    for a in range((img.text[0] + 15) & ~15, img.text[1], 16):
        o = img.pe.off(a)
        i = img.index.get(a)
        if o and d[o] != 0xCC and i and img.ins[i - 1] == "int3":
            out.add(a)
    return out


CALLCONV = re.compile(r"__(?:thiscall|cdecl|stdcall|fastcall) ")
KEYWORDS = re.compile(r"\b(?:class|struct|union|enum) ")
QUALIFIERS = (" const", " volatile")


def undecorated_parts(sym, und):
    """Our map symbol and its llvm-undname text -> (name, arguments with qualifiers), both in the
    PDB's spelling except for spaces; (None, None) when the text does not parse. C names lose
    their leading underscore and @N suffix."""
    if not sym.startswith("?"):
        s = re.sub(r"@\d+$", "", sym)
        return (s[1:] if s[:1] in ("_", "@") else s), ""
    if und == sym:
        return None, None
    start = None
    for m in CALLCONV.finditer(und):
        pre = und[:m.start()]
        if pre.count("(") == pre.count(")") and pre.count("<") == pre.count(">"):
            start = m.end()
            break
    if start is None:
        return None, None
    rest, tail = und[start:], ""
    while rest.endswith(QUALIFIERS):
        q = next(x for x in QUALIFIERS if rest.endswith(x))
        tail = q + tail
        rest = rest[:-len(q)]
    if not rest.endswith(")"):
        return None, None
    depth = 0
    for i in range(len(rest) - 1, -1, -1):
        depth += {")": 1, "(": -1}.get(rest[i], 0)
        if depth == 0:
            break
    name, args = rest[:i].replace(" dtor'", " destructor'"), rest[i:] + tail
    return KEYWORDS.sub("", name), KEYWORDS.sub("", args)


def name_key(name):
    """A function name without arguments, as procs.tsv or undecorated_parts() spell it -> the key
    both sides are paired by. Conversion operators lose const (the PDB writes it after the type)."""
    k = name.replace(" ", "")
    if re.search(r"operator[A-Za-z_]", k) and not re.search(r"operator(new|delete)", k):
        k = k.replace("const", "")
    return k


def pair_by_name(m, ref_names, our_names):
    """Pair reference and our functions with the same name_key(): a name unique on both sides at
    once; within a name, by object, then by identical masked body, then by the most similar body."""
    gr, go = defaultdict(list), defaultdict(list)
    for a, (name, mod, *_) in ref_names.items():
        if a in m.ref.img.start_set:
            gr[name_key(name)].append((a, mod.lower()))
    for a, (name, mod) in our_names.items():
        if name is not None:
            go[name_key(name)].append((a, mod.lower()))
    n = 0
    for key, rs in gr.items():
        os_ = go.get(key)
        if not os_:
            continue
        if len(rs) == 1 and len(os_) == 1:
            n += m.add(rs[0][0], os_[0][0], "pdbname")
            continue
        for mod in {x for _, x in rs}:
            r1 = [a for a, x in rs if x == mod]
            o1 = [a for a, x in os_ if x == mod]
            if len(r1) == 1 and len(o1) == 1:
                n += m.add(r1[0], o1[0], "pdbname")
        rl = [a for a, _ in rs if a not in m.r2o]
        ol = [a for a, _ in os_ if a not in m.o2r]
        if not rl or not ol:
            continue
        kr = {a: masked(m.ref.get(a)[0]) for a in rl}
        ko = {b: masked(m.ours.get(b)[0]) for b in ol}
        for a in rl:
            same = [b for b in ol if ko[b] == kr[a]]
            if len(same) == 1 and sum(kr[x] == kr[a] for x in rl) == 1:
                n += m.add(a, same[0], "pdbname")
        rl = [a for a in rl if a not in m.r2o]
        ol = [b for b in ol if b not in m.o2r]
        if len(rl) == 1 and len(ol) == 1:
            n += m.add(rl[0], ol[0], "pdbname")
            continue
        if not rl or not ol or len(rl) * len(ol) > 400:
            continue
        scored = [(difflib.SequenceMatcher(None, kr[a], ko[b], autojunk=False).ratio(), a, b)
                  for a in rl for b in ol if len(kr[a]) + len(ko[b]) < 6000]
        for ratio, a, b in sorted(scored, reverse=True):
            if ratio >= 0.5:
                n += m.add(a, b, "pdbname")
    return n


def branch_targets(img):
    """Direct call targets plus import thunks (jmp *[IAT slot]), and direct jmp targets (some are
    tail calls)."""
    calls, jumps = set(), set()
    for a, i in zip(img.addrs, img.ins):
        op, _, args = i.partition(" ")
        m = BRANCH.match(args)
        if m and op in ("call", "jmp"):
            (calls if op == "call" else jumps).add(int(m.group(1), 16))
        elif op == "jmp" and BRANCH.match(args[1:]) and int(args[1:], 16) in img.imports:
            calls.add(a)
    return calls, jumps


def after_terminator(img, a):
    """True when the code before a cannot run into it: ret, jmp, int3 padding, or a jump table."""
    i = img.index[a] - 1
    while i >= 0 and img.ins[i] in PAD_OPS and img.ins[i] != "int3":
        i -= 1  # nop padding (nvtc.lib's objects), back to the instruction before it
    if i >= 0 and img.ins[i].split(" ", 1)[0] in ("ret", "jmp", "int3"):
        return True
    o = img.pe.off(a)
    if o and img.pe.data[o - 1] == 0xCC:
        return True
    # after a jump table, but not inside one (whose entries point back into the function before)
    prev, cur = img.pe.u32(a - 4), img.pe.u32(a)
    return (prev is not None and img.text[0] <= prev < img.text[1]
            and not (cur is not None and a - 0x10000 <= cur < a))


def body(img, start):
    """[(op, args, external target or None, [function pointer immediates], address, branch target,
    objdump's args, address of the next instruction)], tables, size. args is None for a branch out
    of the function."""
    lo, end = img.extent(start)
    i = img.index[lo]
    rows = []
    while i < len(img.addrs) and img.addrs[i] < end:
        rows.append((img.addrs[i], img.ins[i], img.addrs[i + 1] if i + 1 < len(img.addrs) else img.text[1]))
        i += 1
    # Jump and index tables follow the code: cut at the first in-function table address.
    tables = []
    for a, ins, _ in rows:
        for m in MEMBASE.finditer(ins):
            t = int(m.group(1), 16)
            if lo <= t < end:
                tables.append((t, ins.startswith("jmp")))
    cut = min((t for t, _ in tables), default=end)
    rows = [r for r in rows if r[0] < cut]
    while rows and rows[-1][1] in PAD_OPS:
        rows.pop()
    size = (rows[-1][0] - lo + 1) if rows else 0
    if rows:
        nxt = img.index[rows[-1][0]] + 1
        size = (img.addrs[nxt] if nxt < len(img.addrs) else end) - lo
    out = []
    for a, ins, nxt in rows:
        op, _, args = ins.partition(" ")
        target = None
        m = BRANCH.match(args)
        if m and (op.startswith("j") or op.startswith("call") or op.startswith("loop")):
            t = int(m.group(1), 16)
            if lo <= t < end:
                out.append((op, f"<+{t - lo:#x}>", None, [], a, t, args, nxt))
                continue
            target = t
            out.append((op, None, target, [], a, t, args, nxt))
            continue
        ptrs = []

        def sub(h):
            v = int(h.group(0), 16)
            if img.lo <= v < img.hi:
                if v in img.imports:
                    return "IMP:" + img.imports[v]
                if img.text[0] <= v < img.text[1]:
                    ptrs.append(v)
                return "ADDR"
            return h.group(0)
        out.append((op, HEX.sub(sub, args), None, ptrs, a, None, args, nxt))
    tabs = []
    for t, is_jump in sorted(set(tables)):
        if t >= end:
            continue
        if is_jump:
            offs = []
            p = t
            while len(offs) < 1024:
                v = img.pe.u32(p)
                if v is None or not lo <= v < cut:
                    break
                offs.append(f"{v - lo:#x}")
                p += 4
            tabs.append("TABLE " + ",".join(offs))
        else:
            nxt = min([x for x, _ in tables if x > t] + [end])
            o = img.pe.off(t)
            raw = img.pe.data[o:o + (nxt - t)].rstrip(b"\xcc")
            tabs.append("BYTES " + raw.hex())
    return out, tabs, size


class Side:
    def __init__(self, img):
        self.img = img
        self.cache = {}

    def get(self, a):
        if a not in self.cache:
            self.cache[a] = body(self.img, a)
        return self.cache[a]


def masked(rows):
    return [f"{x[0]} {'EXT' if x[1] is None else x[1]}" for x in rows]


def labelled(rows, label):
    return [f"{x[0]} {label(x[2]) if x[1] is None else x[1]}" for x in rows]


class Tokens:
    """Strict tokens for one side's address operands (--loose leaves them out): what a relocated
    field points at, named so that both builds agree whatever their layout.

    code             a mapped function: the label the branches get (ours: our name; the
                     reference's: the name of the function it is mapped to); CODE for code that is
                     not a mapped function start (on the reference's side often a function the
                     extents without a PDB merged into the one before), so that two such
                     pointers compare as the loose mode does; in-function tables TABLE#<n>, other
                     in-function addresses <+offset>
    import slot      IMP:<name>
    vftable          vftable:<class>@<subobject offset>, plus +offset into it
    type descriptor  TD:<decorated class name> (dynamic_cast, typeid)
    writable data    data:<name> through data_name(), ADDR when it has none
    read-only data   its content (pe_refs.py)"""

    def __init__(self, img, label, data_name, mapped):
        self.img, self.label, self.data_name, self.mapped = img, label, data_name, mapped
        self.refs = img.refs()
        r = img.rtti()
        self.vft = {}
        for v in r["vtables"]:
            name = f"vftable:{ANON.sub('?A', v['class'])}@{v['offset']:#x}"
            for k in range(len(v["slots"])):
                self.vft.setdefault(v["vftable"] + 4 * k, name + (f"+{4 * k:#x}" if k else ""))
        self.tds = {a: "TD:" + ANON.sub("?A", raw) for a, raw in r["tds"].items()}

    def token(self, t, op="", args="", imm=True, indexed=False, depth=0, func=None):
        img = self.img
        if func is not None and func[0] <= t < func[1]:
            if t in func[2]:
                return f"TABLE#{func[2].index(t)}"
            return f"<+{t - func[0]:#x}>"
        if img.text[0] <= t < img.text[1]:
            return self.label(t) if t in self.mapped else "CODE"
        if t in img.imports:
            return "IMP:" + img.imports[t]
        if t in self.vft:
            return self.vft[t]
        if t in self.tds:
            return self.tds[t]
        sec = self.refs.section(t)
        if sec is None:
            return f"?{t:#x}"
        if sec[5] & pe_refs.SCN_WRITE:
            return self.data_name(t)
        return self.refs.content_token(t, op, args, imm, indexed, depth,
                                       lambda x, depth=0: self.token(x, depth=depth, func=func))

    def lines(self, start, rows):
        """The rows as labelled() gives them, with every relocated operand a token and every
        other number literal."""
        lo, end = self.img.extent(start)
        tables = sorted({int(m.group(1), 16) for x in rows if x[1] is not None and x[5] is None
                         for m in MEMBASE.finditer(x[6]) if lo <= int(m.group(1), 16) < end})
        func = (lo, end, tables)

        def token(t, op, args, imm, indexed):
            return self.token(t, op, args, imm, indexed, 0, func)
        out = []
        for x in rows:
            if x[1] is None:
                out.append(f"{x[0]} {self.label(x[2])}")
            elif x[5] is not None:
                out.append(f"{x[0]} {x[1]}")
            else:
                out.append(f"{x[0]} " + self.refs.operands(x[0], x[6], x[4], x[7], token))
        return out


def refine(m, tr, to, name_data, rounds=8):
    """Strict mode only. Functions the loose normalisation cannot tell apart (GetPanelClassName,
    GetRecvTable and the factories of many classes, the $E initialisers and destructors of one
    object) are paired by order or body, and the strict tokens show where two were swapped: the
    masked bodies are equal, the strict lines differ. Each round names the writable data without
    the votes of such suspect pairs that are not sure (STRENGTH below 2), maps the
    functions tables point at (pair_tables), then pairs again, among the suspect pairs and the
    unmapped functions with the same masked body as a suspect pair or as an unmapped function on
    the other side, those whose strict lines are equal: unique on both sides, or as many on both
    sides and all mapped already (in address order; any order scores the same, and an unmapped
    function is not paired by a guess); one left on each side of a group pairs too. Returns the
    number of pairs made."""

    def suspects():
        """Pairs whose masked bodies are equal and whose strict lines differ."""
        out = {}
        for r, o in m.r2o.items():
            a, ta, _ = m.ref.get(r)
            b, tb, _ = m.ours.get(o)
            ma = masked(a)
            if ma == masked(b):
                sa, sb = "\n".join(tr.lines(r, a) + ta), "\n".join(to.lines(o, b) + tb)
                if sa != sb:
                    out[r] = ("\n".join(ma), sa, sb)
        return out

    def weak(pairs):
        return {r for r in pairs if strength(m, r) < 2}

    total = 0
    name_data()
    for _ in range(rounds):
        name_data(weak(suspects()))
        tables = pair_tables(m)
        total += tables
        groups = defaultdict(lambda: (defaultdict(list), defaultdict(list), [], []))
        for r, (key, sa, sb) in suspects().items():
            g = groups[key]
            g[0][sa].append(r)
            g[1][sb].append(m.r2o[r])
            g[2].append(r)
            g[3].append(m.r2o[r])
        unmapped = defaultdict(lambda: ([], []))
        for side, k in ((m.ref, 0), (m.ours, 1)):
            for x in m.unmapped(side):
                unmapped["\n".join(masked(side.get(x)[0]))][k].append(x)
        for key, (rs, os_) in unmapped.items():
            if (rs and os_) or key in groups:
                g = groups[key]
                for r in rs:
                    a, ta, _ = m.ref.get(r)
                    g[0]["\n".join(tr.lines(r, a) + ta)].append(r)
                for o in os_:
                    b, tb, _ = m.ours.get(o)
                    g[1]["\n".join(to.lines(o, b) + tb)].append(o)
        new = []
        for by_r, by_o, old_r, old_o in groups.values():
            made = [p for t, rs in by_r.items() if len(rs) == len(by_o.get(t, ()))
                    and (len(rs) == 1 or all(r in m.r2o for r in rs) and all(o in m.o2r for o in by_o[t]))
                    for p in zip(sorted(rs), sorted(by_o[t]))]
            made = [(r, o) for r, o in made if m.r2o.get(r) != o]
            if not made:
                continue
            done_r, done_o = {r for r, _ in made}, {o for _, o in made}
            # the partners the new pairs take away: one left on each side pairs with the other
            left_r = [r for r in old_r if r not in done_r and m.r2o[r] in done_o]
            left_o = [o for o in old_o if o not in done_o and m.o2r[o] in done_r]
            if len(left_r) == 1 and len(left_o) == 1:
                made.append((left_r[0], left_o[0]))
            new += made
        if not new and not tables:
            break
        for r, o in new:
            for x in (r, m.o2r.get(o)):
                if x in m.r2o:
                    del m.o2r[m.r2o.pop(x)]
                    del m.how[x]
        for r, o in new:
            total += m.add(r, o, "strict")
        m.propagate()
    else:
        name_data(weak(suspects()))
    return total


# How sure a pair is, by how it was mapped: by a name, a vftable slot or a unique set of strings
# (2), through the call graph, a table or the strict lines (1), or by position or a body several
# functions share (0, which can pair two alike functions the wrong way round). The $E
# initialisers and destructors count 0 however they were paired: every object numbers its own,
# so their names recur (and the names carried over from the May DLL pair them by body).
STRENGTH = {"vftable": 2, "pdbname": 2, "names": 2, "string": 2, "strict": 1, "callgraph": 1, "table": 1}
STATIC_FN = re.compile(r"^_?\$E\d+$")


def strength(m, r):
    if STATIC_FN.match(m.ours.img.names.get(m.r2o[r], "")):
        return 0
    return STRENGTH.get(m.how[r], 0)


def pair_tables(m):
    """Unmapped functions that tables in read-only data point at (the vftables of classes built
    without RTTI, callback tables): where the aligned operands of a mapped pair with equal masked
    bodies point at relocated words on both sides, the entries at the same index, up to the next
    referenced address, vote as in the call graph. Returns the number of pairs made."""
    ref, ours = m.ref.img, m.ours.img
    rr, ro = ref.refs(), ours.refs()
    votes = defaultdict(Counter)
    for r, o in m.r2o.items():
        a, _, _ = m.ref.get(r)
        b, _, _ = m.ours.get(o)
        if masked(a) != masked(b):
            continue
        for xa, xb in zip(a, b):
            if xa[1] is None or xa[5] is not None:
                continue
            ts, us = rr.operand_targets(xa[4], xa[7], xa[6]), ro.operand_targets(xb[4], xb[7], xb[6])
            if len(ts) != len(us):
                continue
            for x, y in zip(ts, us):
                if x not in rr.site_set or y not in ro.site_set or rr.writable(x) or ro.writable(y):
                    continue
                ex, ey = rr.next_bound(x), ro.next_bound(y)
                k = 0
                while x + k in rr.site_set and y + k in ro.site_set and k < 4 * pe_refs.TABLE_CAP \
                        and (k == 0 or (x + k < ex and y + k < ey)):
                    tx, ty = rr.target(x + k), ro.target(y + k)
                    if tx in ref.start_set and ty in ours.start_set:
                        votes[tx][ty] += 1
                    k += 4
    n = 0
    for tx, c in votes.items():
        if tx in m.r2o:
            continue
        (ty, k), *rest = c.most_common(2)
        if ty not in m.o2r and (not rest or k > 2 * rest[0][1]):
            n += m.add(tx, ty, "table")
    return n


def pair_data(m, skip=()):
    """Writable data the reference uses -> ours: in mapped pairs whose bodies line up, the
    relocated operands at the same positions that both point into .data or .bss vote, as the
    call graph does for functions; the pairs of reference functions in skip do not vote. Votes
    rank by whether the pair's masked bodies are equal, then by its STRENGTH: an address takes
    the candidate with the most votes of the best rank that has any, when no other candidate has
    as many there. Type descriptors are named by class and left out."""
    ref, ours = m.ref.img, m.ours.img
    rr, ro = ref.refs(), ours.refs()
    tdr, tdo = ref.rtti()["tds"], ours.rtti()["tds"]
    votes = defaultdict(Counter)
    for r, o in m.r2o.items():
        if r in skip:
            continue
        a, _, _ = m.ref.get(r)
        b, _, _ = m.ours.get(o)
        ma, mb = masked(a), masked(b)
        if ma == mb:
            blocks = [(0, 0, len(ma))]
        elif len(ma) + len(mb) < 6000:
            blocks = difflib.SequenceMatcher(None, ma, mb, autojunk=False).get_matching_blocks()
        else:
            blocks = []
        rank = (ma == mb, strength(m, r))
        for i, j, n in blocks:
            for k in range(n):
                xa, xb = a[i + k], b[j + k]
                if xa[1] is None or xa[5] is not None:
                    continue
                ts, us = rr.operand_targets(xa[4], xa[7], xa[6]), ro.operand_targets(xb[4], xb[7], xb[6])
                if len(ts) != len(us):
                    continue
                for x, y in zip(ts, us):
                    if rr.writable(x) and ro.writable(y) and x not in tdr and y not in tdo:
                        votes[x][rank, y] += 1
    out = {}
    for x, c in votes.items():
        best = max(k for k, _ in c)
        (y, n), *rest = Counter({y: n for (k, y), n in c.items() if k == best}).most_common(2)
        if not rest or n > rest[0][1]:
            out[x] = y
    return out


def read_map_data(path):
    """Our data symbols from the /MAP file (sections 2 and up): sorted [(address, name)]."""
    out = []
    with open(path, errors="replace") as f:
        for line in f:
            parts = line.split()
            if len(parts) < 4 or not re.match(r"000[2-9]:", parts[0]) or parts[1].startswith("."):
                continue
            try:
                out.append((int(parts[2], 16), parts[1]))
            except ValueError:
                continue
    return sorted(set(out))


def string_refs(img, start):
    """String literals a function's instructions point at, in order."""
    lo, end = img.extent(start)
    i = img.index[lo]
    out = []
    while i < len(img.addrs) and img.addrs[i] < end:
        for h in HEX.findall(img.ins[i]):
            v = int(h, 16)
            if img.lo <= v < img.hi and not img.text[0] <= v < img.text[1] and v not in img.imports:
                s = img.pe.cstr(v, 256)
                if s and len(s) >= 3 and all(32 <= ord(c) < 127 or c in "\t\n\r" for c in s):
                    out.append(s)
        i += 1
    return tuple(out)


class Mapper:
    def __init__(self, ref, ours):
        self.ref, self.ours = Side(ref), Side(ours)
        self.r2o, self.o2r, self.how = {}, {}, {}
        self.votes = defaultdict(Counter)
        self.done = set()

    def add(self, r, o, how):
        if r in self.r2o or o in self.o2r:
            return False
        if r not in self.ref.img.start_set or o not in self.ours.img.start_set:
            return False
        self.r2o[r], self.o2r[o], self.how[r] = o, r, how
        return True

    def propagate(self):
        """Call graph: pair the external targets of mapped pairs at aligned positions."""
        total = 0
        while True:
            todo = [r for r in self.r2o if r not in self.done]
            if not todo:
                break
            for r in todo:
                self.done.add(r)
                a, _, _ = self.ref.get(r)
                b, _, _ = self.ours.get(self.r2o[r])
                ma, mb = masked(a), masked(b)
                if ma == mb:
                    blocks = [(0, 0, len(ma))]
                elif len(ma) + len(mb) < 6000:
                    blocks = difflib.SequenceMatcher(None, ma, mb, autojunk=False).get_matching_blocks()
                else:
                    blocks = []
                for i, j, n in blocks:
                    for k in range(n):
                        ra, rb = a[i + k], b[j + k]
                        pairs = []
                        if ra[2] is not None and rb[2] is not None:
                            pairs.append((ra[2], rb[2]))
                        if len(ra[3]) == len(rb[3]):
                            pairs += list(zip(ra[3], rb[3]))
                        for x, y in pairs:
                            if x in self.ref.img.start_set and y in self.ours.img.start_set:
                                self.votes[x][y] += 1
            new = 0
            for x, c in list(self.votes.items()):
                if x in self.r2o:
                    continue
                (y, n), *rest = c.most_common(2) + [(None, 0)]
                second = rest[0][1] if rest else 0
                if y is None or y in self.o2r or n <= 2 * second:
                    continue
                if self.add(x, y, "callgraph"):
                    new += 1
            total += new
            if not new:
                break
        return total

    def unmapped(self, side):
        img = side.img
        m = self.r2o if side is self.ref else self.o2r
        return [a for a in img.starts if a not in m]

    def by_unique_key(self, key_ref, key_ours, how):
        kr, ko = defaultdict(list), defaultdict(list)
        for a in self.unmapped(self.ref):
            k = key_ref(a)
            if k:
                kr[k].append(a)
        for a in self.unmapped(self.ours):
            k = key_ours(a)
            if k:
                ko[k].append(a)
        n = 0
        for k, rs in kr.items():
            os_ = ko.get(k)
            if len(rs) == 1 and os_ and len(os_) == 1:
                n += self.add(rs[0], os_[0], how)
        return n

    def order(self):
        """Fill gaps between anchors that are in the same order in both binaries."""
        pairs = sorted(self.r2o.items())
        # longest chain increasing on both sides (patience sorting)
        tails, prev, idx = [], [None] * len(pairs), []
        for k, (r, o) in enumerate(pairs):
            p = bisect.bisect_left(tails, o)
            if p == len(tails):
                tails.append(o)
                idx.append(k)
            else:
                tails[p] = o
                idx[p] = k
            prev[k] = idx[p - 1] if p else None
        chain = []
        k = idx[-1] if idx else None
        while k is not None:
            chain.append(pairs[k])
            k = prev[k]
        chain.reverse()
        rs, os_ = self.ref.img.starts, self.ours.img.starts
        n = 0
        for (r1, o1), (r2, o2) in zip(chain, chain[1:]):
            gr = [a for a in rs[bisect.bisect_right(rs, r1):bisect.bisect_left(rs, r2)] if a not in self.r2o]
            go = [a for a in os_[bisect.bisect_right(os_, o1):bisect.bisect_left(os_, o2)] if a not in self.o2r]
            if not gr or not go or len(gr) > 400 or len(go) > 400:
                continue
            fr = [hashlib.sha1("\n".join(masked(self.ref.get(a)[0])).encode()).digest() for a in gr]
            fo = [hashlib.sha1("\n".join(masked(self.ours.get(a)[0])).encode()).digest() for a in go]
            sm = difflib.SequenceMatcher(None, fr, fo, autojunk=False)
            for tag, i1, i2, j1, j2 in sm.get_opcodes():
                if tag == "equal":
                    for k in range(i2 - i1):
                        n += self.add(gr[i1 + k], go[j1 + k], "order")
                elif tag == "replace" and i2 - i1 == j2 - j1:
                    for k in range(i2 - i1):
                        a = masked(self.ref.get(gr[i1 + k])[0])
                        b = masked(self.ours.get(go[j1 + k])[0])
                        if len(a) + len(b) < 6000 and \
                                difflib.SequenceMatcher(None, a, b, autojunk=False).ratio() >= 0.6:
                            n += self.add(gr[i1 + k], go[j1 + k], "order")
        return n


ANON = re.compile(r"\?A0x[0-9a-f]{8}")


def vtable_classes(img):
    """(class, subobject offset) -> slot targets. Anonymous namespaces carry a per-build hash
    (?A0x1aa454be::GripPanel), so it is dropped from the name."""
    r = img.rtti()
    out = {}
    for v in r["vtables"]:
        out[(ANON.sub("?A", v["class"]), v["offset"])] = v["slots"]
    return out


def demangle(names):
    try:
        out = subprocess.run(["llvm-undname"], input="\n".join(names) + "\n", capture_output=True,
                             text=True).stdout
    except OSError:
        return {n: n for n in names}
    # One block per name, ending in an empty line: the name, then its undecorated form, which is
    # missing when the name does not parse (the error goes to stderr).
    res = {}
    for block in out.split("\n\n"):
        lines = block.strip("\n").split("\n")
        if len(lines) == 2:
            res[lines[0]] = lines[1]
    return {n: res.get(n, n) for n in names}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ref")
    ap.add_argument("ours")
    ap.add_argument("map")
    ap.add_argument("decomp", nargs="?")
    ap.add_argument("--out", required=True)
    ap.add_argument("--ref-procs", help="the reference's functions from its PDB (procs.tsv)")
    ap.add_argument("--ref-names", help="names for reference functions, procs.tsv's columns, extents not used")
    ap.add_argument("--ours-procs", action="store_true", help="<our map> is a procs.tsv: map two references")
    ap.add_argument("--show")
    ap.add_argument("--loose", action="store_true",
                    help="the original scoring: every image address is ADDR, .rdata is not read")
    args = ap.parse_args()

    ref, ours = Image(args.ref), Image(args.ours)
    if args.ours_procs:
        funcs = {a: (n, mod) for a, (n, mod, _) in read_procs(args.map).items()}
    else:
        funcs = read_map(args.map)
    ours.names = {a: n for a, (n, _) in funcs.items()}
    objs = {a: o for a, (_, o) in funcs.items()}
    ours.finish(funcs)
    vr, vo = vtable_classes(ref), vtable_classes(ours)
    xr, xo = exports(args.ref, ref.lo), exports(args.ours, ours.lo)
    er = ref.lo + entry_point(ref.pe)
    eo = ours.lo + entry_point(ours.pe)
    calls, jumps = branch_targets(ref)
    strong = {t for s in vr.values() for t in s} | calls | set(xr.values()) | {er}
    procs = read_procs(args.ref_procs) if args.ref_procs else {}
    if args.decomp:
        weak = ref_starts(args.decomp, ref) | jumps
    else:
        weak = pointer_seeds(ref) | padding_starts(ref) | jumps
    # The PDB's extents are exact: no other start inside one of its functions (exception handler
    # funclets, jump targets, pointers to code after a jump table).
    ext = sorted((a, a + v[2]) for a, v in procs.items())
    los = [a for a, _ in ext]

    def outside(a):
        i = bisect.bisect_right(los, a) - 1
        return i < 0 or a >= ext[i][1] or a == los[i]
    strong = {a for a in strong if outside(a)} | set(procs)
    weak = {a for a in weak if outside(a)}
    ref.finish(strong | weak)
    # The decompile's seeds and jmp targets include points inside functions; a real start never
    # follows an instruction that falls through into it.
    ref.finish(strong | {a for a in weak if a in ref.index and after_terminator(ref, a)})
    print(f"reference: {len(ref.starts)} functions; ours: {len(ours.starts)} functions", file=sys.stderr)

    refnames = {}
    names_tsv = os.path.join(args.decomp, "names.tsv") if args.decomp else ""
    if os.path.exists(names_tsv):
        with open(names_tsv) as f:
            for line in f:
                p = line.rstrip("\n").split("\t")
                if len(p) >= 3 and p[0] != "address":
                    refnames[int(p[0], 16)] = (p[1], p[2])
    for a, (n, *_) in procs.items():
        refnames[a] = (n, "pdb")
    ref_names = dict(procs)
    if args.ref_names:
        for a, v in read_procs(args.ref_names).items():
            if a in ref.start_set:
                ref_names.setdefault(a, v)
                refnames.setdefault(a, (v[0], "names"))
    dem = {} if args.ours_procs else demangle(sorted(set(ours.names.values())))
    our_names = {}
    if ref_names:
        for a, n in ours.names.items():
            name = n if args.ours_procs else undecorated_parts(n, dem.get(n, n))[0]
            our_names[a] = (name, objs.get(a, "").split(":")[-1])

    m = run_mapping(ref, ours, vr, vo, (er, eo), xr, xo, refnames, ref_names, our_names, bool(procs))
    hints = {a for a in start_hints(m) if outside(a)}
    if hints:
        # A second pass with the reference starts the first one implied.
        print(f"{len(hints)} reference starts found through the mapping, mapping again", file=sys.stderr)
        ref.finish(set(ref.starts) | hints)
        m = run_mapping(ref, ours, vr, vo, (er, eo), xr, xo, refnames, ref_names, our_names, bool(procs))

    def label(t):
        r = m.r2o.get(t)
        if r is None:
            o = ref.owner(t)
            if o is not None and o in m.r2o:
                return f"<{ours.names.get(m.r2o[o], '?')}+{t - o:#x}>"
            return f"<?{t:08x}>"
        return f"<{ours.names.get(r, '?')}>"

    def label_ours(t):
        if t in ours.names:
            return f"<{ours.names[t]}>"
        o = ours.owner(t)
        return f"<{ours.names.get(o, '?')}+{t - o:#x}>" if o is not None else f"<?{t:08x}>"

    if args.loose:
        def lines_ref(r, rows):
            return labelled(rows, label)

        def lines_ours(o, rows):
            return labelled(rows, label_ours)
    else:
        syms = [] if args.ours_procs else read_map_data(args.map)
        starts = [a for a, _ in syms]

        def our_data(y):
            i = bisect.bisect_right(starts, y) - 1
            if i < 0:
                return f"data:{y:08x}"
            a, name = syms[i]
            return f"data:{name}" + (f"+{y - a:#x}" if y != a else "")
        tr, to = Tokens(ref, label, None, m.r2o), Tokens(ours, label_ours, None, m.o2r)

        paired = [0]

        def name_data(skip=()):
            data = pair_data(m, skip)
            named = {y: our_data(y) for y in set(data.values())}
            tr.data_name = lambda x: named[data[x]] if x in data else "ADDR"
            to.data_name = lambda y: named.get(y, "ADDR")
            paired[0] = len(data)
        moved = refine(m, tr, to, name_data)
        print(f"strict: {moved} pairs made where the loose bodies are equal; {paired[0]} writable data "
              f"addresses paired", file=sys.stderr)
        if not args.ours_procs:
            n, bad = pe_refs.check_map(ours.refs(), args.map)
            print(f"strict: content reader checked on {n} __real@ and ??_C@ entries of our map, "
                  f"{len(bad)} disagree", file=sys.stderr)
            for b in bad[:10]:
                print("  " + b, file=sys.stderr)

        def lines_ref(r, rows):
            return tr.lines(r, rows)

        def lines_ours(o, rows):
            return to.lines(o, rows)

    if args.show:
        sel = []
        for want in args.show.split(","):
            found = []
            try:
                a = int(want, 16)
                if a in m.r2o:
                    found.append(a)
            except ValueError:
                pass
            found += [r for r, o in m.r2o.items() if want in ours.names.get(o, "")]
            sel += found[:20]
        for r in sel:
            a, ta, _ = m.ref.get(r)
            b, tb, _ = m.ours.get(m.r2o[r])
            print(f"=== {r:08x} <-> {m.r2o[r]:08x} {ours.names[m.r2o[r]]} ({m.how[r]})")
            for line in difflib.unified_diff(lines_ref(r, a) + ta, lines_ours(m.r2o[r], b) + tb,
                                             "reference", "ours", lineterm="", n=3):
                print(line)
        return

    absent_o = {c for c, _ in vr} - {c for c, _ in vo}
    absent_r = {c for c, _ in vo} - {c for c, _ in vr}
    only_ref = {t for (c, _), s in vr.items() if c in absent_o for t in s}
    only_ours = {t for (c, _), s in vo.items() if c in absent_r for t in s}

    rows = []
    for r in ref.starts:
        a, ta, sa = m.ref.get(r)
        o = m.r2o.get(r)
        rn = refnames.get(r, ("", ""))[0]
        if o is None:
            rows.append(("", "only-ref" if r in only_ref else "unmapped", 0.0, sa, 0, "", f"{r:08x}", "", "", rn))
            continue
        b, tb, sb = m.ours.get(o)
        la, lb = lines_ref(r, a) + ta, lines_ours(o, b) + tb
        if la == lb:
            status, ratio = "same", 1.0
        else:
            status = "changed"
            ratio = difflib.SequenceMatcher(None, la, lb, autojunk=False).ratio() if len(la) + len(lb) < 20000 else 0.0
        rows.append((objs.get(o, "?"), status, ratio, sa, sb, ours.names[o], f"{r:08x}", f"{o:08x}", m.how[r], rn))
    for o in ours.starts:
        if o not in m.o2r:
            _, _, sb = m.ours.get(o)
            rows.append((objs.get(o, "?"), "only-ours" if o in only_ours else "unmapped", 0.0, 0, sb,
                         ours.names[o], "", f"{o:08x}", "", ""))
    with open(args.out, "w") as f:
        for r in rows:
            f.write(f"{r[0]}\t{r[1]}\t{r[2]:.3f}\t{r[3]}\t{r[4]}\t{r[5]}\t{dem.get(r[5], '')}\t"
                    f"{r[6]}\t{r[7]}\t{r[8]}\t{r[9]}\n")

    def group(obj):
        return obj.split(":")[0] if ":" in obj else os.path.splitext(os.path.basename(args.ours))[0]

    nref = len(ref.starts)
    bref = sum(r[3] for r in rows if r[6])
    mapped = [r for r in rows if r[6] and r[7]]
    print(f"mapped {len(mapped)} of {nref} reference functions "
          f"({100 * len(mapped) / max(nref, 1):.1f}%, {sum(r[3] for r in mapped)} of {bref} bytes)", file=sys.stderr)
    print("  by " + ", ".join(f"{k} {v}" for k, v in Counter(m.how.values()).most_common()), file=sys.stderr)
    totals = defaultdict(lambda: [0, 0])
    for r in rows:
        side = "ours" if r[1] == "unmapped" and not r[6] else ""
        key = (r[1] + (" (ours)" if side else ""))
        totals[key][0] += 1
        totals[key][1] += r[3] or r[4]
    for status, (n, size) in sorted(totals.items()):
        print(f"{status}\t{n} functions\t{size} bytes", file=sys.stderr)
    per = defaultdict(Counter)
    for r in mapped:
        per[group(r[0])][r[1]] += 1
    for g, c in sorted(per.items(), key=lambda x: -sum(x[1].values())):
        print(f"  {g}: " + ", ".join(f"{k} {v}" for k, v in sorted(c.items())), file=sys.stderr)


def run_mapping(ref, ours, vr, vo, entries, xr, xo, refnames, ref_names, our_names, exact):
    """exact: ref_names are the reference's own PDB names, paired before anything else; names
    carried over from another DLL are paired after the vftables, which they can contradict (a small
    accessor whose body moved to a neighbour's when a class layout changed)."""
    m = Mapper(ref, ours)
    if ref_names and exact:
        print(f"pdbname: {pair_by_name(m, ref_names, our_names)} pairs", file=sys.stderr)
    # vftables of the same class, subobject and slot count
    votes = defaultdict(Counter)
    for key, slots in vr.items():
        o = vo.get(key)
        if o and len(o) == len(slots):
            for x, y in zip(slots, o):
                votes[x][y] += 1
    for x, c in votes.items():
        (y, n), *rest = c.most_common(2)
        if not rest or n > 2 * rest[0][1]:
            m.add(x, y, "vftable")
    m.add(*entries, "vftable")
    for name, a in xr.items():
        if name in xo:
            m.add(a, xo[name], "vftable")
    if ref_names and not exact:
        print(f"pdbname: {pair_by_name(m, ref_names, our_names)} pairs", file=sys.stderr)
    # names.tsv names unique among ours
    simple = defaultdict(list)
    for a, n in ours.names.items():
        simple[msvc_simple_demangle(n)].append(a)
    for a, (n, conf) in refnames.items():
        if conf in ("high", "medium") and len(simple.get(n, [])) == 1:
            m.add(a, simple[n][0], "names")
    steps = [
        ("order", m.order),
        ("string", lambda: m.by_unique_key(lambda a: string_refs(ref, a), lambda a: string_refs(ours, a), "string")),
        ("body", lambda: m.by_unique_key(
            lambda a: _body_key(m.ref.get(a)[0]), lambda a: _body_key(m.ours.get(a)[0]), "body")),
    ]
    rounds = 0
    while True:
        rounds += 1
        changed = m.propagate()
        for name, fn in steps:
            changed += fn()
            changed += m.propagate()
        print(f"round {rounds}: {len(m.r2o)} mapped", file=sys.stderr)
        if not changed or rounds >= 8:
            break

    return m


def start_hints(m):
    """Reference function starts implied by mapped pairs whose bodies line up: where our branch goes
    to the start of another of our functions, the reference's goes to a start too (a tail jump into
    a CRT function, je __floor_default); where our body is a strict prefix of the reference's, the
    reference's next instruction starts the next function."""
    ref, ours = m.ref.img, m.ours.img
    hints = set()
    for r, o in m.r2o.items():
        a, _, _ = m.ref.get(r)
        b, _, _ = m.ours.get(o)
        ma, mb = masked(a), masked(b)
        if len(mb) < len(ma) and ma[:len(mb)] == mb:
            nxt = a[len(mb)][4]
            if after_terminator(ref, nxt):
                hints.add(nxt)
            continue
        if len(ma) + len(mb) >= 6000:
            continue
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, ma, mb, autojunk=False).get_opcodes():
            if tag != "replace" or i2 - i1 != j2 - j1:
                continue
            for x, y in zip(a[i1:i2], b[j1:j2]):
                if x[0] == y[0] and y[1] is None and y[2] in ours.start_set and x[5] is not None \
                        and x[5] not in ref.start_set and x[5] in ref.index:
                    hints.add(x[5])
    return hints


def _body_key(rows):
    if len(rows) < 8:
        return None
    return hashlib.sha1("\n".join(masked(rows)).encode()).digest()


def entry_point(pe):
    """AddressOfEntryPoint (an RVA) from the optional header."""
    opt = struct_u32(pe.data, 0x3C) + 24
    return struct_u32(pe.data, opt + 16)


EXPORT_RVA = re.compile(r"\+base\[\s*\d+\]\s+([0-9a-f]+)\s+Export RVA")
EXPORT_NAME = re.compile(r"\+base\[\s*\d+\]\s+[0-9a-f]+\s+(\S+)$")


def exports(path, base):
    """Exported name -> address (objdump -p lists the address table, then the name table)."""
    out = subprocess.run(["objdump", "-p", path], capture_output=True, text=True, check=True).stdout
    rvas = [int(x, 16) for x in EXPORT_RVA.findall(out)]
    names = [m.group(1) for m in map(EXPORT_NAME.search, out.splitlines()) if m and "Export RVA" not in m.string]
    return {n: base + a for n, a in zip(names, rvas)}


def struct_u32(d, o):
    return int.from_bytes(d[o:o + 4], "little")


if __name__ == "__main__":
    main()
