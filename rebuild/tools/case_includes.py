"""Make the Windows-cased #include lines of a source tree resolve on a case-sensitive file system.

The client sources were written against NTFS, so they include "materialsystem/materialsystemutil.h"
when the file is MaterialSystemUtil.h. Rather than edit Valve's files, this scans every quoted and
angled #include in the tree, and for each one that does not resolve exactly (next to the including
file or in an include directory) but does resolve case-insensitively, creates a symlink
<alias dir>/<name as written> -> <container path of the real file>. Add -I<alias dir> last.

    python3 -I case_includes.py <host source root> <container source root> <alias dir> <includes...>

includes are the -I directories relative to the source root.
"""
import os
import re
import sys

# Only the trees client.dll compiles from are scanned, so includes in tools elsewhere (utils/, dlls/)
# do not add aliases.
SCAN = ("cl_dll", "game_shared", "public", "common", "tier1", "vgui2")
INC = re.compile(rb'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)


def ci_resolve(base, rel, cache):
    """Return the on-disk relative path of rel under base, matching each component case-insensitively."""
    cur = base
    parts = []
    for p in rel.replace("\\", "/").split("/"):
        if p in ("", "."):
            continue
        if p == "..":
            cur = os.path.dirname(cur)
            parts.append(p)
            continue
        if cur not in cache:
            try:
                cache[cur] = {n.lower(): n for n in os.listdir(cur)}
            except OSError:
                cache[cur] = {}
        n = cache[cur].get(p.lower())
        if n is None:
            return None
        parts.append(n)
        cur = os.path.join(cur, n)
    return os.path.normpath(os.path.join(base, *parts)) if parts else None


def main():
    host, cont, alias = sys.argv[1], sys.argv[2].rstrip("/"), sys.argv[3]
    incdirs = [os.path.join(host, d) for d in sys.argv[4:]]
    cache = {}
    links = {}
    walk = [w for top in SCAN for w in os.walk(os.path.join(host, top))]
    for dirpath, dirs, files in walk:
        for fn in files:
            if not fn.lower().endswith((".cpp", ".h", ".inl", ".hpp", ".c")):
                continue
            path = os.path.join(dirpath, fn)
            with open(path, "rb") as f:
                text = f.read()
            for m in INC.finditer(text):
                rel = m.group(1).decode("latin-1").replace("\\", "/")
                bases = [dirpath] + incdirs
                if any(os.path.isfile(os.path.join(b, rel)) for b in bases):
                    continue
                for b in bases:
                    real = ci_resolve(b, rel, cache)
                    if real and os.path.isfile(real):
                        links.setdefault(os.path.normpath(rel), cont + "/" + os.path.relpath(real, host))
                        break
    # A file opened through an alias looks for its own quoted includes next to the alias, so give
    # each alias its real directory's headers as siblings too (the alias dir is searched last).
    for rel, target in list(links.items()):
        real_dir = os.path.join(host, os.path.dirname(os.path.relpath(target, cont)))
        for n in os.listdir(real_dir):
            if n.lower().endswith((".h", ".inl", ".hpp")):
                sib = os.path.normpath(os.path.join(os.path.dirname(rel), n))
                links.setdefault(sib, cont + "/" + os.path.relpath(os.path.join(real_dir, n), host))
    made = 0
    for rel, target in links.items():
        if rel.startswith(".."):
            continue
        dst = os.path.join(alias, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if os.path.islink(dst) and os.readlink(dst) == target:
            continue
        if os.path.lexists(dst):
            os.remove(dst)
        os.symlink(target, dst)
        made += 1
    print(f"{len(links)} case aliases ({made} new) in {alias}")


if __name__ == "__main__":
    main()
