"""Recover MSVC 7.1 RTTI and vftables from a 32-bit PE (client.dll has no PDB, but keeps RTTI).

Layout (MSVC x86):
- TypeDescriptor: {pVFTable(type_info), spare, char name[]} with name ".?AV<Class>@@" or ".?AU...".
- Complete Object Locator (COL): {signature 0, offset, cdOffset, pTypeDescriptor, pClassHierarchy}.
  offset is where this vftable's subobject sits in the complete object (0 = primary vftable).
- Class Hierarchy Descriptor: {signature, attributes, numBaseClasses, pBaseClassArray}; each Base
  Class Descriptor: {pTypeDescriptor, numContainedBases, mdisp, pdisp, vdisp, attributes}.
- vftable[-1] points at the COL, so every dword that points at a COL starts a vftable one dword on.
  Slots run until the next COL pointer or the first dword that is not a .text address.

Usage:
    python3 -I pe_rtti.py <dll> <out dir>
writes rtti.tsv (class, vftable, col offset, slots, bases), vftables.txt (one slot per line) and
vftable_slots.tsv (vftable, class, col offset, slot, target).
Import as a module for the parsed data: load(path) -> Pe, rtti(pe) -> dict.
"""
import os
import struct
import sys


class Pe:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        d = self.data
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsize = struct.unpack_from("<H", d, pe + 20)[0]
        opt = pe + 24
        self.base = struct.unpack_from("<I", d, opt + 28)[0]
        self.sections = []
        for i in range(nsec):
            o = opt + optsize + i * 40
            name = d[o:o + 8].rstrip(b"\0").decode()
            vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", d, o + 8)
            flags = struct.unpack_from("<I", d, o + 36)[0]
            self.sections.append((name, self.base + va, max(vsize, rawsize), rawptr, rawsize, flags))

    def section(self, name):
        return next(s for s in self.sections if s[0] == name)

    def off(self, va):
        for name, start, size, rawptr, rawsize, flags in self.sections:
            if start <= va < start + rawsize:
                return rawptr + va - start
        return None

    def u32(self, va):
        o = self.off(va)
        if o is None or o + 4 > len(self.data):
            return None
        return struct.unpack_from("<I", self.data, o)[0]

    def cstr(self, va, limit=512):
        o = self.off(va)
        if o is None:
            return None
        e = self.data.find(b"\0", o, o + limit)
        return self.data[o:e].decode("latin-1") if e >= 0 else None

    def is_code(self, va):
        name, start, size, rawptr, rawsize, flags = self.section(".text")
        return start <= va < start + size


def demangle_type(raw):
    """'.?AVCFoo@CBar@@' -> 'CBar::CFoo'; templates are kept close to the raw form."""
    s = raw[4:] if raw.startswith((".?AV", ".?AU")) else raw
    if s.endswith("@@"):
        s = s[:-2]
    if s.startswith("?$"):
        return s  # template: leave mangled, it is still unique
    parts = [p for p in s.split("@") if p]
    return "::".join(reversed(parts))


def rtti(pe):
    d = pe.data
    tds = {}
    for name in (".data", ".rdata"):
        sec = pe.section(name)
        _, start, size, rawptr, rawsize, _ = sec
        chunk = d[rawptr:rawptr + rawsize]
        i = 0
        while True:
            i = chunk.find(b".?A", i)
            if i < 0:
                break
            if chunk[i + 3:i + 4] in (b"V", b"U") and i >= 8:
                va = start + i - 8
                raw = pe.cstr(start + i)
                if raw and raw.endswith("@@"):
                    tds[va] = raw
            i += 3
    # COLs live in .rdata, 4-byte aligned.
    _, rstart, rsize, rraw, rrawsize, _ = pe.section(".rdata")
    cols = {}
    for o in range(0, rrawsize - 20, 4):
        sig, offset, cdoff, ptd, pchd = struct.unpack_from("<IIIII", d, rraw + o)
        if sig == 0 and ptd in tds:
            cols[rstart + o] = (offset, cdoff, ptd, pchd)
    col_refs = set()
    vtables = []
    for o in range(0, rrawsize - 4, 4):
        v = struct.unpack_from("<I", d, rraw + o)[0]
        if v in cols:
            col_refs.add(rstart + o)
    for ref in sorted(col_refs):
        col = pe.u32(ref)
        vt = ref + 4
        slots = []
        a = vt
        while True:
            if a != vt and a in col_refs:
                break
            t = pe.u32(a)
            if t is None or not pe.is_code(t):
                break
            slots.append(t)
            a += 4
        if not slots:
            continue
        offset, cdoff, ptd, pchd = cols[col]
        vtables.append({"vftable": vt, "col": col, "offset": offset, "td": ptd,
                        "class": demangle_type(tds[ptd]), "raw": tds[ptd], "slots": slots,
                        "bases": bases(pe, pchd, tds)})
    return {"tds": tds, "cols": cols, "vtables": vtables}


def bases(pe, pchd, tds):
    n = pe.u32(pchd + 8)
    arr = pe.u32(pchd + 12)
    out = []
    if n is None or arr is None or n > 64:
        return out
    for i in range(n):
        bcd = pe.u32(arr + 4 * i)
        if bcd is None:
            break
        ptd = pe.u32(bcd)
        mdisp = pe.u32(bcd + 8)
        if ptd in tds:
            out.append((demangle_type(tds[ptd]), mdisp))
    return out


def main():
    pe = Pe(sys.argv[1])
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    r = rtti(pe)
    with open(os.path.join(out, "rtti.tsv"), "w") as f:
        f.write("class\tvftable\tcol_offset\tslots\tbases\n")
        for v in r["vtables"]:
            b = ",".join(f"{n}@{m:#x}" for n, m in v["bases"][1:])
            f.write(f"{v['class']}\t{v['vftable']:08x}\t{v['offset']:#x}\t{len(v['slots'])}\t{b}\n")
    with open(os.path.join(out, "vftable_slots.tsv"), "w") as f, open(os.path.join(out, "vftables.txt"), "w") as g:
        for v in r["vtables"]:
            g.write(f"VTABLE {v['class']} @ {v['vftable']:08x} offset {v['offset']:#x} slots {len(v['slots'])}\n")
            for i, t in enumerate(v["slots"]):
                f.write(f"{v['vftable']:08x}\t{v['class']}\t{v['offset']:#x}\t{i}\t{t:08x}\n")
                g.write(f"  [{i}] +0x{i * 4:x} {t:08x}\n")
    classes = {v["class"] for v in r["vtables"]}
    print(f"{len(r['tds'])} type descriptors, {len(r['cols'])} COLs, {len(r['vtables'])} vftables, "
          f"{len(classes)} classes with vftables, "
          f"{len({t for v in r['vtables'] for t in v['slots']})} distinct slot targets")


if __name__ == "__main__":
    main()
