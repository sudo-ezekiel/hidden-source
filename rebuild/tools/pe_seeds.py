"""List function-start seeds and vftable labels for a Ghidra pass over an MSVC PE (client.dll).

Ghidra's auto-analysis of client.dll found only about 8600 functions: most virtuals and callbacks are
reached only through data. This collects every code address the image points at:
- vftable slots (from pe_rtti.py);
- base relocations in .rdata/.data whose target is in .text (function pointer tables, RecvProp
  proxies, ConCommand callbacks, datadesc input functions, factories);
- base relocations inside .text whose target is in .text and whose site follows a push imm32 or
  mov reg, imm32 opcode (callbacks passed as arguments). Switch jump tables also sit in .text, but
  their neighbours are other table entries, not those opcodes.

Usage:
    python3 -I pe_seeds.py <dll> <out dir>
writes seeds.txt (hex address per line) and labels.txt (hex address, label) for RecoverFunctions.java.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_rtti  # noqa: E402


def relocations(pe):
    name, start, size, raw, rawsize, _ = pe.section(".reloc")
    d = pe.data
    o = raw
    out = []
    while o < raw + rawsize:
        page, block = struct.unpack_from("<II", d, o)
        if block == 0:
            break
        for i in range((block - 8) // 2):
            e = struct.unpack_from("<H", d, o + 8 + 2 * i)[0]
            if e >> 12 == 3:
                out.append(pe.base + page + (e & 0xFFF))
        o += block
    return out


def section_of(pe, va):
    for s in pe.sections:
        if s[1] <= va < s[1] + s[2]:
            return s[0]
    return None


def label_name(cls, offset):
    safe = "".join(c if c.isalnum() or c == "_" else "_" for c in cls.replace("::", "__"))
    return f"vftable_{safe}" + (f"_at_{offset:x}" if offset else "")


def main():
    pe = pe_rtti.Pe(sys.argv[1])
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    r = pe_rtti.rtti(pe)
    seeds = set()
    for v in r["vtables"]:
        seeds.update(v["slots"])
    for site in relocations(pe):
        t = pe.u32(site)
        if t is None or not pe.is_code(t):
            continue
        sec = section_of(pe, site)
        if sec in (".rdata", ".data"):
            seeds.add(t)
        elif sec == ".text":
            o = pe.off(site)
            op = pe.data[o - 1]
            if op == 0x68 or 0xB8 <= op <= 0xBF:
                seeds.add(t)
    with open(os.path.join(out, "seeds.txt"), "w") as f:
        for a in sorted(seeds):
            f.write(f"{a:08x}\n")
    with open(os.path.join(out, "labels.txt"), "w") as f:
        for v in r["vtables"]:
            f.write(f"{v['vftable']:08x}\t{label_name(v['class'], v['offset'])}\n")
    print(f"{len(seeds)} seeds, {len(r['vtables'])} vftable labels -> {out}")


if __name__ == "__main__":
    main()
