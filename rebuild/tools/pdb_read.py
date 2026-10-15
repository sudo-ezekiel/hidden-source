"""Read Hidden's May 2006 server PDB (MSF 7.00, written by VC 7.1) with the standard library only.

Usage:
    python3 -I pdb_read.py [--pdb PATH] [--base HEX] <command> [args]

Commands (tables are TSV on stdout):
    modules               index, object, source file, number of procs, in link order
    functions             va, size, module, source file, first line, last line, G/L, C++ signature
    locals <substring>    parameters and locals of every function whose name contains <substring>:
                          C++ type, frame offset or register, block depth
    class <name>          size, bases with offsets, vfptr, data members (offset, type, bitfield), static
                          members, methods (vtable offset of each virtual, intro = introduces the slot),
                          nested types; a name that matches nothing lists the classes containing it
    enum <name>           enumerators and values
    globals               global and static variables: name, type, va, module, scope
    constants             S_CONSTANT records: name, type, value, module
    lines <module>        every line record of the modules whose object name contains <module>:
                          va, file, line, function

The PDB records virtual addresses as section:offset; --base is the DLL's image base (0x22000000 for
dll_2006-05-06_patch.dll). Layout notes for each structure are next to the code that reads it.
"""
import argparse
import bisect
import os
import struct
import sys

DEFAULT_PDB = os.path.expanduser("~/.cache/subject51764/hsrebuild/builds/server_2006-05-06_patch.pdb")
DEFAULT_BASE = 0x22000000

# CodeView symbol kinds (VC 7.1, 32-bit type indices, NUL-terminated names)
S_END, S_OBJNAME, S_THUNK32, S_BLOCK32, S_LABEL32, S_REGISTER = 0x0006, 0x1101, 0x1102, 0x1103, 0x1105, 0x1106
S_CONSTANT, S_UDT, S_BPREL32, S_LDATA32, S_GDATA32, S_PUB32 = 0x1107, 0x1108, 0x110b, 0x110c, 0x110d, 0x110e
S_LPROC32, S_GPROC32, S_REGREL32, S_COMPILE2 = 0x110f, 0x1110, 0x1111, 0x1116
S_PROCREF, S_LPROCREF = 0x1125, 0x1127

# Type leaves
LF_VTSHAPE, LF_MODIFIER, LF_POINTER, LF_PROCEDURE, LF_MFUNCTION = 0x000a, 0x1001, 0x1002, 0x1008, 0x1009
LF_ARGLIST, LF_FIELDLIST, LF_BITFIELD, LF_METHODLIST = 0x1201, 0x1203, 0x1205, 0x1206
LF_BCLASS, LF_VBCLASS, LF_IVBCLASS, LF_INDEX, LF_VFUNCTAB = 0x1400, 0x1401, 0x1402, 0x1404, 0x1409
LF_ENUMERATE, LF_ARRAY, LF_CLASS, LF_STRUCTURE, LF_UNION, LF_ENUM = 0x1502, 0x1503, 0x1504, 0x1505, 0x1506, 0x1507
LF_MEMBER, LF_STMEMBER, LF_METHOD, LF_NESTTYPE, LF_ONEMETHOD = 0x150d, 0x150e, 0x150f, 0x1510, 0x1511
UDT_LEAVES = (LF_CLASS, LF_STRUCTURE, LF_UNION, LF_ENUM)
PROP_FWDREF = 0x80

PRIMITIVES = {0x00: "<notype>", 0x03: "void", 0x08: "HRESULT", 0x10: "signed char", 0x20: "unsigned char",
              0x70: "char", 0x71: "wchar_t", 0x68: "__int8", 0x69: "unsigned __int8", 0x11: "short",
              0x21: "unsigned short", 0x72: "__int16", 0x73: "unsigned __int16", 0x12: "long",
              0x22: "unsigned long", 0x74: "int", 0x75: "unsigned int", 0x13: "__int64", 0x23: "unsigned __int64",
              0x76: "__int64", 0x77: "unsigned __int64", 0x40: "float", 0x41: "double", 0x42: "long double",
              0x30: "bool", 0x31: "bool16", 0x32: "bool32"}
PRIMITIVE_SIZE = {0x00: 0, 0x03: 0, 0x08: 4, 0x10: 1, 0x20: 1, 0x70: 1, 0x71: 2, 0x68: 1, 0x69: 1, 0x11: 2,
                  0x21: 2, 0x72: 2, 0x73: 2, 0x12: 4, 0x22: 4, 0x74: 4, 0x75: 4, 0x13: 8, 0x23: 8, 0x76: 8,
                  0x77: 8, 0x40: 4, 0x41: 8, 0x42: 10, 0x30: 1, 0x31: 2, 0x32: 4}

REGISTERS = {1: "al", 2: "cl", 3: "dl", 4: "bl", 5: "ah", 6: "ch", 7: "dh", 8: "bh", 9: "ax", 10: "cx", 11: "dx",
             12: "bx", 13: "sp", 14: "bp", 15: "si", 16: "di", 17: "eax", 18: "ecx", 19: "edx", 20: "ebx",
             21: "esp", 22: "ebp", 23: "esi", 24: "edi"}
REGISTERS.update({128 + i: "st%d" % i for i in range(8)})
REGISTERS.update({146 + i: "mm%d" % i for i in range(8)})
REGISTERS.update({154 + i: "xmm%d" % i for i in range(8)})

ACCESS = {0: "", 1: "private", 2: "protected", 3: "public"}
# CV_fldattr_t.mprop: 0 vanilla, 1 virtual, 2 static, 3 friend, 4 introducing virtual, 5 pure, 6 pure intro
MPROP = {0: "", 1: "virtual", 2: "static", 3: "friend", 4: "virtual intro", 5: "pure virtual",
         6: "pure virtual intro"}
INTRO = (4, 6)


def split_scope(name):
    """A qualified name split at the :: outside template arguments."""
    parts, depth, start, i = [], 0, 0, 0
    while i < len(name):
        c = name[i]
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif depth == 0 and name.startswith("::", i):
            parts.append(name[start:i])
            start = i + 2
            i += 1
        i += 1
    return parts + [name[start:]]


def cstr(b, p):
    """NUL-terminated latin-1 string at p, and the position after the NUL."""
    e = b.index(b"\0", p)
    return b[p:e].decode("latin-1"), e + 1


def numeric(b, p):
    """CodeView numeric leaf at p: values below 0x8000 are inline, larger ones name their width."""
    v, = struct.unpack_from("<H", b, p)
    if v < 0x8000:
        return v, p + 2
    fmt = {0x8000: "<b", 0x8001: "<h", 0x8002: "<H", 0x8003: "<i", 0x8004: "<I", 0x8005: "<f", 0x8006: "<d",
           0x8009: "<q", 0x800a: "<Q"}[v]
    return struct.unpack_from(fmt, b, p + 2)[0], p + 2 + struct.calcsize(fmt)


class MSF:
    """MSF 7.00 container: superblock at 0, block map address at +52, stream directory spread over blocks."""

    def __init__(self, path):
        self.data = open(path, "rb").read()
        if not self.data.startswith(b"Microsoft C/C++ MSF 7.00\r\n\x1aDS"):
            raise SystemExit("%s: not an MSF 7.00 PDB" % path)
        bs, _, _, dirbytes, _, bmaddr = struct.unpack_from("<6I", self.data, 32)
        self.bs = bs
        nblk = lambda n: (n + bs - 1) // bs
        dirblocks = struct.unpack_from("<%dI" % nblk(dirbytes), self.data, bmaddr * bs)
        d = b"".join(self.data[b * bs:(b + 1) * bs] for b in dirblocks)[:dirbytes]
        count, = struct.unpack_from("<I", d, 0)
        self.sizes = struct.unpack_from("<%dI" % count, d, 4)
        pos = 4 + 4 * count
        self.blocks = []
        for s in self.sizes:
            n = 0 if s == 0xffffffff else nblk(s)
            self.blocks.append(struct.unpack_from("<%dI" % n, d, pos))
            pos += 4 * n
        self.cache = {}

    def stream(self, i):
        if i not in self.cache:
            bs = self.bs
            size = self.sizes[i] if self.sizes[i] != 0xffffffff else 0
            self.cache[i] = b"".join(self.data[b * bs:(b + 1) * bs] for b in self.blocks[i])[:size]
        return self.cache[i]


class Module:
    def __init__(self, index, name, obj, symstream, symbytes, c11bytes, contrib):
        self.index, self.name, self.obj = index, name, obj
        self.symstream, self.symbytes, self.c11bytes = symstream, symbytes, c11bytes
        self.contrib = contrib
        self.files = []        # DBI file info: every source file the module names
        self.procs = []
        self.data = []         # S_LDATA32 / S_GDATA32 in the module stream
        self.constants = []
        self.udts = []
        self.labels = []
        self.thunks = []
        self.lines = []        # (file, seg, [(offset, line)])
        self.source = ""

    @property
    def short(self):
        return self.name.rsplit("\\", 1)[-1]


class Proc:
    def __init__(self, module, kind, name, ti, seg, off, size):
        self.module, self.kind, self.name, self.ti = module, kind, name, ti
        self.seg, self.off, self.size = seg, off, size
        self.children = []     # (depth, kind, fields) in record order
        self.file, self.first, self.last = "", 0, 0
        self.lines = []


class Types:
    """TPI stream (V70 header: version, header size, first and last+1 type index, record bytes)."""

    def __init__(self, tpi):
        version, hsize, self.lo, self.hi, nbytes = struct.unpack_from("<5I", tpi, 0)
        if version != 19990903:
            raise SystemExit("unexpected TPI version %d" % version)
        self.recs = []
        p = hsize
        while p < hsize + nbytes:
            ln, leaf = struct.unpack_from("<HH", tpi, p)
            self.recs.append((leaf, tpi[p + 4:p + 2 + ln]))
            p += 2 + ln
        if len(self.recs) != self.hi - self.lo:
            raise SystemExit("TPI holds %d records, header says %d" % (len(self.recs), self.hi - self.lo))
        self.parsed = {}
        self.defs = {}
        for i in range(len(self.recs)):
            ti = self.lo + i
            if self.recs[i][0] in UDT_LEAVES:
                r = self.get(ti)
                if not r["prop"] & PROP_FWDREF:
                    self.defs.setdefault(r["name"], ti)

    def get(self, ti):
        if ti in self.parsed:
            return self.parsed[ti]
        leaf, b = self.recs[ti - self.lo]
        r = {"leaf": leaf}
        if leaf == LF_MODIFIER:
            r["type"], attr = struct.unpack_from("<IH", b, 0)
            r["const"], r["volatile"] = bool(attr & 1), bool(attr & 2)
        elif leaf == LF_POINTER:
            r["type"], attr = struct.unpack_from("<II", b, 0)
            r["mode"] = (attr >> 5) & 7          # 0 pointer, 1 reference, 2 member data, 3 member function
            r["const"], r["volatile"] = bool(attr & 0x400), bool(attr & 0x200)
            if r["mode"] in (2, 3):
                r["class"], = struct.unpack_from("<I", b, 8)
        elif leaf == LF_PROCEDURE:
            r["ret"], r["call"], _, r["nargs"], r["args"] = struct.unpack_from("<IBBHI", b, 0)
        elif leaf == LF_MFUNCTION:
            (r["ret"], r["class"], r["this"], r["call"], _, r["nargs"], r["args"],
             r["thisadjust"]) = struct.unpack_from("<IIIBBHIi", b, 0)
        elif leaf == LF_ARGLIST:
            n, = struct.unpack_from("<I", b, 0)
            r["types"] = list(struct.unpack_from("<%dI" % n, b, 4))
        elif leaf == LF_BITFIELD:
            r["type"], r["bits"], r["pos"] = struct.unpack_from("<IBB", b, 0)
        elif leaf == LF_ARRAY:
            r["type"], r["index"] = struct.unpack_from("<II", b, 0)
            r["size"], p = numeric(b, 8)
            r["name"], _ = cstr(b, p)
        elif leaf in (LF_CLASS, LF_STRUCTURE):
            r["count"], r["prop"], r["fields"], r["derived"], r["vshape"] = struct.unpack_from("<HHIII", b, 0)
            r["size"], p = numeric(b, 16)
            r["name"], _ = cstr(b, p)
        elif leaf == LF_UNION:
            r["count"], r["prop"], r["fields"] = struct.unpack_from("<HHI", b, 0)
            r["size"], p = numeric(b, 8)
            r["name"], _ = cstr(b, p)
        elif leaf == LF_ENUM:
            r["count"], r["prop"], r["type"], r["fields"] = struct.unpack_from("<HHII", b, 0)
            r["name"], _ = cstr(b, 12)
        elif leaf == LF_METHODLIST:
            r["methods"] = []
            p = 0
            while p < len(b):
                attr, _, ti2 = struct.unpack_from("<HHI", b, p)
                p += 8
                vtoff = None
                if (attr >> 2) & 7 in INTRO:
                    vtoff, = struct.unpack_from("<I", b, p)
                    p += 4
                r["methods"].append((attr, ti2, vtoff))
        elif leaf == LF_VTSHAPE:
            r["count"], = struct.unpack_from("<H", b, 0)
        elif leaf == LF_FIELDLIST:
            r["members"] = self.fieldlist(b)
        else:
            raise SystemExit("type 0x%x: unhandled leaf 0x%04x" % (ti, leaf))
        self.parsed[ti] = r
        return r

    @staticmethod
    def fieldlist(b):
        """Sub-records of an LF_FIELDLIST, each padded to 4 with 0xf1..0xf3 bytes."""
        out = []
        p = 0
        while p < len(b):
            if b[p] > 0xf0:
                p += b[p] & 0x0f
                continue
            leaf, attr = struct.unpack_from("<HH", b, p)
            m = {"leaf": leaf, "attr": attr}
            if leaf == LF_BCLASS:
                m["type"], = struct.unpack_from("<I", b, p + 4)
                m["offset"], p = numeric(b, p + 8)
            elif leaf in (LF_VBCLASS, LF_IVBCLASS):
                m["type"], m["vbptr"] = struct.unpack_from("<II", b, p + 4)
                m["vbpoff"], p = numeric(b, p + 12)
                m["vboff"], p = numeric(b, p)
            elif leaf in (LF_VFUNCTAB, LF_INDEX):
                m["type"], = struct.unpack_from("<I", b, p + 4)
                p += 8
            elif leaf == LF_ENUMERATE:
                m["value"], p = numeric(b, p + 4)
                m["name"], p = cstr(b, p)
            elif leaf == LF_MEMBER:
                m["type"], = struct.unpack_from("<I", b, p + 4)
                m["offset"], p = numeric(b, p + 8)
                m["name"], p = cstr(b, p)
            elif leaf in (LF_STMEMBER, LF_NESTTYPE):
                m["type"], = struct.unpack_from("<I", b, p + 4)
                m["name"], p = cstr(b, p + 8)
            elif leaf == LF_METHOD:
                m["count"] = attr
                m["list"], = struct.unpack_from("<I", b, p + 4)
                m["name"], p = cstr(b, p + 8)
            elif leaf == LF_ONEMETHOD:
                m["type"], = struct.unpack_from("<I", b, p + 4)
                p += 8
                m["vtoff"] = None
                if (attr >> 2) & 7 in INTRO:
                    m["vtoff"], = struct.unpack_from("<I", b, p)
                    p += 4
                m["name"], p = cstr(b, p)
            else:
                raise SystemExit("field list: unhandled leaf 0x%04x" % leaf)
            out.append(m)
        return out

    def members(self, fields):
        """Field list members, following LF_INDEX continuations."""
        out = []
        while fields:
            ms = self.get(fields)["members"]
            fields = 0
            for m in ms:
                if m["leaf"] == LF_INDEX:
                    fields = m["type"]
                else:
                    out.append(m)
        return out

    def resolve(self, ti):
        """A forward-referenced class, struct, union or enum resolved to its definition by name."""
        if ti < self.lo:
            return ti
        r = self.get(ti)
        if r["leaf"] in UDT_LEAVES and r["prop"] & PROP_FWDREF:
            return self.defs.get(r["name"], ti)
        return ti

    def sizeof(self, ti):
        if ti < self.lo:
            if (ti >> 8) & 7:
                return 4
            return PRIMITIVE_SIZE.get(ti & 0xff, 0)
        r = self.get(self.resolve(ti))
        if r["leaf"] == LF_MODIFIER:
            return self.sizeof(r["type"])
        if r["leaf"] == LF_POINTER:
            return 4
        if r["leaf"] in (LF_ARRAY, LF_CLASS, LF_STRUCTURE, LF_UNION):
            return r["size"]
        if r["leaf"] == LF_ENUM:
            return self.sizeof(r["type"])
        return 0

    def args(self, ti):
        types = self.get(ti)["types"]
        if not types:
            return "void"
        return ", ".join("..." if t == 0 else self.decl(t) for t in types)

    def decl(self, ti, inner=""):
        """C++ declaration of type ti around the declarator `inner` (a name, '*p', '(*pfn)', ...)."""
        sep = " " if inner and not inner.startswith("[") else ""
        if ti < self.lo:
            if ti >> 8 & 7:
                return self.decl(ti & 0xff, "*" + inner)
            return PRIMITIVES.get(ti & 0xff, "<prim 0x%x>" % ti) + sep + inner
        r = self.get(ti)
        leaf = r["leaf"]
        if leaf == LF_MODIFIER:
            q = ("const " if r["const"] else "") + ("volatile " if r["volatile"] else "")
            return q + self.decl(r["type"], inner)
        if leaf == LF_POINTER:
            mark = {0: "*", 1: "&", 2: None, 3: None}[r["mode"]]
            if mark is None:
                mark = self.decl(r["class"]) + "::*"
            if r["const"]:
                mark += " const"
            if r["volatile"]:
                mark += " volatile"
            target = self.get(r["type"]) if r["type"] >= self.lo else {"leaf": 0}
            s = mark + (" " if inner and r["const"] else "") + inner
            if target["leaf"] in (LF_PROCEDURE, LF_MFUNCTION, LF_ARRAY):
                s = "(" + s + ")"
            return self.decl(r["type"], s)
        if leaf == LF_ARRAY:
            elem = self.sizeof(r["type"])
            n = r["size"] // elem if elem else 0
            return self.decl(r["type"], inner + ("[%d]" % n if n else "[]"))
        if leaf in (LF_PROCEDURE, LF_MFUNCTION):
            s = inner + "(" + self.args(r["args"]) + ")"
            if leaf == LF_MFUNCTION and self.is_const_this(r["this"]):
                s += " const"
            return self.decl(r["ret"], s)
        if leaf == LF_BITFIELD:
            return self.decl(r["type"], inner) + " : %d" % r["bits"]
        if leaf in UDT_LEAVES:
            return r["name"] + sep + inner
        return "<leaf 0x%x>" % leaf + sep + inner

    def is_const_this(self, this):
        if not this or this < self.lo:
            return False
        p = self.get(this)
        if p["leaf"] != LF_POINTER or p["type"] < self.lo:
            return False
        t = self.get(p["type"])
        return t["leaf"] == LF_MODIFIER and t["const"]

    def find(self, name, leaves):
        ti = self.defs.get(name)
        if ti and self.get(ti)["leaf"] in leaves:
            return ti
        return None

    def names_like(self, sub, leaves):
        sub = sub.lower()
        return sorted(n for n, ti in self.defs.items() if sub in n.lower() and self.get(ti)["leaf"] in leaves)

    def methods(self, cls_ti):
        """(name, attr, function type, vtable offset or None) for every method of a class, in order."""
        r = self.get(self.resolve(cls_ti))
        out = []
        if r["leaf"] not in (LF_CLASS, LF_STRUCTURE, LF_UNION) or not r["fields"]:
            return out
        for m in self.members(r["fields"]):
            if m["leaf"] == LF_ONEMETHOD:
                out.append((m["name"], m["attr"], m["type"], m["vtoff"]))
            elif m["leaf"] == LF_METHOD:
                out.extend((m["name"], a, ti, vt) for a, ti, vt in self.get(m["list"])["methods"])
        return out

    def bases(self, cls_ti):
        r = self.get(self.resolve(cls_ti))
        if r["leaf"] not in (LF_CLASS, LF_STRUCTURE) or not r["fields"]:
            return []
        return [(m["type"], m["offset"]) for m in self.members(r["fields"]) if m["leaf"] == LF_BCLASS]

    def overrides(self, a, b):
        """Whether method types a and b take the same arguments with the same constness."""
        ra, rb = self.get(a), self.get(b)
        return (self.args(ra["args"]) == self.args(rb["args"])
                and self.is_const_this(ra.get("this")) == self.is_const_this(rb.get("this")))

    def vslot(self, cls_ti, name, mti, suboff=0):
        """Where a virtual method's slot is introduced: (vtable offset, offset of that base's subobject in
        the class, introducing class name), searching the bases for overrides. None if not found."""
        for n, attr, ti, vtoff in self.methods(cls_ti):
            same = n == name or (n.startswith("~") and name.startswith("~"))
            if same and (attr >> 2) & 7 in INTRO and self.overrides(ti, mti):
                return vtoff, suboff, self.get(self.resolve(cls_ti))["name"]
        for base, off in self.bases(cls_ti):
            hit = self.vslot(base, name, mti, suboff + off)
            if hit:
                return hit
        return None


class PDB:
    def __init__(self, path, base=DEFAULT_BASE):
        self.msf = MSF(path)
        self.base = base
        dbi = self.msf.stream(3)
        (sig, version, _age, self.gsi_stream, _build, self.psi_stream, _dllver, self.symrec_stream, _rbld,
         mi, sc, sm, fi, ts, _mfc, od, ec, _flags, _machine) = struct.unpack_from("<iIIHHHHHHiiiiiIiiHH", dbi, 0)
        if sig != -1 or version != 19990903:
            raise SystemExit("unexpected DBI header %d %d" % (sig, version))
        p = 64
        self.contribs = self.read_contribs(dbi[p + mi:p + mi + sc])
        self.modules = self.read_modules(dbi[p:p + mi])
        self.read_file_info(dbi[p + mi + sc + sm:p + mi + sc + sm + fi])
        opt = struct.unpack_from("<%dH" % (od // 2), dbi, p + mi + sc + sm + fi + ts + ec)
        sh = self.msf.stream(opt[5])
        self.sections = [struct.unpack_from("<I", sh, i * 40 + 12)[0] for i in range(len(sh) // 40)]
        self._types = None
        self._loaded = False

    # --- DBI substreams -------------------------------------------------------------------------------

    @staticmethod
    def read_contribs(b):
        """Section contributions, V60: version word, then 28-byte entries (section, offset, size,
        characteristics, module index, data and reloc CRCs)."""
        ver, = struct.unpack_from("<I", b, 0)
        if ver != 0xeffe0000 + 19970605:
            raise SystemExit("unexpected section contribution version 0x%x" % ver)
        out = []
        for p in range(4, len(b), 28):
            sec, off, size, _ch, mod = struct.unpack_from("<HxxiiIH", b, p)
            out.append((sec, off, size, mod))
        out.sort()
        return out

    def read_modules(self, b):
        """Module records: 64-byte header (the first section contribution at +4, symbol stream u16 at +34,
        symbol bytes at +36, C11 line bytes at +40, C13 line bytes at +44), module name and object name,
        each NUL-terminated, then padding to 4."""
        mods = []
        p = 0
        while p < len(b):
            symstream, symbytes, c11bytes, c13bytes = struct.unpack_from("<HIII", b, p + 34)
            contrib = struct.unpack_from("<HxxiiIH", b, p + 4)
            name, q = cstr(b, p + 64)
            obj, q = cstr(b, q)
            if c13bytes:
                raise SystemExit("%s: C13 line info is not handled" % name)
            mods.append(Module(len(mods), name, obj, symstream, symbytes, c11bytes, contrib))
            p = (q + 3) & ~3
        return mods

    def read_file_info(self, b):
        """File info: module count, (truncated) file count, per-module first index and file count,
        then the name offsets and the names buffer."""
        nmod, _ = struct.unpack_from("<HH", b, 0)
        counts = struct.unpack_from("<%dH" % nmod, b, 4 + 2 * nmod)
        total = sum(counts)
        offs = struct.unpack_from("<%dI" % total, b, 4 + 4 * nmod)
        names = 4 + 4 * nmod + 4 * total
        i = 0
        for m, n in zip(self.modules, counts):
            m.files = [cstr(b, names + offs[i + k])[0] for k in range(n)]
            i += n

    # --- addresses ------------------------------------------------------------------------------------

    def va(self, seg, off):
        if 0 < seg <= len(self.sections):
            return self.base + self.sections[seg - 1] + off
        return 0

    def module_at(self, seg, off):
        i = bisect.bisect_right(self.contribs, (seg, off, 1 << 31, 1 << 16)) - 1
        if i >= 0:
            s, o, size, mod = self.contribs[i]
            if s == seg and o <= off < o + size:
                return self.modules[mod]
        return None

    # --- module streams -------------------------------------------------------------------------------

    @property
    def types(self):
        if self._types is None:
            self._types = Types(self.msf.stream(2))
        return self._types

    def load(self):
        if self._loaded:
            return
        self._loaded = True
        for m in self.modules:
            if m.symstream == 0xffff:
                continue
            s = self.msf.stream(m.symstream)
            self.read_symbols(m, s)
            self.read_lines(m, s[m.symbytes:m.symbytes + m.c11bytes])
            self.attach_lines(m)

    def read_symbols(self, m, s):
        """The module's CodeView symbols, after a 4-byte signature. Procs, blocks and thunks open a
        scope that S_END closes; the records inside a proc become its children with their depth."""
        r = 4
        scopes = []            # open scopes; a proc entry is the Proc, blocks and thunks are None
        proc = None
        while r < m.symbytes:
            ln, kind = struct.unpack_from("<HH", s, r)
            b = r + 4
            r += 2 + ln
            depth = len(scopes) - 1
            if kind in (S_GPROC32, S_LPROC32):
                size, = struct.unpack_from("<I", s, b + 12)
                ti, off, seg = struct.unpack_from("<IIH", s, b + 24)
                name, _ = cstr(s, b + 35)
                proc = Proc(m, "G" if kind == S_GPROC32 else "L", name, ti, seg, off, size)
                m.procs.append(proc)
                scopes.append(proc)
            elif kind == S_BLOCK32:
                size, off, seg = struct.unpack_from("<IIH", s, b + 8)
                name, _ = cstr(s, b + 18)
                if proc:
                    proc.children.append((depth + 1, "block", {"va": self.va(seg, off), "size": size,
                                                                "name": name}))
                scopes.append(None)
            elif kind == S_THUNK32:
                off, seg, size, _ord = struct.unpack_from("<IHHB", s, b + 12)
                name, _ = cstr(s, b + 21)
                m.thunks.append({"name": name, "va": self.va(seg, off), "size": size})
                scopes.append(None)
            elif kind == S_END:
                if scopes:
                    scopes.pop()
                if not scopes:
                    proc = None
            elif kind == S_BPREL32:
                off, ti = struct.unpack_from("<iI", s, b)
                name, _ = cstr(s, b + 8)
                self.child(proc, depth, "bprel", {"off": off, "type": ti, "name": name})
            elif kind == S_REGISTER:
                ti, reg = struct.unpack_from("<IH", s, b)
                name, _ = cstr(s, b + 6)
                self.child(proc, depth, "register", {"reg": reg, "type": ti, "name": name})
            elif kind == S_REGREL32:
                off, ti, reg = struct.unpack_from("<iIH", s, b)
                name, _ = cstr(s, b + 10)
                self.child(proc, depth, "regrel", {"off": off, "reg": reg, "type": ti, "name": name})
            elif kind in (S_LDATA32, S_GDATA32):
                ti, off, seg = struct.unpack_from("<IIH", s, b)
                name, _ = cstr(s, b + 10)
                d = {"type": ti, "va": self.va(seg, off), "name": name, "kind": "L" if kind == S_LDATA32 else "G",
                     "module": m, "scope": proc.name if proc else ""}
                m.data.append(d)
                self.child(proc, depth, "static", d)
            elif kind == S_CONSTANT:
                ti, = struct.unpack_from("<I", s, b)
                value, p = numeric(s, b + 4)
                name, _ = cstr(s, p)
                d = {"type": ti, "value": value, "name": name, "module": m, "scope": proc.name if proc else ""}
                m.constants.append(d)
                self.child(proc, depth, "constant", d)
            elif kind == S_UDT:
                ti, = struct.unpack_from("<I", s, b)
                name, _ = cstr(s, b + 4)
                m.udts.append((ti, name))
                self.child(proc, depth, "typedef", {"type": ti, "name": name})
            elif kind == S_LABEL32:
                off, seg, _flags = struct.unpack_from("<IHB", s, b)
                name, _ = cstr(s, b + 7)
                m.labels.append((self.va(seg, off), name))
                self.child(proc, depth, "label", {"va": self.va(seg, off), "name": name})
            elif kind in (S_OBJNAME, S_COMPILE2):
                pass
            else:
                raise SystemExit("%s: unhandled symbol kind 0x%04x" % (m.name, kind))

    @staticmethod
    def child(proc, depth, kind, fields):
        if proc is not None:
            proc.children.append((depth, kind, fields))

    def read_lines(self, m, b):
        """C11 line info: file count and segment count, the offset of each file's table, the module's
        segment ranges and numbers. Each file table: segment count, the offset of each segment's line
        block, the segment ranges, the file name (NUL-terminated). Each line block: segment, pair count,
        the code offsets, then the line numbers (u16)."""
        if not b:
            return
        nfile, _nseg = struct.unpack_from("<HH", b, 0)
        for fbase in struct.unpack_from("<%dI" % nfile, b, 4):
            nseg, = struct.unpack_from("<H", b, fbase)
            lbases = struct.unpack_from("<%dI" % nseg, b, fbase + 4)
            name, _ = cstr(b, fbase + 4 + 12 * nseg)
            for lb in lbases:
                seg, npair = struct.unpack_from("<HH", b, lb)
                offs = struct.unpack_from("<%dI" % npair, b, lb + 4)
                lines = struct.unpack_from("<%dH" % npair, b, lb + 4 + 4 * npair)
                m.lines.append((name, seg, list(zip(offs, lines))))

    def attach_lines(self, m):
        """Give each proc the lines inside its range: its file is the one that holds its first byte."""
        procs = sorted(m.procs, key=lambda p: (p.seg, p.off))
        starts = [(p.seg, p.off) for p in procs]
        count = {}
        for name, seg, pairs in m.lines:
            for off, line in pairs:
                i = bisect.bisect_right(starts, (seg, off)) - 1
                if i < 0:
                    continue
                p = procs[i]
                if off >= p.off + max(p.size, 1):
                    continue
                p.lines.append((off, name, line))
        for p in procs:
            p.lines.sort()
            if not p.lines:
                continue
            at_start = [x for x in p.lines if x[0] == p.lines[0][0]]
            p.file = at_start[0][1]
            mine = [line for _, f, line in p.lines if f == p.file]
            p.first, p.last = min(mine), max(mine)
            count[p.file] = count.get(p.file, 0) + 1
        stem = os.path.splitext(m.short)[0].lower()
        for f in m.files:
            if os.path.splitext(f.rsplit("\\", 1)[-1])[0].lower() == stem and f.lower().endswith((".cpp", ".c")):
                m.source = f
                break
        else:
            srcs = [f for f in count if f.lower().endswith((".cpp", ".c", ".cxx"))]
            m.source = max(srcs, key=lambda f: count[f]) if srcs else ""

    # --- global symbol stream -------------------------------------------------------------------------

    def global_symbols(self):
        """S_GDATA32, S_LDATA32, S_CONSTANT and S_UDT records of the global symbol record stream."""
        s = self.msf.stream(self.symrec_stream)
        p = 0
        while p < len(s):
            ln, kind = struct.unpack_from("<HH", s, p)
            b = p + 4
            p += 2 + ln
            if kind in (S_GDATA32, S_LDATA32):
                ti, off, seg = struct.unpack_from("<IIH", s, b)
                yield kind, {"type": ti, "seg": seg, "off": off, "name": cstr(s, b + 10)[0]}
            elif kind == S_CONSTANT:
                ti, = struct.unpack_from("<I", s, b)
                value, q = numeric(s, b + 4)
                yield kind, {"type": ti, "value": value, "name": cstr(s, q)[0]}

    # --- signatures -----------------------------------------------------------------------------------

    def signature(self, proc):
        t = self.types
        if proc.ti < t.lo:
            return proc.name
        r = t.get(proc.ti)
        if r["leaf"] not in (LF_PROCEDURE, LF_MFUNCTION):
            return proc.name
        args = "(" + t.args(r["args"]) + ")"
        prefix = ""
        short = split_scope(proc.name)[-1]
        if r["leaf"] == LF_MFUNCTION:
            if not r["this"]:
                prefix = "static "
            attrs = [a for n, a, ti, _ in t.methods(r["class"]) if n == short and ti == proc.ti]
            if attrs and (attrs[0] >> 2) & 7 in (1, 4, 5, 6):
                prefix = "virtual "
            if t.is_const_this(r["this"]):
                args += " const"
        scope = split_scope(proc.name)
        ctor = len(scope) > 1 and (short.startswith("~") or short.split("<")[0] == scope[-2].split("<")[0])
        conversion = short.startswith("operator ") and short.split()[1] not in ("new", "delete", "new[]", "delete[]")
        if ctor or conversion:
            return prefix + proc.name + args
        return prefix + t.decl(r["ret"], proc.name + args)


# --- commands ---------------------------------------------------------------------------------------------


def cmd_modules(pdb, _args):
    pdb.load()
    print("index\tobject\tsource\tprocs")
    for m in pdb.modules:
        print("%d\t%s\t%s\t%d" % (m.index, m.name, m.source, len(m.procs)))


def cmd_functions(pdb, _args):
    pdb.load()
    print("va\tsize\tmodule\tfile\tfirst\tlast\tkind\tsignature")
    for m in pdb.modules:
        for p in m.procs:
            print("%08x\t%d\t%s\t%s\t%d\t%d\t%s\t%s" % (pdb.va(p.seg, p.off), p.size, m.short, p.file, p.first,
                                                       p.last, p.kind, pdb.signature(p)))


def location(kind, f):
    """S_BPREL32 offsets are from the frame base (the return address in the frame-pointer-omitted functions,
    so the first stack parameter is at +4). S_REGISTER numbers in this PDB are one above CV_HREG_e, checked
    against the code: `this` (in ecx) is 19, SetBit's pBuf and nBit (edx, ecx) are 20 and 19, write_char's
    ch (al) is 2, _abstract_cw's cw (bx) is 13. S_REGREL32 uses the plain numbers (ebp 22, esp 21)."""
    if kind == "bprel":
        return "frame%+d" % f["off"]
    if kind == "regrel":
        return "%s%+d" % (reg_name(f["reg"]), f["off"])
    return reg_name(f["reg"] - 1)


def reg_name(reg):
    if reg in REGISTERS:
        return REGISTERS[reg]
    if reg > 0xff and reg & 0xff in REGISTERS and reg >> 8 in REGISTERS:
        return "%s:%s" % (REGISTERS[reg >> 8], REGISTERS[reg & 0xff])
    return "reg%d" % reg


def cmd_locals(pdb, args):
    pdb.load()
    t = pdb.types
    for m in pdb.modules:
        for p in m.procs:
            if args.substring not in p.name:
                continue
            print("# %s\n# %08x size %d, %s, %s lines %d-%d" % (pdb.signature(p), pdb.va(p.seg, p.off), p.size,
                                                               m.short, p.file, p.first, p.last))
            nargs = 0
            if p.ti >= t.lo and t.get(p.ti)["leaf"] in (LF_PROCEDURE, LF_MFUNCTION):
                r = t.get(p.ti)
                nargs = len([a for a in t.get(r["args"])["types"] if a])
                if r["leaf"] == LF_MFUNCTION and r["this"]:
                    nargs += 1
            print("depth\trole\tlocation\ttype\tname")
            seen = 0
            for depth, kind, f in p.children:
                if kind in ("bprel", "register", "regrel"):
                    role = "param" if seen < nargs and depth == 0 else "local"
                    seen += 1
                    print("%d\t%s\t%s\t%s\t%s" % (depth, role, location(kind, f), t.decl(f["type"]), f["name"]))
                elif kind == "block":
                    print("%d\tblock\t%08x\tsize %d\t%s" % (depth, f["va"], f["size"], f["name"]))
                elif kind == "static":
                    print("%d\tstatic\t%08x\t%s\t%s" % (depth, f["va"], t.decl(f["type"]), f["name"]))
                elif kind == "constant":
                    print("%d\tconstant\t%s\t%s\t%s" % (depth, f["value"], t.decl(f["type"]), f["name"]))
                elif kind == "typedef":
                    print("%d\ttypedef\t\t%s\t%s" % (depth, t.decl(f["type"]), f["name"]))
                elif kind == "label":
                    print("%d\tlabel\t%08x\t\t%s" % (depth, f["va"], f["name"]))
            print()


def cmd_class(pdb, args):
    t = pdb.types
    leaves = (LF_CLASS, LF_STRUCTURE, LF_UNION)
    ti = t.find(args.name, leaves)
    if ti is None:
        for n in t.names_like(args.name, leaves):
            print(n)
        return
    r = t.get(ti)
    kind = {LF_CLASS: "class", LF_STRUCTURE: "struct", LF_UNION: "union"}[r["leaf"]]
    print("# %s %s: size 0x%x (%d), type 0x%x" % (kind, r["name"], r["size"], r["size"], ti))
    print("kind\toffset\ttype\tname\tnote")
    for m in t.members(r["fields"]) if r["fields"] else []:
        leaf, attr = m["leaf"], m["attr"]
        access = ACCESS[attr & 3]
        if leaf == LF_BCLASS:
            print("base\t0x%x\t%s\t\t%s" % (m["offset"], t.decl(m["type"]), access))
        elif leaf in (LF_VBCLASS, LF_IVBCLASS):
            print("%s\t\t%s\t\t%s vbptr +0x%x, slot %d" % ("vbase" if leaf == LF_VBCLASS else "ivbase",
                                                         t.decl(m["type"]), access, m["vbpoff"], m["vboff"]))
        elif leaf == LF_VFUNCTAB:
            print("vfptr\t0x0\t%s\t\t" % t.decl(m["type"]))
        elif leaf == LF_MEMBER:
            mt = m["type"]
            note = access
            if mt >= t.lo and t.get(mt)["leaf"] == LF_BITFIELD:
                bf = t.get(mt)
                note += " bits %d at %d" % (bf["bits"], bf["pos"])
                mt = bf["type"]
            print("member\t0x%x\t%s\t%s\t%s" % (m["offset"], t.decl(mt), m["name"], note))
        elif leaf == LF_STMEMBER:
            print("static\t\t%s\t%s\t%s" % (t.decl(m["type"]), m["name"], access))
        elif leaf == LF_NESTTYPE:
            print("nested\t\t%s\t%s\t" % (t.decl(m["type"]), m["name"]))
        elif leaf in (LF_ONEMETHOD, LF_METHOD):
            entries = [(attr, m["type"], m["vtoff"])] if leaf == LF_ONEMETHOD else t.get(m["list"])["methods"]
            ctor = m["name"] == r["name"].rsplit("::", 1)[-1].split("<")[0] or m["name"].startswith("~")
            for a, mti, vtoff in entries:
                mprop = (a >> 2) & 7
                note = [ACCESS[a & 3], MPROP[mprop], "compiler" if a & 0x200 else ""]
                slot = ""
                if mprop in (1, 5):
                    hit = t.vslot(ti, m["name"], mti)
                    if hit:
                        vtoff, sub, intro = hit
                        note.append("from " + intro)
                        if sub:
                            note.append("vfptr at +0x%x" % sub)
                if vtoff is not None:
                    slot = "vt+0x%x" % vtoff
                text = t.decl(mti, m["name"])
                if ctor and text.startswith("void "):
                    text = text[5:]
                print("method\t%s\t%s\t%s\t%s" % (slot, text, m["name"], " ".join(x for x in note if x)))
        else:
            print("leaf 0x%x" % leaf)


def cmd_enum(pdb, args):
    t = pdb.types
    ti = t.find(args.name, (LF_ENUM,))
    if ti is None:
        for n in t.names_like(args.name, (LF_ENUM,)):
            print(n)
        return
    r = t.get(ti)
    print("# enum %s: underlying %s, %d values, type 0x%x" % (r["name"], t.decl(r["type"]), r["count"], ti))
    print("value\tname")
    for m in t.members(r["fields"]):
        print("%d\t%s" % (m["value"], m["name"]))


def cmd_globals(pdb, _args):
    pdb.load()
    t = pdb.types
    print("name\ttype\tva\tmodule\tscope")
    seen = set()
    for m in pdb.modules:
        for d in m.data:
            seen.add((d["name"], d["va"]))
            scope = d["scope"] or ("static" if d["kind"] == "L" else "global")
            print("%s\t%s\t%08x\t%s\t%s" % (d["name"], t.decl(d["type"]), d["va"], m.short, scope))
    for kind, d in pdb.global_symbols():
        if kind == S_CONSTANT:
            continue
        va = pdb.va(d["seg"], d["off"])
        if (d["name"], va) in seen:
            continue
        seen.add((d["name"], va))
        m = pdb.module_at(d["seg"], d["off"])
        print("%s\t%s\t%08x\t%s\t%s" % (d["name"], t.decl(d["type"]), va, m.short if m else "",
                                        "global" if kind == S_GDATA32 else "static"))


def cmd_constants(pdb, _args):
    pdb.load()
    t = pdb.types
    print("name\ttype\tvalue\tmodule\tscope")
    seen = set()
    for m in pdb.modules:
        for d in m.constants:
            seen.add((d["name"], d["value"]))
            print("%s\t%s\t%s\t%s\t%s" % (d["name"], t.decl(d["type"]), d["value"], m.short, d["scope"]))
    for kind, d in pdb.global_symbols():
        if kind == S_CONSTANT and (d["name"], d["value"]) not in seen:
            seen.add((d["name"], d["value"]))
            print("%s\t%s\t%s\t\t" % (d["name"], t.decl(d["type"]), d["value"]))


def cmd_lines(pdb, args):
    pdb.load()
    print("va\tfile\tline\tfunction")
    for m in pdb.modules:
        if args.module.lower() not in m.name.lower():
            continue
        procs = sorted(m.procs, key=lambda p: (p.seg, p.off))
        starts = [(p.seg, p.off) for p in procs]
        rows = []
        for name, seg, pairs in m.lines:
            for off, line in pairs:
                i = bisect.bisect_right(starts, (seg, off)) - 1
                fn = procs[i].name if i >= 0 and off < procs[i].off + procs[i].size else ""
                rows.append((pdb.va(seg, off), name, line, fn))
        for row in sorted(rows):
            print("%08x\t%s\t%d\t%s" % row)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--pdb", default=DEFAULT_PDB)
    ap.add_argument("--base", type=lambda s: int(s, 16), default=DEFAULT_BASE)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("modules")
    sub.add_parser("functions")
    sub.add_parser("locals").add_argument("substring")
    sub.add_parser("class").add_argument("name")
    sub.add_parser("enum").add_argument("name")
    sub.add_parser("globals")
    sub.add_parser("constants")
    sub.add_parser("lines").add_argument("module")
    args = ap.parse_args()
    pdb = PDB(args.pdb, args.base)
    try:
        globals()["cmd_" + args.cmd](pdb, args)
    except BrokenPipeError:
        sys.stderr.close()


if __name__ == "__main__":
    main()
