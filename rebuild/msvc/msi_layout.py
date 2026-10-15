"""Copy the files of an MSI's cabinets into their install tree, without running the installer.

Cabinet entries are named by the File table's key. The File table gives each key its long file
name and component, the Component table the component's directory, and the Directory table the
path up to the root key named here.

    python3 -I msi_layout.py <File.tsv> <Component.tsv> <Directory.tsv> <cab dir> <out dir> <root key>

The .tsv files are msi_table.py's output. Only directories under <root key> are laid out, files
whose cabinet was not extracted are counted and skipped, and every path must stay inside <out dir>.
"""
import csv
import os
import shutil
import sys


def rows(p):
    with open(p, newline="") as f:
        r = csv.reader(f, delimiter="\t", quoting=csv.QUOTE_NONE)
        head = next(r)
        return [dict(zip(head, x)) for x in r]


def long_name(n):
    n = n.split(":")[-1]
    return n.split("|")[-1]


files, comps, dirs = (rows(p) for p in sys.argv[1:4])
cab, out, root = sys.argv[4], os.path.realpath(sys.argv[5]), sys.argv[6]
parent = {d["Directory"]: (d["Directory_Parent"], long_name(d["DefaultDir"])) for d in dirs}
comp_dir = {c["Component"]: c["Directory_"] for c in comps}


def path_of(key):
    parts = []
    while key and key != root:
        if key not in parent:
            return None
        par, name = parent[key]
        if name != ".":
            parts.append(name)
        key = par
    return None if key != root else os.path.join(*reversed(parts)) if parts else ""


n = missing = 0
for f in files:
    rel = path_of(comp_dir.get(f["Component_"], ""))
    if rel is None:
        print("skip", f["FileName"], "not under", root)
        continue
    if not os.path.exists(os.path.join(cab, f["File"])):
        missing += 1
        continue
    dst = os.path.realpath(os.path.join(out, rel, long_name(f["FileName"])))
    if not dst.startswith(out + os.sep):
        raise SystemExit("path escapes out dir: " + dst)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copyfile(os.path.join(cab, f["File"]), dst)
    n += 1
print(n, "files laid out,", missing, "not in the extracted cabs")
