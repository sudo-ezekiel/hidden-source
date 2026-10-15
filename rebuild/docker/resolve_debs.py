"""Resolve the .deb closure for a set of packages from a verified Debian Packages index.

Usage: python3 -I resolve_debs.py <Packages> <installed.txt> <pkg>... > debs.tsv
Prints one line per package to fetch: name, version, Filename, SHA256.
Only the first alternative of each dependency is followed; packages already
installed in the base image are skipped.
"""
import re
import sys


def parse(path):
    pkgs = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for block in f.read().split("\n\n"):
            fields = {}
            key = None
            for line in block.splitlines():
                if line.startswith((" ", "\t")) and key:
                    fields[key] += "\n" + line
                elif ":" in line:
                    key, _, val = line.partition(":")
                    fields[key] = val.strip()
            if "Package" in fields:
                pkgs[fields["Package"]] = fields
    return pkgs


def deps(fields):
    out = []
    for field in ("Pre-Depends", "Depends"):
        for clause in fields.get(field, "").split(","):
            clause = clause.strip()
            if clause:
                first = clause.split("|")[0].strip()
                out.append(re.split(r"[\s(]", first)[0])
    return out


def main():
    pkgs = parse(sys.argv[1])
    with open(sys.argv[2]) as f:
        installed = {line.strip() for line in f if line.strip()}
    provides = {}
    for name, fields in pkgs.items():
        for p in fields.get("Provides", "").split(","):
            if p.strip():
                provides.setdefault(p.strip(), name)
    todo = list(sys.argv[3:])
    seen = set()
    while todo:
        name = todo.pop()
        name = name if name in pkgs else provides.get(name, name)
        if name in seen or name in installed:
            continue
        seen.add(name)
        if name not in pkgs:
            print(f"MISSING\t{name}", file=sys.stderr)
            continue
        todo.extend(deps(pkgs[name]))
    for name in sorted(seen):
        if name in pkgs:
            f = pkgs[name]
            print(f"{name}\t{f['Version']}\t{f['Filename']}\t{f['SHA256']}")


if __name__ == "__main__":
    main()
