"""Tell addresses from constants in an MSVC 7.1 PE (client.dll, server.dll), for the strict compare.

Neither Hidden's DLLs nor ours are position independent, but both carry PE base relocations
(.reloc, data directory 5): blocks of a 4 KB page RVA plus 16-bit entries, type 3
(IMAGE_REL_BASED_HIGHLOW) marking a 32-bit absolute address. An operand whose four bytes one of
them covers is an address; every other number is a constant, inside the image range or not.

msvc_compare.py names what an address operand points at (a function by the mapping, a vftable, an
import slot, writable data through the mapping). This module gives the token for read-only data
without a name, from its content:

    an x87 operand by its suffix   f32:62 (flds, fsts, fmuls), f64:0.5 (fldl), i32:0x1 (fildl),
                                   i16:0x8 (filds), f80 (fldt); SSE movss, mulss f32, movsd, mulsd,
                                   movlpd f64, packed ones x128
    a sized integer read           w32:0x42780000, or *<token> for a pointer that is relocated
    a C string                     "text" (a __FILE__ path relative to the tree), L"text" for UTF-16
    a table of addresses           [<token>,<token>,...] for relocated words in a row
    other bytes                    ro:0011... up to the next relocated or referenced address

Usage (normally imported by msvc_compare.py):
    python3 -I pe_refs.py <dll> <address> [<op> <args>]
    python3 -I pe_refs.py <dll> --check-map <map>   checks the content reader against the map's
                                                    __real@<hex> and ??_C@ names
"""
import bisect
import hashlib
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_rtti  # noqa: E402
from elf_refs import DATA_CAP, PRINTABLE, STRING_CAP, TABLE_CAP, fp_kind, fp_text, operand_size  # noqa: E402
from pe_seeds import relocations  # noqa: E402

SCN_CODE = 0x20000000
SCN_WRITE = 0x80000000
NUMBER = re.compile(r"(\$?)(-?0x[0-9a-f]+)\b")
SSE_F32 = {"movss", "addss", "subss", "mulss", "divss", "minss", "maxss", "sqrtss", "comiss", "ucomiss",
           "cvtss2sd", "cvtss2si", "cvttss2si", "rsqrtss", "rcpss", "cmpss"}
SSE_F64 = {"movsd", "addsd", "subsd", "mulsd", "divsd", "minsd", "maxsd", "sqrtsd", "comisd", "ucomisd",
           "cvtsd2ss", "cvtsd2si", "cvttsd2si", "movlpd", "movhpd", "movlps", "movhps", "movq", "cmpsd"}
# The tree's top directories: a __FILE__ path is cut to start at the first of them.
TREE_DIRS = {"cl_dll", "dlls", "game_shared", "public", "tier0", "tier1", "vgui2", "common", "utils",
             "mathlib", "vstdlib"}
WIN_SOURCE = re.compile(r"^[\w.:\\/ +-]*[\\/][\w.+-]+\.(?:cpp|c|h)$", re.I)


def sse_kind(op):
    """(label, size, struct format) of an SSE memory operand, or None."""
    if op in SSE_F32:
        return ("f32", 4, "<f")
    if op in SSE_F64:
        return ("f64", 8, "<d")
    if op.endswith(("ps", "pd")) or op in ("movdqa", "movdqu", "pand", "por", "pxor"):
        return ("x128", 16, None)
    return None


def source_path(text):
    """__FILE__ as the tree-relative path. Hidden compiled from \\hl2sdk\\src (client) and
    \\HL2Mods\\hidden (server), our build from the project folder (..\\game_shared\\x.cpp)."""
    if not WIN_SOURCE.match(text):
        return text
    parts = [p for p in re.split(r"[\\/]", text) if p and p != "."]
    for i, p in enumerate(parts):
        if p.lower() in TREE_DIRS:
            return "/".join(parts[i:]).lower()
    return text


def data_directories(pe):
    """[(rva, size)] of the optional header's data directories (PE32)."""
    d = pe.data
    opt = struct.unpack_from("<I", d, 0x3C)[0] + 24
    n = struct.unpack_from("<I", d, opt + 92)[0]
    return [struct.unpack_from("<II", d, opt + 96 + 8 * i) for i in range(min(n, 16))]


class Refs:
    """Base relocations, sections and read-only content of one PE, read once."""

    def __init__(self, pe):
        """bounds: every address something points at (a relocated field's value), the section
        ends and the data directories; untyped data ends at the next one."""
        self.pe = pe
        self.sites = sorted(relocations(pe))
        self.site_set = set(self.sites)
        bounds = set()  # every address something points at, and the section ends
        for s in self.sites:
            t = pe.u32(s)
            if t is not None:
                bounds.add(t)
        bounds.update(s[1] + s[2] for s in pe.sections)
        # the tables the PE header points at (imports, IAT, exports, debug directory, load
        # config) sit in .rdata after the data the code uses
        for rva, size in data_directories(pe):
            if rva:
                bounds.update((pe.base + rva, pe.base + rva + size))
        self.bounds = sorted(bounds)
        self.secs = sorted(pe.sections, key=lambda s: s[1])
        self._sec_starts = [s[1] for s in self.secs]

    def section(self, addr):
        """(name, start, size, rawptr, rawsize, flags) of the section holding addr, or None."""
        i = bisect.bisect_right(self._sec_starts, addr) - 1
        if i >= 0 and addr < self.secs[i][1] + self.secs[i][2]:
            return self.secs[i]
        return None

    def writable(self, addr):
        sec = self.section(addr)
        return bool(sec and sec[5] & SCN_WRITE)

    def read(self, addr, n):
        sec = self.section(addr)
        if sec is None or addr >= sec[1] + sec[4]:
            return None
        n = min(n, sec[1] + sec[4] - addr)
        o = sec[3] + addr - sec[1]
        return self.pe.data[o:o + n]

    def target(self, site):
        return self.pe.u32(site)

    def sites_in(self, start, end):
        """Relocated 4-byte fields that start inside [start, end). end is the next instruction
        objdump shows, which can be a function start inside this one's last instruction (a seed
        on an operand), so a field may run past it."""
        i = bisect.bisect_left(self.sites, start)
        out = []
        while i < len(self.sites) and self.sites[i] < end:
            out.append(self.sites[i])
            i += 1
        return out

    def next_bound(self, addr):
        i = bisect.bisect_right(self.bounds, addr)
        return self.bounds[i] if i < len(self.bounds) else addr + DATA_CAP

    def next_site(self, addr):
        i = bisect.bisect_left(self.sites, addr)
        return self.sites[i] if i < len(self.sites) else 1 << 32

    # -- operands ------------------------------------------------------------------------------

    def fields(self, addr, end, args):
        """Relocated fields of the instruction at addr, end being the next address objdump shows.
        That can be a function start found inside this instruction (a seed on an operand, after
        which objdump's sweep was redone): a field there still belongs to this instruction when its
        value is one of the numbers in args."""
        sites = self.sites_in(addr, end)
        values = {int(n.group(2), 16) & 0xFFFFFFFF for n in NUMBER.finditer(args)}
        values -= {self.target(x) for x in sites}
        for x in self.sites_in(end, addr + 12):
            if self.target(x) in values:
                sites.append(x)
                values.discard(self.target(x))
        return sites

    def operands(self, op, args, addr, end, token):
        """args of the instruction at [addr, end) with every relocated field replaced by
        token(target, op, args, imm, indexed); the other numbers stay as they are."""
        sites = self.fields(addr, end, args)
        if not sites:
            return args
        end = max(end, sites[-1] + 4)
        numbers = [[n, False] for n in NUMBER.finditer(args)]
        subs = []
        for site in sites:
            field = self.target(site)
            hits = [h for h in numbers if not h[1] and int(h[0].group(2), 16) & 0xFFFFFFFF == field]
            if not hits:
                subs.append((len(args), len(args), " {" + token(field, op, args, True, False) + "}"))
                continue
            # The immediate is the last field of an instruction, after any displacement.
            imm = [h for h in hits if h[0].group(1)]
            mem = [h for h in hits if not h[0].group(1)]
            hit = imm[0] if imm and (site == end - 4 or not mem) else (mem or imm)[0]
            hit[1] = True
            n = hit[0]
            is_imm = bool(n.group(1))
            indexed = not is_imm and args[n.end():n.end() + 1] == "("
            subs.append((n.start(), n.end(), n.group(1) + token(field, op, args, is_imm, indexed)))
        for start, stop, text in sorted(subs, reverse=True):
            args = args[:start] + text + args[stop:]
        return args

    def operand_targets(self, addr, end, args):
        """Targets of the relocated fields of the instruction at [addr, end), in field order."""
        return [self.target(s) for s in self.fields(addr, end, args)]

    # -- read-only content ---------------------------------------------------------------------

    def content_token(self, addr, op, args, imm, indexed, depth, token):
        """Token for read-only data without a name; token(target, depth=...) names a pointer
        found in it."""
        memory = not imm and op != "lea"
        kind = (fp_kind(op) or sse_kind(op)) if memory else None
        if kind is not None:
            label, size, fmt = kind
            if indexed:
                n = max(1, min(DATA_CAP, self.next_bound(addr) - addr) // size)
                return f"{label}[" + ",".join(fp_text(self.read(addr + i * size, size), fmt) for i in range(n)) + "]"
            return f"{label}:{fp_text(self.read(addr, size), fmt)}"
        if memory and not indexed:
            size = operand_size(op, args)
            if size == 4 and addr in self.site_set and depth == 0:
                return "*" + token(self.target(addr), depth=depth + 1)
            raw = self.read(addr, size) or b""
            return f"w{size * 8}:{int.from_bytes(raw, 'little'):#x}"
        return self.data_token(addr, depth, token)

    def data_token(self, addr, depth, token):
        """Untyped read-only data: an address table, a C string, or the bytes up to the next
        referenced address, at most DATA_CAP and without trailing zeros, with any pointer among
        them as its token (an SEH scope table is -1 and two code addresses). The cap is lower than
        the server's because .rdata holds data no operand points at (the C++ EH tables, the debug
        directory), whose image offsets depend on layout and are not relocated."""
        bound = self.next_bound(addr)
        if addr in self.site_set and depth == 0:
            entries = []
            a = addr
            while a in self.site_set and len(entries) < TABLE_CAP and (a == addr or a < bound):
                entries.append(token(self.target(a), depth=depth + 1))
                a += 4
            return "[" + ",".join(entries) + "]"
        s = self.string_at(addr)
        if s is not None:
            return s
        end = addr + max(1, min(bound - addr, DATA_CAP))
        sites = self.sites_in(addr, end)
        if sites:
            parts, a = [], addr
            for x in sites:
                if x > a:
                    parts.append((self.read(a, x - a) or b"").hex())
                parts.append(token(self.target(x), depth=depth + 1) if depth == 0 else "PTR")
                a = x + 4
            tail = (self.read(a, end - a) or b"").rstrip(b"\0") if a < end else b""
            if tail:
                parts.append(tail.hex())
            return "ro:{" + ",".join(parts) + "}"
        raw = self.read(addr, end - addr) or b""
        return "ro:" + (raw.rstrip(b"\0") or raw[:1]).hex()

    def string_at(self, addr):
        """A C string (narrow, or UTF-16 of two characters or more) at addr as a token, or None.
        Zero bytes read as the empty string.
        With /GF equal strings are one copy, so the token is the text, never the address."""
        raw = self.read(addr, 4096) or b""
        # a pointer is not text
        raw = raw[:max(0, self.next_site(addr) - addr)]
        if len(raw) >= 6 and raw[0] and not raw[1]:
            n = 0
            while n + 1 < len(raw) and (raw[n] or raw[n + 1]):
                n += 2
            text = raw[:n]
            if n >= 4 and n + 1 < len(raw) and not any(text[1::2]) and all(b in PRINTABLE for b in text[0::2]):
                return "L" + quote(text[0::2])
        nul = raw.find(b"\0")
        if nul >= 0 and all(b in PRINTABLE for b in raw[:nul]):
            return quote(raw[:nul])
        return None


def quote(text):
    s = source_path(text.decode("latin-1"))
    if len(s) > STRING_CAP:
        s = s[:STRING_CAP] + "..." + hashlib.sha1(text).hexdigest()[:8]
    return '"' + s.encode("unicode_escape").decode("ascii").replace('"', '\\"') + '"'


def string_length(mangled):
    """Byte length (with the NUL) a ??_C@_0 / ??_C@_1 name encodes, or None."""
    m = re.match(r"\?\?_C@_[01]([0-9]|[A-P]+@)", mangled)
    if not m:
        return None
    g = m.group(1)
    if g.isdigit():
        return int(g) + 1
    return int("".join("0123456789abcdef"["ABCDEFGHIJKLMNOP".index(c)] for c in g[:-1]), 16)


def check_map(refs, map_path):
    """Compare the content reader with the map's names: __real@<hex> holds those bits, a
    ??_C@_0 string is read as a narrow string of the encoded length, ??_C@_1 as UTF-16.
    Returns (checked, [disagreements])."""
    n, bad = 0, []
    with open(map_path, errors="replace") as f:
        for line in f:
            parts = line.split()
            if len(parts) < 3 or not parts[0].startswith("0002:"):
                continue
            name = parts[1]
            try:
                va = int(parts[2], 16)
            except ValueError:
                continue
            if name.startswith("__real@"):
                hexv = name[7:]
                size = len(hexv) // 2
                raw = refs.read(va, size)
                n += 1
                if raw is None or int.from_bytes(raw, "little") != int(hexv, 16):
                    bad.append(f"{va:08x} {name}: {raw.hex() if raw else None}")
                fmt = {4: "<f", 8: "<d"}.get(size)
                tok = refs.content_token(va, "flds" if size == 4 else "fldl", "", False, False, 0, None)
                if fmt and tok != f"{'f32' if size == 4 else 'f64'}:{fp_text(raw, fmt)}":
                    bad.append(f"{va:08x} {name}: token {tok}")
            elif name.startswith("??_C@_"):
                length = string_length(name)
                tok = refs.string_at(va)
                n += 1
                wide = name.startswith("??_C@_1")
                raw = refs.read(va, length or 0) or b""
                if wide and length and length >= 6:
                    ok = tok is not None and tok.startswith("L") and len(raw) == length and raw[-2:] == b"\0\0"
                elif wide:
                    ok = True  # a one-character L"x" reads as the narrow "x"; the bytes still compare
                else:
                    # the text runs to the first NUL; a literal can end in more of them (".vmt\0")
                    ok = (length == 1 and raw == b"\0") or (tok is not None and not tok.startswith("L")
                                                             and len(raw) == length
                                                             and not raw[raw.find(b"\0"):].strip(b"\0"))
                if not ok:
                    bad.append(f"{va:08x} {name}: {tok}")
    return n, bad


def main():
    pe = pe_rtti.Pe(sys.argv[1])
    refs = Refs(pe)
    if sys.argv[2] == "--check-map":
        n, bad = check_map(refs, sys.argv[3])
        print(f"{n} __real@ and ??_C@ entries checked, {len(bad)} disagree")
        for b in bad[:50]:
            print("  " + b)
        return
    addr = int(sys.argv[2], 16)
    op = sys.argv[3] if len(sys.argv) > 3 else ""
    args = sys.argv[4] if len(sys.argv) > 4 else ""
    print(refs.content_token(addr, op, args, not op, False, 0, lambda t, *a, **k: f"{t:#x}"))


if __name__ == "__main__":
    main()
