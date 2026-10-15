"""Print one table of an MSI database whose streams 7z has already split out.

7z x on an .msi writes each table as a stream named "!<Table>", next to "!_StringPool",
"!_StringData" and "!_Columns". Tables are stored column by column; string cells are indexes
into the string pool (2 bytes, or 3 when the pool's codepage word has its top bit set) and
integer cells carry their top bit flipped.

    python3 -I msi_table.py <streams dir> <Table> > <Table>.tsv

Prints the column names, then one tab-separated row per record. Reads only.
"""
import os
import struct
import sys


def strings(d):
    pool = open(os.path.join(d, "!_StringPool"), "rb").read()
    data = open(os.path.join(d, "!_StringData"), "rb").read()
    codepage = struct.unpack_from("<I", pool, 0)[0]
    width = 3 if codepage & 0x80000000 else 2
    out = [""]
    pos, i = 0, 4
    while i + 4 <= len(pool):
        ln, refs = struct.unpack_from("<HH", pool, i)
        i += 4
        if ln == 0 and refs != 0 and i + 4 <= len(pool):
            # long string: the next entry carries the length
            ln = struct.unpack_from("<I", pool, i)[0]
            i += 4
        out.append(data[pos:pos + ln].decode("cp1252", "replace"))
        pos += ln
    return out, width


def columns(d, sp, width):
    raw = open(os.path.join(d, "!_Columns"), "rb").read()
    row = width + 2 + width + 2
    n = len(raw) // row
    off = 0
    tab = [int.from_bytes(raw[off + k * width: off + k * width + width], "little") for k in range(n)]
    off += n * width
    num = [struct.unpack_from("<H", raw, off + k * 2)[0] ^ 0x8000 for k in range(n)]
    off += n * 2
    name = [int.from_bytes(raw[off + k * width: off + k * width + width], "little") for k in range(n)]
    off += n * width
    typ = [struct.unpack_from("<H", raw, off + k * 2)[0] ^ 0x8000 for k in range(n)]
    cols = {}
    for k in range(n):
        cols.setdefault(sp[tab[k]], []).append((num[k], sp[name[k]], typ[k]))
    for t in cols:
        cols[t].sort()
    return cols


def table(d, name):
    sp, width = strings(d)
    cols = columns(d, sp, width)[name]
    raw = open(os.path.join(d, "!" + name), "rb").read()
    sizes = []
    for _, _, t in cols:
        if t & 0x0800 or (t & 0xff) == 0:
            sizes.append(width)
        else:
            sizes.append(2 if (t & 0xff) <= 2 else 4)
    rowsize = sum(sizes)
    n = len(raw) // rowsize
    data = []
    off = 0
    for (num, cname, t), sz in zip(cols, sizes):
        vals = []
        for k in range(n):
            b = raw[off + k * sz: off + k * sz + sz]
            v = int.from_bytes(b, "little")
            if t & 0x0800:
                vals.append(sp[v] if v < len(sp) else "?%d" % v)
            elif v == 0:
                vals.append("")
            elif sz == 2:
                vals.append(str(v ^ 0x8000))
            else:
                vals.append(str(v ^ 0x80000000))
        data.append(vals)
        off += n * sz
    print("\t".join(c[1] for c in cols))
    for k in range(n):
        print("\t".join(col[k] for col in data))


if __name__ == "__main__":
    table(sys.argv[1], sys.argv[2])
