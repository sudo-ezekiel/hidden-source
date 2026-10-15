"""Summarise a client_check.sh run: PASS/MSVC/FAIL per source file of the client build.

    python3 -I client_check_report.py <work dir>/out <msvc_only.txt> [file.cpp ...]

PASS: the object was built. MSVC: GCC rejected the file, but every error it reported matches a
pattern in msvc_only.txt, i.e. a construct MSVC 7.1 accepts and GCC 3.4 cannot be made to (binding a
temporary to a non-const reference, DirectDraw types). FAIL: anything else; the first error is shown.
Writes <out>/client_check.tsv for every file in the build, and prints the non-passing files (or, with
file arguments, the status of just those files). Exits 1 when a named file fails.
"""
import os
import re
import sys

DIAG = re.compile(r"^(\S+?):(\d+)(?::\d+)?: (.*)$")


def errors(log):
    try:
        with open(log, errors="replace") as f:
            lines = f.read().splitlines()
    except OSError:
        return ["not compiled (no log)"]
    out = []
    for line in lines:
        m = DIAG.match(line)
        if not m or m.group(3).startswith(("note:", "warning:", "In ")):
            continue
        out.append(line.replace("/src/", "").replace("/case/", "(case alias) "))
    return out or [(lines[-1] if lines else "failed without output")]


def load_patterns(path):
    pats = []
    with open(path) as f:
        for line in f:
            line = line.rstrip("\r\n")
            if line and not line.startswith("#"):
                pats.append(re.compile(line))
    return pats


def main():
    out, patfile = sys.argv[1], sys.argv[2]
    wanted = {a.removeprefix("./") for a in sys.argv[3:]}
    pats = load_patterns(patfile)
    rows = []
    with open(os.path.join(out, "client.mk.files")) as f:
        for line in f:
            src, obj = line.rstrip("\n").split("\t")
            o = os.path.join(out, "obj", obj)
            if os.path.exists(o):
                rows.append((src, "PASS", ""))
                continue
            errs = errors(o + ".log")
            real = [e for e in errs if not any(p.search(e) for p in pats)]
            if real:
                rows.append((src, "FAIL", real[0][:300]))
            else:
                rows.append((src, "MSVC", errs[0][:300]))
    with open(os.path.join(out, "client_check.tsv"), "w") as f:
        for r in rows:
            f.write("\t".join(r) + "\n")
    shown = [r for r in rows if r[0] in wanted] if wanted else [r for r in rows if r[1] != "PASS"]
    for src, status, err in sorted(shown, key=lambda r: (r[1], r[0])):
        print(f"{status}  {src}" + (f"\n      {err}" if err else ""))
    n = {s: sum(1 for r in rows if r[1] == s) for s in ("PASS", "MSVC", "FAIL")}
    print(f"client check: {n['PASS']} of {len(rows)} files pass, {n['MSVC']} MSVC-only, {n['FAIL']} fail "
          f"({os.path.join(out, 'client_check.tsv')})")
    if wanted and any(r[1] == "FAIL" for r in shown):
        sys.exit(1)


if __name__ == "__main__":
    main()
