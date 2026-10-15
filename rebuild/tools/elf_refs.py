"""Name what an address operand in a GCC 3.4 server_i486.so points at, for the strict compare.

Hidden's server and ours are non-PIC shared objects with text relocations (DT_TEXTREL), so the
dynamic relocation table marks every operand that holds an address:

    R_386_32 against a symbol    the field holds the addend; the target is the symbol plus it
    R_386_PC32 against a symbol  a call or jump to an exported function
    R_386_RELATIVE               the field holds the link-time address of something local

An operand no relocation covers is a constant. A covered one is replaced by a token for its target
that does not depend on layout:

    a symbol (function, global, vtable, static)   its name plus the offset into it; a read-only
                                                  object with no relocations inside gets a short
                                                  hash of its bytes, so a changed constant table
                                                  shows up
    read-only data with no symbol                 its content: an x87 operand by operand size
                                                  (f32:62, f64:0.5, i32:0x1), a sized integer read
                                                  (w32:0x42780000), a C string ("text"; a __FILE__
                                                  path relative to the tree), a table of
                                                  addresses ([+0x1c,+0x40] for a jump table, offsets
                                                  into the function) or the bytes up to the next
                                                  symbol or referenced address (ro:0011...)

Usage (normally imported by compare_builds.py and fn_diff.py):
    python3 -I elf_refs.py <server_i486.so> <address> [<op> <args>]
"""
import bisect
import hashlib
import posixpath
import re
import struct
import subprocess
import sys

STRING_CAP = 256    # longest string kept literally; longer ones keep a hash of the rest
DATA_CAP = 64       # bytes shown for untyped read-only data
TABLE_CAP = 1024    # entries read from an address table

REG32 = re.compile(r"%e(?:[abcd]x|[sd]i|[sb]p)\b")
REG16 = re.compile(r"%(?:[abcd]x|[sd]i|[sb]p)\b")
REG8 = re.compile(r"%[abcd][lh]\b")
FP_KINDS = {"s": ("f32", 4, "<f"), "l": ("f64", 8, "<d"), "t": ("f80", 10, None)}
FI_KINDS = {"ll": ("i64", 8, "<q"), "l": ("i32", 4, "<i"), "s": ("i16", 2, "<h")}
PRINTABLE = set(range(0x20, 0x7F)) | {9, 10, 13} | set(range(0x80, 0x100))
SOURCE_PATH = re.compile(r"^[\w./+-]*/[\w.+-]+\.(?:cpp|c|h)$")


def _int(text):
    return int(text, 16) if text.startswith("0x") else int(text)


class Refs:
    """Sections, symbols and dynamic relocations of one ELF file, read once with readelf."""

    def __init__(self, path, canon=lambda name: name):
        self.canon = canon
        with open(path, "rb") as f:
            self.data = f.read()
        self._sections(path)
        self._symbols(path)
        self._relocations(path)
        self._hash_cache = {}

    # -- loading -------------------------------------------------------------------------------

    def _sections(self, path):
        out = subprocess.run(["readelf", "-SW", path], capture_output=True, text=True, check=True).stdout
        self.secs = []  # (addr, end, file offset or None for NOBITS, name, writable, executable)
        for line in out.splitlines():
            parts = line.replace("[ ", "[").split()
            if len(parts) < 10 or not parts[0].startswith("[") or parts[0] == "[Nr]":
                continue
            name, typ, addr, off, size = parts[1], parts[2], int(parts[3], 16), int(parts[4], 16), int(parts[5], 16)
            flags = parts[7] if len(parts) == 11 else ""
            if addr and "A" in flags:
                self.secs.append((addr, addr + size, None if typ == "NOBITS" else off, name, "W" in flags, "X" in flags))
        self.secs.sort()
        self._sec_starts = [s[0] for s in self.secs]

    def _symbols(self, path):
        out = subprocess.run(["readelf", "-sW", path], capture_output=True, text=True, check=True).stdout
        best = {}  # address -> (rank, name, size)
        in_symtab = False
        for line in out.splitlines():
            if line.startswith("Symbol table"):
                in_symtab = "'.symtab'" in line
                continue
            parts = line.split()
            if not in_symtab or len(parts) < 8 or not parts[0][:-1].isdigit():
                continue
            typ, bind, ndx, name = parts[3], parts[4], parts[6], parts[7]
            if ndx in ("UND", "ABS") or typ in ("SECTION", "FILE") or not name:
                continue
            addr, size = int(parts[1], 16), _int(parts[2])
            # One name per address: functions and objects before untyped labels, then global,
            # weak, local, then the shortest name, so both builds pick the same alias.
            # The size is the largest of the aliases, so an untyped label does not hide its object.
            rank = (typ not in ("FUNC", "OBJECT"), {"GLOBAL": 0, "WEAK": 1}.get(bind, 2), len(name), name)
            prev = best.get(addr)
            if prev is None or rank < prev[0]:
                best[addr] = (rank, name, max(size, prev[2] if prev else 0))
            else:
                best[addr] = (prev[0], prev[1], max(size, prev[2]))
        self.sym_starts = sorted(best)
        self.sym_name = [best[a][1] for a in self.sym_starts]
        self.sym_size = [best[a][2] for a in self.sym_starts]

    def _relocations(self, path):
        out = subprocess.run(["readelf", "-rW", path], capture_output=True, text=True, check=True).stdout
        self.rel = {}  # field address -> (type, symbol name or None, symbol value)
        for line in out.splitlines():
            parts = line.split()
            if len(parts) < 3 or not parts[2].startswith("R_386_"):
                continue
            typ = parts[2][6:]
            if typ not in ("32", "PC32", "RELATIVE"):
                continue
            off = int(parts[0], 16)
            if typ == "RELATIVE":
                self.rel[off] = ("RELATIVE", None, 0)
            else:
                value = int(parts[3], 16) if len(parts) > 3 else 0
                name = parts[4].split("@")[0] if len(parts) > 4 else None
                self.rel[off] = (typ, name, value)
        self.rel_sorted = sorted(self.rel)
        # Every address something points at ends the untyped data before it.
        bounds = set(self.sym_starts)
        for off, (typ, name, value) in self.rel.items():
            if typ == "RELATIVE" or (typ == "32" and value):
                bounds.add(self._target(off, typ, value))
        bounds.update(s[1] for s in self.secs)
        self.bounds = sorted(bounds)

    # -- raw access ----------------------------------------------------------------------------

    def section(self, addr):
        i = bisect.bisect_right(self._sec_starts, addr) - 1
        if i >= 0 and addr < self.secs[i][1]:
            return self.secs[i]
        return None

    def read(self, addr, n):
        sec = self.section(addr)
        if sec is None or sec[2] is None:
            return None
        n = min(n, sec[1] - addr)
        start = sec[2] + addr - sec[0]
        return self.data[start: start + n]

    def word(self, addr):
        raw = self.read(addr, 4)
        return struct.unpack("<I", raw)[0] if raw and len(raw) == 4 else 0

    def _target(self, off, typ, value):
        return (self.word(off) + (value if typ == "32" else 0)) & 0xFFFFFFFF

    def symbol_at(self, addr):
        """(name, start, size) of the symbol containing addr, or None."""
        i = bisect.bisect_right(self.sym_starts, addr) - 1
        for j in range(i, max(i - 8, -1), -1):
            start, size = self.sym_starts[j], self.sym_size[j]
            if addr == start or start <= addr < start + size:
                return self.sym_name[j], start, size
        return None

    def preceding_symbol(self, addr):
        i = bisect.bisect_right(self.sym_starts, addr) - 1
        return (self.sym_name[i], self.sym_starts[i]) if i >= 0 else None

    def next_bound(self, addr):
        i = bisect.bisect_right(self.bounds, addr)
        return self.bounds[i] if i < len(self.bounds) else addr + DATA_CAP

    def relocs_in(self, start, end):
        i = bisect.bisect_left(self.rel_sorted, start)
        out = []
        while i < len(self.rel_sorted) and self.rel_sorted[i] < end:
            out.append(self.rel_sorted[i])
            i += 1
        return out

    # -- tokens --------------------------------------------------------------------------------

    def _named(self, name, off):
        name = self.canon(name)
        return name if off == 0 else f"{name}{off:+#x}"

    def _object_hash(self, start, size):
        """Short hash of a read-only object with no relocations inside, else ''."""
        if start not in self._hash_cache:
            sec = self.section(start)
            h = ""
            if sec and not sec[4] and not sec[5] and sec[2] is not None and size and not self.relocs_in(start, start + size):
                h = "#" + hashlib.sha1(self.read(start, size)).hexdigest()[:8]
            self._hash_cache[start] = h
        return self._hash_cache[start]

    def symbol_token(self, addr, func=None):
        """Name for an address that has a symbol, or None. func = (start, size) of the current function."""
        if func and func[0] <= addr < func[0] + func[1] and addr != func[0]:
            return f"+{addr - func[0]:#x}"
        sym = self.symbol_at(addr)
        if sym is None:
            return None
        name, start, size = sym
        return self._named(name, addr - start) + self._object_hash(start, size)

    def reloc_target(self, off, end=None):
        """(target address or None, name for an undefined symbol) for the relocation at off.

        end is the address after the instruction, for R_386_PC32."""
        typ, name, value = self.rel[off]
        field = self.word(off)
        if typ == "RELATIVE":
            return field, None
        if not value:  # undefined: imported from tier0, vstdlib or libc
            addend = field - (1 << 32) if field & 0x80000000 else field
            if typ == "PC32":
                addend += (end or off + 4) - off
            return None, self._named(name, addend)
        if typ == "PC32":
            return (value + field - off + (end or off + 4)) & 0xFFFFFFFF, None
        return (value + field) & 0xFFFFFFFF, None

    def address_token(self, addr, op="", args="", imm=True, indexed=False, func=None, depth=0):
        """Token for what addr points at. op and args are the instruction that uses it; imm says the
        address is an immediate ($addr), indexed that it is the displacement of a table access."""
        tok = self.symbol_token(addr, func)
        if tok is not None:
            return tok
        sec = self.section(addr)
        if sec is None:
            return f"?{addr:#x}"
        if sec[4] or sec[5]:  # writable or code but no symbol covers it: nearest symbol before
            prev = self.preceding_symbol(addr)
            return self._named(prev[0], addr - prev[1]) if prev else f"{sec[3]}{addr - sec[0]:+#x}"
        return self.content_token(addr, op, args, imm, indexed, func, depth)

    def content_token(self, addr, op, args, imm, indexed, func, depth):
        memory = not imm and op != "lea"
        kind = fp_kind(op) if memory else None
        if kind is not None:
            label, size, fmt = kind
            if indexed:
                n = max(1, min(DATA_CAP, self.next_bound(addr) - addr) // size)
                return f"{label}[" + ",".join(fp_text(self.read(addr + i * size, size), fmt) for i in range(n)) + "]"
            return f"{label}:{fp_text(self.read(addr, size), fmt)}"
        if memory and not indexed:
            size = operand_size(op, args)
            if size == 4 and addr in self.rel and depth == 0:
                return "*" + self.reloc_token(addr, func=func, depth=depth + 1)
            raw = self.read(addr, size) or b""
            return f"w{size * 8}:{int.from_bytes(raw, 'little'):#x}"
        return self.data_token(addr, func, depth)

    def data_token(self, addr, func, depth):
        """Untyped read-only data: an address table, a C string, or raw bytes."""
        bound = self.next_bound(addr)
        if addr in self.rel and depth == 0:
            entries = []
            a = addr
            while a in self.rel and len(entries) < TABLE_CAP and (a == addr or a < bound):
                entries.append(self.reloc_token(a, func=func, depth=depth + 1))
                a += 4
            return "[" + ",".join(entries) + "]"
        raw = self.read(addr, 4096) or b""
        nul = raw.find(b"\0")
        region = raw[: max(1, bound - addr)]
        if nul >= 0 and all(b in PRINTABLE for b in raw[:nul]):
            text = raw[:nul]
            tail = region[nul + 1:]
            if (nul > 0 or not region[1:].strip(b"\0")) and not tail.strip(b"\0"):
                s = source_path(text.decode("latin-1"))
                if len(s) > STRING_CAP:
                    s = s[:STRING_CAP] + "..." + hashlib.sha1(text).hexdigest()[:8]
                return '"' + s.encode("unicode_escape").decode("ascii").replace('"', '\\"') + '"'
        region = region[:DATA_CAP * 4]
        text = region[:DATA_CAP].hex()
        if len(region) > DATA_CAP:
            text += "..." + hashlib.sha1(region).hexdigest()[:8]
        return "ro:" + text

    def reloc_token(self, off, op="", args="", imm=True, indexed=False, func=None, depth=0, end=None):
        target, undefined = self.reloc_target(off, end)
        if undefined is not None:
            return undefined
        return self.address_token(target, op, args, imm, indexed, func, depth)


def source_path(text):
    """__FILE__ as the tree-relative path. Hidden compiled from dlls/ ("../dlls/../tier1/mempool.cpp"),
    our Docker build from /src ("/src/tier1/mempool.cpp"): the same source file either way."""
    if not SOURCE_PATH.match(text):
        return text
    if text.startswith("/") and "/src/" in text:
        text = text[text.index("/src/") + 5:]
    text = posixpath.normpath(text)
    while text.startswith("../"):
        text = text[3:]
    return text


def fp_kind(op):
    """(label, size, struct format) of an x87 memory operand, from the AT&T suffix, or None."""
    if not op.startswith("f"):
        return None
    if op in ("fldcw", "fnstcw", "fstcw", "fnstsw", "fstsw"):
        return ("w16", 2, "<H")
    if op.startswith("fi"):
        for suffix in ("ll", "l", "s"):
            if op.endswith(suffix):
                return FI_KINDS[suffix]
        return None
    return FP_KINDS.get(op[-1])


def fp_text(raw, fmt):
    if not raw:
        return "?"
    if fmt is None or len(raw) != struct.calcsize(fmt):
        return "0x" + raw[::-1].hex()
    value = struct.unpack(fmt, raw)[0]
    if fmt in ("<f", "<d"):
        # The shortest decimal that gives the same bits back.
        for digits in range(6, 18):
            text = f"{value:.{digits}g}"
            if struct.pack(fmt, float(text)) == raw:
                return text
        return repr(value)
    return f"{value:#x}"


def operand_size(op, args):
    if op.startswith(("movzb", "movsb")):
        return 1
    if op.startswith(("movzw", "movsw")):
        return 2
    if REG32.search(args):
        return 4
    if REG16.search(args):
        return 2
    if REG8.search(args):
        return 1
    return {"b": 1, "w": 2}.get(op[-1:], 4)


def main():
    refs = Refs(sys.argv[1])
    addr = int(sys.argv[2], 16)
    op = sys.argv[3] if len(sys.argv) > 3 else ""
    args = sys.argv[4] if len(sys.argv) > 4 else ""
    print(refs.address_token(addr, op, args, imm=not op))


if __name__ == "__main__":
    main()
