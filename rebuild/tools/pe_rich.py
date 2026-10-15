"""Print a PE file's Rich header: which tool builds made its objects, and how many of each.

The linker writes, between the DOS stub and the PE header, "DanS" and one (product id << 16 | build,
object count) pair per tool build, XORed with a checksum key that follows "Rich". Product ids name
the tool: Utc1310_CPP is cl 13.10 compiling C++, Utc1310_LTCG_CPP the same with /GL, Masm710 ml 7.10,
Implib710 an import library object, Linker710 the linker itself.

Usage:
    python3 -I pe_rich.py <file.dll> [<file.dll> ...]
"""
import struct
import sys

# Product ids of the Visual Studio 6 to 2003 tools (the ones these builds can contain).
PRODUCTS = {
    0x01: "Import0", 0x04: "Linker600", 0x05: "Cvtomf600", 0x06: "Cvtres500", 0x0a: "Utc12_C",
    0x0b: "Utc12_CPP", 0x0e: "Masm613", 0x0f: "Masm710", 0x12: "Masm614", 0x13: "Linker512",
    0x19: "Implib700", 0x1c: "Utc13_C", 0x1d: "Utc13_CPP", 0x2b: "Utc13_LTCG_C",
    0x2c: "Utc13_LTCG_CPP", 0x3d: "Linker700", 0x3f: "Export700", 0x40: "Masm700",
    0x45: "Cvtres700", 0x5a: "Linker710", 0x5b: "Cvtomf710", 0x5c: "Export710", 0x5d: "Implib710",
    0x5e: "Cvtres710", 0x5f: "Utc1310_C", 0x60: "Utc1310_CPP", 0x61: "Utc1310_C_Std",
    0x62: "Utc1310_CPP_Std", 0x63: "Utc1310_LTCG_C", 0x64: "Utc1310_LTCG_CPP", 0x69: "AliasObj710",
}


def rich(data):
    """[(product id, build, count)] from the Rich header, or [] when there is none."""
    end = data.find(b"Rich", 0, 0x1000)
    if end < 0:
        return []
    key = struct.unpack_from("<I", data, end + 4)[0]
    start = data.find(struct.pack("<I", 0x536E6144 ^ key), 0, end)  # "DanS"
    out = []
    for o in range(start + 16, end, 8):
        comp, count = (x ^ key for x in struct.unpack_from("<II", data, o))
        out.append((comp >> 16, comp & 0xFFFF, count))
    return out


def main():
    for path in sys.argv[1:]:
        with open(path, "rb") as f:
            entries = rich(f.read())
        print(path)
        for prod, build, count in entries:
            print(f"  {PRODUCTS.get(prod, f'product {prod:#x}'):18} build {build:5d}  {count:5d} objects")


if __name__ == "__main__":
    main()
