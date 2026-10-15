"""Split and decode the InstallShield stream inside VC_Toolkit_2003_Setup.exe.

7z unpacks the setup .exe's PE sections and leaves the payload as a file named "[0]". That payload
is an InstallShield stream: "InstallShield\\0", a u16 file count at offset 14, then from 0x2e one
entry per file, each a 260-byte name, 52 bytes of header (flags u32 at +0x104, size u32 at
+0x10c) and the file's bytes. Flag 2 marks a file encoded with InstallShield's XOR: the key is
the file's name XORed with 13 35 86 07, and each byte is decoded as ~(key ^ swap_nibbles(byte)),
the key running over the whole file without restarting.

    python3 -I is_stream.py "<7z out>/[0]" <out dir>

Writes every file decoded into <out dir> and prints name, flags, size and first bytes.
"""
import os
import struct
import sys

MAGIC = (0x13, 0x35, 0x86, 0x07)
ENTRY = 312


def decode(data, name):
    key = bytes(c ^ MAGIC[i % 4] for i, c in enumerate(name.encode("latin-1")))
    n = len(key)
    table = bytes(((b << 4) | (b >> 4)) & 0xff for b in range(256))
    return bytes(~(key[i % n] ^ table[b]) & 0xff for i, b in enumerate(data))


def main(src, out):
    data = open(src, "rb").read()
    if data[:14] != b"InstallShield\0":
        raise SystemExit("not an InstallShield stream: " + src)
    count = struct.unpack_from("<H", data, 14)[0]
    pos = 0x2e
    for _ in range(count):
        name = data[pos:pos + 260].split(b"\0")[0].decode("latin-1")
        flags, _, size = struct.unpack_from("<III", data, pos + 0x104)
        body = data[pos + ENTRY: pos + ENTRY + size]
        if flags & 2:
            body = decode(body, name)
        base = os.path.basename(name.replace("\\", "/"))
        if not base or base in (".", ".."):
            raise SystemExit("bad file name in stream: %r" % name)
        with open(os.path.join(out, base), "wb") as f:
            f.write(body)
        print(f"{name!r} flags {flags} size {size} starts {body[:8].hex()}")
        pos += ENTRY + size
    print("end", pos, "of", len(data))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
