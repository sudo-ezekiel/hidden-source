"""Compare the member offsets behind our RecvProps with client.dll's (a data layout check).

client.dll's RecvProps carry the offset of the member they write (recv_tables.py reads them from
the decompile). Our offsets come from compiling a probe per source file with GCC 3.4: the file is
included whole and each RecvProp's member is turned into a constant, "(int)&((Class *)0)->member",
built with -fno-access-control so private members are reachable. MSVC and GCC lay out these classes
the same way (vfptr first, members in declaration order, natural alignment), so a different offset
is a different layout: a member missing, extra, or out of order before it.

Usage:
    python3 -I recv_offsets.py <work dir> [DT_x ...]
needs <work dir>/src (the mirror) and <work dir>/out/client.mk(.files) from client_check.sh, and
runs the compiles in the hsrebuild-gcc34 image. Prints, per table, every prop whose offset differs,
and writes <work dir>/out/layout/recv_offsets.tsv.
"""
import os
import re
import subprocess
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recv_tables as rt  # noqa: E402

H = os.path.expanduser("~/.cache/subject51764/hsrebuild")


def main():
    work = sys.argv[1]
    only = set(sys.argv[2:])
    src = os.path.join(work, "src")
    out = os.path.join(work, "out", "layout")
    os.makedirs(out, exist_ok=True)
    ref, helpers, construct = rt.client_tables(os.path.join(H, "decomp/client0614/decomp.c"),
                                               os.path.join(H, "builds/client_2006-06-14_4bfull.dll"))
    ours = rt.source_tables(src, os.path.join(work, "out", "client.mk.files"))
    byfile = defaultdict(list)
    for t, info in ours.items():
        if only and t not in only:
            continue
        if t not in ref:
            continue
        refprops = {p["name"]: p for p in ref[t]["props"]}
        for p in info["props"]:
            r = refprops.get(p["name"])
            if r is None or r.get("offset") is None or not p.get("member") or p["type"] == "DataTable" and p["name"] == "baseclass":
                continue
            byfile[info["file"]].append((t, info["class"], p["name"], p["member"], r["offset"]))
    mk = open(os.path.join(work, "out", "client.mk")).read()
    inc = re.search(r"^INCLUDES=(.*)$", mk, re.M).group(1)
    cfl = re.search(r"^CFLAGS=(.*)$", mk, re.M).group(1).replace("-O3", "-O0")
    extra = "-fno-operator-names -fno-access-control -I/src/rebuild/client_shims -I/case -include /src/rebuild/client_shims/client_shim.h"
    probes = os.path.join(out, "probes")
    os.makedirs(probes, exist_ok=True)
    jobs = []
    for i, (f, items) in enumerate(sorted(byfile.items())):
        name = f"probe{i}"
        lines = [f'#include "/src/{f}"']
        for j, (t, cls, pname, member, off) in enumerate(items):
            lines.append(f"extern const int hsprobe_{j} = (int)(long)&(((({cls} *)0))->{member});")
        with open(os.path.join(probes, name + ".cpp"), "w") as w:
            w.write("\n".join(lines) + "\n")
        jobs.append((name, f, items))
    script = "\n".join(
        f"g++-3.4 -w {inc} {cfl} {extra} -S -o /out/layout/probes/{n}.s /out/layout/probes/{n}.cpp "
        f"> /out/layout/probes/{n}.log 2>&1" for n, f, items in jobs)
    with open(os.path.join(probes, "run.sh"), "w") as w:
        w.write(script + "\n")
    subprocess.run(["docker", "run", "--rm", "--platform", "linux/386", "-u", f"{os.getuid()}:{os.getgid()}",
                    "-v", f"{src}:/src:ro", "-v", f"{os.path.join(work, 'out')}:/out",
                    "-v", f"{os.path.join(work, 'case')}:/case:ro", "hsrebuild-gcc34",
                    "sh", "-c", f"cat /out/layout/probes/run.sh | xargs -P 6 -I CMD sh -c CMD"],
                   check=False)
    rows = []
    for n, f, items in jobs:
        vals = {}
        try:
            asm = open(os.path.join(probes, n + ".s")).read()
            for m in re.finditer(r"^hsprobe_(\d+):\s*\n\s*\.long\s+(-?\d+)", asm, re.M):
                vals[int(m.group(1))] = int(m.group(2))
        except OSError:
            pass
        for j, (t, cls, pname, member, off) in enumerate(items):
            v = vals.get(j)
            rows.append((t, cls, pname, member, off, v, f))
    with open(os.path.join(out, "recv_offsets.tsv"), "w") as w:
        w.write("table\tclass\tprop\tmember\treference\tours\tstatus\n")
        for t, cls, pname, member, off, v, f in rows:
            st = "same" if v == off else ("?" if v is None else "DIFF")
            w.write(f"{t}\t{cls}\t{pname}\t{member}\t{off:#x}\t{'' if v is None else hex(v)}\t{st}\n")
    bad = defaultdict(list)
    counts = defaultdict(int)
    for t, cls, pname, member, off, v, f in rows:
        st = "same" if v == off else ("?" if v is None else "DIFF")
        counts[st] += 1
        if st != "same":
            bad[t].append(f"{member} ref {off:#x} ours {'?' if v is None else hex(v)}")
    for t in sorted(bad):
        print(f"{t}: " + "; ".join(bad[t]))
    print(f"recv member offsets: {dict(counts)} ({os.path.join(out, 'recv_offsets.tsv')}); "
          f"'?' = probe did not compile, see {probes}/*.log")


if __name__ == "__main__":
    main()
