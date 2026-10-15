"""Print a PE file's Authenticode digests and save its signature blob.

Hashes the file the way Authenticode does (skipping the checksum, the security directory entry
and the certificate table), so the digest can be checked against the one signed inside the
PKCS#7 blob:

    python3 -I authenticode.py <file.exe> <out.p7>
    openssl pkcs7 -inform DER -in <out.p7> -print_certs -noout
    openssl asn1parse -inform DER -in <out.p7>    # the 20-byte OCTET STRING is the signed SHA1
"""
import hashlib
import struct
import sys


def main(path, out):
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("not a PE file: " + path)
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    secdir = opt + (96 if magic == 0x10b else 112) + 4 * 8
    checksum = opt + 64
    sec_off, sec_size = struct.unpack_from("<II", data, secdir)
    print("security directory offset", sec_off, "size", sec_size, "file size", len(data))
    for name in ("sha1", "md5", "sha256"):
        h = hashlib.new(name)
        h.update(data[:checksum])
        h.update(data[checksum + 4:secdir])
        h.update(data[secdir + 8:sec_off if sec_size else len(data)])
        if sec_size:
            h.update(data[sec_off + sec_size:])
        print(name, h.hexdigest())
    if sec_size:
        length, rev, ctype = struct.unpack_from("<IHH", data, sec_off)
        print("WIN_CERTIFICATE length", length, "revision", hex(rev), "type", ctype)
        open(out, "wb").write(data[sec_off + 8: sec_off + length])


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
