"""Dump every save/restore field table (DEFINE_FIELD and friends) from a GCC 3.4 server_i486.so.

BEGIN_DATADESC builds a function-local static array `dataDesc` inside DataMapInit<Class>,
which survives in the symbol table as _ZZ11DataMapInitI<Class>EP9datamap_tPT_E8dataDesc. Its
entries are typedescription_t, 56 bytes each in this SDK generation:

    fieldType, fieldName, fieldOffset[2], fieldSize (u16), flags (s16), externalName,
    pSaveRestoreOps, inputFunc (8-byte member pointer), td, fieldSizeInBytes,
    override_field, override_count, fieldTolerance

The first entry is the SDK's empty FIELD_VOID placeholder. Class inheritance comes from the
GCC typeinfo objects (_ZTI...): __si_class_type_info has one base pointer after the name.

Usage:
    python3 -I dump_datamaps.py <server_i486.so> > fields.tsv
Columns: class, base class, field, field type, offset (hex), count, flags (hex), external name.
"""
import re
import struct
import subprocess
import sys

ENTRY = 56
FIELD_TYPES = [
    "FIELD_VOID", "FIELD_FLOAT", "FIELD_STRING", "FIELD_VECTOR", "FIELD_QUATERNION", "FIELD_INTEGER",
    "FIELD_BOOLEAN", "FIELD_SHORT", "FIELD_CHARACTER", "FIELD_COLOR32", "FIELD_EMBEDDED", "FIELD_CUSTOM",
    "FIELD_CLASSPTR", "FIELD_EHANDLE", "FIELD_EDICT", "FIELD_POSITION_VECTOR", "FIELD_TIME", "FIELD_TICK",
    "FIELD_MODELNAME", "FIELD_SOUNDNAME", "FIELD_INPUT", "FIELD_FUNCTION", "FIELD_VMATRIX",
    "FIELD_VMATRIX_WORLDSPACE", "FIELD_MATRIX3X4_WORLDSPACE", "FIELD_INTERVAL", "FIELD_MODELINDEX",
    "FIELD_MATERIALINDEX",
]


class Elf:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        self.secs = []
        out = subprocess.run(["readelf", "-SW", path], capture_output=True, text=True, check=True).stdout
        for line in out.splitlines():
            parts = line.replace("[ ", "[").split()
            if len(parts) > 6 and parts[0].startswith("["):
                try:
                    self.secs.append((parts[1], parts[2], int(parts[3], 16), int(parts[4], 16), int(parts[5], 16)))
                except ValueError:
                    pass

    def read(self, addr, n):
        for name, typ, va, off, size in self.secs:
            if va and va <= addr < va + size and typ != "NOBITS":
                return self.data[off + addr - va: off + addr - va + n]
        return None

    def cstr(self, addr):
        if not addr:
            return ""
        b = self.read(addr, 256)
        return b.split(b"\0")[0].decode("latin-1") if b else f"<{addr:#x}>"


def symbols(path):
    out = subprocess.run(["readelf", "-sW", path], capture_output=True, text=True, check=True).stdout
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 8 and parts[3] == "OBJECT":
            syms.setdefault(parts[7], (int(parts[1], 16), int(parts[2], 0) if not parts[2].startswith("0x") else int(parts[2], 16)))
    return syms


def demangle(names):
    res = subprocess.run(["c++filt", "-t"], input="\n".join(names), capture_output=True, text=True, check=True)
    return res.stdout.split("\n")[: len(names)]


def main():
    path = sys.argv[1]
    elf = Elf(path)
    syms = symbols(path)

    # typeinfo address -> mangled type, for walking base pointers
    ti_by_addr = {addr: name[4:] for name, (addr, _) in syms.items() if name.startswith("_ZTI")}
    si_vtable = next((a for n, (a, _) in syms.items() if n == "_ZTVN10__cxxabiv120__si_class_type_infoE"), None)

    desc_re = re.compile(r"^_ZZ11DataMapInitI(.+)EP9datamap_tPT_E8dataDesc$")
    tables = []
    for name, (addr, size) in syms.items():
        m = desc_re.match(name)
        if m:
            tables.append((m.group(1), addr, size))

    mangled = [t[0] for t in tables]
    pretty = dict(zip(mangled, demangle(mangled)))

    def base_of(cls_mangled):
        ti = syms.get("_ZTI" + cls_mangled)
        if not ti:
            return ""
        raw = elf.read(ti[0], 12)
        if not raw:
            return ""
        vt, _name, base = struct.unpack("<3I", raw)
        if si_vtable is None or vt != si_vtable + 8:
            return "" if vt != (si_vtable or 0) + 8 else ""
        b = ti_by_addr.get(base)
        return demangle([b])[0] if b else f"<{base:#x}>"

    out = sys.stdout
    for cls, addr, size in sorted(tables, key=lambda t: pretty[t[0]]):
        base = base_of(cls)
        for i in range(size // ENTRY):
            raw = elf.read(addr + i * ENTRY, ENTRY)
            if not raw or len(raw) < ENTRY:
                break
            ftype, fname, off0, off1 = struct.unpack_from("<4I", raw, 0)
            fsize, flags = struct.unpack_from("<Hh", raw, 16)
            (ext,) = struct.unpack_from("<I", raw, 20)
            if ftype == 0 and fname == 0:
                continue
            tname = FIELD_TYPES[ftype] if ftype < len(FIELD_TYPES) else str(ftype)
            out.write(f"{pretty[cls]}\t{base}\t{elf.cstr(fname)}\t{tname}\t{off0:#x}\t{fsize}\t{flags & 0xffff:#x}\t{elf.cstr(ext)}\n")


if __name__ == "__main__":
    main()
