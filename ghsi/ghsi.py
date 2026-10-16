#!/usr/bin/env python3
"""Install GHSI ("Gee's Hidden: Source Installer", the Enhanced Edition) on top of this rebuild.

GHSI changes no code: the client.dll, server.dll and server_i486.so it ships are Hidden's official
2006 files. Its fixes are text files (kept in this folder: hidden/, server/hidden/, sdkbase/), maps,
a server browser fix for Source SDK Base 2006, and SourceMod plugins for the dedicated server.
Everything that is not text is fetched from GHSI's own GitHub downloads and checked against
manifest.tsv. See README.md in this folder.

  python3 -I ghsi.py fetch                          download, check and unpack GHSI's downloads
  python3 -I ghsi.py client SOURCEMODS [--bin F]... install the mod into SOURCEMODS/hidden
  python3 -I ghsi.py server DIR [--bin F]...        install the Windows dedicated server into DIR
  python3 -I ghsi.py sdkbase SDKBASE                apply the server browser fix to Source SDK Base
  python3 -I ghsi.py sdkbase SDKBASE --restore      undo it
  python3 -I ghsi.py manifest BASE_ZIP              rewrite manifest.tsv (BASE_ZIP: hsb4b-full.zip)

--bin takes a built client.dll, server.dll or server_i486.so (repeat it for each) and installs it
in place of Hidden's. The downloads are kept in --cache DIR, given before the command (default
~/.cache/hidden-source-ghsi).
"""
import argparse
import binascii
import hashlib
import io
import os
import shutil
import subprocess
import sys
import tarfile
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
MANIFEST = os.path.join(HERE, "manifest.tsv")

# GHSI V11 (9 March 2026) for the game files and the browser fix; the dedicated server package was
# last published with V10 (9 December 2025).
GHSI_COMMIT = "1a51561f778e55d14f54d96f7ca7dbc222837399"
LFS = "https://media.githubusercontent.com/media/GeeTwentyFive/GHSI/" + GHSI_COMMIT + "/src/"
DOWNLOADS = {
    "game_files.b64.py": (LFS + "_GAME_FILES_COMPRESSED_DATA.py",
                          "194fa731270d891946176250549e210852392399737a416bf58b095144d895bc"),
    "browser_fix.b64.py": (LFS + "_SERVER_BROWSER_FIX_COMPRESSED_DATA.py",
                           "cfa685362886c3c4bfee45c33477326127d4edc95aa6df2d8eebbeb777346829"),
    "HiddenServer.7z": ("https://github.com/GeeTwentyFive/GHSI/releases/download/V10/HiddenServer.7z",
                        "aded4ba0544e461b8378efc6bca2d19ff2f1139488e01de1f18bc17959e796d6"),
}
UNPACKED = {"game_files": "game_files.b64.py", "browser_fix": "browser_fix.b64.py", "server": "HiddenServer.7z"}

# GHSI's engine.dll is Source SDK Base's (build 124232) with 7 bytes changed to NOPs. Patching
# the installed file gives the same bytes, so GHSI's copy of Valve's engine is never needed.
ENGINE_ORIGINAL = "4fc435499f0f18db9f1e3d4af84b463ffd2099cbfa3a1037516dd4aedf7a02b7"
ENGINE_PATCHED = "bf871995061f8f83c4e1c511391563cc4e1f109d0540b60e36457f0b6d1bab7e"
ENGINE_PATCH = [
    # call that prints "A2C_PRINT from %s : %s" for every remote print packet
    (0x00AD5B, bytes.fromhex("e8e0750600"), b"\x90" * 5),
    # IsUsingMasterLegacyMode: the jne that returns true when sv_master_legacy_mode is set (its
    # default is 1), so the engine always registers with Steam's master servers
    (0x1773A9, bytes.fromhex("750d"), b"\x90" * 2),
]

# Files of the client layer the dedicated server package does not take from it: the server keeps
# Hidden's own gameinfo.txt and config_default.cfg, and its server.cfg and map lists are in
# server/hidden.
CLIENT_ONLY = {"gameinfo.txt", "cfg/config_default.cfg", "cfg/autoexec.cfg", "cfg/GHS_config.cfg"}

BACKUP = "ghsi-backup"
NEW_FILES = "new-files.txt"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def files_under(root):
    for d, _, names in os.walk(root):
        for n in names:
            yield os.path.relpath(os.path.join(d, n), root).replace(os.sep, "/")


def replace_file(src, dst, data=None):
    """Copy src (or write data) to dst through a temporary file and a rename, so a running game
    never sees half a file."""
    os.makedirs(os.path.dirname(dst) or ".", exist_ok=True)
    tmp = dst + ".ghsi-tmp"
    if data is None:
        shutil.copyfile(src, tmp)
        shutil.copystat(src, tmp)
    else:
        with open(tmp, "wb") as f:
            f.write(data)
    os.replace(tmp, dst)


def copy_tree(src, dst, skip=()):
    for d, _, _ in os.walk(src):  # empty folders too (srcds's SAVE, Steam's caches)
        os.makedirs(os.path.join(dst, os.path.relpath(d, src)), exist_ok=True)
    for rel in sorted(files_under(src)):
        if rel not in skip:
            replace_file(os.path.join(src, rel), os.path.join(dst, rel))


def read_manifest(section):
    out = {}
    with open(MANIFEST, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            sec, digest, size, path = line.rstrip("\n").split("\t")
            if sec == section:
                out[path] = (digest, int(size))
    return out


def check_manifest(section, root, prefix=""):
    """Check every manifest file of a section under root (paths in the manifest are relative to
    the unpacked download; prefix maps them to the install)."""
    bad = []
    for path, (digest, size) in read_manifest(section).items():
        if not path.startswith(prefix):
            continue
        p = os.path.join(root, path[len(prefix):])
        if not os.path.isfile(p) or os.path.getsize(p) != size or sha256(p) != digest:
            bad.append(path)
    if bad:
        sys.exit(f"{len(bad)} files of '{section}' do not match manifest.tsv, first: {bad[0]}")
    return len(read_manifest(section))


def unpack_b64_tar(src, out):
    """GHSI stores a tar.gz as `NAME = b"<base64>"` in a .py file. Read it as data (never import
    it) and extract with tarfile's "data" filter: no absolute paths, no "..", no links out."""
    with open(src, "rb") as f:
        raw = f.read()
    data = binascii.a2b_base64(raw[raw.index(b'b"') + 2:raw.rindex(b'"')])
    del raw
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as t:
        t.extractall(out, filter="data")


def cmd_fetch(a):
    dl = os.path.join(a.cache, "downloads")
    os.makedirs(dl, exist_ok=True)
    for name, (url, digest) in DOWNLOADS.items():
        p = os.path.join(dl, name)
        if os.path.isfile(p) and sha256(p) == digest:
            print(f"have {name}")
            continue
        print(f"downloading {url}")
        with urllib.request.urlopen(url) as r, open(p + ".part", "wb") as f:
            shutil.copyfileobj(r, f, 1 << 20)
        got = sha256(p + ".part")
        if got != digest:
            sys.exit(f"{name}: sha256 {got}, expected {digest}")
        os.replace(p + ".part", p)
    for out, name in UNPACKED.items():
        d = os.path.join(a.cache, out)
        done = os.path.join(d, ".ghsi-complete")
        if os.path.isfile(done):
            print(f"have {out}/")
            continue
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        print(f"unpacking {name} -> {out}/")
        src = os.path.join(dl, name)
        if name.endswith(".7z"):
            # paths in this archive were checked: all relative, none with ".."
            subprocess.run(["7z", "x", "-y", "-bso0", "-bsp0", "-o" + d, src], check=True)
        else:
            unpack_b64_tar(src, d)
        open(done, "w").close()
    if not os.path.isfile(MANIFEST):
        sys.exit("no manifest.tsv to check against: write it with `ghsi.py manifest`")
    for section, sub in (("client", "game_files"), ("sdkbase", "browser_fix"), ("server", "server")):
        n = check_manifest(section, os.path.join(a.cache, sub))
        print(f"{section}: {n} files match manifest.tsv")


def unpacked(a, sub):
    d = os.path.join(a.cache, sub)
    if not os.path.isfile(os.path.join(d, ".ghsi-complete")):
        sys.exit(f"{d} is missing: run `ghsi.py fetch` first")
    return d


def install_bins(bins, bindir):
    for b in bins or []:
        name = os.path.basename(b)
        if name not in ("client.dll", "server.dll", "server_i486.so"):
            sys.exit(f"--bin takes client.dll, server.dll or server_i486.so, not {name}")
        replace_file(b, os.path.join(bindir, name))
        print(f"installed {b}")


def cmd_client(a):
    src = unpacked(a, "game_files")
    mod = os.path.join(a.sourcemods, "hidden")
    # as GHSI's installer does: its whole tree over sourcemods (hidden, and hidden-4a, which holds
    # beta 4a voice lines and is not mounted by gameinfo.txt)
    for top in sorted(os.listdir(src)):
        if os.path.isdir(os.path.join(src, top)):
            copy_tree(os.path.join(src, top), os.path.join(a.sourcemods, top))
    copy_tree(os.path.join(HERE, "hidden"), mod)
    n = check_manifest("client", a.sourcemods, "")
    install_bins(a.bin, os.path.join(mod, "bin"))
    print(f"installed GHSI into {mod} ({n} manifest files checked)")


def cmd_server(a):
    src = unpacked(a, "server")
    copy_tree(src, a.dir, skip={".ghsi-complete"})
    mod = os.path.join(a.dir, "ServerFiles", "hidden")
    copy_tree(os.path.join(HERE, "hidden"), mod, skip=CLIENT_ONLY)
    copy_tree(os.path.join(HERE, "server", "hidden"), mod)
    n = check_manifest("server", a.dir, "")
    install_bins(a.bin, os.path.join(mod, "bin"))
    print(f"installed the GHSI dedicated server into {a.dir} ({n} manifest files checked)")
    print("start it with ServerFiles/srcds.exe -game hidden -tickrate 128 -console +map hdn_stalkyard")


def cmd_sdkbase(a):
    root = a.sdkbase
    if not os.path.isfile(os.path.join(root, "bin", "engine.dll")):
        sys.exit(f"{root} has no bin/engine.dll: give the Source SDK Base folder")
    backup = os.path.join(root, BACKUP)
    if a.restore:
        if not os.path.isdir(backup):
            sys.exit(f"nothing to restore: no {backup}")
        with open(os.path.join(backup, NEW_FILES), encoding="utf-8") as f:
            new = [rel for rel in f.read().split("\n") if rel]
        for rel in new:
            if os.path.isfile(os.path.join(root, rel)):
                os.remove(os.path.join(root, rel))
        # and the folders the fix created, deepest first
        for d in sorted({os.path.dirname(rel) for rel in new}, key=len, reverse=True):
            while d and os.path.isdir(os.path.join(root, d)) and not os.listdir(os.path.join(root, d)):
                os.rmdir(os.path.join(root, d))
                d = os.path.dirname(d)
        for rel in files_under(backup):
            if rel != NEW_FILES:
                replace_file(os.path.join(backup, rel), os.path.join(root, rel))
        shutil.rmtree(backup)
        print(f"restored {root}")
        return
    if os.path.isdir(backup):
        sys.exit(f"{backup} exists: the fix is already applied (use --restore to undo it)")
    src = unpacked(a, "browser_fix")
    engine = os.path.join(root, "bin", "engine.dll")
    have = sha256(engine)
    if have not in (ENGINE_ORIGINAL, ENGINE_PATCHED):
        sys.exit(f"bin/engine.dll is not Source SDK Base build 124232 (sha256 {have}); not touching it")
    files = {rel: os.path.join(src, rel) for rel in files_under(src) if rel != ".ghsi-complete"}
    for rel in list(files):
        if os.path.isfile(os.path.join(HERE, "sdkbase", rel)):
            files[rel] = os.path.join(HERE, "sdkbase", rel)
    new = []
    for rel in sorted(files):
        dst = os.path.join(root, rel)
        if os.path.isfile(dst):
            replace_file(dst, os.path.join(backup, rel))
        else:
            new.append(rel)
    os.makedirs(backup, exist_ok=True)
    with open(os.path.join(backup, NEW_FILES), "w", encoding="utf-8") as f:
        f.write("\n".join(new) + "\n")
    for rel, p in sorted(files.items()):
        if rel != "bin/engine.dll":
            replace_file(p, os.path.join(root, rel))
    if have == ENGINE_ORIGINAL:
        data = bytearray(open(engine, "rb").read())
        for off, old, nop in ENGINE_PATCH:
            if data[off:off + len(old)] != old:
                sys.exit(f"engine.dll: unexpected bytes at {off:#x}")
            data[off:off + len(old)] = nop
        if hashlib.sha256(data).hexdigest() != ENGINE_PATCHED:
            sys.exit("engine.dll: patched result does not match GHSI's engine.dll")
        replace_file(None, engine, bytes(data))
    print(f"applied the server browser fix to {root} ({len(files)} files, {len(new)} new); "
          f"backup in {backup}")


def cmd_manifest(a):
    """For maintainers: list every file GHSI adds or changes that is not text in this folder."""
    lines = ["# section\tsha256\tsize\tpath (relative to the unpacked download)"]
    game = unpacked(a, "game_files")
    base = a.base_zip
    import zipfile
    with zipfile.ZipFile(base) as z:
        official = {i.filename: i for i in z.infolist() if not i.is_dir()}
        for rel in sorted(files_under(game)):
            if rel == ".ghsi-complete" or os.path.isfile(os.path.join(HERE, rel)):
                continue
            p = os.path.join(game, rel)
            if rel in official and z.read(rel) == open(p, "rb").read():
                continue
            lines.append(f"client\t{sha256(p)}\t{os.path.getsize(p)}\t{rel}")
    bf = unpacked(a, "browser_fix")
    for rel in sorted(files_under(bf)):
        if rel != ".ghsi-complete" and not os.path.isfile(os.path.join(HERE, "sdkbase", rel)):
            p = os.path.join(bf, rel)
            lines.append(f"sdkbase\t{sha256(p)}\t{os.path.getsize(p)}\t{rel}")
    srv = unpacked(a, "server")
    plugins = "ServerFiles/hidden/addons/sourcemod/plugins/"
    for rel in sorted(files_under(srv)):
        if rel.startswith(plugins) and os.path.basename(rel).startswith("hsm_"):
            p = os.path.join(srv, rel)
            lines.append(f"server\t{sha256(p)}\t{os.path.getsize(p)}\t{rel}")
    with open(MANIFEST, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {MANIFEST} ({len(lines) - 1} files)")


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--cache", default=os.path.expanduser("~/.cache/hidden-source-ghsi"))
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("fetch")
    c = sub.add_parser("client")
    c.add_argument("sourcemods")
    c.add_argument("--bin", action="append")
    s = sub.add_parser("server")
    s.add_argument("dir")
    s.add_argument("--bin", action="append")
    b = sub.add_parser("sdkbase")
    b.add_argument("sdkbase")
    b.add_argument("--restore", action="store_true")
    m = sub.add_parser("manifest")
    m.add_argument("base_zip", help="hsb4b-full.zip, the official beta 4b release")
    a = p.parse_args()
    {"fetch": cmd_fetch, "client": cmd_client, "server": cmd_server, "sdkbase": cmd_sdkbase,
     "manifest": cmd_manifest}[a.cmd](a)


if __name__ == "__main__":
    main()
